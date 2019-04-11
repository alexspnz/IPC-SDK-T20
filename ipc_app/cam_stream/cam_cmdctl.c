
#include "cam_cmdctl.h"
#include "ipc_debug.h"


typedef void (*cmd_func)(int SID, int avIndex, char *buf, int type, int iMediaNo);
typedef struct cam_cmdctl_s
{
	int type;
	cmd_func cfunc;
}CAM_CMDCTL_S;

CAM_CMDCTL_S Cmd_CTL_Func[] =
{
	/*cam_media.c*/
	{IOTYPE_USER_IPCAM_START, 					Cam_Media_Cmd},
	{IOTYPE_USER_IPCAM_STOP, 					Cam_Media_Cmd},
	{IOTYPE_USER_IPCAM_AUDIOSTART, 				Cam_Media_Cmd},
	{IOTYPE_USER_IPCAM_AUDIOSTOP, 				Cam_Media_Cmd},
	{IOTYPE_USER_IPCAM_SPEAKERSTART, 			Cam_Media_Cmd},
	{IOTYPE_USER_IPCAM_SPEAKERSTOP, 			Cam_Media_Cmd},
	{IOTYPE_USER_IPCAM_GETAUDIOOUTFORMAT_REQ, 	Cam_Media_Cmd},
	{IOTYPE_USER_IPCAM_SETSTREAMCTRL_REQ, 		Cam_Media_Cmd},
	{IOTYPE_USER_IPCAM_GETSTREAMCTRL_REQ, 		Cam_Media_Cmd},
	{IOTYPE_USER_IPCAM_GET_VIDEOMODE_REQ, 		Cam_Media_Cmd},
	{IOTYPE_USER_IPCAM_GET_ENVIRONMENT_REQ, 	Cam_Media_Cmd},
	{IOTYPE_USER_IPCAM_SET_VIDEOMODE_REQ, 		Cam_Media_Cmd},

	/*cam_cmdctl.c*/
	{IOTYPE_USER_IPCAM_DEVINFO_REQ,				Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_SET_TIMEZONE_REQ, 		Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_GET_TIMEZONE_REQ,		Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_SETPASSWORD_REQ,			Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_LISTWIFIAP_REQ,			Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_SETWIFI_REQ,				Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_GETWIFI_REQ, 			Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_GETMOTIONDETECT_REQ,		Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_GETRECORD_REQ,			Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_GETMOTIONDETECT_REQ, 	Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_SETMOTIONDETECT_REQ, 	Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_FORMATEXTSTORAGE_REQ,	Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_SET_TIME_REQ,			Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_GET_TYPE_REQ,			Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_GET_CAMERA_ALL_REQ,		Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_GETSUPPORTSTREAM_REQ,    Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_SET_DEVICE_AREA_REQ,     Cam_Func_Cmd},
	{IOTYPE_USER_IPCAM_GET_DEVICE_AREA_REQ,     Cam_Func_Cmd},
};

void Handle_IOCTRL_Cmd(int SID, int avIndex, char *buf, int type, int iMediaNo)
{
	DBG("Handle_IOCTRL_Cmd 0x%x Enter\n", type);
	unsigned int i = 0;
	for(i = 0; i < sizeof(Cmd_CTL_Func); i++){
		if (Cmd_CTL_Func[i].type == type){
			(*Cmd_CTL_Func[i].cfunc)(SID, avIndex, buf, type, iMediaNo);
			return ;
		}
	}
	DBG("err type 0x%x\n", type);
	return ;
}

void Cam_Func_Cmd(int SID, int avIndex, char *buf, int type, int iMediaNo)
{
	switch(type){
		case IOTYPE_USER_IPCAM_GETSUPPORTSTREAM_REQ: 
			DBG("IOTYPE_USER_IPCAM_GETSUPPORTSTREAM_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			break;
		case IOTYPE_USER_IPCAM_SETPASSWORD_REQ:
		{
			DBG("IOTYPE_USER_IPCAM_SETPASSWORD Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			SMsgAVIoctrlSetPasswdReq *p = (SMsgAVIoctrlSetPasswdReq*)buf;
			SMsgAVIoctrlSetPasswdResp resp;

			unsigned char key[] = AES_KEY;
			DBG("old pass %s new pass %s\n", p->oldpasswd, p->newpasswd);
			int len;
			unsigned char de[32];
			memset(de,0,sizeof(de));
			char * base_data = ipc_base64_decode(p->newpasswd, strlen(p->newpasswd),&len);
			ipc_aes_cbc_decrypt((unsigned char *)base_data, len, de, key);
			free(base_data);
			memset(g_camenv_attr.admin_pwd, 0, sizeof(g_camenv_attr.admin_pwd));
			memcpy(g_camenv_attr.admin_pwd, de, sizeof(de));
			resp.result = 0;

			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_SETPASSWORD_RESP, (char *)&resp, sizeof(SMsgAVIoctrlSetPasswdResp)) < 0)
				break;
		}break;
		case IOTYPE_USER_IPCAM_SET_TIMEZONE_REQ:
		{
			DBG("IOTYPE_USER_IPCAM_SET_TIMEZONE_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			SMsgAVIoctrlTimeZone *p = (SMsgAVIoctrlTimeZone*)buf;
			struct timeval curtime;
			struct timezone curzone;
			
			gettimeofday(&curtime, &curzone);
			DBG("current zone is %d, dsttime is %d\n", curzone.tz_minuteswest, curzone.tz_dsttime);
			
			g_camenv_attr.zone_hour = (p->nGMTDiff/100);
			g_camenv_attr.zone_min = (p->nGMTDiff%100);
			DBG("t_zone %d %d, cbsize %d nIsSupportTimeZone %d nGMTDiff %d szTimeZoneString %s\n",
					g_camenv_attr.zone_hour, g_camenv_attr.zone_min, 
					p->cbSize, p->nIsSupportTimeZone, p->nGMTDiff, p->szTimeZoneString);

			char savezone[64] = {0};
			if (g_camenv_attr.zone_hour > 0)
				sprintf(savezone, "GMT-%02d:%02d\n", (int)g_camenv_attr.zone_hour, abs((int)g_camenv_attr.zone_min));
			else
				sprintf(savezone, "GMT+%02d:%02d\n", (int)fabs(g_camenv_attr.zone_hour), abs((int)g_camenv_attr.zone_min));
			DBG("savezone %s\n", savezone);

			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_SET_TIMEZONE_RESP  , (char *)p, sizeof(SMsgAVIoctrlTimeZone)) < 0)
				break;
		}break;
		case IOTYPE_USER_IPCAM_GET_TIMEZONE_REQ: 
		{
			DBG("IOTYPE_USER_IPCAM_GET_TIMEZONE_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			SMsgAVIoctrlTimeZone resp;
			resp.cbSize = sizeof(SMsgAVIoctrlTimeZone);
			resp.nIsSupportTimeZone = 0;
			resp.nGMTDiff = (int)(g_camenv_attr.zone_hour*60);
			strcpy(resp.szTimeZoneString, "ipcam");
			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_GET_TIMEZONE_RESP, (char *)&resp, sizeof(SMsgAVIoctrlTimeZone)) < 0)
				break;
		}break;
		case IOTYPE_USER_IPCAM_SET_TIME_REQ:
        {
        	DBG("IOTYPE_USER_IPCAM_SET_TIME_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
            SMsgAVIoctrlTimeReq *p = (SMsgAVIoctrlTimeReq*)buf;
            DBG("utc_time :%ld\n",p->utc_time);

            struct timeval time_tv;
            time_tv.tv_sec = p->utc_time;
            time_tv.tv_usec = 0;

			struct timezone tz;
            tz.tz_minuteswest = -(g_camenv_attr.zone_hour*60)-g_camenv_attr.zone_min;
            tz.tz_dsttime = 0;
            DBG("tz.tz_minuteswest=%d\n",tz.tz_minuteswest);

            SMsgAVIoctrlTimeResp resp;
            resp.result = settimeofday(&time_tv, &tz);
			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_SET_TIME_RESP, (char *)&resp, sizeof(SMsgAVIoctrlTimeResp)) < 0)
				break;
        }break;
		case IOTYPE_USER_IPCAM_GET_TYPE_REQ:
		{
			DBG("IOTYPE_USER_IPCAM_TYPE_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			SMsgAVIoctrlDeviceType resp;
			resp.type = 0;			//0:卡片机 , 1 摇头机， 2 枪机  3 全景
			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_GET_TYPE_RESP, (char *)&resp, sizeof(SMsgAVIoctrlDeviceType)) < 0)
				break;
		}break;	
		case IOTYPE_USER_IPCAM_GET_CAMERA_ALL_REQ:
		{
			DBG("IOTYPE_USER_IPCAM_GET_CAMERA_ALL_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			SMsgAVIoctrlGetCameraInfoResp resp;
			resp.camera 		   = Get_Cam_Attr(CAM_ONOFF);
			resp.leaveHomeMode     = Get_Cam_Attr(CAM_MOTION);
			resp.infraredMode	   = 3;
			resp.rotateState 	   = Get_Cam_Attr(CAM_ROTATE);
			resp.cameraMicphone    = Get_Cam_Attr(CAM_MIC_EN);
			resp.cameraVoicePrompt = Get_Cam_Attr(CAM_VINFO_EN);
			resp.cameraCueLight    = Get_Cam_Attr(CAM_LIGHT_EN);
			resp.cameraVoiceNum    = Get_Cam_Attr(CAM_SPK_VOL);
			resp.TimeZone = (int)(g_camenv_attr.zone_hour*100)+(int)g_camenv_attr.zone_min;
			resp.cameraAutoCruise  = Get_Cam_Attr(CAM_AUTOTURN);
			DBG("IOTYPE_USER_IPCAM_GET_CAMERA_ALL_REQ camera:%d, leaveHomeMode:%d, infraredMode:%d, rotateState:%d, cameraMicphone:%d,cameraVoicePrompt:%d, cameraCueLight:%d, cameraVoiceNum:%d, TimeZone:%d, cameraAutoCruise:%d\n",
				resp.camera, resp.leaveHomeMode, resp.infraredMode,resp.rotateState, resp.cameraMicphone, \
				resp.cameraVoicePrompt, resp.cameraCueLight, resp.cameraVoiceNum, resp.TimeZone, resp.cameraAutoCruise);
			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_GET_CAMERA_ALL_RESP, (char *)&resp, sizeof(SMsgAVIoctrlGetCameraInfoResp)) < 0)
				break;
		}break;
		case IOTYPE_USER_IPCAM_DEVINFO_REQ:
		{
			DBG("IOTYPE_USER_IPCAM_DEVINFO_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			SMsgAVIoctrlDeviceInfoResp resp;
			memset(&resp,0,sizeof(SMsgAVIoctrlDeviceInfoResp));
			strcpy((char *)resp.model, g_camenv_attr.cam_model);
			strcpy((char *)resp.vendor, g_camenv_attr.cam_vendor);
			DBG("model %s\n",resp.model);
			DBG("version %x %d\n", g_camenv_attr.cam_version, g_camenv_attr.cam_version);
			resp.version = g_camenv_attr.cam_version;
			resp.channel = avIndex;
			if(Check_Record_RemainSize(g_camenv_attr.sd_path, RECORD_TYPE_MP4) == 0){
				resp.total = g_camenv_attr.sd_total;
				resp.free = g_camenv_attr.sd_free;
			}
			else{
				resp.total = 0;
				resp.free = 0;
			}

			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_DEVINFO_RESP, (char *)&resp, sizeof(SMsgAVIoctrlDeviceInfoResp)) < 0)
				break;
		}break;
		case IOTYPE_USER_IPCAM_GET_DEVICE_AREA_REQ:
        {
        	DBG("IOTYPE_USER_IPCAM_GET_DEVICE_AREA_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
            SMsgAVIoctrlDeviceAreaReqResq resp;

			resp.isCN  = Get_Cam_Attr(CAM_CLOUD_ENV);
            resp.result = 0;

            if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_GET_DEVICE_AREA_RESP, (char *)&resp, sizeof(SMsgAVIoctrlDeviceAreaReqResq)) < 0)
                break;
        }break;
        case IOTYPE_USER_IPCAM_SET_DEVICE_AREA_REQ:
        {
        	DBG("IOTYPE_USER_IPCAM_SET_DEVICE_AREA_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
            SMsgAVIoctrlDeviceAreaReqResq *q =(SMsgAVIoctrlDeviceAreaReqResq *)buf;
            SMsgAVIoctrlDeviceAreaReqResq resp;

			Set_Cam_Attr(CAM_CLOUD_ENV, q->isCN);
            resp.result = 0;

			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_SET_DEVICE_AREA_RESP, (char *)&resp, sizeof(SMsgAVIoctrlDeviceAreaReqResq)) < 0)
                break;
        }break;
		case IOTYPE_USER_IPCAM_SET_CAMERA_VOL_REQ:
		{
			DBG("IOTYPE_USER_IPCAM_SET_CAMERA_VOL_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			SMsgAVIoctrlSetCameraVolReq *p = (SMsgAVIoctrlSetCameraVolReq*)buf;
			Set_Cam_Attr(CAM_SPK_VOL, p->vol);
			
			SMsgAVIoctrlSetCameraVolResp resp;
			resp.result = 1;
			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_SET_CAMERA_VOL_RESP  , (char *)&resp, sizeof(SMsgAVIoctrlSetCameraVolResp)) < 0)
				break;
		}break;
			
		default:
			DBG("NO Find Cmd index %d iMediaNo %d\n", avIndex, iMediaNo);
			break;
		
	}
}

