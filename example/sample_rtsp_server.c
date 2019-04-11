/*
 * sample_video.c
 * Author:yuanhao@yingshixun.com
 * Date:2019.2
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

#include "../include/ipc_comm.h"
#include "../include/ipc_debug.h"
#include "../include/ipc_video.h"
#include "../include/ipc_ivs.h"
#include "../include/ipc_audio.h"
#include "../include/ipc_rtsp_server.h"

#define FILE_JPEG		"/tmp/mmcblk0p1/snapshot.jpeg"
static int _loop = 1;

void sigint(int dummy)
{
    printf("got int signal,exit!\n");
    _loop = 0;
}

void init_signals(void)
{
    struct sigaction sa;

    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGINT);

    sa.sa_handler = sigint;
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = sigint;
    sigaction(SIGINT, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);
}

int get_audio_cb(char *frame, unsigned long len, double timestamp)
{
	printf("audio_len = %d\n", len);
	return 0;
}

static int get_stream0_cb(int stream_id, char *frame, unsigned long len, int keyframe, double timestamp)
{
	return 0;
}

static int get_stream1_cb(int stream_id, char *frame, unsigned long len, int keyframe, double timestamp)
{
	return 0;
}

static void motion_cb(int result)
{
//	printf("motion_cb result = %d\n", result);

	return ;
}

static int get_jpeg(void)
{
	int ret;
	int len = 400*1024;

	char *pic_buff = (char *)malloc(len);
    if(NULL == pic_buff){
        printf("malloc buffer Err with %s\n",strerror(errno));
        return -1;
    }

    if(ipc_get_jpeg(pic_buff,&len) == 0)
    {
    	remove(FILE_JPEG);
     	FILE *fp = fopen(FILE_JPEG,"wb+");
      	fwrite(pic_buff,1,len,fp);
      	fclose(fp);
    }
	
    free(pic_buff);
	return 0;
}

static int set_bitrate(void)
{	
	int ret = 0;

	ret = ipc_set_bitrate(0, 512);
	
	return ret;
}

static int set_Iframe(void)
{	
	int ret = 0;

	ret = ipc_set_Iframe(0);

	return ret;
}


int main(int argc, char **argv)
{
	init_signals();

	IPC_VIDEO_S video;
    memset(&video, 0, sizeof(video));

	video.video_cb[0] = get_stream0_cb;
	video.video_stream[0].enable = 1;
	video.video_stream[0].bitrate = 768;
	video.video_stream[0].fps = 15;
	video.video_stream[0].gop = 2;
	video.video_stream[0].width = 1920;
	video.video_stream[0].height = 1080;
	video.video_stream[0].rc_mode = IPC_RC_MODE_SMART;
	video.video_stream[0].channel_id = MAIN_CHANNEL;

	video.video_cb[1] = get_stream1_cb;
	video.video_stream[1].enable = 1;
	video.video_stream[1].bitrate = 512;
	video.video_stream[1].fps = 15;
	video.video_stream[1].gop = 2;
	video.video_stream[1].width = 640;
	video.video_stream[1].height = 360;
	video.video_stream[1].rc_mode = IPC_RC_MODE_SMART;
	video.video_stream[1].channel_id = SECOND_CHANNEL;
	
	ipc_video_init(&video);
	ipc_motion_detection_start(motion_cb);
	ipc_jpeg_init(1280, 720);

	IPC_AUDIO_S audio;
	memset(&audio, 0, sizeof(audio));
	audio.audio_cb = get_audio_cb;		
	ipc_audio_init(&audio);

	handle hndRtsps = rtsps_init(554);
	while(_loop){

		usleep(100);

		if(access("/tmp/set_Iframe", F_OK) == 0){
			remove("/tmp/set_Iframe");
			set_Iframe();
		}

		if(access("/tmp/set_bitrate", F_OK) == 0){
			remove("/tmp/set_bitrate");
			set_bitrate();
		}

		if(access("/tmp/snapshot", F_OK) == 0){
			remove("/tmp/snapshot");
			get_jpeg();
		}
	}

	rtsps_destroy(hndRtsps);
	ipc_audio_exit();	
	ipc_jpeg_exit();
	ipc_motion_detection_stop();
	ipc_video_exit();	

	return 0;
}

