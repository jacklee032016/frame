/* 
 *
 */
#include <utilStunSimple.h>
#include <utilErrno.h>
#include <basePool.h>
#include <baseLog.h>
#include <baseSock.h>
#include <baseOs.h>

bstatus_t utilstun_create_bind_req( bpool_t *pool, 
					     void **msg, bsize_t *len,
					     buint32_t id_hi, 
					     buint32_t id_lo)
{
	utilstun_msg_hdr *hdr;

	BASE_CHECK_STACK();


	hdr = BASE_POOL_ZALLOC_T(pool, utilstun_msg_hdr);
	if (!hdr)
	return BASE_ENOMEM;

	hdr->type = bhtons(USTUN_BINDING_REQUEST);
	hdr->tsx[2] = bhtonl(id_hi);
	hdr->tsx[3] = bhtonl(id_lo);
	*msg = hdr;
	*len = sizeof(utilstun_msg_hdr);

	return BASE_SUCCESS;
}

bstatus_t utilstun_parse_msg( void *buf, bsize_t buf_len, 
				      utilstun_msg *msg)
{
    buint16_t msg_type, msg_len;
    char *p_attr;

    BASE_CHECK_STACK();

    msg->hdr = (utilstun_msg_hdr*)buf;
    msg_type = bntohs(msg->hdr->type);

    switch (msg_type) {
    case USTUN_BINDING_REQUEST:
    case USTUN_BINDING_RESPONSE:
    case USTUN_BINDING_ERROR_RESPONSE:
    case USTUN_SHARED_SECRET_REQUEST:
    case USTUN_SHARED_SECRET_RESPONSE:
    case USTUN_SHARED_SECRET_ERROR_RESPONSE:
	break;
    default:
	BASE_ERROR("Error: unknown msg type %d", msg_type);
	return UTIL_ESTUNINMSGTYPE;
    }

    msg_len = bntohs(msg->hdr->length);
    if (msg_len != buf_len - sizeof(utilstun_msg_hdr)) {
	BASE_ERROR("Error: invalid msg_len %d (expecting %d)", msg_len, buf_len - sizeof(utilstun_msg_hdr));
	return UTIL_ESTUNINMSGLEN;
    }

    msg->attr_count = 0;
    p_attr = (char*)buf + sizeof(utilstun_msg_hdr);

    while (msg_len > 0) {
	utilstun_attr_hdr **attr = &msg->attr[msg->attr_count];
	buint32_t len;
	buint16_t attr_type;

	*attr = (utilstun_attr_hdr*)p_attr;
	len = bntohs((buint16_t) ((*attr)->length)) + sizeof(utilstun_attr_hdr);
	len = (len + 3) & ~3;

	if (msg_len < len) {
	    BASE_ERROR("Error: length mismatch in attr %d", msg->attr_count);
	    return UTIL_ESTUNINATTRLEN;
	}

	attr_type = bntohs((*attr)->type);
	if (attr_type > USTUN_ATTR_REFLECTED_FROM &&
	    attr_type != USTUN_ATTR_XOR_MAPPED_ADDR)
	{
	    BASE_WARN("Warning: unknown attr type %x in attr %d. Attribute was ignored.", attr_type, msg->attr_count);
	}

	msg_len = (buint16_t)(msg_len - len);
	p_attr += len;
	++msg->attr_count;
    }

    return BASE_SUCCESS;
}

void* utilstun_msg_find_attr( utilstun_msg *msg, utilstun_attr_type t)
{
    int i;

    BASE_CHECK_STACK();

    for (i=0; i<msg->attr_count; ++i) {
	utilstun_attr_hdr *attr = msg->attr[i];
	if (bntohs(attr->type) == t)
	    return attr;
    }

    return 0;
}
