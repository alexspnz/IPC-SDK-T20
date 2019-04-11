
#ifndef __IPC_MTD_H__
#define __IPC_MTD_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"

int ipc_mtd_erase(char *devicename);
int ipc_mtd_rerase(char *devicename);
int ipc_mtd_write(char *filebuf, int filesize, char *devicename);
int ipc_mtd_verify(char *filebuf, int filesize, char *devicename);

int ipc_mtd_readflag(char *devicename, char* output, int len);
int ipc_mtd_writeflag(char *devicename, char* input, int len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

