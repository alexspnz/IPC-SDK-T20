#ifndef __FW_MD5_H__
#define __FW_MD5_H__


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"

int md5_filesum(char* _szFilePath, unsigned char sum[64]);
int md5_memsum(char* input, int len, unsigned char sum[64]);



#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
