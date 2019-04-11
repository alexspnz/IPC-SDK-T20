/*
 * ipc_osd.c
 * Author:yuanhao@yingshixun.com
 * Date:2019.2
 */

#include "imp_log.h"
#include "ipc_video.h"
#include "imp_osd.h"
#include "ipc_osd.h"
#include "ipc_comm.h"
#include "bgramapinfo.h"
#include "ipc_debug.h"

#include <sys/queue.h>

#define TAG "Sample-OSD"

/*OSD的使用一般有以下几个步骤:
 *   创建OSD组
 *   绑定OSD组到系统中
 *   创建OSD区域
 *   注册OSD区域到OSD组中
 *   设置OSD组区域属性和区域属性
 *   设置OSD功能开关
*/


typedef struct ipc_osd_info_s
{
	IPC_VIDEO_S video_info;
	IPC_OSD_S osd_info[FS_CHN_NUM];
	IMPRgnHandle rHanderFont[FS_CHN_NUM];
    pthread_t pthread_id[FS_CHN_NUM];
    pthread_mutex_t pthread_mutex;
	unsigned char running;
}
IPC_OSD_INFO_S;
static IPC_OSD_INFO_S* g_osd_args = NULL;

static void *ipc_osd_update_thread(void *args)
{
	int ret;

	/*generate time*/
	char DateStr[40];
	time_t currTime;
	struct tm *currDate;
	unsigned i = 0, j = 0;
	void *dateData = NULL;
	IMPOSDRgnAttrData rAttrData;
	
	int grpNum = 0;
	grpNum = (int ) (*((int*)args));

	char pr_name[64];
    memset(pr_name,0,sizeof(pr_name));
    sprintf(pr_name,"osd_stream[%d]",grpNum);
    prctl(PR_SET_NAME,pr_name);

	unsigned char font_offset = 0;
    if(grpNum == MAIN_CHANNEL)   
		font_offset = 0;		
    else 
		font_offset = 13;		
	
	uint32_t *data = malloc(OSD_LETTER_NUM * g_osd_args->osd_info[grpNum].region_height * g_osd_args->osd_info[grpNum].region_width * sizeof(uint32_t));

	ret = IMP_OSD_ShowRgn(g_osd_args->rHanderFont[grpNum], grpNum, 1);
	if (ret != 0) {
		IMP_LOG_ERR(TAG, "IMP_OSD_ShowRgn(%d) timeStamp error\n", grpNum);
		return (void *)-1;
	}
	
	while(g_osd_args->running) {
		int penpos_t = 0;
		int fontadv = 0;

		time(&currTime);
		currDate = localtime(&currTime);

		memset(DateStr, 0, 40);
		strftime(DateStr, 40, "%Y-%m-%d %I:%M:%S", currDate);
		for (i = 0; i < OSD_LETTER_NUM; i++) {
			switch(DateStr[i]) {
				case '0' ... '9':
					dateData = (void *)gBgramap[DateStr[i] - '0' + font_offset].pdata;
					fontadv = gBgramap[DateStr[i] - '0' + font_offset].width;
					penpos_t += gBgramap[DateStr[i] - '0' + font_offset].width;
					break;
				case '-':
					dateData = (void *)gBgramap[10 + font_offset].pdata;
					fontadv = gBgramap[10 + font_offset].width;
					penpos_t += gBgramap[10 + font_offset].width;
					break;
				case ' ':
					dateData = (void *)gBgramap[11 + font_offset].pdata;
					fontadv = gBgramap[11 + font_offset].width;
					penpos_t += gBgramap[11 + font_offset].width;
					break;
				case ':':
					dateData = (void *)gBgramap[12 + font_offset].pdata;
					fontadv = gBgramap[12 + font_offset].width;
					penpos_t += gBgramap[12 + font_offset].width;
					break;
				default:
					break;
			}

			for (j = 0; j < g_osd_args->osd_info[grpNum].region_height; j++) {
				memcpy((void *)((uint32_t *)data + j*OSD_LETTER_NUM*g_osd_args->osd_info[grpNum].region_width + penpos_t),
						(void *)((uint32_t *)dateData + j*fontadv), fontadv*sizeof(uint32_t));
			}
		}
		rAttrData.picData.pData = data;
		ret = IMP_OSD_UpdateRgnAttrData(g_osd_args->rHanderFont[grpNum], &rAttrData);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_OSD_Update[%d] error\n", grpNum);
		}

		sleep(1);
	}

	ret = IMP_OSD_ShowRgn(g_osd_args->rHanderFont[grpNum], grpNum, 0);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_OSD_ShowRgn[%d] close error\n", grpNum);
	}
	
	if(data){
		free(data);
		data = NULL;
	}

	DBG("ipc_osd_update_thread[%d] exit\n", grpNum);	
	pthread_exit(0);
}

static int ipc_osd_grp_init(void)
{
	int i;
	for(i = 0; i < FS_CHN_NUM; i++){
		if(g_osd_args->video_info.video_stream[i].enable){		
			if (IMP_OSD_CreateGroup(i) < 0) {
				IMP_LOG_ERR(TAG, "IMP_OSD_CreateGroup(%d) error !\n", i);
				return -1;
			}
		}
	}

	DBG("ipc_osd_grp_init OK\n");
	return 0;
}

static int ipc_osd_rgn_init(void)
{
	int i;
	for(i = 0; i < FS_CHN_NUM; i++){
		if(g_osd_args->video_info.video_stream[i].enable){	
			g_osd_args->rHanderFont[i] = IMP_OSD_CreateRgn(NULL);
			if (g_osd_args->rHanderFont[i] == INVHANDLE) {
				IMP_LOG_ERR(TAG, "IMP_OSD_CreateRgn error !\n");
				return -1;
			}	

			if(IMP_OSD_RegisterRgn(g_osd_args->rHanderFont[i], i, NULL) < 0){
				IMP_LOG_ERR(TAG, "IVS IMP_OSD_RegisterRgn failed\n");
				return -1;
			}
		}
	}

	DBG("ipc_osd_rgn_init OK\n");
	return 0;
}

static int ipc_osd_rgnattr_init(void)
{
	int i, ret;
	IMPOSDRgnAttr rAttrFont;
	IMPOSDGrpRgnAttr grAttrFont;
	
	for(i = 0; i < FS_CHN_NUM; i++){
		if(g_osd_args->video_info.video_stream[i].enable){	
			memset(&rAttrFont, 0, sizeof(IMPOSDRgnAttr));
			rAttrFont.type = OSD_REG_PIC;
			rAttrFont.rect.p0.x = g_osd_args->osd_info[i].x_start;
			rAttrFont.rect.p0.y = g_osd_args->osd_info[i].y_start;
			rAttrFont.rect.p1.x = g_osd_args->osd_info[i].x_end;   
			rAttrFont.rect.p1.y = g_osd_args->osd_info[i].y_end;
			rAttrFont.fmt = PIX_FMT_BGRA;
			rAttrFont.data.picData.pData = NULL;

			DBG("###Channel=%d, x_start=%d, y_start=%d, x_end=%d, y_end=%d, region_width=%d, region_height=%d####\n",
				i, g_osd_args->osd_info[i].x_start, g_osd_args->osd_info[i].y_start, g_osd_args->osd_info[i].x_end,
				g_osd_args->osd_info[i].y_end, g_osd_args->osd_info[i].region_width, g_osd_args->osd_info[i].region_height);

			ret = IMP_OSD_SetRgnAttr(g_osd_args->rHanderFont[i], &rAttrFont);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_OSD_SetRgnAttr error !\n");
				return -1;
			}

			ret = IMP_OSD_GetGrpRgnAttr(g_osd_args->rHanderFont[i], i, &grAttrFont);
			if (ret < 0){
				IMP_LOG_ERR(TAG, "IMP_OSD_GetGrpRgnAttr error !\n");
				return -1;
			}
	
			memset(&grAttrFont, 0, sizeof(IMPOSDGrpRgnAttr));
			grAttrFont.show = 0;
			grAttrFont.gAlphaEn = 1;
			grAttrFont.fgAlhpa = 0xff;
			grAttrFont.layer = 3;
			
			ret = IMP_OSD_SetGrpRgnAttr(g_osd_args->rHanderFont[i], i, &grAttrFont); 
			if (ret < 0){
				IMP_LOG_ERR(TAG, "IMP_OSD_SetGrpRgnAttr error !\n");
				return -1;
			}
			
			ret = IMP_OSD_Start(i); 
			if (ret < 0){
				IMP_LOG_ERR(TAG, "IMP_OSD_Start error!\n");
				return -1;
			}
		}
	}

	DBG("ipc_osd_rgnattr_init OK\n");
	return  0;
}

int ipc_osd_start()
{
	int i, ret;
	
	ret = ipc_osd_rgn_init();
	CHECK(ret == 0, -1, "ipc_osd_rgn_init failed\n");

	ret = ipc_osd_rgnattr_init();
	CHECK(ret == 0, -1, "ipc_osd_rgnattr_init failed\n");

	g_osd_args->running = 1;
	for(i = 0; i < FS_CHN_NUM; i++){
		if(g_osd_args->video_info.video_stream[i].enable){	
			ret = pthread_create(&g_osd_args->pthread_id[i], NULL, ipc_osd_update_thread, &g_osd_args->video_info.video_stream[i].channel_id);
			CHECK(ret == 0, -1, "ipc_osd_update_thread[%d] failed\n", i);
		}
	}

	DBG("ipc_osd_start OK\n");
    return 0;
}

static void ipc_osd_attr_init()
{
	int i;
	
	for(i = 0; i < FS_CHN_NUM; i++){
		if(g_osd_args->video_info.video_stream[i].enable){
			memset(&g_osd_args->osd_info[i], 0, sizeof(IPC_OSD_S));
			if(i == MAIN_CHANNEL){
				g_osd_args->osd_info[i].enable = 1;
				g_osd_args->osd_info[i].region_height = MAIN_REGION_HEIGHT;
				g_osd_args->osd_info[i].region_width = MAIN_REGION_WIDTH;
				g_osd_args->osd_info[i].x_start = g_osd_args->video_info.video_stream[i].width - MAIN_REGION_WIDTH*OSD_LETTER_NUM;
				g_osd_args->osd_info[i].y_start = 0;
				g_osd_args->osd_info[i].x_end = g_osd_args->osd_info[i].x_start + MAIN_REGION_WIDTH*OSD_LETTER_NUM-1;
				g_osd_args->osd_info[i].y_end = g_osd_args->osd_info[i].y_start + MAIN_REGION_HEIGHT-1;
			}
			else if(i == SECOND_CHANNEL){
				g_osd_args->osd_info[i].enable = 1;
				g_osd_args->osd_info[i].region_height = SECOND_REGION_HEIGHT;
				g_osd_args->osd_info[i].region_width = SECOND_REGION_WIDTH;
				g_osd_args->osd_info[i].x_start = g_osd_args->video_info.video_stream[i].width - SECOND_REGION_WIDTH*OSD_LETTER_NUM;
				g_osd_args->osd_info[i].y_start = 0;
				g_osd_args->osd_info[i].x_end = g_osd_args->osd_info[i].x_start + SECOND_REGION_WIDTH*OSD_LETTER_NUM-1;
				g_osd_args->osd_info[i].y_end = g_osd_args->osd_info[i].y_start + SECOND_REGION_HEIGHT-1;

			}else if(i == THIRD_CHANNEL){
				g_osd_args->osd_info[i].enable = 1;
				g_osd_args->osd_info[i].region_height = THIRD_REGION_HEIGHT;
				g_osd_args->osd_info[i].region_width = THIRD_REGION_WIDTH;
				g_osd_args->osd_info[i].x_start = g_osd_args->video_info.video_stream[i].width - THIRD_REGION_WIDTH*OSD_LETTER_NUM;
				g_osd_args->osd_info[i].y_start = 0;
				g_osd_args->osd_info[i].x_end = g_osd_args->osd_info[i].x_start + THIRD_REGION_WIDTH*OSD_LETTER_NUM-1;
				g_osd_args->osd_info[i].y_end = g_osd_args->osd_info[i].y_start + THIRD_REGION_HEIGHT-1;
			}
		}
	}	

}

int ipc_osd_init(IPC_VIDEO_S *video)
{
	int ret = 0;

	CHECK(g_osd_args == NULL, -1, "ipc_osd_init failed\n");
	g_osd_args = (IPC_OSD_INFO_S*)malloc(sizeof(IPC_OSD_INFO_S));
	CHECK(g_osd_args != NULL, -1, "ipc_osd_init failed\n");
  
    memset(g_osd_args, 0, sizeof(IPC_OSD_INFO_S));
    pthread_mutex_init(&g_osd_args->pthread_mutex, NULL);
	memcpy(&g_osd_args->video_info, video, sizeof(IPC_VIDEO_S));

	ret = ipc_osd_grp_init();
	CHECK(ret == 0, -1, "ipc_osd_grp_init failed\n");

	ipc_osd_attr_init();

	DBG("ipc_osd_init OK\n");
	return ret;
}

void ipc_osd_exit(void)
{
	int i, ret = 0;
	g_osd_args->running = 0;

	for(i = 0; i < FS_CHN_NUM; i++){	
		if(g_osd_args->video_info.video_stream[i].enable){	
			pthread_join(g_osd_args->pthread_id[i], NULL);
/*
			ret = IMP_OSD_Stop(i);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_OSD_Stop error\n");
			}
*/	
			ret = IMP_OSD_UnRegisterRgn(g_osd_args->rHanderFont[i], i);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_OSD_UnRegisterRgn timeStamp error\n");
			}

			IMP_OSD_DestroyRgn(g_osd_args->rHanderFont[i]);
		}
	}

	if(g_osd_args){
		free(g_osd_args);
		g_osd_args = NULL;
	}

	DBG("ipc_osd_exit OK\n");
	return ;
}

