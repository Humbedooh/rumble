#include "rumble.h"
#include "sqlite3.h"
#include "servers.h"
#include "private.h"
#include "database.h"
#include <sys/stat.h>
#include "comm.h"

mqueue* current = 0;
cvector* badmx;

void get_smtp_response(sessionHandle* session, rumble_sendmail_response* res) {
	char* line, *flag;
	unsigned char b = '-';
	flag = (char*) calloc(1,200);
	if (!flag) merror();
	if ( res ) {
        while (b == '-' ) {
            line = rcread(session);
            res->replyCode = 500;
            if (!line) break;
            memset(res->replyMessage, 0, 1000);
            if (sscanf(line, "%3u%c%200c", &res->replyCode, &b, res->replyMessage) < 2) { res->replyCode = 500; break; }
            memset(flag, 0, 200);
            if ( sscanf(line, "%*3u%*1[ %-]%20[A-Z0-9]", flag) ) {
                if ( strlen(flag) > 2 ) {
                    rsdict(res->flags, flag, flag);
                }
            }
            free(line);
		}
	}
}

rumble_sendmail_response* rumble_send_email(masterHandle* master, const char* mailserver, const char* filename, address* sender, address* recipient) {
	rumble_sendmail_response* res;
	sessionHandle s;
	clientHandle c;
	FILE* fp;
	uint32_t fsize, chunk;
	char buffer[2048];
	const char* me;
	s.client = &c;
	res = (rumble_sendmail_response*) malloc(sizeof(rumble_sendmail_response));
	if (!res) merror();
	res->flags = cvector_init();
	res->replyCode = 500;
	res->replyMessage = (char*) calloc(1,1024);
	res->replyServer = (char*) calloc(1, strlen(mailserver)+1);
	if (!res->replyServer || !res->replyMessage ||!res->flags) merror();
	strncpy(res->replyServer, mailserver, strlen(mailserver));
	sprintf(res->replyMessage, "Server could not be reached.");
	
	fp = fopen(filename, "rb");
	if (!fp) { perror("Couldn't open file!"); return res; }
	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	rewind(fp);
	res->replyCode = 250;
	return res;
	printf("connecting to %s...\n", mailserver);
	c.tls = 0;
	c.socket = comm_open(master, mailserver, 25);
	me = rumble_get_dictionary_value(master->_core.conf, "servername");

	FD_ZERO(&c.fd);
	FD_SET(c.socket, &c.fd);
	
	// Append BATV (unless we already have BATV or VERP)
	if ( !strlen(sender->tag) ) {
		rumbleKeyValuePair* el;
		char *batv = rumble_create_filename();
		sprintf(sender->tag, "prvs=%s", batv);
		el = (rumbleKeyValuePair*) malloc(sizeof(rumbleKeyValuePair));
		if (!el) merror();
		el->key = batv;
		el->value = (char*) time(0);
		cvector_add(master->_core.batv, el);
	}
	while ( c.socket ) {
		get_smtp_response(&s, res);
		if ( res->replyCode >= 300 ) break;

		// Try EHLO first
		rcprintf(&s, "EHLO %s\r\n", me);
		get_smtp_response(&s, res);
		if ( res->replyCode >= 300 ) {
			// Or...try HELO
			rcprintf(&s, "HELO %s\r\n", me);
			get_smtp_response(&s, res);
			if ( res->replyCode >= 300 ) break;
		}

		// Do a MAIL FROM
		if ( rhdict(res->flags, "SIZE") ) {
			rcprintf(&s, "MAIL FROM: <%s=%s@%s> SIZE=%u\r\n", sender->tag, sender->user, sender->domain, fsize);
		}
		else {
			rcprintf(&s, "MAIL FROM: <%s=%s@%s>\r\n", sender->tag, sender->user, sender->domain);
		}
		get_smtp_response(&s, res);
		if ( res->replyCode >= 300 ) break;

		// Do an RCPT TO
		rcprintf(&s, "RCPT TO: <%s@%s>\r\n", recipient->user, recipient->domain);
		get_smtp_response(&s, res);
		if ( res->replyCode >= 300 ) break;

		// Do a DATA
		rcprintf(&s, "DATA\r\n", sender);
		get_smtp_response(&s, res);
		if ( res->replyCode >= 400 ) break;
		while (!feof(fp)) {
			memset(buffer, 0, 2000);
			chunk = fread(buffer, 1, 2000, fp);
			send(c.socket, buffer, chunk, 0);
		}
		rcsend(&s, ".\r\n");
		get_smtp_response(&s, res);
		break;
	}
	fclose(fp);
	rcprintf(&s, "QUIT\r\n", sender);
	if ( c.socket ) close(c.socket);
	return res;
}

void* rumble_worker_process(void* m) {
    mqueue* item;
	char* tmp;
    rumble_mailbox* user;
	void* state;
    ssize_t rc;
    masterHandle* master = (masterHandle*) m;
    sessionHandle* sess = (sessionHandle*) malloc(sizeof(sessionHandle));
    if (!sess) merror();
    sess->_master = m;
    tmp = (char*) calloc(1,256);
    while (1) {
        pthread_mutex_lock(&master->_core.workmutex);
        pthread_cond_wait(&master->_core.workcond, &master->_core.workmutex);
        
        // Make a private copy of the mail struct address and reset the global one
        item = current;
        current = 0;
        pthread_mutex_unlock(&master->_core.workmutex);
		if (!item) continue;

		// Check for rampant loops
		item->loops++;
		if ( item->loops > 5 ) { 
			if ( item->recipient ) rumble_free_address(item->recipient);
			if ( item->sender ) rumble_free_address(item->recipient);
			if ( item->fid) free((char*) item->fid);
			if ( item->flags) free((char*) item->flags);
			free(item);
			printf("Mail is looping, ignoring it!\n");
			continue;
		}

        // Local delivery?
        if ( rumble_domain_exists(item->recipient->domain)) {
            printf("%s is local domain, looking for user %s@%s\n", item->recipient->domain, item->recipient->user, item->recipient->domain);
            user = rumble_account_data(sess, item->recipient->user, item->recipient->domain);
            if ( user ) {
                uint32_t fsize;
                item->account = user;
                
                // pre-delivery parsing (virus, spam, that sort of stuff)
                rc = rumble_server_schedule_hooks(master, (sessionHandle*)item, RUMBLE_HOOK_PARSER); // hack, hack, hack
                if ( rc == RUMBLE_RETURN_OKAY ) {
                    if ( user->type & RUMBLE_MTYPE_MBOX ) { // mail box
						
						char *ofilename, *nfilename;
                        const char* path;

						printf("Delivering to mailbox %s @ %s...\n", user->user, user->domain->name);
						// Start by making a copy of the letter
						fsize = rumble_copy_mail(master, item->fid,user->user,user->domain->name, (char**) &item->fid);
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
						free(ofilename);
						free(nfilename);

						state = rumble_database_prepare(master->_core.db, \
								"INSERT INTO mbox (uid, fid, size, flags) VALUES (%u, %s, %u,0)",
								item->account->uid, item->fid, fsize);
						rumble_database_run(state);
						rumble_database_cleanup(state);
                        // done here!
                    }
                    if ( user->type & RUMBLE_MTYPE_ALIAS ) { // mail alias
                        if ( strlen(user->arg)) {
                            char* pch = strtok(user->arg," ");
                            char* email = (char*) calloc(1,256);
                            while ( pch != NULL ) {
                                memset(email,0,128);
                                if ( strlen(pch) >= 3 ) {
									char* loops = (char*) calloc(1,4);
									sprintf(loops,"%u",item->loops);
                                    if (sscanf(pch, "%256c", email)) {
                                        rumble_string_lower(email);
										state = rumble_database_prepare(master->_core.db, \
										"INSERT INTO queue (loops, fid, sender, recipient, flags) VALUES (%s,%s,%s,%s,%s)", \
                                         loops, item->fid, item->sender->raw, email, item->flags);
										rumble_database_run(state);
										rumble_database_cleanup(state);
                                    }
                                }
                                pch = strtok(NULL, " ");
                            }
							free(email);
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
			char *filename;
            uint32_t delivered = 500;
			rumble_sendmail_response* res;
            printf ("%s@%s is a foreign user\n",item->recipient->user, item->recipient->domain);

            mx = comm_mxLookup(item->recipient->domain);
            if (!mx) merror();
            if ( cvector_size(mx) ) {
				filename = (char*) calloc(1,256);
				if (!filename) merror();
				sprintf(filename, "%s/%s", rrdict(master->_core.conf, "storagefolder"), item->fid);
                for (mxr = (mxRecord*) cvector_first(mx); mxr != NULL; mxr = (mxRecord*) cvector_next(mx)) {
                    if ( rhdict(badmx, mxr->host) ) continue; // ignore bogus MX records
                    printf("Trying %s (%u)...\n", mxr->host, mxr->preference);
					res = rumble_send_email(master, mxr->host, filename, item->sender, item->recipient); // Anything below 300 would be good :>
					delivered = ( res->replyCode < delivered ) ? res->replyCode : delivered; // get the best result from all servers we've tried
					rumble_flush_dictionary(res->flags);
					free(res->replyMessage);
					free(res);
					if ( delivered <= 299 ) break; // yay!
                }
				free(filename);
            }
			if ( delivered >= 500 ) { // critical failure, giving up.
				printf("Critical failure, giving up for now.\n");
			} 
			else if ( delivered >= 400 ) { // temp failure, push mail back into queue (schedule next try in 30 minutes).
				
				sprintf(tmp, "<%s=%s@%s>", item->sender->tag, item->sender->user, item->sender->domain);
				state = rumble_database_prepare( master->_core.db, \
                        "INSERT INTO queue (time, loops, fid, sender, recipient, flags) VALUES (strftime('%%s', 'now', '+10 minutes'),%u,%s,%s,%s,%s,%s)", \
						item->loops, item->fid, tmp, item->recipient->raw, item->flags);
                rumble_database_run(state);
				rumble_database_cleanup(state);
				memset(tmp, 0, 256);
			} 
			else {
				printf("Mail delivered.\r\n");
			}
			// All done!
        }
		
        if ( item->recipient ) rumble_free_address(item->recipient);
        if ( item->sender ) rumble_free_address(item->recipient);
        if ( item->fid) free((char*) item->fid);
        if ( item->flags) free((char*) item->flags);
		item->account = 0;
        free(item);
        }
    }

void* rumble_worker_init(void* m) {
    masterHandle* master = (masterHandle*) m;
    int x;
	char tmp[1024];
    const char* ignmx;
    const char* statement = "SELECT time, loops, fid, sender, recipient, flags, id FROM queue WHERE time <= strftime('%s','now') LIMIT 1";
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
        pthread_create(t, NULL, rumble_worker_process, m);
    }
    while (1) {
        rc = sqlite3_step(state);
        if ( rc == SQLITE_ROW ) {
            int l;
            uint32_t mid;
            char* sql, *zErrMsg;
            mqueue* item = (mqueue*) calloc(1,sizeof(mqueue));
            if (item) {
				item->mType = 0;

				// delivery time
                item->date = sqlite3_column_int(state, 0);

				// loops
				item->loops = sqlite3_column_int(state, 1);
            
                // fid
                l = sqlite3_column_bytes(state,2);
                item->fid = (char*) calloc(1,l+1);
                memcpy((char*) item->fid, sqlite3_column_text(state,2), l);
            
                // sender
				memset(tmp, 0, 1024);
                l = sqlite3_column_bytes(state,3);
                memcpy((char*) tmp, sqlite3_column_text(state,3), l);
				item->sender = rumble_parse_mail_address(tmp);
				            
                // recipient
				memset(tmp, 0, 1024);
				l = sqlite3_column_bytes(state,4);
				memcpy((char*) tmp, sqlite3_column_text(state,4), l);
				item->recipient = rumble_parse_mail_address(tmp);
            
                //flags
                l = sqlite3_column_bytes(state,5);
                item->flags = (char*) calloc(1,l+1);
                memcpy((char*) item->flags, sqlite3_column_text(state,5), l);
            
                mid = sqlite3_column_int(state, 6);
            
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

