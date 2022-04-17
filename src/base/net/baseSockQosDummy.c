/* 
 *
 */
#include <baseSockQos.h>
#include <baseErrno.h>
#include <baseLog.h>

/* Dummy implementation of QoS API. 
 */
#if defined(BASE_QOS_IMPLEMENTATION) && BASE_QOS_IMPLEMENTATION==BASE_QOS_DUMMY

bstatus_t bsock_set_qos_params(bsock_t sock,
					   bqos_params *param)
{
	BASE_UNUSED_ARG(sock);
	BASE_UNUSED_ARG(param);

	BASE_WARN("bsock_set_qos_params() is not implemented for this platform");
	return BASE_ENOTSUP;
}

bstatus_t bsock_set_qos_type(bsock_t sock,
					 bqos_type type)
{
	BASE_UNUSED_ARG(sock);
	BASE_UNUSED_ARG(type);

	BASE_WARN("bsock_set_qos_type() is not implemented for this platform"));
	return BASE_ENOTSUP;
}


bstatus_t bsock_get_qos_params(bsock_t sock, bqos_params *p_param)
{
	BASE_UNUSED_ARG(sock);
	BASE_UNUSED_ARG(p_param);

	BASE_WARN("bsock_get_qos_params() is not implemented for this platform"));
	return BASE_ENOTSUP;
}

bstatus_t bsock_get_qos_type(bsock_t sock, bqos_type *p_type)
{
	BASE_UNUSED_ARG(sock);
	BASE_UNUSED_ARG(p_type);

	BASE_WARN( "bsock_get_qos_type() is not implemented for this platform"));
	return BASE_ENOTSUP;
}

#endif

