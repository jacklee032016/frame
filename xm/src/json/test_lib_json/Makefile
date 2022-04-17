#
# $Author$
#


include $(RULE_DIR)/Rules.mak

CODE_SUBDIRS = \
		.


CFLAGS += \
	-std=gnu++11 

		
SRC += \
    jsontest.cpp \
	fuzz.cpp \
	main.cpp \


LOCAL_ALL := \
		jsoncpp_test \


include $(RULE_DIR)/Makefile.post

LDFLAGS := -L$(BIN_DIR)/lib 

ifeq ($(ARCH),arm)
LDFLAGS += 
else
LDFLAGS += 
endif

LDFLAGS += -lJsonCpp

#  -nostartfiles 
jsoncpp_test:$(OBJS)
	$(CXX) -s -o $@ $^ $(LDFLAGS) 
	$(MOVE) $@ $(BIN_DIR)/usr/bin  

