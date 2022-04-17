/* 
 *
 */
#include "testUtilTest.h"


#if INCLUDE_XML_TEST

#include <utilXml.h>
#include <libBase.h>

static const char *xml_doc[] =
{
"   <?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"   <p:pidf-full xmlns=\"urn:ietf:params:xml:ns:pidf\"\n"
"          xmlns:p=\"urn:ietf:params:xml:ns:pidf-diff\"\n"
"          xmlns:r=\"urn:ietf:params:xml:ns:pidf:rpid\"\n"
"          xmlns:c=\"urn:ietf:params:xml:ns:pidf:caps\"\n"
"          entity=\"pres:someone@example.com\"\n"
"          version=\"567\">\n"
"\n"
"    <tuple id=\"sg89ae\">\n"
"     <status>\n"
"      <basic>open</basic>\n"
"      <r:relationship>assistant</r:relationship>\n"
"     </status>\n"
"     <c:servcaps>\n"
"      <c:audio>true</c:audio>\n"
"      <c:video>false</c:video>\n"
"      <c:message>true</c:message>\n"
"     </c:servcaps>\n"
"     <contact priority=\"0.8\">tel:09012345678</contact>\n"
"    </tuple>\n"
"\n"
"    <tuple id=\"cg231jcr\">\n"
"     <status>\n"
"      <basic>open</basic>\n"
"     </status>\n"
"     <contact priority=\"1.0\">im:pep@example.com</contact>\n"
"    </tuple>\n"
"\n"
"    <tuple id=\"r1230d\">\n"
"     <status>\n"
"      <basic>closed</basic>\n"
"      <r:activity>meeting</r:activity>\n"
"     </status>\n"
"     <r:homepage>http://example.com/~pep/</r:homepage>\n"
"     <r:icon>http://example.com/~pep/icon.gif</r:icon>\n"
"     <r:card>http://example.com/~pep/card.vcd</r:card>\n"
"     <contact priority=\"0.9\">sip:pep@example.com</contact>\n"
"    </tuple>\n"
"\n"
"    <note xml:lang=\"en\">Full state presence document</note>\n"
"\n"
"    <r:person>\n"
"     <r:status>\n"
"      <r:activities>\n"
"       <r:on-the-phone/>\n"
"       <r:busy/>\n"
"      </r:activities>\n"
"     </r:status>\n"
"    </r:person>\n"
"\n"
"    <r:device id=\"urn:esn:600b40c7\">\n"
"     <r:status>\n"
"      <c:devcaps>\n"
"       <c:mobility>\n"
"        <c:supported>\n"
"         <c:mobile/>\n"
"        </c:supported>\n"
"       </c:mobility>\n"
"      </c:devcaps>\n"
"     </r:status>\n"
"    </r:device>\n"
"\n"
"   </p:pidf-full>\n"
}
;

static int xml_parse_print_test(const char *doc)
{
    bstr_t msg;
    bpool_t *pool;
    bxml_node *root;
    char *output;
    int output_len;

    pool = bpool_create(mem, "xml", 4096, 1024, NULL);
    bstrdup2(pool, &msg, doc);
    root = bxml_parse(pool, msg.ptr, msg.slen);
    if (!root) {
	BASE_CRIT("  Error: unable to parse XML");
	return -10;
    }

    output = (char*)bpool_zalloc(pool, msg.slen + 512);
    output_len = bxml_print(root, output, msg.slen+512, BASE_TRUE);
    if (output_len < 1) {
	BASE_CRIT("  Error: buffer too small to print XML file");
	return -20;
    }
    output[output_len] = '\0';


    bpool_release(pool);
    return 0;
}

int xml_test()
{
    unsigned i;
    for (i=0; i<sizeof(xml_doc)/sizeof(xml_doc[0]); ++i) {
	int status;
	if ((status=xml_parse_print_test(xml_doc[i])) != 0)
	    return status;
    }
    return 0;
}

#else
int dummy_xml_test;
#endif


