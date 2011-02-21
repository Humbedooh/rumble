/* File: main.c Author: Administrator Created on January 2, 2011, 8:22 AM */
#include "rumble.h"
#include "database.h"
#include "comm.h"
#include "private.h"
#define RUMBLE_INITIAL_THREADS  20
extern masterHandle *rumble_database_master_handle;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_master_init(masterHandle *master) {

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        SMTP initialization
     -------------------------------------------------------------------------------------------------------------------
     */

    /*$3
     ===================================================================================================================
        Vectors
     ===================================================================================================================
     */

    master->smtp.cue_hooks = cvector_init();
    master->smtp.init_hooks = cvector_init();
    master->smtp.threads = dvector_init();
    master->smtp.handles = dvector_init();
    master->smtp.commands = cvector_init();
    master->smtp.init = rumble_smtp_init;
    pthread_mutex_init(&master->smtp.mutex, 0);

    /*$3
     ===================================================================================================================
        Commands
     ===================================================================================================================
     */

    rumble_smtp_add_command(master, "MAIL", rumble_server_smtp_mail);
    rumble_smtp_add_command(master, "RCPT", rumble_server_smtp_rcpt);
    rumble_smtp_add_command(master, "HELO", rumble_server_smtp_helo);
    rumble_smtp_add_command(master, "EHLO", rumble_server_smtp_ehlo);
    rumble_smtp_add_command(master, "NOOP", rumble_server_smtp_noop);
    rumble_smtp_add_command(master, "DATA", rumble_server_smtp_data);
    rumble_smtp_add_command(master, "VRFY", rumble_server_smtp_vrfy);
    rumble_smtp_add_command(master, "RSET", rumble_server_smtp_rset);
    rumble_smtp_add_command(master, "AUTH", rumble_server_smtp_auth);
    rumble_smtp_add_command(master, "STAR", rumble_server_smtp_tls);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        POP3 initialization
     -------------------------------------------------------------------------------------------------------------------
     */

    /*$3
     ===================================================================================================================
        Vectors
     ===================================================================================================================
     */

    master->pop3.cue_hooks = cvector_init();
    master->pop3.init_hooks = cvector_init();
    master->pop3.threads = dvector_init();
    master->pop3.handles = dvector_init();
    master->pop3.commands = cvector_init();
    master->pop3.init = rumble_pop3_init;
    pthread_mutex_init(&master->pop3.mutex, 0);

    /*$3
     ===================================================================================================================
        Commands
     ===================================================================================================================
     */

    rumble_pop3_add_command(master, "CAPA", rumble_server_pop3_capa);
    rumble_pop3_add_command(master, "USER", rumble_server_pop3_user);
    rumble_pop3_add_command(master, "PASS", rumble_server_pop3_pass);
    rumble_pop3_add_command(master, "TOP", rumble_server_pop3_top);
    rumble_pop3_add_command(master, "UIDL", rumble_server_pop3_uidl);
    rumble_pop3_add_command(master, "DELE", rumble_server_pop3_dele);
    rumble_pop3_add_command(master, "RETR", rumble_server_pop3_retr);
    rumble_pop3_add_command(master, "LIST", rumble_server_pop3_list);
    rumble_pop3_add_command(master, "STAR", rumble_server_pop3_starttls);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        IMAP4 initialization
     -------------------------------------------------------------------------------------------------------------------
     */

    /*$3
     ===================================================================================================================
        Vectors
     ===================================================================================================================
     */

    master->imap.cue_hooks = cvector_init();
    master->imap.init_hooks = cvector_init();
    master->imap.threads = dvector_init();
    master->imap.handles = dvector_init();
    master->imap.commands = cvector_init();
    master->imap.init = rumble_imap_init;
    pthread_mutex_init(&master->imap.mutex, 0);

    /*$3
     ===================================================================================================================
        Commands
     ===================================================================================================================
     */

    rumble_imap_add_command(master, "LOGIN", rumble_server_imap_login);
    rumble_imap_add_command(master, "LOGOUT", rumble_server_imap_logout);
    rumble_imap_add_command(master, "NOOP", rumble_server_imap_noop);
    rumble_imap_add_command(master, "CAPABILITY", rumble_server_imap_capability);
    rumble_imap_add_command(master, "AUTHENTICATE", rumble_server_imap_authenticate);
    rumble_imap_add_command(master, "STARTTLS", rumble_server_imap_starttls);
    rumble_imap_add_command(master, "SELECT", rumble_server_imap_select);
    rumble_imap_add_command(master, "EXAMINE", rumble_server_imap_examine);
    rumble_imap_add_command(master, "CREATE", rumble_server_imap_create);
    rumble_imap_add_command(master, "DELETE", rumble_server_imap_delete);
    rumble_imap_add_command(master, "RENAME", rumble_server_imap_rename);
    rumble_imap_add_command(master, "SUBSCRIBE", rumble_server_imap_subscribe);
    rumble_imap_add_command(master, "UNSUBSCRIBE", rumble_server_imap_unsubscribe);
    rumble_imap_add_command(master, "LIST", rumble_server_imap_list);
    rumble_imap_add_command(master, "LSUB", rumble_server_imap_lsub);
    rumble_imap_add_command(master, "STATUS", rumble_server_imap_status);
    rumble_imap_add_command(master, "APPEND", rumble_server_imap_append);
    rumble_imap_add_command(master, "CHECK", rumble_server_imap_check);
    rumble_imap_add_command(master, "CLOSE", rumble_server_imap_close);
    rumble_imap_add_command(master, "EXPUNGE", rumble_server_imap_expunge);
    rumble_imap_add_command(master, "SEARCH", rumble_server_imap_search);
    rumble_imap_add_command(master, "FETCH", rumble_server_imap_fetch);
    rumble_imap_add_command(master, "STORE", rumble_server_imap_store);
    rumble_imap_add_command(master, "COPY", rumble_server_imap_copy);
    rumble_imap_add_command(master, "IDLE", rumble_server_imap_idle);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Core vectors
     -------------------------------------------------------------------------------------------------------------------
     */

    master->_core.modules = dvector_init();
    master->_core.workers = dvector_init();
    master->_core.feed_hooks = cvector_init();
    master->_core.parser_hooks = cvector_init();
    master->_core.batv = dvector_init();
    rumble_database_master_handle = master;
    master->domains.list = dvector_init();
    master->domains.rrw = rumble_rw_init();
    master->mailboxes.rrw = rumble_rw_init();
    master->mailboxes.list = dvector_init();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int main(int argc, char **argv) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             x;
    pthread_t       *t;
    pthread_attr_t  attr;
    masterHandle    *master;
    dvector         *args = dvector_init();
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    master = (masterHandle *) malloc(sizeof(masterHandle));
    if (!master) merror();
    for (x = 0; x < argc; x++) {
        rumble_scan_flags(args, argv[x]);
    }

    if (rhdict(args, "--TEST")) {
        rumble_test();
        exit(EXIT_SUCCESS);
    }

    printf("Starting Rumble Mail Server (v/%u.%02u.%04u)\r\n", RUMBLE_MAJOR, RUMBLE_MINOR, RUMBLE_REV);
    srand(time(0));
    rumble_config_load(master, args);
    rumble_master_init(master);
    rumble_database_load(master);
    rumble_modules_load(master);
    rumble_database_update_domains();
    rumble_crypt_init(master);
    
    pthread_attr_init(&attr);
    printf("%-48s", "Launching core service...");
    t = (pthread_t *) malloc(sizeof(pthread_t));
    dvector_add(master->_core.workers, t);
    pthread_create(t, NULL, rumble_worker_init, master);
    printf("[OK]\n");
    if (rumble_config_int(master, "enablesmtp")) {

        /*~~*/
        int n;
        /*~~*/

        printf("%-48s", "Launching SMTP service...");
        master->smtp.socket = comm_init(master, rumble_config_str(master, "smtpport"));
        for (n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            t = (pthread_t *) malloc(sizeof(pthread_t));
            dvector_add(master->smtp.threads, t);
            pthread_create(t, &attr, master->smtp.init, master);
        }

        printf("[OK]\n");
    }

    if (rumble_config_int(master, "enablepop3")) {

        /*~~*/
        int n;
        /*~~*/

        printf("%-48s", "Launching POP3 service...");
        master->pop3.socket = comm_init(master, rumble_config_str(master, "pop3port"));
        for (n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            t = (pthread_t *) malloc(sizeof(pthread_t));
            dvector_add(master->pop3.threads, t);
            pthread_create(t, &attr, master->pop3.init, master);
        }

        printf("[OK]\n");
    }

    if (rumble_config_int(master, "enableimap4")) {

        /*~~*/
        int n;
        /*~~*/

        printf("%-48s", "Launching IMAP4 service...");
        master->imap.socket = comm_init(master, rumble_config_str(master, "imap4port"));
        for (n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            t = (pthread_t *) malloc(sizeof(pthread_t));
            dvector_add(master->imap.threads, t);
            pthread_create(t, &attr, master->imap.init, master);
        }

        printf("[OK]\n");
    }

    sleep(999999);
    return (EXIT_SUCCESS);
}
