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


# --std=c++14
CFLAGS += \
	$(JSONCPP_CFLAGS) \
	


###################################################################
# define source files
###################################################################




SRC += \
    json_reader.cpp \
    json_value.cpp \
    json_writer.cpp \


TARGET_LIB:=libJsonCpp

TARGET_LIB_SHARED:=$(TARGET_LIB).so
TARGET_LIB_STATIC:=$(TARGET_LIB).a
		 
LOCAL_ALL:=

include $(RULE_DIR)/Makefile.post
