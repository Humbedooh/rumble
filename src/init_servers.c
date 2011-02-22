/*$I0 */
#include "rumble.h"
#include "private.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_master_init(masterHandle *master) {

    /*$4
     *******************************************************************************************************************
        SMTP initialization
     *******************************************************************************************************************
     */

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Vectors
     -------------------------------------------------------------------------------------------------------------------
     */

    master->smtp.cue_hooks = cvector_init();
    master->smtp.init_hooks = cvector_init();
    master->smtp.threads = dvector_init();
    master->smtp.handles = dvector_init();
    master->smtp.commands = cvector_init();
    master->smtp.capabilities = cvector_init();
    master->smtp.init = rumble_smtp_init;
    pthread_mutex_init(&master->smtp.mutex, 0);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Commands
     -------------------------------------------------------------------------------------------------------------------
     */

    rumble_service_add_command(&master->smtp, "MAIL", rumble_server_smtp_mail);
    rumble_service_add_command(&master->smtp, "RCPT", rumble_server_smtp_rcpt);
    rumble_service_add_command(&master->smtp, "HELO", rumble_server_smtp_helo);
    rumble_service_add_command(&master->smtp, "EHLO", rumble_server_smtp_ehlo);
    rumble_service_add_command(&master->smtp, "NOOP", rumble_server_smtp_noop);
    rumble_service_add_command(&master->smtp, "DATA", rumble_server_smtp_data);
    rumble_service_add_command(&master->smtp, "VRFY", rumble_server_smtp_vrfy);
    rumble_service_add_command(&master->smtp, "RSET", rumble_server_smtp_rset);
    rumble_service_add_command(&master->smtp, "AUTH", rumble_server_smtp_auth);
    rumble_service_add_command(&master->smtp, "STARTTLS", rumble_server_smtp_tls);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Capabilities
     -------------------------------------------------------------------------------------------------------------------
     */

    rumble_service_add_capability(&master->smtp, "STARTTLS");
    rumble_service_add_capability(&master->smtp, "EXPN");
    rumble_service_add_capability(&master->smtp, "VRFY");
    rumble_service_add_capability(&master->smtp, "PIPELINING");
    rumble_service_add_capability(&master->smtp, "8BITMIME");
    rumble_service_add_capability(&master->smtp, "AUTH LOGIN PLAIN");
    rumble_service_add_capability(&master->smtp, "DSN");
    rumble_service_add_capability(&master->smtp, "SIZE");
    rumble_service_add_capability(&master->smtp, "ENHANCEDSTATUSCODES");
    rumble_service_add_capability(&master->smtp, "XVERP");
    rumble_service_add_capability(&master->smtp, "IMPLEMENTATION Rumble Mail Server");

    /*$4
     *******************************************************************************************************************
        POP3 initialization
     *******************************************************************************************************************
     */

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Vectors
     -------------------------------------------------------------------------------------------------------------------
     */

    master->pop3.cue_hooks = cvector_init();
    master->pop3.init_hooks = cvector_init();
    master->pop3.threads = dvector_init();
    master->pop3.handles = dvector_init();
    master->pop3.commands = cvector_init();
    master->pop3.capabilities = cvector_init();
    master->pop3.init = rumble_pop3_init;
    pthread_mutex_init(&master->pop3.mutex, 0);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Commands
     -------------------------------------------------------------------------------------------------------------------
     */

    rumble_service_add_command(&master->pop3, "CAPA", rumble_server_pop3_capa);
    rumble_service_add_command(&master->pop3, "USER", rumble_server_pop3_user);
    rumble_service_add_command(&master->pop3, "PASS", rumble_server_pop3_pass);
    rumble_service_add_command(&master->pop3, "TOP", rumble_server_pop3_top);
    rumble_service_add_command(&master->pop3, "UIDL", rumble_server_pop3_uidl);
    rumble_service_add_command(&master->pop3, "DELE", rumble_server_pop3_dele);
    rumble_service_add_command(&master->pop3, "RETR", rumble_server_pop3_retr);
    rumble_service_add_command(&master->pop3, "LIST", rumble_server_pop3_list);
    rumble_service_add_command(&master->pop3, "STARTTLS", rumble_server_pop3_starttls);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Capabilities
     -------------------------------------------------------------------------------------------------------------------
     */

    rumble_service_add_capability(&master->pop3, "STARTTLS");
    rumble_service_add_capability(&master->pop3, "TOP");
    rumble_service_add_capability(&master->pop3, "UIDL");
    rumble_service_add_capability(&master->pop3, "PIPELINING");
    rumble_service_add_capability(&master->pop3, "IMPLEMENTATION Rumble Mail Server");

    /*$4
     *******************************************************************************************************************
        IMAP4 initialization
     *******************************************************************************************************************
     */

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Vectors
     -------------------------------------------------------------------------------------------------------------------
     */

    master->imap.cue_hooks = cvector_init();
    master->imap.init_hooks = cvector_init();
    master->imap.threads = dvector_init();
    master->imap.handles = dvector_init();
    master->imap.commands = cvector_init();
    master->imap.capabilities = cvector_init();
    master->imap.init = rumble_imap_init;
    pthread_mutex_init(&master->imap.mutex, 0);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Commands
     -------------------------------------------------------------------------------------------------------------------
     */

    rumble_service_add_command(&master->imap, "LOGIN", rumble_server_imap_login);
    rumble_service_add_command(&master->imap, "LOGOUT", rumble_server_imap_logout);
    rumble_service_add_command(&master->imap, "NOOP", rumble_server_imap_noop);
    rumble_service_add_command(&master->imap, "CAPABILITY", rumble_server_imap_capability);
    rumble_service_add_command(&master->imap, "AUTHENTICATE", rumble_server_imap_authenticate);
    rumble_service_add_command(&master->imap, "STARTTLS", rumble_server_imap_starttls);
    rumble_service_add_command(&master->imap, "SELECT", rumble_server_imap_select);
    rumble_service_add_command(&master->imap, "EXAMINE", rumble_server_imap_examine);
    rumble_service_add_command(&master->imap, "CREATE", rumble_server_imap_create);
    rumble_service_add_command(&master->imap, "DELETE", rumble_server_imap_delete);
    rumble_service_add_command(&master->imap, "RENAME", rumble_server_imap_rename);
    rumble_service_add_command(&master->imap, "SUBSCRIBE", rumble_server_imap_subscribe);
    rumble_service_add_command(&master->imap, "UNSUBSCRIBE", rumble_server_imap_unsubscribe);
    rumble_service_add_command(&master->imap, "LIST", rumble_server_imap_list);
    rumble_service_add_command(&master->imap, "LSUB", rumble_server_imap_lsub);
    rumble_service_add_command(&master->imap, "STATUS", rumble_server_imap_status);
    rumble_service_add_command(&master->imap, "APPEND", rumble_server_imap_append);
    rumble_service_add_command(&master->imap, "CHECK", rumble_server_imap_check);
    rumble_service_add_command(&master->imap, "CLOSE", rumble_server_imap_close);
    rumble_service_add_command(&master->imap, "EXPUNGE", rumble_server_imap_expunge);
    rumble_service_add_command(&master->imap, "SEARCH", rumble_server_imap_search);
    rumble_service_add_command(&master->imap, "FETCH", rumble_server_imap_fetch);
    rumble_service_add_command(&master->imap, "STORE", rumble_server_imap_store);
    rumble_service_add_command(&master->imap, "COPY", rumble_server_imap_copy);
    rumble_service_add_command(&master->imap, "IDLE", rumble_server_imap_idle);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Capabilities
     -------------------------------------------------------------------------------------------------------------------
     */

    rumble_service_add_capability(&master->imap, "IMAP4rev1");
    rumble_service_add_capability(&master->imap, "IDLE");
    rumble_service_add_capability(&master->imap, "STARTTLS");
    rumble_service_add_capability(&master->imap, "CONDSTORE");
    rumble_service_add_capability(&master->imap, "AUTH=PLAIN");
    rumble_service_add_capability(&master->imap, "UIDPLUS");

    /*$3
     ===================================================================================================================
        Core vectors
     ===================================================================================================================
     */

    master->_core.modules = dvector_init();
    master->_core.workers = dvector_init();
    master->_core.feed_hooks = cvector_init();
    master->_core.parser_hooks = cvector_init();
    master->_core.batv = dvector_init();
    master->domains.list = dvector_init();
    master->domains.rrw = rumble_rw_init();
    master->mailboxes.rrw = rumble_rw_init();
    master->mailboxes.list = dvector_init();
}
