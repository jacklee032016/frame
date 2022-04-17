/* 
 *
 */
#include <baseSslSock.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <basePool.h>
#include <baseString.h>

/*
 * Initialize the SSL socket configuration with the default values.
 */
void bssl_sock_param_default(bssl_sock_param *param)
{
    bbzero(param, sizeof(*param));

    /* Socket config */
    param->sock_af = BASE_AF_INET;
    param->sock_type = bSOCK_STREAM();
    param->async_cnt = 1;
    param->concurrency = -1;
    param->whole_data = BASE_TRUE;
#if (BASE_SSL_SOCK_IMP == BASE_SSL_SOCK_IMP_GNUTLS)
    /* GnuTLS is allowed to send bigger chunks.*/
    param->send_buffer_size = 65536;
#else
    param->send_buffer_size = 8192;
#endif
#if !defined(BASE_SYMBIAN) || BASE_SYMBIAN==0
    param->read_buffer_size = 1500;
#endif
    param->qos_type = BASE_QOS_TYPE_BEST_EFFORT;
    param->qos_ignore_error = BASE_TRUE;

    param->sockopt_ignore_error = BASE_TRUE;

    /* Security config */
    param->proto = BASE_SSL_SOCK_PROTO_DEFAULT;
}


/*
 * Duplicate SSL socket parameter.
 */
void bssl_sock_param_copy( bpool_t *pool, 
				     bssl_sock_param *dst,
				     const bssl_sock_param *src)
{
    /* Init secure socket param */
    bmemcpy(dst, src, sizeof(*dst));
    if (src->ciphers_num > 0) {
	unsigned i;
	dst->ciphers = (bssl_cipher*)
			bpool_calloc(pool, src->ciphers_num, 
				       sizeof(bssl_cipher));
	for (i = 0; i < src->ciphers_num; ++i)
	    dst->ciphers[i] = src->ciphers[i];
    }

    if (src->curves_num > 0) {
	unsigned i;
    	dst->curves = (bssl_curve *)bpool_calloc(pool, src->curves_num,
					   	     sizeof(bssl_curve));
	for (i = 0; i < src->curves_num; ++i)
	    dst->curves[i] = src->curves[i];
    }

    if (src->server_name.slen) {
        /* Server name must be null-terminated */
        bstrdup_with_null(pool, &dst->server_name, &src->server_name);
    }

    if (src->sigalgs.slen) {
    	/* Sigalgs name must be null-terminated */
    	bstrdup_with_null(pool, &dst->sigalgs, &src->sigalgs);
    }

    if (src->entropy_path.slen) {
    	/* Path name must be null-terminated */
    	bstrdup_with_null(pool, &dst->entropy_path, &src->entropy_path);
    }
}


bstatus_t bssl_cert_get_verify_status_strings(
						buint32_t verify_status, 
						const char *error_strings[],
						unsigned *count)
{
    unsigned i = 0, shift_idx = 0;
    unsigned unknown = 0;
    buint32_t errs;

    BASE_ASSERT_RETURN(error_strings && count, BASE_EINVAL);

    if (verify_status == BASE_SSL_CERT_ESUCCESS && *count) {
	error_strings[0] = "OK";
	*count = 1;
	return BASE_SUCCESS;
    }

    errs = verify_status;

    while (errs && i < *count) {
	buint32_t err;
	const char *p = NULL;

	if ((errs & 1) == 0) {
	    shift_idx++;
	    errs >>= 1;
	    continue;
	}

	err = (1 << shift_idx);

	switch (err) {
	case BASE_SSL_CERT_EISSUER_NOT_FOUND:
	    p = "The issuer certificate cannot be found";
	    break;
	case BASE_SSL_CERT_EUNTRUSTED:
	    p = "The certificate is untrusted";
	    break;
	case BASE_SSL_CERT_EVALIDITY_PERIOD:
	    p = "The certificate has expired or not yet valid";
	    break;
	case BASE_SSL_CERT_EINVALID_FORMAT:
	    p = "One or more fields of the certificate cannot be decoded "
		"due to invalid format";
	    break;
	case BASE_SSL_CERT_EISSUER_MISMATCH:
	    p = "The issuer info in the certificate does not match to the "
		"(candidate) issuer certificate";
	    break;
	case BASE_SSL_CERT_ECRL_FAILURE:
	    p = "The CRL certificate cannot be found or cannot be read "
		"properly";
	    break;
	case BASE_SSL_CERT_EREVOKED:
	    p = "The certificate has been revoked";
	    break;
	case BASE_SSL_CERT_EINVALID_PURPOSE:
	    p = "The certificate or CA certificate cannot be used for the "
		"specified purpose";
	    break;
	case BASE_SSL_CERT_ECHAIN_TOO_LONG:
	    p = "The certificate chain length is too long";
	    break;
	case BASE_SSL_CERT_EIDENTITY_NOT_MATCH:
	    p = "The server identity does not match to any identities "
		"specified in the certificate";
	    break;
	case BASE_SSL_CERT_EUNKNOWN:
	default:
	    unknown++;
	    break;
	}
	
	/* Set error string */
	if (p)
	    error_strings[i++] = p;

	/* Next */
	shift_idx++;
	errs >>= 1;
    }

    /* Unknown error */
    if (unknown && i < *count)
	error_strings[i++] = "Unknown verification error";

    *count = i;

    return BASE_SUCCESS;
}
