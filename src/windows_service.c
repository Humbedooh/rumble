#include "private.h"

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
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(hStatus, &ServiceStatus);
    rumbleStart();
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
	int x = 0;
    /*~~~~~~*/

    rumble_debug(NULL, "startup", "Running as a Windows Service");
    ServiceStatus.dwServiceType = SERVICE_WIN32;
    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwServiceSpecificExitCode = 0;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;
    hStatus = RegisterServiceCtrlHandlerA("Rumble Mail Server", (LPHANDLER_FUNCTION) ControlHandler);
    if (hStatus == (SERVICE_STATUS_HANDLE) 0) {

        /* Registering Control Handler failed */
        rumble_debug(NULL, "startup", "ERROR: Couldn't register as a Windows service");
        return;
    }

    rumble_debug(NULL, "startup", "Successfully registered as service, running main processes");

    /* Initialize Service */
    error = InitService();
    rumble_debug(NULL, "startup", "Returned from main process");
    if (error) {
        rumble_debug(NULL, "startup", "ERROR: rumbleStart() returned badly, shutting down.");

        /* Initialization failed */
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        ServiceStatus.dwWin32ExitCode = -1;
        SetServiceStatus(hStatus, &ServiceStatus);
        return;
    }

    rumble_debug(NULL, "startup", "Sending SERVICE_RUNNING status to Windows Services");

    /* We report the running status to SCM. */
    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);

    /* The worker loop of a service */
    while (ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
        Sleep(3);
		x++;
		if (x == 20) {
			x = 0;
			cleanup();
		}
    }

    rumble_debug(NULL, "startup", "EXIT: Program halted by services, shutting down.");
    exit(EXIT_SUCCESS);
    return;
}


#endif
void windows_service_start() {
#ifdef RUMBLE_MSC
    ServiceTable[0].lpServiceName = "Rumble Mail Server";
    ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTIONA) ServiceMain;
    ServiceTable[1].lpServiceName = NULL;
    ServiceTable[1].lpServiceProc = NULL;

    /* Start the control dispatcher thread for our service */
    StartServiceCtrlDispatcherA(ServiceTable);
    while (1) sleep(3600);
#endif
}