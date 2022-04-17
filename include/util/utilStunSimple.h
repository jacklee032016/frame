/* 
 *
 */
#ifndef __USTUN_SIMPLE_H__
#define __USTUN_SIMPLE_H__

/**
 * @brief STUN client.
 */

#include <utilTypes.h>
#include <baseSock.h>


BASE_BEGIN_DECL

/*
 * This enumeration describes STUN message types.
 */
typedef enum utilstun_msg_type
{
	USTUN_BINDING_REQUEST		    = 0x0001,
	USTUN_BINDING_RESPONSE		    = 0x0101,
	USTUN_BINDING_ERROR_RESPONSE	    = 0x0111,
	USTUN_SHARED_SECRET_REQUEST	    = 0x0002,
	USTUN_SHARED_SECRET_RESPONSE	    = 0x0102,
	USTUN_SHARED_SECRET_ERROR_RESPONSE    = 0x0112
} utilstun_msg_type;


/*
 * This enumeration describes STUN attribute types.
 */
typedef enum utilstun_attr_type
{
	USTUN_ATTR_MAPPED_ADDR = 1,
	USTUN_ATTR_RESPONSE_ADDR,
	USTUN_ATTR_CHANGE_REQUEST,
	USTUN_ATTR_SOURCE_ADDR,
	USTUN_ATTR_CHANGED_ADDR,
	USTUN_ATTR_USERNAME,
	USTUN_ATTR_PASSWORD,
	USTUN_ATTR_MESSAGE_INTEGRITY,
	USTUN_ATTR_ERROR_CODE,
	USTUN_ATTR_UNKNOWN_ATTRIBUTES,
	USTUN_ATTR_REFLECTED_FROM,
	USTUN_ATTR_XOR_MAPPED_ADDR = 0x0020
} utilstun_attr_type;


/*
 * This structre describes STUN message header.
 */
typedef struct _utilstun_msg_hdr
{
    buint16_t		type;
    buint16_t		length;
    buint32_t		tsx[4];
} utilstun_msg_hdr;


/*
 * This structre describes STUN attribute header.
 */
typedef struct _utilstun_attr_hdr
{
    buint16_t		type;
    buint16_t		length;
} utilstun_attr_hdr;


/*
 * This structre describes STUN MAPPED-ADDR attribute.
 */
typedef struct _utilstun_mapped_addr_attr
{
    utilstun_attr_hdr	hdr;
    buint8_t		ignored;
    buint8_t		family;
    buint16_t		port;
    buint32_t		addr;
} utilstun_mapped_addr_attr;

typedef utilstun_mapped_addr_attr utilstun_response_addr_attr;
typedef utilstun_mapped_addr_attr utilstun_changed_addr_attr;
typedef utilstun_mapped_addr_attr utilstun_src_addr_attr;
typedef utilstun_mapped_addr_attr utilstun_reflected_form_attr;

typedef struct _utilstun_change_request_attr
{
    utilstun_attr_hdr	hdr;
    buint32_t		value;
} utilstun_change_request_attr;

typedef struct _utilstun_username_attr
{
    utilstun_attr_hdr	hdr;
    buint32_t		value[1];
} utilstun_username_attr;

typedef utilstun_username_attr utilstun_password_attr;

typedef struct _utilstun_error_code_attr
{
    utilstun_attr_hdr	hdr;
    buint16_t		ignored;
    buint8_t		err_class;
    buint8_t		number;
    char		reason[4];
} utilstun_error_code_attr;

typedef struct _utilstun_msg
{
    utilstun_msg_hdr    *hdr;
    int			attr_count;
    utilstun_attr_hdr   *attr[USTUN_MAX_ATTR];
} utilstun_msg;

/* STUN message API (stun.c). */

bstatus_t utilstun_create_bind_req( bpool_t *pool, 
					      void **msg, bsize_t *len,
					      buint32_t id_hi,
					      buint32_t id_lo);
bstatus_t utilstun_parse_msg( void *buf, bsize_t len,
				        utilstun_msg *msg);
void* utilstun_msg_find_attr( utilstun_msg *msg, utilstun_attr_type t);


/**
 * @defgroup UTIL_STUN_CLIENT Simple STUN Helper
 * @ingroup BASE_PROTOCOLS
 * @brief A simple and small footprint STUN resolution helper
 * @{
 *
 * This is the older implementation of STUN client, with only one function
 * provided (utilstun_get_mapped_addr()) to retrieve the public IP address
 * of multiple sockets.
 */

/**
 * This is the main function to request the mapped address of local sockets
 * to multiple STUN servers. This function is able to find the mapped 
 * addresses of multiple sockets simultaneously, and for each socket, two
 * requests will be sent to two different STUN servers to see if both servers
 * get the same public address for the same socket. (Note that application can
 * specify the same address for the two servers, but still two requests will
 * be sent for each server).
 *
 * This function will perform necessary retransmissions of the requests if
 * response is not received within a predetermined period. When all responses
 * have been received, the function will compare the mapped addresses returned
 * by the servers, and when both are equal, the address will be returned in
 * \a mapped_addr argument.
 *
 * @param pf		The pool factory where memory will be allocated from.
 * @param sock_cnt	Number of sockets in the socket array.
 * @param sock		Array of local UDP sockets which public addresses are
 *			to be queried from the STUN servers.
 * @param srv1		Host name or IP address string of the first STUN
 *			server.
 * @param port1		The port number of the first STUN server. 
 * @param srv2		Host name or IP address string of the second STUN
 *			server.
 * @param port2		The port number of the second STUN server. 
 * @param mapped_addr	Array to receive the mapped public address of the local
 *			UDP sockets, when the function returns BASE_SUCCESS.
 *
 * @return		This functions returns BASE_SUCCESS if responses are
 *			received from all servers AND all servers returned the
 *			same mapped public address. Otherwise this function may
 *			return one of the following error codes:
 *			- UTIL_ESTUNNOTRESPOND: no respons from servers.
 *			- UTIL_ESTUNSYMMETRIC: different mapped addresses
 *			  are returned by servers.
 *			- etc.
 *
 */
bstatus_t utilstun_get_mapped_addr( bpool_factory *pf,
					      int sock_cnt, bsock_t sock[],
					      const bstr_t *srv1, int port1,
					      const bstr_t *srv2, int port2,
					      bsockaddr_in mapped_addr[]);


/*
 * This structre describes configurable setting for requesting mapped address.
 */
typedef struct _utilstun_setting
{
    /**
     * Specifies whether STUN request generated by old STUN library should
     * insert magic cookie (specified in RFC 5389) in the transaction ID.
     */
    bbool_t	use_stun2;
    
    /**
     * Address family of the STUN servers.
     */
    int af;

    /**
     * Host name or IP address string of the first STUN server.
     */
    bstr_t srv1;

    /**
     * The port number of the first STUN server.
     */
    int port1;

    /**
     * Host name or IP address string of the second STUN server.
     */
    bstr_t srv2;

    /**
     * The port number of the second STUN server.
     */
    int port2;

} utilstun_setting;


/**
 * Another version of mapped address resolution of local sockets to multiple
 * STUN servers configured in #utilstun_setting. This function is able to find
 * the mapped addresses of multiple sockets simultaneously, and for each
 * socket, two requests will be sent to two different STUN servers to see if
 * both servers get the same public address for the same socket. (Note that
 * application can specify the same address for the two servers, but still
 * two requests will be sent for each server).
 *
 * This function will perform necessary retransmissions of the requests if
 * response is not received within a predetermined period. When all responses
 * have been received, the function will compare the mapped addresses returned
 * by the servers, and when both are equal, the address will be returned in
 * \a mapped_addr argument.
 *
 * @param pf		The pool factory where memory will be allocated from.
 * @param opt		The STUN settings.
 * @param sock_cnt	Number of sockets in the socket array.
 * @param sock		Array of local UDP sockets which public addresses are
 *			to be queried from the STUN servers.
 * @param mapped_addr	Array to receive the mapped public address of the local
 *			UDP sockets, when the function returns BASE_SUCCESS.
 *
 * @return		This functions returns BASE_SUCCESS if responses are
 *			received from all servers AND all servers returned the
 *			same mapped public address. Otherwise this function may
 *			return one of the following error codes:
 *			- UTIL_ESTUNNOTRESPOND: no respons from servers.
 *			- UTIL_ESTUNSYMMETRIC: different mapped addresses
 *			  are returned by servers.
 *			- etc.
 *
 */
bstatus_t utilstun_get_mapped_addr2( bpool_factory *pf,
					      const utilstun_setting *opt,
					      int sock_cnt,
					      bsock_t sock[],
					      bsockaddr_in mapped_addr[]);


BASE_END_DECL

#endif


