#
# $Author$
#


include $(RULE_DIR)/Rules.mak

CODE_SUBDIRS = \
		.


CFLAGS += \
	$(PJ_CFLAGS) -I../../include \
		
SRC += \
	activesock.c \
	atomic.c \
	echo_clt.c \
	errno.c \
	exception.c \
	fifobuf.c \
	file.c \
	hash_test.c \
	ioq_perf.c \
	ioq_udp.c \
	ioq_unreg.c \
	ioq_tcp.c \
	list.c \
	mutex.c \
	os.c \
	pool.c pool_perf.c \
	rand.c \
	rbtree.c \
	select.c \
	sleep.c \
	sock.c \
	sock_perf.c \
	ssl_sock.c \
	string.c \
	test.c \
	thread.c \
	timer.c \
	timestamp.c \
	udp_echo_srv_sync.c \
	udp_echo_srv_ioqueue.c \
	util.c \
	main.c \


LOCAL_ALL := \
		pjTest \


include $(RULE_DIR)/Makefile.post

LDFLAGS := -L$(BIN_DIR)/lib 

ifeq ($(ARCH),arm)
LDFLAGS += 
else
LDFLAGS += 
endif

LDFLAGS += -lPj -lpthread

#  -nostartfiles 
pjTest:$(OBJS)
	$(CXX) -s -o $@ $^ $(LDFLAGS) 
	$(MOVE) $@ $(BIN_DIR)/usr/bin  

