
#ifndef __CAM_CONF_H__
#define __CAM_CONF_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"
#include "ipc_video.h"
#include "cam_env.h"
#include "cam_func.h"

int Get_Cam_Attr(int iType);

void Set_Cam_Attr(int iType, int iValue);

void Cam_Attr_Init();


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

