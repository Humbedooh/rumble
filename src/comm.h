/* File: comm.h Author: Administrator Created on January 2, 2011, 10:01 AM */
#ifndef COMM_H
#   define COMM_H
#   include "rumble.h"
#   include <errno.h>
#   include <signal.h>
#   ifndef SOCKET_ERROR
#      define SOCKET_ERROR    - 1
#   endif
#   ifndef TCP_NODELAY
#      define TCP_NODELAY 0x200
#   endif
void            *get_in_addr(struct sockaddr *sa);
socketHandle    comm_init(masterHandle *m, const char *port);
void            comm_accept(socketHandle sock, clientHandle *client);
int             *rumble_comm_read_waitForInput(sessionHandle *session, int timeout);
dvector         *comm_mxLookup(const char *domain);
void            comm_mxFree(dvector *list);
socketHandle    comm_open(masterHandle *m, const char *host, unsigned short port);
rumbleService   *comm_registerService(masterHandle *m, const char *svcName, void * (*init) (void *), const char *port, int threadCount);
int             comm_setServiceStack(rumbleService *svc, unsigned int stacksize);
int             comm_startService(rumbleService *svc);
rumbleService   *comm_serviceHandleExtern(masterHandle *m, const char *svcName);
rumbleService   *comm_serviceHandle(const char *svcName);
int             comm_suspendService(rumbleService *svc);
int             comm_killService(rumbleService *svc);
int             comm_resumeService(rumbleService *svc);
#endif /* COMM_H */
