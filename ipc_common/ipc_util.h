#ifndef __IPC_UTIL_H__
#define __IPC_UTIL_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"

int util_time_abs(struct timeval* pTime);
int util_time_local(struct timeval* pTime);
int util_time_sub(struct timeval* pStart, struct timeval* pEnd);
int util_time_pass(struct timeval* previous);
int util_file_size(char* path);
int util_file_read(char* path, char* buf, int len);
int util_system(const char *format, ...);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
