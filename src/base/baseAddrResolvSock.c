/* 
 *
 */
#include <baseAddrResolv.h>
#include <baseAssert.h>
#include <baseString.h>
#include <baseErrno.h>
#include <baseIpHelper.h>
#include <compat/socket.h>

#if defined(BASE_GETADDRINFO_USE_CFHOST) && BASE_GETADDRINFO_USE_CFHOST!=0
#   include <CoreFoundation/CFString.h>
#   include <CFNetwork/CFHost.h>
#endif

bstatus_t bgethostbyname(const bstr_t *hostname, bhostent *phe)
{
    struct hostent *he;
    char copy[BASE_MAX_HOSTNAME];

    bassert(hostname && hostname ->slen < BASE_MAX_HOSTNAME);
    
    if (hostname->slen >= BASE_MAX_HOSTNAME)
	return BASE_ENAMETOOLONG;

    bmemcpy(copy, hostname->ptr, hostname->slen);
    copy[ hostname->slen ] = '\0';

    he = gethostbyname(copy);
    if (!he) {
	return BASE_ERESOLVE;
	/* DO NOT use bget_netos_error() since host resolution error
	 * is reported in h_errno instead of errno!
	return bget_netos_error();
	 */
    }

    phe->h_name = he->h_name;
    phe->h_aliases = he->h_aliases;
    phe->h_addrtype = he->h_addrtype;
    phe->h_length = he->h_length;
    phe->h_addr_list = he->h_addr_list;

    return BASE_SUCCESS;
}

/* Resolve IPv4/IPv6 address */
bstatus_t bgetaddrinfo(int af, const bstr_t *nodename,
				   unsigned *count, baddrinfo ai[])
{
#if defined(BASE_SOCK_HAS_GETADDRINFO) && BASE_SOCK_HAS_GETADDRINFO!=0
    char nodecopy[BASE_MAX_HOSTNAME];
    bbool_t has_addr = BASE_FALSE;
    unsigned i;
#if defined(BASE_GETADDRINFO_USE_CFHOST) && BASE_GETADDRINFO_USE_CFHOST!=0
    CFStringRef hostname;
    CFHostRef hostRef;
    bstatus_t status = BASE_SUCCESS;
#else
    int rc;
    struct addrinfo hint, *res, *orig_res;
#endif

    BASE_ASSERT_RETURN(nodename && count && *count && ai, BASE_EINVAL);
    BASE_ASSERT_RETURN(nodename->ptr && nodename->slen, BASE_EINVAL);
    BASE_ASSERT_RETURN(af==BASE_AF_INET || af==BASE_AF_INET6 ||
		     af==BASE_AF_UNSPEC, BASE_EINVAL);

#if BASE_WIN32_WINCE

    /* Check if nodename is IP address */
    bbzero(&ai[0], sizeof(ai[0]));
    if ((af==BASE_AF_INET || af==BASE_AF_UNSPEC) &&
	binet_pton(BASE_AF_INET, nodename,
		     &ai[0].ai_addr.ipv4.sin_addr) == BASE_SUCCESS)
    {
	af = BASE_AF_INET;
	has_addr = BASE_TRUE;
    } else if ((af==BASE_AF_INET6 || af==BASE_AF_UNSPEC) &&
	       binet_pton(BASE_AF_INET6, nodename,
	                    &ai[0].ai_addr.ipv6.sin6_addr) == BASE_SUCCESS)
    {
	af = BASE_AF_INET6;
	has_addr = BASE_TRUE;
    }

    if (has_addr) {
	bstr_t tmp;

	tmp.ptr = ai[0].ai_canonname;
	bstrncpy_with_null(&tmp, nodename, BASE_MAX_HOSTNAME);
	ai[0].ai_addr.addr.sa_family = (buint16_t)af;
	*count = 1;

	return BASE_SUCCESS;
    }

#else /* BASE_WIN32_WINCE */
    BASE_UNUSED_ARG(has_addr);
#endif

    /* Copy node name to null terminated string. */
    if (nodename->slen >= BASE_MAX_HOSTNAME)
	return BASE_ENAMETOOLONG;
    bmemcpy(nodecopy, nodename->ptr, nodename->slen);
    nodecopy[nodename->slen] = '\0';

#if defined(BASE_GETADDRINFO_USE_CFHOST) && BASE_GETADDRINFO_USE_CFHOST!=0
    hostname =  CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, nodecopy,
						kCFStringEncodingASCII,
						kCFAllocatorNull);
    hostRef = CFHostCreateWithName(kCFAllocatorDefault, hostname);
    if (CFHostStartInfoResolution(hostRef, kCFHostAddresses, nil)) {
	CFArrayRef addrRef = CFHostGetAddressing(hostRef, nil);
	i = 0;
	if (addrRef != nil) {
	    CFIndex idx, naddr;
	    
	    naddr = CFArrayGetCount(addrRef);
	    for (idx = 0; idx < naddr && i < *count; idx++) {
		struct sockaddr *addr;
		size_t addr_size;
		
		addr = (struct sockaddr *)
		       CFDataGetBytePtr(CFArrayGetValueAtIndex(addrRef, idx));
		/* This should not happen. */
		bassert(addr);
		
		/* Ignore unwanted address families */
		if (af!=BASE_AF_UNSPEC && addr->sa_family != af)
		    continue;

		/* Store canonical name */
		bansi_strcpy(ai[i].ai_canonname, nodecopy);
		
		/* Store address */
		addr_size = sizeof(*addr);
		if (addr->sa_family == BASE_AF_INET6) {
		    addr_size = addr->sa_len;
		}
		BASE_ASSERT_ON_FAIL(addr_size <= sizeof(bsockaddr), continue);
		bmemcpy(&ai[i].ai_addr, addr, addr_size);
		BASE_SOCKADDR_RESET_LEN(&ai[i].ai_addr);
		
		i++;
	    }
	}
	
	*count = i;
	if (*count == 0)
	    status = BASE_ERESOLVE;

    } else {
	status = BASE_ERESOLVE;
    }
    
    CFRelease(hostRef);
    CFRelease(hostname);
    
    return status;
#else
    /* Call getaddrinfo() */
    bbzero(&hint, sizeof(hint));
    hint.ai_family = af;
    hint.ai_socktype = bSOCK_DGRAM() | bSOCK_STREAM();

    rc = getaddrinfo(nodecopy, NULL, &hint, &res);
    if (rc != 0)
	return BASE_ERESOLVE;

    orig_res = res;

    /* Enumerate each item in the result */
    for (i=0; i<*count && res; res=res->ai_next) {
	/* Ignore unwanted address families */
	if (af!=BASE_AF_UNSPEC && res->ai_family != af)
	    continue;

	/* Store canonical name (possibly truncating the name) */
	if (res->ai_canonname) {
	    bansi_strncpy(ai[i].ai_canonname, res->ai_canonname,
			    sizeof(ai[i].ai_canonname));
	    ai[i].ai_canonname[sizeof(ai[i].ai_canonname)-1] = '\0';
	} else {
	    bansi_strcpy(ai[i].ai_canonname, nodecopy);
	}

	/* Store address */
	BASE_ASSERT_ON_FAIL(res->ai_addrlen <= sizeof(bsockaddr), continue);
	bmemcpy(&ai[i].ai_addr, res->ai_addr, res->ai_addrlen);
	BASE_SOCKADDR_RESET_LEN(&ai[i].ai_addr);

	/* Next slot */
	++i;
    }

    *count = i;

    freeaddrinfo(orig_res);

    /* Done */
    return (*count > 0? BASE_SUCCESS : BASE_ERESOLVE);
#endif

#else	/* BASE_SOCK_HAS_GETADDRINFO */
    bbool_t has_addr = BASE_FALSE;

    BASE_ASSERT_RETURN(count && *count, BASE_EINVAL);

#if BASE_WIN32_WINCE

    /* Check if nodename is IP address */
    bbzero(&ai[0], sizeof(ai[0]));
    if ((af==BASE_AF_INET || af==BASE_AF_UNSPEC) &&
	binet_pton(BASE_AF_INET, nodename,
		     &ai[0].ai_addr.ipv4.sin_addr) == BASE_SUCCESS)
    {
	af = BASE_AF_INET;
	has_addr = BASE_TRUE;
    }
    else if ((af==BASE_AF_INET6 || af==BASE_AF_UNSPEC) &&
	     binet_pton(BASE_AF_INET6, nodename,
			  &ai[0].ai_addr.ipv6.sin6_addr) == BASE_SUCCESS)
    {
	af = BASE_AF_INET6;
	has_addr = BASE_TRUE;
    }

    if (has_addr) {
	bstr_t tmp;

	tmp.ptr = ai[0].ai_canonname;
	bstrncpy_with_null(&tmp, nodename, BASE_MAX_HOSTNAME);
	ai[0].ai_addr.addr.sa_family = (buint16_t)af;
	*count = 1;

	return BASE_SUCCESS;
    }

#else /* BASE_WIN32_WINCE */
    BASE_UNUSED_ARG(has_addr);
#endif

    if (af == BASE_AF_INET || af == BASE_AF_UNSPEC) {
	bhostent he;
	unsigned i, max_count;
	bstatus_t status;
	
	/* VC6 complains that "he" is uninitialized */
	#ifdef _MSC_VER
	bbzero(&he, sizeof(he));
	#endif

	status = bgethostbyname(nodename, &he);
	if (status != BASE_SUCCESS)
	    return status;

	max_count = *count;
	*count = 0;

	bbzero(ai, max_count * sizeof(baddrinfo));

	for (i=0; he.h_addr_list[i] && *count<max_count; ++i) {
	    bansi_strncpy(ai[*count].ai_canonname, he.h_name,
			    sizeof(ai[*count].ai_canonname));
	    ai[*count].ai_canonname[sizeof(ai[*count].ai_canonname)-1] = '\0';

	    ai[*count].ai_addr.ipv4.sin_family = BASE_AF_INET;
	    bmemcpy(&ai[*count].ai_addr.ipv4.sin_addr,
		      he.h_addr_list[i], he.h_length);
	    BASE_SOCKADDR_RESET_LEN(&ai[*count].ai_addr);

	    (*count)++;
	}

	return (*count > 0? BASE_SUCCESS : BASE_ERESOLVE);

    } else {
	/* IPv6 is not supported */
	*count = 0;

	return BASE_EIPV6NOTSUP;
    }
#endif	/* BASE_SOCK_HAS_GETADDRINFO */
}

