
include ../../Rules.make
PWD     	:= $(shell pwd)

MIDDLEWARE_PATH := $(PWD)/../../ipc_middleware

AUD_DTC_CFLAGS		:= -fvisibility=hidden
AUD_DTC_CFLAGS 		+= -Wall -g -I./ -I../../include/  
AUD_DTC_CFLAGS		+= -O2
AUD_DTC_LDFLAGS		:= -Wl,--no-export-dynamic -L.
AUD_DTC_LDFLAGS		+= -lpthread -lstdc++ -lrt -lm libsinvoice.a
AUD_DTC_LDFLAGS 	+= $(PWD)/../../release/libipc.so 
AUD_DTC_LDFLAGS 	+= $(SDK_PATH)/lib/uclibc/libalog.so
AUD_DTC_LDFLAGS 	+= $(SDK_PATH)/lib/uclibc/libimp.so
AUD_DTC_LDFLAGS 	+= $(SDK_PATH)/lib/uclibc/libsysutils.so
AUD_DTC_LDFLAGS 	+= $(MIDDLEWARE_PATH)/openssl/lib/libcrypto.so
AUD_DTC_LDFLAGS 	+= $(MIDDLEWARE_PATH)/openssl/lib/libssl.so


AUD_DTC_SRC  := audio_tone.c
AUD_DTC_SRC  += linklist.c

TARGET := aud_dtc

all: $(TARGET) install

aud_dtc:
	@echo "[$(LINK)] $@"
	@$(LINK) -lm -ldl $(AUD_DTC_SRC) -o $(TARGET) $(AUD_DTC_CFLAGS) $(AUD_DTC_LDFLAGS)
#	@$(STRIP) $@
	
install:
	@cp $(TARGET) /home/yh/nfs/ -af
	
clean:
	@rm -f $(TARGET)
	@rm -f *.o
