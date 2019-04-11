
#ifndef __IPC_VIDEO_H__
#define __IPC_VIDEO_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#define SENSOR_FPS_DAY 		15
#define SENSOR_FPS_NIGHT 	10


typedef enum {
	IPC_RC_MODE_FIXQP   = 0,
    IPC_RC_MODE_CBR 	= 1,
    IPC_RC_MODE_VBR		= 2,
    IPC_RC_MODE_SMART	= 3,
    IPC_RC_MODE_BUTT,
}IPC_RC_MODE_E;

typedef enum {
	MAIN_CHANNEL	= 0,
    SECOND_CHANNEL 	= 1,
    THIRD_CHANNEL	= 2,
    JPG_CHANNEL 	= 3,
    
}IPC_VIDEO_CHANNEL_E;

typedef struct {
    int enable;
    int height;
    int width;
    int fps;
	int gop;
    int bitrate;		//kbps
    int channel_id;
	IPC_RC_MODE_E rc_mode;
}IPC_STREAM_S;

#define FS_CHN_NUM		3

/*****************************************************************************
 函 数 名: ipc_video_frame_cb
 功能描述  : 视频帧回调函数
 输入参数  : 无
 输出参数  :   stream 流id
           frame 视频帧
           len 帧大小
           key 关键帧标志
           pts 时间戳(us) 绝对时间
 返 回 值: 成功返回0,失败返回小于0
*****************************************************************************/
typedef int (*ipc_video_frame_cb)(int stream, char *frame, unsigned long len, int key, unsigned long pts);

typedef struct {
    IPC_STREAM_S video_stream[FS_CHN_NUM];
    ipc_video_frame_cb video_cb[FS_CHN_NUM];
}IPC_VIDEO_S;

#define bg0806

#if defined ov9732
#define SENSOR_NAME				"ov9732"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x36
#define NRVBS					3

#elif defined imx323
#define SENSOR_NAME				"imx323"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x1a
#define NRVBS					2

#elif defined gc2023
#define SENSOR_NAME				"gc2023"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x37
#define NRVBS					2

#elif defined bg0806
#define SENSOR_NAME				"bg0806"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x32
#define NRVBS					2

#elif defined sc1135
#define SENSOR_NAME				"sc1135"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x30
#define NRVBS					3

#elif defined jxf22
#define SENSOR_NAME				"jxf22"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x40
#define NRVBS					3

#elif defined jxh42
#define SENSOR_NAME				"jxh42"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x300
#define NRVBS					3
#endif

#define SENSOR_FRAME_RATE_DEN   1
#define MIN_QP 					20  //25
#define MAX_QP 					42  //51

int ipc_video_init(IPC_VIDEO_S *video);
int ipc_video_exit(void);
int ipc_jpeg_init(unsigned int width, unsigned int height);
int ipc_jpeg_exit(void);
int ipc_get_jpeg(char *buf, int *bufLen);
int ipc_set_bitrate(int channel, int bitrate);
int ipc_set_Iframe(int channel);
int ipc_set_inversion(int enable);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
