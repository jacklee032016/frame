/* 
 *
 */
#ifndef __BASE_TYPES_H__
#define __BASE_TYPES_H__

#include <baseConfig.h>


#ifdef __cplusplus
extern "C" {
#endif


/** Signed 32bit integer. */
typedef int		bint32_t;

/** Unsigned 32bit integer. */
typedef unsigned int	buint32_t;

/** Signed 16bit integer. */
typedef short		bint16_t;

/** Unsigned 16bit integer. */
typedef unsigned short	buint16_t;

/** Signed 8bit integer. */
typedef signed char	bint8_t;

/** Unsigned 8bit integer. */
typedef unsigned char	buint8_t;

/** Large unsigned integer. */
typedef size_t		bsize_t;

/** Large signed integer. */
#if defined(BASE_WIN64) && BASE_WIN64!=0
    typedef bint64_t     bssize_t;
#else
    typedef long           bssize_t;
#endif

/** Status code. */
typedef int		bstatus_t;

/** Boolean. */
typedef int		bbool_t;

/** Native char type, which will be equal to wchar_t for Unicode
 * and char for ANSI. */
#if defined(BASE_NATIVE_STRING_IS_UNICODE) && BASE_NATIVE_STRING_IS_UNICODE!=0
    typedef wchar_t bchar_t;
#else
    typedef char bchar_t;
#endif

/** This macro creates Unicode or ANSI literal string depending whether
 *  native platform string is Unicode or ANSI. */
#if defined(BASE_NATIVE_STRING_IS_UNICODE) && BASE_NATIVE_STRING_IS_UNICODE!=0
#   define BASE_T(literal_str)	L##literal_str
#else
#   define BASE_T(literal_str)	literal_str
#endif

/** Some constants */
enum bconstants_
{
	/** Status is OK. */
	BASE_SUCCESS=0,

	BASE_FAILED = -1,

	/** True value. */
	BASE_TRUE=1,

	/** False value. */
	BASE_FALSE=0
};

/**
 * File offset type.
 */
#if defined(BASE_HAS_INT64) && BASE_HAS_INT64!=0
typedef bint64_t boff_t;
#else
typedef bssize_t boff_t;
#endif

/**
 * This type is used as replacement to legacy C string, and used throughout
 * the library. By convention, the string is NOT null terminated.
 */
struct _bstr_t
{
	/** Buffer pointer, which is by convention NOT null terminated. */
	char       *ptr;

	/** The length of the string. */
	bssize_t  slen;
};

/**
 * This structure represents high resolution (64bit) time value. The time
 * values represent time in cycles, which is retrieved by calling
 * #bTimeStampGet().
 */
typedef union _btimestamp
{
	struct
	{
#if defined(BASE_IS_LITTLE_ENDIAN) && BASE_IS_LITTLE_ENDIAN!=0
		buint32_t lo;     /**< Low 32-bit value of the 64-bit value. */
		buint32_t hi;     /**< high 32-bit value of the 64-bit value. */
#else
		buint32_t hi;     /**< high 32-bit value of the 64-bit value. */
		buint32_t lo;     /**< Low 32-bit value of the 64-bit value. */
#endif
	} u32;                  /**< The 64-bit value as two 32-bit values. */

#if BASE_HAS_INT64
	buint64_t u64;        /**< The whole 64-bit value, where available. */
#endif
} btimestamp;



/**
 * The opaque data type for linked list, which is used as arguments throughout the linked list operations.
 */
typedef void blist_type;

/** 
 * List.
 */
typedef struct _blist blist;

/**
 * Opaque data type for hash tables.
 */
typedef struct _bhash_table_t bhash_table_t;

/**
 * Opaque data type for hash entry (only used internally by hash table).
 */
typedef struct _bhash_entry bhash_entry;

/**
 * Data type for hash search iterator.
 * This structure should be opaque, however applications need to declare
 * concrete variable of this type, that's why the declaration is visible here.
 */
typedef struct _bhash_iterator_t
{
	buint32_t	     index;     /**< Internal index.     */
	bhash_entry   *entry;     /**< Internal entry.     */
} bhash_iterator_t;


/**
 * Forward declaration for memory pool factory.
 */
typedef struct _bpool_factory bpool_factory;

/**
 * Opaque data type for memory pool.
 */
typedef struct _bpool_t bpool_t;

/**
 * Forward declaration for caching pool, a pool factory implementation.
 */
typedef struct _bcaching_pool bcaching_pool;

/**
 * This type is used as replacement to legacy C string, and used throughout
 * the library.
 */
typedef struct _bstr_t bstr_t;

/**
 * Opaque data type for I/O Queue structure.
 */
typedef struct bioqueue_t bioqueue_t;

/**
 * Opaque data type for key that identifies a handle registered to the
 * I/O queue framework.
 */
typedef struct bioqueue_key_t bioqueue_key_t;

/**
 * Opaque data to identify timer heap.
 */
typedef struct _btimer_heap_t	btimer_heap_t;

/** 
 * Opaque data type for atomic operations.
 */
typedef struct _batomic_t batomic_t;

/**
 * Value type of an atomic variable.
 */
typedef BASE_ATOMIC_VALUE_TYPE batomic_value_t;
 
/* ************************************************************************* */

/** Thread handle. */
typedef struct bthread_t bthread_t;

/** Lock object. */
typedef struct block_t block_t;

/** Group lock */
typedef struct bgrp_lock_t bgrp_lock_t;

/** Mutex handle. */
typedef struct _bmutex_t bmutex_t;

/** Semaphore handle. */
typedef struct bsem_t bsem_t;

/** Event object. */
typedef struct bevent_t bevent_t;

/** Unidirectional stream pipe object. */
typedef struct bpipe_t bpipe_t;

/** Operating system handle. */
typedef void *bOsHandle_t;

/** Socket handle. */
#if defined(BASE_WIN64) && BASE_WIN64!=0
    typedef bint64_t bsock_t;
#else
    typedef long bsock_t;
#endif

/** Generic socket address. */
typedef void bsockaddr_t;

/** Forward declaration. */
typedef struct _bsockaddr_in bsockaddr_in;

/** Color type. */
typedef unsigned int bcolor_t;

/** Exception id. */
typedef int bexception_id_t;

/* ************************************************************************* */

/** Utility macro to compute the number of elements in static array. */
#define BASE_ARRAY_SIZE(a)    (sizeof(a)/sizeof(a[0]))

/**
 * Length of object names.
 */
#define BASE_MAX_OBJ_NAME	32

/* ************************************************************************* */
/*
 * General.
 */
/**
 * Initialize the PJ Library.
 * This function must be called before using the library. The purpose of this
 * function is to initialize static library data, such as character table used
 * in random string generation, and to initialize operating system dependent
 * functionality (such as WSAStartup() in Windows).
 *
 * Apart from calling libBaseInit(), application typically should also initialize
 * the random seed by calling bsrand().
 *
 * @return BASE_SUCCESS on success.
 */
bstatus_t libBaseInit(void);


/**
 * Shutdown .
 */
void libBaseShutdown(void);




/**
 * Swap the byte order of an 16bit data.
 *
 * @param val16	    The 16bit data.
 *
 * @return	    An 16bit data with swapped byte order.
 */
BASE_INLINE_SPECIFIER bint16_t bswap16(bint16_t val16)
{
    buint8_t *p = (buint8_t*)&val16;
    buint8_t tmp = *p;
    *p = *(p+1);
    *(p+1) = tmp;
    return val16;
}

/**
 * Swap the byte order of an 32bit data.
 *
 * @param val32	    The 32bit data.
 *
 * @return	    An 32bit data with swapped byte order.
 */
BASE_INLINE_SPECIFIER bint32_t bswap32(bint32_t val32)
{
    buint8_t *p = (buint8_t*)&val32;
    buint8_t tmp = *p;
    *p = *(p+3);
    *(p+3) = tmp;
    tmp = *(p+1);
    *(p+1) = *(p+2);
    *(p+2) = tmp;
    return val32;
}


/**
 * @}
 */
/**
 * @addtogroup BASE_TIME Time Data Type and Manipulation.
 * @ingroup BASE_MISC
 * @{
 */

/**
 * Representation of time value in this library.
 * This type can be used to represent either an interval or a specific time or date. 
 */
typedef struct _btime_val
{
	/** The seconds part of the time. */
	long    sec;

	/** The miliseconds fraction of the time. */
	long    msec;
} btime_val;

/**
 * Normalize the value in time value.
 * @param t     Time value to be normalized.
 */
void btime_val_normalize(btime_val *t);

/**
 * Get the total time value in miliseconds. This is the same as
 * multiplying the second part with 1000 and then add the miliseconds
 * part to the result.
 *
 * @param t     The time value.
 * @return      Total time in miliseconds.
 * @hideinitializer
 */
#define BASE_TIME_VAL_MSEC(t)	((t).sec * 1000 + (t).msec)

/**
 * This macro will check if \a t1 is equal to \a t2.
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if both time values are equal.
 * @hideinitializer
 */
#define BASE_TIME_VAL_EQ(t1, t2)	((t1).sec==(t2).sec && (t1).msec==(t2).msec)

/**
 * This macro will check if \a t1 is greater than \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is greater than t2.
 * @hideinitializer
 */
#define BASE_TIME_VAL_GT(t1, t2)	((t1).sec>(t2).sec || \
                                ((t1).sec==(t2).sec && (t1).msec>(t2).msec))

/**
 * This macro will check if \a t1 is greater than or equal to \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is greater than or equal to t2.
 * @hideinitializer
 */
#define BASE_TIME_VAL_GTE(t1, t2)	(BASE_TIME_VAL_GT(t1,t2) || \
                                 BASE_TIME_VAL_EQ(t1,t2))

/**
 * This macro will check if \a t1 is less than \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is less than t2.
 * @hideinitializer
 */
#define BASE_TIME_VAL_LT(t1, t2)	(!(BASE_TIME_VAL_GTE(t1,t2)))

/**
 * This macro will check if \a t1 is less than or equal to \a t2.
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is less than or equal to t2.
 * @hideinitializer
 */
#define BASE_TIME_VAL_LTE(t1, t2)	(!BASE_TIME_VAL_GT(t1, t2))

/**
 * Add \a t2 to \a t1 and store the result in \a t1. Effectively
 *
 * this macro will expand as: (\a t1 += \a t2).
 * @param t1    The time value to add.
 * @param t2    The time value to be added to \a t1.
 * @hideinitializer
 */
#define BASE_TIME_VAL_ADD(t1, t2)	    do {			    \
					(t1).sec += (t2).sec;	    \
					(t1).msec += (t2).msec;	    \
					btime_val_normalize(&(t1)); \
				    } while (0)


/**
 * Substract \a t2 from \a t1 and store the result in \a t1. Effectively
 * this macro will expand as (\a t1 -= \a t2).
 *
 * @param t1    The time value to subsctract.
 * @param t2    The time value to be substracted from \a t1.
 * @hideinitializer
 */
#define BASE_TIME_VAL_SUB(t1, t2)	    do {			    \
					(t1).sec -= (t2).sec;	    \
					(t1).msec -= (t2).msec;	    \
					btime_val_normalize(&(t1)); \
				    } while (0)


/**
 * This structure represent the parsed representation of time.
 * It is acquired by calling #btime_decode().
 */
typedef struct _bparsed_time
{
	/** This represents day of week where value zero means Sunday */
	int wday;

	/* This represents day of the year, 0-365, where zero means 1st of January.
	*/
	/*int yday; */

	/** This represents day of month: 1-31 */
	int day;

	/** This represents month, with the value is 0 - 11 (zero is January) */
	int mon;

	/** This represent the actual year (unlike in ANSI libc where
	*  the value must be added by 1900) */
	int year;

	/** This represents the second part, with the value is 0-59 */
	int sec;

	/** This represents the minute part, with the value is: 0-59 */
	int min;

	/** This represents the hour part, with the value is 0-23 */
	int hour;

	/** This represents the milisecond part, with the value is 0-999 */
	int msec;

} bparsed_time;


/*
 * Terminal.
 */
/**
 * Color code combination.
 */
enum
{
	BASE_TERM_COLOR_R	= 2,    /**< Red            */
	BASE_TERM_COLOR_G	= 4,    /**< Green          */
	BASE_TERM_COLOR_B	= 1,    /**< Blue.          */
	BASE_TERM_COLOR_BRIGHT = 8    /**< Bright mask.   */
};

#if 1
#define	BTRACE()	    BASE_INFO("")
#else
#define	TRACE()	    ;
#endif

/* TRACE with message */
#if 1
#define	MTRACE(msg,...)		BASE_LOG(3, (THIS_FILE, msg, ##__VA_ARGS__) )
#else
#define	MTRACE(msg)			
#endif

#if 0
#define	BASE_CRIT(msg,...)		BASE_LOG(2, (THIS_FILE, msg, ##__VA_ARGS__) )
#define	BASE_ERROR(msg,...)		BASE_LOG(3, (THIS_FILE, msg, ##__VA_ARGS__) )
#define	BASE_WARN(msg,...)		BASE_LOG(4, (THIS_FILE, msg, ##__VA_ARGS__) )

#define	BASE_INFO(msg,...)		BASE_LOG(5, (THIS_FILE, msg, ##__VA_ARGS__) )
#define	BASE_DEBUG(msg,...)		BASE_LOG(6, (THIS_FILE, msg, ##__VA_ARGS__) )
#else
#define	BASE_CRIT(msg,...)		BASE_LOG(2, (THIS_FILE, "%s().%d:" msg, __FUNCTION__, __LINE__, ##__VA_ARGS__) )
#define	BASE_ERROR(msg,...)		BASE_LOG(3, (THIS_FILE, "%s().%d:" msg, __FUNCTION__, __LINE__, ##__VA_ARGS__) )
#define	BASE_WARN(msg,...)		BASE_LOG(4, (THIS_FILE, "%s().%d:" msg, __FUNCTION__, __LINE__, ##__VA_ARGS__) )

#define	BASE_INFO(msg,...)		BASE_LOG(5, (THIS_FILE, "%s().%d:" msg, __FUNCTION__, __LINE__, ##__VA_ARGS__) )
#define	BASE_DEBUG(msg,...)		BASE_LOG(6, (THIS_FILE, "%s().%d:" msg, __FUNCTION__, __LINE__, ##__VA_ARGS__) )
#endif

//#define	BASE_STR_INFO(str, msg,...)		BASE_LOG(6, (str, "%s %s().%d" msg, __FILE__,  __FUNCTION__, __LINE__, ##__VA_ARGS__) )
#define	BASE_STR_INFO(str, msg,...)		BASE_LOG(5, (str, msg, ##__VA_ARGS__) )


typedef		void (*AtExitFunc)(void *);
//typedef void (*bexit_callback)(void);

bstatus_t libBaseAtExit(AtExitFunc func, char *_name, void *_data);

#ifdef __cplusplus
}
#endif


#endif

