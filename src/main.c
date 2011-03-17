/* File: main.c Author: Administrator Created on January 2, 2011, 8:22 AM */
#include "rumble.h"
#include "database.h"
#include "comm.h"
#include "private.h"
#include "rumble_version.h"
#define RUMBLE_INITIAL_THREADS  25
extern masterHandle *rumble_database_master_handle;
extern int (*lua_callback) (lua_State *, void *, void *);

static dvector* s_args;
 FILE* runlog;

#ifdef RUMBLE_MSC
SERVICE_STATUS          ServiceStatus; 
SERVICE_STATUS_HANDLE   hStatus; 
SERVICE_TABLE_ENTRY ServiceTable[2];
void  ServiceMain(int argc, char** argv); 
void  ControlHandler(DWORD request) {
     switch(request) 
   { 
      case SERVICE_CONTROL_STOP: 
         ServiceStatus.dwWin32ExitCode = 0; 
         ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
         SetServiceStatus (hStatus, &ServiceStatus);
         return; 
 
      case SERVICE_CONTROL_SHUTDOWN: 
         ServiceStatus.dwWin32ExitCode = 0; 
         ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
         SetServiceStatus (hStatus, &ServiceStatus);
         return;
        
      default:
		  ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
         break;
    } 
 
    SetServiceStatus (hStatus, &ServiceStatus);
}
int InitService() {
	rumbleStart();
    ServiceStatus.dwCurrentState = 
      SERVICE_RUNNING; 
   SetServiceStatus (hStatus, &ServiceStatus);
    return 0;
}

void ServiceMain(int argc, char** argv) 
{ 
   int error; 
	fprintf(runlog, "Running ServiceMain()\r\n");
   ServiceStatus.dwServiceType = 
      SERVICE_WIN32; 
   ServiceStatus.dwCurrentState = 
      SERVICE_START_PENDING; 
   ServiceStatus.dwControlsAccepted   =  
      SERVICE_ACCEPT_STOP | 
      SERVICE_ACCEPT_SHUTDOWN;
   ServiceStatus.dwWin32ExitCode = 0; 
   ServiceStatus.dwServiceSpecificExitCode = 0; 
   ServiceStatus.dwCheckPoint = 0; 
   ServiceStatus.dwWaitHint = 0; 
 
   hStatus = RegisterServiceCtrlHandler(
      L"Rumble Mail Server", 
      (LPHANDLER_FUNCTION)ControlHandler); 
   if (hStatus == (SERVICE_STATUS_HANDLE)0) 
   { 
      // Registering Control Handler failed
	   fprintf(runlog, "Register failed!\r\n");
      return; 
   }  
   fprintf(runlog, "Initializing service\r\n");
   // Initialize Service 
   error = InitService(); 
   fprintf(runlog, "Init done\r\n");
   fflush(runlog);
   if (error) 
   {
	   fprintf(runlog, "rumbleStart returned badly\r\n");
      // Initialization failed
      ServiceStatus.dwCurrentState = 
         SERVICE_STOPPED; 
      ServiceStatus.dwWin32ExitCode = -1; 
      SetServiceStatus(hStatus, &ServiceStatus); 
	  
      return; 
   } 
   fprintf(runlog, "Giving the OK\r\n");
   // We report the running status to SCM. 
   ServiceStatus.dwCurrentState = 
      SERVICE_RUNNING; 
   SetServiceStatus (hStatus, &ServiceStatus);
 
   
   // The worker loop of a service
   while (ServiceStatus.dwCurrentState == 
          SERVICE_RUNNING)
   {

      Sleep(999);
   }
   fprintf(runlog, "Program halted\r\n");
   fclose(runlog);
   exit(EXIT_SUCCESS);
   return; 
}


#endif
/*
 =======================================================================================================================
 =======================================================================================================================
 */
int rumbleStart() {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    pthread_t       *t;
    pthread_attr_t  attr;
	masterHandle    *master;
	
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	fprintf(runlog, "Running rumbleStart()\r\n");
	fflush(runlog);
    master = (masterHandle *) malloc(sizeof(masterHandle));
    if (!master) merror();
    master->_core.uptime = time(0);
	
	lua_callback = rumble_lua_callback;
    printf("Starting Rumble Mail Server (v/%u.%02u.%04u)\r\n", RUMBLE_MAJOR, RUMBLE_MINOR, RUMBLE_REV);
    srand(time(0));
    rumble_database_master_handle = master;
    rumble_config_load(master, s_args);
    rumble_master_init(master);
	if (rhdict(s_args, "execpath")) rsdict(master->_core.conf, "execpath", rrdict(s_args, "execpath"));
	fprintf(runlog,"Loading db...\r\n");
	fflush(runlog);
    rumble_database_load(master, runlog);
    
	fprintf(runlog,"Loading modules...\r\n");
	fflush(runlog);
	rumble_database_update_domains();
    rumble_modules_load(master, runlog);
    printf("%-48s", "Launching core service...");
	fprintf(runlog,"%-48s", "Launching core service...\r\n");
	fflush(runlog);
    pthread_attr_init(&attr);
    t = (pthread_t *) malloc(sizeof(pthread_t));
	dvector_add(master->mailman.threads, t);
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
	if (rhdict(s_args, "--service")) {
		fprintf(runlog, "rumbleStart() detected --service, returning to ctrl thread\r\n");
		return (EXIT_SUCCESS);
	}
    fflush(runlog);
    sleep(999999);
    return (EXIT_SUCCESS);
}


int main(int argc, char **argv) {
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             x;
	char r_path[512];
        s_args = dvector_init();
	runlog = fopen("rumble_status.log", "w");
	if (argc) {
            char* m = argv[0], *n;
            memset(r_path, 0, 512);
#ifndef RUMBLE_MSC
            while (m != NULL) {
                n = strchr(m+1, '/');
                if (n) {m = n;}
                else {break;}
            }
            strncpy(r_path, argv[0], strlen(argv[0])-strlen(m));
            chdir(r_path);

#else
            while (m != NULL) {
                n = strchr(m+1, '\\');
                if (n) m = n;
                else break;
            }
            strncpy(r_path, argv[0], strlen(argv[0])-strlen(m));
            SetCurrentDirectoryA(r_path);
#endif
            if (strlen(r_path)) {
                fprintf(runlog, "Entering directory: %s\r\n", r_path);
                rsdict(s_args, "execpath", r_path);
            }
	}
    for (x = 0; x < argc; x++) {
        rumble_scan_flags(s_args, argv[x]);
        rsdict(s_args, argv[x], "true");
        
    }

	fprintf(runlog, "Running main()\r\n");
	fflush(runlog);

    if (rhdict(s_args, "--service")) {
#ifndef RUMBLE_MSC
        /*~~~~~~~~~~~~~*/
        int pid = fork();
		
        /*~~~~~~~~~~~~~*/

        if (pid != 0) exit(EXIT_SUCCESS);
        
        fclose(stdout);
        setsid();
		rumbleStart();
#else
		
		ServiceTable[0].lpServiceName = L"Rumble Mail Server";
	   ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

	   ServiceTable[1].lpServiceName = NULL;
	   ServiceTable[1].lpServiceProc = NULL;
	   // Start the control dispatcher thread for our service
	   StartServiceCtrlDispatcher(ServiceTable);
	   while(1) sleep(3600);
#endif
	   return 0;
    }
	else {
		rumbleStart();
                return 0;
	}
}

    
