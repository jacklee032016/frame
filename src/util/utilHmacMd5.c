/* 
 */
#include <utilHmacMd5.h>
#include <baseString.h>


void bhmac_md5_init(bhmac_md5_context *hctx, 
			      const buint8_t *key, unsigned key_len)
{
    buint8_t k_ipad[64];
    buint8_t tk[16];
    int i;

    /* if key is longer than 64 bytes reset it to key=MD5(key) */
    if (key_len > 64) {
        bmd5_context      tctx;

        bmd5_init(&tctx);
        bmd5_update(&tctx, key, key_len);
        bmd5_final(&tctx, tk);

        key = tk;
        key_len = 16;
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
     * perform inner MD5
     */
    bmd5_init(&hctx->context);
    bmd5_update(&hctx->context, k_ipad, 64);

}

void bhmac_md5_update(bhmac_md5_context *hctx,
				 const buint8_t *input, 
				 unsigned input_len)
{
    bmd5_update(&hctx->context, input, input_len);
}

void bhmac_md5_final(bhmac_md5_context *hctx,
				buint8_t digest[16])
{
    bmd5_final(&hctx->context, digest);

    /*
     * perform outer MD5
     */
    bmd5_init(&hctx->context);
    bmd5_update(&hctx->context, hctx->k_opad, 64);
    bmd5_update(&hctx->context, digest, 16);
    bmd5_final(&hctx->context, digest);
}

void bhmac_md5( const buint8_t *input, unsigned input_len, 
			  const buint8_t *key, unsigned key_len, 
			  buint8_t digest[16] )
{
    bhmac_md5_context ctx;

    bhmac_md5_init(&ctx, key, key_len);
    bhmac_md5_update(&ctx, input, input_len);
    bhmac_md5_final(&ctx, digest);
}

