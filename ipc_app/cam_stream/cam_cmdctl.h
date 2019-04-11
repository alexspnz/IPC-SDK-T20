
#ifndef __CAM_CMDCTL_H__
#define __CAM_CMDCTL_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"
#include "cam_env.h"
#include "cam_func.h"
#include "cam_media_record.h"


void Handle_IOCTRL_Cmd(int SID, int avIndex, char *buf, int type, int iMediaNo);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

