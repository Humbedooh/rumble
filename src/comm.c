/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "comm.h"
#include <stdarg.h>
masterHandle    *comm_master_handle = 0;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return (&(((struct sockaddr_in *) sa)->sin_addr));
    }

    return (&(((struct sockaddr_in6 *) sa)->sin6_addr));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
socketHandle comm_init(masterHandle *m, const char *port) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int                 sockfd; /* our socket! yaaay. */
    struct addrinfo     hints;
    int                 yes = 1;
    const char          *bindTo = 0;
#ifdef RUMBLE_WINSOCK
    struct sockaddr_in  x;
    WSADATA             wsaData;
#endif
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    memset(&hints, 0, sizeof hints);
    hints.ai_family = rumble_config_int(m, "forceipv4") ? AF_INET : AF_UNSPEC;  /* Force IPv4 or use default? */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    /* use my IP */
#ifdef RUMBLE_WINSOCK
    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
        perror("Winsock failed to start");
        exit(EXIT_FAILURE);
    }

    if ((sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0) {
        perror("Winsock: Couldn't create socket");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(int)) == SOCKET_ERROR) {
        perror("setsockopt");
        exit(0);
    }

    x.sin_family = hints.ai_family;
    x.sin_port = htons(atoi(port));
    if (rhdict(m->_core.conf, "bindtoaddress")) bindTo = rrdict(m->_core.conf, "bindtoaddress");
    if (bindTo && strcmp(bindTo, "0.0.0.0")) {
        x.sin_addr.s_addr = inet_addr(bindTo);
    } else x.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    if (bind(sockfd, (struct sockaddr *) &x, sizeof(x)) == SOCKET_ERROR) {
        disconnect(sockfd);
        fprintf(stderr, "Server: failed to bind: %d\n", WSAGetLastError());
        exit(0);
    }

    if (&x == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(0);
    }

#else
    /*~~~~~~~~~~~~~~~~~~~~~~*/
    int             rv;
    struct addrinfo *servinfo,
                    *p;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    if (rhdict(m->_core.conf, "bindtoaddress")) bindTo = rrdict(m->_core.conf, "bindtoaddress");
    if ((rv = getaddrinfo(bindTo, port, &hints, &servinfo)) != 0) {
        rumble_debug(NULL, "comm.c", "ERROR: getaddrinfo: %s\n", gai_strerror(rv));
        return (0);
    }

    /* Loop through all the results and bind to the first we can */
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == SOCKET_ERROR) {
            rumble_debug(NULL, "comm.c", "ERROR: Couldn't create basic socket with protocol %#X!", p->ai_family);
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            rumble_debug(NULL, "comm.c", "ERROR: setsockopt failed!");
            exit(0);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            disconnect(sockfd);
            rumble_debug(NULL, "comm.c", "ERROR: Couldn't bind to socket (protocol %#X) on port %s!", p->ai_family, port);
            continue;
        }
        break;
    }

    if (p == NULL) {
        return (0);
    }

    freeaddrinfo(servinfo);         /* all done with this structure */
#endif
    if (listen(sockfd, 10) == SOCKET_ERROR) {
        rumble_debug(NULL, "comm.c", "ERROR: Couldn't listen on socket on port %s!", port);
        exit(0);
    }

    return (sockfd);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
socketHandle comm_open(masterHandle *m, const char *host, unsigned short port) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    socketHandle        sockfd = 0;
    struct addrinfo     hints;
    int                 yes = 1;
    char                *IP;
    struct hostent      *server;
    struct sockaddr_in  x;
#ifdef RUMBLE_WINSOCK
    WSADATA             wsaData;
#endif
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    memset(&hints, 0, sizeof hints);
    hints.ai_family = rumble_config_int(m, "forceipv4") ? AF_INET : AF_UNSPEC;  /* Force IPv4 or use default? */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    /* use my IP */
#ifdef RUMBLE_WINSOCK
    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
        perror("Winsock failed to start");
        exit(EXIT_FAILURE);
    }

    if ((sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0) {
        perror("Winsock: Couldn't create socket");
    }

#else
    /*~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             rv;
    char            portc[10];
    struct addrinfo *servinfo,
                    *p;
    const char      *bindTo = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~*/

    sprintf(portc, "%u", port);
    if (rhdict(m->_core.conf, "outgoingbindtoaddress")) bindTo = rrdict(m->_core.conf, "outgoingbindtoaddress");
    if ((rv = getaddrinfo(bindTo, portc, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return (0);
    }

    /* Loop through all the results and bind to the first we can */
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == SOCKET_ERROR) {
            perror("server: socket");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);         /* all done with this structure */
#endif
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (const char *) &yes, sizeof(int));
    server = gethostbyname(host);
    x.sin_port = htons(port);
    x.sin_family = rumble_config_int(m, "forceipv4") ? AF_INET : AF_UNSPEC;
    IP = inet_ntoa(*(struct in_addr *) *server->h_addr_list);
    x.sin_addr.s_addr = inet_addr(IP);
    if (server) {
        if (connect(sockfd, (struct sockaddr *) &x, sizeof x)) {
            return (0);
        }
    }

    return (sockfd);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_comm_printf(sessionHandle *session, const char *d, ...) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    va_list vl;
    char    *buffer;
    int     len;
#if (R_ARCH > 32) && !defined(_CRTIMP)
    char    moo[1024];
#endif
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!d) return (0);
    va_start(vl, d);
#ifdef _CRTIMP      /* Windows CRT library has a nifty function for this */
    len = _vscprintf(d, vl) + 10;
#else
#   if R_ARCH > 32  /* Some odd 64 bit libc bug? */
    len = vsnprintf(moo, 1023, d, vl) + 1;
#   else
    len = vsnprintf(NULL, 0, d, vl) + 1;
#   endif
#endif
    va_end(vl);
    buffer = (char *) calloc(1, len + 1);
    if (!buffer) merror();
    va_start(vl, d);
    vsprintf(buffer, d, vl);
    if (send(session->client->socket, "", 0, 0) == -1) return (-1); /* Check if we can send at all (avoid GnuTLS crash) */
    if (session->client->tls != NULL) len = (session->client->send) (session->client->tls, buffer, strlen(buffer), 0);
    else len = send(session->client->socket, buffer, (int) strlen(buffer), 0);
    session->client->bsent += strlen(buffer);
    free(buffer);
    va_end(vl);
    return (len);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void comm_accept(socketHandle sock, clientHandle *client) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    socklen_t   sin_size = sizeof client->client_info;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    while (1) {

        /* loop through accept() till we get something worth passing along. */
        client->socket = accept(sock, (struct sockaddr *) &(client->client_info), &sin_size);
        client->tls = 0;
        client->send = 0;
        client->recv = 0;
        client->brecv = 0;
        client->bsent = 0;
        client->rejected = 0;
        if (client->socket == SOCKET_ERROR) {
            perror("Error while attempting accept()");
            break;
        }

        FD_ZERO(&client->fd);
        FD_SET(client->socket, &client->fd);
#ifdef RUMBLE_WINSOCK
        strncpy(client->addr, inet_ntoa(((struct sockaddr_in *) &(client->client_info))->sin_addr), 46);
#else
        inet_ntop(client->client_info.ss_family, get_in_addr((struct sockaddr *) &client->client_info), client->addr, sizeof client->addr);
#endif
        break;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char *rumble_comm_read(sessionHandle *session) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char            b = 0;
    ssize_t         rc = 0;
    uint32_t        p;
    struct timeval  t;
    signed int      f;
    time_t          z;
    char            *ret = (char *) calloc(1, 1025);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!ret) {
        perror("Calloc failed!");
        exit(1);
    }

    t.tv_sec = (session->_tflags & RUMBLE_THREAD_IMAP) ? 30 : 10;
    t.tv_usec = 0;
    z = time(0);
    for (p = 0; p < 1024; p++) {
        f = select(session->client->socket + 1, &session->client->fd, NULL, NULL, &t);
        if (f > 0) {
            if (send(session->client->socket, "", 0, 0) == -1) return (NULL);
            if (session->client->recv) rc = (session->client->recv) (session->client->tls, &b, 1, 0);
            else rc = recv(session->client->socket, &b, 1, 0);
            if (rc <= 0) {
                free(ret);
                return (NULL);
            }

            /*
             * printf("%c\n", b);
             */
            ret[p] = b;
            if (b == '\n') break;
        } else {
            z = time(0) - z;
            free(ret);
#ifndef PRIdPTR
#   define PRIdPTR "ld"
#endif
            printf("timeout after %"PRIdPTR " secs! %d\r\n", z, f);
            return (NULL);
        }
    }

    if (session->_svc) ((rumbleService *) session->_svc)->traffic.received += strlen(ret);
    session->client->brecv += strlen(ret);
    return (ret);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char *rumble_comm_read_bytes(sessionHandle *session, int len) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    char            *buffer;
    ssize_t         rc = 0;
    struct timeval  t;
    signed int      f;
    /*~~~~~~~~~~~~~~~~~~~~*/

    t.tv_sec = (session->_tflags & RUMBLE_THREAD_IMAP) ? 1000 : 10;
    t.tv_usec = 0;
    buffer = (char *) calloc(1, len + 1);
    f = select(session->client->socket + 1, &session->client->fd, NULL, NULL, &t);
    if (f > 0) {
        if (session->client->recv) rc = (session->client->recv) (session->client->tls, buffer, len, 0);
        else rc = recv(session->client->socket, buffer, len, 0);
        if (rc <= 0) {
            free(buffer);
            return (NULL);
        }

        if (session->_svc) ((rumbleService *) session->_svc)->traffic.received += len;
        session->client->brecv += len;
        return (buffer);
    }

    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_comm_send(sessionHandle *session, const char *message) {
    if (session->_svc) ((rumbleService *) session->_svc)->traffic.sent += strlen(message);
    session->client->bsent += strlen(message);
    if (send(session->client->socket, "", 0, 0) == -1) return (-1); /* Check if we can send at all (avoid GnuTLS crash) */
    if (session->client->send) return ((session->client->send) (session->client->tls, message, strlen(message), 0));
    return (send(session->client->socket, message, (int) strlen(message), 0));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_comm_send_bytes(sessionHandle *session, const char *message, size_t len) {
    if (session->_svc) ((rumbleService *) session->_svc)->traffic.sent += len;
    session->client->bsent += len;
    if (session->client->send) return ((session->client->send) (session->client->tls, message, len, 0));
    return (send(session->client->socket, message, (int) len, 0));
}
