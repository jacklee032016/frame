/* 
 *
 */
#include <baseSockQos.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseLog.h>
#include <baseString.h>

#define ALL_FLAGS   (BASE_QOS_PARAM_HAS_DSCP | BASE_QOS_PARAM_HAS_SO_PRIO | \
                     BASE_QOS_PARAM_HAS_WMM)

/* "Standard" mapping between traffic type and QoS params */
static const bqos_params qos_map[] = 
{
    /* flags	dscp  prio wmm_prio */
    {ALL_FLAGS, 0x00, 0,    BASE_QOS_WMM_PRIO_BULK_EFFORT},   /* BE */
    {ALL_FLAGS, 0x08, 2,    BASE_QOS_WMM_PRIO_BULK},	    /* BK */    
    {ALL_FLAGS, 0x28, 5,    BASE_QOS_WMM_PRIO_VIDEO},	    /* VI */
    {ALL_FLAGS, 0x30, 6,    BASE_QOS_WMM_PRIO_VOICE},	    /* VO */
    {ALL_FLAGS, 0x38, 7,    BASE_QOS_WMM_PRIO_VOICE},	    /* CO */
    {ALL_FLAGS, 0x28, 5,    BASE_QOS_WMM_PRIO_VIDEO}	    /* SIG */
};


/* Retrieve the mapping for the specified type */
bstatus_t bqos_get_params(bqos_type type, 
				      bqos_params *p_param)
{
    BASE_ASSERT_RETURN(type<=BASE_QOS_TYPE_CONTROL && p_param, BASE_EINVAL);
    bmemcpy(p_param, &qos_map[type], sizeof(*p_param));
    return BASE_SUCCESS;
}

/* Get the matching traffic type */
bstatus_t bqos_get_type( const bqos_params *param,
				     bqos_type *p_type)
{
    unsigned dscp_type = BASE_QOS_TYPE_BEST_EFFORT,
	     prio_type = BASE_QOS_TYPE_BEST_EFFORT,
	     wmm_type = BASE_QOS_TYPE_BEST_EFFORT;
    unsigned i, count=0;

    BASE_ASSERT_RETURN(param && p_type, BASE_EINVAL);

    if (param->flags & BASE_QOS_PARAM_HAS_DSCP)  {
	for (i=0; i<=BASE_QOS_TYPE_CONTROL; ++i) {
	    if (param->dscp_val >= qos_map[i].dscp_val)
		dscp_type = (bqos_type)i;
	}
	++count;
    }

    if (param->flags & BASE_QOS_PARAM_HAS_SO_PRIO) {
	for (i=0; i<=BASE_QOS_TYPE_CONTROL; ++i) {
	    if (param->so_prio >= qos_map[i].so_prio)
		prio_type = (bqos_type)i;
	}
	++count;
    }

    if (param->flags & BASE_QOS_PARAM_HAS_WMM) {
	for (i=0; i<=BASE_QOS_TYPE_CONTROL; ++i) {
	    if (param->wmm_prio >= qos_map[i].wmm_prio)
		wmm_type = (bqos_type)i;
	}
	++count;
    }

    if (count)
	*p_type = (bqos_type)((dscp_type + prio_type + wmm_type) / count);
    else
	*p_type = BASE_QOS_TYPE_BEST_EFFORT;

    return BASE_SUCCESS;
}

/* Apply QoS */
bstatus_t bsock_apply_qos( bsock_t sock,
				       bqos_type qos_type,
				       bqos_params *qos_params,
				       unsigned log_level,
				       const char *log_sender,
				       const char *sock_name)
{
    bstatus_t qos_type_rc = BASE_SUCCESS,
		qos_params_rc = BASE_SUCCESS;

    if (!log_sender)
	log_sender = THIS_FILE;
    if (!sock_name)
	sock_name = "socket";

    if (qos_type != BASE_QOS_TYPE_BEST_EFFORT) {
	qos_type_rc = bsock_set_qos_type(sock, qos_type);

	if (qos_type_rc != BASE_SUCCESS) {
	    bperror(log_level, log_sender,  qos_type_rc, 
		      "Error setting QoS type %d to %s", 
		      qos_type, sock_name);
	}
    }

    if (qos_params && qos_params->flags) {
	qos_params_rc = bsock_set_qos_params(sock, qos_params);
	if (qos_params_rc != BASE_SUCCESS) {
	    bperror(log_level, log_sender,  qos_params_rc, 
		      "Error setting QoS params (flags=%d) to %s", 
		      qos_params->flags, sock_name);
	    if (qos_type_rc != BASE_SUCCESS)
		return qos_params_rc;
	}
    } else if (qos_type_rc != BASE_SUCCESS)
	return qos_type_rc;

    return BASE_SUCCESS;
}


bstatus_t bsock_apply_qos2( bsock_t sock,
 				        bqos_type qos_type,
				        const bqos_params *qos_params,
				        unsigned log_level,
				        const char *log_sender,
				        const char *sock_name)
{
    bqos_params qos_params_buf, *qos_params_copy = NULL;

    if (qos_params) {
	bmemcpy(&qos_params_buf, qos_params, sizeof(*qos_params));
	qos_params_copy = &qos_params_buf;
    }

    return bsock_apply_qos(sock, qos_type, qos_params_copy,
			     log_level, log_sender, sock_name);
}
