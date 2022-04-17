/* 
 *
 */
#include <utilErrno.h>
#include <utilTypes.h>
#include <baseAssert.h>
#include <baseString.h>

/* UTIL's own error codes/messages 
 * MUST KEEP THIS ARRAY SORTED!!
 * Message must be limited to 64 chars!
 */
#if defined(BASE_HAS_ERROR_STRING) && BASE_HAS_ERROR_STRING!=0
struct _util_str
{
	int				code;
	const char		*msg;
};

static const struct _util_str _utilErrStr[] = 
{
	/* STUN errors */
	BASE_BUILD_ERR( UTIL_ESTUNRESOLVE,	"Unable to resolve STUN server" ),
	BASE_BUILD_ERR( UTIL_ESTUNINMSGTYPE,	"Unknown STUN message type" ),
	BASE_BUILD_ERR( UTIL_ESTUNINMSGLEN,	"Invalid STUN message length" ),
	BASE_BUILD_ERR( UTIL_ESTUNINATTRLEN,	"STUN attribute length error" ),
	BASE_BUILD_ERR( UTIL_ESTUNINATTRTYPE,	"Invalid STUN attribute type" ),
	BASE_BUILD_ERR( UTIL_ESTUNININDEX,	"Invalid STUN server/socket index" ),
	BASE_BUILD_ERR( UTIL_ESTUNNOBINDRES,	"No STUN binding response in the message" ),
	BASE_BUILD_ERR( UTIL_ESTUNRECVERRATTR,	"Received STUN error attribute" ),
	BASE_BUILD_ERR( UTIL_ESTUNNOMAP,	"No STUN mapped address attribute" ),
	BASE_BUILD_ERR( UTIL_ESTUNNOTRESPOND,	"Received no response from STUN server" ),
	BASE_BUILD_ERR( UTIL_ESTUNSYMMETRIC,	"Symetric NAT detected by STUN" ),

	/* XML errors */
	BASE_BUILD_ERR( UTIL_EINXML,		"Invalid XML message" ),

	/* JSON errors */
	BASE_BUILD_ERR( UTIL_EINJSON,		"Invalid JSON document" ),

	/* DNS errors */
	BASE_BUILD_ERR( UTIL_EDNSQRYTOOSMALL,	"DNS query packet buffer is too small"),
	BASE_BUILD_ERR( UTIL_EDNSINSIZE,	"Invalid DNS packet length"),
	BASE_BUILD_ERR( UTIL_EDNSINCLASS,	"Invalid DNS class"),
	BASE_BUILD_ERR( UTIL_EDNSINNAMEPTR,	"Invalid DNS name pointer"),
	BASE_BUILD_ERR( UTIL_EDNSINNSADDR,	"Invalid DNS nameserver address"),
	BASE_BUILD_ERR( UTIL_EDNSNONS,		"No nameserver is in DNS resolver"),
	BASE_BUILD_ERR( UTIL_EDNSNOWORKINGNS,	"No working DNS nameserver"),
	BASE_BUILD_ERR( UTIL_EDNSNOANSWERREC,	"No answer record in the DNS response"),
	BASE_BUILD_ERR( UTIL_EDNSINANSWER,	"Invalid DNS answer"),

	BASE_BUILD_ERR( UTIL_EDNS_FORMERR,	"DNS \"Format error\""),
	BASE_BUILD_ERR( UTIL_EDNS_SERVFAIL,	"DNS \"Server failure\""),
	BASE_BUILD_ERR( UTIL_EDNS_NXDOMAIN,	"DNS \"Name Error\""),
	BASE_BUILD_ERR( UTIL_EDNS_NOTIMPL,	"DNS \"Not Implemented\""),
	BASE_BUILD_ERR( UTIL_EDNS_REFUSED,	"DNS \"Refused\""),
	BASE_BUILD_ERR( UTIL_EDNS_YXDOMAIN,	"DNS \"The name exists\""),
	BASE_BUILD_ERR( UTIL_EDNS_YXRRSET,	"DNS \"The RRset (name, type) exists\""),
	BASE_BUILD_ERR( UTIL_EDNS_NXRRSET,	"DNS \"The RRset (name, type) does not exist\""),
	BASE_BUILD_ERR( UTIL_EDNS_NOTAUTH,	"DNS \"Not authorized\""),
	BASE_BUILD_ERR( UTIL_EDNS_NOTZONE,	"DNS \"The zone specified is not a zone\""),

	/* STUN */
	BASE_BUILD_ERR( UTIL_ESTUNTOOMANYATTR,	"Too many STUN attributes"),
	BASE_BUILD_ERR( UTIL_ESTUNUNKNOWNATTR,	"Unknown STUN attribute"),
	BASE_BUILD_ERR( UTIL_ESTUNINADDRLEN,	"Invalid STUN socket address length"),
	BASE_BUILD_ERR( UTIL_ESTUNIPV6NOTSUPP,	"STUN IPv6 attribute not supported"),
	BASE_BUILD_ERR( UTIL_ESTUNNOTRESPONSE,	"Expecting STUN response message"),
	BASE_BUILD_ERR( UTIL_ESTUNINVALIDID,	"STUN transaction ID mismatch"),
	BASE_BUILD_ERR( UTIL_ESTUNNOHANDLER,	"Unable to find STUN handler for the request"),
	BASE_BUILD_ERR( UTIL_ESTUNMSGINTPOS,	"Found non-FINGERPRINT attr. after MESSAGE-INTEGRITY"),
	BASE_BUILD_ERR( UTIL_ESTUNFINGERPOS,	"Found STUN attribute after FINGERPRINT"),
	BASE_BUILD_ERR( UTIL_ESTUNNOUSERNAME,	"Missing STUN USERNAME attribute"),
	BASE_BUILD_ERR( UTIL_ESTUNMSGINT,	"Missing/invalid STUN MESSAGE-INTEGRITY attribute"),
	BASE_BUILD_ERR( UTIL_ESTUNDUPATTR,	"Found duplicate STUN attribute"),
	BASE_BUILD_ERR( UTIL_ESTUNNOREALM,	"Missing STUN REALM attribute"),
	BASE_BUILD_ERR( UTIL_ESTUNNONCE,	"Missing/stale STUN NONCE attribute value"),
	BASE_BUILD_ERR( UTIL_ESTUNTSXFAILED,	"STUN transaction terminates with failure"),

	/* HTTP Client */
	BASE_BUILD_ERR( UTIL_EHTTPINURL,	"Invalid URL format"),
	BASE_BUILD_ERR( UTIL_EHTTPINPORT,	"Invalid URL port number"),
	BASE_BUILD_ERR( UTIL_EHTTPINCHDR,	"Incomplete response header received"),
	BASE_BUILD_ERR( UTIL_EHTTPINSBUF,	"Insufficient buffer"),
	BASE_BUILD_ERR( UTIL_EHTTPLOST,	        "Connection lost"),

	/* CLI */
	BASE_BUILD_ERR( BASE_CLI_EEXIT,	                "Exit current session"),
	BASE_BUILD_ERR( BASE_CLI_EMISSINGARG,	        "Missing argument"),
	BASE_BUILD_ERR( BASE_CLI_ETOOMANYARGS,	        "Too many arguments"),
	BASE_BUILD_ERR( BASE_CLI_EINVARG,	        "Invalid argument"),
	BASE_BUILD_ERR( BASE_CLI_EBADNAME,	        "Command name already exists"),
	BASE_BUILD_ERR( BASE_CLI_EBADID,	        "Command id already exists"),
	BASE_BUILD_ERR( BASE_CLI_EBADXML,	        "Invalid XML format"),
	BASE_BUILD_ERR( BASE_CLI_ETELNETLOST,	        "Connection lost"),
};
#endif


/*
 */
bstr_t utilStrError(bstatus_t statcode, char *buf, bsize_t bufsize )
{
	bstr_t errstr;

#if defined(BASE_HAS_ERROR_STRING) && (BASE_HAS_ERROR_STRING != 0)

	if (statcode >= UTIL_ERRNO_START && statcode < UTIL_ERRNO_START + BASE_ERRNO_SPACE_SIZE)
	{
		/* Find the error in the table. Use binary search!
		*/
		int first = 0;
		int n = BASE_ARRAY_SIZE(_utilErrStr);

		while (n > 0)
		{
			int half = n/2;
			int mid = first + half;

			if (_utilErrStr[mid].code < statcode)
			{
				first = mid+1;
				n -= (half+1);
			}
			else if (_utilErrStr[mid].code > statcode)
			{
				n = half;
			}
			else
			{
				first = mid;
				break;
			}
		}


		if (BASE_ARRAY_SIZE(_utilErrStr) && _utilErrStr[first].code == statcode)
		{
			bstr_t msg;

			msg.ptr = (char*)_utilErrStr[first].msg;
			msg.slen = bansi_strlen(_utilErrStr[first].msg);

			errstr.ptr = buf;
			bstrncpy_with_null(&errstr, &msg, bufsize);
			return errstr;

		} 
	}

#endif	/* BASE_HAS_ERROR_STRING */

	/* Error not found. */
	errstr.ptr = buf;
	errstr.slen = bansi_snprintf(buf, bufsize, "Unknown util error %d", statcode);
	if (errstr.slen < 1 || errstr.slen >= (bssize_t)bufsize)
		errstr.slen = bufsize - 1;
	
	return errstr;
}


bstatus_t libUtilInit(void)
{
	bstatus_t status;

	status = bregister_strerror(UTIL_ERRNO_START, BASE_ERRNO_SPACE_SIZE,  &utilStrError);
	bassert(status == BASE_SUCCESS);

	return status;
}

