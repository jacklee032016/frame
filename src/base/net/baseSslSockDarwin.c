/* 
 *
 */

#include <baseSslSock.h>
#include <baseActiveSock.h>
#include <compat/socket.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseList.h>
#include <baseLock.h>
#include <baseLog.h>
#include <math.h>
#include <baseOs.h>
#include <basePool.h>
#include <baseString.h>
#include <baseTimer.h>
#include <baseFileIo.h>

/* Only build when BASE_HAS_SSL_SOCK and the implementation is Darwin SSL. */
#if defined(BASE_HAS_SSL_SOCK) && BASE_HAS_SSL_SOCK != 0 && \
    (BASE_SSL_SOCK_IMP == BASE_SSL_SOCK_IMP_DARWIN)

#include "TargetConditionals.h"

#include <Security/Security.h>
#include <Security/SecureTransport.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CommonCrypto/CommonDigest.h> 

#define SSL_SOCK_IMP_USE_CIRC_BUF

#include "ssl_sock_imp_common.h"
#include "ssl_sock_imp_common.c"

/* Maximum ciphers */
#define MAX_CIPHERS             100

/* Secure socket structure definition. */
typedef struct darwinssl_sock_t {
    bssl_sock_t  	  base;

    SSLContextRef 	  ssl_ctx;
} darwinssl_sock_t;


/*
 *******************************************************************
 * Static/internal functions.
 *******************************************************************
 */

#define BASE_SSL_ERRNO_START		(BASE_ERRNO_START_USER + \
					 BASE_ERRNO_SPACE_SIZE*6)

#define BASE_SSL_ERRNO_SPACE_SIZE		BASE_ERRNO_SPACE_SIZE

/* Convert from Darwin SSL error to bstatus_t. */
static bstatus_t bstatus_from_err(darwinssl_sock_t *dssock,
				      const char *msg,
				      OSStatus err)
{
    bstatus_t status = (bstatus_t)-err;

    if (__builtin_available(macOS 10.3, iOS 11.3, *)) {
        CFStringRef errmsg;
    
        errmsg = SecCopyErrorMessageString(err, NULL);
        BASE_ERROR("Darwin SSL error %s [%d]: %s", (msg? msg: ""), err,
    	           CFStringGetCStringPtr(errmsg, kCFStringEncodingUTF8));
        CFRelease(errmsg);
    }

    if (status > BASE_SSL_ERRNO_SPACE_SIZE)
	status = BASE_SSL_ERRNO_SPACE_SIZE;
    status += BASE_SSL_ERRNO_START;

    if (dssock)
        dssock->base.last_err = err;

    return status;
}


/* SSLHandshake() and SSLWrite() will call this function to
 * send/write (encrypted) data */
static OSStatus SocketWrite(SSLConnectionRef connection,
                            const void *data,
                            size_t *dataLength)  /* IN/OUT */
{
    bssl_sock_t *ssock = (bssl_sock_t *)connection;
    bsize_t len = *dataLength;

    block_acquire(ssock->write_mutex);
    if (circ_write(&ssock->circ_buf_output, data, len) != BASE_SUCCESS) {
        block_release(ssock->write_mutex);
	*dataLength = 0;
        return errSSLInternal;
    }
    block_release(ssock->write_mutex);

    return noErr;
}

static OSStatus SocketRead(SSLConnectionRef connection,
                           void *data,          /* owned by
                                                 * caller, data
                                                 * RETURNED */
                           size_t *dataLength)  /* IN/OUT */
{
    bssl_sock_t *ssock = (bssl_sock_t *)connection;
    bsize_t len = *dataLength;

    block_acquire(ssock->circ_buf_input_mutex);

    if (circ_empty(&ssock->circ_buf_input)) {
        block_release(ssock->circ_buf_input_mutex);

        /* Data buffers not yet filled */
        *dataLength = 0;
	return errSSLWouldBlock;
    }

    bsize_t circ_buf_size = circ_size(&ssock->circ_buf_input);
    bsize_t read_size = BASE_MIN(circ_buf_size, len);

    circ_read(&ssock->circ_buf_input, data, read_size);

    block_release(ssock->circ_buf_input_mutex);

    *dataLength = read_size;

    return (read_size < len? errSSLWouldBlock: noErr);
}

static bssl_sock_t *ssl_alloc(bpool_t *pool)
{
    return (bssl_sock_t *)BASE_POOL_ZALLOC_T(pool, darwinssl_sock_t);
}

static bstatus_t create_data_from_file(CFDataRef *data,
					 bstr_t *fname, bstr_t *path)
{
    CFURLRef file;
    CFReadStreamRef read_stream;
    UInt8 data_buf[8192];
    CFIndex nbytes = 0;
    
    if (path) {
        CFURLRef filepath;
        CFStringRef path_str;
        
        path_str = CFStringCreateWithBytes(NULL, (const UInt8 *)path->ptr,
        				   path->slen,
        				   kCFStringEncodingUTF8, false);
        if (!path_str) return BASE_ENOMEM;
    
        filepath = CFURLCreateWithFileSystemPath(NULL, path_str,
        					 kCFURLPOSIXPathStyle, true);
        CFRelease(path_str);
        if (!filepath) return BASE_ENOMEM;
    
        path_str = CFStringCreateWithBytes(NULL, (const UInt8 *)fname->ptr,
        				   fname->slen,
    		   			   kCFStringEncodingUTF8, false);
        if (!path_str) {
    	    CFRelease(filepath);
    	    return BASE_ENOMEM;
        }
    
        file = CFURLCreateCopyAppendingPathComponent(NULL, filepath,
        					     path_str, false);
        CFRelease(path_str);
        CFRelease(filepath);
    } else {
        file = CFURLCreateFromFileSystemRepresentation(NULL,
    	       (const UInt8 *)fname->ptr, fname->slen, false);
    }
    
    if (!file)
        return BASE_ENOMEM;
    
    read_stream = CFReadStreamCreateWithFile(NULL, file);
    CFRelease(file);
    
    if (!read_stream)
        return BASE_ENOTFOUND;
    
    if (!CFReadStreamOpen(read_stream)) {
        BASE_CRIT("Failed opening file");
        CFRelease(read_stream);
        return BASE_EINVAL;
    }
    
    nbytes = CFReadStreamRead(read_stream, data_buf,
    			      sizeof(data_buf));
    if (nbytes > 0)
        *data = CFDataCreate(NULL, data_buf, nbytes);
    else
    	*data = NULL;
    
    CFReadStreamClose(read_stream);
    CFRelease(read_stream);
    
    return (*data? BASE_SUCCESS: BASE_EINVAL);
}

static bstatus_t set_cert(darwinssl_sock_t *dssock, bssl_cert_t *cert)
{
    CFStringRef password = NULL;
    CFDataRef cert_data = NULL;
    void *keys[1] = {NULL};
    void *values[1] = {NULL};
    CFDictionaryRef options;
    CFArrayRef items;
    CFIndex i, count;
    SecIdentityRef identity = NULL;
    CFTypeRef cert_arr[1];
    CFArrayRef cert_refs;
    OSStatus err;
    bstatus_t status;


    if (cert->privkey_file.slen || cert->privkey_buf.slen ||
    	cert->privkey_pass.slen)
    {
    	BASE_WARN( "Ignoring supplied private key. Private key must be placed in the keychain instead."));
    }


    if (cert->cert_file.slen) {
    	status = create_data_from_file(&cert_data, &cert->cert_file, NULL);
    	if (status != BASE_SUCCESS) {
    	    BASE_CRIT( "Failed reading cert file");
    	    return status;
    	}
    } else if (cert->cert_buf.slen) {
    	cert_data = CFDataCreate(NULL, (const UInt8 *)cert->cert_buf.ptr,
    				 cert->cert_buf.slen);
    	if (!cert_data)
    	    return BASE_ENOMEM;
    }

    if (cert_data) {
        if (cert->privkey_pass.slen) {
	    password = CFStringCreateWithBytes(NULL,
		           (const UInt8 *)cert->privkey_pass.ptr,
		           cert->privkey_pass.slen,
		           kCFStringEncodingUTF8,
		           false);
	    keys[0] = (void *)kSecImportExportPassphrase;
	    values[0] = (void *)password;
        }
    
        options = CFDictionaryCreate(NULL, (const void **)keys,
    				     (const void **)values,
    				     (password? 1: 0), NULL, NULL);
        if (!options)
    	    return BASE_ENOMEM;
        
#if TARGET_OS_IPHONE
        err = SecPKCS12Import(cert_data, options, &items);
#else
        {
    	    SecExternalFormat bformat[3] = {kSecFormatPKCS12,
    	    				       kSecFormatPEMSequence,
    	    				       kSecFormatX509Cert/* DER */};
    	    SecExternalItemType btype = kSecItemTypeCertificate;
    	    SecItemImportExportKeyParameters key_params;
    
    	    bbzero(&key_params, sizeof(key_params));
    	    key_params.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
    	    key_params.passphrase = password;
    
    	    for (i = 0; i < BASE_ARRAY_SIZE(bformat); i++) {
    	    	items = NULL;
    		err = SecItemImport(cert_data, NULL, &bformat[i],
    				    &btype, 0, &key_params, NULL, &items);
    		if (err == noErr && items) {
    		    break;
    		}
    	    }
        }
#endif
    
        CFRelease(options);
        if (password)
    	    CFRelease(password);
        CFRelease(cert_data);
        if (err != noErr || !items) {
    	    return bstatus_from_err(dssock, "SecItemImport", err);
        }
    
        count = CFArrayGetCount(items);
    
        for (i = 0; i < count; i++) {
    	    CFTypeRef item;
    	    CFTypeID item_id;
    	
    	    item = (CFTypeRef) CFArrayGetValueAtIndex(items, i);
    	    item_id = CFGetTypeID(item);
    
    	    if (item_id == CFDictionaryGetTypeID()) {
    	    	identity = (SecIdentityRef)
    		           CFDictionaryGetValue((CFDictionaryRef) item,
    					        kSecImportItemIdentity);
    	        break;
    	    }
#if !TARGET_OS_IPHONE
    	    else if (item_id == SecCertificateGetTypeID()) {
    	    	err = SecIdentityCreateWithCertificate(NULL,
    		          (SecCertificateRef) item, &identity);
    	    	if (err != noErr) {
    		    bstatus_from_err(dssock, "SecIdentityCreate", err);
    	    	    if (err == errSecItemNotFound) {
    	    	    	BASE_CRIT("Private key must be placed in the keychain");
    	    	    }
    	    	} else {
    	    	    break;
    	    	}
    	    }
#endif
        }
    
        CFRelease(items);
    
        if (!identity) {
    	    BASE_CRIT("Failed extracting identity from the cert file");
    	    return BASE_EINVAL;
        }
        
        cert_arr[0] = identity;
        cert_refs = CFArrayCreate(NULL, (const void **)cert_arr, 1,
    			      &kCFTypeArrayCallBacks);
        if (!cert_refs)
    	    return BASE_ENOMEM;
    
        err = SSLSetCertificate(dssock->ssl_ctx, cert_refs);
    
        CFRelease(cert_refs);
        CFRelease(identity);
        if (err != noErr)
    	    return bstatus_from_err(dssock, "SetCertificate", err);
    }

    return BASE_SUCCESS;
}

/* Create and initialize new Darwin SSL context and instance */
static bstatus_t ssl_create(bssl_sock_t *ssock)
{
    darwinssl_sock_t *dssock = (darwinssl_sock_t *)ssock;
    SSLContextRef ssl_ctx;
    SSLProtocol min_proto = kSSLProtocolUnknown;
    SSLProtocol max_proto = kSSLProtocolUnknown;
    OSStatus err;
    bstatus_t status;

   /* Initialize input circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->circ_buf_input, 8192);
    if (status != BASE_SUCCESS)
        return status;

    /* Initialize output circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->circ_buf_output, 8192);
    if (status != BASE_SUCCESS)
        return status;

    /* Create SSL context */
    ssl_ctx = SSLCreateContext(NULL, (ssock->is_server? kSSLServerSide:
    				      kSSLClientSide), kSSLStreamType);
    if (ssl_ctx == NULL)
    	return BASE_ENOMEM;

    dssock->ssl_ctx = ssl_ctx;

    /* Set certificate */
    if (ssock->cert)  {
	status = set_cert(dssock, ssock->cert);
	if (status != BASE_SUCCESS)
	    return status;
    }

    /* Set min and max protocol version */
    if (ssock->param.proto == BASE_SSL_SOCK_PROTO_DEFAULT) {
        /* SSL 2.0 is deprecated. */
	ssock->param.proto = BASE_SSL_SOCK_PROTO_ALL &
			     ~BASE_SSL_SOCK_PROTO_SSL2;
    }

    if (ssock->param.proto & BASE_SSL_SOCK_PROTO_SSL2) {
    	if (!min_proto) min_proto = kSSLProtocol2;
	max_proto = kSSLProtocol2;
    }
    if (ssock->param.proto & BASE_SSL_SOCK_PROTO_SSL3) {
    	if (!min_proto) min_proto = kSSLProtocol3;
	max_proto = kSSLProtocol3;
    }
    if (ssock->param.proto & BASE_SSL_SOCK_PROTO_TLS1) {
    	if (!min_proto) min_proto = kTLSProtocol1;
	max_proto = kTLSProtocol1;
    }
    if (ssock->param.proto & BASE_SSL_SOCK_PROTO_TLS1_1) {
    	if (!min_proto) min_proto = kTLSProtocol11;
	max_proto = kTLSProtocol11;
    }
    if (ssock->param.proto & BASE_SSL_SOCK_PROTO_TLS1_2) {
    	if (!min_proto) min_proto = kTLSProtocol12;
	max_proto = kTLSProtocol12;
    }
    if (min_proto != kSSLProtocolUnknown) {
        err = SSLSetProtocolVersionMin(ssl_ctx, min_proto);
        if (err != noErr) bstatus_from_err(dssock, "SetVersionMin", err);
    }

    if (max_proto != kSSLProtocolUnknown) {
        err = SSLSetProtocolVersionMax(ssl_ctx, max_proto);
        if (err != noErr) bstatus_from_err(dssock, "SetVersionMax", err);
    }

    /* SSL verification options */
    if (!ssock->is_server && !ssock->param.verify_peer) {
    	err = SSLSetSessionOption(ssl_ctx, kSSLSessionOptionBreakOnServerAuth,
                                  true);
    	if (err != noErr)
    	    bstatus_from_err(dssock, "BreakOnServerAuth", err);
    } else if (ssock->is_server && ssock->param.require_client_cert) {
    	err = SSLSetClientSideAuthenticate(ssl_ctx, kAlwaysAuthenticate);
        if (err != noErr)
            	bstatus_from_err(dssock, "SetClientSideAuth", err);

    	if (!ssock->param.verify_peer) {
    	    err = SSLSetSessionOption(ssl_ctx,
                              	      kSSLSessionOptionBreakOnClientAuth,
                              	      true);
            if (err != noErr)
            	bstatus_from_err(dssock, "BreakOnClientAuth", err);
        }
    }

    /* Set cipher list */
    if (ssock->param.ciphers_num > 0) {
    	int i, n = ssock->param.ciphers_num;
     	SSLCipherSuite ciphers[MAX_CIPHERS];
    	
    	if (n > BASE_ARRAY_SIZE(ciphers))
    	    n = BASE_ARRAY_SIZE(ciphers);
    	for (i = 0; i < n; i++)
	    ciphers[i] = (SSLCipherSuite)ssock->param.ciphers[i];
    
    	err = SSLSetEnabledCiphers(ssl_ctx, ciphers, n);
    	if (err != noErr)
	    return bstatus_from_err(dssock, "SetEnabledCiphers", err);
    }

    /* Register I/O functions */
    err = SSLSetIOFuncs(ssl_ctx, SocketRead, SocketWrite);
    if (err != noErr)
	return bstatus_from_err(dssock, "SetIOFuncs", err);

    /* Establish a connection */
    err = SSLSetConnection(ssl_ctx, ssock);
    if (err != noErr)
	return bstatus_from_err(dssock, "SetConnection", err);

    return BASE_SUCCESS;
}


/* Destroy Darwin SSL. */
static void ssl_destroy(bssl_sock_t *ssock)
{
    darwinssl_sock_t *dssock = (darwinssl_sock_t *)ssock;

    /* Close the connection and free SSL context */
    if (dssock->ssl_ctx) {
    	SSLClose(dssock->ssl_ctx);

    	CFRelease(dssock->ssl_ctx);
    	dssock->ssl_ctx = NULL;
    }

    /* Destroy circular buffers */
    circ_deinit(&ssock->circ_buf_input);
    circ_deinit(&ssock->circ_buf_output);
}


/* Reset socket state. */
static void ssl_reset_sock_state(bssl_sock_t *ssock)
{
    block_acquire(ssock->circ_buf_output_mutex);
    ssock->ssl_state = SSL_STATE_NULL;
    block_release(ssock->circ_buf_output_mutex);

    ssl_close_sockets(ssock);
}


/* This function is taken from Apple's sslAppUtils.cpp (version 58286.41.2),
 * with some modifications.
 */
const char *sslGetCipherSuiteString(SSLCipherSuite cs)
{
    switch (cs) {
        /* TLS cipher suites, RFC 2246 */
        case SSL_NULL_WITH_NULL_NULL:               
            return "TLS_NULL_WITH_NULL_NULL";
        case SSL_RSA_WITH_NULL_MD5:                 
            return "TLS_RSA_WITH_NULL_MD5";
        case SSL_RSA_WITH_NULL_SHA:                 
            return "TLS_RSA_WITH_NULL_SHA";
        case SSL_RSA_EXPORT_WITH_RC4_40_MD5:        
            return "TLS_RSA_EXPORT_WITH_RC4_40_MD5";
        case SSL_RSA_WITH_RC4_128_MD5:              
            return "TLS_RSA_WITH_RC4_128_MD5";
        case SSL_RSA_WITH_RC4_128_SHA:              
            return "TLS_RSA_WITH_RC4_128_SHA";
        case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:    
            return "TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5";
        case SSL_RSA_WITH_IDEA_CBC_SHA:             
            return "TLS_RSA_WITH_IDEA_CBC_SHA";
        case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:     
            return "TLS_RSA_EXPORT_WITH_DES40_CBC_SHA";
        case SSL_RSA_WITH_DES_CBC_SHA:              
            return "TLS_RSA_WITH_DES_CBC_SHA";
        case SSL_RSA_WITH_3DES_EDE_CBC_SHA:         
            return "TLS_RSA_WITH_3DES_EDE_CBC_SHA";
        case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:  
            return "TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA";
        case SSL_DH_DSS_WITH_DES_CBC_SHA:           
            return "TLS_DH_DSS_WITH_DES_CBC_SHA";
        case SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA:      
            return "TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA";
        case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:  
            return "TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA";
        case SSL_DH_RSA_WITH_DES_CBC_SHA:           
            return "TLS_DH_RSA_WITH_DES_CBC_SHA";
        case SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA:      
            return "TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA";
        case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA: 
            return "TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA";
        case SSL_DHE_DSS_WITH_DES_CBC_SHA:          
            return "TLS_DHE_DSS_WITH_DES_CBC_SHA";
        case SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA:     
            return "TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA";
        case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA: 
            return "TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA";
        case SSL_DHE_RSA_WITH_DES_CBC_SHA:          
            return "TLS_DHE_RSA_WITH_DES_CBC_SHA";
        case SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA:     
            return "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA";
        case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:    
            return "TLS_DH_anon_EXPORT_WITH_RC4_40_MD5";
        case SSL_DH_anon_WITH_RC4_128_MD5:          
            return "TLS_DH_anon_WITH_RC4_128_MD5";
        case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA: 
            return "TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA";
        case SSL_DH_anon_WITH_DES_CBC_SHA:          
            return "TLS_DH_anon_WITH_DES_CBC_SHA";
        case SSL_DH_anon_WITH_3DES_EDE_CBC_SHA:     
            return "TLS_DH_anon_WITH_3DES_EDE_CBC_SHA";

        /* SSLv3 Fortezza cipher suites, from NSS */
        case SSL_FORTEZZA_DMS_WITH_NULL_SHA:        
            return "SSL_FORTEZZA_DMS_WITH_NULL_SHA";
        case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:
            return "SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA";

        /* TLS addenda using AES-CBC, RFC 3268 */
        case TLS_RSA_WITH_AES_128_CBC_SHA:          
            return "TLS_RSA_WITH_AES_128_CBC_SHA";
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA:       
            return "TLS_DH_DSS_WITH_AES_128_CBC_SHA";
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA:       
            return "TLS_DH_RSA_WITH_AES_128_CBC_SHA";
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:      
            return "TLS_DHE_DSS_WITH_AES_128_CBC_SHA";
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:      
            return "TLS_DHE_RSA_WITH_AES_128_CBC_SHA";
        case TLS_DH_anon_WITH_AES_128_CBC_SHA:      
            return "TLS_DH_anon_WITH_AES_128_CBC_SHA";
        case TLS_RSA_WITH_AES_256_CBC_SHA:          
            return "TLS_RSA_WITH_AES_256_CBC_SHA";
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA:       
            return "TLS_DH_DSS_WITH_AES_256_CBC_SHA";
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA:       
            return "TLS_DH_RSA_WITH_AES_256_CBC_SHA";
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:      
            return "TLS_DHE_DSS_WITH_AES_256_CBC_SHA";
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:      
            return "TLS_DHE_RSA_WITH_AES_256_CBC_SHA";
        case TLS_DH_anon_WITH_AES_256_CBC_SHA:      
            return "TLS_DH_anon_WITH_AES_256_CBC_SHA";

        /* ECDSA addenda, RFC 4492 */
        case TLS_ECDH_ECDSA_WITH_NULL_SHA:          
            return "TLS_ECDH_ECDSA_WITH_NULL_SHA";
        case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:       
            return "TLS_ECDH_ECDSA_WITH_RC4_128_SHA";
        case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:  
            return "TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA";
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:   
            return "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA";
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:   
            return "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA";
        case TLS_ECDHE_ECDSA_WITH_NULL_SHA:         
            return "TLS_ECDHE_ECDSA_WITH_NULL_SHA";
        case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:      
            return "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA";
        case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA: 
            return "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA";
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:  
            return "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA";
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:  
            return "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA";
        case TLS_ECDH_RSA_WITH_NULL_SHA:            
            return "TLS_ECDH_RSA_WITH_NULL_SHA";
        case TLS_ECDH_RSA_WITH_RC4_128_SHA:         
            return "TLS_ECDH_RSA_WITH_RC4_128_SHA";
        case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:    
            return "TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA";
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:     
            return "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA";
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:     
            return "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA";
        case TLS_ECDHE_RSA_WITH_NULL_SHA:           
            return "TLS_ECDHE_RSA_WITH_NULL_SHA";
        case TLS_ECDHE_RSA_WITH_RC4_128_SHA:        
            return "TLS_ECDHE_RSA_WITH_RC4_128_SHA";
        case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:   
            return "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA";
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:    
            return "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA";
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:    
            return "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA";
        case TLS_ECDH_anon_WITH_NULL_SHA:           
            return "TLS_ECDH_anon_WITH_NULL_SHA";
        case TLS_ECDH_anon_WITH_RC4_128_SHA:        
            return "TLS_ECDH_anon_WITH_RC4_128_SHA";
        case TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA:   
            return "TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA";
        case TLS_ECDH_anon_WITH_AES_128_CBC_SHA:    
            return "TLS_ECDH_anon_WITH_AES_128_CBC_SHA";
        case TLS_ECDH_anon_WITH_AES_256_CBC_SHA:    
            return "TLS_ECDH_anon_WITH_AES_256_CBC_SHA";

        /* TLS 1.2 addenda, RFC 5246 */
        case TLS_RSA_WITH_AES_128_CBC_SHA256:       
            return "TLS_RSA_WITH_AES_128_CBC_SHA256";
        case TLS_RSA_WITH_AES_256_CBC_SHA256:       
            return "TLS_RSA_WITH_AES_256_CBC_SHA256";
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA256:    
            return "TLS_DH_DSS_WITH_AES_128_CBC_SHA256";
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA256:    
            return "TLS_DH_RSA_WITH_AES_128_CBC_SHA256";
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA256:   
            return "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256";
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:   
            return "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256";
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA256:    
            return "TLS_DH_DSS_WITH_AES_256_CBC_SHA256";
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA256:    
            return "TLS_DH_RSA_WITH_AES_256_CBC_SHA256";
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA256:   
            return "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256";
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:   
            return "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256";
        case TLS_DH_anon_WITH_AES_128_CBC_SHA256:   
            return "TLS_DH_anon_WITH_AES_128_CBC_SHA256";
        case TLS_DH_anon_WITH_AES_256_CBC_SHA256:   
            return "TLS_DH_anon_WITH_AES_256_CBC_SHA256";

        /* TLS addenda using AES-GCM, RFC 5288 */
        case TLS_RSA_WITH_AES_128_GCM_SHA256:       
            return "TLS_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_RSA_WITH_AES_256_GCM_SHA384:       
            return "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:   
            return "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:   
            return "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384";
        case TLS_DH_RSA_WITH_AES_128_GCM_SHA256:    
            return "TLS_DH_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_DH_RSA_WITH_AES_256_GCM_SHA384:    
            return "TLS_DH_RSA_WITH_AES_256_GCM_SHA384";
        case TLS_DHE_DSS_WITH_AES_128_GCM_SHA256:   
            return "TLS_DHE_DSS_WITH_AES_128_GCM_SHA256";
        case TLS_DHE_DSS_WITH_AES_256_GCM_SHA384:   
            return "TLS_DHE_DSS_WITH_AES_256_GCM_SHA384";
        case TLS_DH_DSS_WITH_AES_128_GCM_SHA256:    
            return "TLS_DH_DSS_WITH_AES_128_GCM_SHA256";
        case TLS_DH_DSS_WITH_AES_256_GCM_SHA384:    
            return "TLS_DH_DSS_WITH_AES_256_GCM_SHA384";
        case TLS_DH_anon_WITH_AES_128_GCM_SHA256:   
            return "TLS_DH_anon_WITH_AES_128_GCM_SHA256";
        case TLS_DH_anon_WITH_AES_256_GCM_SHA384:   
            return "TLS_DH_anon_WITH_AES_256_GCM_SHA384";

        /* ECDSA addenda, RFC 5289 */
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:   
            return "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256";
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:   
            return "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384";
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256:    
            return "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256";
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384:    
            return "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384";
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:     
            return "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256";
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:     
            return "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384";
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256:      
            return "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256";
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384:      
            return "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384";
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:   
            return "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256";
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:   
            return "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384";
        case TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256:    
            return "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256";
        case TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384:    
            return "TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384";
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:     
            return "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:     
            return "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384";
        case TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256:      
            return "TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384:      
            return "TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384";
            
        /*
         * Tags for SSL 2 cipher kinds which are not specified for SSL 3.
         */
        case SSL_RSA_WITH_RC2_CBC_MD5:              
            return "TLS_RSA_WITH_RC2_CBC_MD5";
        case SSL_RSA_WITH_IDEA_CBC_MD5:             
            return "TLS_RSA_WITH_IDEA_CBC_MD5";
        case SSL_RSA_WITH_DES_CBC_MD5:              
            return "TLS_RSA_WITH_DES_CBC_MD5";
        case SSL_RSA_WITH_3DES_EDE_CBC_MD5:         
            return "TLS_RSA_WITH_3DES_EDE_CBC_MD5";
        case SSL_NO_SUCH_CIPHERSUITE:               
            return "SSL_NO_SUCH_CIPHERSUITE";
            
        default:
            return "TLS_CIPHER_STRING_UNKNOWN";
    }
}

static void ssl_ciphers_populate(void)
{
    if (!ssl_cipher_num) {
     	SSLContextRef ssl_ctx;
     	SSLCipherSuite ciphers[MAX_CIPHERS];
     	size_t i, n;
     	OSStatus err;

	ssl_ctx = SSLCreateContext(NULL, kSSLClientSide, kSSLStreamType);
	if (ssl_ctx == NULL) return;
	
	err = SSLGetNumberSupportedCiphers(ssl_ctx, &n);
	if (err != noErr) goto on_error;
	if (n > BASE_ARRAY_SIZE(ssl_ciphers))
	    n = BASE_ARRAY_SIZE(ssl_ciphers);

	err = SSLGetSupportedCiphers(ssl_ctx, ciphers, &n);
	if (err != noErr) goto on_error;

	for (i = 0; i < n; i++) {
	    ssl_ciphers[i].id = ciphers[i];
	    ssl_ciphers[i].name = sslGetCipherSuiteString(ciphers[i]);
	}
	
	ssl_cipher_num = n;

on_error:
	CFRelease(ssl_ctx);
    }
}


static bssl_cipher ssl_get_cipher(bssl_sock_t *ssock)
{
    darwinssl_sock_t *dssock = (darwinssl_sock_t *)ssock;
    OSStatus err;
    SSLCipherSuite cipher;

    err = SSLGetNegotiatedCipher(dssock->ssl_ctx, &cipher);
    return (err == noErr)? cipher: BASE_TLS_UNKNOWN_CIPHER;
}


#if !TARGET_OS_IPHONE
static void get_info_and_cn(CFArrayRef array, CFMutableStringRef info,
			    CFStringRef *cn)
{
    const void *keys[] = {kSecOIDOrganizationalUnitName, kSecOIDCountryName,
    			  kSecOIDStateProvinceName, kSecOIDLocalityName,
    			  kSecOIDOrganizationName, kSecOIDCommonName};
    const char *labels[] = { "OU=", "C=", "ST=", "L=", "O=", "CN="};
    bbool_t add_separator = BASE_FALSE;
    int i, n;

    *cn = NULL;
    for(i = 0; i < sizeof(keys)/sizeof(keys[0]);  i++) {
        for (n = 0 ; n < CFArrayGetCount(array); n++) {
            CFDictionaryRef dict;
            CFTypeRef dictkey;
            CFStringRef str;

            dict = CFArrayGetValueAtIndex(array, n);
            if (CFGetTypeID(dict) != CFDictionaryGetTypeID())
                continue;
            dictkey = CFDictionaryGetValue(dict, kSecPropertyKeyLabel);
            if (!CFEqual(dictkey, keys[i]))
                continue;
            str = (CFStringRef) CFDictionaryGetValue(dict,
            					     kSecPropertyKeyValue);
            					     
            if (CFStringGetLength(str) > 0) {
            	if (add_separator) {
                    CFStringAppendCString(info, "/", kCFStringEncodingUTF8);
                }
                CFStringAppendCString(info, labels[i], kCFStringEncodingUTF8);
                CFStringAppend(info, str);
		add_separator = BASE_TRUE;
		
		if (CFEqual(keys[i], kSecOIDCommonName))
		    *cn = str;
            }
        }
    }
}

static CFDictionaryRef get_cert_oid(SecCertificateRef cert, CFStringRef oid,
				    CFTypeRef *value)
{
    void *key[1];
    CFArrayRef key_arr;
    CFDictionaryRef vals, dict;

    key[0] = (void *)oid;
    key_arr = CFArrayCreate(NULL, (const void **)key, 1,
    			    &kCFTypeArrayCallBacks);

    vals = SecCertificateCopyValues(cert, key_arr, NULL);
    dict = CFDictionaryGetValue(vals, key[0]);
    *value = CFDictionaryGetValue(dict, kSecPropertyKeyValue);

    CFRelease(key_arr);

    return vals;
}

#endif

/* Get certificate info; in case the certificate info is already populated,
 * this function will check if the contents need updating by inspecting the
 * issuer and the serial number. */
static void get_cert_info(bpool_t *pool, bssl_cert_info *ci,
			  SecCertificateRef cert)
{
    bbool_t update_needed;
    char buf[512];
    size_t bufsize = sizeof(buf);
    const buint8_t *serial_no = NULL;
    size_t serialsize = 0;
    CFMutableStringRef issuer_info;
    CFStringRef str;
    CFDataRef serial = NULL;
#if !TARGET_OS_IPHONE
    CFStringRef issuer_cn = NULL;
    CFDictionaryRef dict;
#endif

    bassert(pool && ci && cert);

    /* Get issuer */
    issuer_info = CFStringCreateMutable(NULL, 0);
#if !TARGET_OS_IPHONE
{
    /* Unfortunately, unlike on Mac, on iOS we don't have these APIs
     * to query the certificate info such as the issuer, version,
     * validity, and alt names.
     */
    CFArrayRef issuer_vals;
    
    dict = get_cert_oid(cert, kSecOIDX509V1IssuerName,
    			(CFTypeRef *)&issuer_vals);
    if (dict) {
    	get_info_and_cn(issuer_vals, issuer_info, &issuer_cn);
    	if (issuer_cn)
    	    issuer_cn = CFStringCreateCopy(NULL, issuer_cn);
    	CFRelease(dict);
    }
}
#endif
    CFStringGetCString(issuer_info, buf, bufsize, kCFStringEncodingUTF8);

    /* Get serial no */
    if (__builtin_available(macOS 10.13, iOS 11.0, *)) {
	serial = SecCertificateCopySerialNumberData(cert, NULL);
    	if (serial) {
    	    serial_no = CFDataGetBytePtr(serial);
    	    serialsize = CFDataGetLength(serial);
    	}
    }

    /* Check if the contents need to be updated */
    update_needed = bstrcmp2(&ci->issuer.info, buf) ||
                    bmemcmp(ci->serial_no, serial_no, serialsize);
    if (!update_needed) {
        CFRelease(issuer_info);
        return;
    }

    /* Update cert info */

    bbzero(ci, sizeof(bssl_cert_info));

    /* Version */
#if !TARGET_OS_IPHONE
{
    CFStringRef version;
    
    dict = get_cert_oid(cert, kSecOIDX509V1Version,
    			(CFTypeRef *)&version);
    if (dict) {
    	ci->version = CFStringGetIntValue(version);
    	CFRelease(dict);
    }
}
#endif

    /* Issuer */
    bstrdup2(pool, &ci->issuer.info, buf);
#if !TARGET_OS_IPHONE
    if (issuer_cn) {
    	CFStringGetCString(issuer_cn, buf, bufsize, kCFStringEncodingUTF8);
    	bstrdup2(pool, &ci->issuer.cn, buf);
    	CFRelease(issuer_cn);
    }
#endif
    CFRelease(issuer_info);

    /* Serial number */
    if (serial) {
    	if (serialsize > sizeof(ci->serial_no))
    	    serialsize = sizeof(ci->serial_no);
    	bmemcpy(ci->serial_no, serial_no, serialsize);
    	CFRelease(serial);
    }

    /* Subject */
    str = SecCertificateCopySubjectSummary(cert);
    CFStringGetCString(str, buf, bufsize, kCFStringEncodingUTF8);
    bstrdup2(pool, &ci->subject.cn, buf);
    CFRelease(str);
#if !TARGET_OS_IPHONE
{
    CFArrayRef subject;
    CFMutableStringRef subject_info;
    
    dict = get_cert_oid(cert, kSecOIDX509V1SubjectName,
    			(CFTypeRef *)&subject);
    if (dict) {
     	subject_info = CFStringCreateMutable(NULL, 0);

    	get_info_and_cn(subject, subject_info, &str);
    
    	CFStringGetCString(subject_info, buf, bufsize, kCFStringEncodingUTF8);
    	bstrdup2(pool, &ci->subject.info, buf);
    
    	CFRelease(dict);
    	CFRelease(subject_info);
    }
}
#endif

    /* Validity */
#if !TARGET_OS_IPHONE
{
    CFNumberRef validity;
    double interval;
    
    dict = get_cert_oid(cert, kSecOIDX509V1ValidityNotBefore,
    			(CFTypeRef *)&validity);
    if (dict) {
        if (CFNumberGetValue(validity, CFNumberGetType(validity),
        		     &interval))
        {
            /* Darwin's absolute reference date is 1 Jan 2001 00:00:00 GMT */
    	    ci->validity.start.sec = (unsigned long)interval + 978278400L;
    	}
    	CFRelease(dict);
    }

    dict = get_cert_oid(cert, kSecOIDX509V1ValidityNotAfter,
    			(CFTypeRef *)&validity);
    if (dict) {
    	if (CFNumberGetValue(validity, CFNumberGetType(validity),
    			     &interval))
    	{
    	    ci->validity.end.sec = (unsigned long)interval + 978278400L;
    	}
    	CFRelease(dict);
    }
}
#endif

    /* Subject Alternative Name extension */
#if !TARGET_OS_IPHONE
{
    CFArrayRef altname;
    CFIndex i;

    dict = get_cert_oid(cert, kSecOIDSubjectAltName, (CFTypeRef *)&altname);
    if (!dict || !CFArrayGetCount(altname))
    	return;

    ci->subj_alt_name.entry = bpool_calloc(pool, CFArrayGetCount(altname),
					     sizeof(*ci->subj_alt_name.entry));

    for (i = 0; i < CFArrayGetCount(altname); ++i) {
    	CFDictionaryRef item;
    	CFStringRef label, value;
	bssl_cert_name_type type = BASE_SSL_CERT_NAME_UNKNOWN;
        
        item = CFArrayGetValueAtIndex(altname, i);
        if (CFGetTypeID(item) != CFDictionaryGetTypeID())
            continue;
        
        label = (CFStringRef)CFDictionaryGetValue(item, kSecPropertyKeyLabel);
	if (CFGetTypeID(label) != CFStringGetTypeID())
	    continue;

        value = (CFStringRef)CFDictionaryGetValue(item, kSecPropertyKeyValue);

	if (!CFStringCompare(label, CFSTR("DNS Name"),
			     kCFCompareCaseInsensitive))
	{
	    if (CFGetTypeID(value) != CFStringGetTypeID())
	    	continue;
	    CFStringGetCString(value, buf, bufsize, kCFStringEncodingUTF8);
	    type = BASE_SSL_CERT_NAME_DNS;
	} else if (!CFStringCompare(label, CFSTR("IP Address"),
			     	    kCFCompareCaseInsensitive))
	{
	    if (CFGetTypeID(value) != CFStringGetTypeID())
	    	continue;
	    CFStringGetCString(value, buf, bufsize, kCFStringEncodingUTF8);
	    type = BASE_SSL_CERT_NAME_IP;
	} else if (!CFStringCompare(label, CFSTR("Email Address"),
			     	    kCFCompareCaseInsensitive))
	{
	    if (CFGetTypeID(value) != CFStringGetTypeID())
	    	continue;
	    CFStringGetCString(value, buf, bufsize, kCFStringEncodingUTF8);
	    type = BASE_SSL_CERT_NAME_RFC822;
	} else if (!CFStringCompare(label, CFSTR("URI"),
			     	    kCFCompareCaseInsensitive))
	{
	    CFStringRef uri;

	    if (CFGetTypeID(value) != CFURLGetTypeID())
	    	continue;
	    uri = CFURLGetString((CFURLRef)value);
	    CFStringGetCString(uri, buf, bufsize, kCFStringEncodingUTF8);
	    type = BASE_SSL_CERT_NAME_URI;
	}

	if (type != BASE_SSL_CERT_NAME_UNKNOWN) {
	    ci->subj_alt_name.entry[ci->subj_alt_name.cnt].type = type;
	    if (type == BASE_SSL_CERT_NAME_IP) {
	    	char ip_buf[BASE_INET6_ADDRSTRLEN+10];
	    	int len = CFStringGetLength(value);
		int af = bAF_INET();

		if (len == sizeof(bin6_addr)) af = bAF_INET6();
		binet_ntop2(af, buf, ip_buf, sizeof(ip_buf));
		bstrdup2(pool,
		    &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name,
		    ip_buf);
	    } else {
		bstrdup2(pool,
	 	    &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name,
		    buf);
	    }
	    ci->subj_alt_name.cnt++;
	}
    }

    CFRelease(dict);
}
#endif
}

/* Update local & remote certificates info. This function should be
 * called after handshake or renegotiation successfully completed. */
static void ssl_update_certs_info(bssl_sock_t *ssock)
{
    darwinssl_sock_t *dssock = (darwinssl_sock_t *)ssock;
    OSStatus err;
    CFIndex count;
    SecTrustRef trust;
    SecCertificateRef cert;

    bassert(ssock->ssl_state == SSL_STATE_ESTABLISHED);
    
    /* Get active local certificate */
    /* There's no API to get local cert */

    /* Get active remote certificate */
    err = SSLCopyPeerTrust(dssock->ssl_ctx, &trust);

    if (err == noErr && trust) {
    	count = SecTrustGetCertificateCount(trust);
    	if (count > 0) {
	    cert = SecTrustGetCertificateAtIndex(trust, 0);
      	    get_cert_info(ssock->pool, &ssock->remote_cert_info, cert);
    	}
	CFRelease(trust);
    } else if (err != noErr) {
    	bstatus_from_err(dssock, "CopyPeerTrust", err);
    }
}

static void ssl_set_state(bssl_sock_t *ssock, bbool_t is_server)
{
    BASE_UNUSED_ARG(ssock);
    BASE_UNUSED_ARG(is_server);
}

static void ssl_set_peer_name(bssl_sock_t *ssock)
{
    darwinssl_sock_t *dssock = (darwinssl_sock_t *)ssock;

    /* Set server name to connect */
    if (ssock->param.server_name.slen) {
        OSStatus err;
        
    	err = SSLSetPeerDomainName(dssock->ssl_ctx,
    				   ssock->param.server_name.ptr,
    				   ssock->param.server_name.slen);
    	if (err != noErr) {
    	    bstatus_from_err(dssock, "SetPeerDomainName", err);
        }
    }
}

static void auto_verify_result(bssl_sock_t *ssock, OSStatus ret)
{
    switch(ret) {
    case errSSLBadCert:
    case errSSLPeerBadCert:
    case errSSLBadCertificateStatusResponse:
    case errSSLPeerUnsupportedCert:
	ssock->verify_status |= BASE_SSL_CERT_EINVALID_FORMAT;
	break;

    case errSSLCertNotYetValid:
    case errSSLCertExpired:
    case errSSLPeerCertExpired:
	ssock->verify_status |= BASE_SSL_CERT_EVALIDITY_PERIOD;
	break;

    case errSSLPeerCertRevoked:
	ssock->verify_status |= BASE_SSL_CERT_EREVOKED;
	break;	

    case errSSLHostNameMismatch:
    	ssock->verify_status |= BASE_SSL_CERT_EIDENTITY_NOT_MATCH;
    	break;

    case errSSLPeerCertUnknown:
    case errSSLUnknownRootCert:
    case errSSLNoRootCert:
    case errSSLPeerUnknownCA:
    case errSSLXCertChainInvalid:
    case errSSLUnrecognizedName:
    	ssock->verify_status |= BASE_SSL_CERT_EUNTRUSTED;
    	break;
    }
}

static bstatus_t verify_cert(darwinssl_sock_t * dssock, bssl_cert_t *cert)
{
    CFDataRef ca_data = NULL;
    SecTrustRef trust;
    bstatus_t status;
    OSStatus err;

    err = SSLCopyPeerTrust(dssock->ssl_ctx, &trust);
    if (err != noErr || !trust) {
    	return bstatus_from_err(dssock, "SSLHandshake-CopyPeerTrust", err);
    }

    if (cert && cert->CA_file.slen) {
    	status = create_data_from_file(&ca_data, &cert->CA_file,
    				       (cert->CA_path.slen? &cert->CA_path:
    				        NULL));
    	if (status != BASE_SUCCESS)
    	    BASE_CRIT( "Failed reading CA file");
    } else if (cert && cert->CA_buf.slen) {
    	ca_data = CFDataCreate(NULL, (const UInt8 *)cert->CA_buf.ptr,
    			       cert->CA_buf.slen);
    	if (!ca_data)
    	    BASE_CRIT("Not enough memory for CA buffer");
    }
    
    if (ca_data) {
    	SecCertificateRef ca_cert;
    	CFMutableArrayRef ca_array;
    	
    	ca_array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
  	if (!ca_array) {
  	    CFRelease(ca_data);
    	    BASE_CRIT("Not enough memory for CA array");
  	    return BASE_ENOMEM;
  	}
    	
    	ca_cert = SecCertificateCreateWithData(NULL, ca_data);
    	CFRelease(ca_data);
    	
    	if (!ca_cert) {
    	    BASE_CRIT( "Failed creating certificate from CA file/buffer. It has to be in DER format.");
    	} else {
    	
    	    CFArrayAppendValue(ca_array, ca_cert);
    	    CFRelease(ca_cert);
    	
  	    err = SecTrustSetAnchorCertificates(trust, ca_array);
  	    CFRelease(ca_array);
  	    if (err != noErr)
  	        bstatus_from_err(dssock, "SetAnchorCerts", err);

  	    err = SecTrustSetAnchorCertificatesOnly(trust, true);
  	    if (err != noErr)
  	        bstatus_from_err(dssock, "SetAnchorCertsOnly", err);
  	}
    }

    err = SecTrustEvaluateAsync(trust,
    	      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
              ^(SecTrustRef trust, SecTrustResultType trust_result)
              {
    	           /* Unfortunately SecTrustEvaluate() cannot seem to get us
    	            * more specific verification result like the original
    	            * error status returned directly by SSLHandshake()
    	            * (see auto_verify_result() above).
    	            *
    	            * If we wish to obtain more info, we can use
    	            * SecTrustCopyProperties()/SecTrustCopyResult(). However,
    	            * the return value will be a dictionary of strings, which
    	            * cannot be easily checked and compared to, due to
    	            * lack of documented possibilites as well as possible
    	            * changes of the strings themselves.
     		    */
     		   bssl_sock_t *ssock = (bssl_sock_t *)dssock;

    		   switch (trust_result) {
    		   case kSecTrustResultInvalid:
		       ssock->verify_status |= BASE_SSL_CERT_EINVALID_FORMAT;
		       break;

    		   case kSecTrustResultDeny:
    		   case kSecTrustResultFatalTrustFailure:
    		       ssock->verify_status |= BASE_SSL_CERT_EUNTRUSTED;
    		       break;

    		   case kSecTrustResultRecoverableTrustFailure:
    		       /* Doc: "If you receive this result, you can retry
    		        * after changing settings. For example, if trust is
    		        * denied because the certificate has expired, ..."
    		        * But this error can also mean another (recoverable)
    		        * failure, though.
    		        */
    		       ssock->verify_status |= BASE_SSL_CERT_EVALIDITY_PERIOD;
    		       break;
    	
    		   case kSecTrustResultOtherError:
		       ssock->verify_status |= BASE_SSL_CERT_EUNKNOWN;
		       break;

    		   case kSecTrustResultUnspecified:
		   case kSecTrustResultProceed:
		       /* The doc says that if the trust result is proceed or
		        * unspecified, it means that the evaluation succeeded.
		        */
		       break;
	      	   }
	      });
    
    CFRelease(trust);
    
    if (err != noErr)
    	return bstatus_from_err(dssock, "SecTrustEvaluateAsync", err);

    return BASE_SUCCESS;
}

/* Try to perform an asynchronous handshake */
static bstatus_t ssl_do_handshake(bssl_sock_t *ssock)
{
    darwinssl_sock_t *dssock = (darwinssl_sock_t *)ssock;
    OSStatus ret;
    bstatus_t status;

    /* Perform SSL handshake */
    block_acquire(ssock->write_mutex);
    ret = SSLHandshake(dssock->ssl_ctx);
    if (ret == errSSLServerAuthCompleted ||
    	ret == errSSLClientAuthCompleted)
    {
        /* Setting kSSLSessionOptionBreakOnServerAuth or
         * kSSLSessionOptionBreakOnClientAuth enables returning from
         * SSLHandshake() when the Secure Transport's
         * automatic verification of server/client certificates is
         * complete to allow application to perform its own
         * certificate verification.
         */
        verify_cert(dssock, ssock->cert);

        /* Here we just continue the handshake and let the application
         * verify the certificate later.
         */
    	ret = SSLHandshake(dssock->ssl_ctx);
    }
    block_release(ssock->write_mutex);

    status = flush_circ_buf_output(ssock, &ssock->handshake_op_key, 0, 0);
    if (status != BASE_SUCCESS && status != BASE_EPENDING) {
	return status;
    }

    if (ret == noErr) {
        /* Handshake has been completed */
        ssock->ssl_state = SSL_STATE_ESTABLISHED;
        return BASE_SUCCESS;
    } else if (ret != errSSLWouldBlock) {
        /* Handshake fails */
        auto_verify_result(ssock, ret);
        return bstatus_from_err(dssock, "SSLHandshake", ret);
    }

    return BASE_EPENDING;
}

static bstatus_t ssl_read(bssl_sock_t *ssock, void *data, int *size)
{
    darwinssl_sock_t *dssock = (darwinssl_sock_t *)ssock;
    bsize_t processed;
    OSStatus err;

    err = SSLRead(dssock->ssl_ctx, data, *size, &processed);

    *size = (int)processed;
    if (err != noErr && err != errSSLWouldBlock)
        return bstatus_from_err(dssock, "SSLRead", err);

    return BASE_SUCCESS;
}

/*
 * Write the plain data to Darwin SSL, it will be encrypted by SSLWrite()
 * and call SocketWrite.
 */
static bstatus_t ssl_write(bssl_sock_t *ssock, const void *data,
			     bssize_t size, int *nwritten)
{
    darwinssl_sock_t *dssock = (darwinssl_sock_t *)ssock;
    bsize_t processed;
    OSStatus err;

    err = SSLWrite(dssock->ssl_ctx, (read_data_t *)data, size, &processed);
    *nwritten = (int)processed;
    if (err != noErr) {
        return bstatus_from_err(dssock, "SSLWrite", err);
    } else if (processed < size) {
    	return BASE_ENOMEM;
    }
    
    return BASE_SUCCESS;
}

static bstatus_t ssl_renegotiate(bssl_sock_t *ssock)
{
    darwinssl_sock_t *dssock = (darwinssl_sock_t *)ssock;
    OSStatus err;

    err = SSLReHandshake(dssock->ssl_ctx);
    if (err != noErr) {
        return bstatus_from_err(dssock, "SSLReHandshake", err);
    }
    
    return BASE_SUCCESS;
}

#endif

