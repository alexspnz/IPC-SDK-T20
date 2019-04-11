include ../Rules.make

CFLAGS += -Wall -g -I./ -I../include 

IMP_LIBS := ../release/libipc.so
IMP_LIBS += $(SDK_PATH)/lib/uclibc/libalog.so
IMP_LIBS += $(SDK_PATH)/lib/uclibc/libimp.so
IMP_LIBS += $(SDK_PATH)/lib/uclibc/libsysutils.so
IMP_LIBS += ../ipc_middleware/openssl/lib/libcrypto.so
IMP_LIBS += ../ipc_middleware/openssl/lib/libssl.so
IMP_LIBS += -lstdc++ -lpthread

TARGET := sample_video
TARGET += sample_audio_cap
TARGET += sample_audio_play
TARGET += sample_rtsp_server

all: $(TARGET) install

sample_video:
	@echo "[$(LINK)] $@"
	@$(LINK) -lpthread -lm -ldl sample_video.c -o sample_video $(CFLAGS) $(IMP_LIBS)
	@$(STRIP) $@

sample_audio_cap:
	@echo "[$(LINK)] $@"
	@$(LINK) -lpthread -lm -ldl sample_audio_cap.c -o sample_audio_cap $(CFLAGS) $(IMP_LIBS)
	@$(STRIP) $@
	
sample_audio_play:
	@echo "[$(LINK)] $@"
	@$(LINK) -lpthread -lm -ldl sample_audio_play.c -o sample_audio_play $(CFLAGS) $(IMP_LIBS)
	@$(STRIP) $@
	
sample_rtsp_server:
	@echo "[$(LINK)] $@"
	@$(LINK) -lpthread -lm -ldl sample_rtsp_server.c -o sample_rtsp_server $(CFLAGS) $(IMP_LIBS)
	@$(STRIP) $@
	
install:
	@cp $(TARGET) /home/yh/nfs/ -af
	
clean:
	@rm -f $(TARGET)
	@rm -f *.o
