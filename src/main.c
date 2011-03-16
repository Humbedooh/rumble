/* File: main.c Author: Administrator Created on January 2, 2011, 8:22 AM */
#include "rumble.h"
#include "database.h"
#include "comm.h"
#include "private.h"
#include "rumble_version.h"
#define RUMBLE_INITIAL_THREADS  10
extern masterHandle *rumble_database_master_handle;
extern int (*lua_callback) (lua_State *, void *, void *);

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int main(int argc, char **argv) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             x;
    pthread_t       *t;
    pthread_attr_t  attr;
    masterHandle    *master;
    dvector         *args = dvector_init();
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    master = (masterHandle *) malloc(sizeof(masterHandle));
    if (!master) merror();
    master->_core.uptime = time(0);
    for (x = 0; x < argc; x++) {
        rumble_scan_flags(args, argv[x]);
        printf("%d = %s\n", x, argv[x]);
    }

    if (rhdict(args, "--TEST")) {
        rumble_test();
        exit(EXIT_SUCCESS);
    }

#ifndef RUMBLE_MSC
    if (rhdict(args, "-D")) {

        /*~~~~~~~~~~~~~*/
        int pid = fork();
        /*~~~~~~~~~~~~~*/

        if (pid < 0) exit(EXIT_FAILURE);
        if (pid > 0) exit(EXIT_SUCCESS);
        setsid();
    }
#endif
    lua_callback = rumble_lua_callback;
    printf("Starting Rumble Mail Server (v/%u.%02u.%04u)\r\n", RUMBLE_MAJOR, RUMBLE_MINOR, RUMBLE_REV);
    srand(time(0));
    rumble_database_master_handle = master;
    rumble_config_load(master, args);
    rumble_master_init(master);
    rumble_database_load(master);
    rumble_database_update_domains();
    rumble_modules_load(master);
    printf("%-48s", "Launching core service...");
    pthread_attr_init(&attr);
    t = (pthread_t *) malloc(sizeof(pthread_t));
    dvector_add(master->_core.workers, t);
    pthread_create(t, NULL, rumble_worker_init, master);
    printf("[OK]\n");
    if (rumble_config_int(master, "enablesmtp")) {

        /*~~*/
        int n;
        /*~~*/

        master->smtp.enabled = 1;
        printf("%-48s", "Launching SMTP service...");
        master->smtp.socket = comm_init(master, rumble_config_str(master, "smtpport"));
        for (n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            t = (pthread_t *) malloc(sizeof(pthread_t));
            dvector_add(master->smtp.threads, t);
            pthread_create(t, &attr, master->smtp.init, master);
        }

        printf("[OK]\n");
    }

    if (rumble_config_int(master, "enablepop3")) {

        /*~~*/
        int n;
        /*~~*/

        master->pop3.enabled = 1;
        printf("%-48s", "Launching POP3 service...");
        master->pop3.socket = comm_init(master, rumble_config_str(master, "pop3port"));
        for (n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            t = (pthread_t *) malloc(sizeof(pthread_t));
            dvector_add(master->pop3.threads, t);
            pthread_create(t, &attr, master->pop3.init, master);
        }

        printf("[OK]\n");
    }

    if (rumble_config_int(master, "enableimap4")) {

        /*~~*/
        int n;
        /*~~*/

        master->imap.enabled = 1;
        printf("%-48s", "Launching IMAP4 service...");
        master->imap.socket = comm_init(master, rumble_config_str(master, "imap4port"));
        for (n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            t = (pthread_t *) malloc(sizeof(pthread_t));
            dvector_add(master->imap.threads, t);
            pthread_create(t, &attr, master->imap.init, master);
        }

        printf("[OK]\n");
    }

    sleep(999999);
    return (EXIT_SUCCESS);
}
