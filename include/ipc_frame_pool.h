
#ifndef __FRAME_POOL_H__
#define __FRAME_POOL_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"

typedef enum
{
    SAL_VIDEO_STREAM	= 0,         
    SAL_AUDIO_STREAM   	= 1,                
}SAL_STREAM_TYPE;                                                                                                                                         

typedef struct frame_info_s
{
    void* data;
    int len;
    double timestamp;
    int keyFrame;
    unsigned int sequence;

    int reference;
}frame_info_s;

extern handle gHndMainFramePool;  //���������
extern handle gHndSubFramePool;   //���������
extern handle gHndAudioFramePool;

/*****************************************************************************
 �� �� ��: frame_pool_init
 ��������  : ��ʼ������Ƶ֡��ģ��
 �������  : capacity ��󻺴��֡��
 �������  : ��
 �� �� ֵ: �ɹ����ؾ��,ʧ�ܷ���NULL
*****************************************************************************/
handle frame_pool_init(int capacity);

/*****************************************************************************
 �� �� ��: frame_pool_destroy
 ��������  : ȥ��ʼ������Ƶ֡��ģ��
 �������  : hndFramePool ����Ƶ֡�ؾ��
 �������  : ��
 �� �� ֵ: �ɹ�����0,ʧ�ܷ���С��0
*****************************************************************************/
int frame_pool_destroy(handle hndFramePool);

/*****************************************************************************
 �� �� ��: frame_pool_add
 ��������  : ������Ƶ֡��������֡
 �������  : hndFramePool ����Ƶ֡�ؾ��
            frame ����Ƶ֡
            len ����Ƶ֡��С
            key �Ƿ�ؼ�֡
            pts ����Ƶ֡ʱ���
 �������  : ��
 �� �� ֵ: �ɹ�����0,ʧ�ܷ���С��0
*****************************************************************************/
int frame_pool_add(handle hndFramePool, char *frame, unsigned long len, int key, double pts);

/*****************************************************************************
 �� �� ��: frame_pool_register
 ��������  : ע������Ƶ֡�ص�Reader����ȳ���������һ��I֡�Ż᷵�سɹ�
 �������  : hndFramePool ����Ƶ֡�ؾ��
            head 1����֡��ͷ����ʼ��ȡ 0����֡��β����ʼ��ȡ
 �������  : ��
 �� �� ֵ: �ɹ����ؾ��,ʧ�ܷ���NULL
*****************************************************************************/
handle frame_pool_register(handle hndFramePool, int head, int stream_type);

/*****************************************************************************
 �� �� ��: frame_pool_unregister
 ��������  : ע������Ƶ֡�ص�Reader
 �������  : hndReader ���
 �������  : ��
 �� �� ֵ: �ɹ�����0,ʧ�ܷ���С��0
*****************************************************************************/
int frame_pool_unregister(handle hndReader);

/*****************************************************************************
 �� �� ��: frame_pool_get
 ��������  : ��֡�ػ�ȡ����Ƶ֡
 �������  : hndReader ע���õľ��
 �������  : ��
 �� �� ֵ: �ɹ�����frame_info_s�ṹ,ʧ�ܷ���NULL
*****************************************************************************/
frame_info_s* frame_pool_get(handle hndReader);

/*****************************************************************************
 �� �� ��: frame_pool_release
 ��������  : �ͷŴ�֡�ػ�ȡ������Ƶ֡
 �������  : hndReader ע���õľ��
            pstFrameInfo ��֡�ػ�ȡ������Ƶ֡
 �������  : ��
 �� �� ֵ: �ɹ�����0,ʧ�ܷ���С��0
*****************************************************************************/
int frame_pool_release(handle hndReader, frame_info_s* pstFrameInfo);

/*****************************************************************************
 �� �� ��: frame_pool_sps_get
 ��������  : ��֡�ػ�ȡh264��Ƶ֡��SPS
 �������  : hndReader ע���õľ��
 �������  : out ���SPS
            len ���SPS�ĳ���
 �� �� ֵ: �ɹ�����0,ʧ�ܷ���С��0
*****************************************************************************/
int frame_pool_sps_get(handle hndReader, char** out, int* len);

/*****************************************************************************
 �� �� ��: frame_pool_pps_get
 ��������  : ��֡�ػ�ȡh264��Ƶ֡��PPS
 �������  : hndReader ע���õľ��
 �������  : out ���PPS
            len ���PPS�ĳ���
 �� �� ֵ: �ɹ�����0,ʧ�ܷ���С��0
*****************************************************************************/
int frame_pool_pps_get(handle hndReader, char** out, int* len);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

