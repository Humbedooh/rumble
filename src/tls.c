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
	#ifndef RUMBLE_IS_LIBRARY
	printf("%-48s", "Loading SSL...");
	if (gnutls_global_init()) { printf("[BAD]\n"); return; }
	if (gnutls_certificate_allocate_credentials ((gnutls_certificate_credentials_t*)&master->_core.tls.credentials)) { printf("[BAD]\n"); return; }
	
	gnutls_certificate_set_x509_key_file (master->_core.tls.credentials, "config/server.cert", "config/server.key", GNUTLS_X509_FMT_PEM);
	generate_dh_params();
	generate_rsa_params();
	gnutls_certificate_set_dh_params (master->_core.tls.credentials, dh_params);
	gnutls_certificate_set_rsa_export_params (master->_core.tls.credentials, rsa_params);
	//gnutls_ia_set_server_avp_function (master->_core.tls.iacred, client_avp);
	#endif
	printf("[OK]\n");
	
}


void comm_starttls(sessionHandle* session) {
#ifndef RUMBLE_IS_LIBRARY
	int ret;
	masterHandle* master = (masterHandle*) session->_master;

    ret = gnutls_init (&session->client->tls, GNUTLS_SERVER);
	ret = gnutls_priority_set_direct (session->client->tls, "EXPORT", NULL);
	ret = gnutls_credentials_set (session->client->tls, GNUTLS_CRD_CERTIFICATE, master->_core.tls.credentials);
	gnutls_certificate_server_set_request (session->client->tls, GNUTLS_CERT_REQUEST);
	gnutls_dh_set_prime_bits (session->client->tls, 1024);

	gnutls_transport_set_ptr (session->client->tls, (gnutls_transport_ptr_t) session->client->socket);
	ret = gnutls_handshake (session->client->tls);

	if (ret < 0)
    {
      fprintf (stderr, "*** Handshake failed\n");
      gnutls_perror (ret);
	  session->client->tls = 0;
	  return;
    }
  else
    {
      printf ("- Handshake was completed\n");
    }
        session->client->recv = (dummySocketOp) gnutls_record_recv;
        session->client->send = (dummySocketOp) gnutls_record_send;

#endif
}

void comm_stoptls(sessionHandle* session) {
#ifndef RUMBLE_IS_LIBRARY
	if (session->client->tls) {
		gnutls_bye (session->client->tls, GNUTLS_SHUT_RDWR);
		session->client->tls = 0;
		gnutls_deinit (session->client->tls);
	}
#endif
}