
include Rules.make

PWD := $(shell pwd)

CFLAGS += -g -O2 -Wall -fPIC 
CFLAGS += -I$(IMP_PATH)/include/imp/
CFLAGS += -I$(IMP_PATH)/include/sysutils/
CFLAGS += -I$(PWD)/include/
CFLAGS += -I$(PWD)/ipc_log/
CFLAGS += -I$(PWD)/ipc_common/
CFLAGS += -I$(PWD)/ipc_video/
CFLAGS += -I$(PWD)/ipc_audio/
CFLAGS += -I$(PWD)/ipc_ircut/
CFLAGS += -I$(PWD)/ipc_rtsp_server/
#CFLAGS += -I$(PWD)/ipc_fwupgrade/
CFLAGS += -I$(PWD)/ipc_middleware/openssl/include/
#CFLAGS += -I$(PWD)/ipc_middleware/curl/include/
#CFLAGS += -I$(PWD)/ipc_middleware/json/include/

IMP_LIBS := $(SDK_PATH)/lib/uclibc/libalog.so
IMP_LIBS += $(SDK_PATH)/lib/uclibc/libimp.so
IMP_LIBS += $(SDK_PATH)/lib/uclibc/libsysutils.so
IMP_LIBS += $(PWD)/ipc_middleware/openssl/lib/libcrypto.so
IMP_LIBS += $(PWD)/ipc_middleware/openssl/lib/libssl.so
#IMP_LIBS += $(PWD)/ipc_middleware/curl/lib/libcurl.so
#IMP_LIBS += $(PWD)/ipc_middleware/json/lib/libjson.so

APP  :=
APP  += $(PWD)/example 
APP  += $(PWD)/ipc_app/cam_stream 

SRC  := $(wildcard $(PWD)/ipc_log/*.c)
SRC  += $(wildcard $(PWD)/ipc_common/*.c)
SRC  += $(wildcard $(PWD)/ipc_video/*.c)
SRC  += $(wildcard $(PWD)/ipc_ircut/*.c)
SRC  += $(wildcard $(PWD)/ipc_audio/*.c)
SRC  += $(wildcard $(PWD)/ipc_rtsp_server/*.c)
#SRC  += $(wildcard $(PWD)/ipc_fwupgrade/*.c)

OBJS=$(patsubst %.c,%.o,$(SRC))

TARGET=release/libipc.so

all:$(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(PWD)/release
	@$(LINK) -shared -lpthread -lm -lrt -ldl -Wl,-gc-sections -o $@ $^ $(IMP_LIBS)
#	@$(STRIP) $@

	cp $(PWD)/ipc_common/*.h  $(PWD)/include/
	cp $(PWD)/ipc_video/*.h  $(PWD)/include/ 
	cp $(PWD)/ipc_ircut/*.h  $(PWD)/include/ 
	cp $(PWD)/ipc_audio/*.h  $(PWD)/include/
	cp $(PWD)/ipc_rtsp_server/*.h $(PWD)/include/
	
	@make -C $(APP)
	
%.o: %.c
	@$(CC) $(CFLAGS) -c -o $@ $<

install:
	@cp $(TARGET) /home/yh/nfs/ -af	
	
clean:
	@rm -f $(OBJS) $(TARGET)
	@rm -fr $(PWD)/release
	@make -C ./example clean
