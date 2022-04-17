/* $Id: symbols.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <libBase.h>

/*
 * addr_resolv.h
 */
BASE_EXPORT_SYMBOL(bgethostbyname)

/*
 * array.h
 */
BASE_EXPORT_SYMBOL(barray_insert)
BASE_EXPORT_SYMBOL(barray_erase)
BASE_EXPORT_SYMBOL(barray_find)

/*
 * config.h
 */
BASE_EXPORT_SYMBOL(bConfigDump)
	
/*
 * errno.h
 */
BASE_EXPORT_SYMBOL(bget_os_error)
BASE_EXPORT_SYMBOL(bset_os_error)
BASE_EXPORT_SYMBOL(bget_netos_error)
BASE_EXPORT_SYMBOL(bset_netos_error)
BASE_EXPORT_SYMBOL(extStrError)

/*
 * except.h
 */
BASE_EXPORT_SYMBOL(bthrow_exception_)
BASE_EXPORT_SYMBOL(bpush_exception_handler_)
BASE_EXPORT_SYMBOL(bpop_exception_handler_)
BASE_EXPORT_SYMBOL(bsetjmp)
BASE_EXPORT_SYMBOL(blongjmp)
BASE_EXPORT_SYMBOL(bexception_id_alloc)
BASE_EXPORT_SYMBOL(bexception_id_free)
BASE_EXPORT_SYMBOL(bexception_id_name)


/*
 * fifobuf.h
 */
BASE_EXPORT_SYMBOL(bfifobuf_init)
BASE_EXPORT_SYMBOL(bfifobuf_max_size)
BASE_EXPORT_SYMBOL(bfifobuf_alloc)
BASE_EXPORT_SYMBOL(bfifobuf_unalloc)
BASE_EXPORT_SYMBOL(bfifobuf_free)

/*
 * guid.h
 */
BASE_EXPORT_SYMBOL(bgenerate_unique_string)
BASE_EXPORT_SYMBOL(bcreate_unique_string)

/*
 * hash.h
 */
BASE_EXPORT_SYMBOL(bhash_calc)
BASE_EXPORT_SYMBOL(bhash_create)
BASE_EXPORT_SYMBOL(bhash_get)
BASE_EXPORT_SYMBOL(bhash_set)
BASE_EXPORT_SYMBOL(bhash_count)
BASE_EXPORT_SYMBOL(bhash_first)
BASE_EXPORT_SYMBOL(bhash_next)
BASE_EXPORT_SYMBOL(bhash_this)

/*
 * ioqueue.h
 */
BASE_EXPORT_SYMBOL(bioqueue_create)
BASE_EXPORT_SYMBOL(bioqueue_destroy)
BASE_EXPORT_SYMBOL(bioqueue_set_lock)
BASE_EXPORT_SYMBOL(bioqueue_register_sock)
BASE_EXPORT_SYMBOL(bioqueue_unregister)
BASE_EXPORT_SYMBOL(bioqueue_get_user_data)
BASE_EXPORT_SYMBOL(bioqueue_poll)
BASE_EXPORT_SYMBOL(bioqueue_read)
BASE_EXPORT_SYMBOL(bioqueue_recv)
BASE_EXPORT_SYMBOL(bioqueue_recvfrom)
BASE_EXPORT_SYMBOL(bioqueue_write)
BASE_EXPORT_SYMBOL(bioqueue_send)
BASE_EXPORT_SYMBOL(bioqueue_sendto)
#if defined(BASE_HAS_TCP) && BASE_HAS_TCP != 0
BASE_EXPORT_SYMBOL(bioqueue_accept)
BASE_EXPORT_SYMBOL(bioqueue_connect)
#endif

/*
 * list.h
 */
BASE_EXPORT_SYMBOL(blist_insert_before)
BASE_EXPORT_SYMBOL(blist_insert_nodes_before)
BASE_EXPORT_SYMBOL(blist_insert_after)
BASE_EXPORT_SYMBOL(blist_insert_nodes_after)
BASE_EXPORT_SYMBOL(blist_merge_first)
BASE_EXPORT_SYMBOL(blist_merge_last)
BASE_EXPORT_SYMBOL(blist_erase)
BASE_EXPORT_SYMBOL(blist_find_node)
BASE_EXPORT_SYMBOL(blist_search)


/*
 * log.h
 */
BASE_EXPORT_SYMBOL(blog_write)
#if BASE_LOG_MAX_LEVEL >= 1
BASE_EXPORT_SYMBOL(blog_set_log_func)
BASE_EXPORT_SYMBOL(blog_get_log_func)
BASE_EXPORT_SYMBOL(blog_set_level)
BASE_EXPORT_SYMBOL(blog_get_level)
BASE_EXPORT_SYMBOL(blog_set_decor)
BASE_EXPORT_SYMBOL(blog_get_decor)
BASE_EXPORT_SYMBOL(blog_1)
#endif
#if BASE_LOG_MAX_LEVEL >= 2
BASE_EXPORT_SYMBOL(blog_2)
#endif
#if BASE_LOG_MAX_LEVEL >= 3
BASE_EXPORT_SYMBOL(blog_3)
#endif
#if BASE_LOG_MAX_LEVEL >= 4
BASE_EXPORT_SYMBOL(blog_4)
#endif
#if BASE_LOG_MAX_LEVEL >= 5
BASE_EXPORT_SYMBOL(blog_5)
#endif
#if BASE_LOG_MAX_LEVEL >= 6
BASE_EXPORT_SYMBOL(blog_6)
#endif

/*
 * os.h
 */
BASE_EXPORT_SYMBOL(libBaseInit)
BASE_EXPORT_SYMBOL(bgetpid)
BASE_EXPORT_SYMBOL(bthreadRegister)
BASE_EXPORT_SYMBOL(bthreadCreate)
BASE_EXPORT_SYMBOL(bthreadGetName)
BASE_EXPORT_SYMBOL(bthreadResume)
BASE_EXPORT_SYMBOL(bthreadThis)
BASE_EXPORT_SYMBOL(bthreadJoin)
BASE_EXPORT_SYMBOL(bthreadDestroy)
BASE_EXPORT_SYMBOL(bthreadSleepMs)
#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK != 0
BASE_EXPORT_SYMBOL(bthread_check_stack)
BASE_EXPORT_SYMBOL(bthread_get_stack_max_usage)
BASE_EXPORT_SYMBOL(bthread_get_stack_info)
#endif
BASE_EXPORT_SYMBOL(batomic_create)
BASE_EXPORT_SYMBOL(batomic_destroy)
BASE_EXPORT_SYMBOL(batomic_set)
BASE_EXPORT_SYMBOL(batomic_get)
BASE_EXPORT_SYMBOL(batomic_inc)
BASE_EXPORT_SYMBOL(batomic_dec)
BASE_EXPORT_SYMBOL(bthreadLocalAlloc)
BASE_EXPORT_SYMBOL(bthreadLocalFree)
BASE_EXPORT_SYMBOL(bthreadLocalSet)
BASE_EXPORT_SYMBOL(bthreadLocalGet)
BASE_EXPORT_SYMBOL(benter_critical_section)
BASE_EXPORT_SYMBOL(bleave_critical_section)
BASE_EXPORT_SYMBOL(bmutex_create)
BASE_EXPORT_SYMBOL(bmutex_lock)
BASE_EXPORT_SYMBOL(bmutex_unlock)
BASE_EXPORT_SYMBOL(bmutex_trylock)
BASE_EXPORT_SYMBOL(bmutex_destroy)
#if defined(BASE_LIB_DEBUG) && BASE_LIB_DEBUG != 0
BASE_EXPORT_SYMBOL(bmutex_is_locked)
#endif
#if defined(BASE_HAS_SEMAPHORE) && BASE_HAS_SEMAPHORE != 0
BASE_EXPORT_SYMBOL(bsem_create)
BASE_EXPORT_SYMBOL(bsem_wait)
BASE_EXPORT_SYMBOL(bsem_trywait)
BASE_EXPORT_SYMBOL(bsem_post)
BASE_EXPORT_SYMBOL(bsem_destroy)
#endif
BASE_EXPORT_SYMBOL(bgettimeofday)
BASE_EXPORT_SYMBOL(btime_decode)
#if defined(BASE_HAS_HIGH_RES_TIMER) && BASE_HAS_HIGH_RES_TIMER != 0
BASE_EXPORT_SYMBOL(bgettickcount)
BASE_EXPORT_SYMBOL(bTimeStampGet)
BASE_EXPORT_SYMBOL(bTimeStampGetFreq)
BASE_EXPORT_SYMBOL(belapsed_time)
BASE_EXPORT_SYMBOL(belapsed_usec)
BASE_EXPORT_SYMBOL(belapsed_nanosec)
BASE_EXPORT_SYMBOL(belapsed_cycle)
#endif

	
/*
 * pool.h
 */
BASE_EXPORT_SYMBOL(bpool_create)
BASE_EXPORT_SYMBOL(bpool_release)
BASE_EXPORT_SYMBOL(bpool_getobjname)
BASE_EXPORT_SYMBOL(bpool_reset)
BASE_EXPORT_SYMBOL(bpool_get_capacity)
BASE_EXPORT_SYMBOL(bpool_get_used_size)
BASE_EXPORT_SYMBOL(bpool_alloc)
BASE_EXPORT_SYMBOL(bpool_calloc)
BASE_EXPORT_SYMBOL(bpool_factory_default_policy)
BASE_EXPORT_SYMBOL(bpool_create_int)
BASE_EXPORT_SYMBOL(bpool_init_int)
BASE_EXPORT_SYMBOL(bpool_destroy_int)
BASE_EXPORT_SYMBOL(bcaching_pool_init)
BASE_EXPORT_SYMBOL(bcaching_pool_destroy)

/*
 * rand.h
 */
BASE_EXPORT_SYMBOL(brand)
BASE_EXPORT_SYMBOL(bsrand)

/*
 * rbtree.h
 */
BASE_EXPORT_SYMBOL(brbtree_init)
BASE_EXPORT_SYMBOL(brbtree_first)
BASE_EXPORT_SYMBOL(brbtree_last)
BASE_EXPORT_SYMBOL(brbtree_next)
BASE_EXPORT_SYMBOL(brbtree_prev)
BASE_EXPORT_SYMBOL(brbtree_insert)
BASE_EXPORT_SYMBOL(brbtree_find)
BASE_EXPORT_SYMBOL(brbtree_erase)
BASE_EXPORT_SYMBOL(brbtree_max_height)
BASE_EXPORT_SYMBOL(brbtree_min_height)

/*
 * sock.h
 */
BASE_EXPORT_SYMBOL(BASE_AF_UNIX)
BASE_EXPORT_SYMBOL(BASE_AF_INET)
BASE_EXPORT_SYMBOL(BASE_AF_INET6)
BASE_EXPORT_SYMBOL(BASE_AF_PACKET)
BASE_EXPORT_SYMBOL(BASE_AF_IRDA)
BASE_EXPORT_SYMBOL(BASE_SOCK_STREAM)
BASE_EXPORT_SYMBOL(BASE_SOCK_DGRAM)
BASE_EXPORT_SYMBOL(BASE_SOCK_RAW)
BASE_EXPORT_SYMBOL(BASE_SOCK_RDM)
BASE_EXPORT_SYMBOL(BASE_SOL_SOCKET)
BASE_EXPORT_SYMBOL(BASE_SOL_IP)
BASE_EXPORT_SYMBOL(BASE_SOL_TCP)
BASE_EXPORT_SYMBOL(BASE_SOL_UDP)
BASE_EXPORT_SYMBOL(BASE_SOL_IPV6)
BASE_EXPORT_SYMBOL(bntohs)
BASE_EXPORT_SYMBOL(bhtons)
BASE_EXPORT_SYMBOL(bntohl)
BASE_EXPORT_SYMBOL(bhtonl)
BASE_EXPORT_SYMBOL(binet_ntoa)
BASE_EXPORT_SYMBOL(binet_aton)
BASE_EXPORT_SYMBOL(binet_addr)
BASE_EXPORT_SYMBOL(bsockaddr_in_set_str_addr)
BASE_EXPORT_SYMBOL(bsockaddr_in_init)
BASE_EXPORT_SYMBOL(bgethostname)
BASE_EXPORT_SYMBOL(bgethostaddr)
BASE_EXPORT_SYMBOL(bsock_socket)
BASE_EXPORT_SYMBOL(bsock_close)
BASE_EXPORT_SYMBOL(bsock_bind)
BASE_EXPORT_SYMBOL(bsock_bind_in)
#if defined(BASE_HAS_TCP) && BASE_HAS_TCP != 0
BASE_EXPORT_SYMBOL(bsock_listen)
BASE_EXPORT_SYMBOL(bsock_accept)
BASE_EXPORT_SYMBOL(bsock_shutdown)
#endif
BASE_EXPORT_SYMBOL(bsock_connect)
BASE_EXPORT_SYMBOL(bsock_getpeername)
BASE_EXPORT_SYMBOL(bsock_getsockname)
BASE_EXPORT_SYMBOL(bsock_getsockopt)
BASE_EXPORT_SYMBOL(bsock_setsockopt)
BASE_EXPORT_SYMBOL(bsock_recv)
BASE_EXPORT_SYMBOL(bsock_recvfrom)
BASE_EXPORT_SYMBOL(bsock_send)
BASE_EXPORT_SYMBOL(bsock_sendto)

/*
 * sock_select.h
 */
BASE_EXPORT_SYMBOL(BASE_FD_ZERO)
BASE_EXPORT_SYMBOL(BASE_FD_SET)
BASE_EXPORT_SYMBOL(BASE_FD_CLR)
BASE_EXPORT_SYMBOL(BASE_FD_ISSET)
BASE_EXPORT_SYMBOL(bsock_select)

/*
 * string.h
 */
BASE_EXPORT_SYMBOL(bstr)
BASE_EXPORT_SYMBOL(bstrassign)
BASE_EXPORT_SYMBOL(bstrcpy)
BASE_EXPORT_SYMBOL(bstrcpy2)
BASE_EXPORT_SYMBOL(bstrdup)
BASE_EXPORT_SYMBOL(bstrdup_with_null)
BASE_EXPORT_SYMBOL(bstrdup2)
BASE_EXPORT_SYMBOL(bstrdup3)
BASE_EXPORT_SYMBOL(bstrcmp)
BASE_EXPORT_SYMBOL(bstrcmp2)
BASE_EXPORT_SYMBOL(bstrncmp)
BASE_EXPORT_SYMBOL(bstrncmp2)
BASE_EXPORT_SYMBOL(bstricmp)
BASE_EXPORT_SYMBOL(bstricmp2)
BASE_EXPORT_SYMBOL(bstrnicmp)
BASE_EXPORT_SYMBOL(bstrnicmp2)
BASE_EXPORT_SYMBOL(bstrcat)
BASE_EXPORT_SYMBOL(bstrltrim)
BASE_EXPORT_SYMBOL(bstrrtrim)
BASE_EXPORT_SYMBOL(bstrtrim)
BASE_EXPORT_SYMBOL(bcreate_random_string)
BASE_EXPORT_SYMBOL(bstrtoul)
BASE_EXPORT_SYMBOL(butoa)
BASE_EXPORT_SYMBOL(butoa_pad)

/*
 * timer.h
 */
BASE_EXPORT_SYMBOL(btimer_heap_mem_size)
BASE_EXPORT_SYMBOL(btimer_heap_create)
BASE_EXPORT_SYMBOL(btimer_entry_init)
BASE_EXPORT_SYMBOL(btimer_heap_schedule)
BASE_EXPORT_SYMBOL(btimer_heap_cancel)
BASE_EXPORT_SYMBOL(btimer_heap_count)
BASE_EXPORT_SYMBOL(btimer_heap_earliest_time)
BASE_EXPORT_SYMBOL(btimer_heap_poll)

/*
 * types.h
 */
BASE_EXPORT_SYMBOL(btime_val_normalize)

