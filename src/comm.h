<<<<<<< HEAD
=======
/*$T comm.h GC 1.140 02/16/11 21:04:56 */

>>>>>>> 7c6078b307d012f3ab1c0cc605edd7fa50d50252
/* File: comm.h Author: Administrator Created on January 2, 2011, 10:01 AM */
#ifndef COMM_H
#   define COMM_H
#   include "rumble.h"
#   include <errno.h>
#   include <signal.h>
#   ifndef SOCKET_ERROR
#      define SOCKET_ERROR    - 1
#   endif
void            *get_in_addr(struct sockaddr *sa);
socketHandle    comm_init(masterHandle *m, const char *port);
void            comm_accept(socketHandle sock, clientHandle *client);
void            comm_starttls(sessionHandle *session);
void            comm_stoptls(sessionHandle *session);
cvector         *comm_mxLookup(const char *domain);
socketHandle    comm_open(masterHandle *m, const char *host, unsigned short port);
#endif /* COMM_H */
