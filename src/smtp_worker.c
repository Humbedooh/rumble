#include "rumble.h"
#include <sqlite3.h>
#include "servers.h"
#include "private.h"
#include "database.h"

mqueue* current;

void* rumble_worker_process(void* m) {
    masterHandle* master = (masterHandle*) m;
    sessionHandle* sess = malloc(sizeof(sessionHandle));
    sess->_master = m;
    while (1) {
        pthread_cond_wait(&master->readOnly.workcond, &master->readOnly.workmutex);
        // do stuff here
        mqueue* item = current;
        pthread_mutex_unlock(&master->readOnly.workmutex);
        printf("Handling mail no. %s\n", item->fid);
        // Local delivery?
        if ( rumble_domain_exists(sess, item->recipient.domain)) {
            printf("%s is local domain, looking for user %s@%s\n", item->recipient.domain, item->recipient.user, item->recipient.domain);
            userAccount* user = rumble_get_account(master, item->recipient.user, item->recipient.domain);
            if ( user ) {
                item->account = user;
                
                // Start by making a copy of the letter
                item->fid = rumble_copy_mail(master, item->fid,user->user,user->domain);
                
                // pre-delivery parsing (virus, spam, that sort of stuff)
                ssize_t rc = rumble_server_schedule_hooks(master, (sessionHandle*)item, RUMBLE_HOOK_PARSER); // hack, hack, hack
                if ( rc == RUMBLE_RETURN_OKAY ) {
                    if ( user->type & RUMBLE_MTYPE_MBOX ) { // mail box
                        // move file to user's inbox
                        const char* path = rumble_config_str(master, "storagefolder");
                        char* ofilename = calloc(1, strlen(path) + 26);
                        char* nfilename = calloc(1, strlen(path) + 26);
                        sprintf(ofilename, "%s/%s", path, item->fid);
                        sprintf(nfilename, "%s/%s.msg", path, item->fid);
                        #ifdef RUMBLE_DEBUG_STORAGE
                        printf("Moving %s to %s\n", ofilename, nfilename);
                        #endif
                        rename(ofilename, nfilename);
                        // done here!
                    }
                    if ( user->type & RUMBLE_MTYPE_ALIAS ) { // mail alias
                        if ( strlen(user->arg)) {
                            char* pch = strtok(user->arg," ");
                            char* usr = calloc(1,128);
                            char* dmn = calloc(1,128);
                            while ( pch != NULL ) {
                                memset(usr,0,128);
                                memset(dmn,0,128);
                                if ( strlen(pch) >= 3 ) {
                                    sscanf(pch, "%128[^@]@%128c", usr, dmn);
                                    rumble_string_lower(dmn);
                                    rumble_string_lower(usr);
                                    sqlite3_stmt* state = rumble_sql_inject((sqlite3*) master->readOnly.db, \
                                            "INSERT INTO queue (fid, sender, user, domain, flags) VALUES (?,?,?,?,?)", \
                                            item->fid, item->sender.raw, usr, dmn, item->flags);
                                    sqlite3_step(state);
                                    sqlite3_finalize(state);
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
            printf ("%s@%s is a foreign user\n",item->recipient.user, item->recipient.domain);
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
    pthread_cond_init(&master->readOnly.workcond, NULL);
    pthread_mutex_init(&master->readOnly.workmutex, NULL);
    int x;
    for (x = 0; x < 10; x++ ) {
        pthread_t* t = malloc(sizeof(pthread_t));
        cvector_add(master->readOnly.workers, t);
        pthread_create(t, NULL, rumble_worker_process, (void *)m);
    }
    const char* statement = "SELECT time, fid, sender, user, domain, flags, id FROM queue LIMIT 1";
    while (1) {
        sqlite3_stmt* state;
        sqlite3_prepare_v2(master->readOnly.db, statement, -1, &state, NULL);
        int rc = sqlite3_step(state);
        if ( rc == SQLITE_ROW ) {
            mqueue* item = calloc(1,sizeof(mqueue));
            item->date = sqlite3_column_int(state, 0);
            int l;
            
            // fid
            l = sqlite3_column_bytes(state,1);
            item->fid = calloc(1,l+1);
            memcpy((char*) item->fid, sqlite3_column_text(state,1), l);
            
            // sender
            l = sqlite3_column_bytes(state,2);
            item->sender.raw = calloc(1,l+1);
            memcpy((char*) item->sender.raw, sqlite3_column_text(state,2), l);
            
            // recipient
            l = sqlite3_column_bytes(state,3);
            item->recipient.user = calloc(1,l+1);
            memcpy((char*) item->recipient.user, sqlite3_column_text(state,3), l);
            l = sqlite3_column_bytes(state,4);
            item->recipient.domain = calloc(1,l+1);
            memcpy((char*) item->recipient.domain, sqlite3_column_text(state,4), l);
            
            //flags
            l = sqlite3_column_bytes(state,5);
            item->flags = calloc(1,l+1);
            memcpy((char*) item->flags, sqlite3_column_text(state,5), l);
            
            uint32_t mid = sqlite3_column_int(state, 6);
            
            sqlite3_finalize(state);
            char* sql = calloc(1,128);
            sprintf(sql, "DELETE FROM queue WHERE id=%u", mid);
            char *zErrMsg = 0;
            l = sqlite3_exec(master->readOnly.db,sql, 0, 0, &zErrMsg);
            free(sql);
            pthread_mutex_lock(&master->readOnly.workmutex);
            current = item;
            pthread_cond_signal(&master->readOnly.workcond);
            pthread_mutex_unlock(&master->readOnly.workmutex);
            
        }
        else {
            sqlite3_finalize(state);
            sleep(10); // sleep for 10 seconds if there's nothing to do right now.
        }
    }
}

