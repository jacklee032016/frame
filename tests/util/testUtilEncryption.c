/* 
 *
 */

#include <testUtilTest.h>
#include <libBase.h>
#include <libUtil.h>


#if INCLUDE_ENCRYPTION_TEST

/*
 * Encryption algorithm tests.
 */


/*
 * SHA1 test from the original sha1.c source file.
 */
static char *sha1_test_data[] = {
    "abc",
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
    "A million repetitions of 'a'"
};
static char *sha1_test_results[] = {
    "A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D",
    "84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1",
    "34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F"
};


static void digest_to_hex(const buint8_t digest[BASE_SHA1_DIGEST_SIZE], 
			  char *output)
{
    int i,j;
    char *c = output;
    
    for (i = 0; i < BASE_SHA1_DIGEST_SIZE/4; i++) {
        for (j = 0; j < 4; j++) {
            sprintf(c,"%02X", digest[i*4+j] & 0xFF);
            c += 2;
        }
        sprintf(c, " ");
        c += 1;
    }
    *(c - 1) = '\0';
}

static int sha1_test1(void)
{
    enum { MILLION = 1000000 };
    int k;
    bsha1_context context;
    buint8_t digest[20];
    char output[80];
    bpool_t *pool;
    buint8_t *block;

    BASE_INFO("  SHA1 test vector 1 from sha1.c..");
    
    for (k = 0; k < 2; k++){ 
        bsha1_init(&context);
        bsha1_update(&context, (buint8_t*)sha1_test_data[k], 
		       bansi_strlen(sha1_test_data[k]));
        bsha1_final(&context, digest);
	digest_to_hex(digest, output);

        if (bansi_strcmp(output, sha1_test_results[k])) {
	    BASE_ERROR("    incorrect hash result on k=%d", k);
            return -10;
        }
    }

    /* million 'a' vector we feed separately */
    bsha1_init(&context);
    for (k = 0; k < MILLION; k++)
        bsha1_update(&context, (buint8_t*)"a", 1);
    bsha1_final(&context, digest);
    digest_to_hex(digest, output);
    if (strcmp(output, sha1_test_results[2])) {
	BASE_ERROR("    incorrect hash result!");
        return -20;
    }

    /* million 'a' test, using block */
    pool = bpool_create(mem, "sha1test", 256, 512, NULL);
    block = (buint8_t*)bpool_alloc(pool, MILLION);
    bmemset(block, 'a', MILLION);

    bsha1_init(&context);
    bsha1_update(&context, block, MILLION);
    bsha1_final(&context, digest);
    digest_to_hex(digest, output);
    if (strcmp(output, sha1_test_results[2])) {
	bpool_release(pool);
	BASE_ERROR( "    incorrect hash result for block update!");
        return -21;
    }

    /* verify that original buffer was not modified */
    for (k=0; k<MILLION; ++k) {
	if (block[k] != 'a') {
	    bpool_release(pool);
	    BASE_INFO("    block was modified!");
	    return -22;
	}
    }

    bpool_release(pool);

    /* success */
    return(0);
}


/*
 * SHA1 test from RFC 3174
 */
/*
 *  Define patterns for testing
 */
#define TEST1   "abc"
#define TEST2a  "abcdbcdecdefdefgefghfghighijhi"
#define TEST2b  "jkijkljklmklmnlmnomnopnopq"
#define TEST2   TEST2a TEST2b
#define TEST3   "a"
#define TEST4a  "01234567012345670123456701234567"
#define TEST4b  "01234567012345670123456701234567"
    /* an exact multiple of 512 bits */
#define TEST4   TEST4a TEST4b

static char *testarray[4] =
{
    TEST1,
    TEST2,
    TEST3,
    TEST4
};
static int repeatcount[4] = { 1, 1, 1000000, 10 };
static char *resultarray[4] =
{
    "A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D",
    "84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1",
    "34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F",
    "DEA356A2 CDDD90C7 A7ECEDC5 EBB56393 4F460452"
};

static int sha1_test2(void)
{
    bsha1_context sha;
    int i;
    buint8_t digest[20];
    char char_digest[64];

    BASE_INFO("  SHA1 test vector 2 from rfc 3174..");

    for(i = 0; i < 4; ++i) {
	int j;

        bsha1_init(&sha);

        for(j = 0; j < repeatcount[i]; ++j) {
            bsha1_update(&sha,
			   (const buint8_t *) testarray[i],
			   bansi_strlen(testarray[i]));
        }

        bsha1_final(&sha, digest);

	digest_to_hex(digest, char_digest);
	if (bansi_strcmp(char_digest, resultarray[i])) {
	    BASE_ERROR("    digest mismatch in test %d", i);
	    return -40;
	}
    }

    return 0;
}


/*
 * HMAC-MD5 and HMAC-SHA1 test vectors from RFC 2202
 */
struct rfc2202_test
{
    char       *key;
    unsigned	key_len;
    char       *input;
    unsigned	input_len;
    char       *md5_digest;
    char       *sha1_digest;
};


struct rfc2202_test rfc2202_test_vector[] = 
{
    {
	"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
	16,
	"Hi There",
	8,
	"\x92\x94\x72\x7a\x36\x38\xbb\x1c\x13\xf4\x8e\xf8\x15\x8b\xfc\x9d",
	NULL
    },
    {
	"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
	"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
	20,
	"Hi There",
	8,
	NULL,
	"\xb6\x17\x31\x86\x55\x05\x72\x64\xe2\x8b\xc0\xb6\xfb\x37\x8c\x8e\xf1\x46\xbe\x00"
    },
    {
	"Jefe",
	4,
	"what do ya want for nothing?",
	28,
	"\x75\x0c\x78\x3e\x6a\xb0\xb5\x03\xea\xa8\x6e\x31\x0a\x5d\xb7\x38",
	"\xef\xfc\xdf\x6a\xe5\xeb\x2f\xa2\xd2\x74\x16\xd5\xf1\x84\xdf\x9c\x25\x9a\x7c\x79"
    },
    {
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa",
	16,
	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd",
	50,
	"\x56\xbe\x34\x52\x1d\x14\x4c\x88\xdb\xb8\xc7\x33\xf0\xe8\xb3\xf6",
	NULL
    },
    {
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
	20,
	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd",
	50,
	NULL,
	"\x12\x5d\x73\x42\xb9\xac\x11\xcd\x91\xa3\x9a\xf4\x8a\xa1\x7b\x4f\x63\xf1\x75\xd3"
    },
    {
	"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19",
	25,
	"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd",
	50,
	"\x69\x7e\xaf\x0a\xca\x3a\x3a\xea\x3a\x75\x16\x47\x46\xff\xaa\x79",
	"\x4c\x90\x07\xf4\x02\x62\x50\xc6\xbc\x84\x14\xf9\xbf\x50\xc8\x6c\x2d\x72\x35\xda"
    },
    {
	"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"
	"\x0c\x0c\x0c\x0c\x0c\x0c",
	16,
	"Test With Truncation",
	20,
	"\x56\x46\x1e\xf2\x34\x2e\xdc\x00\xf9\xba\xb9\x95\x69\x0e\xfd\x4c",
	NULL
    },
    {
	"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"
	"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c",
	20,
	"Test With Truncation",
	20,
	NULL,
	"\x4c\x1a\x03\x42\x4b\x55\xe0\x7f\xe7\xf2\x7b\xe1\xd5\x8b\xb9\x32\x4a\x9a\x5a\x04"
    },
    {
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
	80,
	"Test Using Larger Than Block-Size Key - Hash Key First",
	54,
	"\x6b\x1a\xb7\xfe\x4b\xd7\xbf\x8f\x0b\x62\xe6\xce\x61\xb9\xd0\xcd",
	"\xaa\x4a\xe5\xe1\x52\x72\xd0\x0e\x95\x70\x56\x37\xce\x8a\x3b\x55\xed\x40\x21\x12"
    },
    {
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
	80,
	"Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data",
	73,
	"\x6f\x63\x0f\xad\x67\xcd\xa0\xee\x1f\xb1\xf5\x62\xdb\x3a\xa5\x3e",
	"\xe8\xe9\x9d\x0f\x45\x23\x7d\x78\x6d\x6b\xba\xa7\x96\x5c\x78\x08\xbb\xff\x1a\x91"
    }
};


static int rfc2202_test(void)
{
    unsigned i;

    BASE_INFO("  verifying test vectors from rfc 2202..");

    /* Verify that test vectors are valid */
    for (i=0; i<BASE_ARRAY_SIZE(rfc2202_test_vector); ++i) {
	BASE_ASSERT_RETURN(bansi_strlen(rfc2202_test_vector[i].input) == 
			    rfc2202_test_vector[i].input_len, -50);
	BASE_ASSERT_RETURN(bansi_strlen(rfc2202_test_vector[i].key) == 
			    rfc2202_test_vector[i].key_len, -52);
	BASE_ASSERT_RETURN(rfc2202_test_vector[i].md5_digest==NULL ||
			    bansi_strlen(rfc2202_test_vector[i].md5_digest)<=16,
			    -54);
	BASE_ASSERT_RETURN(rfc2202_test_vector[i].sha1_digest==NULL ||
			    bansi_strlen(rfc2202_test_vector[i].sha1_digest)<=20,
			    -56);
    }

    /* Test HMAC-MD5 */
    BASE_INFO("  HMAC-MD5 test vectors from rfc 2202..");
    for (i=0; i<BASE_ARRAY_SIZE(rfc2202_test_vector); ++i) {
	buint8_t digest_buf[18], *digest;

	if (rfc2202_test_vector[i].md5_digest == NULL)
	    continue;

	digest_buf[0] = '\0';
	digest_buf[17] = '\0';

	digest = digest_buf+1;

	bhmac_md5((buint8_t*)rfc2202_test_vector[i].input, 
		    rfc2202_test_vector[i].input_len,
		    (buint8_t*)rfc2202_test_vector[i].key, 
		    rfc2202_test_vector[i].key_len,
		    digest);

	/* Check for overwrites */
	if (digest_buf[0] != '\0' || digest_buf[17] != '\0') {
	    BASE_ERROR("    error: overwriting outside buffer on test %d", i);
	    return -60;
	}

	/* Compare digest */
	if (bmemcmp(rfc2202_test_vector[i].md5_digest, digest, 16)) {
	    BASE_ERROR("    error: digest mismatch on test %d", i);
	    return -65;
	}
    }

    /* Test HMAC-SHA1 */
    BASE_INFO("  HMAC-SHA1 test vectors from rfc 2202..");
    for (i=0; i<BASE_ARRAY_SIZE(rfc2202_test_vector); ++i) {
	buint8_t digest_buf[22], *digest;

	if (rfc2202_test_vector[i].sha1_digest == NULL)
	    continue;

	digest_buf[0] = '\0';
	digest_buf[21] = '\0';

	digest = digest_buf+1;

	bhmac_sha1((buint8_t*)rfc2202_test_vector[i].input, 
		     rfc2202_test_vector[i].input_len,
		     (buint8_t*)rfc2202_test_vector[i].key, 
		     rfc2202_test_vector[i].key_len,
		     digest);

	/* Check for overwrites */
	if (digest_buf[0] != '\0' || digest_buf[21] != '\0') {
	    BASE_ERROR("    error: overwriting outside buffer on test %d", i);
	    return -70;
	}

	/* Compare digest */
	if (bmemcmp(rfc2202_test_vector[i].sha1_digest, digest, 20)) {
	    BASE_ERROR("    error: digest mismatch on test %d", i);
	    return -75;
	}
    }


    /* Success */
    return 0;
}

/* CRC32 test data, generated from crc32 test on a Linux box */
struct crc32_test_t
{
    char	    *input;
    buint32_t	     crc;
} crc32_test_data[] = 
{
    {
	"",
	0x0
    },
    {
	"Hello World",
	0x4a17b156
    },
    {
	/* Something read from /dev/random */
	"\x21\x21\x98\x10\x62\x59\xbc\x58\x42\x24\xe5\xf3\x92\x0a\x68\x3c\xa7\x67\x73\xc3",
	0x506693be
    },
    {
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
	0xcab11777
    },
    {
	"123456789",
	0xCBF43926
    }
};

/*
 * CRC32 test
 */
static int crc32_test(void)
{
    unsigned i;

    BASE_INFO("  crc32 test..");

    /* testing bcrc32_calc */
    for (i=0; i<BASE_ARRAY_SIZE(crc32_test_data); ++i) {
	buint32_t crc;

	crc = bcrc32_calc((buint8_t*)crc32_test_data[i].input,
			    bansi_strlen(crc32_test_data[i].input));
	if (crc != crc32_test_data[i].crc) {
	    BASE_ERROR("    error: crc mismatch on test %d", i);
	    return -80;
	}
    }

    /* testing incremental CRC32 calculation */
    for (i=0; i<BASE_ARRAY_SIZE(crc32_test_data); ++i) {
	bcrc32_context ctx;
	buint32_t crc0, crc1;
	bsize_t len;

	len = bansi_strlen(crc32_test_data[i].input);
	crc0 = bcrc32_calc((buint8_t*)crc32_test_data[i].input, len);

	bcrc32_init(&ctx);
	bcrc32_update(&ctx, (buint8_t*)crc32_test_data[i].input,
			len / 2);

	if (len/2 > 0) {
	    bcrc32_update(&ctx, (buint8_t*)crc32_test_data[i].input + len/2,
			    len - len/2);
	}

	crc1 = bcrc32_final(&ctx);

	if (crc0 != crc1) {
	    BASE_ERROR("    error: crc algorithm error on test %d", i);
	    return -85;
	}

    }
    return 0;
}

enum
{
    ENCODE = 1,
    DECODE = 2,
    ENCODE_DECODE = 3
};

/*
 * Base64 test vectors (RFC 4648)
 */
static struct base64_test_vec
{
    const char *base256;
    const char *base64;
    unsigned flag;
} base64_test_vec[] = 
{
    {
	"",
	"",
	ENCODE_DECODE
    },
    {
	"f",
	"Zg==",
	ENCODE_DECODE
    },
    {
	"fo",
	"Zm8=",
	ENCODE_DECODE
    },
    {
	"foo",
	"Zm9v",
	ENCODE_DECODE
    },
    {
	"foob",
	"Zm9vYg==",
	ENCODE_DECODE
    },
    {
	"fooba",
	"Zm9vYmE=",
	ENCODE_DECODE
    },
    {
	"foobar",
	"Zm9vYmFy",
	ENCODE_DECODE
    },
    {
	"\x14\xfb\x9c\x03\xd9\x7e",
	"FPucA9l+",
	ENCODE_DECODE
    },
    {
	"\x14\xfb\x9c\x03\xd9",
	"FPucA9k=",
	ENCODE_DECODE
    },
    {
	"\x14\xfb\x9c\x03",
	"FPucAw==",
	ENCODE_DECODE
    },
    /* with whitespaces */
    {
	"foobar",
	"Zm9v\r\nYmFy",
	DECODE
    },
    {
    	"foobar",
    	"\nZ\r\nm 9\tv\nYm\nF\ny\n",
    	DECODE
    },
};


static int base64_test(void)
{
    unsigned i;
    char output[80];
    bstatus_t rc;

    BASE_INFO("  base64 test..");

    for (i=0; i<BASE_ARRAY_SIZE(base64_test_vec); ++i) {
	bstr_t input;
	int out_len;

	/* Encode test */
	if (base64_test_vec[i].flag & ENCODE) {
	    out_len = sizeof(output);
	    rc = bbase64_encode((buint8_t*)base64_test_vec[i].base256,
				  (int)strlen(base64_test_vec[i].base256),
				  output, &out_len);
	    if (rc != BASE_SUCCESS)
		return -90;

	    if (out_len != (int)strlen(base64_test_vec[i].base64))
		return -91;

	    output[out_len] = '\0';
	    if (strcmp(output, base64_test_vec[i].base64) != 0)
		return -92;
	}

	/* Decode test */
	if (base64_test_vec[i].flag & DECODE) {
	    out_len = sizeof(output);
	    input.ptr = (char*)base64_test_vec[i].base64;
	    input.slen = strlen(base64_test_vec[i].base64);
	    rc = bbase64_decode(&input, (buint8_t*)output, &out_len);
	    if (rc != BASE_SUCCESS)
		return -95;

	    if (out_len != (int)strlen(base64_test_vec[i].base256))
		return -96;

	    output[out_len] = '\0';

	    if (strcmp(output, base64_test_vec[i].base256) != 0)
		return -97;
	}
    }

    return 0;
}


int encryption_test()
{
    int rc;

    rc = base64_test();
    if (rc != 0)
	return rc;

    rc = sha1_test1();
    if (rc != 0)
	return rc;

    rc = sha1_test2();
    if (rc != 0)
	return rc;

    rc = rfc2202_test();
    if (rc != 0)
	return rc;

    rc = crc32_test();
    if (rc != 0)
	return rc;

    return 0;
}

static void crc32_update(bcrc32_context *c, const buint8_t *data,
			 bsize_t nbytes)
{
    bcrc32_update(c, data, nbytes);
}

static void crc32_final(bcrc32_context *ctx, buint32_t *digest)
{
    *digest = bcrc32_final(ctx);
}

int encryption_benchmark()
{
    bpool_t *pool;
    buint8_t *input;
    union {
	bmd5_context md5_context;
	bsha1_context sha1_context;
    } context;
    buint8_t digest[32];
    bsize_t input_len;
    struct algorithm
    {
	const char *name;
	void (*init_context)(void*);
	void (*update)(void*, const buint8_t*, unsigned);
	void (*final)(void*, void*);
	buint32_t t;
    } algorithms[] = 
    {
	{
	    "MD5  ",
	    (void (*)(void*))&bmd5_init,
	    (void (*)(void*, const buint8_t*, unsigned))&bmd5_update,
	    (void (*)(void*, void*))&bmd5_final
	},
	{
	    "SHA1 ",
	    (void (*)(void*))&bsha1_init,
	    (void (*)(void*, const buint8_t*, unsigned))&bsha1_update,
	    (void (*)(void*, void*))&bsha1_final
	},
	{
	    "CRC32",
	    (void (*)(void*))&bcrc32_init,
	    (void (*)(void*, const buint8_t*, unsigned))&crc32_update,
	    (void (*)(void*, void*))&crc32_final
	}
    };
#if defined(BASE_LIB_DEBUG) && BASE_LIB_DEBUG!=0
    enum { LOOP = 1000 };
#else
    enum { LOOP = 10000 };
#endif
    unsigned i;
    double total_len;

    input_len = 2048;
    total_len = (unsigned)input_len * LOOP;
    pool = bpool_create(mem, "enc", input_len+256, 0, NULL);
    if (!pool)
	return BASE_ENOMEM;

    input = (buint8_t*)bpool_alloc(pool, input_len);
    bmemset(input, '\xaa', input_len);
    
    BASE_INFO("  feeding %d Mbytes of data", (unsigned)(total_len/1024/1024));

    /* Dry run */
    for (i=0; i<BASE_ARRAY_SIZE(algorithms); ++i) {
	algorithms[i].init_context(&context);
	algorithms[i].update(&context, input, (unsigned)input_len);
	algorithms[i].final(&context, digest);
    }

    /* Run */
    for (i=0; i<BASE_ARRAY_SIZE(algorithms); ++i) {
	int j;
	btimestamp t1, t2;

	bTimeStampGet(&t1);
	algorithms[i].init_context(&context);
	for (j=0; j<LOOP; ++j) {
	    algorithms[i].update(&context, input, (unsigned)input_len);
	}
	algorithms[i].final(&context, digest);
	bTimeStampGet(&t2);

	algorithms[i].t = belapsed_usec(&t1, &t2);
    }

    /* Results */
    for (i=0; i<BASE_ARRAY_SIZE(algorithms); ++i) {
	double bytes;

	bytes = (total_len * 1000000 / algorithms[i].t);
	BASE_INFO("    %s:%8d usec (%3d.%03d Mbytes/sec)",
		   algorithms[i].name, algorithms[i].t,
		   (unsigned)(bytes / 1024 / 1024),
		   ((unsigned)(bytes) % (1024 * 1024)) / 1024);
    }

    return 0;
}

#endif

