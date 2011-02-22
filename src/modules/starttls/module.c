/*
 * File: module.c Author: Humbedooh A simple (but efficient) load balancing module
 * for rumble. Created on January 3, 2011, 8:08 PM
 */
#include "../../rumble.h"
#include <gnutls/gnutls.h>
#include <gnutls/extra.h>
#include <gcrypt.h>
#include <errno.h>
#ifndef ENOMEM
#   define ENOMEM  12
#endif
GCRY_THREAD_OPTION_PTHREAD_IMPL;
static gnutls_dh_params_t   dh_params;
static gnutls_rsa_params_t  rsa_params;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void generate_dh_params(void) {
    gnutls_dh_params_init(&dh_params);
    gnutls_dh_params_generate2(dh_params, 1024);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void generate_rsa_params(void) {
    gnutls_rsa_params_init(&rsa_params);
    gnutls_rsa_params_generate2(rsa_params, 1024);
}

/*
 =======================================================================================================================
    Generic STARTTLS handler
 =======================================================================================================================
 */
ssize_t rumble_tls_start(masterHandle *master, sessionHandle *session, const char *arg, const char *extra) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int                                 ret;
    gnutls_certificate_credentials_t    *pcred;
    gnutls_session_t                    psess;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    switch (session->_tflags & RUMBLE_THREAD_SVCMASK)
    {
    case RUMBLE_THREAD_SMTP:    rcsend(session, "220 OK, starting TLS\r\n"); break;
    case RUMBLE_THREAD_IMAP:    rcprintf(session, "%s OK Begin TLS negotiation now\r\n", extra); break;
    case RUMBLE_THREAD_POP3:    rcsend(session, "OK, starting TLS\r\n"); break;
    default:                    return (RUMBLE_RETURN_IGNORE);
    }

    pcred = (gnutls_certificate_credentials_t *) master->_core.tls_credentials;
    ret = gnutls_init(&psess, GNUTLS_SERVER);
    ret = gnutls_priority_set_direct(psess, "EXPORT", NULL);
    ret = gnutls_credentials_set(psess, GNUTLS_CRD_CERTIFICATE, *pcred);
    gnutls_certificate_server_set_request(psess, GNUTLS_CERT_REQUEST);
    gnutls_dh_set_prime_bits(psess, 1024);
    gnutls_transport_set_ptr(psess, (gnutls_transport_ptr_t) session->client->socket);
    ret = gnutls_handshake(psess);
    session->client->tls = psess;
    if (ret < 0) {
        fprintf(stderr, "*** TLS Handshake failed\n");
        gnutls_perror(ret);
        session->client->tls = 0;
        return (RUMBLE_RETURN_FAILURE);
    }

    /* Set the dummy send/recv operators */
    session->client->recv = (dummySocketOp) gnutls_record_recv;
    session->client->send = (dummySocketOp) gnutls_record_send;
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    Generic STOPTLS handler (or called when a TLS connection is closed)
 =======================================================================================================================
 */
ssize_t rumble_tls_stop(sessionHandle *session) {
    if (session->client->tls) {
        gnutls_bye((gnutls_session_t) session->client->tls, GNUTLS_SHUT_RDWR);
        gnutls_deinit((gnutls_session_t) session->client->tls);
        session->client->tls = 0;
    }

    session->client->recv = 0;
    session->client->send = 0;
}

/*
 =======================================================================================================================
    Standard module initialization function
 =======================================================================================================================
 */
rumblemodule rumble_module_init(void *master, rumble_module_info *modinfo) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    masterHandle    *m = (masterHandle *) master;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    modinfo->title = "TLS module";
    modinfo->description = "Enables TLS/SSL transport for rumble.";

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    gnutls_certificate_credentials_t    *pcred;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    gcry_control(GCRYCTL_DISABLE_SECMEM_WARN);
    gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
    gcry_control(GCRYCTL_INIT_SECMEM, 16384, 0);
    if (gnutls_global_init()) {
        fprintf(stderr, "<TLS> Failed!\r\n");
        return;
    }

    m->_core.tls_credentials = (gnutls_certificate_credentials_t *) calloc(1, sizeof(gnutls_certificate_credentials_t));
    pcred = (gnutls_certificate_credentials_t *) m->_core.tls_credentials;
    if (gnutls_certificate_allocate_credentials(pcred)) {
        fprintf(stderr, "<TLS> Failed!\r\n");
        return;
    }

    gnutls_certificate_set_x509_key_file(*pcred, "config/server.cert", "config/server.key", GNUTLS_X509_FMT_PEM);
    generate_dh_params();
    generate_rsa_params();
    gnutls_certificate_set_dh_params(*pcred, dh_params);
    gnutls_certificate_set_rsa_export_params(*pcred, rsa_params);

    /*$3
     ===================================================================================================================
        Service hooks
     ===================================================================================================================
     */

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Hook the module to STARTTLS requests.
     -------------------------------------------------------------------------------------------------------------------
     */

    rumble_service_add_command(&m->imap, "STARTTLS", rumble_tls_start);
    rumble_service_add_command(&m->pop3, "STARTTLS", rumble_tls_start);
    rumble_service_add_command(&m->smtp, "STARTTLS", rumble_tls_start);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Announce the STARTTLS capability
     -------------------------------------------------------------------------------------------------------------------
     */

    rumble_service_add_capability(&m->imap, "STARTTLS");
    rumble_service_add_capability(&m->pop3, "STARTTLS");
    rumble_service_add_capability(&m->smtp, "STARTTLS");

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Hook onto services closing connections
     -------------------------------------------------------------------------------------------------------------------
     */

    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_CLOSE, rumble_tls_stop);
    rumble_hook_function(master, RUMBLE_HOOK_POP3 + RUMBLE_HOOK_CLOSE, rumble_tls_stop);
    rumble_hook_function(master, RUMBLE_HOOK_IMAP + RUMBLE_HOOK_CLOSE, rumble_tls_stop);
    return (EXIT_SUCCESS);  /* Tell rumble that the module loaded okay. */
}
