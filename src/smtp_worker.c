#include "rumble.h"
#include "sqlite3.h"
#include "servers.h"
#include "private.h"
#include "database.h"
#include <sys/stat.h>
#include "comm.h"

mqueue* current = 0;
cvector* badmx;

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

		// Local delivery?
		if ( rumble_domain_exists(sess, item->recipient.domain)) {
			printf("%s is local domain, looking for user %s@%s\n", item->recipient.domain, item->recipient.user, item->recipient.domain);
			user = rumble_get_account(master, item->recipient.user, item->recipient.domain);
			if ( user ) {
				uint32_t fsize;
				item->account = user;
                
				// Start by making a copy of the letter
				fsize = rumble_copy_mail(master, item->fid,user->user,user->domain, (char**) &item->fid);
				if ( !item->fid || !fsize ) { fprintf(stderr, "<smtp::worker> Bad mail file, aborting\n"); continue;}
				// pre-delivery parsing (virus, spam, that sort of stuff)
				rc = rumble_server_schedule_hooks(master, (sessionHandle*)item, RUMBLE_HOOK_PARSER); // hack, hack, hack
				if ( rc == RUMBLE_RETURN_OKAY ) {
					if ( user->type & RUMBLE_MTYPE_MBOX ) { // mail box
						// move file to user's inbox
						char* cfsize, *cuid;
						sqlite3_stmt* state;
						const char* path = rumble_config_str(master, "storagefolder");
						char* ofilename = (char*) calloc(1, strlen(path) + 26);
						char* nfilename = (char*) calloc(1, strlen(path) + 26);
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
									if (sscanf(pch, "%128[^@]@%128c", usr, dmn)) {
										rumble_string_lower(dmn);
										rumble_string_lower(usr);
										state = rumble_sql_inject((sqlite3*) master->_core.db, \
												"INSERT INTO queue (fid, sender, user, domain, flags) VALUES (?,?,?,?,?)", \
												item->fid, item->sender.raw, usr, dmn, item->flags);
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
			uint32_t delivered = 0;
			printf ("%s@%s is a foreign user\n",item->recipient.user, item->recipient.domain);
			mx = comm_mxLookup(item->recipient.domain);
			if (!mx) merror();
			if ( cvector_size(mx) ) {
				for (mxr = (mxRecord*) cvector_first(mx); mxr != NULL; mxr = (mxRecord*) cvector_next(mx)) {
					if ( rhdict(badmx, mxr->host) ) continue; // ignore bogus MX records
					printf("Trying %s (%u)...\n", mxr->host, mxr->preference);
				}
			}
			if (!delivered) printf("Sowwy, couldn't deliver the message for %s@%s anywhere :(\n", item->recipient.user, item->recipient.domain);
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
	const char* statement = "SELECT time, fid, sender, user, domain, flags, id FROM queue LIMIT 1";
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
		int rc;
        sqlite3_stmt* state;
        sqlite3_prepare_v2((sqlite3*) master->_core.db, statement, -1, &state, NULL);
        rc = sqlite3_step(state);
        if ( rc == SQLITE_ROW ) {
			int l;
			uint32_t mid;
			char* sql, *zErrMsg;
            mqueue* item = (mqueue*) calloc(1,sizeof(mqueue));
			if (item) {
				item->date = sqlite3_column_int(state, 0);
            
				// fid
				l = sqlite3_column_bytes(state,1);
				item->fid = (char*) calloc(1,l+1);
				memcpy((char*) item->fid, sqlite3_column_text(state,1), l);
            
				// sender
				l = sqlite3_column_bytes(state,2);
				item->sender.raw = (char*) calloc(1,l+1);
				memcpy((char*) item->sender.raw, sqlite3_column_text(state,2), l);
            
				// recipient
				l = sqlite3_column_bytes(state,3);
				item->recipient.user = (char*) calloc(1,l+1);
				memcpy((char*) item->recipient.user, sqlite3_column_text(state,3), l);
				l = sqlite3_column_bytes(state,4);
				item->recipient.domain = (char*) calloc(1,l+1);
				memcpy((char*) item->recipient.domain, sqlite3_column_text(state,4), l);
            
				//flags
				l = sqlite3_column_bytes(state,5);
				item->flags = (char*) calloc(1,l+1);
				memcpy((char*) item->flags, sqlite3_column_text(state,5), l);
            
				mid = sqlite3_column_int(state, 6);
            
				sqlite3_finalize(state);
				sql = (char*) calloc(1,128);
				if (!sql) return 0;
				sprintf(sql, "DELETE FROM queue WHERE id=%u", mid);
				zErrMsg = 0;
				l = sqlite3_exec((sqlite3*) master->_core.db,sql, 0, 0, &zErrMsg);
				free(sql);
				printf(":");
				pthread_mutex_lock(&master->_core.workmutex);
				current = item;
				pthread_cond_signal(&master->_core.workcond);
                pthread_mutex_unlock(&master->_core.workmutex);
			}
        }
        else {
            sqlite3_finalize(state);
            sleep(5); // sleep for 5 seconds if there's nothing to do right now.
        }
    }
}

