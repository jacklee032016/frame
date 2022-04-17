/* 
 *
 */
#ifndef __UTIL_DNS_H__
#define __UTIL_DNS_H__


/**
 * @brief Low level DNS message parsing and packetization.
 */
#include <utilTypes.h>
#include <baseSock.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_DNS DNS and Asynchronous DNS Resolver
 * @ingroup BASE_PROTOCOLS
 */

/**
 * @defgroup BASE_DNS_PARSING Low-level DNS Message Parsing and Packetization
 * @ingroup BASE_DNS
 * @{
 *
 * This module provides low-level services to parse and packetize DNS queries
 * and responses. The functions support building a DNS query packet and parse
 * the data in the DNS response. This implementation conforms to the 
 * following specifications:
 *  - RFC 1035: DOMAIN NAMES - IMPLEMENTATION AND SPECIFICATION
 *  - RFC 1886: DNS Extensions to support IP version 6
 *
 * To create a DNS query packet, application should call #bdns_make_query()
 * function, specifying the desired DNS query type, the name to be resolved,
 * and the buffer where the DNS packet will be built into. 
 *
 * When incoming DNS query or response packet arrives, application can use
 * #bdns_parse_packet() to parse the TCP/UDP payload into parsed DNS packet
 * structure.
 *
 * This module does not provide any networking functionalities to send or
 * receive DNS packets. This functionality should be provided by higher layer
 * modules such as @ref BASE_DNS_RESOLVER.
 */

enum
{
    BASE_DNS_CLASS_IN	= 1	/**< DNS class IN.			    */
};

/**
 * This enumeration describes standard DNS record types as described by
 * RFC 1035, RFC 2782, and others.
 */
typedef enum bdns_type
{
    BASE_DNS_TYPE_A	= 1,    /**< Host address (A) record.		    */
    BASE_DNS_TYPE_NS	= 2,    /**< Authoritative name server (NS)	    */
    BASE_DNS_TYPE_MD	= 3,    /**< Mail destination (MD) record.	    */
    BASE_DNS_TYPE_MF	= 4,    /**< Mail forwarder (MF) record.	    */
    BASE_DNS_TYPE_CNAME	= 5,	/**< Canonical name (CNAME) record.	    */
    BASE_DNS_TYPE_SOA	= 6,    /**< Marks start of zone authority.	    */
    BASE_DNS_TYPE_MB	= 7,    /**< Mailbox domain name (MB).		    */
    BASE_DNS_TYPE_MG	= 8,    /**< Mail group member (MG).		    */
    BASE_DNS_TYPE_MR	= 9,    /**< Mail rename domain name.		    */
    BASE_DNS_TYPE_NULL	= 10,	/**< NULL RR.				    */
    BASE_DNS_TYPE_WKS	= 11,	/**< Well known service description	    */
    BASE_DNS_TYPE_PTR	= 12,	/**< Domain name pointer.		    */
    BASE_DNS_TYPE_HINFO	= 13,	/**< Host information.			    */
    BASE_DNS_TYPE_MINFO	= 14,	/**< Mailbox or mail list information.	    */
    BASE_DNS_TYPE_MX	= 15,	/**< Mail exchange record.		    */
    BASE_DNS_TYPE_TXT	= 16,	/**< Text string.			    */
    BASE_DNS_TYPE_RP	= 17,	/**< Responsible person.		    */
    BASE_DNS_TYPE_AFSB	= 18,	/**< AFS cell database.			    */
    BASE_DNS_TYPE_X25	= 19,	/**< X.25 calling address.		    */
    BASE_DNS_TYPE_ISDN	= 20,	/**< ISDN calling address.		    */
    BASE_DNS_TYPE_RT	= 21,	/**< Router.				    */
    BASE_DNS_TYPE_NSAP	= 22,	/**< NSAP address.			    */
    BASE_DNS_TYPE_NSAP_PTR= 23,	/**< NSAP reverse address.		    */
    BASE_DNS_TYPE_SIG	= 24,	/**< Signature.				    */
    BASE_DNS_TYPE_KEY	= 25,	/**< Key.				    */
    BASE_DNS_TYPE_PX	= 26,	/**< X.400 mail mapping.		    */
    BASE_DNS_TYPE_GPOS	= 27,	/**< Geographical position (withdrawn)	    */
    BASE_DNS_TYPE_AAAA	= 28,	/**< IPv6 address.			    */
    BASE_DNS_TYPE_LOC	= 29,	/**< Location.				    */
    BASE_DNS_TYPE_NXT	= 30,	/**< Next valid name in the zone.	    */
    BASE_DNS_TYPE_EID	= 31,	/**< Endpoint idenfitier.		    */
    BASE_DNS_TYPE_NIMLOC	= 32,	/**< Nimrod locator.			    */
    BASE_DNS_TYPE_SRV	= 33,	/**< Server selection (SRV) record.	    */
    BASE_DNS_TYPE_ATMA	= 34,	/**< DNS ATM address record.		    */
    BASE_DNS_TYPE_NAPTR	= 35,	/**< DNS Naming authority pointer record.   */
    BASE_DNS_TYPE_KX	= 36,	/**< DNS key exchange record.		    */
    BASE_DNS_TYPE_CERT	= 37,	/**< DNS certificate record.		    */
    BASE_DNS_TYPE_A6	= 38,	/**< DNS IPv6 address (experimental)	    */
    BASE_DNS_TYPE_DNAME	= 39,	/**< DNS non-terminal name redirection rec. */

    BASE_DNS_TYPE_OPT	= 41,	/**< DNS options - contains EDNS metadata.  */
    BASE_DNS_TYPE_APL	= 42,	/**< DNS Address Prefix List (APL) record.  */
    BASE_DNS_TYPE_DS	= 43,	/**< DNS Delegation Signer (DS)		    */
    BASE_DNS_TYPE_SSHFP	= 44,	/**< DNS SSH Key Fingerprint		    */
    BASE_DNS_TYPE_IPSECKEY= 45,	/**< DNS IPSEC Key.			    */
    BASE_DNS_TYPE_RRSIG	= 46,	/**< DNS Resource Record signature.	    */
    BASE_DNS_TYPE_NSEC	= 47,	/**< DNS Next Secure Name.		    */
    BASE_DNS_TYPE_DNSKEY	= 48	/**< DNSSEC Key.			    */
} bdns_type;



/**
 * Standard DNS header, according to RFC 1035, which will be present in
 * both DNS query and DNS response. 
 *
 * Note that all values seen by application would be in
 * host by order. The library would convert them to network
 * byte order as necessary.
 */
typedef struct bdns_hdr
{
    buint16_t  id;	    /**< Transaction ID.	    */
    buint16_t  flags;	    /**< Flags.			    */
    buint16_t  qdcount;   /**< Nb. of queries.	    */
    buint16_t  anscount;  /**< Nb. of res records	    */
    buint16_t  nscount;   /**< Nb. of NS records.	    */
    buint16_t  arcount;   /**< Nb. of additional records  */
} bdns_hdr;

/** Create RCODE flag */
#define BASE_DNS_SET_RCODE(c)	((buint16_t)((c) & 0x0F))

/** Create RA (Recursion Available) bit */
#define BASE_DNS_SET_RA(on)	((buint16_t)((on) << 7))

/** Create RD (Recursion Desired) bit */
#define BASE_DNS_SET_RD(on)	((buint16_t)((on) << 8))

/** Create TC (Truncated) bit */
#define BASE_DNS_SET_TC(on)	((buint16_t)((on) << 9))

/** Create AA (Authoritative Answer) bit */
#define BASE_DNS_SET_AA(on)	((buint16_t)((on) << 10))

/** Create four bits opcode */
#define BASE_DNS_SET_OPCODE(o)	((buint16_t)((o)  << 11))

/** Create query/response bit */
#define BASE_DNS_SET_QR(on)	((buint16_t)((on) << 15))


/** Get RCODE value */
#define BASE_DNS_GET_RCODE(val)	(((val) & BASE_DNS_SET_RCODE(0x0F)) >> 0)

/** Get RA bit */
#define BASE_DNS_GET_RA(val)	(((val) & BASE_DNS_SET_RA(1)) >> 7)

/** Get RD bit */
#define BASE_DNS_GET_RD(val)	(((val) & BASE_DNS_SET_RD(1)) >> 8)

/** Get TC bit */
#define BASE_DNS_GET_TC(val)	(((val) & BASE_DNS_SET_TC(1)) >> 9)

/** Get AA bit */
#define BASE_DNS_GET_AA(val)	(((val) & BASE_DNS_SET_AA(1)) >> 10)

/** Get OPCODE value */
#define BASE_DNS_GET_OPCODE(val)	(((val) & BASE_DNS_SET_OPCODE(0x0F)) >> 11)

/** Get QR bit */
#define BASE_DNS_GET_QR(val)	(((val) & BASE_DNS_SET_QR(1)) >> 15)


/** 
 * These constants describe DNS RCODEs. Application can fold these constants
 * into  bstatus_t namespace by calling #BASE_STATUS_FROM_DNS_RCODE()
 * macro.
 */
typedef enum bdns_rcode
{
    BASE_DNS_RCODE_FORMERR    = 1,    /**< Format error.			    */
    BASE_DNS_RCODE_SERVFAIL   = 2,    /**< Server failure.		    */
    BASE_DNS_RCODE_NXDOMAIN   = 3,    /**< Name Error.			    */
    BASE_DNS_RCODE_NOTIMPL    = 4,    /**< Not Implemented.		    */
    BASE_DNS_RCODE_REFUSED    = 5,    /**< Refused.			    */
    BASE_DNS_RCODE_YXDOMAIN   = 6,    /**< The name exists.		    */
    BASE_DNS_RCODE_YXRRSET    = 7,    /**< The RRset (name, type) exists.	    */
    BASE_DNS_RCODE_NXRRSET    = 8,    /**< The RRset (name, type) doesn't exist*/
    BASE_DNS_RCODE_NOTAUTH    = 9,    /**< Not authorized.		    */
    BASE_DNS_RCODE_NOTZONE    = 10    /**< The zone specified is not a zone.  */

} bdns_rcode;


/**
 * This structure describes a DNS query record.
 */
typedef struct bdns_parsed_query
{
    bstr_t	name;	    /**< The domain in the query.		    */
    buint16_t	type;	    /**< Type of the query (bdns_type)	    */
    buint16_t	dnsclass;   /**< Network class (BASE_DNS_CLASS_IN=1)	    */
} bdns_parsed_query;


/**
 * This structure describes a Resource Record parsed from the DNS packet.
 * All integral values are in host byte order.
 */
typedef struct bdns_parsed_rr
{
    bstr_t	 name;	    /**< The domain name which this rec pertains.   */
    buint16_t	 type;	    /**< RR type code.				    */
    buint16_t	 dnsclass;  /**< Class of data (BASE_DNS_CLASS_IN=1).	    */
    buint32_t	 ttl;	    /**< Time to live.				    */
    buint16_t	 rdlength;  /**< Resource data length.			    */
    void	*data;	    /**< Pointer to the raw resource data, only
				 when the type is not known. If it is known,
				 the data will be put in rdata below.	    */
    
    /** For resource types that are recognized/supported by this library,
     *  the parsed resource data will be placed in this rdata union.
     */
    union rdata
    {
	/** SRV Resource Data (BASE_DNS_TYPE_SRV, 33) */
	struct srv {
	    buint16_t	prio;	/**< Target priority (lower is higher).	    */
	    buint16_t weight;	/**< Weight/proportion			    */
	    buint16_t port;	/**< Port number of the service		    */
	    bstr_t	target;	/**< Target name.			    */
	} srv;

	/** CNAME Resource Data (BASE_DNS_TYPE_CNAME, 5) */
	struct cname {
	    bstr_t	name;	/**< Primary canonical name for an alias.   */
	} cname;

	/** NS Resource Data (BASE_DNS_TYPE_NS, 2) */
	struct ns {
	    bstr_t	name;	/**< Primary name server.		    */
	} ns;

	/** PTR Resource Data (BASE_DNS_TYPE_PTR, 12) */
	struct ptr {
	    bstr_t	name;	/**< PTR name.				    */
	} ptr;

	/** A Resource Data (BASE_DNS_TYPE_A, 1) */
	struct a {
	    bin_addr	ip_addr;/**< IPv4 address in network byte order.    */
	} a;

	/** AAAA Resource Data (BASE_DNS_TYPE_AAAA, 28) */
	struct aaaa {
	    bin6_addr	ip_addr;/**< IPv6 address in network byte order.    */
	} aaaa;

    } rdata;

} bdns_parsed_rr;


/**
 * This structure describes the parsed repersentation of the raw DNS packet.
 * Note that all integral values in the parsed packet are represented in
 * host byte order.
 */
typedef struct bdns_parsed_packet
{
    bdns_hdr		 hdr;	/**< Pointer to DNS hdr, in host byte order */
    bdns_parsed_query	*q;	/**< Array of DNS queries.		    */
    bdns_parsed_rr	*ans;	/**< Array of DNS RR answer.		    */
    bdns_parsed_rr	*ns;	/**< Array of NS record in the answer.	    */
    bdns_parsed_rr	*arr;	/**< Array of additional RR answer.	    */
} bdns_parsed_packet;


/**
 * Option flags to be specified when calling #bdns_packet_dup() function.
 * These flags can be combined with bitwise OR operation.
 */
enum bdns_dup_options
{
    BASE_DNS_NO_QD    = 1, /**< Do not duplicate the query section.	    */
    BASE_DNS_NO_ANS   = 2, /**< Do not duplicate the answer section.	    */
    BASE_DNS_NO_NS    = 4, /**< Do not duplicate the NS section.		    */
    BASE_DNS_NO_AR    = 8  /**< Do not duplicate the additional rec section   */
};


/**
 * Create DNS query packet to resolve the specified names. This function
 * can be used to build any types of DNS query, such as A record or DNS SRV
 * record.
 *
 * Application specifies the type of record and the name to be queried,
 * and the function will build the DNS query packet into the buffer
 * specified. Once the packet is successfully built, application can send
 * the packet via TCP or UDP connection.
 *
 * @param packet	The buffer to put the DNS query packet.
 * @param size		On input, it specifies the size of the buffer.
 *			On output, it will be filled with the actual size of
 *			the DNS query packet.
 * @param id		DNS query ID to associate DNS response with the
 *			query.
 * @param qtype		DNS type of record to be queried (see #bdns_type).
 * @param name		Name to be queried from the DNS server.
 *
 * @return		BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bdns_make_query(void *packet,
				       unsigned *size,
				       buint16_t id,
				       int qtype,
				       const bstr_t *name);

/**
 * Parse raw DNS packet into parsed DNS packet structure. This function is
 * able to parse few DNS resource records such as A record, PTR record,
 * CNAME record, NS record, and SRV record.
 *
 * @param pool		Pool to allocate memory for the parsed packet.
 * @param packet	Pointer to the DNS packet (the TCP/UDP payload of 
 *			the raw packet).
 * @param size		The size of the DNS packet.
 * @param p_res		Pointer to store the resulting parsed packet.
 *
 * @return		BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bdns_parse_packet(bpool_t *pool,
					 const void *packet,
					 unsigned size,
					 bdns_parsed_packet **p_res);

/**
 * Duplicate DNS packet.
 *
 * @param pool		The pool to allocate memory for the duplicated packet.
 * @param p		The DNS packet to be cloned.
 * @param options	Option flags, from bdns_dup_options.
 * @param p_dst		Pointer to store the cloned DNS packet.
 */
void bdns_packet_dup(bpool_t *pool,
				const bdns_parsed_packet*p,
				unsigned options,
				bdns_parsed_packet **p_dst);


/**
 * Utility function to get the type name string of the specified DNS type.
 *
 * @param type		DNS type (see #bdns_type).
 *
 * @return		String name of the type (e.g. "A", "SRV", etc.).
 */
const char * bdns_get_type_name(int type);


/**
 * Initialize DNS record as DNS SRV record.
 *
 * @param rec		The DNS resource record to be initialized as DNS
 *			SRV record.
 * @param res_name	Resource name.
 * @param dnsclass	DNS class.
 * @param ttl		Resource TTL value.
 * @param prio		DNS SRV priority.
 * @param weight	DNS SRV weight.
 * @param port		Target port.
 * @param target	Target name.
 */
void bdns_init_srv_rr(bdns_parsed_rr *rec,
				 const bstr_t *res_name,
				 unsigned dnsclass,
				 unsigned ttl,
				 unsigned prio,
				 unsigned weight,
				 unsigned port,
				 const bstr_t *target);

/**
 * Initialize DNS record as DNS CNAME record.
 *
 * @param rec		The DNS resource record to be initialized as DNS
 *			CNAME record.
 * @param res_name	Resource name.
 * @param dnsclass	DNS class.
 * @param ttl		Resource TTL value.
 * @param name		Host name.
 */
void bdns_init_cname_rr(bdns_parsed_rr *rec,
				   const bstr_t *res_name,
				   unsigned dnsclass,
				   unsigned ttl,
				   const bstr_t *name);

/**
 * Initialize DNS record as DNS A record.
 *
 * @param rec		The DNS resource record to be initialized as DNS
 *			A record.
 * @param res_name	Resource name.
 * @param dnsclass	DNS class.
 * @param ttl		Resource TTL value.
 * @param ip_addr	Host address.
 */
void bdns_init_a_rr(bdns_parsed_rr *rec,
			       const bstr_t *res_name,
			       unsigned dnsclass,
			       unsigned ttl,
			       const bin_addr *ip_addr);

/**
 * Initialize DNS record as DNS AAAA record.
 *
 * @param rec		The DNS resource record to be initialized as DNS
 *			AAAA record.
 * @param res_name	Resource name.
 * @param dnsclass	DNS class.
 * @param ttl		Resource TTL value.
 * @param ip_addr	Host address.
 */
void bdns_init_aaaa_rr(bdns_parsed_rr *rec,
				  const bstr_t *res_name,
				  unsigned dnsclass,
				  unsigned ttl,
				  const bin6_addr *ip_addr);

/**
 * Dump DNS packet to standard log.
 *
 * @param res		The DNS packet.
 */
void bdns_dump_packet(const bdns_parsed_packet *res);

BASE_END_DECL


#endif

