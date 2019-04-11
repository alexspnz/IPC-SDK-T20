/*
 * sample_audio_cap.c
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

#define AI_PATH "/tmp/mmcblk0p1/ai.pcm"
static int ai_fd;
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

int get_audio_cb(char *frame, unsigned long len, double pts)
{
//	printf("Audio frame_len = %ld\n", len);
	int ret;
	
	ret = write(ai_fd, frame, len);
	if (ret != len) {
		printf("ai write error:%s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	init_signals();

	ai_fd = open(AI_PATH, O_RDWR | O_CREAT | O_TRUNC, 0777);
	if (ai_fd < 0) {
		printf("failed: %s\n", strerror(errno));
		return -1;
	}

	IPC_AUDIO_S audio;
	memset(&audio, 0, sizeof(audio));

	audio.audio_cb = get_audio_cb;		
	ipc_audio_init(&audio);
	
	while(_loop){

		sleep(100);
	}
	
	ipc_audio_exit();	
	close(ai_fd);

	return 0;
}

