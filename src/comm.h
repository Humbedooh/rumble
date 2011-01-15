/* 
 * File:   comm.h
 * Author: Administrator
 *
 * Created on January 2, 2011, 10:01 AM
 */

#include "rumble.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#ifndef COMM_H
#define	COMM_H

#ifdef RUMBLE_WINSOCK
#define close closesocket
typedef int socklen_t;
typedef uint16_t sa_family_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;
struct addrinfo {
  int             ai_flags;		/* input flags */
  int             ai_family;		/* address family of socket */
  int             ai_socktype;		/* socket type */
  int             ai_protocol;		/* ai_protocol */
  socklen_t       ai_addrlen;		/* length of socket address */
  char            *ai_canonname;	/* canonical name of service location */
  struct sockaddr *ai_addr;		/* socket address of socket */
  struct addrinfo *ai_next;		/* pointer to next in list */
};


struct sockaddr_in6
{
  sa_family_t	  sin6_family;		/* AF_INET6 */
  in_port_t	  sin6_port;		/* Port number. */
  uint32_t	  sin6_flowinfo;	/* Traffic class and flow inf. */
  struct in6_addr sin6_addr;		/* IPv6 address. */
  uint32_t	  sin6_scope_id;	/* Set of interfaces for a scope. */
};
#endif

   void *get_in_addr(struct sockaddr *sa);
    socketHandle comm_init(masterHandle* m, const char* port);
    void comm_accept(socketHandle sock, clientHandle* client);
    cvector* comm_mxLookup(const char* domain);
    

#endif	/* COMM_H */

