#ifndef __RTSP_SERVER_H__
#define __RTSP_SERVER_H__


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"

typedef struct client_s // �ͻ�������Ľڵ�����
{
    int fd;
    int running;
    pthread_t pid;
}client_s;

/*****************************************************************************
 �� �� ��: rtsps_init
 ��������  : ��ʼ��RTSP������ģ�飬ʹ��VLC����ʱ��
            ���ڴ�RTSP֧��TCP�Ĵ��䷽ʽ����Ҫ����VLC��RTP OVER TCPѡ��
            ������������URL���ӣ�
            rtsp://192.168.0.110:554/MainStream
            rtsp://192.168.0.110:554/SubStream
 �������  : port Ĭ������554
 �������  : ��
 �� �� ֵ: �ɹ����ؾ��,ʧ�ܷ���NULL
*****************************************************************************/
handle rtsps_init(int port);

/*****************************************************************************
 �� �� ��: rtsps_destroy
 ��������  : ȥ��ʼ��RTSP������ģ�飬�ͷ�������Դ
 �������  : hndRtsps RTSP���������
 �������  : ��
 �� �� ֵ: �ɹ�����0,ʧ�ܷ���С��0
*****************************************************************************/
int rtsps_destroy(handle hndRtsps);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif


#endif



