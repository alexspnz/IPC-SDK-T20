#ifndef __AUDIO_CODEC_H__
#define __AUDIO_CODEC_H__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define AAC_LOG(fmt,args...) do{\
			printf("[%s:%s:%d ]-->",__FILE__,__FUNCTION__,__LINE__);\
			printf(fmt, ##args);\
		}while(0)


int aac_encoder_init(int samplerate, int samplebit,int channel ,void *callback_fn);
int aac_encode(unsigned char * InAudioData, int InLen );
int aac_encoder_uninit();
int aac_encoder_query_state();


int aac_decoder_init(int channel ,int samplebit);
int aac_decoder_start();
int aac_decode(unsigned char *src,  int  src_len , uint8_t **dst );
int aac_decoder_stop();
int aac_decoder_uninit();

typedef void (*AAC_ENC_cb)(unsigned char *data , int size);


#endif	/*__AUDIO_CODEC_H__*/
