/* 
 *
 */
#ifndef __UTIL_ERRNO_H__
#define __UTIL_ERRNO_H__


#include <baseErrno.h>

/**
 * @defgroup UTIL_ERROR Error Codes
 * @ingroup UTIL_BASE
 * @{
 */

/**
 * Start of error code relative to BASE_ERRNO_START_USER.
 * This value is 320000.
 */
#define UTIL_ERRNO_START    (BASE_ERRNO_START_USER + BASE_ERRNO_SPACE_SIZE*3)


/************************************************************
 * STUN ERROR
 ***********************************************************/
/**
 * @hideinitializer
 * Unable to resolve STUN server
 */
#define UTIL_ESTUNRESOLVE	    (UTIL_ERRNO_START+1)	/* 320001 */
/**
 * @hideinitializer
 * Unknown STUN message type.
 */
#define UTIL_ESTUNINMSGTYPE   (UTIL_ERRNO_START+2)	/* 320002 */
/**
 * @hideinitializer
 * Invalid STUN message length
 */
#define UTIL_ESTUNINMSGLEN    (UTIL_ERRNO_START+3)	/* 320003 */
/**
 * @hideinitializer
 * Invalid STUN attribute length
 */
#define UTIL_ESTUNINATTRLEN   (UTIL_ERRNO_START+4)	/* 320004 */
/**
 * @hideinitializer
 * Invalid STUN attribute type
 */
#define UTIL_ESTUNINATTRTYPE  (UTIL_ERRNO_START+5)	/* 320005 */
/**
 * @hideinitializer
 * Invalid STUN server/socket index
 */
#define UTIL_ESTUNININDEX     (UTIL_ERRNO_START+6)	/* 320006 */
/**
 * @hideinitializer
 * No STUN binding response in the message
 */
#define UTIL_ESTUNNOBINDRES   (UTIL_ERRNO_START+7)	/* 320007 */
/**
 * @hideinitializer
 * Received STUN error attribute
 */
#define UTIL_ESTUNRECVERRATTR (UTIL_ERRNO_START+8)	/* 320008 */
/**
 * @hideinitializer
 * No STUN mapped address attribute
 */
#define UTIL_ESTUNNOMAP       (UTIL_ERRNO_START+9)	/* 320009 */
/**
 * @hideinitializer
 * Received no response from STUN server
 */
#define UTIL_ESTUNNOTRESPOND  (UTIL_ERRNO_START+10)	/* 320010 */
/**
 * @hideinitializer
 * Symetric NAT detected by STUN
 */
#define UTIL_ESTUNSYMMETRIC   (UTIL_ERRNO_START+11)	/* 320011 */
/**
 * @hideinitializer
 * Invalid STUN magic value
 */
#define UTIL_ESTUNNOTMAGIC    (UTIL_ERRNO_START+12)	/* 320012 */
/**
 * @hideinitializer
 * Invalid STUN fingerprint value
 */
#define UTIL_ESTUNFINGERPRINT (UTIL_ERRNO_START+13)	/* 320013 */



/************************************************************
 * XML ERROR
 ***********************************************************/
/**
 * @hideinitializer
 * General invalid XML message.
 */
#define UTIL_EINXML	    (UTIL_ERRNO_START+20)	/* 320020 */


/************************************************************
 * JSON ERROR
 ***********************************************************/
/**
 * @hideinitializer
 * General invalid JSON message.
 */
#define UTIL_EINJSON	    (UTIL_ERRNO_START+30)	/* 320030 */


/************************************************************
 * DNS ERROR
 ***********************************************************/
/**
 * @hideinitializer
 * DNS query packet buffer is too small.
 * This error occurs when the user supplied buffer for creating DNS
 * query (#bdns_make_query() function) is too small.
 */
#define UTIL_EDNSQRYTOOSMALL  (UTIL_ERRNO_START+40)	/* 320040 */
/**
 * @hideinitializer
 * Invalid DNS packet length.
 * This error occurs when the received DNS response packet does not
 * match all the fields length.
 */
#define UTIL_EDNSINSIZE	    (UTIL_ERRNO_START+41)	/* 320041 */
/**
 * @hideinitializer
 * Invalid DNS class.
 * This error occurs when the received DNS response contains network
 * class other than IN (Internet).
 */
#define UTIL_EDNSINCLASS	    (UTIL_ERRNO_START+42)	/* 320042 */
/**
 * @hideinitializer
 * Invalid DNS name pointer.
 * This error occurs when parsing the compressed names inside DNS
 * response packet, when the name pointer points to an invalid address
 * or the parsing has triggerred too much recursion.
 */
#define UTIL_EDNSINNAMEPTR    (UTIL_ERRNO_START+43)	/* 320043 */
/**
 * @hideinitializer
 * Invalid DNS nameserver address. If hostname was specified for nameserver
 * address, this error means that the function was unable to resolve
 * the nameserver hostname.
 */
#define UTIL_EDNSINNSADDR	    (UTIL_ERRNO_START+44)	/* 320044 */
/**
 * @hideinitializer
 * No nameserver is in DNS resolver. No nameserver is configured in the 
 * resolver.
 */
#define UTIL_EDNSNONS	    (UTIL_ERRNO_START+45)	/* 320045 */
/**
 * @hideinitializer
 * No working DNS nameserver. All nameservers have been queried,
 * but none was able to serve any DNS requests. These "bad" nameservers
 * will be re-tested again for "goodness" after some period.
 */
#define UTIL_EDNSNOWORKINGNS  (UTIL_ERRNO_START+46)	/* 320046 */
/**
 * @hideinitializer
 * No answer record in the DNS response.
 */
#define UTIL_EDNSNOANSWERREC  (UTIL_ERRNO_START+47)	/* 320047 */
/**
 * @hideinitializer
 * Invalid DNS answer. This error is raised for example when the DNS
 * answer does not have a query section, or the type of RR in the answer
 * doesn't match the query.
 */
#define UTIL_EDNSINANSWER	    (UTIL_ERRNO_START+48)	/* 320048 */


/* DNS ERRORS MAPPED FROM RCODE: */

/**
 * Start of error code mapped from DNS RCODE
 */
#define UTIL_DNS_RCODE_START  (UTIL_ERRNO_START+50)	/* 320050 */

/**
 * Map DNS RCODE status into bstatus_t.
 */
#define BASE_STATUS_FROM_DNS_RCODE(rcode)	(rcode==0 ? BASE_SUCCESS : \
					 UTIL_DNS_RCODE_START+rcode)
/**
 * @hideinitializer
 * Format error - The name server was unable to interpret the query.
 * This corresponds to DNS RCODE 1.
 */
#define UTIL_EDNS_FORMERR	    BASE_STATUS_FROM_DNS_RCODE(1)	/* 320051 */
/**
 * @hideinitializer
 * Server failure - The name server was unable to process this query due to a
 * problem with the name server.
 * This corresponds to DNS RCODE 2.
 */
#define UTIL_EDNS_SERVFAIL    BASE_STATUS_FROM_DNS_RCODE(2)	/* 320052 */
/**
 * @hideinitializer
 * Name Error - Meaningful only for responses from an authoritative name
 * server, this code signifies that the domain name referenced in the query 
 * does not exist.
 * This corresponds to DNS RCODE 3.
 */
#define UTIL_EDNS_NXDOMAIN    BASE_STATUS_FROM_DNS_RCODE(3)	/* 320053 */
/**
 * @hideinitializer
 * Not Implemented - The name server does not support the requested kind of 
 * query.
 * This corresponds to DNS RCODE 4.
 */
#define UTIL_EDNS_NOTIMPL    BASE_STATUS_FROM_DNS_RCODE(4)	/* 320054 */
/**
 * @hideinitializer
 * Refused - The name server refuses to perform the specified operation for
 * policy reasons.
 * This corresponds to DNS RCODE 5.
 */
#define UTIL_EDNS_REFUSED	    BASE_STATUS_FROM_DNS_RCODE(5)	/* 320055 */
/**
 * @hideinitializer
 * The name exists.
 * This corresponds to DNS RCODE 6.
 */
#define UTIL_EDNS_YXDOMAIN    BASE_STATUS_FROM_DNS_RCODE(6)	/* 320056 */
/**
 * @hideinitializer
 * The RRset (name, type) exists.
 * This corresponds to DNS RCODE 7.
 */
#define UTIL_EDNS_YXRRSET	    BASE_STATUS_FROM_DNS_RCODE(7)	/* 320057 */
/**
 * @hideinitializer
 * The RRset (name, type) does not exist.
 * This corresponds to DNS RCODE 8.
 */
#define UTIL_EDNS_NXRRSET	    BASE_STATUS_FROM_DNS_RCODE(8)	/* 320058 */
/**
 * @hideinitializer
 * The requestor is not authorized to perform this operation.
 * This corresponds to DNS RCODE 9.
 */
#define UTIL_EDNS_NOTAUTH	    BASE_STATUS_FROM_DNS_RCODE(9)	/* 320059 */
/**
 * @hideinitializer
 * The zone specified is not a zone.
 * This corresponds to DNS RCODE 10.
 */
#define UTIL_EDNS_NOTZONE	    BASE_STATUS_FROM_DNS_RCODE(10)/* 320060 */


/************************************************************
 * NEW STUN ERROR
 ***********************************************************/
/* Messaging errors */
/**
 * @hideinitializer
 * Too many STUN attributes.
 */
#define UTIL_ESTUNTOOMANYATTR (UTIL_ERRNO_START+110)/* 320110 */
/**
 * @hideinitializer
 * Unknown STUN attribute. This error happens when the decoder encounters
 * mandatory attribute type which it doesn't understand.
 */
#define UTIL_ESTUNUNKNOWNATTR (UTIL_ERRNO_START+111)/* 320111 */
/**
 * @hideinitializer
 * Invalid STUN socket address length.
 */
#define UTIL_ESTUNINADDRLEN   (UTIL_ERRNO_START+112)/* 320112 */
/**
 * @hideinitializer
 * STUN IPv6 attribute not supported
 */
#define UTIL_ESTUNIPV6NOTSUPP (UTIL_ERRNO_START+113)/* 320113 */
/**
 * @hideinitializer
 * Expecting STUN response message.
 */
#define UTIL_ESTUNNOTRESPONSE (UTIL_ERRNO_START+114)/* 320114 */
/**
 * @hideinitializer
 * STUN transaction ID mismatch.
 */
#define UTIL_ESTUNINVALIDID   (UTIL_ERRNO_START+115)/* 320115 */
/**
 * @hideinitializer
 * Unable to find handler for the request.
 */
#define UTIL_ESTUNNOHANDLER   (UTIL_ERRNO_START+116)/* 320116 */
/**
 * @hideinitializer
 * Found non-FINGERPRINT attribute after MESSAGE-INTEGRITY. This is not
 * valid since MESSAGE-INTEGRITY MUST be the last attribute or the
 * attribute right before FINGERPRINT before the message.
 */
#define UTIL_ESTUNMSGINTPOS    (UTIL_ERRNO_START+118)/* 320118 */
/**
 * @hideinitializer
 * Found attribute after FINGERPRINT. This is not valid since FINGERPRINT
 * MUST be the last attribute in the message.
 */
#define UTIL_ESTUNFINGERPOS   (UTIL_ERRNO_START+119)/* 320119 */
/**
 * @hideinitializer
 * Missing STUN USERNAME attribute.
 * When credential is included in the STUN message (MESSAGE-INTEGRITY is
 * present), the USERNAME attribute must be present in the message.
 */
#define UTIL_ESTUNNOUSERNAME  (UTIL_ERRNO_START+120)/* 320120 */
/**
 * @hideinitializer
 * Unknown STUN username/credential.
 */
#define UTIL_ESTUNUSERNAME    (UTIL_ERRNO_START+121)/* 320121 */
/**
 * @hideinitializer
 * Missing/invalidSTUN MESSAGE-INTEGRITY attribute.
 */
#define UTIL_ESTUNMSGINT	    (UTIL_ERRNO_START+122)/* 320122 */
/**
 * @hideinitializer
 * Found duplicate STUN attribute.
 */
#define UTIL_ESTUNDUPATTR	    (UTIL_ERRNO_START+123)/* 320123 */
/**
 * @hideinitializer
 * Missing STUN REALM attribute.
 */
#define UTIL_ESTUNNOREALM	    (UTIL_ERRNO_START+124)/* 320124 */
/**
 * @hideinitializer
 * Missing/stale STUN NONCE attribute value.
 */
#define UTIL_ESTUNNONCE	    (UTIL_ERRNO_START+125)/* 320125 */
/**
 * @hideinitializer
 * STUN transaction terminates with failure.
 */
#define UTIL_ESTUNTSXFAILED    (UTIL_ERRNO_START+126)/* 320126 */


//#define BASE_STATUS_FROM_STUN_CODE(code)	(UTIL_ERRNO_START+code)

/************************************************************
 * HTTP Client ERROR
 ***********************************************************/
/**
 * @hideinitializer
 * Invalid URL format
 */
#define UTIL_EHTTPINURL	    (UTIL_ERRNO_START+151)/* 320151 */
/**
 * @hideinitializer
 * Invalid port number
 */
#define UTIL_EHTTPINPORT	    (UTIL_ERRNO_START+152)/* 320152 */
/**
 * @hideinitializer
 * Incomplete headers received
 */
#define UTIL_EHTTPINCHDR	    (UTIL_ERRNO_START+153)/* 320153 */
/**
 * @hideinitializer
 * Insufficient buffer
 */
#define UTIL_EHTTPINSBUF	    (UTIL_ERRNO_START+154)/* 320154 */
/**
 * @hideinitializer
 * Connection lost
 */
#define UTIL_EHTTPLOST	    (UTIL_ERRNO_START+155)/* 320155 */

/************************************************************
 * CLI ERROR
 ***********************************************************/

/**
 * @hideinitializer
 * End the current session. This is a special error code returned by
 * bcli_sess_exec() to indicate that "exit" or equivalent command has been
 * called to end the current session.
 */
#define BASE_CLI_EEXIT        	    (UTIL_ERRNO_START+201)/* 320201 */
/**
 * @hideinitializer
 * A required CLI argument is not specified.
 */
#define BASE_CLI_EMISSINGARG    	    (UTIL_ERRNO_START+202)/* 320202 */
 /**
 * @hideinitializer
 * Too many CLI arguments.
 */
#define BASE_CLI_ETOOMANYARGS    	    (UTIL_ERRNO_START+203)/* 320203 */
/**
 * @hideinitializer
 * Invalid CLI argument. Typically this is caused by extra characters
 * specified in the command line which does not match any arguments.
 */
#define BASE_CLI_EINVARG        	    (UTIL_ERRNO_START+204)/* 320204 */
/**
 * @hideinitializer
 * CLI command with the specified name already exist.
 */
#define BASE_CLI_EBADNAME        	    (UTIL_ERRNO_START+205)/* 320205 */
/**
 * @hideinitializer
 * CLI command with the specified id already exist.
 */
#define BASE_CLI_EBADID        	    (UTIL_ERRNO_START+206)/* 320206 */
/**
 * @hideinitializer
 * Invalid XML format for CLI command specification.
 */
#define BASE_CLI_EBADXML        	    (UTIL_ERRNO_START+207)/* 320207 */
/**
 * @hideinitializer
 * CLI command entered by user match with more than one command/argument 
 * specification.
 */
#define BASE_CLI_EAMBIGUOUS	    (UTIL_ERRNO_START+208)/* 320208 */
/**
 * @hideinitializer
 * Telnet connection lost.
 */
#define BASE_CLI_ETELNETLOST          (UTIL_ERRNO_START+211)/* 320211 */

BASE_BEGIN_DECL
bstatus_t libUtilInit(void);
BASE_END_DECL

#endif

