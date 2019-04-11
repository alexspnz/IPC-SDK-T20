/*
 * ipc_video.c
 * Author:yuanhao@yingshixun.com
 * Date:2019.2
 */

#include "imp_log.h"
#include "imp_common.h"
#include "imp_system.h"
#include "imp_isp.h"
#include "imp_framesource.h"
#include "imp_encoder.h"

#include "ipc_comm.h"
#include "ipc_video.h"
#include "ipc_ircut.h"
#include "ipc_osd.h"
#include "ipc_ivs.h"
#include "ipc_debug.h"
#include "ipc_frame_pool.h"

#define TAG "Sample-Video"

typedef struct ipc_video_info_s
{
    IPC_VIDEO_S video;
	IMPFSChnAttr framesource_chn[FS_CHN_NUM];
	IMPCell frame_source[FS_CHN_NUM];
	IMPCell imp_encoder[FS_CHN_NUM];
	IMPCell osd_cell[FS_CHN_NUM];
	IMPCell ivs_grp;
    pthread_t pthread_id[FS_CHN_NUM];
    pthread_mutex_t pthread_mutex;
    unsigned char running;
}
IPC_VIDEO_INFO_S;
static IPC_VIDEO_INFO_S* g_video_args = NULL;

extern int IMP_Encoder_SetChnResizeMode(int encChn, int en);
int ipc_jpeg_init(unsigned int width, unsigned int height)
{
    int ret;
    int enc_grp = MAIN_CHANNEL;
	int profie = 0;
	
    IMPEncoderAttr *enc_attr;
    IMPEncoderCHNAttr channel_attr;

    memset(&channel_attr, 0, sizeof(IMPEncoderCHNAttr));
    enc_attr = &channel_attr.encAttr;
    enc_attr->enType = PT_JPEG;
    enc_attr->bufSize = 0;
    enc_attr->profile = profie;		// image quality 0 ~ 2
    enc_attr->picWidth = width;		//SENSOR_WIDTH;
    enc_attr->picHeight = height;	//SENSOR_HEIGHT;

    /* Create Channel */
    ret = IMP_Encoder_CreateChn(JPG_CHANNEL, &channel_attr);
    if (ret < 0) {
        IMP_LOG_ERR(TAG,"IMP_Encoder_CreateChn(%d) error: %d\n",JPG_CHANNEL, ret);
        return -1;
    }

    /* Resigter Channel */
    ret = IMP_Encoder_RegisterChn(enc_grp, JPG_CHANNEL);
    if (ret < 0) {
        IMP_LOG_ERR(TAG, "IMP_Encoder_RegisterChn(0, %d) error: %d\n",JPG_CHANNEL, ret);
        return -1;
    }

	ret = IMP_Encoder_SetChnResizeMode(JPG_CHANNEL, 1);
	if (ret < 0) {
        IMP_LOG_ERR(TAG,  "IMP_Encoder_RegisterChn(0, %d) error: %d\n",JPG_CHANNEL, ret);
        return -1;
    }

    DBG("ipc_jpeg_init OK\n");
    return 0;
}

int ipc_jpeg_exit(void)
{//抓取截图去初始化
    int ret = -1;
    /* unresigter Channel */
    ret = IMP_Encoder_UnRegisterChn(JPG_CHANNEL);
    if (ret < 0) {
        IMP_LOG_ERR(TAG,  "IMP_Encoder_UnRegisterChn(0, %d) error: %d\n",JPG_CHANNEL, ret);
        return -1;
    }
    /* destroy Channel */
    ret = IMP_Encoder_DestroyChn(JPG_CHANNEL);
    if (ret < 0) {
        IMP_LOG_ERR(TAG, "IMP_Encoder_DestroyChn(%d) error: %d\n",JPG_CHANNEL, ret);
        return -1;
    }

	DBG("ipc_jpeg_exit OK\n");
    return ret;
}

int ipc_get_jpeg(char *buf, int *bufLen)
{
    int ret,i;
    unsigned int len;

    ret = IMP_Encoder_StartRecvPic(JPG_CHANNEL);
    if (ret < 0) {
        IMP_LOG_ERR(TAG, "IMP_Encoder_StartRecvPic(%d) failed\n", JPG_CHANNEL);
        return -1;
    }

    /* Polling JPEG Snap, set timeout as 1000msec */
    ret = IMP_Encoder_PollingStream(JPG_CHANNEL, 1000);
    if (ret < 0) {
        IMP_LOG_ERR(TAG,"Polling stream timeout\n");
        return -1;
    }

    IMPEncoderStream stream;
    /* Get JPEG Snap */
    ret = IMP_Encoder_GetStream(JPG_CHANNEL, &stream, 1);
    if (ret < 0) {
        IMP_LOG_ERR(TAG, "IMP_Encoder_GetStream() failed\n");
        return -1;
    }

    len = 0;
    for(i=0;i<stream.packCount;i++)
    {
        len += stream.pack[i].length;
    }

    if(len > *bufLen)
    {
        IMP_LOG_ERR(TAG,"buflen %d is too samall\n",*bufLen);
        ret = -1;
    }
    else
    {
        len = 0;
        for(i=0;i<stream.packCount;i++)
        {
            memcpy(buf+len,(void *)stream.pack[i].virAddr,stream.pack[i].length);
            len += stream.pack[i].length;
        }
        *bufLen = len;
        ret = 0;
    }

    IMP_Encoder_ReleaseStream(JPG_CHANNEL, &stream);
    IMP_Encoder_StopRecvPic(JPG_CHANNEL);

    return ret;
}

int ipc_set_bitrate(int channel, int bitrate)
{
    int ret = 0;
    IMPEncoderAttrRcMode rcAttr;

    memset(&rcAttr,0,sizeof(IMPEncoderAttrRcMode));
    ret = IMP_Encoder_GetChnAttrRcMode(channel, &rcAttr);
    if (ret < 0) {
        IMP_LOG_ERR(TAG, "IMP_Encoder_GetChnRcAttr Error with result %d\n",ret);
        return -1;
    }

    rcAttr.attrH264Smart.maxBitRate = bitrate;                    			//Smart264_mode
    ret = IMP_Encoder_SetChnAttrRcMode(channel, &rcAttr);
    if (ret < 0) {
        IMP_LOG_ERR(TAG, "IMP_Encoder_SetChnRcAttr Error with result %d\n",ret);
        return -1;
    }
	
    DBG("IPC_SetBitrate Done\n");
    return 0;
}

int ipc_set_Iframe(int channel)
{
	int ret;

	ret = IMP_Encoder_RequestIDR(channel);
    if (ret < 0) {
        IMP_LOG_ERR(TAG, "IMP_Encoder_RequestIDR Error with result %d\n",ret);
        return -1;
    }
	
	DBG("IPC_SetIFrame Done\n");
    return 0;
}

int ipc_set_inversion(int enable)
{
    int ret;
    IMPISPTuningOpsMode key;
    if(enable)
        key = IMPISP_TUNING_OPS_MODE_ENABLE ;
    else
        key = IMPISP_TUNING_OPS_MODE_DISABLE;

    ret = IMP_ISP_Tuning_SetISPHflip(key);
    if(ret < 0) {
        IMP_LOG_ERR(TAG, "IMP_ISP_Tuning_SetISPHflip Error with result %d\n",ret);
        return -1;
    }

    ret = IMP_ISP_Tuning_SetISPVflip(key);
    if(ret < 0) {
        IMP_LOG_ERR(TAG, "IMP_ISP_Tuning_SetISPVflip Error with result %d\n",ret);
        return -1;
    }

    DBG("IPC_SetInversion Done\n");
    return 0;
}

extern int IMP_OSD_SetPoolSize(int size);
extern int IMP_Encoder_SetPoolSize(int size);
static IMPSensorInfo sensor_info;
int ipc_system_init()
{
	int ret = 0;

	memset(&sensor_info, 0, sizeof(IMPSensorInfo));
	memcpy(sensor_info.name, SENSOR_NAME, sizeof(SENSOR_NAME));
	sensor_info.cbus_type = SENSOR_CUBS_TYPE;
	memcpy(sensor_info.i2c.type, SENSOR_NAME, sizeof(SENSOR_NAME));
	sensor_info.i2c.addr = SENSOR_I2C_ADDR;
	DBG("Current Sensor is %s\n", sensor_info.name);

	IMP_OSD_SetPoolSize(180*1024);
	IMP_Encoder_SetPoolSize(1*1024*1024);
    IMPVersion version;
    memset(&version,0,sizeof(IMPVersion));
	
    ret = IMP_System_GetVersion(&version);
    if(ret < 0){
        IMP_LOG_ERR(TAG, "IMP_System_GetVersion failed\n");
        return -1;
    }
    DBG("Current IMP Version is %s\n", version.aVersion);

	ret = IMP_ISP_Open();
	if(ret < 0){
		IMP_LOG_ERR(TAG, "failed to open ISP\n");
		return -1;
	}

	ret = IMP_ISP_AddSensor(&sensor_info);
	if(ret < 0){
		IMP_LOG_ERR(TAG, "failed to AddSensor\n");
		return -1;
	}

	ret = IMP_ISP_EnableSensor();
	if(ret < 0){
		IMP_LOG_ERR(TAG, "failed to EnableSensor\n");
		return -1;
	}

	ret = IMP_System_Init();
	if(ret < 0){
		IMP_LOG_ERR(TAG, "IMP_System_Init failed\n");
		return -1;
	}

	/* enable turning, to debug graphics */
	ret = IMP_ISP_EnableTuning();
	if(ret < 0){
		IMP_LOG_ERR(TAG, "IMP_ISP_EnableTuning failed\n");
		return -1;
	}

    ret = IMP_ISP_Tuning_SetSensorFPS(g_video_args->video.video_stream[MAIN_CHANNEL].fps, SENSOR_FRAME_RATE_DEN);
    if (ret < 0){
        IMP_LOG_ERR(TAG, "failed to set sensor fps\n");
        return -1;
    }

	DBG("ipc_system_init OK\n");
	return 0;
}

int ipc_framesource_init()
{
	int i, ret;

	for (i = 0; i < FS_CHN_NUM; i++) {
	
		if(g_video_args->video.video_stream[i].enable){
			ret = IMP_FrameSource_CreateChn(i, &g_video_args->framesource_chn[i]);
			if(ret < 0){
				IMP_LOG_ERR(TAG, "IMP_FrameSource_CreateChn(chn%d) error !\n", i);
				return -1;
			}

			ret = IMP_FrameSource_SetChnAttr(i, &g_video_args->framesource_chn[i]);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_FrameSource_SetChnAttr(chn%d) error !\n", i);
				return -1;
			}
		}
	}

	DBG("ipc_framesource_init OK\n");
	return 0;
}

int ipc_encoder_init()
{
	int i, ret;
	IMPEncoderAttr *enc_attr;
	IMPEncoderRcAttr *rc_attr;
	IMPEncoderCHNAttr channel_attr;
	memset(&channel_attr, 0, sizeof(IMPEncoderCHNAttr));

	for (i = 0; i < FS_CHN_NUM; i++) {
		if (g_video_args->video.video_stream[i].enable) {
			enc_attr = &channel_attr.encAttr;
			enc_attr->enType = PT_H264;
			enc_attr->bufSize = 0;
			enc_attr->profile = 2;
			enc_attr->picWidth = g_video_args->video.video_stream[i].width;
			enc_attr->picHeight = g_video_args->video.video_stream[i].height;

			DBG("video_stream[%d].Width=%d, Height=%d\n", i, g_video_args->video.video_stream[i].width, g_video_args->video.video_stream[i].height);
			
			rc_attr = &channel_attr.rcAttr;
            rc_attr->outFrmRate.frmRateNum = g_video_args->video.video_stream[i].fps;
            rc_attr->outFrmRate.frmRateDen = SENSOR_FRAME_RATE_DEN;
            rc_attr->maxGop = g_video_args->video.video_stream[i].gop * rc_attr->outFrmRate.frmRateNum / rc_attr->outFrmRate.frmRateDen;
			
            if (g_video_args->video.video_stream[i].rc_mode == IPC_RC_MODE_CBR) {
                rc_attr->attrRcMode.rcMode = ENC_RC_MODE_CBR;
                rc_attr->attrRcMode.attrH264Cbr.outBitRate = g_video_args->video.video_stream[i].bitrate;
                rc_attr->attrRcMode.attrH264Cbr.maxQp = MAX_QP;
                rc_attr->attrRcMode.attrH264Cbr.minQp = MIN_QP;
                rc_attr->attrRcMode.attrH264Cbr.iBiasLvl = 0;
                rc_attr->attrRcMode.attrH264Cbr.frmQPStep = 3;
                rc_attr->attrRcMode.attrH264Cbr.gopQPStep = rc_attr->outFrmRate.frmRateNum / rc_attr->outFrmRate.frmRateDen;
                rc_attr->attrRcMode.attrH264Cbr.adaptiveMode = false;
                rc_attr->attrRcMode.attrH264Cbr.gopRelation = false;

                rc_attr->attrHSkip.hSkipAttr.skipType = IMP_Encoder_STYPE_N1X;
                rc_attr->attrHSkip.hSkipAttr.m = 0;
                rc_attr->attrHSkip.hSkipAttr.n = 0;
                rc_attr->attrHSkip.hSkipAttr.maxSameSceneCnt = 0;
                rc_attr->attrHSkip.hSkipAttr.bEnableScenecut = 0;
                rc_attr->attrHSkip.hSkipAttr.bBlackEnhance = 0;
                rc_attr->attrHSkip.maxHSkipType = IMP_Encoder_STYPE_N1X;
            }
			else if (g_video_args->video.video_stream[i].rc_mode == IPC_RC_MODE_VBR) {
                rc_attr->attrRcMode.rcMode = ENC_RC_MODE_VBR;
                rc_attr->attrRcMode.attrH264Vbr.maxQp = MAX_QP;
                rc_attr->attrRcMode.attrH264Vbr.minQp = MIN_QP;
                rc_attr->attrRcMode.attrH264Vbr.staticTime = 2;
                rc_attr->attrRcMode.attrH264Vbr.maxBitRate = g_video_args->video.video_stream[i].bitrate;
                rc_attr->attrRcMode.attrH264Vbr.iBiasLvl = 0;
                rc_attr->attrRcMode.attrH264Vbr.changePos = 80;
                rc_attr->attrRcMode.attrH264Vbr.qualityLvl = 2;
                rc_attr->attrRcMode.attrH264Vbr.frmQPStep = 3;
                rc_attr->attrRcMode.attrH264Vbr.gopQPStep = rc_attr->outFrmRate.frmRateNum / rc_attr->outFrmRate.frmRateDen;
                rc_attr->attrRcMode.attrH264Vbr.gopRelation = false;

                rc_attr->attrHSkip.hSkipAttr.skipType = IMP_Encoder_STYPE_N1X;
                rc_attr->attrHSkip.hSkipAttr.m = 0;
                rc_attr->attrHSkip.hSkipAttr.n = 0;
                rc_attr->attrHSkip.hSkipAttr.maxSameSceneCnt = 0;
                rc_attr->attrHSkip.hSkipAttr.bEnableScenecut = 0;
                rc_attr->attrHSkip.hSkipAttr.bBlackEnhance = 0;
                rc_attr->attrHSkip.maxHSkipType = IMP_Encoder_STYPE_N1X;
            }
			else if (g_video_args->video.video_stream[i].rc_mode == IPC_RC_MODE_SMART){
                rc_attr->attrRcMode.rcMode = ENC_RC_MODE_SMART;
                rc_attr->attrRcMode.attrH264Smart.maxQp = MAX_QP;
                rc_attr->attrRcMode.attrH264Smart.minQp = MIN_QP;
                rc_attr->attrRcMode.attrH264Smart.staticTime = 2;
                rc_attr->attrRcMode.attrH264Smart.maxBitRate = g_video_args->video.video_stream[i].bitrate;
                rc_attr->attrRcMode.attrH264Smart.iBiasLvl = 0;
                rc_attr->attrRcMode.attrH264Smart.changePos = 80;
                rc_attr->attrRcMode.attrH264Smart.qualityLvl = 2;
                rc_attr->attrRcMode.attrH264Smart.frmQPStep = 3;
                rc_attr->attrRcMode.attrH264Smart.gopQPStep = rc_attr->outFrmRate.frmRateNum / rc_attr->outFrmRate.frmRateDen;
                rc_attr->attrRcMode.attrH264Smart.gopRelation = false;

                rc_attr->attrHSkip.hSkipAttr.skipType = IMP_Encoder_STYPE_N1X;
				rc_attr->attrHSkip.hSkipAttr.m = rc_attr->maxGop - 1;
                rc_attr->attrHSkip.hSkipAttr.n = 1;
                rc_attr->attrHSkip.hSkipAttr.maxSameSceneCnt = 1;
                rc_attr->attrHSkip.hSkipAttr.bEnableScenecut = 0;
                rc_attr->attrHSkip.hSkipAttr.bBlackEnhance = 0;
                rc_attr->attrHSkip.maxHSkipType = IMP_Encoder_STYPE_N1X;
            } 
			else { 
                rc_attr->attrRcMode.rcMode = ENC_RC_MODE_FIXQP;
                rc_attr->attrRcMode.attrH264FixQp.qp = MAX_QP;

                rc_attr->attrHSkip.hSkipAttr.skipType = IMP_Encoder_STYPE_N1X;
                rc_attr->attrHSkip.hSkipAttr.m = 0;
                rc_attr->attrHSkip.hSkipAttr.n = 0;
                rc_attr->attrHSkip.hSkipAttr.maxSameSceneCnt = 0;
                rc_attr->attrHSkip.hSkipAttr.bEnableScenecut = 0;
                rc_attr->attrHSkip.hSkipAttr.bBlackEnhance = 0;
                rc_attr->attrHSkip.maxHSkipType = IMP_Encoder_STYPE_N1X;
            }
	
			ret = IMP_Encoder_CreateChn(i, &channel_attr);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_Encoder_CreateChn(%d) error !\n", i);
				return -1;
			}
	
			ret = IMP_Encoder_RegisterChn(i, i);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_Encoder_RegisterChn(%d, %d) error: %d\n",
						i, i, ret);
				return -1;
			}
		}
	}

	DBG("ipc_encoder_init OK\n");	
	return 0;
}

int ipc_framesource_streamon()
{
	int ret = 0, i = 0;
	/* Enable channels */
	for (i = 0; i < FS_CHN_NUM; i++) {
		if (g_video_args->video.video_stream[i].enable) {
			ret = IMP_FrameSource_EnableChn(i);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_FrameSource_EnableChn(%d) error: %d\n", i, ret);
				return -1;
			}
		}
	}

	DBG("ipc_framesource_streamon OK\n");
	return 0;
}

static int ipc_h264_cb(int stream, char *frame, unsigned long len, int keyframe, unsigned long int timestamp)
{
	g_video_args->video.video_cb[stream](stream, frame, len, keyframe, timestamp);
/*
	if(stream == MAIN_CHANNEL)
		frame_pool_add(gHndMainFramePool, frame, len, keyframe, timestamp/1000);
	else if(stream == SECOND_CHANNEL)
		frame_pool_add(gHndSubFramePool, frame, len, keyframe, timestamp/1000);
*/
	return 0;
}

void *ipc_get_h264_stream(void *args)
{
	int ret, cont, len, keyframe, i = 0;  
	i = (int ) (*((int*)args));

	ret = IMP_Encoder_StartRecvPic(i);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_Encoder_StartRecvPic(%d) failed\n", i);
		return ((void *)-1);
	}

	char pr_name[64];
    memset(pr_name,0,sizeof(pr_name));
    sprintf(pr_name,"get_stream[%d]",i);
    prctl(PR_SET_NAME,pr_name);

	while(g_video_args->running)
	{
		IMPEncoderStream stream;
		memset(&stream, 0, sizeof(IMPEncoderStream));
	
		ret = IMP_Encoder_PollingStream(i, 1000);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "Polling stream[%d] timeout\n", i);
			continue;
		}
		
		/* Get H264 Stream */
		ret = IMP_Encoder_GetStream(i, &stream, 1);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_Encoder_GetStream() failed\n");
			return ((void *)-1);
		}
		else{
			IMPEncoderCHNStat stat;
    		if(IMP_Encoder_Query(i, &stat) != 0){
				continue;
			}
		}

		for(cont = 0, len = 0; cont < stream.packCount; cont++)
        	len += stream.pack[cont].length;

    	if(stream.refType == IMP_Encoder_FSTYPE_IDR )
        	keyframe = 1;
    	else
        	keyframe = 0;

    	ipc_h264_cb(i, (char *)stream.pack[0].virAddr, len, keyframe, (unsigned long int)stream.pack[0].timestamp);
	
		IMP_Encoder_ReleaseStream(i, &stream);
	}

	ret = IMP_Encoder_StopRecvPic(i);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_Encoder_StopRecvPic(%d) failed\n", i);
		return ((void *)-1);
	}

	DBG("ipc_get_h264_stream[%d] exit\n", i);
	pthread_exit(0);
}

static int ipc_module_bind(void)
{
	int i, ret =0;

	for (i = 0; i < FS_CHN_NUM; i++) {
		if (g_video_args->video.video_stream[i].enable) {

			if(i == MAIN_CHANNEL){
				ret = IMP_System_Bind(&g_video_args->frame_source[i], &g_video_args->osd_cell[i]);
				if (ret < 0) {
					IMP_LOG_ERR(TAG, "Bind FrameSource channel%d and OSD failed\n",i);
					return -1;
				}
			}else if(i == SECOND_CHANNEL){
				ret = IMP_System_Bind(&g_video_args->frame_source[i], &g_video_args->ivs_grp);
				if (ret < 0) {
					IMP_LOG_ERR(TAG, "Bind FrameSource channel%d and OSD failed\n",i);
					return -1;
				}

				ret = IMP_System_Bind(&g_video_args->ivs_grp, &g_video_args->osd_cell[i]);
				if (ret < 0) {
					IMP_LOG_ERR(TAG, "Bind FrameSource channel%d and OSD failed\n",i);
					return -1;
				}
			}
			
			ret = IMP_System_Bind(&g_video_args->osd_cell[i], &g_video_args->imp_encoder[i]);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "Bind OSD channel%d and Encoder failed\n",i);
				return -1;
			}	
		}
	}

	DBG("ipc_module_bind OK\n");
	return ret;
}

static int ipc_grp_init(void)
{
	int i, ret = 0;

	for (i = 0; i < FS_CHN_NUM; i++) {
		if (g_video_args->video.video_stream[i].enable) {
			ret = IMP_Encoder_CreateGroup(i);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_Encoder_CreateGroup(%d) error !\n", i);
				return -1;
			}
		}
	}

	DBG("ipc_grp_init OK\n");
	return ret;
}

static void ipc_video_attr_init(void)
{
	int i;
	for(i = 0; i < FS_CHN_NUM; i++)
	{
		if(g_video_args->video.video_stream[i].enable){

            g_video_args->framesource_chn[i].pixFmt = PIX_FMT_NV12;
            g_video_args->framesource_chn[i].outFrmRateNum = g_video_args->video.video_stream[i].fps;
            g_video_args->framesource_chn[i].outFrmRateDen = 1;
            g_video_args->framesource_chn[i].nrVBs = NRVBS;
            g_video_args->framesource_chn[i].type = FS_PHY_CHANNEL;

            g_video_args->framesource_chn[i].crop.enable = 1;
            g_video_args->framesource_chn[i].crop.top = 0;
            g_video_args->framesource_chn[i].crop.left = 0;
            g_video_args->framesource_chn[i].crop.width = g_video_args->video.video_stream[MAIN_CHANNEL].width;
            g_video_args->framesource_chn[i].crop.height = g_video_args->video.video_stream[MAIN_CHANNEL].height;

            g_video_args->framesource_chn[i].picWidth = g_video_args->video.video_stream[i].width;
            g_video_args->framesource_chn[i].picHeight = g_video_args->video.video_stream[i].height;
 
			if(i == MAIN_CHANNEL){
				g_video_args->framesource_chn[i].scaler.enable = 0;
			}
			else if(i == SECOND_CHANNEL){
            	g_video_args->framesource_chn[i].scaler.enable = 1;
            	g_video_args->framesource_chn[i].scaler.outwidth = g_video_args->video.video_stream[i].width;
            	g_video_args->framesource_chn[i].scaler.outheight = g_video_args->video.video_stream[i].height;
			}
		
			g_video_args->frame_source[i].deviceID = DEV_ID_FS;
			g_video_args->frame_source[i].groupID = i;
			g_video_args->frame_source[i].outputID = 0;

			g_video_args->imp_encoder[i].deviceID = DEV_ID_ENC;
			g_video_args->imp_encoder[i].groupID = i;
			g_video_args->imp_encoder[i].outputID = 0;

			g_video_args->osd_cell[i].deviceID = DEV_ID_OSD;
			g_video_args->osd_cell[i].groupID = i;
			g_video_args->osd_cell[i].outputID = 0;
		}
			
	}

	g_video_args->ivs_grp.deviceID = DEV_ID_IVS;
	g_video_args->ivs_grp.groupID = 0;
	g_video_args->ivs_grp.outputID = 0;
	
}

int ipc_video_init(IPC_VIDEO_S *video)
{
	int i, ret;

	CHECK(g_video_args == NULL, -1, "ipc_video_init failed\n");
	CHECK(video != NULL, -1, "ipc_video_init failed\n");

	g_video_args = (IPC_VIDEO_INFO_S*)malloc(sizeof(IPC_VIDEO_INFO_S));
	CHECK(g_video_args != NULL, -1, "ipc_video_init failed\n");
  
    memset(g_video_args, 0, sizeof(IPC_VIDEO_INFO_S));
    pthread_mutex_init(&g_video_args->pthread_mutex, NULL);
	memcpy(&g_video_args->video, video, sizeof(IPC_VIDEO_S));
	
	/* Step.1 Video attr init */
	ipc_video_attr_init();

	/* Step.2 System init */
	ret = ipc_system_init();
	CHECK(ret == 0, -1, "ipc_system_init failed\n");
		
	/* Step.3 FrameSource init */
	ret = ipc_framesource_init();
	CHECK(ret == 0, -1, "ipc_framesource_init failed\n");

	/* Step.4 Group init */
	ret = ipc_grp_init();
	CHECK(ret == 0, -1, "ipc_grp_init failed\n");
	
	/* Step.5 Encoder Channel init */
	ret = ipc_encoder_init();
	CHECK(ret == 0, -1, "ipc_encoder_init failed\n");

	/* Step.6 OSD init */
	ret = ipc_osd_init(&g_video_args->video);
	CHECK(ret == 0, -1, "ipc_osd_init failed\n");

	/* Step.7 IVS init */
	ret = ipc_ivs_init();
	CHECK(ret == 0, -1, "ipc_ivs_init failed\n");

	/* Step.8 Bind device*/
	ret = ipc_module_bind();
	CHECK(ret == 0, -1, "ipc_module_bind failed\n");

	/* Step.9 OSD start*/	
	ret = ipc_osd_start();
	CHECK(ret == 0, -1, "ipc_osd_start failed\n");

	/* Step.10 IRCUT start*/	
	ret = ipc_ircut_init();
	CHECK(ret == 0, -1, "ipc_ircut_init failed\n");
		
	/* Step.11 Stream On */
	IMP_FrameSource_SetFrameDepth(0, 0);
	ret = ipc_framesource_streamon();
	CHECK(ret == 0, -1, "ipc_framesource_streamon failed\n");

	/* Step.12 Get stream */
    g_video_args->running = 1;
	ipc_ircut_mode_set(IPC_IR_MODE_AUTO);
	
	for(i = 0; i < FS_CHN_NUM; i++){
		if(g_video_args->video.video_stream[i].enable){
    		ret = pthread_create(&g_video_args->pthread_id[i], NULL, ipc_get_h264_stream, &g_video_args->video.video_stream[i].channel_id);
			CHECK(ret == 0, -1, "ipc_get_h264_stream[%d] failed\n", i);
		}
	}
	
	return 0;
}

int ipc_framesource_streamoff()
{
	int ret = 0, i = 0;
	/* Enable channels */
	for (i = 0; i < FS_CHN_NUM; i++) {
		if (g_video_args->video.video_stream[i].enable){
			pthread_join(g_video_args->pthread_id[i], NULL);
		
			ret = IMP_FrameSource_DisableChn(i);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_FrameSource_DisableChn(%d) error: %d\n", i, ret);
				return -1;
			}
		}
	}

	DBG("ipc_framesource_streamoff OK\n");	
	return 0;
}

static int ipc_encoder_chn_exit(int encChn)
{
	int ret;
	IMPEncoderCHNStat chn_stat;
	ret = IMP_Encoder_Query(encChn, &chn_stat);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_Encoder_Query(%d) error: %d\n",
					encChn, ret);
		return -1;
	}

	if (chn_stat.registered) {
		ret = IMP_Encoder_UnRegisterChn(encChn);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_Encoder_UnRegisterChn(%d) error: %d\n",
						encChn, ret);
			return -1;
		}

		ret = IMP_Encoder_DestroyChn(encChn);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_Encoder_DestroyChn(%d) error: %d\n",
						encChn, ret);
			return -1;
		}
	}

	DBG("ipc_encoder_chn[%d]_exit OK\n", encChn);
	return 0;
}

static int ipc_encoder_exit(void)
{
	int i, ret = 0;

	for(i = 0; i < FS_CHN_NUM; i++){
		if(g_video_args->video.video_stream[i].enable){
			ret = ipc_encoder_chn_exit(i);
			CHECK(ret == 0, -1, "ipc_encoder_chn[%d]_exit failed\n", i);
			
			ret = IMP_Encoder_DestroyGroup(i);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_Encoder_DestroyGroup(%d) error: %d\n", i, ret);
				return -1;
			}
				
		}
	}

	DBG("ipc_encoder_exit OK\n");
	return ret;
}

int ipc_framesource_exit()
{
	int ret,i;

	for (i = 0; i <  FS_CHN_NUM; i++) {
		if (g_video_args->video.video_stream[i].enable) {
			/*Destroy channel i*/
			ret = IMP_FrameSource_DestroyChn(i);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_FrameSource_DestroyChn() error: %d\n", ret);
				return -1;
			}
		}
	}

	DBG("ipc_framesource_exit OK\n");
	return 0;
}

int ipc_system_exit()
{
	int ret = 0;

	IMP_System_Exit();

	ret = IMP_ISP_DisableSensor();
	if(ret < 0){
		IMP_LOG_ERR(TAG, "failed to EnableSensor\n");
		return -1;
	}

	ret = IMP_ISP_DelSensor(&sensor_info);
	if(ret < 0){
		IMP_LOG_ERR(TAG, "failed to AddSensor\n");
		return -1;
	}

	ret = IMP_ISP_DisableTuning();
	if(ret < 0){
		IMP_LOG_ERR(TAG, "IMP_ISP_DisableTuning failed\n");
		return -1;
	}

	if(IMP_ISP_Close()){
		IMP_LOG_ERR(TAG, "failed to open ISP\n");
		return -1;
	}

	DBG("ipc_system_exit OK\n");
	return 0;
}

static int ipc_module_unbind(void)
{
	int i, ret = 0;

	for (i = 0; i < FS_CHN_NUM; i++) {
		if (g_video_args->video.video_stream[i].enable) {
			ret = IMP_System_UnBind(&g_video_args->osd_cell[i], &g_video_args->imp_encoder[i]);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "UnBind OSD%d and Encoder failed\n",i);
				return -1;
			}

			if(i == MAIN_CHANNEL){
				ret = IMP_System_UnBind(&g_video_args->frame_source[i], &g_video_args->osd_cell[i]);
				if (ret < 0) {
					IMP_LOG_ERR(TAG, "UnBind OSD%d and FS failed\n",i);
					return -1;
				}
			}else if(i == SECOND_CHANNEL){
				ret = IMP_System_UnBind(&g_video_args->ivs_grp, &g_video_args->osd_cell[i]);
				if (ret < 0) {
					IMP_LOG_ERR(TAG, "UnBind OSD%d and FS failed\n",i);
					return -1;
				}

				ret = IMP_System_UnBind(&g_video_args->frame_source[i], &g_video_args->ivs_grp);
				if (ret < 0) {
					IMP_LOG_ERR(TAG, "UnBind OSD%d and FS failed\n",i);
					return -1;
				}
			}
		}
	}
	
	DBG("ipc_module_unbind OK\n");
	return 0;
}

static int ipc_grp_exit(void)
{
	int i;

	for(i = 0; i < FS_CHN_NUM; i++){
		if(g_video_args->video.video_stream[i].enable){
			if(IMP_OSD_DestroyGroup(i) < 0){
				IMP_LOG_ERR(TAG, "IMP_OSD_DestroyGroup(%d) error\n", i);
				return -1;
			}
		}
	}

	DBG("ipc_grp_exit OK\n");
	return 0;
}

int ipc_video_exit(void)
{
	int ret;

	/* Step.1 OSD exit */
	ipc_osd_exit();	
	
	/* Step.2 Stream Off */
	g_video_args->running = 0;
	ret = ipc_framesource_streamoff();
	CHECK(ret == 0, -1, "ipc_framesource_streamoff failed\n");

	/* Step.3 UnBind device */
	ret = ipc_module_unbind();
	CHECK(ret == 0, -1, "ipc_module_unbind failed\n");

	/* Step.4 IVS exit */
	ret = ipc_ivs_exit();
	CHECK(ret == 0, -1, "ipc_ivs_exit failed\n");

	/* Step.5 Group exit */
	ret = ipc_grp_exit();
	CHECK(ret == 0, -1, "ipc_grp_exit failed\n");

	/* Step.6 Encoder exit */
	ret = ipc_encoder_exit();
	CHECK(ret == 0, -1, "ipc_encoder_exit failed\n");

	/* Step.7 FrameSource exit */
	ret = ipc_framesource_exit();
	CHECK(ret == 0, -1, "ipc_framesource_exit failed\n");	

	/* Step.8 IRCUT exit */
	ret = ipc_ircut_exit();
	CHECK(ret == 0, -1, "ipc_ircut_exit failed\n");	

	/* Step.9 System exit */
	ret = ipc_system_exit();
	CHECK(ret == 0, -1, "ipc_system_exit failed\n");

	if(g_video_args){
		free(g_video_args);
    	g_video_args = NULL;
	}

	return 0;
}

