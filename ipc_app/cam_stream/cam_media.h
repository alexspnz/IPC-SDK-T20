
#ifndef __CAM_MEDIA_H__
#define __CAM_MEDIA_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"
#include "ipc_video.h"
#include "ipc_audio.h"
#include "ipc_osd.h"
#include "cam_media_record.h"
#include "cam_media_framepool.h"

#define AUDIO_BUF_SIZE 				2048
#define BITRATE_SWITCH_DELAY		0x1388


#define VIDEO_RECORD_BUFFER_SIZE 	512*1024
#define USED_VIDEO_RECORD_BUF_NUM 	15
#define MAX_VIDEO_RECORD_BUF_NUM	30

#define AUDIO_RECORD_BUFFER_SIZE 	128*1024
#define USED_AUDIO_RECORD_BUF_NUM 	10
#define MAX_AUDIO_RECORD_BUF_NUM	20

#define VIDEO_LIVE_BUFFER_SIZE 		512*1024
#define USED_VIDEO_LIVE_BUF_NUM 	15
#define MAX_VIDEO_LIVE_BUF_NUM		30

#define AUDIO_LIVE_BUFFER_SIZE 		128*1024
#define USED_AUDIO_LIVE_BUF_NUM 	10
#define MAX_AUDIO_LIVE_BUF_NUM		20

extern av_buffer *video_record_buffer;
extern av_buffer *audio_record_buffer;
extern av_buffer *video_live_buffer;
extern av_buffer *audio_live_buffer;
extern pthread_mutex_t video_record_lock, audio_record_lock, video_live_lock, audio_live_lock;

extern IPC_VIDEO_S g_video_attr;
extern IPC_AUDIO_S g_audio_attr;
extern s_record_info g_record_attr;

int Cam_Media_Init();
void Cam_Media_UnInit();
void Cam_Media_Cmd(int SID, int avIndex, char *buf, int type, int iMediaNo);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

