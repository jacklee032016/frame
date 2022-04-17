/* 
 *
 */
#include <baseSockQos.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseString.h>
#include <compat/socket.h>

/* This is the implementation of QoS with BSD socket's setsockopt(),
 * using Darwin-specific SO_NET_SERVICE_TYPE if available, and IP_TOS/
 * IPV6_TCLASS as fallback.
 */
#if defined(BASE_QOS_IMPLEMENTATION) && BASE_QOS_IMPLEMENTATION==BASE_QOS_DARWIN

#include <sys/socket.h>

#ifdef SO_NET_SERVICE_TYPE
static bstatus_t sock_set_net_service_type(bsock_t sock, int val)
{
    bstatus_t status;

    status = bsock_setsockopt(sock, bSOL_SOCKET(), SO_NET_SERVICE_TYPE,
				&val, sizeof(val));
    if (status == BASE_STATUS_FROM_OS(OSERR_ENOPROTOOPT))
	status = BASE_ENOTSUP;

    return status;
}
#endif

static bstatus_t sock_set_net_service_type_type(bsock_t sock,
						  bqos_type type)
{
#ifdef SO_NET_SERVICE_TYPE
    int val = NET_SERVICE_TYPE_BE;

    switch (type) {
	case BASE_QOS_TYPE_BEST_EFFORT:
	    val = NET_SERVICE_TYPE_BE;
	    break;
	case BASE_QOS_TYPE_BACKGROUND:
	    val = NET_SERVICE_TYPE_BK;
	    break;
	case BASE_QOS_TYPE_SIGNALLING:
	    val = NET_SERVICE_TYPE_SIG;
	    break;	    
	case BASE_QOS_TYPE_VIDEO:
	    val = NET_SERVICE_TYPE_VI;
	    break;
	case BASE_QOS_TYPE_VOICE:
	case BASE_QOS_TYPE_CONTROL:
	default:
	    val = NET_SERVICE_TYPE_VO;
	    break;
    }

    return sock_set_net_service_type(sock, val);
#else
    return BASE_ENOTSUP;
#endif
}

static bstatus_t sock_set_net_service_type_params(bsock_t sock,
						    bqos_params *param)
{
#ifdef SO_NET_SERVICE_TYPE
    bstatus_t status;
    int val = -1;

    BASE_ASSERT_RETURN(param, BASE_EINVAL);

    /*
     * Sources:
     *  - IETF draft-szigeti-tsvwg-ieee-802-11e-01
     *  - iOS 10 SDK, sys/socket.h
     */
    if (val == -1 && param->flags & BASE_QOS_PARAM_HAS_DSCP) {
	if (param->dscp_val == 0) /* DF */
	    val = NET_SERVICE_TYPE_BE;
	else if (param->dscp_val < 0x10) /* CS1, AF11, AF12, AF13 */
	    val = NET_SERVICE_TYPE_BK;
	else if (param->dscp_val == 0x10) /* CS2 */
	    val = NET_SERVICE_TYPE_OAM;
	else if (param->dscp_val < 0x18) /* AF21, AF22, AF23 */
	    val = NET_SERVICE_TYPE_RD;
	else if (param->dscp_val < 0x20) /* CS3, AF31, AF32, AF33 */
	    val = NET_SERVICE_TYPE_AV;
	else if (param->dscp_val == 0x20) /* CS4 */
	    val = NET_SERVICE_TYPE_RD;
	else if (param->dscp_val < 0x28) /* AF41, AF42, AF43 */
	    val = NET_SERVICE_TYPE_VI;
	else if (param->dscp_val == 0x28) /* CS5 */
	    val = NET_SERVICE_TYPE_SIG;
	else
	    val = NET_SERVICE_TYPE_VO; /* VOICE-ADMIT, EF, CS6, etc. */
    }

    if (val == -1 && param->flags & BASE_QOS_PARAM_HAS_WMM) {
	switch (param->wmm_prio) {
	    case BASE_QOS_WMM_PRIO_BULK_EFFORT:
		val = NET_SERVICE_TYPE_BE;
		break;
	    case BASE_QOS_WMM_PRIO_BULK:
		val = NET_SERVICE_TYPE_BK;
		break;
	    case BASE_QOS_WMM_PRIO_VIDEO:
		val = NET_SERVICE_TYPE_VI;
		break;
	    case BASE_QOS_WMM_PRIO_VOICE:
		val = NET_SERVICE_TYPE_VO;
		break;
	}
    }

    if (val == -1) {
	bqos_type type;

	status = bqos_get_type(param, &type);

	if (status == BASE_SUCCESS)
	    return sock_set_net_service_type_type(sock, type);

	val = NET_SERVICE_TYPE_BE;
    }

    return sock_set_net_service_type(sock, val);
#else
    return BASE_ENOTSUP;
#endif
}

static bstatus_t sock_set_ip_ds(bsock_t sock, bqos_params *param)
{
    bstatus_t status = BASE_SUCCESS;

    BASE_ASSERT_RETURN(param, BASE_EINVAL);

    if (param->flags & BASE_QOS_PARAM_HAS_DSCP) {
	/* We need to know if the socket is IPv4 or IPv6 */
	bsockaddr sa;
	int salen = sizeof(salen);

	/* Value is dscp_val << 2 */
	int val = (param->dscp_val << 2);

	status = bsock_getsockname(sock, &sa, &salen);
	if (status != BASE_SUCCESS)
	    return status;

	if (sa.addr.sa_family == bAF_INET()) {
	    /* In IPv4, the DS field goes in the TOS field */
	    status = bsock_setsockopt(sock, bSOL_IP(), bIP_TOS(),
					&val, sizeof(val));
	} else if (sa.addr.sa_family == bAF_INET6()) {
	    /* In IPv6, the DS field goes in the Traffic Class field */
	    status = bsock_setsockopt(sock, bSOL_IPV6(),
					bIPV6_TCLASS(),
					&val, sizeof(val));
	} else
	    status = BASE_EINVAL;

	if (status != BASE_SUCCESS) {
	    param->flags &= ~(BASE_QOS_PARAM_HAS_DSCP);
	}
    }

    return status;
}

bstatus_t bsock_set_qos_params(bsock_t sock,
					   bqos_params *param)
{
    bstatus_t status;

    BASE_ASSERT_RETURN(param, BASE_EINVAL);

    /* No op? */
    if (!param->flags)
	return BASE_SUCCESS;

    /* Clear prio field since we don't support it */
    param->flags &= ~(BASE_QOS_PARAM_HAS_SO_PRIO);

    /* Try SO_NET_SERVICE_TYPE */
    status = sock_set_net_service_type_params(sock, param);
    if (status == BASE_SUCCESS)
	return status;

    if (status != BASE_ENOTSUP) {
	/* SO_NET_SERVICE_TYPE sets both DSCP and WMM */
	param->flags &= ~(BASE_QOS_PARAM_HAS_DSCP);
	param->flags &= ~(BASE_QOS_PARAM_HAS_WMM);
	return status;
    }

    /* Fall back to IP_TOS/IPV6_TCLASS */
    return sock_set_ip_ds(sock, param);
}

bstatus_t bsock_set_qos_type(bsock_t sock,
					 bqos_type type)
{
    bstatus_t status;
    bqos_params param;

    /* Try SO_NET_SERVICE_TYPE */
    status = sock_set_net_service_type_type(sock, type);
    if (status == BASE_SUCCESS || status != BASE_ENOTSUP)
	return status;

    /* Fall back to IP_TOS/IPV6_TCLASS */
    status = bqos_get_params(type, &param);
    if (status != BASE_SUCCESS)
	return status;

    return sock_set_ip_ds(sock, &param);
}

#ifdef SO_NET_SERVICE_TYPE
static bstatus_t sock_get_net_service_type(bsock_t sock, int *p_val)
{
    bstatus_t status;
    int optlen = sizeof(*p_val);

    BASE_ASSERT_RETURN(p_val, BASE_EINVAL);

    status = bsock_getsockopt(sock, bSOL_SOCKET(), SO_NET_SERVICE_TYPE,
				p_val, &optlen);
    if (status == BASE_STATUS_FROM_OS(OSERR_ENOPROTOOPT))
	status = BASE_ENOTSUP;

    return status;
}
#endif

static bstatus_t sock_get_net_service_type_type(bsock_t sock,
						  bqos_type *p_type)
{
#ifdef SO_NET_SERVICE_TYPE
    bstatus_t status;
    int val;

    BASE_ASSERT_RETURN(p_type, BASE_EINVAL);

    status = sock_get_net_service_type(sock, &val);
    if (status == BASE_SUCCESS) {
	switch (val) {
	    default:
	    case NET_SERVICE_TYPE_BE:
		*p_type = BASE_QOS_TYPE_BEST_EFFORT;
		break;
	    case NET_SERVICE_TYPE_BK:
		*p_type = BASE_QOS_TYPE_BACKGROUND;
		break;
	    case NET_SERVICE_TYPE_SIG:
		*p_type = BASE_QOS_TYPE_SIGNALLING;
		break;	    
	    case NET_SERVICE_TYPE_VI:
	    case NET_SERVICE_TYPE_RV:
	    case NET_SERVICE_TYPE_AV:
	    case NET_SERVICE_TYPE_OAM:
	    case NET_SERVICE_TYPE_RD:
		*p_type = BASE_QOS_TYPE_VIDEO;
		break;
	    case NET_SERVICE_TYPE_VO:
		*p_type = BASE_QOS_TYPE_VOICE;
		break;
	}
    }

    return status;
#else
    return BASE_ENOTSUP;
#endif
}

static bstatus_t sock_get_net_service_type_params(bsock_t sock,
						    bqos_params *p_param)
{
#ifdef SO_NET_SERVICE_TYPE
    bstatus_t status;
    int val;

    BASE_ASSERT_RETURN(p_param, BASE_EINVAL);

    status = sock_get_net_service_type(sock, &val);
    if (status == BASE_SUCCESS) {
	bbzero(p_param, sizeof(*p_param));

	/* Note: these are just educated guesses, chosen for symmetry with
	 * sock_set_net_service_type_params: we can't know the actual values
	 * chosen by the OS, or even if DSCP/WMM are used at all.
	 *
	 * The source for mapping DSCP to WMM is:
	 *  - IETF draft-szigeti-tsvwg-ieee-802-11e-01
	 */
	switch (val) {
	    default:
	    case NET_SERVICE_TYPE_BE:
		p_param->flags = BASE_QOS_PARAM_HAS_DSCP | BASE_QOS_PARAM_HAS_WMM;
		p_param->dscp_val = 0; /* DF */
		p_param->wmm_prio = BASE_QOS_WMM_PRIO_BULK_EFFORT; /* AC_BE */
		break;
	    case NET_SERVICE_TYPE_BK:
		p_param->flags = BASE_QOS_PARAM_HAS_DSCP | BASE_QOS_PARAM_HAS_WMM;
		p_param->dscp_val = 0x08; /* CS1 */
		p_param->wmm_prio = BASE_QOS_WMM_PRIO_BULK; /* AC_BK */
		break;
	    case NET_SERVICE_TYPE_SIG:
		p_param->flags = BASE_QOS_PARAM_HAS_DSCP | BASE_QOS_PARAM_HAS_WMM;
		p_param->dscp_val = 0x28; /* CS5 */
		p_param->wmm_prio = BASE_QOS_WMM_PRIO_VIDEO; /* AC_VI */
		break;
	    case NET_SERVICE_TYPE_VI:
		p_param->flags = BASE_QOS_PARAM_HAS_DSCP | BASE_QOS_PARAM_HAS_WMM;
		p_param->dscp_val = 0x22; /* AF41 */
		p_param->wmm_prio = BASE_QOS_WMM_PRIO_VIDEO; /* AC_VI */
		break;
	    case NET_SERVICE_TYPE_VO:
		p_param->flags = BASE_QOS_PARAM_HAS_DSCP | BASE_QOS_PARAM_HAS_WMM;
		p_param->dscp_val = 0x30; /* CS6 */
		p_param->wmm_prio = BASE_QOS_WMM_PRIO_VOICE; /* AC_VO */
		break;
	    case NET_SERVICE_TYPE_RV:
		p_param->flags = BASE_QOS_PARAM_HAS_DSCP | BASE_QOS_PARAM_HAS_WMM;
		p_param->dscp_val = 0x22; /* AF41 */
		p_param->wmm_prio = BASE_QOS_WMM_PRIO_VIDEO; /* AC_VI */
		break;
	    case NET_SERVICE_TYPE_AV:
		p_param->flags = BASE_QOS_PARAM_HAS_DSCP | BASE_QOS_PARAM_HAS_WMM;
		p_param->dscp_val = 0x18; /* CS3 */
		p_param->wmm_prio = BASE_QOS_WMM_PRIO_VIDEO; /* AC_VI */
		break;
	    case NET_SERVICE_TYPE_OAM:
		p_param->flags = BASE_QOS_PARAM_HAS_DSCP | BASE_QOS_PARAM_HAS_WMM;
		p_param->dscp_val = 0x10; /* CS2 */
		p_param->wmm_prio = BASE_QOS_WMM_PRIO_BULK_EFFORT; /* AC_BE */
		break;
	    case NET_SERVICE_TYPE_RD:
		p_param->flags = BASE_QOS_PARAM_HAS_DSCP | BASE_QOS_PARAM_HAS_WMM;
		p_param->dscp_val = 0x20; /* CS4 */
		p_param->wmm_prio = BASE_QOS_WMM_PRIO_VIDEO; /* AC_VI */
		break;
	}
    }

    return status;
#else
    return BASE_ENOTSUP;
#endif
}

bstatus_t bsock_get_qos_params(bsock_t sock,
					   bqos_params *p_param)
{
    bstatus_t status;
    int val, optlen;
    bsockaddr sa;
    int salen = sizeof(salen);

    BASE_ASSERT_RETURN(p_param, BASE_EINVAL);

    bbzero(p_param, sizeof(*p_param));

    /* Try SO_NET_SERVICE_TYPE */
    status = sock_get_net_service_type_params(sock, p_param);
    if (status != BASE_ENOTSUP)
	return status;

    /* Fall back to IP_TOS/IPV6_TCLASS */
    status = bsock_getsockname(sock, &sa, &salen);
    if (status != BASE_SUCCESS)
	return status;

    optlen = sizeof(val);
    if (sa.addr.sa_family == bAF_INET()) {
	status = bsock_getsockopt(sock, bSOL_IP(), bIP_TOS(),
				    &val, &optlen);
    } else if (sa.addr.sa_family == bAF_INET6()) {
	status = bsock_getsockopt(sock, bSOL_IPV6(), bIPV6_TCLASS(),
				    &val, &optlen);
    } else
	status = BASE_EINVAL;
    if (status == BASE_SUCCESS) {
	p_param->flags |= BASE_QOS_PARAM_HAS_DSCP;
	p_param->dscp_val = (buint8_t)(val >> 2);
    }

    return status;
}

bstatus_t bsock_get_qos_type(bsock_t sock,
					 bqos_type *p_type)
{
    bqos_params param;
    bstatus_t status;

    BASE_ASSERT_RETURN(p_type, BASE_EINVAL);

    status = sock_get_net_service_type_type(sock, p_type);
    if (status != BASE_ENOTSUP)
	return status;

    status = bsock_get_qos_params(sock, &param);
    if (status != BASE_SUCCESS)
	return status;

    return bqos_get_type(&param, p_type);
}

#endif	/* BASE_QOS_IMPLEMENTATION */
