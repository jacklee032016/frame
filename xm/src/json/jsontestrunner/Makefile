#
# $Author$
#


include $(RULE_DIR)/Rules.mak

CODE_SUBDIRS = \
		.


CFLAGS += \
	-std=gnu++11 

		
SRC += \
		main.cpp \


LOCAL_ALL := \
		jsontestrunner \


include $(RULE_DIR)/Makefile.post

LDFLAGS := -L$(BIN_DIR)/lib 

ifeq ($(ARCH),arm)
LDFLAGS += 
else
LDFLAGS += 
endif

LDFLAGS += -lJsonCpp

#  -nostartfiles 
jsontestrunner:$(OBJS)
	$(CXX) -s -o $@ $^ $(LDFLAGS) 
	$(MOVE) $@ $(BIN_DIR)/usr/bin  

