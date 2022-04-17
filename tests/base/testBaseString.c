/* 
 *
 */
#include <baseString.h>
#include <basePool.h>
#include <baseLog.h>
#include <baseOs.h>
#include "testBaseTest.h"

/**
 * \page page_baselib_string_test Test: String
 *
 * This file provides implementation of \b string_test(). It tests the
 * functionality of the string API.
 *
 * \section sleep_test_sec Scope of the Test
 *
 * API tested:
 *  - bstr()
 *  - bstrcmp()
 *  - bstrcmp2()
 *  - bstricmp()
 *  - bstrlen()
 *  - bstrncmp()
 *  - bstrnicmp()
 *  - bstrchr()
 *  - bstrdup()
 *  - bstrdup2()
 *  - bstrcpy()
 *  - bstrcat()
 *  - bstrtrim()
 *  - butoa()
 *  - bstrtoul()
 *  - bstrtoul2()
 *  - bcreate_random_string()
 *  - ... and mode..
 *
 */

#if INCLUDE_STRING_TEST

#ifdef _MSC_VER
#   pragma warning(disable: 4204)
#endif

#define HELLO_WORLD	"Hello World"
#define HELLO_WORLD_LEN	11
#define JUST_HELLO	"Hello"
#define JUST_HELLO_LEN	5
#define UL_VALUE	3456789012UL

#if 1
/* See if both integers have the same sign */
BASE_INLINE_SPECIFIER int cmp(const char *expr, int i, int j)
{
    int r = !((i>0 && j>0) || (i<0 && j<0) || (i==0 && j==0));
    if (r) {
	BASE_ERROR(" error: %s: expecting %d, got %d", expr, j, i);
    }
    return r;
}
#else
/* For strict comparison, must be equal */
BASE_INLINE_SPECIFIER int cmp(const char *expr, int i, int j)
{
    BASE_UNUSED_ARG(expr);
    return i!=j;
}
#endif

#define C(expr, res)	cmp(#expr, expr, res)

static int stricmp_test(void)
{
/* This specificly tests and benchmark bstricmp(), bstricmp_alnum().
 * In addition, it also tests bstricmp2(), bstrnicmp(), and 
 * bstrnicmp2().
 */
#define STRTEST(res,res2,S1,S2,code)	\
	    do { \
		s1.ptr=S1; s1.slen=(S1)?len:0; \
		s2.ptr=S2; s2.slen=(S2)?len:0; \
		bTimeStampGet(&t1); \
	        if (C(bstricmp(&s1,&s2),res)) return code; \
		bTimeStampGet(&t2); \
		bsub_timestamp(&t2, &t1); \
		badd_timestamp(&e1, &t2); \
		bTimeStampGet(&t1); \
	        if (C(bstricmp_alnum(&s1,&s2),res)) return code-1; \
		bTimeStampGet(&t2); \
		bsub_timestamp(&t2, &t1); \
		badd_timestamp(&e2, &t2); \
		if (C(bstricmp2(&s1,S2),res2)) return code*10; \
		if (C(bstrnicmp(&s1,&s2,len),res)) return code*100; \
		if (C(bstrnicmp2(&s1,S2,len),res)) return code*1000; \
	    } while (0)

    char *buf;
    bstr_t s1, s2;
    btimestamp t1, t2, e1, e2, zero;
    buint32_t c1, c2;
    int len;

    e1.u32.hi = e1.u32.lo = e2.u32.hi = e2.u32.lo = 0;

    bthreadSleepMs(0);

#define SNULL 0

    /* Compare empty strings. */
    len=0;
    STRTEST( 0, 0, "","",-500);
    STRTEST( 0, 0, SNULL,"",-502);
    STRTEST( 0, 0, "",SNULL,-504);
    STRTEST( 0, 0, SNULL,SNULL,-506);
    STRTEST( 0, -1, "hello","world",-508);

    /* equal, length=1 
     * use buffer to simulate non-aligned string.
     */
    buf = "a""A";
    len=1;
    STRTEST( 0,  -1, "a",buf+0,-510);
    STRTEST( 0,  0, "a",buf+1,-512);
    STRTEST(-1, -1, "O", "P", -514);
    STRTEST(-1, -1, SNULL, "a", -516);
    STRTEST( 1,  1, "a", SNULL, -518);

    /* equal, length=2 
     * use buffer to simulate non-aligned string.
     */
    buf = "aa""Aa""aA""AA";
    len=2;
    STRTEST( 0, -1, "aa",buf+0,-520);
    STRTEST( 0, -1, "aa",buf+2,-522);
    STRTEST( 0, -1, "aa",buf+4,-524);
    STRTEST( 0, 0, "aa",buf+6,-524);

    /* equal, length=3 
     * use buffer to simulate non-aligned string.
     */
    buf = "aaa""Aaa""aAa""aaA""AAa""aAA""AaA""AAA";
    len=3;
    STRTEST( 0, -1, "aaa",buf+0,-530);
    STRTEST( 0, -1, "aaa",buf+3,-532);
    STRTEST( 0, -1, "aaa",buf+6,-534);
    STRTEST( 0, -1, "aaa",buf+9,-536);
    STRTEST( 0, -1, "aaa",buf+12,-538);
    STRTEST( 0, -1, "aaa",buf+15,-540);
    STRTEST( 0, -1, "aaa",buf+18,-542);
    STRTEST( 0, 0, "aaa",buf+21,-534);

    /* equal, length=4 */
    len=4;
    STRTEST( 0, 0, "aaaa","aaaa",-540);
    STRTEST( 0, 0, "aaaa","Aaaa",-542);
    STRTEST( 0, 0, "aaaa","aAaa",-544);
    STRTEST( 0, 0, "aaaa","aaAa",-546);
    STRTEST( 0, 0, "aaaa","aaaA",-548);
    STRTEST( 0, 0, "aaaa","AAaa",-550);
    STRTEST( 0, 0, "aaaa","aAAa",-552);
    STRTEST( 0, 0, "aaaa","aaAA",-554);
    STRTEST( 0, 0, "aaaa","AaAa",-556);
    STRTEST( 0, 0, "aaaa","aAaA",-558);
    STRTEST( 0, 0, "aaaa","AaaA",-560);
    STRTEST( 0, 0, "aaaa","AAAa",-562);
    STRTEST( 0, 0, "aaaa","aAAA",-564);
    STRTEST( 0, 0, "aaaa","AAaA",-566);
    STRTEST( 0, 0, "aaaa","AaAA",-568);
    STRTEST( 0, 0, "aaaa","AAAA",-570);

    /* equal, length=5 */
    buf = "aaaAa""AaaaA""AaAaA""AAAAA";
    len=5;
    STRTEST( 0, -1, "aaaaa",buf+0,-580);
    STRTEST( 0, -1, "aaaaa",buf+5,-582);
    STRTEST( 0, -1, "aaaaa",buf+10,-584);
    STRTEST( 0, 0, "aaaaa",buf+15,-586);

    /* not equal, length=1 */
    len=1;
    STRTEST( -1, -1, "a", "b", -600);

    /* not equal, length=2 */
    buf = "ab""ba";
    len=2;
    STRTEST( -1, -1, "aa", buf+0, -610);
    STRTEST( -1, -1, "aa", buf+2, -612);

    /* not equal, length=3 */
    buf = "aab""aba""baa";
    len=3;
    STRTEST( -1, -1, "aaa", buf+0, -620);
    STRTEST( -1, -1, "aaa", buf+3, -622);
    STRTEST( -1, -1, "aaa", buf+6, -624);

    /* not equal, length=4 */
    buf = "aaab""aaba""abaa""baaa";
    len=4;
    STRTEST( -1, -1, "aaaa", buf+0, -630);
    STRTEST( -1, -1, "aaaa", buf+4, -632);
    STRTEST( -1, -1, "aaaa", buf+8, -634);
    STRTEST( -1, -1, "aaaa", buf+12, -636);

    /* not equal, length=5 */
    buf="aaaab""aaaba""aabaa""abaaa""baaaa";
    len=5;
    STRTEST( -1, -1, "aaaaa", buf+0, -640);
    STRTEST( -1, -1, "aaaaa", buf+5, -642);
    STRTEST( -1, -1, "aaaaa", buf+10, -644);
    STRTEST( -1, -1, "aaaaa", buf+15, -646);
    STRTEST( -1, -1, "aaaaa", buf+20, -648);

    zero.u32.hi = zero.u32.lo = 0;
    c1 = belapsed_cycle(&zero, &e1);
    c2 = belapsed_cycle(&zero, &e2);

    if (c1 < c2) {
	BASE_INFO("  info: bstricmp_alnum is slower than bstricmp!");
	//return -700;
    }

    /* Avoid division by zero */
    if (c2 == 0) c2=1;
    
    BASE_INFO("  time: stricmp=%u, stricmp_alnum=%u (speedup=%d.%02dx)", 
		   c1, c2,
		   (c1 * 100 / c2) / 100,
		   (c1 * 100 / c2) % 100);
    return 0;
#undef STRTEST
}

/* This tests bstrcmp(), bstrcmp2(), bstrncmp(), bstrncmp2() */
static int strcmp_test(void)
{
#define STR_TEST(res,S1,S2,code)    \
	    do { \
		s1.ptr=S1; s1.slen=S1?len:0; \
		s2.ptr=S2; s2.slen=S2?len:0; \
	        if (C(bstrcmp(&s1,&s2),res)) return code; \
		if (C(bstrcmp2(&s1,S2),res)) return code-1; \
		if (C(bstrncmp(&s1,&s2,len),res)) return code-2; \
		if (C(bstrncmp2(&s1,S2,len),res)) return code-3; \
	    } while (0)

    bstr_t s1, s2;
    int len;
    
    /* Test with length == 0 */
    len=0;
    STR_TEST(0, "", "", -400);
    STR_TEST(0, SNULL, "", -405);
    STR_TEST(0, "", SNULL, -410);
    STR_TEST(0, SNULL, SNULL, -415);
    STR_TEST(0, "hello", "", -420);
    STR_TEST(0, "hello", SNULL, -425);

    /* Test with length != 0 */
    len = 2;
    STR_TEST(0, "12", "12", -430);
    STR_TEST(1, "12", "1", -435);
    STR_TEST(-1, "1", "12", -440);
    STR_TEST(-1, SNULL, "12", -445);
    STR_TEST(1, "12", SNULL, -450);

    return 0;

#undef STR_TEST
}

int string_test(void)
{
    const bstr_t hello_world = { HELLO_WORLD, HELLO_WORLD_LEN };
    const bstr_t just_hello = { JUST_HELLO, JUST_HELLO_LEN };
    bstr_t s1, s2, s3, s4, s5;
    enum { RCOUNT = 10, RLEN = 16 };
    bstr_t random[RCOUNT];
    bpool_t *pool;
    int i;

    pool = bpool_create(mem, SNULL, 4096, 0, SNULL);
    if (!pool) return -5;
    
    /* 
     * bstr(), bstrcmp(), bstricmp(), bstrlen(), 
     * bstrncmp(), bstrchr() 
     */
    s1 = bstr(HELLO_WORLD);
    if (bstrcmp(&s1, &hello_world) != 0)
	return -10;
    if (bstricmp(&s1, &hello_world) != 0)
	return -20;
    if (bstrcmp(&s1, &just_hello) <= 0)
	return -30;
    if (bstricmp(&s1, &just_hello) <= 0)
	return -40;
    if (bstrlen(&s1) != strlen(HELLO_WORLD))
	return -50;
    if (bstrncmp(&s1, &hello_world, 5) != 0)
	return -60;
    if (bstrnicmp(&s1, &hello_world, 5) != 0)
	return -70;
    if (bstrchr(&s1, HELLO_WORLD[1]) != s1.ptr+1)
	return -80;

    /* 
     * bstrdup() 
     */
    if (!bstrdup(pool, &s2, &s1))
	return -100;
    if (bstrcmp(&s1, &s2) != 0)
	return -110;
    
    /* 
     * bstrcpy(), bstrcat() 
     */
    s3.ptr = (char*) bpool_alloc(pool, 256);
    if (!s3.ptr) 
	return -200;
    bstrcpy(&s3, &s2);
    bstrcat(&s3, &just_hello);

    if (bstrcmp2(&s3, HELLO_WORLD JUST_HELLO) != 0)
	return -210;

    /* 
     * bstrdup2(), bstrtrim(). 
     */
    bstrdup2(pool, &s4, " " HELLO_WORLD "\t ");
    bstrtrim(&s4);
    if (bstrcmp2(&s4, HELLO_WORLD) != 0)
	return -250;

    /* 
     * butoa() 
     */
    s5.ptr = (char*) bpool_alloc(pool, 16);
    if (!s5.ptr)
	return -270;
    s5.slen = butoa(UL_VALUE, s5.ptr);

    /* 
     * bstrtoul() 
     */
    if (bstrtoul(&s5) != UL_VALUE)
	return -280;

    /*
     * bstrtoul2()
     */
    s5 = bstr("123456");

    bstrtoul2(&s5, SNULL, 10);	/* Crash test */

    if (bstrtoul2(&s5, &s4, 10) != 123456UL)
	return -290;
    if (s4.slen != 0)
	return -291;
    if (bstrtoul2(&s5, &s4, 16) != 0x123456UL)
	return -292;

    s5 = bstr("0123ABCD");
    if (bstrtoul2(&s5, &s4, 10) != 123)
	return -293;
    if (s4.slen != 4)
	return -294;
    if (s4.ptr == SNULL || *s4.ptr != 'A')
	return -295;
    if (bstrtoul2(&s5, &s4, 16) != 0x123ABCDUL)
	return -296;
    if (s4.slen != 0)
	return -297;

    /* 
     * bcreate_random_string() 
     * Check that no duplicate strings are returned.
     */
    for (i=0; i<RCOUNT; ++i) {
	int j;
	
	random[i].ptr = (char*) bpool_alloc(pool, RLEN);
	if (!random[i].ptr)
	    return -320;

        random[i].slen = RLEN;
	bcreate_random_string(random[i].ptr, RLEN);

	for (j=0; j<i; ++j) {
	    if (bstrcmp(&random[i], &random[j])==0)
		return -330;
	}
    }

    /* Done. */
    bpool_release(pool);

    /* Case sensitive comparison test. */
    i = strcmp_test();
    if (i != 0)
	return i;

    /* Caseless comparison test. */
    i = stricmp_test();
    if (i != 0)
	return i;

    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_string_test;
#endif	/* INCLUDE_STRING_TEST */

