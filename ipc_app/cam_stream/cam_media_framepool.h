
#ifndef __CAM_MEDIA_FRAMEPOOL_H__
#define __CAM_MEDIA_FRAMEPOOL_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"
#include "ipc_debug.h"

#define MAX_NODE_NUM 30

typedef struct av_node{
	char *data;
	int size;
	int keyframe;
	int read_mark;
	unsigned long int timestamp;
	unsigned char frame_num;
}av_node;

typedef struct av_buffer{
	char *buffer_start;
	int buffer_offset;
	int buffer_sum_size;
	int max_node_num;
	int min_node_num;
	int write_pos;
	int read_pos;
	int polling_once;
	int running;
	int Pframe_pos;
	int wait_Iframe;
	struct av_node node[MAX_NODE_NUM];	
}av_buffer;

typedef enum
{
 	AV_BUFFER_NULL =  0,
	AV_BUFFER_FULL =  1,
	AV_BUFFER_ERR  = -1,
}AV_BUFFER_STATUS;

av_buffer *Cam_FramePool_Init(int buf_size, int min_node_num, int max_node_num);
void Cam_FramePool_UnInit(av_buffer *buf);

int Cam_FramePool_Write(av_buffer *buf, char *data, int size, int keyframe, unsigned long int timestamp, unsigned char frame_num);
int Cam_FramePool_Read(av_buffer *buf, char **data, int *size, int *keyframe, unsigned long int *timestamp, unsigned char *frame_num);

void Cam_FramePool_Start(av_buffer *buf);
void Cam_FramePool_Stop(av_buffer *buf);

void Cam_FramePool_Clean(av_buffer *buf);
int Cam_FramePool_Getlen(av_buffer *buf);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

