/* 
 *
 */
#include <utilHmacSha1.h>
#include <baseString.h>


void bhmac_sha1_init(bhmac_sha1_context *hctx, 
			       const buint8_t *key, unsigned key_len)
{
    buint8_t k_ipad[64];
    buint8_t tk[20];
    unsigned i;

    /* if key is longer than 64 bytes reset it to key=SHA1(key) */
    if (key_len > 64) {
        bsha1_context      tctx;

        bsha1_init(&tctx);
        bsha1_update(&tctx, key, key_len);
        bsha1_final(&tctx, tk);

        key = tk;
        key_len = 20;
    }

    /*
     * HMAC = H(K XOR opad, H(K XOR ipad, text))
     */

    /* start out by storing key in pads */
    bbzero( k_ipad, sizeof(k_ipad));
    bbzero( hctx->k_opad, sizeof(hctx->k_opad));
    bmemcpy( k_ipad, key, key_len);
    bmemcpy( hctx->k_opad, key, key_len);

    /* XOR key with ipad and opad values */
    for (i=0; i<64; i++) {
        k_ipad[i] ^= 0x36;
        hctx->k_opad[i] ^= 0x5c;
    }
    /*
     * perform inner SHA1
     */
    bsha1_init(&hctx->context);
    bsha1_update(&hctx->context, k_ipad, 64);
}

void bhmac_sha1_update(bhmac_sha1_context *hctx,
				 const buint8_t *input, unsigned input_len)
{
    bsha1_update(&hctx->context, input, input_len);
}

void bhmac_sha1_final(bhmac_sha1_context *hctx,
				buint8_t digest[20])
{
    bsha1_final(&hctx->context, digest);

    /*
     * perform outer SHA1
     */
    bsha1_init(&hctx->context);
    bsha1_update(&hctx->context, hctx->k_opad, 64);
    bsha1_update(&hctx->context, digest, 20);
    bsha1_final(&hctx->context, digest);
}

void bhmac_sha1(const buint8_t *input, unsigned input_len, 
			  const buint8_t *key, unsigned key_len, 
			  buint8_t digest[20] )
{
    bhmac_sha1_context ctx;

    bhmac_sha1_init(&ctx, key, key_len);
    bhmac_sha1_update(&ctx, input, input_len);
    bhmac_sha1_final(&ctx, digest);
}

