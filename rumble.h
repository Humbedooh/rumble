/* 
 * File:   rumble.h
 * Author: Humbedooh
 *
 * Created on January 3, 2011, 8:07 PM
 */

#ifndef RUMBLE_H
#define	RUMBLE_H
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cvector.h"
#include "reply_codes.h"

#define RUMBLE_DEBUG // debug output

#ifdef	__cplusplus
extern "C" {
#endif

// Return codes for modules
#define RUMBLE_RETURN_OKAY      1       // Everything went fine, keep going.
#define RUMBLE_RETURN_FAILURE   2       // Something went really wrong, abort the connection!
#define RUMBLE_RETURN_IGNORE    3       // Module handled the return code, skip to next command.
    
// Hook flags
#define RUMBLE_HOOK_ACCEPT      0x00000001
#define RUMBLE_HOOK_COMMAND     0x00000002
#define RUMBLE_HOOK_EXIT        0x00000004
#define RUMBLE_HOOK_STATE_MASK  0x0000000F
    
#define RUMBLE_HOOK_SMTP        0x00000010
#define RUMBLE_HOOK_POP3        0x00000020
#define RUMBLE_HOOK_IMAP        0x00000040
#define RUMBLE_HOOK_SVC_MASK    0x000000F0
    
#define RUMBLE_HOOK_BEFORE      0x00000000
#define RUMBLE_HOOK_AFTER       0x00000100
#define RUMBLE_HOOK_TIMING_MASK 0x00000F00

// Cue definitions
#define RUMBLE_CUE_SMTP_HELO    0x00001000
#define RUMBLE_CUE_SMTP_RCPT    0x00002000
#define RUMBLE_CUE_SMTP_MAIL    0x00004000
#define RUMBLE_CUE_SMTP_DATA    0x00008000
#define RUMBLE_CUE_SMTP_QUIT    0x00010000
    
#define RUMBLE_CUE_POP3_HELO    0x00001000
#define RUMBLE_CUE_POP3_QUIT    0x00002000
#define RUMBLE_CUE_POP3_TOP     0x00004000
#define RUMBLE_CUE_POP3_RETR    0x00008000
#define RUMBLE_CUE_POP3_LIST    0x00010000
#define RUMBLE_CUE_POP3_DELE    0x00020000

#define RUMBLE_CUE_MASK         0x000FF000

    
// Session flags
#define RUMBLE_SMTP_BADRFC        0x01000000   // Client is known to break RFC and requires leniency.
#define RUMBLE_SMTP_WHITELIST     0x02000000   // Client has been whitelisted by a module.
#define RUMBLE_SMTP_AUTHED        0x04000000   // Client is authenticated and considered known.
#define RUMBLE_SMTP_FREEPASS      0x07000000   // Mask that covers all three exceptions.
    
#define RUMBLE_SMTP_HAS_HELO      0x00010000    // Has valid HELO/EHLO
#define RUMBLE_SMTP_HAS_MAIL      0x00020000    // Has valid MAIL FROM
#define RUMBLE_SMTP_HAS_RCPT      0x00040000    // Has valid RCPT
    
#define RUMBLE_ROAD_MASK          0x00FF0000    // Command sequence mask

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
    cvector*        recipients;
    address         sender;
    clientHandle*   client;
    uint32_t        flags;
} sessionHandle;

typedef struct {
    unsigned short flags;
    ssize_t (*func)(sessionHandle*);
    const char*    module;
} hookHandle;


typedef struct {
    struct {
        cvector*        conf;
        cvector*        workers;
        const char*     currentSO;
    }               readOnly;
    struct {
        socketHandle    socket;
        cvector*        threads;
        cvector*        init_hooks;
        cvector*        cue_hooks;
        cvector*        exit_hooks;
    } smtp;
    struct {
        socketHandle    socket;
        cvector*        threads;
        cvector*        init_hooks;
        cvector*        cue_hooks;
        cvector*        exit_hooks;
    } pop3;
    struct {
        socketHandle    socket;
        cvector*        threads;
        cvector*        init_hooks;
        cvector*        cue_hooks;
        cvector*        exit_hooks;
    } imap;
} masterHandle;

typedef struct {
    const char*     key;
    const char*     value;
} configElement;

// Hooking commands
void rumble_hook_function(void* handle, uint32_t flags, ssize_t (*func)(sessionHandle*) );

// Public tool-set
char* rumble_sha160(const unsigned char* d); //SHA1 digest (40 byte hex string)
char* rumble_sha256(const unsigned char* d); //SHA-256 digest (64 byte hex string)

void  rumble_string_lower(char* d); // Converts <d> into lowercase.
void  rumble_string_upper(char* d); // Converts <d> into uppercase.

const char* rumble_smtp_reply_code(unsigned int code);
//const char* rumble_pop3_reply_code(unsigned int code);

ssize_t rumble_comm_send(sessionHandle* session, const char* message);
char* rumble_comm_read(sessionHandle* session);

const char* rumble_config_str(const char* key);
uint32_t rumble_config_int(const char* key);

#ifdef	__cplusplus
}
#endif

#endif	/* RUMBLE_H */

