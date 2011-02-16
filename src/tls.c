#include "rumble.h"
#include <gnutls/gnutls.h>
#include <gnutls/extra.h>

static gnutls_dh_params_t dh_params;
static gnutls_rsa_params_t rsa_params;

static void generate_dh_params() {
  gnutls_dh_params_init (&dh_params);
  gnutls_dh_params_generate2 (dh_params, 1024);
}

static void generate_rsa_params() {
  gnutls_rsa_params_init (&rsa_params);
  gnutls_rsa_params_generate2 (rsa_params, 1024);
}

void rumble_crypt_init(masterHandle* master) {
    gnutls_certificate_credentials_t* pcred;
	printf("%-48s", "Loading SSL...");
	if (gnutls_global_init()) { printf("[BAD]\n"); return; }
    master->_core.tls_credentials = (gnutls_certificate_credentials_t*) calloc(1, sizeof(gnutls_certificate_credentials_t));
    pcred = (gnutls_certificate_credentials_t*) master->_core.tls_credentials;
	if (gnutls_certificate_allocate_credentials (pcred)) { printf("[BAD]\n"); return; }
	
	gnutls_certificate_set_x509_key_file (*pcred, "config/server.cert", "config/server.key", GNUTLS_X509_FMT_PEM);
	generate_dh_params();
	generate_rsa_params();
	gnutls_certificate_set_dh_params (*pcred, dh_params);
	gnutls_certificate_set_rsa_export_params (*pcred, rsa_params);
	printf("[OK]\n");

}


/* Generic STARTTLS handler */
void comm_starttls(sessionHandle* session) {
	int ret;
    gnutls_certificate_credentials_t* pcred;
    gnutls_session_t psess;
	masterHandle* master = (masterHandle*) session->_master;
    pcred = (gnutls_certificate_credentials_t*) master->_core.tls_credentials;
    psess = (gnutls_session_t) session->client->tls;

    ret = gnutls_init (&psess, GNUTLS_SERVER);
	ret = gnutls_priority_set_direct (psess, "EXPORT", NULL);
	ret = gnutls_credentials_set (psess, GNUTLS_CRD_CERTIFICATE, *pcred);
	gnutls_certificate_server_set_request (psess, GNUTLS_CERT_REQUEST);
	gnutls_dh_set_prime_bits (psess, 1024);

	gnutls_transport_set_ptr (psess, (gnutls_transport_ptr_t) session->client->socket);
	ret = gnutls_handshake (psess);

	if (ret < 0) {
      fprintf (stderr, "*** TLS Handshake failed\n");
      gnutls_perror (ret);
	  session->client->tls = 0;
	  return;
    }
    /* Set the dummy send/recv operators */
    session->client->recv = (dummySocketOp) gnutls_record_recv;
    session->client->send = (dummySocketOp) gnutls_record_send;
}

/* Generic STOPTLS handler (or called when a TLS connection is closed) */
void comm_stoptls(sessionHandle* session) {
	if (session->client->tls) {
		gnutls_bye ((gnutls_session_t) session->client->tls, GNUTLS_SHUT_RDWR);
		session->client->tls = 0;
		gnutls_deinit ((gnutls_session_t) session->client->tls);
        session->client->tls = 0;
	}
    session->client->recv = (dummySocketOp) recv;
    session->client->send = (dummySocketOp) send;
}

