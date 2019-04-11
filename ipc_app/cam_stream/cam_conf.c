
#include "cam_conf.h"
#include "ipc_debug.h"

CAM_ENV_S g_camenv_attr;


void Cam_Attr_Init()
{
	memset(&g_camenv_attr, 0, sizeof(CAM_ENV_S));
	pthread_mutex_init(&g_camenv_attr.client_connect_lock, NULL);
	memcpy(g_camenv_attr.admin_pwd, "Ysx37810", sizeof("Ysx37810"));
	memcpy(g_camenv_attr.cam_uid, "RDAFEVZ1NGAD5M7J111A", sizeof("RDAFEVZ1NGAD5M7J111A"));
	g_camenv_attr.onoff = 1;
	g_camenv_attr.running = 1;

	memset(&g_media_attr, 0, sizeof(CAM_MEDIA_S));
	pthread_mutex_init(&g_media_attr.talk_lock, NULL);
	pthread_mutex_init(&g_media_attr.play_lock, NULL);
}

void Cam_Attr_UnInit()
{
	memset(&g_camenv_attr, 0, sizeof(CAM_ENV_S));
	memset(&g_media_attr, 0, sizeof(CAM_MEDIA_S));
}


int Get_Cam_Attr(int iType)
{
	switch(iType){
		case CAM_ONOFF:
			return g_camenv_attr.onoff;
		case CAM_MOTION:
			return g_camenv_attr.motion;
		case CAM_ROTATE:
			return g_camenv_attr.rotate;
		case CAM_MIC_EN:
			return g_camenv_attr.mic_en;	
		case CAM_VINFO_EN:
			return g_camenv_attr.vinfo_en;
		case CAM_LIGHT_EN:
			return g_camenv_attr.light_en;
		case CAM_SPK_VOL:
			return g_camenv_attr.spk_vol;
		case CAM_AUTOTURN:
			return g_camenv_attr.autorun;
		case CAM_CLOUD_ENV:
			return g_camenv_attr.cloud_env;
		case CAM_VQUALITY:
			return g_camenv_attr.video_quality;
		case CAM_INVERSION:
			return g_camenv_attr.inversion;
		
		default:
			return -1;
	}
}

void Set_Cam_Attr(int iType, int iValue)
{
	switch(iType){
		case CAM_CLOUD_ENV:
			g_camenv_attr.cloud_env = iValue;
			break;
		case CAM_VQUALITY:
			g_camenv_attr.video_quality = iValue;
			break;
		case CAM_INVERSION:
			g_camenv_attr.inversion = iValue;
			ipc_set_inversion(g_camenv_attr.inversion);
			break;
		case CAM_SPK_VOL:
			g_camenv_attr.spk_vol = iValue;
			ipc_set_spkvol(g_camenv_attr.spk_vol);
			break;	
		default:
			break;
	}	

	return ;
}

