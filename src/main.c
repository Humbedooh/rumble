/* 
 * File:   main.c
 * Author: Administrator
 *
 * Created on January 2, 2011, 8:22 AM
 */
#include "rumble.h"
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
    master->readOnly.feed_hooks = cvector_init();
}

int main(int argc, char** argv) {
	int x;
	pthread_t* t;
	cvector* args = cvector_init();
    master = (masterHandle*) malloc(sizeof(masterHandle));
    for (x = 0; x < argc; x++) {
        rumble_scan_flags(args, argv[x]);
    }

    rumble_config_load(master, args);
    rumble_master_init(master);
    rumble_database_load(master);
    rumble_modules_load(master);
    
    if ( rumble_config_int(master, "enablesmtp") ) {
		int n;
        printf("Launching SMTP service...");
        master->smtp.socket = comm_init(master, rumble_config_str(master, "smtpport"));
        for ( n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            t = (pthread_t*) malloc(sizeof(pthread_t));
            cvector_add(master->smtp.threads, t);
            pthread_create(t, NULL, master->smtp.init, master);
        }
        printf("OK\n");
        t = (pthread_t*) malloc(sizeof(pthread_t));
        cvector_add(master->readOnly.workers, t);
        pthread_create(t, NULL, rumble_worker_init, master);
        
    }
 
    
    sleep(999999);
    return (EXIT_SUCCESS);
}

