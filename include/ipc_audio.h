
#ifndef __IPC_AUDIO_H__
#define __IPC_AUDIO_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

typedef enum ipc_aw8733_status_e
{
    AW8733A_STATUS_CLOSE     = 0,          // close
    AW8733A_STATUS_12DB      = 1,          // 12dB 
    AW8733A_STATUS_16DB      = 2,          // 16dB 
    AW8733A_STATUS_24DB      = 3,          // 24dB 
    AW8733A_STATUS_27DB      = 4,          // 27.5dB 
}IPC_AW8733A_STATUS_E;

typedef enum ipc_audio_devid_e
{
    IPC_AUDIO_AO	= 0,
    IPC_AUDIO_AI	= 1,
    IPC_AUDIO_SUM	= 2,
    IPC_AUDIO_BUTT,
}IPC_AUDIO_DEVID_E;

typedef struct ipc_audio_attr_s
{
    int enable;                          
    int soundMode;               
    int bitWidth;               
    int volume;                 
    int sampleRate;             
    int ptNumPerFrm;            
}IPC_AUDIO_ATTR_S;

/*****************************************************************************
 函 数 名: ipc_audio_frame_cb
 功能描述  : 视频帧回调函数
 输入参数  : 无
 输出参数  :   frame 音频帧
           len 帧大小
           pts 时间戳(us) 绝对时间
 返 回 值: 成功返回0,失败返回小于0
*****************************************************************************/
typedef int (*ipc_audio_frame_cb)(char *frame, unsigned long len, unsigned long pts);

typedef struct ipc_audio_s
{
	IPC_AUDIO_ATTR_S audio_device[IPC_AUDIO_SUM];	                                                
    ipc_audio_frame_cb audio_cb;
}IPC_AUDIO_S;

extern int ipc_audio_init(IPC_AUDIO_S *audio);
extern int ipc_audio_exit(void);
int ipc_audio_play(char *frame, int frame_len);
int ipc_audio_play_file(char *filepath);
int ipc_set_spkvol(int value);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

