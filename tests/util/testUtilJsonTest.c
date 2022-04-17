/* 
 *
 */
#include "testUtilTest.h"

#if INCLUDE_JSON_TEST

#include <utilJson.h>
#include <baseLog.h>
#include <baseString.h>

static char json_doc1[] =
"{\
    \"Object\": {\
       \"Integer\":  800,\
       \"Negative\":  -12,\
       \"Float\": -7.2,\
       \"String\":  \"A\\tString with tab\",\
       \"Object2\": {\
           \"True\": true,\
           \"False\": false,\
           \"Null\": null\
       },\
       \"Array1\": [116, false, \"string\", {}],\
       \"Array2\": [\
	    {\
        	   \"Float\": 123.,\
	    },\
	    {\
		   \"Float\": 123.,\
	    }\
       ]\
     },\
   \"Integer\":  800,\
   \"Array1\": [116, false, \"string\"]\
}\
";

static int json_verify_1()
{
    bpool_t *pool;
    bjson_elem *elem;
    char *out_buf;
    unsigned size;
    bjson_err_info err;

    pool = bpool_create(mem, "json", 1000, 1000, NULL);

    size = (unsigned)strlen(json_doc1);
    elem = bjson_parse(pool, json_doc1, &size, &err);
    if (!elem) {
	BASE_CRIT("  Error: json_verify_1() parse error");
	goto on_error;
    }

    size = (unsigned)strlen(json_doc1) * 2;
    out_buf = bpool_alloc(pool, size);

    if (bjson_write(elem, out_buf, &size)) {
	BASE_CRIT("  Error: json_verify_1() write error");
	goto on_error;
    }

    BASE_INFO("Json document:\n%s", out_buf);
    bpool_release(pool);
    return 0;

on_error:
    bpool_release(pool);
    return 10;
}


int json_test(void)
{
    int rc;

    rc = json_verify_1();
    if (rc)
	return rc;

    return 0;
}

#else
int json_dummy;
#endif

