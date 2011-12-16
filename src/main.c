/* File: main.c Author: Administrator Created on January 2, 2011, 8:22 AM */
#define __USE_GNU 
#define _GNU_SOURCE
#include "rumble.h"
#include "database.h"
#include "comm.h"
#include "private.h"
#include "rumble_version.h"
static void         cleanup(void);
extern masterHandle *rumble_database_master_handle;
extern masterHandle *public_master_handle;
extern masterHandle *comm_master_handle;
extern int (*lua_callback) (lua_State *, void *, void *);
FILE                    *sysLog;
extern dvector          *debugLog;
extern char             shutUp;
static dvector          *s_args;
char                    *executable;
#ifdef RUMBLE_MSC
SERVICE_STATUS          ServiceStatus;
SERVICE_STATUS_HANDLE   hStatus;
SERVICE_TABLE_ENTRYA    ServiceTable[2];
void                    ServiceMain(int argc, char **argv);

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void ControlHandler(DWORD request) {
    switch (request)
    {
    case SERVICE_CONTROL_STOP:
        ServiceStatus.dwWin32ExitCode = 0;
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(hStatus, &ServiceStatus);
        return;

    case SERVICE_CONTROL_SHUTDOWN:
        ServiceStatus.dwWin32ExitCode = 0;
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(hStatus, &ServiceStatus);
        return;

    default:
        ServiceStatus.dwCurrentState = SERVICE_RUNNING;
        break;
    }

    SetServiceStatus(hStatus, &ServiceStatus);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int InitService(void) {
    rumbleStart();
    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void ServiceMain(int argc, char **argv) {

    /*~~~~~~*/
    int error;
    /*~~~~~~*/

    shutUp = 1;
    rumble_debug("startup", "Running as a Windows Service");
    ServiceStatus.dwServiceType = SERVICE_WIN32;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwServiceSpecificExitCode = 0;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;
    hStatus = RegisterServiceCtrlHandlerA("Rumble Mail Server", (LPHANDLER_FUNCTION) ControlHandler);
    if (hStatus == (SERVICE_STATUS_HANDLE) 0) {

        /* Registering Control Handler failed */
        rumble_debug("startup", "ERROR: Couldn't register as a Windows service");
        return;
    }

    rumble_debug("startup", "Successfully registered as service, running main processes");

    /* Initialize Service */
    error = InitService();
    rumble_debug("startup", "Returned from main process");
    if (error) {
        rumble_debug("startup", "ERROR: rumbleStart() returned badly, shutting down.");

        /* Initialization failed */
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        ServiceStatus.dwWin32ExitCode = -1;
        SetServiceStatus(hStatus, &ServiceStatus);
        return;
    }

    rumble_debug("startup", "Sending SERVICE_RUNNING status to Windows Services");

    /* We report the running status to SCM. */
    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);

    /* The worker loop of a service */
    while (ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
        Sleep(999);
    }

    rumble_debug("startup", "EXIT: Program halted by services, shutting down.");
    exit(EXIT_SUCCESS);
    return;
}

#else
#   include <signal.h>
#   include <execinfo.h>
#   include <signal.h>
#   include <errno.h>
#   include <ucontext.h>
#   include <unistd.h>
#   include <limits.h>
#   include <sys/types.h>


/*
 -----------------------------------------------------------------------------------------------------------------------
    This structure mirrors the one found in /usr/include/asm/ucontext.h
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct _sig_ucontext
{
    unsigned long       uc_flags;
    struct ucontext     *uc_link;
    stack_t             uc_stack;
    struct sigcontext   uc_mcontext;
    sigset_t            uc_sigmask;
} sig_ucontext_t;
static void         signal_handler(int sig, siginfo_t *info, void *ucontext);
void                init_signals(void);
struct sigaction    sigact;
uint32_t            lastClick = 0;
int                 alreadyDead = 0;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void signal_handler(int sig, siginfo_t *info, void *ucontext) {
    if (sig == SIGHUP) printf("FATAL: Program hung up\n");
    else if (sig == SIGQUIT) {
        printf("User ended the program - bye bye!\r\n");
        cleanup();
    }
    else if (sig == SIGKILL) {
        printf("Rumble got killed :(\r\n");
        cleanup();
    }
    else if (sig == SIGTERM) {
        printf("Rumble got killed :(\r\n");
        cleanup();
    }
    else if (sig == SIGINT) {
        if (time(0) - lastClick < 2) { cleanup(); exit(0); }
        printf("Ctrl+C detected. Press it again to exit rumble.\r\n");
        lastClick = time(0);
    }
    else {
        if (!alreadyDead) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            void            *array[50];
            char            **messages;
            int             size,
                            i;
            sig_ucontext_t  *uc;
            ucontext_t *context;
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            alreadyDead++;
            uc = (sig_ucontext_t *) ucontext;
            context = (ucontext_t*) ucontext;

            rumble_debug("debug", "Caught signal %d (%s), address is %p\n", sig, strsignal(sig), info->si_addr);
            
            rumble_debug("debug", "PID=%d \n", getpid ());
            rumble_debug("debug", "signo=%d/%s\n", sig, strsignal (sig));
            rumble_debug("debug",  "code=%d (not always applicable)\n", info->si_code);
            rumble_debug("debug",  "\nContext: 0x%08lx\n", (unsigned long) ucontext);
            rumble_debug("debug", "Register stuff:\n    gs: 0x%08x   fs: 0x%08x   es: 0x%08x   ds: 0x%08x\n"
               "   edi: 0x%08x  esi: 0x%08x  ebp: 0x%08x  esp: 0x%08x\n"
               "   ebx: 0x%08x  edx: 0x%08x  ecx: 0x%08x  eax: 0x%08x\n"
               "  trap:   %8u  err: 0x%08x  cs: 0x%08x\n",
               context->uc_mcontext.gregs [23],     context->uc_mcontext.gregs [22],   context->uc_mcontext.gregs [24],  context->uc_mcontext.gregs [25],
               context->uc_mcontext.gregs [7],    context->uc_mcontext.gregs [6],  context->uc_mcontext.gregs [5], context->uc_mcontext.gregs [4],
               context->uc_mcontext.gregs [3],    context->uc_mcontext.gregs [2],  context->uc_mcontext.gregs [1], context->uc_mcontext.gregs [0],
               context->uc_mcontext.gregs [15], context->uc_mcontext.gregs [16],   context->uc_mcontext.gregs [18] );
           
            size = backtrace(array, 50);

            
            messages = backtrace_symbols(array, size);

            /* skip first stack frame (points here) */
            for (i = 1; i < size && messages != NULL; ++i) {
                rumble_debug("debug", "[backtrace]: (%d) %s\n", i, messages[i]);
            }

            cleanup();
        } else exit(0);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void init_signals(void) {
    sigact.sa_sigaction = signal_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGKILL, &sigact, 0) < 0) printf("Couldn't lock onto SIGKILL\n");
    sigaction(SIGINT, &sigact, 0);
 
    /*
     * sigaddset(&sigact.sa_mask, SIGSEGV);
     */
    sigaction(SIGSEGV, &sigact, 0);
        sigaction(SIGSTKFLT, &sigact, 0);

    /*
     * sigaddset(&sigact.sa_mask, SIGBUS);
     */
    sigaction(SIGBUS, &sigact, 0);

    /*
     * sigaddset(&sigact.sa_mask, SIGQUIT);
     */
    sigaction(SIGQUIT, &sigact, 0);

    /*
     * sigaddset(&sigact.sa_mask, SIGHUP);
     */
    sigaction(SIGHUP, &sigact, 0);

    /*
     * sigaddset(&sigact.sa_mask, SIGKILL);
     * *sigaddset(&sigact.sa_mask, SIGTERM);
     */
    sigaction(SIGKILL, &sigact, (struct sigaction *) NULL);
}

#   ifdef DUMPSTACK

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void dumpstack(void) {

    /*~~~~~~~~~~~~~*/
    char    gdb[160];
    /*~~~~~~~~~~~~~*/

    sprintf(gdb, "echo 'where\ndetach' | gdb --quiet %s %d > %s_dump.log", executable, getpid(), executable);

    /* Change the dbx to gdb */
    system(gdb);
    return;
}
#   endif
#endif

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int rumbleStart(void) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    masterHandle    *master = 0;
    int             rc,
                    x;
    rumbleService   *svc;
    /*~~~~~~~~~~~~~~~~~~~~*/

    srand(time(NULL));
    master = (masterHandle *) malloc(sizeof(masterHandle));
    if (!master) merror();
    rumble_database_master_handle = master;
    public_master_handle = master;
    comm_master_handle = master;
    rumble_debug("startup", "Starting Rumble Mail Server (v/%u.%02u.%04u)", RUMBLE_MAJOR, RUMBLE_MINOR, RUMBLE_REV);
    master->_core.uptime = time(0);
    lua_callback = rumble_lua_callback;
    master->_core.modules = dvector_init();
    master->_core.batv = dvector_init();
    master->domains.list = dvector_init();
    master->domains.rrw = rumble_rw_init();
    master->mailboxes.rrw = rumble_rw_init();
    master->mailboxes.list = dvector_init();
    master->services = cvector_init();
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
    rumble_debug("startup", "Launching core service");
    svc = comm_registerService(master, "mailman", rumble_worker_init, 0, 1);
    comm_setServiceStack(svc, 1024 * 1024);
    rc = comm_startService(svc);
    svc = comm_registerService(master, "smtp", rumble_smtp_init, rumble_config_str(master, "smtpport"), RUMBLE_INITIAL_THREADS);
    comm_setServiceStack(svc, 128 * 1024);  /* Set stack size for service to 128kb (should be enough) */
    if (rumble_config_int(master, "enablesmtp")) {
        rumble_debug("core", "Launching SMTP service");
        rc = comm_startService(svc);
        if (!rc) {
            rumble_debug("core", "ABORT: Couldn't create socket for service!");
            exit(EXIT_SUCCESS);
        }
    }

    svc = comm_registerService(master, "pop3", rumble_pop3_init, rumble_config_str(master, "pop3port"), RUMBLE_INITIAL_THREADS);
    comm_setServiceStack(svc, 128 * 1024);  /* Set stack size for service to 256kb (should be enough) */
    if (rumble_config_int(master, "enablepop3")) {
        rumble_debug("core", "Launching POP3 service...");
        rc = comm_startService(svc);
        if (!rc) {
            rumble_debug("core", "ABORT: Couldn't create socket for service!");
            exit(EXIT_SUCCESS);
        }

        printf("[OK]\n");
    }

    svc = comm_registerService(master, "imap4", rumble_imap_init, rumble_config_str(master, "imap4port"), RUMBLE_INITIAL_THREADS);
    comm_setServiceStack(svc, 1024 * 1024); /* Set stack size for service to 1MB (should be enough) */
    if (rumble_config_int(master, "enableimap4")) {
        rumble_debug("core", "Launching IMAP4 service...");
        rc = comm_startService(svc);
        if (!rc) {
            rumble_debug("startup", "ABORT: Couldn't create socket for service!");
            exit(EXIT_SUCCESS);
        }
    }

    rumble_master_init(master);
    rumble_modules_load(master);
    if (rhdict(s_args, "--service")) {
        rumble_debug("startup", "--service enabled, going stealth.");
        return (EXIT_SUCCESS);
    }

    rumble_debug("startup", "Rumble is up and running, listening for incoming calls!");
    sleep(999999);
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
        printf("opening %s\n", tmpfile);
        sysLog = fopen(tmpfile, "w");
    } else sysLog = fopen("rumble_status.log", "w");
    if (!sysLog) {
        printf("Error: Couldn't open rumble_status.log for writing.\r\nEither rumble is already running, or I don't have access to write to this folder.\r\n");
        exit(0);
    }

    fprintf(sysLog, "---------------------------------------------------\r\n");
    fprintf(sysLog, "New instance of Rumble started, clearing log file.\r\n");
    fprintf(sysLog, "---------------------------------------------------\r\n");
    if (strlen(r_path)) {
        rumble_debug("startup", "Entering directory: %s", r_path);
        rsdict(s_args, "execpath", r_path);
    }

    rumble_debug("startup", "Parsing exec arguments");
#ifndef RUMBLE_MSC
    init_signals();
#else
    atexit(&cleanup);
#endif
    if (rhdict(s_args, "--service")) {
        rumble_debug("startup", "--service detected, launching daemon process");
        shutUp = 1;

        /*~~~~~~~~~~~~~*/
#ifndef RUMBLE_MSC
        int pid = fork();
        /*~~~~~~~~~~~~~*/

        if (pid != 0) exit(EXIT_SUCCESS);
        fclose(stdout);
        setsid();
        rumbleStart();
#else
        ServiceTable[0].lpServiceName = "Rumble Mail Server";
        ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTIONA) ServiceMain;
        ServiceTable[1].lpServiceName = NULL;
        ServiceTable[1].lpServiceProc = NULL;

        /* Start the control dispatcher thread for our service */
        StartServiceCtrlDispatcherA(ServiceTable);
        while (1) sleep(3600);
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
    /* Do stuff later... */
    dvector_element *obj;
    const char      *entry;
    /*~~~~~~~~~~~~~~~~~~~*/

    rumble_debug("exit", "Cleaning up and writing log file.");
    if (sysLog) {
        obj = debugLog->last;
        while (obj) {
            entry = (char *) obj->object;
            if (entry && strlen(entry)) {
                fprintf(sysLog, "%s\r\n", entry);
            }

            obj = obj->prev;
        }
    }

    /*
     * fclose(sysLog);
     */
}
