/* File: main.c Author: Administrator Created on January 2, 2011, 8:22 AM */
#define __USE_GNU
#define _GNU_SOURCE
#include "rumble.h"
#include "database.h"
#include "comm.h"
#include "private.h"
#include "rumble_version.h"
#ifndef RUMBLE_MSC
#   include <sys/types.h>
#   include <pwd.h>
#endif
extern masterHandle *rumble_database_master_handle;
extern masterHandle *public_master_handle;
extern masterHandle *comm_master_handle;
extern int (*lua_callback) (lua_State *, void *, void *);
FILE                    *sysLog;
extern dvector          *debugLog;
extern char             shutUp;
static dvector          *s_args;
char                    *executable;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int rumbleStart(void) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    masterHandle    *master = 0;
    int             rc,
                    x;
    rumbleService   *svc;
#ifndef RUMBLE_MSC
    struct passwd   *runAsEntry;
    __uid_t         runAsUID = 999999;
#endif
    const char      *runAsName;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    srand(time(NULL));
    master = (masterHandle *) malloc(sizeof(masterHandle));
    if (!master) merror();
    rumble_database_master_handle = master;
    public_master_handle = master;
    comm_master_handle = master;
    rumble_debug(NULL, "startup", "Starting Rumble Mail Server (v/%u.%02u.%04u)", RUMBLE_MAJOR, RUMBLE_MINOR, RUMBLE_REV);
    master->_core.uptime = time(0);
    lua_callback = rumble_lua_callback;
    master->_core.modules = dvector_init();
    master->_core.batv = dvector_init();
    master->_core.parser_hooks = cvector_init();
    master->_core.feed_hooks = cvector_init();
    master->domains.list = dvector_init();
    master->domains.rrw = rumble_rw_init();
    master->mailboxes.rrw = rumble_rw_init();
    master->mailboxes.list = dvector_init();
    master->mailboxes.bags = cvector_init();
    master->services = cvector_init();
    master->debug.logfile = sysLog;
    master->debug.logvector = debugLog;
    pthread_mutex_init(&master->lua.mutex, 0);
    for (x = 0; x < RUMBLE_LSTATES; x++) {
        master->lua.states[x].state = 0;
        master->lua.states[x].working = 0;
    }
	

    srand(time(0));
    rumble_config_load(master, s_args);
    if (rhdict(s_args, "execpath")) rsdict(master->_core.conf, "execpath", rrdict(s_args, "execpath"));
    rumble_database_load(master, 0);
    rumble_database_update_domains();
    printf("%-48s", "Launching core service...");
    rumble_debug(NULL, "startup", "Launching mailman service");
    svc = comm_registerService(master, "mailman", rumble_worker_init, 0, 1);
    comm_setServiceStack(svc, 128 * 1024);
    rc = comm_startService(svc);
    svc = comm_registerService(master, "smtp", rumble_smtp_init, rumble_config_str(master, "smtpport"), RUMBLE_INITIAL_THREADS);
    comm_setServiceStack(svc, 128 * 1024);  /* Set stack size for service to 128kb (should be enough) */
    if (rumble_config_int(master, "enablesmtp")) {
        rumble_debug(NULL, "core", "Launching SMTP service");
        rc = comm_startService(svc);
        if (!rc) {
            rumble_debug(NULL, "core", "ABORT: Couldn't create socket for service!");
            exit(EXIT_SUCCESS);
        }
    }

    svc = comm_registerService(master, "pop3", rumble_pop3_init, rumble_config_str(master, "pop3port"), RUMBLE_INITIAL_THREADS);
    comm_setServiceStack(svc, 256 * 1024);  /* Set stack size for service to 256kb (should be enough) */
    if (rumble_config_int(master, "enablepop3")) {
        rumble_debug(NULL, "core", "Launching POP3 service...");
        rc = comm_startService(svc);
        if (!rc) {
            rumble_debug(NULL, "core", "ABORT: Couldn't create socket for service!");
            exit(EXIT_SUCCESS);
        }

        printf("[OK]\n");
    }

    svc = comm_registerService(master, "imap4", rumble_imap_init, rumble_config_str(master, "imap4port"), RUMBLE_INITIAL_THREADS);
    comm_setServiceStack(svc, 512 * 1024);  /* Set stack size for service to 512kb (should be enough) */
    if (rumble_config_int(master, "enableimap4")) {
        rumble_debug(NULL, "core", "Launching IMAP4 service...");
        rc = comm_startService(svc);
        if (!rc) {
            rumble_debug(NULL, "startup", "ABORT: Couldn't create socket for service!");
            exit(EXIT_SUCCESS);
        }
    }

    rumble_master_init(master);
    rumble_modules_load(master);

/*$3
 =======================================================================================================================
    Change into running as RunAs user after creating sockets and setting up the server
 =======================================================================================================================
 */

#ifndef RUMBLE_MSC
    runAsName = rhdict(master->_core.conf, "runas") ? rrdict(master->_core.conf, "runas") : "";
    if (strlen(runAsName)) {
        if (!strcmp(runAsName, "root")) runAsUID = 0;
        else {
            runAsEntry = getpwnam(runAsName);
            if (runAsEntry && runAsEntry->pw_uid) {
                runAsUID = runAsEntry->pw_uid;
            }
        }

        if (runAsUID != 999999) {
            rumble_debug(NULL, "core", "Running as user: %s", runAsName);
            if (setuid(runAsUID)) {
                rumble_debug(NULL, "core", "Error: Could not set process UID to %u!", runAsEntry->pw_uid);
                exit(EXIT_FAILURE);
            }
        } else {
            rumble_debug(NULL, "core", "I couldn't find the user to run as: %s", runAsName);
            exit(EXIT_FAILURE);
        }
    } else rumble_debug(NULL, "core", "no run-as directive set, running as root(?)");
#endif

    /*$3
     ===================================================================================================================
        End RunAs directive
     ===================================================================================================================
     */

    if (rhdict(s_args, "--service")) {
        rumble_debug(NULL, "startup", "--service enabled, going stealth.");
        shutUp = 1;
    }

    rumble_debug(NULL, "startup", "Rumble is up and running, listening for incoming calls!");
#ifdef RUMBLE_MSC
    if (rhdict(s_args, "--service")) {
        cleanup();
        return (0);
    }
#endif
    while (1) {
        cleanup();
        sleep(60);
    }

    return (EXIT_SUCCESS);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int main(int argc, char **argv) {

    /*~~~~~~~~~~~~~~~~*/
    int     x;
    char    r_path[512];
    char    *dstring;
    /*~~~~~~~~~~~~~~~~*/

    executable = *(argv);
    shutUp = 0;
    fflush(stdout);
    s_args = dvector_init();
    memset(r_path, 0, 512);
    if (argc) {

        /*~~~~~~~~~~~~~~~~~*/
        char    *m = argv[0],
                *n;
        /*~~~~~~~~~~~~~~~~~*/

#ifndef RUMBLE_MSC
        while (m != NULL) {
            n = strchr(m + 1, '/');
            if (n) {
                m = n;
            } else {
                break;
            }
        }

        strncpy(r_path, argv[0], strlen(argv[0]) - strlen(m));
        if (chdir(r_path) == -1) { };
#else
        while (m != NULL) {
            n = strchr(m + 1, '\\');
            if (n) m = n;
            else break;
        }

        strncpy(r_path, argv[0], strlen(argv[0]) - strlen(m));
        SetCurrentDirectoryA(r_path);
#endif
    }

    for (x = 0; x < argc; x++) {
        rumble_scan_flags(s_args, argv[x]);
        rsdict(s_args, argv[x], "true");
    }

    debugLog = dvector_init();
    for (x = 0; x < 500; x++) {
        dstring = (char *) calloc(1, 512);
        dvector_add(debugLog, dstring);
    }

    if (strlen(r_path)) {

        /*~~~~~~~~~~~~~~~~~~*/
        char    tmpfile[1024];
        /*~~~~~~~~~~~~~~~~~~*/

        sprintf(tmpfile, "%s/rumble_status.log", r_path);
        sysLog = fopen(tmpfile, "w");
    } else sysLog = fopen("rumble_status.log", "w");
    if (!sysLog) {
        printf("Error: Couldn't open rumble_status.log for writing.\r\nEither rumble is already running, or I don't have access to write to this folder.\r\n");
        exit(0);
    }
    if (strlen(r_path)) {
        rumble_debug(NULL, "startup", "Entering directory: %s", r_path);
        rsdict(s_args, "execpath", r_path);
    }

    attach_debug();
    if (rhdict(s_args, "--service")) {
        shutUp = 1;

        /*~~~~~~~~~~~~~*/
#ifndef RUMBLE_MSC
        int pid = fork();
        /*~~~~~~~~~~~~~*/

        if (pid != 0) exit(EXIT_SUCCESS);
        setsid();
        printf("Starting rumble v/%u.%u.%u as daemon\n", RUMBLE_MAJOR, RUMBLE_MINOR, RUMBLE_REV);
        fclose(stdout);
        rumbleStart();
#else
        windows_service_start();
#endif
        return (0);
    } else {
        rumbleStart();
        return (0);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void cleanup(void) {

    /*~~~~~~~~~~~~~~~~~~~*/
    dvector_element *obj;
    const char      *entry;
    /*~~~~~~~~~~~~~~~~~~~*/

    if (sysLog) {
        rewind(sysLog);
        obj = debugLog->last;
        while (obj) {
            entry = (char *) obj->object;
            if (entry && strlen(entry)) {
                fprintf(sysLog, "%s", entry);
            }

            obj = obj->prev;
        }

        fflush(sysLog);
    }
}
