/*
 *
 */

/*
 * THIS FILE IS INCLUDED BY scanner.c.
 * DO NOT COMPILE THIS FILE ALONE!
 */


void bcis_buf_init( bcis_buf_t *cis_buf)
{
    /* Do nothing. */
    BASE_UNUSED_ARG(cis_buf);
}

bstatus_t bcis_init(bcis_buf_t *cis_buf, bcis_t *cis)
{
    BASE_UNUSED_ARG(cis_buf);
    bbzero(cis->cis_buf, sizeof(cis->cis_buf));
    return BASE_SUCCESS;
}

bstatus_t bcis_dup( bcis_t *new_cis, bcis_t *existing)
{
    bmemcpy(new_cis, existing, sizeof(bcis_t));
    return BASE_SUCCESS;
}


