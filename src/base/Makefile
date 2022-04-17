# Makefile for Atmel version
# dependent on RTOS
#


include $(RULE_DIR)/Rules.mak


###################################################################
# define directory for object files
###################################################################

#	$(LWIP_HOME)/ports/sam \


CODE_SUBDIRS = \
	. \


# --std=c++14 -std=gnu++11 
CFLAGS += \
	$(PJ_CFLAGS) -I../../include \
	


###################################################################
# define source files
###################################################################

LINUX_SRC += \
	addr_resolv_sock.c \
	file_access_unistd.c \
	file_io_ansi.c \
	guid_simple.c \
	log_writer_stdout.c \
	os_core_unix.c \
	os_error_unix.c \
	os_time_unix.c \
	os_timestamp_common.c \
	os_timestamp_posix.c \
	pool_policy_malloc.c \
	sock_bsd.c \
	sock_select.c

ifeq (epoll,$(LINUX_POLL))
LINUX_SRC += ioqueue_epoll.c
else
LINUX_SRC += ioqueue_select.c
endif

LINUX_SRC += sock_qos_bsd.c



SRC += \
	activesock.c \
	array.c \
	config.c \
	ctype.c \
	errno.c \
	except.c \
	fifobuf.c \
	guid.c \
	hash.c \
	ip_helper_generic.c \
	list.c \
	lock.c \
	log.c \
	os_time_common.c \
	os_info.c \
	pool.c \
	pool_buf.c \
	pool_caching.c \
	pool_dbg.c \
	rand.c \
	rbtree.c \
	sock_common.c \
	sock_qos_common.c \
	ssl_sock_common.c \
	ssl_sock_ossl.c \
	ssl_sock_gtls.c \
	ssl_sock_dump.c \
	ssl_sock_darwin.c \
	string.c \
	timer.c \
	types.c	\
	$(LINUX_SRC) \



TARGET_LIB:=libPj

TARGET_LIB_SHARED:=$(TARGET_LIB).so
TARGET_LIB_STATIC:=$(TARGET_LIB).a
		 
LOCAL_ALL:=

include $(RULE_DIR)/Makefile.post
