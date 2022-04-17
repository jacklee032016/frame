/* 
 *
 */
#include <baseSslSock.h>
#include <baseErrno.h>
#include <baseOs.h>
#include <baseString.h>


/* Only build when BASE_HAS_SSL_SOCK is enabled */
#if defined(BASE_HAS_SSL_SOCK) && BASE_HAS_SSL_SOCK!=0

#define CHECK_BUF_LEN()						\
    if ((len < 0) || (len >= end-p)) {				\
	*p = '\0';						\
	return -1;						\
    }								\
    p += len;

bssize_t bssl_cert_info_dump(const bssl_cert_info *ci,
					 const char *indent,
					 char *buf,
					 bsize_t buf_size)
{
    const char *wdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    bparsed_time pt1;
    bparsed_time pt2;
    unsigned i;
    int len = 0;
    char *p, *end;

    p = buf;
    end = buf + buf_size;

    btime_decode(&ci->validity.start, &pt1);
    btime_decode(&ci->validity.end, &pt2);

    /* Version */
    len = bansi_snprintf(p, end-p, "%sVersion    : v%d\n", 
			   indent, ci->version);
    CHECK_BUF_LEN();
    
    /* Serial number */
    len = bansi_snprintf(p, end-p, "%sSerial     : ", indent);
    CHECK_BUF_LEN();

    for (i = 0; i < sizeof(ci->serial_no) && !ci->serial_no[i]; ++i);
    for (; i < sizeof(ci->serial_no); ++i) {
	len = bansi_snprintf(p, end-p, "%02X ", ci->serial_no[i] & 0xFF);
	CHECK_BUF_LEN();
    }
    *(p-1) = '\n';
    
    /* Subject */
    len = bansi_snprintf( p, end-p, "%sSubject    : %.*s\n", indent,
			    (int)ci->subject.cn.slen,
			    ci->subject.cn.ptr);
    CHECK_BUF_LEN();
    len = bansi_snprintf( p, end-p, "%s             %.*s\n", indent,
			    (int)ci->subject.info.slen,
			    ci->subject.info.ptr);
    CHECK_BUF_LEN();

    /* Issuer */
    len = bansi_snprintf( p, end-p, "%sIssuer     : %.*s\n", indent,
			    (int)ci->issuer.cn.slen,
			    ci->issuer.cn.ptr);
    CHECK_BUF_LEN();
    len = bansi_snprintf( p, end-p, "%s             %.*s\n", indent,
			    (int)ci->issuer.info.slen,
			    ci->issuer.info.ptr);
    CHECK_BUF_LEN();

    /* Validity period */
    len = bansi_snprintf( p, end-p, "%sValid from : %s %4d-%02d-%02d "
			    "%02d:%02d:%02d.%03d %s\n", indent,
			    wdays[pt1.wday], pt1.year, pt1.mon+1, pt1.day,
			    pt1.hour, pt1.min, pt1.sec, pt1.msec,
			    (ci->validity.gmt? "GMT":""));
    CHECK_BUF_LEN();

    len = bansi_snprintf( p, end-p, "%sValid to   : %s %4d-%02d-%02d "
			    "%02d:%02d:%02d.%03d %s\n", indent,
			    wdays[pt2.wday], pt2.year, pt2.mon+1, pt2.day,
			    pt2.hour, pt2.min, pt2.sec, pt2.msec,
			    (ci->validity.gmt? "GMT":""));
    CHECK_BUF_LEN();

    /* Subject alternative name extension */
    if (ci->subj_alt_name.cnt) {
	len = bansi_snprintf(p, end-p, "%ssubjectAltName extension\n", 
			       indent);
	CHECK_BUF_LEN();

	for (i = 0; i < ci->subj_alt_name.cnt; ++i) {
	    const char *type = NULL;

	    switch(ci->subj_alt_name.entry[i].type) {
	    case BASE_SSL_CERT_NAME_RFC822:
		type = "MAIL";
		break;
	    case BASE_SSL_CERT_NAME_DNS:
		type = " DNS";
		break;
	    case BASE_SSL_CERT_NAME_URI:
		type = " URI";
		break;
	    case BASE_SSL_CERT_NAME_IP:
		type = "  IP";
		break;
	    default:
		break;
	    }
	    if (type) {
		len = bansi_snprintf( p, end-p, "%s      %s : %.*s\n", indent, 
					type, 
					(int)ci->subj_alt_name.entry[i].name.slen,
					ci->subj_alt_name.entry[i].name.ptr);
		CHECK_BUF_LEN();
	    }
	}
    }

    return (p-buf);
}


#endif  /* BASE_HAS_SSL_SOCK */

