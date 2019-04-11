
#ifndef __IPC_OSD_H__
#define __IPC_OSD_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_video.h"

#define MAIN_REGION_WIDTH		16
#define MAIN_REGION_HEIGHT		34
#define SECOND_REGION_WIDTH		8
#define SECOND_REGION_HEIGHT	18
#define THIRD_REGION_WIDTH		8
#define THIRD_REGION_HEIGHT		18
#define OSD_LETTER_NUM 20

typedef struct {
    int enable;
	int x_start;
	int y_start;
	int x_end;
	int y_end;
	char region_width;
	char region_height;
}IPC_OSD_S;


extern int ipc_osd_init(IPC_VIDEO_S *video_info);
extern int ipc_osd_start(void);
extern void ipc_osd_exit(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

