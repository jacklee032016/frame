/* 
 *
 */

/*
 * THIS FILE IS INCLUDED BY scanner.c.
 * DO NOT COMPILE THIS FILE ALONE!
 */

void bcis_buf_init( bcis_buf_t *cis_buf)
{
    bbzero(cis_buf->cis_buf, sizeof(cis_buf->cis_buf));
    cis_buf->use_mask = 0;
}

bstatus_t bcis_init(bcis_buf_t *cis_buf, bcis_t *cis)
{
    unsigned i;

    cis->cis_buf = cis_buf->cis_buf;

    for (i=0; i<BASE_CIS_MAX_INDEX; ++i) {
        if ((cis_buf->use_mask & (1 << i)) == 0) {
            cis->cis_id = i;
	    cis_buf->use_mask |= (1 << i);
            return BASE_SUCCESS;
        }
    }

    cis->cis_id = BASE_CIS_MAX_INDEX;
    return BASE_ETOOMANY;
}

bstatus_t bcis_dup( bcis_t *new_cis, bcis_t *existing)
{
    bstatus_t status;
    unsigned i;

    /* Warning: typecasting here! */
    status = bcis_init((bcis_buf_t*)existing->cis_buf, new_cis);
    if (status != BASE_SUCCESS)
        return status;

    for (i=0; i<256; ++i) {
        if (BASE_CIS_ISSET(existing, i))
            BASE_CIS_SET(new_cis, i);
        else
            BASE_CIS_CLR(new_cis, i);
    }

    return BASE_SUCCESS;
}

