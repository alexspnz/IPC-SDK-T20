
#include "ipc_comm.h"
#include "ipc_debug.h" 
#include "ipc_audio.h"
#include "linklist.h"
#include "VoiceDecoder.h"

#define SECTION_LEN 256

LNode gLinkListHead;
int start_flag = 0;
static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;
VoiceDecoderCallback decoderCallback;
VoiceDecoder* decoder = NULL;
int detect_finsh = 0;
static uint32_t checksum = 0xff , totalsum = 0;
int section_id = 0;

int num = 0 , offset = 0;
uint8_t tmp_str[SECTION_LEN];
uint8_t dectect_str[SECTION_LEN];
#define MAX_VID_BUF 256

int valid_data_check(uint8_t *buf, int size);

ESVoid onDecoderStart(ESVoid* cbParam)
{
    // when decoder find start of token, this function will be called by decoder.
	memset(tmp_str,0,sizeof(tmp_str));
	start_flag = 1;
	num = 0;
    printf("%s\n", "Decoder start");
}

ESVoid onDecoderToken(ESVoid* cbParam, ESInt32 index)
{
    // when decoder find token which freqIndex is index, this function will be called by decoder.
    if(access("/tmp/aud_bug",F_OK) == 0)
	    printf("%02x \n", (uint8_t)index);
	tmp_str[num++]=(uint8_t)index;
}

int mydecoder(uint8_t *buf, int size)
{
	//printf("size= %d , buf = --%s--\n",size,buf);
	int ssid_len = buf[0];
	if(ssid_len >= 64)
		return -1;
	char ssid[64];
	memset(ssid,0,sizeof(ssid));
	memcpy(ssid,buf+1,ssid_len);
	if(access("/tmp/aud_bug",F_OK) == 0)
		printf("ssid_len=--%d--,ssid=--%s--\n",ssid_len,ssid);

	int psk_len = buf[ssid_len+1];
	if(psk_len >= 64)
		return -1;
	char psk[64];
	memset(psk,0,sizeof(psk));
	memcpy(psk,buf+ssid_len+2,psk_len);
	int i = 0;
	printf("\n");
	for(i = 0; i< psk_len ;i++)
	{
		psk[i] -= 127;
		if(access("/tmp/aud_bug",F_OK) == 0)
			printf("%02x ",psk[i]);
	}
	printf("\n");
	if(access("/tmp/aud_bug",F_OK) == 0)
			printf("psk_len=--%d--,psk=--%s--\n",psk_len,psk);
	return 0;
}

ESVoid onDecoderEnd(ESVoid* cbParam, ESInt32 result)
{
    // when decoder recognise finish, this function will be called by decoder.
    if (result < 0){
		printf("################Decoder end, dtc failed!#########################\n", "");
    }
    else{
		uint8_t sync_flag[]={0x00,0x00};
		uint8_t *str = tmp_str ;
		int i = 0;
		if( memcmp(tmp_str,sync_flag,2) == 0 )
		{
			printf("Find start !\n");
			offset = 0;
			memset(dectect_str,0,sizeof(dectect_str));
			totalsum = 0;
			checksum = tmp_str[2] | ((tmp_str[3] << 8 ) & 0xff00);
			str += 4;
			i += 4;
		}
		int sum = 0;

		memcpy(dectect_str + offset,str,num-i);
		offset += num-i;

		int j;
		printf("dectect_str:");
		for(j=0;j<offset;j++)
		{
			printf(" %02x ",dectect_str[j]);
		}
		if(access("/tmp/aud_bug",F_OK) == 0)
		{
			printf("\n");
			printf("Decoder result:---%s---\n",str);
			printf("start add : ");
		}
		for(;i < num ; i++)
		{
			if(access("/tmp/aud_bug",F_OK) == 0)
				printf("%02x ",tmp_str[i]);
			totalsum += tmp_str[i];
		}

		if(access("/tmp/aud_bug",F_OK) == 0)
		{
			printf("\n");
			printf("totalsum = %02x , checksum = %02x\n",totalsum&0xffff,checksum);
		}
		if((totalsum & 0xffff) == checksum && checksum != 0)
		{
			if(valid_data_check(dectect_str,offset) != 0)
			{
				memset(dectect_str,0,sizeof(dectect_str));
				start_flag = 1;
				num = 0;offset = 0;
				return ;
			}
			printf("checksum is done offset = %d\n",offset);
			checksum = 0xffff;
			if( mydecoder(dectect_str,offset) == 0)
			{
				FILE *fp = fopen("/tmp/sound.txt","wb");
				if(fp){
					int ret=fwrite(dectect_str,1,offset,fp);
					fclose(fp);
					printf("write ret = %d\n",ret);
				}
				exit(0);
			}
		}
    }
}

int valid_data_check(uint8_t *buf, int size)
{
	int ssid_len = buf[0];
	if(ssid_len >= 64)
		return -1;

	int psk_len = buf[ssid_len+1];
	if(psk_len >= 64)
		return -1;

	if((ssid_len + psk_len + 2) != size)
		return -1;

	return 0;
}

void audio_pcm_cb(char *pcm_buf, unsigned long pcm_len, unsigned long pts)
{
	int link_length = GetLength(&gLinkListHead);
	if( link_length > MAX_VID_BUF){
		/*drop the frame */
		fprintf(stderr, "Now auduo length is %d, we will drop this frame\n", link_length);
		return ;
	}

	pthread_mutex_lock(&list_lock);
	AVPacket *pkt = (AVPacket *)malloc(sizeof(AVPacket));
	pkt->data = (uint8_t *)malloc(pcm_len);
	pkt->size = pcm_len;
	memcpy(pkt->data, pcm_buf, pcm_len);

	InsertNode(&gLinkListHead,1,pkt);
	pthread_mutex_unlock(&list_lock);

    return;
}


int main()
{
    int ret;
    uint8_t ch;
    uint8_t tmp[64] = {0};
	IPC_AUDIO_S audio_attr;
	
	ret = InitList(&gLinkListHead);
    if(!ret){
        printf("InitList ERROR !\n");
		return 0;
    }

	memset(&audio_attr, 0, sizeof(audio_attr));
	audio_attr.audio_device[IPC_AUDIO_AO].enable = 1;
	audio_attr.audio_device[IPC_AUDIO_AO].sampleRate = 44100;
	audio_attr.audio_device[IPC_AUDIO_AO].bitWidth = 16;
	audio_attr.audio_device[IPC_AUDIO_AO].soundMode = 1;
	audio_attr.audio_device[IPC_AUDIO_AO].ptNumPerFrm = 1764;
	audio_attr.audio_device[IPC_AUDIO_AO].volume = 80;

	audio_attr.audio_device[IPC_AUDIO_AI].enable = 1;
	audio_attr.audio_device[IPC_AUDIO_AI].sampleRate = 44100;
	audio_attr.audio_device[IPC_AUDIO_AI].bitWidth = 16;
	audio_attr.audio_device[IPC_AUDIO_AI].soundMode = 1;
	audio_attr.audio_device[IPC_AUDIO_AI].ptNumPerFrm = 1764;
	audio_attr.audio_device[IPC_AUDIO_AI].volume = 80;
	audio_attr.audio_cb = audio_pcm_cb;	
	
	ret = ipc_audio_init(&audio_attr);

	LNode *list;
	list = &gLinkListHead;

    VoiceDecoderCallback decoderCallback = { onDecoderStart, onDecoderToken, onDecoderEnd };
    VoiceDecoder* decoder = VoiceDecoder_create("com.sinvoice.for_yinshixun", "SinVoiceDemo", &decoderCallback, ES_NULL);
	if (ES_NULL == decoder){
		printf("VoiceDecoder_create error !\n");
		return -1;
	}
	
	if (!VoiceDecoder_start(decoder)){
		printf("VoiceDecoder_start error !\n");
		return -1;
	}
	
	ESInt16* data = ES_NULL;
	ESUint32 dataLen = 0;

	while(!detect_finsh){
		int link_length = GetLength(list);
		if(link_length){
			pthread_mutex_lock(&list_lock);
			AVPacket *pkt = GetNodePkt(list, link_length);

			if(!VoiceDecoder_putData(decoder, (short*)pkt->data,  pkt->size / 2)){
				printf("VoiceDecoder_putData error !\n");
				return -1;
			}

			free(pkt->data);
			free(pkt);
			DeleteNode(list, link_length);
			pthread_mutex_unlock(&list_lock);
		}
		else
			usleep(50*1000);

	}

	VoiceDecoder_stop(decoder);
	VoiceDecoder_destroy(decoder);

	ipc_audio_exit();
	
    return 0;
}
