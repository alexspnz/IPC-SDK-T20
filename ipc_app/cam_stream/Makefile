
include ../../Rules.make
PWD      := $(shell pwd)

MIDDLEWARE_PATH := $(PWD)/../../ipc_middleware

FFMPEG_CFLAGS :=
FFMPEG_CFLAGS += $(PWD)/include/ffmpeg/libavformat/  
FFMPEG_CFLAGS += $(PWD)/include/ffmpeg/libavcodec/   
FFMPEG_CFLAGS += $(PWD)/include/ffmpeg/libavutil/

IPC_CFLAGS := -Wall -g -I./ -I../../include/  
IPC_CFLAGS += -I$(PWD)/inlude/util/
IPC_CFLAGS += -I$(PWD)/inlude/mp4v2/
IPC_CFLAGS += -I$(PWD)/inlude/aac/
IPC_CFLAGS += -I$(PWD)/inlude/TUTK/
IPC_CFLAGS += -I$(MIDDLEWARE_PATH)/openssl/include/
#IPC_CFLAGS += -I$(FFMPEG_CFLAGS)

IMP_LIBS :=
IMP_LIBS += $(SDK_PATH)/lib/uclibc/libalog.so
IMP_LIBS += $(SDK_PATH)/lib/uclibc/libimp.so
IMP_LIBS += $(SDK_PATH)/lib/uclibc/libsysutils.so

AAC_LIBS :=
AAC_LIBS += $(PWD)/lib/aac/libfdk-aac.a
AAC_LIBS += $(PWD)/lib/aac/libaudio_codec.a

FFMPEG_LIBS :=
FFMPEG_LIBS += $(PWD)/lib/ffmpeg/libavformat.a  
FFMPEG_LIBS += $(PWD)/lib/ffmpeg/libavcodec.a   
FFMPEG_LIBS += $(PWD)/lib/ffmpeg/libavutil.a

MP4V2_LIBS :=
MP4V2_LIBS += $(PWD)/lib/mp4v2/libmp4v2.a

UTIL_LIBS :=
UTIL_LIBS += $(PWD)/lib/util/libiniparser.a
UTIL_LIBS += $(MIDDLEWARE_PATH)/openssl/lib/libcrypto.so
UTIL_LIBS += $(MIDDLEWARE_PATH)/openssl/lib/libssl.so

TUTK_LIBS :=
TUTK_LIBS += $(PWD)/lib/TUTK/libIOTCAPIs.a	
TUTK_LIBS += $(PWD)/lib/TUTK/libAVAPIs.a 

IPC_LIBS := $(PWD)/../../release/libipc.so -lstdc++ -lpthread
IPC_LIBS += $(IMP_LIBS)
IPC_LIBS += $(AAC_LIBS)
IPC_LIBS += $(MP4V2_LIBS)
IPC_LIBS += $(TUTK_LIBS)
IPC_LIBS += $(UTIL_LIBS)
#IPC_LIBS += $(FFMPEG_LIBS)

IPC_SRC  := 
IPC_SRC  += main.c
IPC_SRC  += cam_media.c
IPC_SRC  += cam_media_framepool.c
IPC_SRC  += cam_media_record.c
IPC_SRC  += cam_malloc.c
IPC_SRC  += audio_codec.c
IPC_SRC  += adpcm.c
IPC_SRC  += cam_cmdctl.c
IPC_SRC  += cam_conf.c

TARGET := camstream

all: $(TARGET) install

camstream:
	@echo "[$(LINK)] $@"
	@$(LINK) -lm -ldl $(IPC_SRC) -o $(TARGET) $(IPC_CFLAGS) $(IPC_LIBS)
#	@$(STRIP) $@
	
install:
	@cp $(TARGET) /home/yh/nfs/ -af
	
clean:
	@rm -f $(TARGET)
	@rm -f *.o
