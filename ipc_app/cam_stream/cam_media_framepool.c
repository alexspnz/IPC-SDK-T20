
#include "cam_media_framepool.h"
#include "cam_malloc.h"

av_buffer *Cam_FramePool_Init(int buf_size, int min_node_num, int max_node_num)
{
	av_buffer *buffer = (av_buffer *)mem_malloc(sizeof(av_buffer));
	CHECK(buffer != NULL, NULL, "av_buffer malloc error \n");

	memset(buffer, 0, sizeof(av_buffer));	
	buffer->max_node_num = max_node_num;
	buffer->min_node_num = min_node_num;
	buffer->buffer_start = (char *)mem_malloc(buf_size);
	if(buffer->buffer_start == NULL){
		DBG("buffer_start malloc error \n");
		mem_free(buffer);
		buffer = NULL;
		return buffer;	
	}
	
	memset(buffer->buffer_start, 0, buf_size);
	buffer->buffer_sum_size = buf_size;
	buffer->read_pos = -1;
	buffer->running = 1;
	buffer->wait_Iframe = 1; 

	int index;
	for(index = 0; index < buffer->max_node_num; index++)
		buffer->node[index].read_mark = 1;

	return buffer;
}

void Cam_FramePool_UnInit(av_buffer *buffer)
{	
	av_buffer *buf = buffer;

	if(buf){
		if(buf->buffer_start)
			mem_free(buf->buffer_start);
		buf->buffer_start = NULL;
		
		mem_free(buf);
	}
	
	buf = NULL;
	return;
}

void Cam_FramePool_Start(av_buffer *buffer)
{
	av_buffer *buf = buffer;

	if(buf){
		buf->wait_Iframe = 1; 
		buf->running = 1;
	}
	
	return;
}

void Cam_FramePool_Stop(av_buffer *buffer)
{
	av_buffer *buf = buffer;

	if(buf){ 
		buf->running = 0;
	}

	return;
}

void Cam_FramePool_Clean(av_buffer *buffer)
{
	av_buffer *buf = buffer;

	if(buf){
		int index;
		buf->polling_once = 0;
		buf->read_pos = -1;
		for(index = 0; index < buf->max_node_num; index++ )
			buf->node[index].read_mark = 1;
	}
	
	return;
}

static int Cam_FramePool_Check(av_buffer *buffer)
{
	int ret = 0;
	av_buffer *buf = buffer;

	if(buf)
	{
		if(buf->node[buf->Pframe_pos].read_mark)
			ret = 1;
	}
	
	return ret;
}

int Cam_FramePool_Getlen(av_buffer *buffer)
{
	int ret = 0;
	av_buffer *buf = buffer;
	
	if(buf)
		ret = buf->write_pos;

	return ret;
}

int Cam_FramePool_Write(av_buffer *buffer, char *data, int size, int keyframe, unsigned long int timestamp, unsigned char frame_num)
{
	av_buffer *buf = buffer;

	if(buf->running)
	{
		if(buf){
			if(buf->polling_once == 0) {
				buf->write_pos = 0;
				buf->polling_once = 1;
			}

			if(buf->write_pos == buf->max_node_num)	{
				buf->write_pos = 0;
			}

			if(buf->wait_Iframe)
			{
				if(keyframe){
					if(Cam_FramePool_Check(buf)){				
						buf->write_pos = 0;
						buf->wait_Iframe = 0;
					}
				}
				else{
					return AV_BUFFER_FULL;
				}
			}
						
			if(buf->write_pos){
				if(buf->buffer_sum_size - buf->buffer_offset >= size)
				{
					memcpy(buf->buffer_start + buf->buffer_offset, data, size);
					buf->node[buf->write_pos].data = buf->buffer_start + buf->buffer_offset;
					buf->buffer_offset += size;
				}
				else{
					buf->write_pos ++; 
					if(buf->write_pos == buf->max_node_num) {
						buf->write_pos = 0;
					}

					buf->wait_Iframe = 1;
					return AV_BUFFER_FULL;
				}	
			}
			else{
				if(buf->buffer_sum_size >= size)
				{
					memcpy(buf->buffer_start, data, size);
					buf->node[buf->write_pos].data = buf->buffer_start;
					buf->buffer_offset = size;
				}else{
					buf->wait_Iframe = 1;
					return AV_BUFFER_FULL;
				}
			}

			buf->node[buf->write_pos].frame_num = frame_num;
			buf->node[buf->write_pos].size = size;
			buf->node[buf->write_pos].keyframe = keyframe;
			buf->node[buf->write_pos].timestamp = timestamp;
			buf->node[buf->write_pos].read_mark = 0;

			if(!keyframe)
				buf->Pframe_pos = buf->write_pos;
			
			buf->write_pos ++; 
			if(buf->write_pos == buf->max_node_num)	{
				buf->write_pos = 0;
			}
			
			return AV_BUFFER_NULL;

		}else
			return AV_BUFFER_ERR;
	}else
		return AV_BUFFER_ERR;
}

int Cam_FramePool_Read(av_buffer *buffer, char **data, int *size, int *keyframe, unsigned long int *timestamp, unsigned char *frame_num)
{
	av_buffer *buf = buffer;

	if(buf){
		if(buf->polling_once == 0)
			buf ->read_pos = 0;
		else{
			if(buf->read_pos == -1){
				if((buf->write_pos - buf->min_node_num) >= 0){
					buf ->read_pos = buf->write_pos - buf->min_node_num;
					
				}else{
					buf->read_pos = buf->max_node_num - (buf->min_node_num - buf->write_pos);
				}
			}
		}
		while(1){

			if(buf->node[buf->read_pos].read_mark == 0){
				buf->node[buf->read_pos].read_mark = 1;

				*frame_num = buf->node[buf->write_pos].frame_num;
				*data = buf->node[buf->read_pos].data;
				*size = buf->node[buf->read_pos].size;
				*keyframe = buf->node[buf->read_pos].keyframe;
				*timestamp = buf->node[buf->read_pos].timestamp;

				buf->read_pos++;
				if(buf->read_pos == buf->max_node_num){
					buf->read_pos = 0;
				}
				return AV_BUFFER_NULL;
				
			}else{
				if(buf->read_pos == buf->write_pos){
					*frame_num = 0;
					*size = 0;
					*data = NULL;
					*keyframe = 0;
					*timestamp = 0;
					return AV_BUFFER_FULL;
				}
				buf->read_pos++;
				if(buf->read_pos == buf->max_node_num){
					buf->read_pos = 0;
				}
			}
			
		}
		return AV_BUFFER_NULL;
	}else
		return AV_BUFFER_ERR;
}


