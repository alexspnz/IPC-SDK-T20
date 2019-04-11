#ifndef __IPC_BASE64_H__
#define __IPC_BASE64_H__


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"
#include <openssl/evp.h>


/*****************************************************************************
 函 数 名: ipc_base64_encode
 功能描述  : base64编码
 输入参数  : const char* data, int data_len,int *len
 输出参数  : 无 
 返 回 值: 编码后字符串
*****************************************************************************/
char *ipc_base64_encode(const char* data, int data_len,int *len);  

/*****************************************************************************
 函 数 名: ipc_base64_decode
 功能描述  : base64反编码
 输入参数  : const char* data, int data_len,int *len
 输出参数  : 无 
 返 回 值: 反编码后字符串
*****************************************************************************/
char *ipc_base64_decode(const char *data, int data_len,int *len);  

void ipc_aes_cbc_encrypt(unsigned char* in, int inl, unsigned char *out, int* len, char * key);

void ipc_aes_cbc_decrypt(unsigned char* in, int inl, unsigned char *out, unsigned char *key);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

