/* File: main.c Author: Administrator Created on January 2, 2011, 8:22 AM */
#include "rumble.h"
#include "database.h"
#include "comm.h"
#include "private.h"
#include "rumble_version.h"
#define RUMBLE_INITIAL_THREADS  25
extern masterHandle *rumble_database_master_handle;
extern int (*lua_callback) (lua_State *, void *, void *);
extern FILE* sysLog;
static dvector* s_args;

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
   statusLog("Running as a Windows Service");
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
       statusLog("ERROR: Couldn't register as a Windows service");
      return; 
   }  
   statusLog("Successfully registered as service, running main processes");
   // Initialize Service 
   error = InitService(); 
   statusLog("Returned from main process");
   if (error) 
   {
       statusLog("ERROR: rumbleStart() returned badly, shutting down.");
      // Initialization failed
      ServiceStatus.dwCurrentState = 
         SERVICE_STOPPED; 
      ServiceStatus.dwWin32ExitCode = -1; 
      SetServiceStatus(hStatus, &ServiceStatus); 
	  
      return; 
   } 
   statusLog("Sending SERVICE_RUNNING status to Windows Services");
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
   statusLog("EXIT: Program halted by services, shutting down.");
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
	
    master = (masterHandle *) malloc(sizeof(masterHandle));
    if (!master) merror();
    master->_core.uptime = time(0);
	
	lua_callback = rumble_lua_callback;
    printf("Starting Rumble Mail Server (v/%u.%02u.%04u)\r\n", RUMBLE_MAJOR, RUMBLE_MINOR, RUMBLE_REV);
    statusLog("Starting Rumble Mail Server (v/%u.%02u.%04u)", RUMBLE_MAJOR, RUMBLE_MINOR, RUMBLE_REV);
    srand(time(0));
    rumble_database_master_handle = master;
    rumble_config_load(master, s_args);
    rumble_master_init(master);
    if (rhdict(s_args, "execpath")) rsdict(master->_core.conf, "execpath", rrdict(s_args, "execpath"));
    rumble_database_load(master,0);
    rumble_database_update_domains();
    rumble_modules_load(master, 0);
    printf("%-48s", "Launching core service...");
    statusLog("Launching core service");
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
        statusLog("Launching SMTP service");
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
        statusLog("Launching POP3 service...");
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
        statusLog("Launching IMAP4 service...");
        master->imap.socket = comm_init(master, rumble_config_str(master, "imap4port"));
        for (n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            t = (pthread_t *) malloc(sizeof(pthread_t));
            dvector_add(master->imap.threads, t);
            pthread_create(t, &attr, master->imap.init, master);
        }

        printf("[OK]\n");
    }
	if (rhdict(s_args, "--service")) {
		statusLog("Core: --service enabled, Listening for demands");
		return (EXIT_SUCCESS);
	}
    statusLog("Rumble is up and running, listening for incoming calls!");
    sleep(999999);
    return (EXIT_SUCCESS);
}


int main(int argc, char **argv) {
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             x;
	char r_path[512];
        s_args = dvector_init();
        memset(r_path, 0, 512);
	if (argc) {
            char* m = argv[0], *n;
            
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
	}
    for (x = 0; x < argc; x++) {
        rumble_scan_flags(s_args, argv[x]);
        rsdict(s_args, argv[x], "true");
        
    }
    if (strlen(r_path)) {
        char tmpfile[1024];
        sprintf(tmpfile, "%s/rumble_status.log", r_path);
        printf("opening %s\n", tmpfile);
        sysLog = fopen(tmpfile, "a");
    }
    else sysLog = fopen("rumble_status.log", "a");
    
    fprintf(sysLog, "\r\n------------------------------------------------------\r\n");
    statusLog("New instance of Rumble started");
    fprintf(sysLog, "------------------------------------------------------\r\n");
    if (strlen(r_path)) {
        printf("Entering %s\r\n", r_path);
        statusLog("Entering directory: %s", r_path);
        rsdict(s_args, "execpath", r_path);
    }

    statusLog("Parsing exec arguments");
	

    if (rhdict(s_args, "--service")) {
        statusLog("--service detected, launching daemon process");
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

    
