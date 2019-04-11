
#include "ipc_frame_pool.h"
#include "ipc_debug.h"
#include "ipc_list.h"
#include "ipc_malloc.h"

typedef struct frame_pool_s
{
    handle hndlist;
    int capacity;
    unsigned int sequence;
    pthread_mutex_t mutex;
}frame_pool_s;

typedef struct reader_s
{
    frame_pool_s* pstFramePool;
    int head; //1���Ӷ�ͷ��ʼȡ 0���Ӷ�β��ʼȡ
    char sps[64];
    int sps_size;
    char pps[64];
    int pps_size;
    frame_info_s* LastFrame;
}reader_s;

handle gHndMainFramePool = NULL;
handle gHndSubFramePool = NULL;
handle gHndAudioFramePool = NULL;


handle frame_pool_init(int capacity)
{
    frame_pool_s* pstFramePool = (frame_pool_s*)mem_malloc(sizeof(frame_pool_s));
    CHECK(pstFramePool, NULL, "failed to malloc %d.\n", sizeof(frame_pool_s));
    memset(pstFramePool, 0, sizeof(frame_pool_s));
    pthread_mutex_init(&pstFramePool->mutex, NULL);

    pstFramePool->hndlist = list_init(sizeof(frame_info_s));
    CHECK(NULL != pstFramePool->hndlist, NULL, "failed to init list.\n");

    pstFramePool->capacity = capacity;

    DBG("init completed.capacity: %d\n", pstFramePool->capacity);

    return (handle)pstFramePool;
}

int frame_pool_destroy(handle hndFramePool)
{
    CHECK(NULL != hndFramePool, -1, "invalid parameter.\n");

    int ret = -1;
    frame_pool_s* pstFramePool = (frame_pool_s*)(hndFramePool);
    frame_info_s* frame_info = list_front(pstFramePool->hndlist);
    while (frame_info)
    {
        if (frame_info->data)
        {
            mem_free(frame_info->data);
            frame_info->data = NULL;
        }
        frame_info = list_next(pstFramePool->hndlist, frame_info);
    }
    ret = list_destroy(pstFramePool->hndlist);
    CHECK(ret == 0, -1, "error with %#x\n", ret);

    pthread_mutex_destroy(&pstFramePool->mutex);
    mem_free(pstFramePool);
    DBG("destroy  completed\n");

    return 0;
}

int frame_pool_add(handle hndFramePool, char *frame, unsigned long len, int key, double pts)
{
    CHECK(NULL != hndFramePool, -1, "invalid parameter.\n");

    int ret = -1;
    frame_pool_s* pstFramePool = (frame_pool_s*)(hndFramePool);

    ret = pthread_mutex_lock(&pstFramePool->mutex);
    CHECK(ret == 0 , -1, "error with %s\n", strerror(errno));

    if (list_size(pstFramePool->hndlist) >= pstFramePool->capacity)
    {
        frame_info_s* frame_info = list_front(pstFramePool->hndlist);
        if (frame_info->reference <= 0) 
        {
            mem_free(frame_info->data);
            ret = list_pop_front(pstFramePool->hndlist);
            CHECK(ret == 0, -1, "error with %#x\n", ret);
        }
    }

    frame_info_s frame_info_new;
    memset(&frame_info_new, 0, sizeof(frame_info_new));
    frame_info_new.data = mem_malloc(len);
    CHECK(frame_info_new.data, -1, "failed to malloc %d.\n", len);
    memcpy(frame_info_new.data, frame, len);
    frame_info_new.len = len;
    frame_info_new.timestamp = pts;
    frame_info_new.keyFrame = key;
    frame_info_new.sequence = pstFramePool->sequence++;

    ret = list_push_back(pstFramePool->hndlist, &frame_info_new, sizeof(frame_info_new));
    CHECK(ret == 0, -1, "error with %#x\n", ret);

    //��󻺴浽2��
    ASSERT(list_size(pstFramePool->hndlist) <= pstFramePool->capacity*2, "error with %#x\n", -1);
/*
    DBG("max capacity: %d, current capacity: %d, sequence: %d\n", pstFramePool->capacity,
            list_size(pstFramePool->hndlist), frame_info_new.sequence);
*/
    ret = pthread_mutex_unlock(&pstFramePool->mutex);
    CHECK(ret == 0 , -1, "error with %s\n", strerror(errno));

    return 0;
}

static frame_info_s* frame_pool_find(handle hndlist, int head, int stream_type)
{
    if (head == 1)
    {
        frame_info_s* pNode = (frame_info_s*)list_front(hndlist);
        while (NULL != pNode)
        {
        	if(stream_type == SAL_AUDIO_STREAM)
        	{
				return pNode;
        	}
			else
			{
				if (pNode->keyFrame)
            	{
                	return pNode;
            	}
            	pNode = (frame_info_s*)list_next(hndlist, pNode);
			}
        }
    }
    else
    {
        frame_info_s* pNode = (frame_info_s*)list_back(hndlist);
        while (NULL != pNode)
        {
        	if(stream_type == SAL_AUDIO_STREAM)
        	{
				return pNode;
			}
			else
			{
            	if (pNode->keyFrame)
            	{
                	return pNode;
            	}
            	pNode = (frame_info_s*)list_prev(hndlist, pNode);
			}
        }
    }
    return NULL;
}

int frame_pool_split_vframe(unsigned char* _pu8Frame, int _u32FrameSize)
{
    int i = 0;
    for(i = 4; i < _u32FrameSize - 4; i++)
    {
        if(_pu8Frame[i+0] == 0x00 && _pu8Frame[i+1] == 0x00 && _pu8Frame[i+2] == 0x00 && _pu8Frame[i+3] == 0x01)
        {
            return i;
        }
    }
    return -1;
}

handle frame_pool_register(handle hndFramePool, int head, int stream_type)
{
    CHECK(NULL != hndFramePool, NULL, "invalid parameter.\n");

    int ret = -1;
    frame_pool_s* pstFramePool = (frame_pool_s*)(hndFramePool);

    ret = pthread_mutex_lock(&pstFramePool->mutex);
    CHECK(ret == 0 , NULL, "error with %s\n", strerror(errno));

    frame_info_s* pstLastFrame = frame_pool_find(pstFramePool->hndlist, head, stream_type);
    CHECK(pstLastFrame || pthread_mutex_unlock(&pstFramePool->mutex), NULL, "error with %#x\n", ret);

    reader_s* pstReader = (reader_s*)mem_malloc(sizeof(reader_s));
    CHECK(pstReader, NULL, "failed to malloc %d.\n", sizeof(reader_s));
    memset(pstReader, 0, sizeof(reader_s));

    pstReader->pstFramePool = pstFramePool;
    pstReader->head = head;
    pstReader->LastFrame = pstLastFrame;

	if(stream_type == SAL_VIDEO_STREAM)
	{
		ret = frame_pool_split_vframe(pstLastFrame->data, pstLastFrame->len);
    	if(ret <= 0)
    	{
        	DBG("error with %#x\n", ret);
       		mem_free(pstReader);
        	return NULL;
    	}
		
    	memcpy(pstReader->sps, pstLastFrame->data, ret);
    	pstReader->sps_size = ret;

    	ret = frame_pool_split_vframe(pstLastFrame->data+pstReader->sps_size, pstLastFrame->len-pstReader->sps_size);
    	CHECK(ret > 0 , NULL, "error with %#x\n", ret);
    	memcpy(pstReader->pps, pstLastFrame->data+pstReader->sps_size, ret);
    	pstReader->pps_size = ret;

    	// �ѻ�ȡ����sps pps ȥ����ʼ�� 00 00 00 01
    	pstReader->sps_size -= 4;
    	pstReader->pps_size -= 4;
    	memmove(pstReader->sps, pstReader->sps + 4, pstReader->sps_size);
    	memmove(pstReader->pps, pstReader->pps + 4, pstReader->pps_size);

    	DBG("video register completed.\n");
	}
	else
	{
		DBG("audio register completed.\n");
	}
	
    ret = pthread_mutex_unlock(&pstFramePool->mutex);
    CHECK(ret == 0 , NULL, "error with %s\n", strerror(errno));

    return (handle)pstReader;
}

int frame_pool_unregister(handle hndReader)
{
    CHECK(NULL != hndReader, -1, "invalid parameter.\n");

    reader_s* pstReader = (reader_s*)(hndReader);

    mem_free(pstReader);
    DBG("unregister completed.\n");

    return 0;
}

/*
*���ܴ���frame_pool_register�ɹ�����˺ܾò�frame_pool_get������LastFrame�Ѿ���ɾ����
*�ķ��գ�list_next�᷵��NULL������frame_pool_getһֱ�᷵��NULL
*/
frame_info_s* frame_pool_get(handle hndReader)
{
    CHECK(NULL != hndReader, NULL, "invalid parameter.\n");

    reader_s* pstReader = (reader_s*)(hndReader);

    pthread_mutex_lock(&pstReader->pstFramePool->mutex);
    frame_info_s* ret_fm = NULL;
    if (pstReader->LastFrame)
    {
        frame_info_s* last_frame = list_next(pstReader->pstFramePool->hndlist, pstReader->LastFrame);
        if (last_frame)
        {
            pstReader->LastFrame->reference++;
            ret_fm = pstReader->LastFrame;
            pstReader->LastFrame = last_frame;
        }
    }

    pthread_mutex_unlock(&pstReader->pstFramePool->mutex);

    return ret_fm;
}

int frame_pool_release(handle hndReader, frame_info_s* pstFrameInfo)
{
    CHECK(NULL != hndReader, -1, "invalid parameter.\n");
    CHECK(NULL != pstFrameInfo, -1, "invalid parameter.\n");

    reader_s* pstReader = (reader_s*)(hndReader);

    pthread_mutex_lock(&pstReader->pstFramePool->mutex);

    pstFrameInfo->reference--;
    if (pstFrameInfo->reference < 0)
    {
        pstFrameInfo->reference = 0;
    }

    pthread_mutex_unlock(&pstReader->pstFramePool->mutex);

    return 0;
}

int frame_pool_sps_get(handle hndReader, char** out, int* len)
{
    CHECK(NULL != hndReader, -1, "invalid parameter.\n");

    reader_s* pstReader = (reader_s*)(hndReader);

    pthread_mutex_lock(&pstReader->pstFramePool->mutex);

    if (pstReader->sps_size > 0)
    {
        *out = pstReader->sps;
        *len = pstReader->sps_size;
    }

    pthread_mutex_unlock(&pstReader->pstFramePool->mutex);

    return 0;
}

int frame_pool_pps_get(handle hndReader, char** out, int* len)
{
    CHECK(NULL != hndReader, -1, "invalid parameter.\n");

    reader_s* pstReader = (reader_s*)(hndReader);

    pthread_mutex_lock(&pstReader->pstFramePool->mutex);

    if (pstReader->pps_size > 0)
    {
        *out = pstReader->pps;
        *len = pstReader->pps_size;
    }

    pthread_mutex_unlock(&pstReader->pstFramePool->mutex);

    return 0;
}

