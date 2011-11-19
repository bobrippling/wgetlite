#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "output.h"
#include "wgetlite.h"

static SSL_CTX *ssl_ctx;
static int ssl_inited;

int sslify(struct wgetfile *finfo)
{
	extern struct cfg global_cfg;

	/* shamelessly copied from ii's ssl patch */

	if(!ssl_inited){
		SSL_load_error_strings();
		SSL_library_init();
	}

	if(!ssl_ctx){
		ssl_ctx = SSL_CTX_new(SSLv23_client_method());

		if(!ssl_ctx){
			ERR_print_errors_fp(global_cfg.logf); /* ... weird */
			return 1;
		}
	}

	/* TODO: tofu? confirm unrecognised cert? */

	finfo->ssl = SSL_new(ssl_ctx);
	/*if(!SSL_set_fd(finfo->ssl, finfo->sock)){
		ERR_print_errors_fp(global_cfg.logf);
		return 1;
	}*/

	if(SSL_connect(finfo->ssl) <= 0){
		output_err(OUT_ERR, "ssl connect error");
		ERR_print_errors_fp(global_cfg.logf);
		return 1;
	}

	/* TODO: check cert here */

	return 0;
}

void ssl_termintate()
{
	if(ssl_ctx)
		SSL_CTX_free(ssl_ctx);
}
