/* 
 *
 */
#include <baseSockQos.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseString.h>

/* This is the implementation of QoS with BSD socket's setsockopt(),
 * using IP_TOS/IPV6_TCLASS and SO_PRIORITY
 */ 
#if !defined(BASE_QOS_IMPLEMENTATION) || BASE_QOS_IMPLEMENTATION==BASE_QOS_BSD

bstatus_t bsock_set_qos_params(bsock_t sock,
					   bqos_params *param)
{
    bstatus_t last_err = BASE_ENOTSUP;
    bstatus_t status;

    /* No op? */
    if (!param->flags)
	return BASE_SUCCESS;

    /* Clear WMM field since we don't support it */
    param->flags &= ~(BASE_QOS_PARAM_HAS_WMM);

    /* Set TOS/DSCP */
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
	} else {
	    status = BASE_EINVAL;
	}

	if (status != BASE_SUCCESS) {
	    param->flags &= ~(BASE_QOS_PARAM_HAS_DSCP);
	    last_err = status;
	}
    }

    /* Set SO_PRIORITY */
    if (param->flags & BASE_QOS_PARAM_HAS_SO_PRIO) {
	int val = param->so_prio;
	status = bsock_setsockopt(sock, bSOL_SOCKET(), bSO_PRIORITY(),
				    &val, sizeof(val));
	if (status != BASE_SUCCESS) {
	    param->flags &= ~(BASE_QOS_PARAM_HAS_SO_PRIO);
	    last_err = status;
	}
    }

    return param->flags ? BASE_SUCCESS : last_err;
}

bstatus_t bsock_set_qos_type(bsock_t sock,
					 bqos_type type)
{
    bqos_params param;
    bstatus_t status;

    status = bqos_get_params(type, &param);
    if (status != BASE_SUCCESS)
	return status;

    return bsock_set_qos_params(sock, &param);
}


bstatus_t bsock_get_qos_params(bsock_t sock,
					   bqos_params *p_param)
{
    bstatus_t last_err = BASE_ENOTSUP;
    int val = 0, optlen;
    bsockaddr sa;
    int salen = sizeof(salen);
    bstatus_t status;

    bbzero(p_param, sizeof(*p_param));

    /* Get DSCP/TOS value */
    status = bsock_getsockname(sock, &sa, &salen);
    if (status == BASE_SUCCESS) {
	optlen = sizeof(val);
	if (sa.addr.sa_family == bAF_INET()) {
	    status = bsock_getsockopt(sock, bSOL_IP(), bIP_TOS(), 
					&val, &optlen);
	} else if (sa.addr.sa_family == bAF_INET6()) {
	    status = bsock_getsockopt(sock, bSOL_IPV6(),
					bIPV6_TCLASS(),
					&val, &optlen);
	} else {
	    status = BASE_EINVAL;
	}

        if (status == BASE_SUCCESS) {
    	    p_param->flags |= BASE_QOS_PARAM_HAS_DSCP;
	    p_param->dscp_val = (buint8_t)(val >> 2);
	} else {
	    last_err = status;
	}
    } else {
	last_err = status;
    }

    /* Get SO_PRIORITY */
    optlen = sizeof(val);
    status = bsock_getsockopt(sock, bSOL_SOCKET(), bSO_PRIORITY(),
				&val, &optlen);
    if (status == BASE_SUCCESS) {
	p_param->flags |= BASE_QOS_PARAM_HAS_SO_PRIO;
	p_param->so_prio = (buint8_t)val;
    } else {
	last_err = status;
    }

    /* WMM is not supported */

    return p_param->flags ? BASE_SUCCESS : last_err;
}

bstatus_t bsock_get_qos_type(bsock_t sock,
					 bqos_type *p_type)
{
    bqos_params param;
    bstatus_t status;

    status = bsock_get_qos_params(sock, &param);
    if (status != BASE_SUCCESS)
	return status;

    return bqos_get_type(&param, p_type);
}

#endif	/* BASE_QOS_IMPLEMENTATION */

