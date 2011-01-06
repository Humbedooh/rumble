/* 
 * File:   comm.h
 * Author: Administrator
 *
 * Created on January 2, 2011, 10:01 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <resolv.h>
#include "rumble.h"
#include "database.h"

#define NUMCON 10

#ifndef COMM_H
#define	COMM_H

    void *get_in_addr(struct sockaddr *sa);
    socketHandle comm_init(const char* port);
    clientHandle* comm_accept(socketHandle sock);
    void comm_mxLookup(const char* domain);
    

#endif	/* COMM_H */

