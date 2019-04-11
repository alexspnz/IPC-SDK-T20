#ifndef __IPC_COMM_H__
#define __IPC_COMM_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <netinet/in.h>
#include <linux/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <assert.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <malloc.h>
#include <sys/mman.h>
#include <mtd/mtd-user.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/statfs.h>


typedef enum {
    SENSOR_OV9732 = 0,
	SENSOR_IMX323,
	SENSOR_GC2023,
	SENSOR_BG0806,
	SENSOR_SC1135,
	SENSOR_JXH42,
	SENSOR_BUTT,
}
IPC_SENSOR_E;

typedef void* handle;
typedef unsigned char           uint8_t;
typedef unsigned short int      uint16_t;
typedef unsigned int            uint32_t;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
