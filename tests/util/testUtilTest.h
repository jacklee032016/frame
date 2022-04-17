/*
 *
 */
#include <_baseTypes.h>

#define INCLUDE_XML_TEST	    1
#define INCLUDE_JSON_TEST	    1
#define INCLUDE_ENCRYPTION_TEST	    1
#define INCLUDE_STUN_TEST	    1
#define INCLUDE_RESOLVER_TEST	    1
#define INCLUDE_HTTP_CLIENT_TEST    1

extern int xml_test(void);
extern int json_test(void);
extern int encryption_test();
extern int encryption_benchmark();
extern int stun_test();
extern int test_main(void);
extern int resolver_test(void);
extern int http_client_test();

extern void app_perror(const char *title, bstatus_t rc);
extern bpool_factory *mem;

