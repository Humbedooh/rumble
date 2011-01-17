#include "rumble.h"
#include "sqlite3.h"
#include "servers.h"
#include "private.h"
#include "database.h"
#include <sys/stat.h>
#include "comm.h"

mqueue* current = 0;
cvector* badmx;

uint32_t rumble_send_email(masterHandle* master, const char* mailserver, const char* filename, const char* sender, const char* recipient) {
	return 0;
}

void* rumble_worker_process(void* m) {
    mqueue* item;
    userAccount* user;
    ssize_t rc;
    masterHandle* master = (masterHandle*) m;
    sessionHandle* sess = (sessionHandle*) malloc(sizeof(sessionHandle));
    if (!sess) merror();
    sess->_master = m;
    
    while (1) {
        pthread_mutex_lock(&master->_core.workmutex);
        pthread_cond_wait(&master->_core.workcond, &master->_core.workmutex);
        
        // Make a private copy of the mail struct address and reset the global one
        item = current;
        current = 0;
        pthread_mutex_unlock(&master->_core.workmutex);

		// Check for rampant loops
		item->loops++;
		if ( item->loops > 5 ) { 
			if ( &item->recipient ) rumble_free_address(&item->recipient);
			if ( &item->sender ) rumble_free_address(&item->recipient);
			if ( item->fid) free((char*) item->fid);
			if ( item->flags) free((char*) item->flags);
			free(item);
			printf("Mail is looping, ignoring it!\n");
			continue;
		}

        // Local delivery?
        if ( rumble_domain_exists(sess, item->recipient.domain)) {
            printf("%s is local domain, looking for user %s@%s\n", item->recipient.domain, item->recipient.user, item->recipient.domain);
            user = rumble_get_account(master, item->recipient.user, item->recipient.domain);
            if ( user ) {
                uint32_t fsize;
                item->account = user;
                
                // pre-delivery parsing (virus, spam, that sort of stuff)
                rc = rumble_server_schedule_hooks(master, (sessionHandle*)item, RUMBLE_HOOK_PARSER); // hack, hack, hack
                if ( rc == RUMBLE_RETURN_OKAY ) {
                    if ( user->type & RUMBLE_MTYPE_MBOX ) { // mail box
						char* cfsize, *cuid, *ofilename, *nfilename;
                        sqlite3_stmt* state;
                        const char* path;

						// Start by making a copy of the letter
						fsize = rumble_copy_mail(master, item->fid,user->user,user->domain, (char**) &item->fid);
						if ( !item->fid || !fsize ) { fprintf(stderr, "<smtp::worker> Bad mail file, aborting\n"); continue;}

                        // move file to user's inbox
                        path = rumble_config_str(master, "storagefolder");
                        ofilename = (char*) calloc(1, strlen(path) + 26);
                        nfilename = (char*) calloc(1, strlen(path) + 26);
                        sprintf(ofilename, "%s/%s", path, item->fid);
                        sprintf(nfilename, "%s/%s.msg", path, item->fid);
                        #ifdef RUMBLE_DEBUG_STORAGE
                        printf("Moving %s to %s\n", ofilename, nfilename);
                        #endif
                        if (rename(ofilename, nfilename)) {
                            perror("Couldn't move file");
                        }
                        cfsize = (char*) calloc(1,15);
                        cuid = (char*) calloc(1,15);
                        if (!cfsize || !cuid) merror();
                        state = rumble_sql_inject((sqlite3*) master->_core.db, \
                            "INSERT INTO mbox (uid, fid, size, flags) VALUES (?,?,?,0)",
                            itoa(user->uid, cuid, 10), item->fid, itoa(fsize, cfsize, 10));
                        rc = sqlite3_step(state);
                        sqlite3_finalize(state);
                        free(cfsize);
                        free(cuid);
                        // done here!
                    }
                    if ( user->type & RUMBLE_MTYPE_ALIAS ) { // mail alias
                        if ( strlen(user->arg)) {
                            char* pch = strtok(user->arg," ");
                            char* usr = (char*) calloc(1,128);
                            char* dmn = (char*) calloc(1,128);
                            while ( pch != NULL ) {
                                memset(usr,0,128);
                                memset(dmn,0,128);
                                if ( strlen(pch) >= 3 ) {
                                    sqlite3_stmt* state;
									char* loops = (char*) calloc(1,4);
									sprintf(loops,"%u",item->loops);
                                    if (sscanf(pch, "%128[^@]@%128c", usr, dmn)) {
                                        rumble_string_lower(dmn);
                                        rumble_string_lower(usr);
                                        state = rumble_sql_inject((sqlite3*) master->_core.db, \
                                                "INSERT INTO queue (loops, fid, sender, user, domain, flags) VALUES (?,?,?,?,?,?)", \
                                                loops, item->fid, item->sender.raw, usr, dmn, item->flags);
                                        sqlite3_step(state);
                                        sqlite3_finalize(state);
                                    }
                                }
                                pch = strtok(NULL, " ");
                            }
                        }
                        // done here!
                    }
                    if ( user->type & RUMBLE_MTYPE_MOD ) { // feed to module
                        printf("<worker> Feeding mail to module %s\n", user->arg);
                        rumble_server_schedule_hooks(master, (sessionHandle*)item, RUMBLE_HOOK_FEED); // hack, hack, hack
                        // done here!
                    }
                    if ( user->type & RUMBLE_MTYPE_FEED) { // feed to program or url
                        // done here!
                    }
                }
                rumble_free_account(user);
            }
        }
        
    // Foreign delivery?
        else {
            cvector *mx;
            mxRecord* mxr;
			char *filename, *recipient;
            uint32_t delivered = 0;
			uint32_t dx = 0;
            printf ("%s@%s is a foreign user\n",item->recipient.user, item->recipient.domain);
            mx = comm_mxLookup(item->recipient.domain);
            if (!mx) merror();
            if ( cvector_size(mx) ) {
				filename = (char*) calloc(1,256);
				recipient = (char*) calloc(1,256);
				if (!filename || !recipient) merror();
				sprintf(recipient, "<%s@%s>", item->recipient.user, item->recipient.domain);
				sprintf(filename, "%s/%s", rrdict(master->_core.conf, "storagepath"), item->fid);
                for (mxr = (mxRecord*) cvector_first(mx); mxr != NULL; mxr = (mxRecord*) cvector_next(mx)) {
                    if ( rhdict(badmx, mxr->host) ) continue; // ignore bogus MX records
                    printf("Trying %s (%u)...\n", mxr->host, mxr->preference);
					dx = rumble_send_email(master, mxr->host, filename, item->sender.raw, recipient); // 0 = crit fail, 1 = temp fail, 2 = delivered!
					delivered = ( dx > delivered ) ? dx : delivered; // get the best result from all servers we've tried
					if ( delivered == 2 ) break; // yay!
                }
				free(filename);
				free(recipient);
            }
			if ( delivered == 0 ) { } // critical failure, giving up.
			if ( delivered == 1 ) { // temp failure, push mail back into queue (schedule next try in 30 minutes).
				sqlite3_stmt* state;
				char* loops = (char*) calloc(1,4);
				if (!loops) merror();
				sprintf(loops, "%u", item->loops);
				state = rumble_sql_inject((sqlite3*) master->_core.db, \
                        "INSERT INTO queue (time, loops, fid, sender, user, domain, flags) VALUES (strftime('%s', 'now', '+30 minutes'),?,?,?,?,?,?)", \
						loops, item->fid, item->sender.raw, item->recipient.user, item->recipient.domain, item->flags);
                sqlite3_step(state);
                sqlite3_finalize(state);
			} 
			// All done!
        }
        if ( &item->recipient ) rumble_free_address(&item->recipient);
        if ( &item->sender ) rumble_free_address(&item->recipient);
        if ( item->fid) free((char*) item->fid);
        if ( item->flags) free((char*) item->flags);
        free(item);
        }
    }

void* rumble_worker_init(void* m) {
    masterHandle* master = (masterHandle*) m;
    int x;
    const char* ignmx;
    const char* statement = "SELECT time, loops, fid, sender, user, domain, flags, id FROM queue WHERE time <= strftime('%s','now') LIMIT 1";
	int rc;
    sqlite3_stmt* state;
    sqlite3_prepare_v2((sqlite3*) master->_core.db, statement, -1, &state, NULL);
    pthread_mutex_init(&master->_core.workmutex, NULL);
    pthread_cond_init(&master->_core.workcond, NULL);
    ignmx = rrdict(master->_core.conf, "ignoremx");
    badmx = cvector_init();
    if ( strlen(ignmx) ) rumble_scan_words(badmx, ignmx);

    for (x = 0; x < 10; x++ ) {
        pthread_t* t = (pthread_t*) malloc(sizeof(pthread_t));
        cvector_add(master->_core.workers, t);
        pthread_create(t, NULL, rumble_worker_process, (void *)m);
    }
    while (1) {
        rc = sqlite3_step(state);
        if ( rc == SQLITE_ROW ) {
            int l;
            uint32_t mid;
            char* sql, *zErrMsg;
            mqueue* item = (mqueue*) calloc(1,sizeof(mqueue));
            if (item) {
				// delivery time
                item->date = sqlite3_column_int(state, 0);

				// loops
				item->loops = sqlite3_column_int(state, 1);
            
                // fid
                l = sqlite3_column_bytes(state,2);
                item->fid = (char*) calloc(1,l+1);
                memcpy((char*) item->fid, sqlite3_column_text(state,2), l);
            
                // sender
                l = sqlite3_column_bytes(state,3);
                item->sender.raw = (char*) calloc(1,l+1);
                memcpy((char*) item->sender.raw, sqlite3_column_text(state,3), l);
            
                // recipient
                l = sqlite3_column_bytes(state,4);
                item->recipient.user = (char*) calloc(1,l+1);
                memcpy((char*) item->recipient.user, sqlite3_column_text(state,4), l);
                l = sqlite3_column_bytes(state,5);
                item->recipient.domain = (char*) calloc(1,l+1);
                memcpy((char*) item->recipient.domain, sqlite3_column_text(state,5), l);
            
                //flags
                l = sqlite3_column_bytes(state,6);
                item->flags = (char*) calloc(1,l+1);
                memcpy((char*) item->flags, sqlite3_column_text(state,6), l);
            
                mid = sqlite3_column_int(state, 7);
            
                sqlite3_reset(state);
                sql = (char*) calloc(1,128);
                if (!sql) return 0;
                sprintf(sql, "DELETE FROM queue WHERE id=%u", mid);
                zErrMsg = 0;
                l = sqlite3_exec((sqlite3*) master->_core.db,sql, 0, 0, &zErrMsg);
                free(sql);
                pthread_mutex_lock(&master->_core.workmutex);
                current = item;
                pthread_cond_signal(&master->_core.workcond);
                pthread_mutex_unlock(&master->_core.workmutex);
            }
        }
        else {
            sqlite3_reset(state);
            sleep(5); // sleep for 5 seconds if there's nothing to do right now.
        }
    }
}

