/* 
 * File:   rumble.h
 * Author: Humbedooh
 *
 * Created on January 3, 2011, 8:07 PM
 */
#ifndef RUMBLE_H
#define	RUMBLE_H
//#define FORCE_WIN

#ifdef	__cplusplus
extern "C" {
#endif
#define rumblemodule int
    
#if (((defined(_WIN32) && !defined(__CYGWIN__)) || defined(__MINGW32__)) && !defined(RUMBLE_IGNORE_WIN)) || defined(FORCE_WIN)
        #undef rumblemodule
        #define rumblemodule int __declspec(dllexport)
	#define RUMBLE_WINSOCK
        #define HAVE_STRUCT_TIMESPEC
	#include <Windows.h>
	#include <winsock.h>
	#include <windns.h> // for DnsQuery_A instead of res_query
	//#include <WinCrypt.h>
	//#pragma comment (lib, "Crypt32");
	//#include <sys/types.h>
        #if !defined(__CYGWIN__)
                #include "pthreads-win32/include/pthread.h"
        #else
                #include "pthread.h"
        #endif
	
	#ifndef uint32_t
		typedef unsigned char uint8_t;
		typedef unsigned short uint16_t;
		typedef unsigned int uint32_t;
		typedef unsigned long long uint64_t;
		typedef signed int ssize_t;

	#endif
	#define sleep(a) Sleep(a*1000)
	struct in6_addr
	{
	  union
		{
		  uint8_t 	  __s6_addr[16];
		  uint16_t 	  __s6_addr16[8];
		  uint32_t 	  __s6_addr32[4];
		} __u6;
	#define s6_addr		__u6.__s6_addr
	#define s6_addr16	__u6.__s6_addr16
	#define s6_addr32	__u6.__s6_addr32

	};
	#define close(a) closesocket(a);
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
	#ifndef AI_PASSIVE 
		#define AI_PASSIVE 1
	#endif
#else
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <resolv.h>
	#include <inttypes.h>
	#include "pthread.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cvector.h"
#include "reply_codes.h"

#define RUMBLE_DEBUG_HOOKS              0x00100000
#define RUMBLE_DEBUG_THREADS            0x02000000
#define RUMBLE_DEBUG_STORAGE            0x04000000
#define RUMBLE_DEBUG_COMM               0x00010000
#define RUMBLE_DEBUG_MEMORY				0x00001000 //reroutes malloc and calloc for debugging
#define RUMBLE_DEBUG                    (RUMBLE_DEBUG_STORAGE | RUMBLE_DEBUG_COMM) // debug output flags
#define RUMBLE_VERSION                  0x00020500 // Internal version for module checks


// Return codes for modules
#define RUMBLE_RETURN_OKAY              1       // Everything went fine, keep going.
#define RUMBLE_RETURN_FAILURE           2       // Something went really wrong, abort the connection!
#define RUMBLE_RETURN_IGNORE            3       // Module handled the return code, skip to next command.

    
// Hook flags
#define RUMBLE_HOOK_ACCEPT              0x00000001
#define RUMBLE_HOOK_COMMAND             0x00000002
#define RUMBLE_HOOK_EXIT                0x00000004
#define RUMBLE_HOOK_FEED                0x00000008
#define RUMBLE_HOOK_PARSER              0x00000010
#define RUMBLE_HOOK_STATE_MASK          0x000000FF
    
#define RUMBLE_HOOK_SMTP                0x00000100
#define RUMBLE_HOOK_POP3                0x00000200
#define RUMBLE_HOOK_IMAP                0x00000400
#define RUMBLE_HOOK_SVC_MASK            0x00000F00
    
#define RUMBLE_HOOK_BEFORE              0x00000000
#define RUMBLE_HOOK_AFTER               0x00001000
#define RUMBLE_HOOK_TIMING_MASK         0x0000F000

// Cue definitions
#define RUMBLE_CUE_SMTP_HELO            0x00010000
#define RUMBLE_CUE_SMTP_RCPT            0x00020000
#define RUMBLE_CUE_SMTP_MAIL            0x00040000
#define RUMBLE_CUE_SMTP_DATA            0x00080000
#define RUMBLE_CUE_SMTP_QUIT            0x00100000
#define RUMBLE_CUE_SMTP_RSET            0x00200000
#define RUMBLE_CUE_SMTP_NOOP            0x00400000
#define RUMBLE_CUE_SMTP_VRFY            0x00800000
#define RUMBLE_CUE_SMTP_AUTH            0x01000000
    
#define RUMBLE_CUE_POP3_HELO            0x00010000
#define RUMBLE_CUE_POP3_QUIT            0x00020000
#define RUMBLE_CUE_POP3_TOP             0x00040000
#define RUMBLE_CUE_POP3_RETR            0x00080000
#define RUMBLE_CUE_POP3_LIST            0x00100000
#define RUMBLE_CUE_POP3_DELE            0x00200000

#define RUMBLE_CUE_MASK                 0x0FFF0000

    
// SMTP session flags
#define RUMBLE_SMTP_BADRFC              0x00000100   // Client is known to break RFC and requires leniency.
#define RUMBLE_SMTP_WHITELIST           0x00000200   // Client has been whitelisted by a module.
#define RUMBLE_SMTP_AUTHED              0x00000400   // Client is authenticated and considered known.
#define RUMBLE_SMTP_CAN_RELAY           0x00000600   // Client is allowed to relay emails to other servers.
#define RUMBLE_SMTP_FREEPASS            0x00000700   // Mask that covers all three exceptions.
    
#define RUMBLE_SMTP_HAS_HELO            0x00000001   // Has valid HELO/EHLO
#define RUMBLE_SMTP_HAS_MAIL            0x00000002   // Has valid MAIL FROM
#define RUMBLE_SMTP_HAS_RCPT            0x00000004   // Has valid RCPT
#define RUMBLE_SMTP_HAS_EHLO            0x00000009   // Has extended HELO
#define RUMBLE_SMTP_HAS_BATV			0x00000010	 // Has valid BATV signature
    
#define RUMBLE_ROAD_MASK                0x00FF0000   // Command sequence mask

    
// Thread flags
#define RUMBLE_THREAD_DIE               0x00001000   // Kill signal for threads
#define RUMBLE_THREAD_MISC              0x00010000   // Thread handles miscellaneous stuff
#define RUMBLE_THREAD_SMTP              0x00020000   // Thread handles SMTP
#define RUMBLE_THREAD_POP3              0x00040000   // Thread handles POP3
#define RUMBLE_THREAD_IMAP              0x00080000   // Thread handles IMAP
#define RUMBLE_THREAD_SVCMASK           0x000F0000

    
#define RUMBLE_MTYPE_MBOX               0x00000001   // Regular mailbox
#define RUMBLE_MTYPE_ALIAS              0x00000002   // Alias to somewhere else
#define RUMBLE_MTYPE_MOD                0x00000004   // Mail goes into a module
#define RUMBLE_MTYPE_FEED               0x00000008   // Mail is fed to an external program or URL
    
    
// Structure definitions

#define socketHandle int
#ifndef _SS_PAD2SIZE
	#define _SS_MAXSIZE 128			/* Maximum size. */
	#define _SS_ALIGNSIZE (sizeof (uint64_t))/* Desired alignment. */
	#define _SS_PAD1SIZE (_SS_ALIGNSIZE - sizeof (sa_family_t))
	#define _SS_PAD2SIZE (_SS_MAXSIZE - (sizeof (sa_family_t) \
				  + _SS_PAD1SIZE + _SS_ALIGNSIZE))
	struct sockaddr_storage {
	  unsigned short		ss_family;
	  char			_ss_pad1[_SS_PAD1SIZE];
	  uint64_t		__ss_align;
	  char			_ss_pad2[_SS_PAD2SIZE];
	};
#endif
typedef struct {
    socketHandle				socket;
    struct sockaddr_storage		client_info;
    char						addr[46]; // INET6_ADDRSTRLEN
	fd_set						fd; // for select()
} clientHandle;

typedef struct {
    char*           user;
    char*           domain;
    char*           raw;
    cvector*        flags;
    char*           _flags;
	char*			tag;  // VERP or BATV tags
} address;

typedef struct {
    cvector*        recipients;
	cvector*		dict;
    address*        sender;
    clientHandle*   client;
    uint32_t        flags;
    uint32_t        _tflags;
    void*           _master;
} sessionHandle;

typedef struct {
    const char*         title;
    const char*         description;
    const char*         author;
    const char*         file;
    
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
    struct __core {
        cvector*                conf;
        cvector*                workers;
        pthread_cond_t          workcond;
        pthread_mutex_t         workmutex;
        const char*             currentSO;
        cvector*                modules;
        cvector*                parser_hooks;
        cvector*                feed_hooks;
        void*                   db;
        void*                   mail;
		cvector*				batv; // BATV handles for bounce control
    }               _core;
    rumbleService       smtp;
    rumbleService       pop3;
    rumbleService       imap;
    const char*         cfgdir;
} masterHandle;

typedef struct {
    const char*     key;
    const char*     value;
} rumbleKeyValuePair;

typedef struct {
    const char*     host;
    unsigned int    preference;
} mxRecord;

typedef struct {
    uint32_t            uid;
    char*               user;
    char*               domain;
    uint32_t            type;
    char*               arg;
} userAccount;

typedef struct {
    address*        sender;
    address*        recipient;
    const char*     fid;
    const char*     flags;
    uint32_t        date;
    userAccount*    account;
	uint32_t		loops;
	char			mType; // 0 = regular mail, 1 = bounce
} mqueue;

typedef struct {
	uint32_t		replyCode;
	char*			replyMessage;
	char*			replyServer;
	cvector*		flags;
} rumble_sendmail_response;


// Hooking commands
void rumble_hook_function(void* handle, uint32_t flags, ssize_t (*func)(sessionHandle*) );
rumblemodule rumble_module_check();

// Public tool-set
char* rumble_sha160(const unsigned char* d); //SHA1 digest (40 byte hex string)
char* rumble_sha256(const unsigned char* d); //SHA-256 digest (64 byte hex string)

void  rumble_string_lower(char* d); // Converts <d> into lowercase.
void  rumble_string_upper(char* d); // Converts <d> into uppercase.
char* rumble_mtime(); // mail time

void rumble_scan_words(cvector* dict, const char* wordlist);
void rumble_scan_flags(cvector* dict, const char* flags);
void rumble_flush_dictionary(cvector* dict);
const char* rumble_get_dictionary_value(cvector* dict, const char* flag);
void rumble_add_dictionary_value(cvector* dict, const char* key, const char* value);
uint32_t rumble_has_dictionary_value(cvector* dict, const char* flag);
void rumble_free_address(address* a);
void rumble_free_account(userAccount* user);

const char* rumble_smtp_reply_code(unsigned int code);
//const char* rumble_pop3_reply_code(unsigned int code);

ssize_t rumble_comm_send(sessionHandle* session, const char* message);
ssize_t rumble_comm_printf(sessionHandle* session, const char* d, ...);
char* rumble_comm_read(sessionHandle* session);

const char* rumble_config_str(masterHandle* master, const char* key);
uint32_t rumble_config_int(masterHandle* master, const char* key);

uint32_t rumble_domain_exists(sessionHandle* session, const char* domain);
uint32_t rumble_account_exists(sessionHandle* session, const char* user, const char* domain);
userAccount* rumble_get_account(masterHandle* master, const char* user, const char* domain);
address* rumble_parse_mail_address(const char* addr);
rumble_sendmail_response* rumble_send_email(masterHandle* master, const char* mailserver, const char* filename, address* sender, address* recipient);


// Shortcuts to common functions
#define rrdict   rumble_get_dictionary_value // read dict
#define rsdict   rumble_add_dictionary_value // set dict
#define rfdict   rumble_flush_dictionary     // flush dict
#define rhdict   rumble_has_dictionary_value // returns 1 if value exists, 0 otherwise

#define rcsend   rumble_comm_send
#define rcprintf rumble_comm_printf
#define rcread	 rumble_comm_read
#define merror() {printf("Memory allocation failed, this is bad!\n");exit(1);}

#if (RUMBLE_DEBUG & RUMBLE_DEBUG_MEMORY)
	void* xalloc(size_t m);
	void* yalloc(size_t n, size_t m);
	#define malloc xalloc
	#define calloc yalloc
#endif
#ifdef	__cplusplus
}
#endif

#endif	/* RUMBLE_H */


