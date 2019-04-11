/*
 * sample_audio_play.c
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

#include "../include/ipc_audio.h"


int get_audio_cb(char *frame, unsigned long len, double pts)
{
//	printf("Audio frame_len = %ld\n", len);
	
	return 0;
}

int main(int argc, char **argv)
{	
	int ret;
	char filepath[64] = {0};

	strcpy(filepath, argv[1]);
	printf("play filepath:%s\n", filepath);

	IPC_AUDIO_S audio;
	memset(&audio, 0, sizeof(audio));	
	audio.audio_cb = get_audio_cb;
	
	ipc_audio_init(&audio);

	ret = ipc_audio_play_file(filepath);
	
	ipc_audio_exit();	
	return 0;
}

