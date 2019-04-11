
#ifndef __CAM_FUNC_H__
#define __CAM_FUNC_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"
#include "cam_conf.h"

void Cam_Func_Cmd(int SID, int avIndex, char *buf, int type, int iMediaNo);
void Cam_Media_Cmd(int SID, int avIndex, char *buf, int type, int iMediaNo);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

