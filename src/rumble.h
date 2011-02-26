/*$0
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    This file is part of the Rumble Mail Server package.
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#ifndef RUMBLE_H
#   define RUMBLE_H

/* C<99 compatibility defs */
#   if (__STDC_VERSION__ < 199901L)
#      define inline  static

/* pragma message("Non-C99 compliant compiler used, boooooo!") */
#   endif

/* Checks for Microsoft compiler */
#   if ((defined(_WIN32) && !defined(__CYGWIN__)) || defined(__MINGW32__))
#      define RUMBLE_MSC
#   endif
#   ifndef RUMBLE_MSC
#      ifndef __stdcall
#         define __cdecl
#         define __stdcall
#         define __fastcall
#      endif
#   endif

/*$5
 #######################################################################################################################
    INCLUDES
 #######################################################################################################################
 */

#   include <stdio.h>
#   include <stdlib.h>
#   include <string.h>
#   include <time.h>
#   include "cvector.h"
#   include "rumble_version.h"

/*$3
 =======================================================================================================================
    Microsoft specific headers
 =======================================================================================================================
 */

#   define FORCE_OLD_PTHREAD    /* comment out this field to use native windows threading (vista or above) */
#   ifdef RUMBLE_MSC
#      define RUMBLE_WINSOCK
#      define HAVE_STRUCT_TIMESPEC
#      pragma warning(disable : 5)
#      include <Ws2tcpip.h>
#      include <WinSock2.h>
#      include <windns.h>
#      if (NTDDI_VERSION < NTDDI_WIN6 || defined(FORCE_OLD_PTHREAD))
#         include <pthread.h>
#      else
#         include "winpthreads.h"
#      endif
#   else

/*$3
 =======================================================================================================================
    POSIX headers
 =======================================================================================================================
 */

#      include <unistd.h>
#      include <sys/types.h>
#      include <sys/socket.h>
#      include <netinet/in.h>
#      include <netdb.h>
#      include <arpa/inet.h>
#      include <resolv.h>
#      include <inttypes.h>
#      include "pthread.h"
#   endif

/*$3
 =======================================================================================================================
    Optional Lua support
 =======================================================================================================================
 */

#   define RUMBLE_LUA
#   ifdef RUMBLE_LUA
#      include <lua.h>
#      include <lualib.h>
#      include <lauxlib.h>
#      include "rumble_lua.h"
#   endif

/*$5
 #######################################################################################################################
    FLAG DEFINITIONS
 #######################################################################################################################
 */

/*$3
 =======================================================================================================================
    Debug and version flags
 =======================================================================================================================
 */

#   define RUMBLE_DEBUG_HOOKS      0x00100000
#   define RUMBLE_DEBUG_THREADS    0x02000000
#   define RUMBLE_DEBUG_STORAGE    0x04000000
#   define RUMBLE_DEBUG_COMM       0x00010000
#   define RUMBLE_DEBUG_MEMORY     0x00001000   /* reroutes malloc and calloc for debugging */
#   define RUMBLE_DEBUG            (RUMBLE_DEBUG_STORAGE | RUMBLE_DEBUG_COMM)   /* debug output flags */

/*$3
 =======================================================================================================================
    Module and function return codes
 =======================================================================================================================
 */

#   define RUMBLE_RETURN_OKAY      1    /* Everything went fine, keep going. */
#   define RUMBLE_RETURN_FAILURE   2    /* Something went really wrong, abort the connection! */
#   define RUMBLE_RETURN_IGNORE    3    /* Module handled the return code, skip to next command. */

/*$3
 =======================================================================================================================
    Flags for hooking modules to areas of rumble
 =======================================================================================================================
 */

#   define RUMBLE_HOOK_ACCEPT      0x00000001
#   define RUMBLE_HOOK_COMMAND     0x00000002
#   define RUMBLE_HOOK_EXIT        0x00000004
#   define RUMBLE_HOOK_FEED        0x00000008
#   define RUMBLE_HOOK_PARSER      0x00000010
#   define RUMBLE_HOOK_CLOSE       0x00000020
#   define RUMBLE_HOOK_STATE_MASK  0x000000FF
#   define RUMBLE_HOOK_SMTP        0x00000100
#   define RUMBLE_HOOK_POP3        0x00000200
#   define RUMBLE_HOOK_IMAP        0x00000400
#   define RUMBLE_HOOK_SVC_MASK    0x00000F00
#   define RUMBLE_HOOK_BEFORE      0x00000000
#   define RUMBLE_HOOK_AFTER       0x00001000
#   define RUMBLE_HOOK_TIMING_MASK 0x0000F000

/*$3
 =======================================================================================================================
    Flags for hooking modules to specific cues
 =======================================================================================================================
 */

#   define RUMBLE_CUE_SMTP_HELO    0x00010000
#   define RUMBLE_CUE_SMTP_RCPT    0x00020000
#   define RUMBLE_CUE_SMTP_MAIL    0x00040000
#   define RUMBLE_CUE_SMTP_DATA    0x00080000
#   define RUMBLE_CUE_SMTP_QUIT    0x00100000
#   define RUMBLE_CUE_SMTP_RSET    0x00200000
#   define RUMBLE_CUE_SMTP_NOOP    0x00400000
#   define RUMBLE_CUE_SMTP_VRFY    0x00800000
#   define RUMBLE_CUE_SMTP_AUTH    0x01000000
#   define RUMBLE_CUE_POP3_HELO    0x00010000
#   define RUMBLE_CUE_POP3_QUIT    0x00020000
#   define RUMBLE_CUE_POP3_TOP     0x00040000
#   define RUMBLE_CUE_POP3_RETR    0x00080000
#   define RUMBLE_CUE_POP3_LIST    0x00100000
#   define RUMBLE_CUE_POP3_DELE    0x00200000
#   define RUMBLE_CUE_MASK         0x0FFF0000

/*$3
 =======================================================================================================================
    Flags pertaining to SMTP sessions
 =======================================================================================================================
 */

#   define RUMBLE_SMTP_BADRFC      0x00000100   /* Client is known to break RFC and requires leniency. */
#   define RUMBLE_SMTP_WHITELIST   0x00000200   /* Client has been whitelisted by a module. */
#   define RUMBLE_SMTP_AUTHED      0x00000400   /* Client is authenticated and considered known. */
#   define RUMBLE_SMTP_CAN_RELAY   0x00000600   /* Client is allowed to relay emails to other servers. */
#   define RUMBLE_SMTP_FREEPASS    0x00000700   /* Mask that covers all three exceptions. */
#   define RUMBLE_SMTP_HAS_HELO    0x00000001   /* Has valid HELO/EHLO */
#   define RUMBLE_SMTP_HAS_MAIL    0x00000002   /* Has valid MAIL FROM */
#   define RUMBLE_SMTP_HAS_RCPT    0x00000004   /* Has valid RCPT */
#   define RUMBLE_SMTP_HAS_EHLO    0x00000009   /* Has extended HELO */
#   define RUMBLE_SMTP_HAS_BATV    0x00000010   /* Has valid BATV signature */

/*$3
 =======================================================================================================================
    Flags for POP3 sessions
 =======================================================================================================================
 */

#   define RUMBLE_POP3_HAS_USER    0x00000001   /* Has provided a username (but no password) */
#   define RUMBLE_POP3_HAS_AUTH    0x00000002   /* Has provided both username and password */

/*$3
 =======================================================================================================================
    Flags for IMAP4 sessions
 =======================================================================================================================
 */

#   define rumble_mailman_HAS_SELECT       0x00000001   /* Has selected a mailbox */
#   define rumble_mailman_HAS_TLS          0x00000002   /* Has established TLS or SSL */
#   define rumble_mailman_HAS_READWRITE    0x00000010   /* Read/Write session (SELECT) */
#   define rumble_mailman_HAS_READONLY     0x00000020   /* Read-only session (EXAMINE) */
#   define rumble_mailman_HAS_UID          0x00000100   /* UID-type request. */
#   define RUMBLE_ROAD_MASK                0x000000FF   /* Command sequence mask */

/*$3
 =======================================================================================================================
    Thread flags
 =======================================================================================================================
 */

#   define RUMBLE_THREAD_DIE       0x00001000   /* Kill signal for threads */
#   define RUMBLE_THREAD_MISC      0x00010000   /* Thread handles miscellaneous stuff */
#   define RUMBLE_THREAD_SMTP      0x00020000   /* Thread handles SMTP */
#   define RUMBLE_THREAD_POP3      0x00040000   /* Thread handles POP3 */
#   define RUMBLE_THREAD_IMAP      0x00080000   /* Thread handles IMAP */
#   define RUMBLE_THREAD_SVCMASK   0x000F0000

/*$3
 =======================================================================================================================
    Mailbox type flags
 =======================================================================================================================
 */

#   define RUMBLE_MTYPE_MBOX   0x00000001   /* Regular mailbox */
#   define RUMBLE_MTYPE_ALIAS  0x00000002   /* Alias to somewhere else */
#   define RUMBLE_MTYPE_MOD    0x00000004   /* Mail goes into a module */
#   define RUMBLE_MTYPE_FEED   0x00000008   /* Mail is fed to an external program or URL */
#   define RUMBLE_MTYPE_RELAY  0x00000010   /* Mail is being relayed to another server */

/* Letter flags (for POP3/IMAP4) */
#   define RUMBLE_LETTER_RECENT    0x00000000
#   define RUMBLE_LETTER_UNREAD    0x00000001
#   define RUMBLE_LETTER_READ      0x00000002
#   define RUMBLE_LETTER_DELETED   0x00000010
#   define RUMBLE_LETTER_EXPUNGE   0x00000030
#   define RUMBLE_LETTER_ANSWERED  0x00000100
#   define RUMBLE_LETTER_FLAGGED   0x00001000
#   define RUMBLE_LETTER_DRAFT     0x00010000
#   define RUMBLE_LETTER_UPDATED   0x00100000

/*$5
 #######################################################################################################################
    TYPE DEFINITIONS
 #######################################################################################################################
 */

#   ifdef RUMBLE_MSC
#      define rumblemodule    int __declspec(dllexport)
#      ifndef uint32_t
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;
typedef signed int          ssize_t;
typedef long long           int64_t;
#      endif
#      ifndef PRIu64
#         define PRIu64  "llu"
#      endif
#      define sleep(a)    Sleep(a * 1000)
#      ifndef AI_PASSIVE
#         define AI_PASSIVE  1
#      endif
#   else
#      define rumblemodule    int
#   endif

/*$3
 =======================================================================================================================
    Dummy socket operation pointer to allow for GNUTLS operations in modules without having to include it as a library
    when compiling
 =======================================================================================================================
 */

typedef ssize_t (*dummySocketOp) (void *a, const void *b, int c, int d);
typedef int socketHandle;

/*$5
 #######################################################################################################################
    STRUCTURE DEFINITIONS
 #######################################################################################################################
 */

#   ifdef RUMBLE_MSC
#      define close(a)    closesocket(a)
typedef int         socklen_t;
typedef uint16_t    sa_family_t;
typedef uint16_t    in_port_t;
typedef uint32_t    in_addr_t;
#   endif

/*
 -----------------------------------------------------------------------------------------------------------------------
    clientHandle: The basic struct of any TCP connection
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
    socketHandle            socket;
    struct sockaddr_storage client_info;
    char                    addr[46];
    fd_set                  fd;
    void                    *tls;
    dummySocketOp           recv;   /* Dummy operator for GNUTLS */
    dummySocketOp           send;   /* Dummy operator for GNUTLS */
} clientHandle;
typedef struct
{
    uint32_t        readers;
    uint32_t        writers;
    pthread_cond_t  reading;
    pthread_cond_t  writing;
    pthread_mutex_t mutex;
} rumble_readerwriter;

/*
 -----------------------------------------------------------------------------------------------------------------------
    INTERNAL DOMAIN AND USER ACCOUNT STRUCTS
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
    char        *name;          /* Name (or glob) of domain */
    char        *path;          /* Optional storage path for letters */
    uint32_t    id;             /* Domain ID */
} rumble_domain;
typedef struct
{
    uint32_t        uid;
    char            *user;      /* mailbox name */
    rumble_domain   *domain;    /* Pointer to domain struct */
    uint32_t        type;       /* type of mbox (mbox, alias, feed, mod) */
    char            *arg;       /* If it's of type alias, feed or mod, arg gives the args */
    char            *hash;      /* password hash */
} rumble_mailbox;
typedef struct
{
    char    *user;
    char    *domain;
    char    *raw;
    dvector *flags;
    char    *_flags;
    char    *tag;   /* VERP or BATV tags */
} address;
typedef struct
{
    dvector         *recipients;
    dvector         *dict;
    address         *sender;
    clientHandle    *client;
    uint32_t        flags;
    uint32_t        _tflags;
    void            *_master;
    void            *_svcHandle;
} sessionHandle;
typedef struct
{
    const char  *title;
    const char  *description;
    const char  *author;
    const char  *file;
} rumble_module_info;
typedef struct
{
    uint32_t    flags;
    ssize_t (*func) (sessionHandle *);
    const char          *module;
    rumble_module_info  *modinfo;
    int                 lua_callback;
} hookHandle;
typedef struct
{
    socketHandle    socket;
    dvector         *threads;
    cvector         *init_hooks;
    cvector         *cue_hooks;
    cvector         *exit_hooks;
    cvector         *commands;
    cvector         *capabilities;
    pthread_mutex_t mutex;
    dvector         *handles;
    void * (*init) (void *);
} rumbleService;
typedef struct
{
    struct __core
    {
        dvector         *conf;
        dvector         *workers;
        pthread_cond_t  workcond;
        pthread_mutex_t workmutex;
        const char      *currentSO;
        dvector         *modules;
        cvector         *parser_hooks;
        cvector         *feed_hooks;
        void            *db;
        void            *mail;
        dvector         *batv;  /* BATV handles for bounce control */
        void            *lua;
        void            *tls_credentials;
    } _core;
    rumbleService   smtp;
    rumbleService   pop3;
    rumbleService   imap;
    struct
    {
        rumble_readerwriter *rrw;
        dvector             *list;
    } domains;
    struct
    {
        rumble_readerwriter *rrw;
        dvector             *list;
    } mailboxes;
    const char  *cfgdir;
} masterHandle;
typedef struct
{
    const char  *key;
    const char  *value;
} rumbleKeyValuePair;
typedef struct
{
    uint32_t    key;
    char        *value;
} rumbleIntValuePair;
typedef struct
{
    const char      *host;
    unsigned int    preference;
} mxRecord;
typedef struct
{
    address         *sender;
    address         *recipient;
    const char      *fid;
    const char      *flags;
    uint32_t        date;
    rumble_mailbox  *account;
    uint32_t        loops;
    char            mType;      /* 0 = regular mail, 1 = bounce */
} mqueue;
typedef struct
{
    uint32_t    replyCode;
    char        *replyMessage;
    char        *replyServer;
    dvector     *flags;
} rumble_sendmail_response;
typedef struct
{
    uint64_t    id;         /* Letter ID */
    uint32_t    uid;        /* User ID */
    char        *fid;       /* File ID */
    uint32_t    size;       /* Size of letter */
    uint32_t    delivered;  /* Time of delivery */
    int64_t     folder;     /* Folder name (for IMAP4) */
    uint32_t    flags;      /* Various flags */
    uint32_t    _flags;     /* Original copy of flags (for update checks) */
} rumble_letter;
typedef struct
{
    rumble_mailbox  *account;   /* Pointer to account */
    dvector         *contents;  /* dvector with letters */
    rumble_letter   **letters;  /* post-defined array of letters for fast access */
    uint32_t        size;       /* Number of letters */
} rumble_mailbag;
typedef struct
{
    uint32_t    id;
    char        *name;
    int         subscribed;
} rumble_folder;
typedef struct
{
    dvector             *folders;
    rumble_readerwriter *rrw;
    uint32_t            sessions;
    uint32_t            uid;
} rumble_mailman_shared_bag;
typedef struct
{
    int64_t                     id;
    time_t                      updated;
    uint64_t                    lastMessage;
    char                        *name;
    int                         subscribed;
    dvector                     *letters;
    rumble_mailman_shared_bag   *bag;
} rumble_mailman_shared_folder;
typedef struct
{
    char        **argv;
    uint32_t    argc;
} rumble_args;
typedef struct
{
    rumble_mailbox              *account;
    rumble_mailman_shared_bag   *bag;
    int64_t                     folder;
} accountSession;
typedef ssize_t (*svcCommand) (masterHandle *, sessionHandle *, const char *, const char *);
typedef struct
{
    const char  *cmd;
    svcCommand  func;
} svcCommandHook;
#   ifdef __cplusplus
extern "C"
{
#   endif

/*$5
 #######################################################################################################################
    FUNCTION PROTOTYPES
 #######################################################################################################################
 */

/*$3
 =======================================================================================================================
    Functions for hooking modules into rumble
 =======================================================================================================================
 */

void            rumble_hook_function(void *handle, uint32_t flags, ssize_t (*func) (sessionHandle *));
rumblemodule    rumble_module_check(void);
void            rumble_service_add_command(rumbleService *svc, const char *command, svcCommand func);
void            rumble_service_add_capability(rumbleService *svc, const char *command);

/*$3
 =======================================================================================================================
    Public tool-set
 =======================================================================================================================
 */

void                        rumble_test(void);
char                        *rumble_sha160(const unsigned char *d); /* SHA1 digest (40 byte hex string) */
char                        *rumble_sha256(const unsigned char *d); /* SHA-256 digest (64 byte hex string) */
char                        *rumble_decode_base64(const char *src);
void                        rumble_string_lower(char *d);           /* Converts <d> into lowercase. */
void                        rumble_string_upper(char *d);           /* Converts <d> into uppercase. */
rumble_args                 *rumble_read_words(const char *d);
void                        rumble_args_free(rumble_args *d);
char                        *rumble_mtime(void);            /* mail time */
char                        *rumble_create_filename(void);  /* Generates random 16-letter filenames */
void                        rumble_scan_words(dvector *dict, const char *wordlist);
void                        rumble_scan_flags(dvector *dict, const char *flags);
void                        rumble_flush_dictionary(dvector *dict);
const char                  *rumble_get_dictionary_value(dvector *dict, const char *flag);
void                        rumble_add_dictionary_value(dvector *dict, const char *key, const char *value);
uint32_t                    rumble_has_dictionary_value(dvector *dict, const char *flag);
void                        rumble_free_address(address *a);
void                        rumble_free_account(rumble_mailbox *user);
const char                  *rumble_smtp_reply_code(unsigned int code);
ssize_t                     rumble_comm_send(sessionHandle *session, const char *message);
ssize_t                     rumble_comm_printf(sessionHandle *session, const char *d, ...);
char                        *rumble_comm_read(sessionHandle *session);
const char                  *rumble_config_str(masterHandle *master, const char *key);
uint32_t                    rumble_config_int(masterHandle *master, const char *key);
void                        rumble_crypt_init(masterHandle *master);
address                     *rumble_parse_mail_address(const char *addr);
rumble_sendmail_response    *rumble_send_email
                            (
                                masterHandle    *master,
                                const char      *mailserver,
                                const char      *filename,
                                address         *sender,
                                address         *recipient
                            );

/*$3
 =======================================================================================================================
    Account and domain handling
 =======================================================================================================================
 */

uint32_t        rumble_domain_exists(const char *domain);
uint32_t rumble_account_exists_raw(const char *user, const char *domain);
rumble_domain   *rumble_domain_copy(const char *domain);
cvector         *rumble_domains_list(void);
uint32_t        rumble_account_exists(sessionHandle *session, const char *user, const char *domain);
rumble_mailbox  *rumble_account_data(sessionHandle *session, const char *user, const char *domain);
rumble_mailbox  *rumble_account_data_auth(sessionHandle *session, const char *user, const char *domain, const char *pass);
cvector         *rumble_database_accounts_list(const char *domain);
void            rumble_database_accounts_free(cvector *accounts);   /* cleanup func for the function above. */

/*$3
 =======================================================================================================================
    Mailbox handling
 =======================================================================================================================
 */

FILE                            *rumble_letters_open(rumble_mailbox *mbox, rumble_letter *letter);
rumble_mailman_shared_bag       *rumble_mailman_open_bag(uint32_t uid);
void                            rumble_mailman_close_bag(rumble_mailman_shared_bag *bag);
rumble_mailman_shared_folder    *rumble_mailman_current_folder(accountSession *sess);
rumble_mailman_shared_bag       *rumble_letters_retrieve_shared(uint32_t uid);
void                            rumble_mailman_update_folders(rumble_mailman_shared_bag *bag);
uint32_t                        rumble_mailman_commit(accountSession *imap, rumble_mailman_shared_folder *folder);
void                            rumble_mailman_free(rumble_mailman_shared_bag *bag);
uint32_t                        rumble_mailman_scan_incoming(rumble_mailman_shared_folder *folder);
uint32_t                        rumble_mailman_copy_letter
                                (
                                    rumble_mailbox                  *account,
                                    rumble_letter                   *letter,
                                    rumble_mailman_shared_folder    *folder
                                );

/*$5
 #######################################################################################################################
    SHORTCUTS
 #######################################################################################################################
 */

#   define rrdict      rumble_get_dictionary_value  /* read dict */
#   define rsdict      rumble_add_dictionary_value  /* set dict */
#   define rfdict      rumble_flush_dictionary      /* flush dict */
#   define rhdict      rumble_has_dictionary_value  /* returns 1 if value exists, 0 otherwise */
#   define rcsend      rumble_comm_send
#   define rcprintf    rumble_comm_printf
#   define rcread      rumble_comm_read
#   define merror()    { fprintf(stderr, "Memory allocation failed, this is bad!\n"); exit(1); }
#   define and         &&
#   define or          ||
#   define rivp        (rumbleIntValuePair *)
#   define rmsf        (rumble_mailman_shared_folder *)
#   define rmsb        (rumble_mailman_shared_bag *)

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    Macro for implementing a dvector foreach() block as: For each A in B (as type T), using iterator I do {...}
    example: int myValue, myArray[] = {1,2,3,4,5,6,7,8,9};
    d_iterator iter;
    foreach(int, myValue, myArray, iter) { printf("I got %d\n", myValue);
    }
 -----------------------------------------------------------------------------------------------------------------------
 */

#   define foreach dforeach
#   ifdef __cplusplus
}
#   endif
#   define RUMBLE_MAJOR    (RUMBLE_VERSION & 0xFF000000) >> 24
#   define RUMBLE_MINOR    (RUMBLE_VERSION & 0x00FF0000) >> 16
#   define RUMBLE_REV      (RUMBLE_VERSION & 0x0000FFFF)
#endif /* RUMBLE_H */
