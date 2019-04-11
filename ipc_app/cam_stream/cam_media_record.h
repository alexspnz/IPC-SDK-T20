
#ifndef __CAM_MEDIA_RECORD_H__
#define __CAM_MEDIA_RECORD_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "cam_media_framepool.h"
#include "include/mp4v2/mp4v2.h"

#define MEDIA_RECORD_DBG		1
#define SD_RECORD_PATH			"/tmp/mmcblk0p1/"
#define FLASH_RECORD_PATH		"/flash_record/"

#define RECORD_INDEX_SD			"/tmp/rindex" 			//for record file in TF card
#define RECORD_INDEX_FLASH		"/tmp/rindex_flash" 	//for record file in flash
#define RECORD_INDEX_SD_TMP		"/tmp/rindex_tmp"
#define RECORD_INDEX_FLASH_TMP  "/tmp/rindex_flash_tmp"


typedef enum{
    RECORD_DPI_MIN				= -1,
    RECORD_DPI_360P,
    RECORD_DPI_480P,
    RECORD_DPI_720P,
    RECORD_DPI_1080P,
    RECORD_DPI_MAX,
}e_video_dpi;
	
typedef enum{
    RECORD_TYPE_MIN				= -1,
    RECORD_TYPE_MP4,
    RECORD_TYPE_PCM,
    RECORD_TYPE_AAC,
    RECORD_TYPE_ERR,
    RECORD_TYPE_MAX,
}e_record_type;

typedef enum{
    RECORD_CHANNEL_MIN  		= -1,
    RECORD_CHANNEL1,
    RECORD_CHANNEL2,
    RECORD_CHANNEL3,
    RECORD_CHANNEL_MAX,
}e_record_channel;

typedef enum
{
    RECORD_UNINIT_MIN 			= -1,
    RECORD_UNINIT_PRESERVE,
    RECORD_UNINIT_THROW_AWAY,
    RECORD_UNINIT_MAX,
}e_record_uninit_type;

typedef enum
{
	RECORD_END					=  0,	
    RECORD_WRITE_ERR  			= -1,
    RECORD_NO_DATA				= -2,
    RECORD_READONLY 			= -3,
}e_record_status;

typedef struct 
{
	unsigned int iy;
	char im;
	char id;
	char ih;
	char imi;
	char isec;
	char itype;
}RIndex;

typedef int (*record_status_cb)(int status);

typedef struct{
    uint32_t total_len;                             //second the total record timeout
    uint32_t file_len;                              //second the single file timeout
    e_video_dpi dpi;                                //dpi
    e_record_channel channel;						//record channel id
    record_status_cb status_cb;
}s_record_info;

extern s_record_info record_info;

int Media_Record_Init(s_record_info info, av_buffer *video_handle, av_buffer *audio_handle);
int Media_Record_UnInit(e_record_uninit_type type, e_record_channel channel_id);
int Media_Record_Stop(void);
int Create_Record_Index(char *basePath, FILE *fIndex, e_record_type itype);
int Get_Record_FilePath(int file_type, RIndex *file_time, char *file_name);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

