/* 
 * File:   main.c
 * Author: Administrator
 *
 * Created on January 2, 2011, 8:22 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include "database.h"
#include "comm.h"
#include "cvector.h"
#include "reply_codes.h"
#include "servers.h"

#define RUMBLE_INITIAL_THREADS 10
/*
 * 
 */
masterHandle* master;
masterHandle* master_ext_copy;

void rumble_master_init(masterHandle* master) {
    master->smtp.cue_hooks = cvector_init();
    master->smtp.init_hooks = cvector_init();
    master->smtp.threads = cvector_init();
}

int main(int argc, char** argv) {
    master = malloc(sizeof(masterHandle));
    master_ext_copy = master;
    struct dbStruct db;
    //rumble_database_init(&db) ? printf("moo") : printf("Database loaded...\n");
    //socketHandle smurf = comm_init("110");
    //struct clientHandle* client = comm_accept(smurf);
    //close(client->socket);
    
    rumble_config_load(master);
    rumble_master_init(master);
    rumble_modules_load(master);
    
    if ( rumble_config_int("enablesmtp") ) {
        printf("Launching SMTP service...");
        master->smtp.socket = comm_init((const char*) rumble_config_str("smtpport"));
        int n;
        for ( n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            pthread_t* t = malloc(sizeof(pthread_t));
            cvector_add(master->smtp.threads, t);
            pthread_create(t, NULL, rumble_smtp_init, master);
        }
        printf("OK\n");
    }
 /*   if ( rumble_config_int("enablepop3") ) {
        printf("Launching POP3 service...");
        master->pop3.socket = comm_init((const char*) rumble_config_str("pop3port"));
        int n;
        for ( n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            pthread_t* t = malloc(sizeof(pthread_t));
            cvector_add(master->pop3.threads, t);
            pthread_create(t, NULL, rumble_pop3_init, master);
        }
        printf("OK\n");
    }
    if ( rumble_config_int("enableimap") ) {
        printf("Launching IMAP service...");
        master->imap.socket = comm_init((const char*) rumble_config_str("imapport"));
        int n;
        for ( n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            pthread_t* t = malloc(sizeof(pthread_t));
            cvector_add(master->imap.threads, t);
            pthread_create(t, NULL, rumble_imap_init, master);
        }
        printf("OK\n");
    }
   printf("Launching IMAP service...");
   int n;
    for ( n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
        pthread_t* t = malloc(sizeof(pthread_t));
        cvector_add(master->readOnly.workers, t);
        pthread_create(t, NULL, rumble_worker_init, master);
    }
    printf("OK\n");*/
    pthread_t* t = (pthread_t*) cvector_last(master->smtp.threads);
    pthread_join(*t, NULL);
    return (EXIT_SUCCESS);
}

