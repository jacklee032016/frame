/* 
 *
 */
#include <baseIpHelper.h>
#include <baseAddrResolv.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseString.h>
#include <compat/socket.h>
#include <baseSock.h>

/* Set to 1 to enable tracing */
#if 0
#   include <baseLog.h>

    static const char *get_os_errmsg(void)
    {
	static char errmsg[BASE_ERR_MSG_SIZE];
	extStrError(bget_os_error(), errmsg, sizeof(errmsg));
	return errmsg;
    }
    static const char *get_addr(void *addr)
    {
	static char txt[BASE_INET6_ADDRSTRLEN];
	struct sockaddr *ad = (struct sockaddr*)addr;
	if (ad->sa_family != BASE_AF_INET && ad->sa_family != BASE_AF_INET6)
	    return "?";
	return binet_ntop2(ad->sa_family, bsockaddr_get_addr(ad), 
			     txt, sizeof(txt));
    }
#else
#endif


#if 0
    /* dummy */

#elif defined(BASE_HAS_IFADDRS_H) && BASE_HAS_IFADDRS_H != 0 && \
      defined(BASE_HAS_NET_IF_H) && BASE_HAS_NET_IF_H != 0
/* Using getifaddrs() is preferred since it can work with both IPv4 and IPv6 */
static bstatus_t if_enum_by_af(int af,
				 unsigned *p_cnt,
				 bsockaddr ifs[])
{
    struct ifaddrs *ifap = NULL, *it;
    unsigned max;

    BASE_ASSERT_RETURN(af==BASE_AF_INET || af==BASE_AF_INET6, BASE_EINVAL);
    
    MTRACE("Starting interface enum with getifaddrs() for af=%d", af);

    if (getifaddrs(&ifap) != 0) {
	MTRACE( " getifarrds() failed: %s", get_os_errmsg());
	return BASE_RETURN_OS_ERROR(bget_netos_error());
    }

    it = ifap;
    max = *p_cnt;
    *p_cnt = 0;
    for (; it!=NULL && *p_cnt < max; it = it->ifa_next) {
	struct sockaddr *ad = it->ifa_addr;

	MTRACE(" checking %s", it->ifa_name);

	if ((it->ifa_flags & IFF_UP)==0) {
	    MTRACE("  interface is down");
	    continue; /* Skip when interface is down */
	}

#if BASE_IP_HELPER_IGNORE_LOOPBACK_IF
	if (it->ifa_flags & IFF_LOOPBACK) {
	    MTRACE( "  loopback interface");
	    continue; /* Skip loopback interface */
	}
#endif

	if (ad==NULL) {
	    MTRACE( "  NULL address ignored");
	    continue; /* reported to happen on Linux 2.6.25.9 
			 with ppp interface */
	}

	if (ad->sa_family != af) {
	    MTRACE("  address %s ignored (af=%d)", get_addr(ad), ad->sa_family);
	    continue; /* Skip when interface is down */
	}

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if (af==bAF_INET() &&
	    (bntohl(((bsockaddr_in*)ad)->sin_addr.s_addr) >> 24) == 0)
	{
	    MTRACE("  address %s ignored (0.0.0.0/8 class)", get_addr(ad), ad->sa_family);
	    continue;
	}

	MTRACE("  address %s (af=%d) added at index %d", get_addr(ad), ad->sa_family, *p_cnt);

	bbzero(&ifs[*p_cnt], sizeof(ifs[0]));
	bmemcpy(&ifs[*p_cnt], ad, bsockaddr_get_len(ad));
	BASE_SOCKADDR_RESET_LEN(&ifs[*p_cnt]);
	(*p_cnt)++;
    }

    freeifaddrs(ifap);
    MTRACE("done, found %d address(es)", *p_cnt);
    return (*p_cnt != 0) ? BASE_SUCCESS : BASE_ENOTFOUND;
}

#elif defined(SIOCGIFCONF) && \
      defined(BASE_HAS_NET_IF_H) && BASE_HAS_NET_IF_H != 0

/* Note: this does not work with IPv6 */
static bstatus_t if_enum_by_af(int af,
				 unsigned *p_cnt,
				 bsockaddr ifs[])
{
    bsock_t sock;
    char buf[512];
    struct ifconf ifc;
    struct ifreq *ifr;
    int i, count;
    bstatus_t status;

    BASE_ASSERT_RETURN(af==BASE_AF_INET || af==BASE_AF_INET6, BASE_EINVAL);
    
    MTRACE( "Starting interface enum with SIOCGIFCONF for af=%d",   af);

    status = bsock_socket(af, BASE_SOCK_DGRAM, 0, &sock);
    if (status != BASE_SUCCESS)
	return status;

    /* Query available interfaces */
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;

    if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
	int oserr = bget_netos_error();
	MTRACE(" ioctl(SIOCGIFCONF) failed: %s", get_os_errmsg());
	bsock_close(sock);
	return BASE_RETURN_OS_ERROR(oserr);
    }

    /* Interface interfaces */
    ifr = (struct ifreq*) ifc.ifc_req;
    count = ifc.ifc_len / sizeof(struct ifreq);
    if (count > *p_cnt)
	count = *p_cnt;

    *p_cnt = 0;
    for (i=0; i<count; ++i) {
	struct ifreq *itf = &ifr[i];
        struct ifreq iff = *itf;
	struct sockaddr *ad = &itf->ifr_addr;
	
	MTRACE( " checking interface %s", itf->ifr_name);

	/* Skip address with different family */
	if (ad->sa_family != af) {
	    MTRACE("  address %s (af=%d) ignored", get_addr(ad), (int)ad->sa_family);
	    continue;
	}

        if (ioctl(sock, SIOCGIFFLAGS, &iff) != 0) {
	    MTRACE("  ioctl(SIOCGIFFLAGS) failed: %s", get_os_errmsg());
	    continue;	/* Failed to get flags, continue */
	}

	if ((iff.ifr_flags & IFF_UP)==0) {
	    MTRACE("  interface is down");
	    continue; /* Skip when interface is down */
	}

#if BASE_IP_HELPER_IGNORE_LOOPBACK_IF
	if (iff.ifr_flags & IFF_LOOPBACK) {
	    MTRACE( "  loopback interface");
	    continue; /* Skip loopback interface */
	}
#endif

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if (af==bAF_INET() &&
	    (bntohl(((bsockaddr_in*)ad)->sin_addr.s_addr) >> 24) == 0)
	{
	    MTRACE("  address %s ignored (0.0.0.0/8 class)", get_addr(ad), ad->sa_family);
	    continue;
	}

	MTRACE("  address %s (af=%d) added at index %d", get_addr(ad), ad->sa_family, *p_cnt);

	bbzero(&ifs[*p_cnt], sizeof(ifs[0]));
	bmemcpy(&ifs[*p_cnt], ad, bsockaddr_get_len(ad));
	BASE_SOCKADDR_RESET_LEN(&ifs[*p_cnt]);
	(*p_cnt)++;
    }

    /* Done with socket */
    bsock_close(sock);

    MTRACE("done, found %d address(es)", *p_cnt);
    return (*p_cnt != 0) ? BASE_SUCCESS : BASE_ENOTFOUND;
}


#elif defined(BASE_HAS_NET_IF_H) && BASE_HAS_NET_IF_H != 0
/* Note: this does not work with IPv6 */
static bstatus_t if_enum_by_af(int af, unsigned *p_cnt, bsockaddr ifs[])
{
    struct if_nameindex *if_list;
    struct ifreq ifreq;
    bsock_t sock;
    unsigned i, max_count;
    bstatus_t status;

    BASE_ASSERT_RETURN(af==BASE_AF_INET || af==BASE_AF_INET6, BASE_EINVAL);

    MTRACE("Starting if_nameindex() for af=%d", af);

    status = bsock_socket(af, BASE_SOCK_DGRAM, 0, &sock);
    if (status != BASE_SUCCESS)
	return status;

    if_list = if_nameindex();
    if (if_list == NULL)
	return BASE_ENOTFOUND;

    max_count = *p_cnt;
    *p_cnt = 0;
    for (i=0; if_list[i].if_index && *p_cnt<max_count; ++i) {
	struct sockaddr *ad;
	int rc;

	strncpy(ifreq.ifr_name, if_list[i].if_name, IFNAMSIZ);

	MTRACE( " checking interface %s", ifreq.ifr_name);

	if ((rc=ioctl(sock, SIOCGIFFLAGS, &ifreq)) != 0) {
	    MTRACE("  ioctl(SIOCGIFFLAGS) failed: %s", get_os_errmsg());
	    continue;	/* Failed to get flags, continue */
	}

	if ((ifreq.ifr_flags & IFF_UP)==0) {
	    MTRACE( "  interface is down");
	    continue; /* Skip when interface is down */
	}

#if BASE_IP_HELPER_IGNORE_LOOPBACK_IF
	if (ifreq.ifr_flags & IFF_LOOPBACK) {
	    MTRACE( "  loopback interface");
	    continue; /* Skip loopback interface */
	}
#endif

	/* Note: SIOCGIFADDR does not work for IPv6! */
	if ((rc=ioctl(sock, SIOCGIFADDR, &ifreq)) != 0) {
	    MTRACE("  ioctl(SIOCGIFADDR) failed: %s",  get_os_errmsg());
	    continue;	/* Failed to get address, continue */
	}

	ad = (struct sockaddr*) &ifreq.ifr_addr;

	if (ad->sa_family != af) {
	    MTRACE( "  address %s family %d ignored", get_addr(&ifreq.ifr_addr), ifreq.ifr_addr.sa_family);
	    continue;	/* Not address family that we want, continue */
	}

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if (af==bAF_INET() &&
	    (bntohl(((bsockaddr_in*)ad)->sin_addr.s_addr) >> 24) == 0)
	{
	    MTRACE("  address %s ignored (0.0.0.0/8 class)", get_addr(ad), ad->sa_family);
	    continue;
	}

	/* Got an address ! */
	MTRACE("  address %s (af=%d) added at index %d", get_addr(ad), ad->sa_family, *p_cnt);

	bbzero(&ifs[*p_cnt], sizeof(ifs[0]));
	bmemcpy(&ifs[*p_cnt], ad, bsockaddr_get_len(ad));
	BASE_SOCKADDR_RESET_LEN(&ifs[*p_cnt]);
	(*p_cnt)++;
    }

    if_freenameindex(if_list);
    bsock_close(sock);

    MTRACE("done, found %d address(es)", *p_cnt);
    return (*p_cnt != 0) ? BASE_SUCCESS : BASE_ENOTFOUND;
}

#else
static bstatus_t if_enum_by_af(int af,
				 unsigned *p_cnt,
				 bsockaddr ifs[])
{
    bstatus_t status;

    BASE_ASSERT_RETURN(p_cnt && *p_cnt > 0 && ifs, BASE_EINVAL);

    bbzero(ifs, sizeof(ifs[0]) * (*p_cnt));

    /* Just get one default route */
    status = bgetdefaultipinterface(af, &ifs[0]);
    if (status != BASE_SUCCESS)
	return status;

    *p_cnt = 1;
    return BASE_SUCCESS;
}
#endif /* SIOCGIFCONF */

/*
 * Enumerate the local IP interface currently active in the host.
 */
bstatus_t benum_ip_interface(int af,
					 unsigned *p_cnt,
					 bsockaddr ifs[])
{
    unsigned start;
    bstatus_t status;

    start = 0;
    if (af==BASE_AF_INET6 || af==BASE_AF_UNSPEC) {
	unsigned max = *p_cnt;
	status = if_enum_by_af(BASE_AF_INET6, &max, &ifs[start]);
	if (status == BASE_SUCCESS) {
	    start += max;
	    (*p_cnt) -= max;
	}
    }

    if (af==BASE_AF_INET || af==BASE_AF_UNSPEC) {
	unsigned max = *p_cnt;
	status = if_enum_by_af(BASE_AF_INET, &max, &ifs[start]);
	if (status == BASE_SUCCESS) {
	    start += max;
	    (*p_cnt) -= max;
	}
    }

    *p_cnt = start;

    return (*p_cnt != 0) ? BASE_SUCCESS : BASE_ENOTFOUND;
}

/*
 * Enumerate the IP routing table for this host.
 */
bstatus_t benum_ip_route(unsigned *p_cnt,
				     bip_route_entry routes[])
{
    bsockaddr itf;
    bstatus_t status;

    BASE_ASSERT_RETURN(p_cnt && *p_cnt > 0 && routes, BASE_EINVAL);

    bbzero(routes, sizeof(routes[0]) * (*p_cnt));

    /* Just get one default route */
    status = bgetdefaultipinterface(BASE_AF_INET, &itf);
    if (status != BASE_SUCCESS)
	return status;
    
    routes[0].ipv4.if_addr.s_addr = itf.ipv4.sin_addr.s_addr;
    routes[0].ipv4.dst_addr.s_addr = 0;
    routes[0].ipv4.mask.s_addr = 0;
    *p_cnt = 1;

    return BASE_SUCCESS;
}

