/*
 *
 */
#include <baseSockQos.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseLog.h>

#include <winsock.h>

/* QoS implementation for Windows Mobile 6, must be enabled explicitly
 */
#if defined(BASE_QOS_IMPLEMENTATION) && BASE_QOS_IMPLEMENTATION==BASE_QOS_WM

/* Mapping between our traffic type and WM's DSCP traffic types */
static const int dscp_map[] = 
{
    DSCPBestEffort,
    DSCPBackground,
    DSCPVideo,
    DSCPAudio,
    DSCPControl
};

bstatus_t bsock_set_qos_params(bsock_t sock,
					   bqos_params *param)
{
	BASE_UNUSED_ARG(sock);
	BASE_UNUSED_ARG(param);

	BASE_WARN("bsock_set_qos_params() is not implemented for this platform"));
	return BASE_ENOTSUP;
}

bstatus_t bsock_set_qos_type(bsock_t sock,
					 bqos_type type)
{
    int value;

    BASE_ASSERT_RETURN(type < BASE_ARRAY_SIZE(dscp_map), BASE_EINVAL);

    value = dscp_map[type];
    return bsock_setsockopt(sock, IPPROTO_IP, IP_DSCP_TRAFFIC_TYPE,
			      &value, sizeof(value));
}


bstatus_t bsock_get_qos_params(bsock_t sock,
					   bqos_params *p_param)
{
	BASE_UNUSED_ARG(sock);
	BASE_UNUSED_ARG(p_param);

	BASE_WARN("bsock_get_qos_params() is not implemented for this platform"));
	return BASE_ENOTSUP;
}

bstatus_t bsock_get_qos_type(bsock_t sock,
					 bqos_type *p_type)
{
    bstatus_t status;
    int value, optlen;
    unsigned i;

    optlen = sizeof(value);
    value = 0;
    status = bsock_getsockopt(sock, IPPROTO_IP, IP_DSCP_TRAFFIC_TYPE,
			        &value, &optlen);
    if (status != BASE_SUCCESS)
	return status;

    *p_type = BASE_QOS_TYPE_BEST_EFFORT;
    for (i=0; i<BASE_ARRAY_SIZE(dscp_map); ++i) {
	if (value == dscp_map[i]) {
	    *p_type = (bqos_type)i;
	    break;
	}
    }

    return BASE_SUCCESS;
}

#endif	/* BASE_QOS_IMPLEMENTATION */
