#include "rumble.h"
#include <sqlite3.h>

mqueue* current = 0;

void* rumble_worker_process(void* m) {
    masterHandle* master = (masterHandle*) m;
    sessionHandle* sess = malloc(sizeof(sessionHandle));
    sess->_master = m;
    while (1) {
        pthread_cond_wait(&master->readOnly.workcond, &master->readOnly.workmutex);
        // do stuff here
        mqueue* item = current;
        pthread_mutex_unlock(&master->readOnly.workmutex);
        printf("Got a request to handle mail from %s\n", item->sender.raw);
        
        // Local delivery?
        if ( rumble_domain_exists(sess, item->recipient.domain)) {
            printf("Domain %s is local, checking accounts...\n", item->recipient.domain);
            userAccount* user = rumble_get_account(master, item->recipient.user, item->recipient.domain);
            if ( user ) {
                printf("%s matched user %d (%s@%s) of type %d\n", item->recipient.user, user->uid, user->user, user->domain, user->type);
            }
            else printf("No records found matching %s@%s :/ \n", item->recipient.user, item->recipient.domain);
            
        }
        // Foreign delivery?
        else {
            
        }
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

