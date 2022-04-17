#
# $Id$
#

include $(ROOT_DIR)/Makefile.pre

CODE_SUBDIRS = \
		md5 \
		portable \
		cjson \
		utils \
		lib \


C_FLAGS += $(SHARED_CFLAGS)


################################################
####   source file 
################################################
PORTABL_SRC += \
		portable/cmnMutex.c \
		portable/cmnList.c \
		portable/cmnFifo.c \
		portable/cmnTimer.c \
		portable/cmnThread.c \
		portable/cmnFsm.c \
		portable/cmnFtpLib.c \
		portable/cmnLogUtils.c \
		portable/cmnSysTimer.c \

UTILS_SRC += \
		utils/cmnFileUtils.c \
		utils/cmnListHead.c \
		utils/cmnRbTree.c \
		utils/cmnSysList.c \
		utils/cmnSysUtils.c \
		utils/cmnTime.c \
		utils/cmnUtils.c \
		
		

LIB_SRC += \
		lib/cmnBuffer.c \
		lib/cmnMemory.c \
		lib/cmnNetwork.c \
		lib/cmnPriQueue.c \
		lib/cmnSignals.c \
		lib/cmnStream.c \
		
		
		
		
		

SRC += \
		$(PORTABL_SRC) \
		$(LIB_SRC) \
		$(UTILS_SRC) \
		md5/md5.c \
		md5/md5hl.c \
		md5/base64.c \
		md5/cmnChecksum.c \
		cjson/cJSON.c \
		cjson/cJSON_Utils.c \
		cmn_version.c \
		
 
TARGET_LIB:=libShared

TARGET_LIB_SHARED:=$(TARGET_LIB).so
TARGET_LIB_STATIC:=$(TARGET_LIB).a
		 
LOCAL_ALL:=

include $(ROOT_DIR)/Makefile.post

