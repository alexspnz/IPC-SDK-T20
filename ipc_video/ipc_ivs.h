
#ifndef __IPC_IVS_H__
#define __IPC_IVS_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

typedef enum
{
    IPC_MD_NONE = 0,
    IPC_MD_FACE = 1,                   // 人脸
    IPC_MD_WINDOW = 2,                 // 安防
    IPC_MD_RECORD = 4,                 // 全屏动作检测，用于云录开始/停止
    IPC_MD_FULLSCREEN = 8,             // 全屏动作检测，用于服务端进行未见人形检测
    // QCAM_MD_XXX = 8
} IPC_MOTION_DETECT_TYPE;

// 移动侦测回调函数
typedef void (*ipc_motion_detection_cb)(int result);


extern int ipc_ivs_init(void);
extern int ipc_ivs_exit(void);

extern int ipc_motion_detection_start(ipc_motion_detection_cb cb);
extern int ipc_motion_detection_stop(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

