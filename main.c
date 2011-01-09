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
#include "private.h"

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
    master->smtp.handles = cvector_init();
    master->smtp.init = rumble_smtp_init;
    pthread_mutex_init(&master->smtp.mutex,0);
    
    master->pop3.cue_hooks = cvector_init();
    master->pop3.init_hooks = cvector_init();
    master->pop3.threads = cvector_init();
    master->pop3.handles = cvector_init();
    master->pop3.init = rumble_pop3_init;
    pthread_mutex_init(&master->pop3.mutex,0);
    
    master->imap.cue_hooks = cvector_init();
    master->imap.init_hooks = cvector_init();
    master->imap.threads = cvector_init();
    master->imap.handles = cvector_init();
    //master->imap.init = rumble_imap_init;
    pthread_mutex_init(&master->imap.mutex,0);
    
    master->readOnly.modules = cvector_init();
    master->readOnly.workers = cvector_init();
    
}

int main(int argc, char** argv) {
    master = malloc(sizeof(masterHandle));
    master_ext_copy = master;
    //struct dbStruct db;
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
            pthread_create(t, NULL, master->smtp.init, master);
        }
        printf("OK\n");
    }
 
    
    sleep(99999999);
    return (EXIT_SUCCESS);
}

