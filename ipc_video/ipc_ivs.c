/*
 * ipc_ivs.c
 * Author:yuanhao@yingshixun.com
 * Date:2019.2
 */

#include "imp_log.h"
#include "imp_common.h"
#include "imp_system.h"
#include "imp_isp.h"
#include "imp_framesource.h"
#include "imp_encoder.h"
#include "imp_ivs.h"
#include "imp_ivs_move.h"

#include "ipc_comm.h"
#include "ipc_video.h"
#include "ipc_ivs.h"
#include "ipc_debug.h"

#define TAG "Sample-IVS"

typedef struct ipc_motion_info_s
{
	int grp_num;
	int chn_num;
	IMPIVSInterface *interface;
	pthread_t pthread_id;
    pthread_mutex_t pthread_mutex;
	ipc_motion_detection_cb motion_cb;
    unsigned char running;
}
IPC_MOTION_INFO_S;
static IPC_MOTION_INFO_S* g_motion_args = NULL;


static void *ipc_motion_detection_proc(void *arg)
{
	int i = 0, ret = 0;
	int chn_num = (int)arg;
	IMP_IVS_MoveOutput *result = NULL;
	unsigned int md_result;		

	char pr_name[64];
    memset(pr_name,0,sizeof(pr_name));
    sprintf(pr_name,"motion_proc");
    prctl(PR_SET_NAME,pr_name);

	while(g_motion_args->running) {
		ret = IMP_IVS_PollingResult(chn_num, -1);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_IVS_PollingResult(%d, %d) failed\n", chn_num, IMP_IVS_DEFAULT_TIMEOUTMS);
			return (void *)-1;
		}
		ret = IMP_IVS_GetResult(chn_num, (void **)&result);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_IVS_GetResult(%d) failed\n", chn_num);
			return (void *)-1;
		}
//		IMP_LOG_INFO(TAG, "frame[%d], result->retRoi(%d,%d,%d,%d)\n", i, result->retRoi[0], result->retRoi[1], result->retRoi[2], result->retRoi[3]);

		md_result = 0;
        for (i = 0; i < 12; i++) {
            md_result |= (result->retRoi[i] << i);
        }

        if (md_result!=0) {
            g_motion_args->motion_cb(md_result);
        }
	
		ret = IMP_IVS_ReleaseResult(chn_num, (void *)result);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_IVS_ReleaseResult(%d) failed\n", chn_num);
			return (void *)-1;
		}
	}

	pthread_exit(0);
}

int ipc_motion_detection_start(ipc_motion_detection_cb cb)
{
	int ret = 0;
	IMP_IVS_MoveParam param;
	int i = 0, j = 0;

	if(NULL != g_motion_args){
		IMP_LOG_ERR(TAG, "ipc_motion_start failed\n");
		return -1;
	}

	g_motion_args = (IPC_MOTION_INFO_S*)malloc(sizeof(IPC_MOTION_INFO_S));
	if(NULL == g_motion_args){
		IMP_LOG_ERR(TAG, "ipc_motion_start failed\n");
		return -1;
	}
  
    memset(g_motion_args, 0, sizeof(IPC_MOTION_INFO_S));
    pthread_mutex_init(&g_motion_args->pthread_mutex, NULL);

	g_motion_args->grp_num = 0;
	g_motion_args->chn_num = 0;
	g_motion_args->motion_cb = cb;
    g_motion_args->running = 1;

	memset(&param, 0, sizeof(IMP_IVS_MoveParam));
	param.skipFrameCnt = 5;
	param.frameInfo.width = 640;
	param.frameInfo.height = 360;
	param.roiRectCnt = 4;

	for(i=0; i<param.roiRectCnt; i++){
	  param.sense[i] = 4;
	}

	/* printf("param.sense=%d, param.skipFrameCnt=%d, param.frameInfo.width=%d, param.frameInfo.height=%d\n", param.sense, param.skipFrameCnt, param.frameInfo.width, param.frameInfo.height); */
	for (i = 0; i < 4; i ++) {
        for (j = 0; j < 4; j ++) {
            if (((i * 4) + j) < 4) {
                param.roiRect[(i*4)+j].p0.x = j*(640/4);
                param.roiRect[(i*4)+j].p0.y = i*(360/3);
                param.roiRect[(i*4)+j].p1.x = (j+1)*(640/4) - 1;
                param.roiRect[(i*4)+j].p1.y = (i+1)*(360/3) - 1;
                param.sense[(i*4)+j] = 0;
            }
            else {
                param.roiRect[(i*4)+j].p0.x = 0;
                param.roiRect[(i*4)+j].p0.y = 0;
                param.roiRect[(i*4)+j].p1.x = param.frameInfo.width -1;
                param.roiRect[(i*4)+j].p1.y = param.frameInfo.height-1;
            }
        }
    }

	g_motion_args->interface = IMP_IVS_CreateMoveInterface(&param);
	if (g_motion_args->interface == NULL) {
		IMP_LOG_ERR(TAG, "IMP_IVS_CreateGroup(%d) failed\n", g_motion_args->grp_num);
		return -1;
	}

	ret = IMP_IVS_CreateChn(g_motion_args->chn_num, g_motion_args->interface);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_CreateChn(%d) failed\n", g_motion_args->chn_num);
		return -1;
	}

	ret = IMP_IVS_RegisterChn(g_motion_args->grp_num, g_motion_args->chn_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_RegisterChn(%d, %d) failed\n", g_motion_args->grp_num, g_motion_args->chn_num);
		return -1;
	}

	ret = IMP_IVS_StartRecvPic(g_motion_args->chn_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_StartRecvPic(%d) failed\n", g_motion_args->chn_num);
		return -1;
	}

	if (pthread_create(&g_motion_args->pthread_id, NULL, ipc_motion_detection_proc, (void *)g_motion_args->chn_num) < 0) {
		IMP_LOG_ERR(TAG, "create ipc_motion_detection_proc failed\n");
		return -1;
	}

	return 0;
}

int ipc_motion_detection_stop(void)
{
	int ret = 0;

	g_motion_args->running = 0;
	pthread_join(g_motion_args->pthread_id, NULL);

	ret = IMP_IVS_StopRecvPic(g_motion_args->chn_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_StopRecvPic(%d) failed\n", g_motion_args->chn_num);
		return -1;
	}
	sleep(1);

	ret = IMP_IVS_UnRegisterChn(g_motion_args->chn_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_UnRegisterChn(%d) failed\n", g_motion_args->chn_num);
		return -1;
	}

	ret = IMP_IVS_DestroyChn(g_motion_args->chn_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_DestroyChn(%d) failed\n", g_motion_args->chn_num);
		return -1;
	}

	IMP_IVS_DestroyMoveInterface(g_motion_args->interface);

	if(g_motion_args){
		free(g_motion_args);
		g_motion_args = NULL;
	}

	return 0;
}

/*Function: IVS for motion detction , only use SECOND FS*/
int ipc_ivs_init(void)
{
    int ret;
    /*add IVS group to SECOND stream*/
    ret = IMP_IVS_CreateGroup(0);   //IVS group num is 0
    if(ret < 0){
        IMP_LOG_ERR(TAG, "IMP_IVS_CreateGroup failed\n");
        return -1;
    }

	DBG("ipc_ivs_init OK\n");
    return ret;
}

int ipc_ivs_exit(void)
{
	int ret = 0;

	ret = IMP_IVS_DestroyGroup(0);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_DestroyGroup failed\n");
		return -1;
	}

	DBG("ipc_ivs_exit OK\n");
	return 0;
}

