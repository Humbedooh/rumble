/*$I0 */
#include "rumble.h"
#include "private.h"
#include "comm.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_master_init(masterHandle *master) {

    /*~~~~~~~~~~~~~~~~~*/
    rumbleService   *svc;
    /*~~~~~~~~~~~~~~~~~*/

    /*$4
     *******************************************************************************************************************
        SMTP initialization
     *******************************************************************************************************************
     */

    svc = comm_serviceHandle("smtp");

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Commands
     -------------------------------------------------------------------------------------------------------------------
     */

    if (svc) {
        rumble_debug(NULL, "core", "Adding SMTP commands and capabilities");
        rumble_service_add_command(svc, "MAIL", rumble_server_smtp_mail);
        rumble_service_add_command(svc, "RCPT", rumble_server_smtp_rcpt);
        rumble_service_add_command(svc, "HELO", rumble_server_smtp_helo);
        rumble_service_add_command(svc, "EHLO", rumble_server_smtp_ehlo);
        rumble_service_add_command(svc, "NOOP", rumble_server_smtp_noop);
        rumble_service_add_command(svc, "DATA", rumble_server_smtp_data);
        rumble_service_add_command(svc, "VRFY", rumble_server_smtp_vrfy);
        rumble_service_add_command(svc, "RSET", rumble_server_smtp_rset);
        rumble_service_add_command(svc, "AUTH", rumble_server_smtp_auth);
        rumble_service_add_capability(svc, "IMPLEMENTATION Rumble Mail Server");

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Capabilities
         ---------------------------------------------------------------------------------------------------------------
         */

        rumble_service_add_capability(svc, "EXPN");
        rumble_service_add_capability(svc, "VRFY");
        rumble_service_add_capability(svc, "PIPELINING");
        rumble_service_add_capability(svc, "8BITMIME");
        rumble_service_add_capability(svc, "AUTH LOGIN PLAIN");
        rumble_service_add_capability(svc, "DSN");
        rumble_service_add_capability(svc, "SIZE");
        rumble_service_add_capability(svc, "ENHANCEDSTATUSCODES");
        rumble_service_add_capability(svc, "XVERP");
        rumble_service_add_capability(svc, "IMPLEMENTATION Rumble Mail Server");
    }

    /*$4
     *******************************************************************************************************************
        POP3 initialization
     *******************************************************************************************************************
     */

    svc = comm_serviceHandle("pop3");

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Commands
     -------------------------------------------------------------------------------------------------------------------
     */

    if (svc) {
        rumble_debug(NULL, "core", "Adding POP3 commands and capabilities");
        rumble_service_add_command(svc, "CAPA", rumble_server_pop3_capa);
        rumble_service_add_command(svc, "USER", rumble_server_pop3_user);
        rumble_service_add_command(svc, "PASS", rumble_server_pop3_pass);
        rumble_service_add_command(svc, "TOP", rumble_server_pop3_top);
        rumble_service_add_command(svc, "UIDL", rumble_server_pop3_uidl);
        rumble_service_add_command(svc, "DELE", rumble_server_pop3_dele);
        rumble_service_add_command(svc, "RETR", rumble_server_pop3_retr);
        rumble_service_add_command(svc, "LIST", rumble_server_pop3_list);
        rumble_service_add_command(svc, "STAT", rumble_server_pop3_stat);

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Capabilities
         ---------------------------------------------------------------------------------------------------------------
         */

        rumble_service_add_capability(svc, "TOP");
        rumble_service_add_capability(svc, "UIDL");
        rumble_service_add_capability(svc, "PIPELINING");
      //  rumble_service_add_capability(svc, "IMPLEMENTATION Rumble Mail Server");
    }

    /*$4
     *******************************************************************************************************************
        IMAP4 initialization
     *******************************************************************************************************************
     */

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Commands
     -------------------------------------------------------------------------------------------------------------------
     */

    svc = comm_serviceHandle("imap4");
    if (svc) {
        rumble_debug(NULL, "core", "Adding IMAP4 commands and capabilities");
        rumble_service_add_command(svc, "LOGIN", rumble_server_imap_login);
        rumble_service_add_command(svc, "LOGOUT", rumble_server_imap_logout);
        rumble_service_add_command(svc, "NOOP", rumble_server_imap_noop);
        rumble_service_add_command(svc, "CAPABILITY", rumble_server_imap_capability);
        rumble_service_add_command(svc, "AUTHENTICATE", rumble_server_imap_authenticate);
        rumble_service_add_command(svc, "SELECT", rumble_server_imap_select);
        rumble_service_add_command(svc, "EXAMINE", rumble_server_imap_examine);
        rumble_service_add_command(svc, "CREATE", rumble_server_imap_create);
        rumble_service_add_command(svc, "DELETE", rumble_server_imap_delete);
        rumble_service_add_command(svc, "RENAME", rumble_server_imap_rename);
        rumble_service_add_command(svc, "SUBSCRIBE", rumble_server_imap_subscribe);
        rumble_service_add_command(svc, "UNSUBSCRIBE", rumble_server_imap_unsubscribe);
        rumble_service_add_command(svc, "LIST", rumble_server_imap_list);
        rumble_service_add_command(svc, "LSUB", rumble_server_imap_lsub);
        rumble_service_add_command(svc, "STATUS", rumble_server_imap_status);
        rumble_service_add_command(svc, "APPEND", rumble_server_imap_append);
        rumble_service_add_command(svc, "CHECK", rumble_server_imap_check);
        rumble_service_add_command(svc, "CLOSE", rumble_server_imap_close);
        rumble_service_add_command(svc, "EXPUNGE", rumble_server_imap_expunge);
        rumble_service_add_command(svc, "SEARCH", rumble_server_imap_search);
        rumble_service_add_command(svc, "FETCH", rumble_server_imap_fetch);
        rumble_service_add_command(svc, "STORE", rumble_server_imap_store);
        rumble_service_add_command(svc, "COPY", rumble_server_imap_copy);
        rumble_service_add_command(svc, "IDLE", rumble_server_imap_idle);
        rumble_service_add_command(svc, "TEST", rumble_server_imap_test);

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Capabilities
         ---------------------------------------------------------------------------------------------------------------
         */

        rumble_service_add_capability(svc, "IMAP4rev1");
        rumble_service_add_capability(svc, "IDLE");
        rumble_service_add_capability(svc, "CONDSTORE");
        rumble_service_add_capability(svc, "AUTH=PLAIN");
        rumble_service_add_capability(svc, "LITERAL");
        rumble_service_add_capability(svc, "UIDPLUS");
        rumble_service_add_capability(svc, "ANNOTATEMORE");
        rumble_service_add_capability(svc, "IMPLEMENTATION Rumble Mail Server");
    }
}
