/*
 *
 */
#ifndef __UTIL_CONFIG_H__
#define __UTIL_CONFIG_H__


/**
 * @brief Compile time settings
 */


/* 
 * DNS CONFIGURATION
 */

/**
 * Maximum number of IP addresses in DNS A response.
 */
#ifndef BASE_DNS_MAX_IP_IN_A_REC
#   define BASE_DNS_MAX_IP_IN_A_REC   8
#endif


/**
 * Maximum server address entries per one SRV record
 */
#ifndef BASE_DNS_SRV_MAX_ADDR
#   define BASE_DNS_SRV_MAX_ADDR	    8
#endif


/**
 * This constant specifies the maximum names to keep in the temporary name
 * table when performing name compression scheme when duplicating DNS packet
 * (the #bdns_packet_dup() function).
 *
 * Generally name compression is desired, since it saves some memory (see
 * BASE_DNS_RESOLVER_RES_BUF_SIZE setting). However it comes at the expense of 
 * a little processing overhead to perform name scanning and also a little
 * bit more stack usage (8 bytes per entry on 32bit platform).
 *
 * Default: 16
 */
#ifndef BASE_DNS_MAX_NAMES_IN_NAMETABLE
#   define BASE_DNS_MAX_NAMES_IN_NAMETABLE	    16
#endif


/* **************************************************************************
 * RESOLVER CONFIGURATION
 */


/**
 * Maximum numbers of DNS nameservers that can be configured in resolver.
 */
#ifndef BASE_DNS_RESOLVER_MAX_NS
#   define BASE_DNS_RESOLVER_MAX_NS		    16
#endif


/**
 * Default retransmission delay, in miliseconds. The combination of 
 * retransmission delay and count determines the query timeout.
 *
 * Default: 2000 (2 seconds, according to RFC 1035)
 */
#ifndef BASE_DNS_RESOLVER_QUERY_RETRANSMIT_DELAY
#   define BASE_DNS_RESOLVER_QUERY_RETRANSMIT_DELAY   2000
#endif


/**
 * Maximum number of transmissions before timeout is declared for
 * the query.
 *
 * Default: 5
 */
#ifndef BASE_DNS_RESOLVER_QUERY_RETRANSMIT_COUNT
#   define BASE_DNS_RESOLVER_QUERY_RETRANSMIT_COUNT   5
#endif


/**
 * Maximum life-time of DNS response in the resolver response cache, 
 * in seconds. If the value is zero, then DNS response caching will be 
 * disabled.
 *
 * Default is 300 seconds (5 minutes).
 *
 * @see BASE_DNS_RESOLVER_INVALID_TTL
 */
#ifndef BASE_DNS_RESOLVER_MAX_TTL
#   define BASE_DNS_RESOLVER_MAX_TTL		    (5*60)
#endif

/**
 * The life-time of invalid DNS response in the resolver response cache.
 * An invalid DNS response is a response which RCODE is non-zero and 
 * response without any answer section. These responses can be put in 
 * the cache too to minimize message round-trip.
 *
 * Default: 60 (one minute).
 *
 * @see BASE_DNS_RESOLVER_MAX_TTL
 */
#ifndef BASE_DNS_RESOLVER_INVALID_TTL
#   define BASE_DNS_RESOLVER_INVALID_TTL		    60
#endif

/**
 * The interval on which nameservers which are known to be good to be 
 * probed again to determine whether they are still good. Note that
 * this applies to both active nameserver (the one currently being used)
 * and idle nameservers (good nameservers that are not currently selected).
 * The probing to query the "goodness" of nameservers involves sending
 * the same query to multiple servers, so it's probably not a good idea
 * to send this probing too often.
 *
 * Default: 600 (ten minutes)
 *
 * @see BASE_DNS_RESOLVER_BAD_NS_TTL
 */
#ifndef BASE_DNS_RESOLVER_GOOD_NS_TTL
#   define BASE_DNS_RESOLVER_GOOD_NS_TTL		    (10*60)
#endif

/**
 * The interval on which nameservers which known to be bad to be probed
 * again to determine whether it is still bad.
 *
 * Default: 60 (one minute)
 *
 * @see BASE_DNS_RESOLVER_GOOD_NS_TTL
 */
#ifndef BASE_DNS_RESOLVER_BAD_NS_TTL
#   define BASE_DNS_RESOLVER_BAD_NS_TTL		    (1*60)
#endif


/**
 * Maximum size of UDP packet. RFC 1035 states that maximum size of
 * DNS packet carried over UDP is 512 bytes.
 *
 * Default: 512 byes
 */
#ifndef BASE_DNS_RESOLVER_MAX_UDP_SIZE
#   define BASE_DNS_RESOLVER_MAX_UDP_SIZE		    512
#endif


/**
 * Size of memory pool allocated for each individual DNS response cache.
 * This value here should be more or less the same as maximum UDP packet
 * size (BASE_DNS_RESOLVER_MAX_UDP_SIZE), since the DNS replicator function
 * (#bdns_packet_dup()) is also capable of performing name compressions.
 *
 * Default: 512
 */
#ifndef BASE_DNS_RESOLVER_RES_BUF_SIZE
#   define BASE_DNS_RESOLVER_RES_BUF_SIZE		    512
#endif


/**
 * Size of temporary pool buffer for parsing DNS packets in resolver.
 *
 * default: 4000
 */
#ifndef BASE_DNS_RESOLVER_TMP_BUF_SIZE
#   define BASE_DNS_RESOLVER_TMP_BUF_SIZE		    4000
#endif


/* **************************************************************************
 * SCANNER CONFIGURATION
 */


/**
 * Macro BASE_SCANNER_USE_BITWISE is defined and non-zero (by default yes)
 * will enable the use of bitwise for character input specification (cis).
 * This would save several kilobytes of .bss memory in the SIP parser.
 */
#ifndef BASE_SCANNER_USE_BITWISE
#  define BASE_SCANNER_USE_BITWISE		    1
#endif



/* **************************************************************************
 * STUN CLIENT CONFIGURATION
 */

/**
 * Maximum number of attributes in the STUN packet (for the old STUN
 * library).
 *
 * Default: 16
 */
#ifndef USTUN_MAX_ATTR
#   define USTUN_MAX_ATTR			    16
#endif


/**
 * Maximum number of attributes in the STUN packet (for the new STUN
 * library).
 *
 * Default: 16
 */
#ifndef BASE_STUN_MAX_ATTR
#   define BASE_STUN_MAX_ATTR			    16
#endif


/* **************************************************************************
 * ENCRYPTION
 */

/**
 * Specifies whether CRC32 algorithm should use the table based lookup table
 * for faster calculation, at the expense of about 1KB table size on the
 * executable. If zero, the CRC32 will use non-table based which is more than
 * an order of magnitude slower.
 *
 * Default: 1
 */
#ifndef BASE_CRC32_HAS_TABLES
#   define BASE_CRC32_HAS_TABLES			    1
#endif


/* **************************************************************************
 * HTTP Client configuration
 */
/**
 * Timeout value for HTTP request operation. The value is in ms.
 * Default: 60000ms
 */
#ifndef BASE_HTTP_DEFAULT_TIMEOUT
#   define BASE_HTTP_DEFAULT_TIMEOUT         (60000)
#endif

/* **************************************************************************
 * CLI configuration
 */

/**
 * Initial pool size for CLI.
 * Default: 1024 bytes
 */
#ifndef BASE_CLI_POOL_SIZE
#   define BASE_CLI_POOL_SIZE    1024
#endif

/**
 * Pool increment size for CLI.
 * Default: 512 bytes
 */
#ifndef BASE_CLI_POOL_INC
#   define BASE_CLI_POOL_INC     512
#endif

/**
 * Maximum length of command buffer.
 * Default: 512
 */
#ifndef BASE_CLI_MAX_CMDBUF
#   define BASE_CLI_MAX_CMDBUF		512
#endif

/**
 * Maximum command arguments.
 * Default: 8
 */
#ifndef BASE_CLI_MAX_ARGS
#   define BASE_CLI_MAX_ARGS		8
#endif

/**
 * Maximum number of hints.
 * Default: 32
 */
#ifndef BASE_CLI_MAX_HINTS
#   define BASE_CLI_MAX_HINTS		32
#endif

/**
 * Maximum short name version (shortcuts) for a command.
 * Default: 4
 */
#ifndef BASE_CLI_MAX_SHORTCUTS
#   define BASE_CLI_MAX_SHORTCUTS		4
#endif

/**
 * Initial pool size for console CLI.
 * Default: 256 bytes
 */
#ifndef BASE_CLI_CONSOLE_POOL_SIZE
#   define BASE_CLI_CONSOLE_POOL_SIZE    256
#endif

/**
 * Pool increment size for console CLI.
 * Default: 256 bytes
 */
#ifndef BASE_CLI_CONSOLE_POOL_INC
#   define BASE_CLI_CONSOLE_POOL_INC     256
#endif

/**
 * Initial pool size for telnet CLI.
 * Default: 1024 bytes
 */
#ifndef BASE_CLI_TELNET_POOL_SIZE
#   define BASE_CLI_TELNET_POOL_SIZE 1024
#endif

/**
 * Pool increment size for telnet CLI.
 * Default: 512 bytes
 */
#ifndef BASE_CLI_TELNET_POOL_INC
#   define BASE_CLI_TELNET_POOL_INC  512
#endif

/**
 * Maximum number of argument values of choice type.
 * Default: 16
 */
#ifndef BASE_CLI_MAX_CHOICE_VAL
#   define BASE_CLI_MAX_CHOICE_VAL  16
#endif

/**
 * Maximum number of command history.
 * Default: 16
 */
#ifndef BASE_CLI_MAX_CMD_HISTORY
#   define BASE_CLI_MAX_CMD_HISTORY  16
#endif

#endif

