
#ifndef __CAM_ENV_H__
#define __CAM_ENV_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"
#include "ipc_base64.h"
#include "ipc_video.h"
#include "include/TUTK/IOTCAPIs.h"
#include "include/TUTK/AVAPIs.h"
#include "include/TUTK/AVFRAMEINFO.h"
#include "include/TUTK/AVIOCTRLDEFs.h"


#define MAX_CLIENT_NUMBER			5
#define CAM_UID_LEN					32
#define USER_PWD_LEN				16
#define MAX_SIZE_IOCTRL_BUF			1024
#define SERVTYPE_STREAM_SERVER 		0
#define MEDIA_USER_NUM				10

#define AES_KEY 					"Qi=+-ho!&(wniyeK"
#define RELAY_INVAILD_F 			"/etc/SNIP39/relay"



typedef enum {
	CAM_CMD_MIN = -1,
	CAM_ONOFF,
	CAM_MOTION,
	CAM_ROTATE,
	CAM_MIC_EN,
	CAM_VINFO_EN,
	CAM_LIGHT_EN,
	CAM_SPK_VOL,
	CAM_AUTOTURN,
	CAM_CLOUD_ENV,
	CAM_VQUALITY,
	CAM_INVERSION,
	CAM_CMD_MAX,
	
}CAM_CMD_TYPE_E;


typedef struct cam_env_s{
	unsigned char running;
	unsigned char online_status;
	unsigned char online_user_num;
	unsigned char online_guest_num;
	unsigned char cam_uid[CAM_UID_LEN]; 
	unsigned char login_type;
	unsigned char admin_pwd[USER_PWD_LEN];
	unsigned char guest_pwd[USER_PWD_LEN];
	unsigned int zone_hour;
	unsigned int zone_min;

	unsigned char onoff;
	unsigned char motion;
	unsigned char rotate;
	unsigned char mic_en;
	unsigned char vinfo_en;
	unsigned char light_en;
	unsigned char spk_vol;
	unsigned char autorun;
	unsigned char cloud_env;
	unsigned char video_quality;
	unsigned char inversion;

	unsigned char cam_model[8];
	unsigned char cam_vendor[8];
	unsigned char cam_version[32];
	unsigned char sd_path[32];
	unsigned int  sd_free;
	unsigned int  sd_total;
	
	pthread_t admin_pid;
	pthread_mutex_t client_connect_lock;
	pthread_mutex_t avserver_pthread_lock;
}CAM_ENV_S;

extern CAM_ENV_S g_camenv_attr;

typedef struct tutk_media_s{
	int avIndex;
	unsigned char is_video;
	unsigned char is_audio;
	pthread_mutex_t video_lock;
	pthread_mutex_t audio_lock;
}TUTK_MEDIA_S;

typedef struct cam_media_s{
	TUTK_MEDIA_S g_media_info[MEDIA_USER_NUM];
	unsigned char is_VideoNum;
	/*push_talk*/
	unsigned char is_talk;
	unsigned char is_talking;
	unsigned char talk_avIndex;
	pthread_mutex_t talk_lock;
	/*play_view*/
	unsigned char is_play;
	
	unsigned char is_master;
	unsigned char master_avIndex;
	unsigned char bitrate_level;
	pthread_mutex_t play_lock;
}CAM_MEDIA_S;

extern CAM_MEDIA_S g_media_attr;


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

