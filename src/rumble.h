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
#include "pthread.h"
#include "cvector.h"
#include "reply_codes.h"

#define RUMBLE_DEBUG_HOOKS      0x01000000
#define RUMBLE_DEBUG_THREADS    0x02000000
#define RUMBLE_DEBUG            RUMBLE_DEBUG_THREADS // debug output flags



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
#define RUMBLE_CUE_SMTP_RSET    0x00020000
    
#define RUMBLE_CUE_POP3_HELO    0x00001000
#define RUMBLE_CUE_POP3_QUIT    0x00002000
#define RUMBLE_CUE_POP3_TOP     0x00004000
#define RUMBLE_CUE_POP3_RETR    0x00008000
#define RUMBLE_CUE_POP3_LIST    0x00010000
#define RUMBLE_CUE_POP3_DELE    0x00020000

#define RUMBLE_CUE_MASK         0x000FF000

    
// SMTP session flags
#define RUMBLE_SMTP_BADRFC        0x00000100   // Client is known to break RFC and requires leniency.
#define RUMBLE_SMTP_WHITELIST     0x00000200   // Client has been whitelisted by a module.
#define RUMBLE_SMTP_AUTHED        0x00000400   // Client is authenticated and considered known.
#define RUMBLE_SMTP_FREEPASS      0x00000700   // Mask that covers all three exceptions.
    
#define RUMBLE_SMTP_HAS_HELO      0x00000001    // Has valid HELO/EHLO
#define RUMBLE_SMTP_HAS_MAIL      0x00000002    // Has valid MAIL FROM
#define RUMBLE_SMTP_HAS_RCPT      0x00000004    // Has valid RCPT
#define RUMBLE_SMTP_HAS_EHLO      0x00000009    // Has extended HELO
    
#define RUMBLE_ROAD_MASK          0x00FF0000    // Command sequence mask

    
// Thread flags
#define RUMBLE_THREAD_DIE         0x00001000    // Kill signal for threads
#define RUMBLE_THREAD_MISC        0x00010000    // Thread handles miscellaneous stuff
#define RUMBLE_THREAD_SMTP        0x00020000    // Thread handles SMTP
#define RUMBLE_THREAD_POP3        0x00040000    // Thread handles POP3
#define RUMBLE_THREAD_IMAP        0x00080000    // Thread handles IMAP
#define RUMBLE_THREAD_SVCMASK     0x000F0000
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
    cvector*        flags;
} address;

typedef struct {
    cvector*        recipients;
    address         sender;
    clientHandle*   client;
    uint32_t        flags;
    uint32_t        _tflags;
} sessionHandle;

typedef struct {
    const char*         title;
    const char*         description;
    
} rumble_module_info;

typedef struct {
    uint32_t                    flags;
    ssize_t                     (*func)(sessionHandle*);
    const char*                 module;
    rumble_module_info*         modinfo;
} hookHandle;

typedef struct {
        socketHandle    socket;
        cvector*        threads;
        cvector*        init_hooks;
        cvector*        cue_hooks;
        cvector*        exit_hooks;
        pthread_mutex_t mutex;
        cvector*        handles;
        void*           (*init)(void*);
    } rumbleService;

typedef struct {
    struct {
        cvector*                conf;
        cvector*                workers;
        const char*             currentSO;
        cvector*                modules;
    }               readOnly;
    rumbleService       smtp;
    rumbleService       pop3;
    rumbleService       imap;
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

void rumble_scan_flags(cvector* dict, const char* flags);
void rumble_flush_dictionary(cvector* dict);
const char* rumble_get_dictionary_value(cvector* dict, const char* flag);
void rumble_free_address(address* a);

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

