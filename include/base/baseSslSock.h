/* 
 *
 */
#ifndef __BASE_SSL_SOCK_H__
#define __BASE_SSL_SOCK_H__

/**
 * @brief Secure socket
 */

#include <baseIoQueue.h>
#include <baseSock.h>
#include <baseSockQos.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_SSL_SOCK Secure socket I/O
 * @brief Secure socket provides security on socket operation using standard
 * security protocols such as SSL and TLS.
 * @ingroup BASE_IO
 * @{
 *
 * Secure socket wraps normal socket and applies security features, i.e: 
 * privacy and data integrity, on the socket traffic, using standard security
 * protocols such as SSL and TLS.
 *
 * Secure socket employs active socket operations, which is similar to (and
 * described more detail) in \ref BASE_ACTIVESOCK.
 */


 /**
 * This opaque structure describes the secure socket.
 */
typedef struct bssl_sock_t bssl_sock_t;


/**
 * Opaque declaration of endpoint certificate or credentials. This may contains
 * certificate, private key, and trusted Certificate Authorities list.
 */
typedef struct bssl_cert_t bssl_cert_t;


typedef enum bssl_cert_verify_flag_t
{
    /**
     * No error in verification.
     */
    BASE_SSL_CERT_ESUCCESS				= 0,

    /**
     * The issuer certificate cannot be found.
     */
    BASE_SSL_CERT_EISSUER_NOT_FOUND			= (1 << 0),

    /**
     * The certificate is untrusted.
     */
    BASE_SSL_CERT_EUNTRUSTED				= (1 << 1),

    /**
     * The certificate has expired or not yet valid.
     */
    BASE_SSL_CERT_EVALIDITY_PERIOD			= (1 << 2),

    /**
     * One or more fields of the certificate cannot be decoded due to
     * invalid format.
     */
    BASE_SSL_CERT_EINVALID_FORMAT				= (1 << 3),

    /**
     * The certificate cannot be used for the specified purpose.
     */
    BASE_SSL_CERT_EINVALID_PURPOSE			= (1 << 4),

    /**
     * The issuer info in the certificate does not match to the (candidate) 
     * issuer certificate, e.g: issuer name not match to subject name
     * of (candidate) issuer certificate.
     */
    BASE_SSL_CERT_EISSUER_MISMATCH			= (1 << 5),

    /**
     * The CRL certificate cannot be found or cannot be read properly.
     */
    BASE_SSL_CERT_ECRL_FAILURE				= (1 << 6),

    /**
     * The certificate has been revoked.
     */
    BASE_SSL_CERT_EREVOKED				= (1 << 7),

    /**
     * The certificate chain length is too long.
     */
    BASE_SSL_CERT_ECHAIN_TOO_LONG				= (1 << 8),

    /**
     * The server identity does not match to any identities specified in 
     * the certificate, e.g: subjectAltName extension, subject common name.
     * This flag will only be set by application as SSL socket does not 
     * perform server identity verification.
     */
    BASE_SSL_CERT_EIDENTITY_NOT_MATCH			= (1 << 30),

    /**
     * Unknown verification error.
     */
    BASE_SSL_CERT_EUNKNOWN				= (1 << 31)

} bssl_cert_verify_flag_t;


typedef enum bssl_cert_name_type
{
    BASE_SSL_CERT_NAME_UNKNOWN = 0,
    BASE_SSL_CERT_NAME_RFC822,
    BASE_SSL_CERT_NAME_DNS,
    BASE_SSL_CERT_NAME_URI,
    BASE_SSL_CERT_NAME_IP
} bssl_cert_name_type;

/**
 * Describe structure of certificate info.
 */
typedef struct bssl_cert_info {

    unsigned	version;	    /**< Certificate version	*/

    buint8_t	serial_no[20];	    /**< Serial number, array of
				         octets, first index is
					 MSB			*/

    struct {
        bstr_t	cn;	    /**< Common name		*/
        bstr_t	info;	    /**< One line subject, fields
					 are separated by slash, e.g:
					 "CN=sample.org/OU=HRD" */
    } subject;			    /**< Subject		*/

    struct {
        bstr_t	cn;	    /**< Common name		*/
        bstr_t	info;	    /**< One line subject, fields
					 are separated by slash.*/
    } issuer;			    /**< Issuer			*/

    struct {
	btime_val	start;	    /**< Validity start		*/
	btime_val	end;	    /**< Validity end		*/
	bbool_t	gmt;	    /**< Flag if validity date/time 
					 use GMT		*/
    } validity;			    /**< Validity		*/

    struct {
	unsigned	cnt;	    /**< # of entry		*/
	struct {
	    bssl_cert_name_type type;
				    /**< Name type		*/
	    bstr_t	name;	    /**< The name		*/
	} *entry;		    /**< Subject alt name entry */
    } subj_alt_name;		    /**< Subject alternative
					 name extension		*/

    bstr_t raw;		    /**< Raw certificate in PEM format, only
					 available for remote certificate. */

    struct {
        unsigned    	cnt;        /**< # of entry     */
        bstr_t       *cert_raw;
    } raw_chain;

} bssl_cert_info;

/**
 * The SSL certificate buffer.
 */
typedef bstr_t bssl_cert_buffer;

/**
 * Create credential from files. TLS server application can provide multiple
 * certificates (RSA, ECC, and DSA) by supplying certificate name with "_rsa"
 * suffix, e.g: "extsip_rsa.pem", the library will automatically check for
 * other certificates with "_ecc" and "_dsa" suffix.
 *
 * @param CA_file	The file of trusted CA list.
 * @param cert_file	The file of certificate.
 * @param privkey_file	The file of private key.
 * @param privkey_pass	The password of private key, if any.
 * @param p_cert	Pointer to credential instance to be created.
 *
 * @return		BASE_SUCCESS when successful.
 */
bstatus_t bssl_cert_load_from_files(bpool_t *pool,
						 const bstr_t *CA_file,
						 const bstr_t *cert_file,
						 const bstr_t *privkey_file,
						 const bstr_t *privkey_pass,
						 bssl_cert_t **p_cert);

/**
 * Create credential from files. TLS server application can provide multiple
 * certificates (RSA, ECC, and DSA) by supplying certificate name with "_rsa"
 * suffix, e.g: "extsip_rsa.pem", the library will automatically check for
 * other certificates with "_ecc" and "_dsa" suffix.
 *
 * This is the same as bssl_cert_load_from_files() but also
 * accepts an additional param CA_path to load CA certificates from
 * a directory.
 *
 * @param CA_file	The file of trusted CA list.
 * @param CA_path	The path to a directory of trusted CA list.
 * @param cert_file	The file of certificate.
 * @param privkey_file	The file of private key.
 * @param privkey_pass	The password of private key, if any.
 * @param p_cert	Pointer to credential instance to be created.
 *
 * @return		BASE_SUCCESS when successful.
 */
bstatus_t bssl_cert_load_from_files2(
						bpool_t *pool,
						const bstr_t *CA_file,
						const bstr_t *CA_path,
						const bstr_t *cert_file,
						const bstr_t *privkey_file,
						const bstr_t *privkey_pass,
						bssl_cert_t **p_cert);


/**
 * Create credential from data buffer. The certificate expected is in 
 * PEM format.
 *
 * @param CA_file	The buffer of trusted CA list.
 * @param cert_file	The buffer of certificate.
 * @param privkey_file	The buffer of private key.
 * @param privkey_pass	The password of private key, if any.
 * @param p_cert	Pointer to credential instance to be created.
 *
 * @return		BASE_SUCCESS when successful.
 */
bstatus_t bssl_cert_load_from_buffer(bpool_t *pool,
					const bssl_cert_buffer *CA_buf,
					const bssl_cert_buffer *cert_buf,
					const bssl_cert_buffer *privkey_buf,
					const bstr_t *privkey_pass,
					bssl_cert_t **p_cert);

/**
 * Dump SSL certificate info.
 *
 * @param ci		The certificate info.
 * @param indent	String for left indentation.
 * @param buf		The buffer where certificate info will be printed on.
 * @param buf_size	The buffer size.
 *
 * @return		The length of the dump result, or -1 when buffer size
 *			is not sufficient.
 */
bssize_t bssl_cert_info_dump(const bssl_cert_info *ci,
					  const char *indent,
					  char *buf,
					  bsize_t buf_size);


/**
 * Get SSL certificate verification error messages from verification status.
 *
 * @param verify_status	The SSL certificate verification status.
 * @param error_strings	Array of strings to receive the verification error 
 *			messages.
 * @param count		On input it specifies maximum error messages should be
 *			retrieved. On output it specifies the number of error
 *			messages retrieved.
 *
 * @return		BASE_SUCCESS when successful.
 */
bstatus_t bssl_cert_get_verify_status_strings(
						 buint32_t verify_status, 
						 const char *error_strings[],
						 unsigned *count);

/** 
 * Wipe out the keys in the SSL certificate. 
 *
 * @param cert		The SSL certificate. 
 *
 */
void bssl_cert_wipe_keys(bssl_cert_t *cert);


/** 
 * Cipher suites enumeration.
 */
typedef enum bssl_cipher {

    /* Unsupported cipher */
    BASE_TLS_UNKNOWN_CIPHER                       = -1,

    /* NULL */
    BASE_TLS_NULL_WITH_NULL_NULL               	= 0x00000000,

    /* TLS/SSLv3 */
    BASE_TLS_RSA_WITH_NULL_MD5                 	= 0x00000001,
    BASE_TLS_RSA_WITH_NULL_SHA                 	= 0x00000002,
    BASE_TLS_RSA_WITH_NULL_SHA256              	= 0x0000003B,
    BASE_TLS_RSA_WITH_RC4_128_MD5              	= 0x00000004,
    BASE_TLS_RSA_WITH_RC4_128_SHA              	= 0x00000005,
    BASE_TLS_RSA_WITH_3DES_EDE_CBC_SHA         	= 0x0000000A,
    BASE_TLS_RSA_WITH_AES_128_CBC_SHA          	= 0x0000002F,
    BASE_TLS_RSA_WITH_AES_256_CBC_SHA          	= 0x00000035,
    BASE_TLS_RSA_WITH_AES_128_CBC_SHA256       	= 0x0000003C,
    BASE_TLS_RSA_WITH_AES_256_CBC_SHA256       	= 0x0000003D,
    BASE_TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA      	= 0x0000000D,
    BASE_TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA      	= 0x00000010,
    BASE_TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA     	= 0x00000013,
    BASE_TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA     	= 0x00000016,
    BASE_TLS_DH_DSS_WITH_AES_128_CBC_SHA       	= 0x00000030,
    BASE_TLS_DH_RSA_WITH_AES_128_CBC_SHA       	= 0x00000031,
    BASE_TLS_DHE_DSS_WITH_AES_128_CBC_SHA      	= 0x00000032,
    BASE_TLS_DHE_RSA_WITH_AES_128_CBC_SHA      	= 0x00000033,
    BASE_TLS_DH_DSS_WITH_AES_256_CBC_SHA       	= 0x00000036,
    BASE_TLS_DH_RSA_WITH_AES_256_CBC_SHA       	= 0x00000037,
    BASE_TLS_DHE_DSS_WITH_AES_256_CBC_SHA      	= 0x00000038,
    BASE_TLS_DHE_RSA_WITH_AES_256_CBC_SHA      	= 0x00000039,
    BASE_TLS_DH_DSS_WITH_AES_128_CBC_SHA256    	= 0x0000003E,
    BASE_TLS_DH_RSA_WITH_AES_128_CBC_SHA256    	= 0x0000003F,
    BASE_TLS_DHE_DSS_WITH_AES_128_CBC_SHA256   	= 0x00000040,
    BASE_TLS_DHE_RSA_WITH_AES_128_CBC_SHA256   	= 0x00000067,
    BASE_TLS_DH_DSS_WITH_AES_256_CBC_SHA256    	= 0x00000068,
    BASE_TLS_DH_RSA_WITH_AES_256_CBC_SHA256    	= 0x00000069,
    BASE_TLS_DHE_DSS_WITH_AES_256_CBC_SHA256   	= 0x0000006A,
    BASE_TLS_DHE_RSA_WITH_AES_256_CBC_SHA256   	= 0x0000006B,
    BASE_TLS_DH_anon_WITH_RC4_128_MD5          	= 0x00000018,
    BASE_TLS_DH_anon_WITH_3DES_EDE_CBC_SHA     	= 0x0000001B,
    BASE_TLS_DH_anon_WITH_AES_128_CBC_SHA      	= 0x00000034,
    BASE_TLS_DH_anon_WITH_AES_256_CBC_SHA      	= 0x0000003A,
    BASE_TLS_DH_anon_WITH_AES_128_CBC_SHA256   	= 0x0000006C,
    BASE_TLS_DH_anon_WITH_AES_256_CBC_SHA256   	= 0x0000006D,

    /* TLS (deprecated) */
    BASE_TLS_RSA_EXPORT_WITH_RC4_40_MD5        	= 0x00000003,
    BASE_TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5    	= 0x00000006,
    BASE_TLS_RSA_WITH_IDEA_CBC_SHA             	= 0x00000007,
    BASE_TLS_RSA_EXPORT_WITH_DES40_CBC_SHA     	= 0x00000008,
    BASE_TLS_RSA_WITH_DES_CBC_SHA              	= 0x00000009,
    BASE_TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA  	= 0x0000000B,
    BASE_TLS_DH_DSS_WITH_DES_CBC_SHA           	= 0x0000000C,
    BASE_TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA  	= 0x0000000E,
    BASE_TLS_DH_RSA_WITH_DES_CBC_SHA           	= 0x0000000F,
    BASE_TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA 	= 0x00000011,
    BASE_TLS_DHE_DSS_WITH_DES_CBC_SHA          	= 0x00000012,
    BASE_TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA 	= 0x00000014,
    BASE_TLS_DHE_RSA_WITH_DES_CBC_SHA          	= 0x00000015,
    BASE_TLS_DH_anon_EXPORT_WITH_RC4_40_MD5    	= 0x00000017,
    BASE_TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA 	= 0x00000019,
    BASE_TLS_DH_anon_WITH_DES_CBC_SHA          	= 0x0000001A,

    /* SSLv3 */
    BASE_SSL_FORTEZZA_KEA_WITH_NULL_SHA        	= 0x0000001C,
    BASE_SSL_FORTEZZA_KEA_WITH_FORTEZZA_CBC_SHA	= 0x0000001D,
    BASE_SSL_FORTEZZA_KEA_WITH_RC4_128_SHA     	= 0x0000001E,
    
    /* SSLv2 */
    BASE_SSL_CK_RC4_128_WITH_MD5               	= 0x00010080,
    BASE_SSL_CK_RC4_128_EXPORT40_WITH_MD5      	= 0x00020080,
    BASE_SSL_CK_RC2_128_CBC_WITH_MD5           	= 0x00030080,
    BASE_SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5  	= 0x00040080,
    BASE_SSL_CK_IDEA_128_CBC_WITH_MD5          	= 0x00050080,
    BASE_SSL_CK_DES_64_CBC_WITH_MD5            	= 0x00060040,
    BASE_SSL_CK_DES_192_EDE3_CBC_WITH_MD5      	= 0x000700C0

} bssl_cipher;


/**
 * Get cipher list supported by SSL/TLS backend.
 *
 * @param ciphers	The ciphers buffer to receive cipher list.
 * @param cipher_num	Maximum number of ciphers to be received.
 *
 * @return		BASE_SUCCESS when successful.
 */
bstatus_t bssl_cipher_get_availables(bssl_cipher ciphers[],
					          unsigned *cipher_num);


/**
 * Check if the specified cipher is supported by SSL/TLS backend.
 *
 * @param cipher	The cipher.
 *
 * @return		BASE_TRUE when supported.
 */
bbool_t bssl_cipher_is_supported(bssl_cipher cipher);


/**
 * Get cipher name string.
 *
 * @param cipher	The cipher.
 *
 * @return		The cipher name or NULL if cipher is not recognized/
 *			supported.
 */
const char* bssl_cipher_name(bssl_cipher cipher);


/**
 * Get cipher ID from cipher name string. Note that on different backends
 * (e.g. OpenSSL or Symbian implementation), cipher names may not be
 * equivalent for the same cipher ID.
 *
 * @param cipher_name	The cipher name string.
 *
 * @return		The cipher ID or BASE_TLS_UNKNOWN_CIPHER if the cipher
 *			name string is not recognized/supported.
 */
bssl_cipher bssl_cipher_id(const char *cipher_name);

/**
 * Elliptic curves enumeration.
 */
typedef enum bssl_curve
{
	BASE_TLS_UNKNOWN_CURVE 		= 0,
	BASE_TLS_CURVE_SECT163K1		= 1,
	BASE_TLS_CURVE_SECT163R1		= 2,
	BASE_TLS_CURVE_SECT163R2		= 3,
	BASE_TLS_CURVE_SECT193R1		= 4,
	BASE_TLS_CURVE_SECT193R2		= 5,
	BASE_TLS_CURVE_SECT233K1		= 6,
	BASE_TLS_CURVE_SECT233R1		= 7,
	BASE_TLS_CURVE_SECT239K1		= 8,
	BASE_TLS_CURVE_SECT283K1		= 9,
	BASE_TLS_CURVE_SECT283R1		= 10,
	BASE_TLS_CURVE_SECT409K1		= 11,
	BASE_TLS_CURVE_SECT409R1		= 12,
	BASE_TLS_CURVE_SECT571K1		= 13,
	BASE_TLS_CURVE_SECT571R1		= 14,
	BASE_TLS_CURVE_SECP160K1		= 15,
	BASE_TLS_CURVE_SECP160R1		= 16,
	BASE_TLS_CURVE_SECP160R2		= 17,
	BASE_TLS_CURVE_SECP192K1		= 18,
	BASE_TLS_CURVE_SECP192R1		= 19,
	BASE_TLS_CURVE_SECP224K1		= 20,
	BASE_TLS_CURVE_SECP224R1		= 21,
	BASE_TLS_CURVE_SECP256K1		= 22,
	BASE_TLS_CURVE_SECP256R1		= 23,
	BASE_TLS_CURVE_SECP384R1		= 24,
	BASE_TLS_CURVE_SECP521R1		= 25,
	BASE_TLS_CURVE_BRAINPOOLP256R1	= 26,
	BASE_TLS_CURVE_BRAINPOOLP384R1	= 27,
	BASE_TLS_CURVE_BRAINPOOLP512R1	= 28,
	BASE_TLS_CURVE_ARBITRARY_EXPLICIT_PRIME_CURVES	= 0XFF01,
	BASE_TLS_CURVE_ARBITRARY_EXPLICIT_CHAR2_CURVES	= 0XFF02
} bssl_curve;

/**
 * Get curve list supported by SSL/TLS backend.
 *
 * @param curves	The curves buffer to receive curve list.
 * @param curves_num	Maximum number of curves to be received.
 *
 * @return		BASE_SUCCESS when successful.
 */
bstatus_t bssl_curve_get_availables(bssl_curve curves[],
					         unsigned *curve_num);

/**
 * Check if the specified curve is supported by SSL/TLS backend.
 *
 * @param curve		The curve.
 *
 * @return		BASE_TRUE when supported.
 */
bbool_t bssl_curve_is_supported(bssl_curve curve);


/**
 * Get curve name string.
 *
 * @param curve		The curve.
 *
 * @return		The curve name or NULL if curve is not recognized/
 *			supported.
 */
const char* bssl_curve_name(bssl_curve curve);

/**
 * Get curve ID from curve name string. Note that on different backends
 * (e.g. OpenSSL or Symbian implementation), curve names may not be
 * equivalent for the same curve ID.
 *
 * @param curve_name	The curve name string.
 *
 * @return		The curve ID or BASE_TLS_UNKNOWN_CURVE if the curve
 *			name string is not recognized/supported.
 */
bssl_curve bssl_curve_id(const char *curve_name);

/*
 * Entropy enumeration
 */
typedef enum bssl_entropy
{
	BASE_SSL_ENTROPY_NONE	= 0,
	BASE_SSL_ENTROPY_EGD	= 1,
	BASE_SSL_ENTROPY_RANDOM	= 2,
	BASE_SSL_ENTROPY_URANDOM	= 3,
	BASE_SSL_ENTROPY_FILE	= 4,
	BASE_SSL_ENTROPY_UNKNOWN	= 0x0F
} bssl_entropy_t;

/**
 * This structure contains the callbacks to be called by the secure socket.
 */
typedef struct bssl_sock_cb
{
    /**
     * This callback is called when a data arrives as the result of
     * bssl_sock_start_read().
     *
     * @param ssock	The secure socket.
     * @param data	The buffer containing the new data, if any. If 
     *			the status argument is non-BASE_SUCCESS, this 
     *			argument may be NULL.
     * @param size	The length of data in the buffer.
     * @param status	The status of the read operation. This may contain
     *			non-BASE_SUCCESS for example when the TCP connection
     *			has been closed. In this case, the buffer may
     *			contain left over data from previous callback which
     *			the application may want to process.
     * @param remainder	If application wishes to leave some data in the 
     *			buffer (common for TCP applications), it should 
     *			move the remainder data to the front part of the 
     *			buffer and set the remainder length here. The value
     *			of this parameter will be ignored for datagram
     *			sockets.
     *
     * @return		BASE_TRUE if further read is desired, and BASE_FALSE 
     *			when application no longer wants to receive data.
     *			Application may destroy the secure socket in the
     *			callback and return BASE_FALSE here.
     */
    bbool_t (*on_data_read)(bssl_sock_t *ssock,
			      void *data,
			      bsize_t size,
			      bstatus_t status,
			      bsize_t *remainder);
    /**
     * This callback is called when a packet arrives as the result of
     * bssl_sock_start_recvfrom().
     *
     * @param ssock	The secure socket.
     * @param data	The buffer containing the packet, if any. If 
     *			the status argument is non-BASE_SUCCESS, this 
     *			argument will be set to NULL.
     * @param size	The length of packet in the buffer. If 
     *			the status argument is non-BASE_SUCCESS, this 
     *			argument will be set to zero.
     * @param src_addr	Source address of the packet.
     * @param addr_len	Length of the source address.
     * @param status	This contains
     *
     * @return		BASE_TRUE if further read is desired, and BASE_FALSE 
     *			when application no longer wants to receive data.
     *			Application may destroy the secure socket in the
     *			callback and return BASE_FALSE here.
     */
    bbool_t (*on_data_recvfrom)(bssl_sock_t *ssock,
				  void *data,
				  bsize_t size,
				  const bsockaddr_t *src_addr,
				  int addr_len,
				  bstatus_t status);

    /**
     * This callback is called when data has been sent.
     *
     * @param ssock	The secure socket.
     * @param send_key	Key associated with the send operation.
     * @param sent	If value is positive non-zero it indicates the
     *			number of data sent. When the value is negative,
     *			it contains the error code which can be retrieved
     *			by negating the value (i.e. status=-sent).
     *
     * @return		Application may destroy the secure socket in the
     *			callback and return BASE_FALSE here.
     */
    bbool_t (*on_data_sent)(bssl_sock_t *ssock,
			      bioqueue_op_key_t *send_key,
			      bssize_t sent);

    /**
     * This callback is called when new connection arrives as the result
     * of bssl_sock_start_accept(). If the status of accept operation is
     * needed use on_accept_complete2 instead of this callback.
     *
     * @param ssock	The secure socket.
     * @param newsock	The new incoming secure socket.
     * @param src_addr	The source address of the connection.
     * @param addr_len	Length of the source address.
     *
     * @return		BASE_TRUE if further accept() is desired, and BASE_FALSE
     *			when application no longer wants to accept incoming
     *			connection. Application may destroy the secure socket
     *			in the callback and return BASE_FALSE here.
     */
    bbool_t (*on_accept_complete)(bssl_sock_t *ssock,
				    bssl_sock_t *newsock,
				    const bsockaddr_t *src_addr,
				    int src_addr_len);
    /**
     * This callback is called when new connection arrives as the result
     * of bssl_sock_start_accept().
     *
     * @param asock	The active socket.
     * @param newsock	The new incoming socket.
     * @param src_addr	The source address of the connection.
     * @param addr_len	Length of the source address.
     * @param status	The status of the accept operation. This may contain
     *			non-BASE_SUCCESS for example when the TCP listener is in
     *			bad state for example on iOS platform after the
     *			application waking up from background.
     *
     * @return		BASE_TRUE if further accept() is desired, and BASE_FALSE
     *			when application no longer wants to accept incoming
     *			connection. Application may destroy the active socket
     *			in the callback and return BASE_FALSE here.
     */
    bbool_t (*on_accept_complete2)(bssl_sock_t *ssock,
				     bssl_sock_t *newsock,
				     const bsockaddr_t *src_addr,
				     int src_addr_len, 
				     bstatus_t status);

    /**
     * This callback is called when pending connect operation has been
     * completed.
     *
     * @param ssock	The secure socket.
     * @param status	The connection result. If connection has been
     *			successfully established, the status will contain
     *			BASE_SUCCESS.
     *
     * @return		Application may destroy the secure socket in the
     *			callback and return BASE_FALSE here. 
     */
    bbool_t (*on_connect_complete)(bssl_sock_t *ssock,
				     bstatus_t status);

} bssl_sock_cb;


/** 
 * Enumeration of secure socket protocol types.
 * This can be combined using bitwise OR operation.
 */
typedef enum bssl_sock_proto
{
    /**
     * Default protocol of backend. 
     */   
    BASE_SSL_SOCK_PROTO_DEFAULT = 0,

    /** 
     * SSLv2.0 protocol.	  
     */
    BASE_SSL_SOCK_PROTO_SSL2    = (1 << 0),

    /** 
     * SSLv3.0 protocol.	  
     */
    BASE_SSL_SOCK_PROTO_SSL3    = (1 << 1),

    /**
     * TLSv1.0 protocol.	  
     */
    BASE_SSL_SOCK_PROTO_TLS1    = (1 << 2),

    /** 
     * TLSv1.1 protocol.
     */
    BASE_SSL_SOCK_PROTO_TLS1_1  = (1 << 3),

    /**
     * TLSv1.2 protocol.
     */
    BASE_SSL_SOCK_PROTO_TLS1_2  = (1 << 4),

    /** 
     * Certain backend implementation e.g:OpenSSL, has feature to enable all
     * protocol. 
     */
    BASE_SSL_SOCK_PROTO_SSL23   = (1 << 16) - 1,
    BASE_SSL_SOCK_PROTO_ALL = BASE_SSL_SOCK_PROTO_SSL23,

    /**
     * DTLSv1.0 protocol.	  
     */
    BASE_SSL_SOCK_PROTO_DTLS1   = (1 << 16),

} bssl_sock_proto;


/**
 * Definition of secure socket info structure.
 */
typedef struct bssl_sock_info 
{
    /**
     * Describes whether secure socket connection is established, i.e: TLS/SSL 
     * handshaking has been done successfully.
     */
    bbool_t established;

    /**
     * Describes secure socket protocol being used, see #bssl_sock_proto. 
     * Use bitwise OR operation to combine the protocol type.
     */
    buint32_t proto;

    /**
     * Describes cipher suite being used, this will only be set when connection
     * is established.
     */
    bssl_cipher cipher;

    /**
     * Describes local address.
     */
    bsockaddr local_addr;

    /**
     * Describes remote address.
     */
    bsockaddr remote_addr;
   
    /**
     * Describes active local certificate info.
     */
    bssl_cert_info *local_cert_info;
   
    /**
     * Describes active remote certificate info.
     */
    bssl_cert_info *remote_cert_info;

    /**
     * Status of peer certificate verification.
     */
    buint32_t		verify_status;

    /**
     * Last native error returned by the backend.
     */
    unsigned long	last_native_err;

    /**
     * Group lock assigned to the ioqueue key.
     */
    bgrp_lock_t *grp_lock;

} bssl_sock_info;


/**
 * Definition of secure socket creation parameters.
 */
typedef struct bssl_sock_param
{
    /**
     * Optional group lock to be assigned to the ioqueue key.
     *
     * Note that when a secure socket listener is configured with a group
     * lock, any new secure socket of an accepted incoming connection
     * will have its own group lock created automatically by the library,
     * this group lock can be queried via bssl_sock_get_info() in the info
     * field bssl_sock_info::grp_lock.
     */
    bgrp_lock_t *grp_lock;

    /**
     * Specifies socket address family, either bAF_INET() and bAF_INET6().
     *
     * Default is bAF_INET().
     */
    int sock_af;

    /**
     * Specify socket type, either bSOCK_DGRAM() or bSOCK_STREAM().
     *
     * Default is bSOCK_STREAM().
     */
    int sock_type;

    /**
     * Specify the ioqueue to use. Secure socket uses the ioqueue to perform
     * active socket operations, see \ref BASE_ACTIVESOCK for more detail.
     */
    bioqueue_t *ioqueue;

    /**
     * Specify the timer heap to use. Secure socket uses the timer to provide
     * auto cancelation on asynchronous operation when it takes longer time 
     * than specified timeout period, e.g: security negotiation timeout.
     */
    btimer_heap_t *timer_heap;

    /**
     * Specify secure socket callbacks, see #bssl_sock_cb.
     */
    bssl_sock_cb cb;

    /**
     * Specify secure socket user data.
     */
    void *user_data;

    /**
     * Specify security protocol to use, see #bssl_sock_proto. Use bitwise OR 
     * operation to combine the protocol type.
     *
     * Default is BASE_SSL_SOCK_PROTO_DEFAULT.
     */
    buint32_t proto;

    /**
     * Number of concurrent asynchronous operations that is to be supported
     * by the secure socket. This value only affects socket receive and
     * accept operations -- the secure socket will issue one or more 
     * asynchronous read and accept operations based on the value of this
     * field. Setting this field to more than one will allow more than one
     * incoming data or incoming connections to be processed simultaneously
     * on multiprocessor systems, when the ioqueue is polled by more than
     * one threads.
     *
     * The default value is 1.
     */
    unsigned async_cnt;

    /**
     * The ioqueue concurrency to be forced on the socket when it is 
     * registered to the ioqueue. See #bioqueue_set_concurrency() for more
     * info about ioqueue concurrency.
     *
     * When this value is -1, the concurrency setting will not be forced for
     * this socket, and the socket will inherit the concurrency setting of 
     * the ioqueue. When this value is zero, the secure socket will disable
     * concurrency for the socket. When this value is +1, the secure socket
     * will enable concurrency for the socket.
     *
     * The default value is -1.
     */
    int concurrency;

    /**
     * If this option is specified, the secure socket will make sure that
     * asynchronous send operation with stream oriented socket will only
     * call the callback after all data has been sent. This means that the
     * secure socket will automatically resend the remaining data until
     * all data has been sent.
     *
     * Please note that when this option is specified, it is possible that
     * error is reported after partial data has been sent. Also setting
     * this will disable the ioqueue concurrency for the socket.
     *
     * Default value is 1.
     */
    bbool_t whole_data;

    /**
     * Specify buffer size for sending operation. Buffering sending data
     * is used for allowing application to perform multiple outstanding 
     * send operations. Whenever application specifies this setting too
     * small, sending operation may return BASE_ENOMEM.
     *  
     * Default value is 8192 bytes.
     */
    bsize_t send_buffer_size;

    /**
     * Specify buffer size for receiving encrypted (and perhaps compressed)
     * data on underlying socket. This setting is unused on Symbian, since 
     * SSL/TLS Symbian backend, CSecureSocket, can use application buffer 
     * directly.
     *
     * Default value is 1500.
     */
    bsize_t read_buffer_size;

    /**
     * Number of ciphers contained in the specified cipher preference. 
     * If this is set to zero, then the cipher list used will be determined
     * by the backend default (for OpenSSL backend, setting 
     * BASE_SSL_SOCK_OSSL_CIPHERS will be used).
     */
    unsigned ciphers_num;

    /**
     * Ciphers and order preference. If empty, then default cipher list and
     * its default order of the backend will be used.
     */
    bssl_cipher *ciphers;

    /**
     * Number of curves contained in the specified curve preference.
     * If this is set to zero, then default curve list of the backend
     * will be used.
     *
     * Default: 0 (zero).
     */
    unsigned curves_num;

    /**
     * Curves and order preference. The #bssl_curve_get_availables()
     * can be used to check the available curves supported by backend.
     */
    bssl_curve *curves;

    /**
     * The supported signature algorithms. Set the sigalgs string
     * using this form:
     * "<DIGEST>+<ALGORITHM>:<DIGEST>+<ALGORITHM>"
     * Digests are: "RSA", "DSA" or "ECDSA"
     * Algorithms are: "MD5", "SHA1", "SHA224", "SHA256", "SHA384", "SHA512"
     * Example: "ECDSA+SHA256:RSA+SHA256"
     */
    bstr_t	sigalgs;

    /**
     * Reseed random number generator.
     * For type #BASE_SSL_ENTROPY_FILE, parameter \a entropy_path
     * must be set to a file.
     * For type #BASE_SSL_ENTROPY_EGD, parameter \a entropy_path
     * must be set to a socket.
     *
     * Default value is BASE_SSL_ENTROPY_NONE.
    */
    bssl_entropy_t	entropy_type;

    /**
     * When using a file/socket for entropy #BASE_SSL_ENTROPY_EGD or
     * #BASE_SSL_ENTROPY_FILE, \a entropy_path must contain the path
     * to entropy socket/file.
     *
     * Default value is an empty string.
     */
    bstr_t		entropy_path;

    /**
     * Security negotiation timeout. If this is set to zero (both sec and 
     * msec), the negotiation doesn't have a timeout.
     *
     * Default value is zero.
     */
    btime_val	timeout;

    /**
     * Specify whether endpoint should verify peer certificate.
     *
     * Default value is BASE_FALSE.
     */
    bbool_t verify_peer;
    
    /**
     * When secure socket is acting as server (handles incoming connection),
     * it will require the client to provide certificate.
     *
     * Default value is BASE_FALSE.
     */
    bbool_t require_client_cert;

    /**
     * Server name indication. When secure socket is acting as client 
     * (perform outgoing connection) and the server may host multiple
     * 'virtual' servers at a single underlying network address, setting
     * this will allow client to tell the server a name of the server
     * it is contacting.
     *
     * Default value is zero/not-set.
     */
    bstr_t server_name;

    /**
     * Specify if SO_REUSEADDR should be used for listening socket. This
     * option will only be used with accept() operation.
     *
     * Default is BASE_FALSE.
     */
    bbool_t reuse_addr;

    /**
     * QoS traffic type to be set on this transport. When application wants
     * to apply QoS tagging to the transport, it's preferable to set this
     * field rather than \a qos_param fields since this is more portable.
     *
     * Default value is BASE_QOS_TYPE_BEST_EFFORT.
     */
    bqos_type qos_type;

    /**
     * Set the low level QoS parameters to the transport. This is a lower
     * level operation than setting the \a qos_type field and may not be
     * supported on all platforms.
     *
     * By default all settings in this structure are disabled.
     */
    bqos_params qos_params;

    /**
     * Specify if the transport should ignore any errors when setting the QoS
     * traffic type/parameters.
     *
     * Default: BASE_TRUE
     */
    bbool_t qos_ignore_error;

    /**
     * Specify options to be set on the transport. 
     *
     * By default there is no options.
     * 
     */
    bsockopt_params sockopt_params;

    /**
     * Specify if the transport should ignore any errors when setting the 
     * sockopt parameters.
     *
     * Default: BASE_TRUE
     * 
     */
    bbool_t sockopt_ignore_error;

} bssl_sock_param;


/**
 * The parameter for bssl_sock_start_connect2().
 */
typedef struct bssl_start_connect_param {
    /**
     * The pool to allocate some internal data for the operation.
     */
    bpool_t *pool;

    /**
     * Local address.
     */
    const bsockaddr_t *localaddr;

    /**
     * Port range for socket binding, relative to the start port number
     * specified in \a localaddr. This is only applicable when the start port
     * number is non zero.
     */
    buint16_t local_port_range;

    /**
     * Remote address.
     */
    const bsockaddr_t *remaddr;

    /**
     * Length of buffer containing above addresses.
     */
    int addr_len;

} bssl_start_connect_param;


/**
 * Initialize the secure socket parameters for its creation with 
 * the default values.
 *
 * @param param		The parameter to be initialized.
 */
void bssl_sock_param_default(bssl_sock_param *param);


/**
 * Duplicate bssl_sock_param.
 *
 * @param pool	Pool to allocate memory.
 * @param dst	Destination parameter.
 * @param src	Source parameter.
 */
void bssl_sock_param_copy(bpool_t *pool, 
				     bssl_sock_param *dst,
				     const bssl_sock_param *src);


/**
 * Create secure socket instance.
 *
 * @param pool		The pool for allocating secure socket instance.
 * @param param		The secure socket parameter, see #bssl_sock_param.
 * @param p_ssock	Pointer to secure socket instance to be created.
 *
 * @return		BASE_SUCCESS when successful.
 */
bstatus_t bssl_sock_create(bpool_t *pool,
					const bssl_sock_param *param,
					bssl_sock_t **p_ssock);


/**
 * Set secure socket certificate or credentials. Credentials may include 
 * certificate, private key and trusted Certification Authorities list. 
 * Normally, server socket must provide certificate (and private key).
 * Socket client may also need to provide certificate in case requested
 * by the server.
 *
 * @param ssock		The secure socket instance.
 * @param pool		The pool.
 * @param cert		The endpoint certificate/credentials, see
 *			#bssl_cert_t.
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t bssl_sock_set_certificate(
					    bssl_sock_t *ssock,
					    bpool_t *pool,
					    const bssl_cert_t *cert);


/**
 * Close and destroy the secure socket.
 *
 * @param ssock		The secure socket.
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t bssl_sock_close(bssl_sock_t *ssock);


/**
 * Associate arbitrary data with the secure socket. Application may
 * inspect this data in the callbacks and associate it with higher
 * level processing.
 *
 * @param ssock		The secure socket.
 * @param user_data	The user data to be associated with the secure
 *			socket.
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t bssl_sock_set_user_data(bssl_sock_t *ssock,
					       void *user_data);

/**
 * Retrieve the user data previously associated with this secure
 * socket.
 *
 * @param ssock		The secure socket.
 *
 * @return		The user data.
 */
void* bssl_sock_get_user_data(bssl_sock_t *ssock);


/**
 * Retrieve the local address and port used by specified secure socket.
 *
 * @param ssock		The secure socket.
 * @param info		The info buffer to be set, see #bssl_sock_info.
 *
 * @return		BASE_SUCCESS on successful.
 */
bstatus_t bssl_sock_get_info(bssl_sock_t *ssock,
					  bssl_sock_info *info);


/**
 * Starts read operation on this secure socket. This function will create
 * \a async_cnt number of buffers (the \a async_cnt parameter was given
 * in \a bssl_sock_create() function) where each buffer is \a buff_size
 * long. The buffers are allocated from the specified \a pool. Once the 
 * buffers are created, it then issues \a async_cnt number of asynchronous
 * \a recv() operations to the socket and returns back to caller. Incoming
 * data on the socket will be reported back to application via the 
 * \a on_data_read() callback.
 *
 * Application only needs to call this function once to initiate read
 * operations. Further read operations will be done automatically by the
 * secure socket when \a on_data_read() callback returns non-zero. 
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param flags		Flags to be given to bioqueue_recv().
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t bssl_sock_start_read(bssl_sock_t *ssock,
					    bpool_t *pool,
					    unsigned buff_size,
					    buint32_t flags);

/**
 * Same as #bssl_sock_start_read(), except that the application
 * supplies the buffers for the read operation so that the acive socket
 * does not have to allocate the buffers.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param readbuf	Array of packet buffers, each has buff_size size.
 * @param flags		Flags to be given to bioqueue_recv().
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t bssl_sock_start_read2(bssl_sock_t *ssock,
					     bpool_t *pool,
					     unsigned buff_size,
					     void *readbuf[],
					     buint32_t flags);

/**
 * Same as bssl_sock_start_read(), except that this function is used
 * only for datagram sockets, and it will trigger \a on_data_recvfrom()
 * callback instead.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param flags		Flags to be given to bioqueue_recvfrom().
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t bssl_sock_start_recvfrom(bssl_sock_t *ssock,
						bpool_t *pool,
						unsigned buff_size,
						buint32_t flags);

/**
 * Same as #bssl_sock_start_recvfrom() except that the recvfrom() 
 * operation takes the buffer from the argument rather than creating
 * new ones.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param readbuf	Array of packet buffers, each has buff_size size.
 * @param flags		Flags to be given to bioqueue_recvfrom().
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t bssl_sock_start_recvfrom2(bssl_sock_t *ssock,
						 bpool_t *pool,
						 unsigned buff_size,
						 void *readbuf[],
						 buint32_t flags);

/**
 * Send data using the socket.
 *
 * @param ssock		The secure socket.
 * @param send_key	The operation key to send the data, which is useful
 *			if application wants to submit multiple pending
 *			send operations and want to track which exact data 
 *			has been sent in the \a on_data_sent() callback.
 * @param data		The data to be sent. This data must remain valid
 *			until the data has been sent.
 * @param size		The size of the data.
 * @param flags		Flags to be given to bioqueue_send().
 *
 * @return		BASE_SUCCESS if data has been sent immediately, or
 *			BASE_EPENDING if data cannot be sent immediately or
 *			BASE_ENOMEM when sending buffer could not handle all
 *			queued data, see \a send_buffer_size. The callback
 *			\a on_data_sent() will be called when data is actually
 *			sent. Any other return value indicates error condition.
 */
bstatus_t bssl_sock_send(bssl_sock_t *ssock,
				      bioqueue_op_key_t *send_key,
				      const void *data,
				      bssize_t *size,
				      unsigned flags);

/**
 * Send datagram using the socket.
 *
 * @param ssock		The secure socket.
 * @param send_key	The operation key to send the data, which is useful
 *			if application wants to submit multiple pending
 *			send operations and want to track which exact data 
 *			has been sent in the \a on_data_sent() callback.
 * @param data		The data to be sent. This data must remain valid
 *			until the data has been sent.
 * @param size		The size of the data.
 * @param flags		Flags to be given to bioqueue_send().
 * @param addr		The destination address.
 * @param addr_len	Length of buffer containing destination address.
 *
 * @return		BASE_SUCCESS if data has been sent immediately, or
 *			BASE_EPENDING if data cannot be sent immediately. In
 *			this case the \a on_data_sent() callback will be
 *			called when data is actually sent. Any other return
 *			value indicates error condition.
 */
bstatus_t bssl_sock_sendto(bssl_sock_t *ssock,
					bioqueue_op_key_t *send_key,
					const void *data,
					bssize_t *size,
					unsigned flags,
					const bsockaddr_t *addr,
					int addr_len);


/**
 * Starts asynchronous socket accept() operations on this secure socket. 
 * This function will issue \a async_cnt number of asynchronous \a accept() 
 * operations to the socket and returns back to caller. Incoming
 * connection on the socket will be reported back to application via the
 * \a on_accept_complete() callback.
 *
 * Application only needs to call this function once to initiate accept()
 * operations. Further accept() operations will be done automatically by 
 * the secure socket when \a on_accept_complete() callback returns non-zero.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate some internal data for the
 *			operation.
 * @param localaddr	Local address to bind on.
 * @param addr_len	Length of buffer containing local address.
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t bssl_sock_start_accept(bssl_sock_t *ssock,
					      bpool_t *pool,
					      const bsockaddr_t *local_addr,
					      int addr_len);


/**
 * Same as #bssl_sock_start_accept(), but application can provide
 * a secure socket parameter, which will be used to create a new secure
 * socket reported in \a on_accept_complete() callback when there is
 * an incoming connection.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate some internal data for the
 *			operation.
 * @param localaddr	Local address to bind on.
 * @param addr_len	Length of buffer containing local address.
 * @param newsock_param	Secure socket parameter for new accepted sockets.
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t
bssl_sock_start_accept2(bssl_sock_t *ssock,
			  bpool_t *pool,
			  const bsockaddr_t *local_addr,
			  int addr_len,
			  const bssl_sock_param *newsock_param);


/**
 * Starts asynchronous socket connect() operation and SSL/TLS handshaking 
 * for this socket. Once the connection is done (either successfully or not),
 * the \a on_connect_complete() callback will be called.
 *
 * @param ssock		The secure socket.
 * @param pool		The pool to allocate some internal data for the
 *			operation.
 * @param localaddr	Local address.
 * @param remaddr	Remote address.
 * @param addr_len	Length of buffer containing above addresses.
 *
 * @return		BASE_SUCCESS if connection can be established immediately
 *			or BASE_EPENDING if connection cannot be established 
 *			immediately. In this case the \a on_connect_complete()
 *			callback will be called when connection is complete. 
 *			Any other return value indicates error condition.
 */
bstatus_t bssl_sock_start_connect(bssl_sock_t *ssock,
					       bpool_t *pool,
					       const bsockaddr_t *localaddr,
					       const bsockaddr_t *remaddr,
					       int addr_len);

/**
 * Same as #bssl_sock_start_connect(), but application can provide a 
 * \a port_range parameter, which will be used to bind the socket to 
 * random port.
 *
 * @param ssock		The secure socket.
 *
 * @param connect_param The parameter, refer to \a bssl_start_connect_param.
 *
 * @return		BASE_SUCCESS if connection can be established immediately
 *			or BASE_EPENDING if connection cannot be established 
 *			immediately. In this case the \a on_connect_complete()
 *			callback will be called when connection is complete. 
 *			Any other return value indicates error condition.
 */
bstatus_t bssl_sock_start_connect2(
			      bssl_sock_t *ssock,
			      bssl_start_connect_param *connect_param);

/**
 * Starts SSL/TLS renegotiation over an already established SSL connection
 * for this socket. This operation is performed transparently, no callback 
 * will be called once the renegotiation completed successfully. However, 
 * when the renegotiation fails, the connection will be closed and callback
 * \a on_data_read() will be invoked with non-BASE_SUCCESS status code.
 *
 * @param ssock		The secure socket.
 *
 * @return		BASE_SUCCESS if renegotiation is completed immediately,
 *			or BASE_EPENDING if renegotiation has been started and
 *			waiting for completion, or the appropriate error code 
 *			on failure.
 */
bstatus_t bssl_sock_renegotiate(bssl_sock_t *ssock);

BASE_END_DECL

#endif

