/* 
 * File:   rumble.h
 * Author: Humbedooh
 *
 * Created on January 3, 2011, 8:07 PM
 */

#ifndef RUMBLE_H
#define	RUMBLE_H
#include <sys/socket.h>
#include "database.h"
#include "servers.h"
#include "cvector.h"

#ifdef	__cplusplus
extern "C" {
#endif

// Cue definitions
#define RUMBLE_CUE_SMTP_HELO            1
#define RUMBLE_CUE_SMTP_RCPT            2
#define RUMBLE_CUE_SMTP_MAIL            3
#define RUMBLE_CUE_SMTP_DATA            4
#define RUMBLE_CUE_SMTP_QUIT            5
    
#define RUMBLE_CUE_POP3_HELO            1
#define RUMBLE_CUE_POP3_QUIT            2
#define RUMBLE_CUE_POP3_TOP             3
#define RUMBLE_CUE_POP3_RETR            4
#define RUMBLE_CUE_POP3_LIST            5
#define RUMBLE_CUE_POP3_DELE            6
    
// Flag definitions
#define RUMBLE_SMTP_BADRFC        0x01000000   // Client is known to break RFC and requires leniency.
#define RUMBLE_SMTP_WHITELIST     0x02000000   // Client has been whitelisted by a module.
#define RUMBLE_SMTP_AUTHED        0x04000000   // Client is authenticated and considered known.
#define RUMBLE_SMTP_FREEPASS      0x07000000   // Mask that covers all three exceptions.

// Structure definitions

#define socketHandle int
typedef struct {
    socketHandle               socket;
    struct sockaddr_storage    client_info;
    char                       addr[46]; // INET6_ADDRSTRLEN
} clientHandle;

typedef struct {
    char*           user;
    char*           domain;
    char*           raw;
} address;

typedef struct {
    address         recipient;
    address         sender;
    clientHandle*   client;
    uint32_t        flags;
} sessionHandle;

typedef struct {
    signed char     timing;
    unsigned char   cue;
    ssize_t (*func)(sessionHandle*);
} hookHandle;


typedef struct {
    struct {
        cvector*        conf;
        pthread_t       miscThreads[10];
    }               readOnly;
    struct {
        socketHandle    socket;
        cvector*        threads;
        cvector*        init_hooks;
        cvector*        cue_hooks;
    } smtp;
    struct {
        socketHandle    socket;
        cvector*        threads;
        cvector*        init_hooks;
        cvector*        cue_hooks;
    } pop3;
    struct {
        socketHandle    socket;
        cvector*        threads;
        cvector*        init_hooks;
        cvector*        cue_hooks;
    } imap;
} masterHandle;

typedef struct {
    const char*     key;
    const char*     value;
} configElement;

// Hooking commands
int rumble_module_on_server_init(masterHandle* master);


void rumble_hook_on_smtp_cmd(masterHandle* master, const char* cmd, ssize_t (*func)(sessionHandle*) );
void rumble_hook_on_pop3_cmd(masterHandle* master, const char* cmd, ssize_t (*func)(sessionHandle*) );
void rumble_hook_on_imap_cmd(masterHandle* master, const char* cmd, ssize_t (*func)(sessionHandle*) );

void rumble_hook_on_smtp_accept(masterHandle* master, ssize_t (*func)(sessionHandle*) );

// Public tool-set
char* rumble_sha160(const unsigned char* d);
char* rumble_sha256(const unsigned char* d);
void  rumble_strlower(char* d);
void  rumble_strupper(char* d);

ssize_t rumble_comm_send(sessionHandle* session, const char* message);
ssize_t rumble_comm_recv(sessionHandle* session, char* message_pointer);

const char* rumble_config_str(const char* key);
uint32_t rumble_config_int(const char* key);

#ifdef	__cplusplus
}
#endif

#endif	/* RUMBLE_H */

