/* 
 *
 */

static int decode_test(void)
{
    /* Invalid message type */

    /* Short message */

    /* Long, random message */

    /* Message length in header is shorter */

    /* Message length in header is longer */

    /* Invalid magic */

    /* Attribute length is not valid */

    /* Unknown mandatory attribute type should generate error */

    /* Unknown but non-mandatory should be okay */

    /* String/binary attribute length is larger than the message */

    /* Valid message with MESSAGE-INTEGRITY */

    /* Valid message with FINGERPRINT */

    /* Valid message with MESSAGE-INTEGRITY and FINGERPRINT */

    /* Another attribute not FINGERPRINT exists after MESSAGE-INTEGRITY */

    /* Another attribute exists after FINGERPRINT */

    return 0;
}

static int decode_verify(void)
{
    /* Decode all attribute types */
    return 0;
}

static int auth_test(void)
{
    /* REALM and USERNAME is present, but MESSAGE-INTEGRITY is not present.
     * For short term, must with reply 401 without REALM.
     * For long term, must reply with 401 with REALM.
     */

    /* USERNAME is not present, server must respond with 432 (Missing
     * Username).
     */

    /* If long term credential is wanted and REALM is not present, server 
     * must respond with 434 (Missing Realm) 
     */

    /* If REALM doesn't match, server must respond with 434 (Missing Realm)
     * too, containing REALM and NONCE attribute.
     */

    /* When long term authentication is wanted and NONCE is NOT present,
     * server must respond with 435 (Missing Nonce), containing REALM and
     * NONCE attribute.
     */

    /* Simulate 438 (Stale Nonce) */
    
    /* Simulate 436 (Unknown Username) */

    /* When server wants to use short term credential, but request has
     * REALM, reject with .... ???
     */

    /* Invalid HMAC */

    /* Valid static short term, without NONCE */

    /* Valid static short term, WITH NONCE */

    /* Valid static long term (with NONCE */

    /* Valid dynamic short term (without NONCE) */

    /* Valid dynamic short term (with NONCE) */

    /* Valid dynamic long term (with NONCE) */

    return 0;
}


int stun_test(void)
{
    decode_verify();
    decode_test();
    auth_test();
    return 0;
}

