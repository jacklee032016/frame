/*
 *
 */

#ifndef __BITOPS_H__
#define __BITOPS_H__

#include "config.h"

#include <limits.h>
#include <stdbool.h>

/* Defines */
#define BIT_PER_LONG	(CHAR_BIT * sizeof(unsigned long))
#define BIT_MASK(idx)	(1UL << ((idx) % BIT_PER_LONG))
#define BIT_WORD(idx)	((idx) / BIT_PER_LONG)

/* Helpers */
static inline void __set_bit(unsigned idx, unsigned long *bmap)
{
	bmap[BIT_WORD(idx)] |= BIT_MASK(idx);
}

static inline void __clear_bit(unsigned idx, unsigned long *bmap)
{
	bmap[BIT_WORD(idx)] &= ~BIT_MASK(idx);
}

static inline bool __test_bit(unsigned idx, const unsigned long *bmap)
{
	return !!(bmap[BIT_WORD(idx)] & BIT_MASK(idx));
}

static inline bool __test_and_set_bit(unsigned idx, unsigned long *bmap)
{
	if (__test_bit(idx, bmap))
		return true;

	__set_bit(idx, bmap);

	return false;
}

/* Bits */
enum global_bits
{
	LOG_CONSOLE_BIT,
	NO_SYSLOG_BIT,
	DONT_FORK_BIT,
	DUMP_CONF_BIT,
#ifdef _WITH_VRRP_
	DONT_RELEASE_VRRP_BIT,
	RELEASE_VIPS_BIT,
#endif
#ifdef _WITH_LVS_
	DONT_RELEASE_IPVS_BIT,
#endif
	LOG_DETAIL_BIT,
	LOG_EXTRA_DETAIL_BIT,
	DONT_RESPAWN_BIT,
#ifdef _MEM_CHECK_
	MEM_ERR_DETECT_BIT,
#ifdef _MEM_CHECK_LOG_
	MEM_CHECK_LOG_BIT,
#endif
#endif
#ifdef _WITH_LVS_
	LOG_ADDRESS_CHANGES,
#endif
	CONFIG_TEST_BIT,
};

#endif

