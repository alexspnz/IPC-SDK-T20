
#include "cam_media.h"
#include "ipc_debug.h"
#include "cam_media_record.h"
#include "audio_codec.h"
#include "cam_env.h" 

IPC_VIDEO_S g_video_attr;
IPC_AUDIO_S g_audio_attr;
s_record_info g_record_attr;

struct adpcm_state enc_state, dec_state;
static int pcm_buf_size, aac_buf_size = 0;
static uint8_t aac_buf[768] = {0}, pcm_buf[2048] = {0};

av_buffer *video_record_buffer;
av_buffer *audio_record_buffer;
av_buffer *video_live_buffer;
av_buffer *audio_live_buffer;
pthread_mutex_t video_record_lock, audio_record_lock, video_live_lock, audio_live_lock;
static int switch_vquality = 0;

#define MAIN_LEVEL 					3
#define SECOND_LEVEL				3
static int main_bitrate[MAIN_LEVEL] = {1024, 768, 512};
static int second_bitrate[SECOND_LEVEL] = {512, 256, 128};

typedef struct _SpeakerStruct
{
	int sid;
	int iIndex;
}SpeakerStruct;

static int Set_Bitrate_Up(void)
{
	g_media_attr.bitrate_level = 0;

	ipc_set_bitrate(MAIN_CHANNEL, main_bitrate[g_media_attr.bitrate_level]);
	ipc_set_bitrate(SECOND_CHANNEL, second_bitrate[g_media_attr.bitrate_level]);
	
	DBG("Set Main-Bitrate %d\n", main_bitrate[g_media_attr.bitrate_level]);
	DBG("Set Second-Bitrate %d\n", second_bitrate[g_media_attr.bitrate_level]);
	return 0;
}

static int Set_Resolution_Down(int chn)
{
	int ret = 0;

	if (chn == SECOND_CHANNEL) {
		switch_vquality = 1;
		Set_Cam_Attr(CAM_VQUALITY, 4);	
		g_media_attr.bitrate_level = 0;
		pthread_mutex_lock(&video_live_lock);
		Cam_FramePool_Stop(video_live_buffer);
		Cam_FramePool_Clean(video_live_buffer);
		Cam_FramePool_Start(video_live_buffer);
		pthread_mutex_unlock(&video_live_lock);			
	}else
		ret = -1;

	return ret;
}

static int Get_Cur_Bitrate(int chn)
{
	if(chn == MAIN_CHANNEL)
		return main_bitrate[g_media_attr.bitrate_level];
	else if(chn == SECOND_CHANNEL)
		return second_bitrate[g_media_attr.bitrate_level];
}

static int Set_Bitrate_Down(int chn)
{
	int i_ret = 0;

	if(chn == MAIN_CHANNEL){
		g_media_attr.bitrate_level ++;
		if(g_media_attr.bitrate_level >= MAIN_LEVEL-1)
			g_media_attr.bitrate_level = MAIN_LEVEL-1;

		DBG("Set Main-Bitrate %d\n", main_bitrate[g_media_attr.bitrate_level]);
		i_ret = ipc_set_bitrate(MAIN_CHANNEL, main_bitrate[g_media_attr.bitrate_level]);
		return i_ret;	
	}
	else if(chn == SECOND_CHANNEL){
		g_media_attr.bitrate_level ++;
		if(g_media_attr.bitrate_level >= SECOND_LEVEL-1)
			g_media_attr.bitrate_level = SECOND_LEVEL-1;
		
		DBG("Set Second-Bitrate %d\n", second_bitrate[g_media_attr.bitrate_level]);
		i_ret = ipc_set_bitrate(SECOND_CHANNEL, second_bitrate[g_media_attr.bitrate_level]);
		return i_ret;	
	}	

	return i_ret;
}

static void cam_force_Iframe()
{
	if(Get_Cam_Attr(CAM_VQUALITY) < 3)
		ipc_set_Iframe(MAIN_CHANNEL);
	else 
		ipc_set_Iframe(SECOND_CHANNEL);
}

void audio_pcm2aac(void *buf, int buf_len, const unsigned long int timestamp)
{
	int len;
	int left;
	static unsigned int has_size = 0;
	
	if((pcm_buf_size - has_size ) > buf_len){
		memcpy(pcm_buf + has_size, buf, buf_len);
		has_size += buf_len;
	}
	else{
		memcpy(pcm_buf + has_size, buf, (pcm_buf_size - has_size ));
		left = buf_len - (pcm_buf_size-has_size);
		len = MY_Audio_PCM2AAC(pcm_buf, pcm_buf_size, aac_buf, aac_buf_size);
		
		pthread_mutex_lock(&audio_record_lock);
		Cam_FramePool_Write(audio_record_buffer, aac_buf, len, 1, timestamp, 0);
		pthread_mutex_unlock(&audio_record_lock);

		pthread_mutex_lock(&audio_live_lock);
		Cam_FramePool_Write(audio_live_buffer, aac_buf, len, 1, timestamp, 0);
		pthread_mutex_unlock(&audio_live_lock);
		
		has_size = 0;
		memcpy(pcm_buf + has_size, buf + buf_len - left, left);
		has_size += left;
	}
	return ;
}

static void video_record_cb(int stream_id, char *frame, unsigned long len, int keyframe, unsigned long int timestamp)
{
	if(stream_id == MAIN_CHANNEL){
		pthread_mutex_lock(&video_record_lock);
		Cam_FramePool_Write(video_record_buffer, frame, len, keyframe, timestamp, 0);
		pthread_mutex_unlock(&video_record_lock);
	}
}

static int video_stream0_cb(int stream_id, char *frame, unsigned long len, int keyframe, unsigned long int timestamp)
{
	static unsigned char frame_num = 0;
	CHECK((frame && (len >=0)), -1, "video_stream0_cb callback error \n");

	video_record_cb(stream_id, frame, len, keyframe, timestamp);

	if(Get_Cam_Attr(CAM_VQUALITY) < 3){	
		pthread_mutex_lock(&video_live_lock);
		Cam_FramePool_Write(video_live_buffer, frame, len, keyframe, timestamp, frame_num++);
		pthread_mutex_unlock(&video_live_lock);
	}
	
	return 0;
}

static int video_stream1_cb(int stream_id, char *frame, unsigned long len, int keyframe, unsigned long int timestamp)
{
	static unsigned char frame_num = 0;
	CHECK((frame && (len >=0)), -1, "video_stream1_cb callback error \n");

	video_record_cb(stream_id, frame, len, keyframe, timestamp);

	if(Get_Cam_Attr(CAM_VQUALITY) >= 3){		
		pthread_mutex_lock(&video_live_lock);
		Cam_FramePool_Write(video_live_buffer, frame, len, keyframe, timestamp, frame_num++);
		pthread_mutex_unlock(&video_live_lock);
	}

	return 0;
}

static int audio_pcm_cb(char *frame, unsigned long len, unsigned long int timestamp)
{
	CHECK((frame && (len >=0)), -1, "audio_pcm_cb callback error \n");

	audio_pcm2aac(frame, len, timestamp);

	return 0;
}

int Cam_Video_Init()
{
	int ret = 0;
	
    memset(&g_video_attr, 0, sizeof(g_video_attr));
	g_video_attr.video_cb[MAIN_CHANNEL] = video_stream0_cb;
	g_video_attr.video_stream[MAIN_CHANNEL].enable = 1;
	g_video_attr.video_stream[MAIN_CHANNEL].bitrate = 1024;
	g_video_attr.video_stream[MAIN_CHANNEL].fps = 15;
	g_video_attr.video_stream[MAIN_CHANNEL].gop = 2;
	g_video_attr.video_stream[MAIN_CHANNEL].width = 1920;
	g_video_attr.video_stream[MAIN_CHANNEL].height = 1080;
	g_video_attr.video_stream[MAIN_CHANNEL].rc_mode = IPC_RC_MODE_SMART;
	g_video_attr.video_stream[MAIN_CHANNEL].channel_id = MAIN_CHANNEL;

	g_video_attr.video_cb[SECOND_CHANNEL] = video_stream1_cb;
	g_video_attr.video_stream[SECOND_CHANNEL].enable = 1;
	g_video_attr.video_stream[SECOND_CHANNEL].bitrate = 512;
	g_video_attr.video_stream[SECOND_CHANNEL].fps = 15;
	g_video_attr.video_stream[SECOND_CHANNEL].gop = 2;
	g_video_attr.video_stream[SECOND_CHANNEL].width = 640;
	g_video_attr.video_stream[SECOND_CHANNEL].height = 360;
	g_video_attr.video_stream[SECOND_CHANNEL].rc_mode = IPC_RC_MODE_SMART;
	g_video_attr.video_stream[SECOND_CHANNEL].channel_id = SECOND_CHANNEL;
	ret = ipc_video_init(&g_video_attr);
	
	return ret;
}

int Cam_Audio_Init()
{
	int ret = 0;

	ret = aac_decoder_init();
	CHECK(ret >= 0, -1, "aac_decoder_init error \n");

	ret = audio_encoder_start(16000, &aac_buf_size, &pcm_buf_size);
	CHECK(ret >= 0, -1, "aac_encoder_start error \n");
	CHECK((aac_buf_size && pcm_buf_size), -1, "aac or pcm buf_size error \n");
	DBG("###aac_buf_size=%d, pcm_buf_size=%d###\n", aac_buf_size, pcm_buf_size);
	
	memset(aac_buf, 0, sizeof(aac_buf));
	memset(pcm_buf, 0, sizeof(pcm_buf));
	memset(&enc_state, 0, sizeof(struct adpcm_state));
	memset(&dec_state, 0, sizeof(struct adpcm_state));
	
	memset(&g_audio_attr, 0, sizeof(g_audio_attr));
	g_audio_attr.audio_device[IPC_AUDIO_AO].enable = 1;
	g_audio_attr.audio_device[IPC_AUDIO_AO].sampleRate = 16000;
	g_audio_attr.audio_device[IPC_AUDIO_AO].bitWidth = 16;
	g_audio_attr.audio_device[IPC_AUDIO_AO].soundMode = 1;
	g_audio_attr.audio_device[IPC_AUDIO_AO].ptNumPerFrm = 640;
	g_audio_attr.audio_device[IPC_AUDIO_AO].volume = 80;

	g_audio_attr.audio_device[IPC_AUDIO_AI].enable = 1;
	g_audio_attr.audio_device[IPC_AUDIO_AI].sampleRate = 16000;
	g_audio_attr.audio_device[IPC_AUDIO_AI].bitWidth = 16;
	g_audio_attr.audio_device[IPC_AUDIO_AI].soundMode = 1;
	g_audio_attr.audio_device[IPC_AUDIO_AI].ptNumPerFrm = 640;
	g_audio_attr.audio_device[IPC_AUDIO_AI].volume = 80;
	g_audio_attr.audio_cb = audio_pcm_cb;		
	ret = ipc_audio_init(&g_audio_attr);

	return ret;
}

static int cam_record_cb(int status)
{
	if(status == RECORD_END){
		DBG("###Media Record Over!###\n");
		Media_Record_UnInit(RECORD_UNINIT_MIN,RECORD_CHANNEL1);
	}
	
	return 0;
}

int Cam_Record_Init()
{
	g_record_attr.channel = RECORD_CHANNEL1;
	g_record_attr.dpi = RECORD_DPI_1080P;
	g_record_attr.file_len = 60;
	g_record_attr.total_len = 0;
	g_record_attr.status_cb = cam_record_cb;

	return Media_Record_Init(g_record_attr, video_record_buffer, audio_record_buffer);
}

static int Cam_Media_Err(int ret)
{
	if(ret == AV_ER_EXCEED_MAX_SIZE) // means data not write to queue, send too slow, I want to skip it
	{
		usleep(1000*200);
		return 2;
	}
	else if(ret == AV_ER_SESSION_CLOSE_BY_REMOTE)
	{
		DBG("MediaErrHandling AV_ER_SESSION_CLOSE_BY_REMOTE SID\n");
		return 1;
	}
	else if(ret == AV_ER_REMOTE_TIMEOUT_DISCONNECT)
	{
		DBG("MediaErrHandling AV_ER_REMOTE_TIMEOUT_DISCONNECT SID\n");
		return 1;
	}
	else if(ret == IOTC_ER_INVALID_SID)
	{
		DBG("Session cant be used anymore\n");
		return 1;
	}
	else if(ret < 0)
		DBG("####### %d ######\n",ret);

	return 0;
}

static int Adjust_BitRate(int index, unsigned int time, int err_code)
{
	static unsigned int iLastAdjustTime=0;
	static unsigned int iDownTime = 0;
	int i_ret = -1;
	float fusage = 0;

	if ((time-iLastAdjustTime) > BITRATE_SWITCH_DELAY){
		fusage = avResendBufUsageRate(index);
		DBG("var: 0x%x, 0x%x, 0x%x, %f\n", time, iLastAdjustTime, (time-iLastAdjustTime), fusage);
		iLastAdjustTime = time;
        if(err_code == 2) {
			if(Get_Cam_Attr(CAM_VQUALITY) < 3){
				if(Get_Cur_Bitrate(MAIN_CHANNEL) == main_bitrate[MAIN_LEVEL-1])
					i_ret = Set_Resolution_Down(SECOND_CHANNEL);
				else
            		i_ret = Set_Bitrate_Down(MAIN_CHANNEL);
            	return i_ret;
        	}
			else if(Get_Cam_Attr(CAM_VQUALITY) >= 3){
				if(Get_Cur_Bitrate(SECOND_CHANNEL) != second_bitrate[SECOND_LEVEL-1])
					i_ret = Set_Bitrate_Down(SECOND_CHANNEL);
				return i_ret;
			}	
        }
		if ((time - iDownTime) > BITRATE_SWITCH_DELAY){
			iDownTime = time;
			if (fusage > 0.6){
				if(Get_Cam_Attr(CAM_VQUALITY) < 3){
					if(Get_Cur_Bitrate(MAIN_CHANNEL) == main_bitrate[MAIN_LEVEL-1])
						i_ret = Set_Resolution_Down(SECOND_CHANNEL);
					else
						i_ret = Set_Bitrate_Down(MAIN_CHANNEL);
				}
				else if(Get_Cam_Attr(CAM_VQUALITY) >= 3){
					if(Get_Cur_Bitrate(SECOND_CHANNEL) != second_bitrate[SECOND_LEVEL-1])
						i_ret = Set_Bitrate_Down(SECOND_CHANNEL);
					return i_ret;
				}	
			}
		}
	}
	return i_ret;
}

void set_master_flag(int enable)
{// 0: enable, -1: disable
	g_media_attr.is_master = enable;
}

int get_master_flag()
{
	return g_media_attr.is_master;
}

void set_master_index(int enable)
{//enable >= 0
	g_media_attr.master_avIndex = enable;
}

int get_master_index()
{
	return g_media_attr.master_avIndex;
}

void Cam_Send_Video(const void *data, const int len, const unsigned long pts, const int keyframe, unsigned char frame_num)
{
	int index = 0;
	int ret, iEr;
	
	FRAMEINFO_t frameInfo;
	memset(&frameInfo, 0, sizeof(FRAMEINFO_t));
	frameInfo.codec_id = MEDIA_CODEC_VIDEO_H264;
	frameInfo.timestamp = pts/1000;
	frameInfo.onlineNum = g_media_attr.is_VideoNum;
	frameInfo.frame_num = frame_num;

	if (keyframe == 1)
		frameInfo.flags = IPC_FRAME_FLAG_IFRAME;
	else
		frameInfo.flags = IPC_FRAME_FLAG_PBFRAME;

	int iNumView = 0;
	for(index = 0; index < MAX_CLIENT_NUMBER; index++){
		if(g_media_attr.g_media_info[index].is_video == 0)
			continue;
			
		iNumView++;	
		ret = avSendFrameData(g_media_attr.g_media_info[index].avIndex, (const char*)data, len, &frameInfo, sizeof(FRAMEINFO_t));
		iEr = Cam_Media_Err(ret);
		if (iEr == 1){
			g_media_attr.g_media_info[index].is_video = 0;
		}
		else if (iEr == 2){
			DBG("send %d AV_ER_EXCEED_MAX_SIZE avResendBufUsageRate %f\n", index, avResendBufUsageRate(g_media_attr.g_media_info[index].avIndex));
			avServResetBuffer(g_media_attr.g_media_info[index].avIndex, RESET_ALL, 0);
		}

		if (get_master_flag() == 0){
			Set_Bitrate_Up();
			set_master_flag(-1);
		}

		ret = Adjust_BitRate(g_media_attr.g_media_info[index].avIndex, frameInfo.timestamp, iEr);
        if(ret == 0) {
            avServResetBuffer(g_media_attr.g_media_info[index].avIndex, RESET_ALL, 0);
			cam_force_Iframe();
        }
	}
	g_media_attr.is_VideoNum = iNumView;
}

static void cam_save_h264(const void *data, const int len, const unsigned long pts, const int keyframe)
{
	FILE *h264_fp = NULL;
	char file_name[128] = {'\0'};
	sprintf(file_name, "/tmp/%ld.h264", pts);

	h264_fp = fopen(file_name, "w+");
	if(h264_fp == NULL){
		DBG("fopen %s error!\n", file_name);
		return ;
	}
	fwrite(data, len, 1, h264_fp);
	fclose(h264_fp);
	return ;
}

void *Video_Live_Proc(void *args)
{
	int ret;
	int index;
    prctl(PR_SET_NAME, "Video_Live_Proc");
	
	char *tmp_data = NULL;
	int tmp_size = 0;
	int tmp_keyframe = 0;
	unsigned long int tmp_timestamp = 0;
	unsigned char tmp_frame_num = 0;
	unsigned char force_Iframe = 1;

	while(g_camenv_attr.running){
		pthread_mutex_lock(&video_live_lock);
		Cam_FramePool_Read(video_live_buffer, &tmp_data, &tmp_size, &tmp_keyframe, &tmp_timestamp, &tmp_frame_num);
		pthread_mutex_unlock(&video_live_lock);
		
		if(tmp_size) {
			if(tmp_data) {
				if(switch_vquality){
					if(tmp_keyframe){
						Cam_Send_Video(tmp_data, tmp_size, tmp_timestamp, tmp_keyframe, tmp_frame_num);
						switch_vquality = 0;
					}
					else{
						if(force_Iframe){
							force_Iframe = 0;
							cam_force_Iframe();
						}
					}
				}
				else
					Cam_Send_Video(tmp_data, tmp_size, tmp_timestamp, tmp_keyframe, tmp_frame_num);	
//					if(tmp_keyframe)
//						cam_save_h264(tmp_data, tmp_size, tmp_timestamp, tmp_keyframe);
			}
		}
		else
			usleep(50*1000);
	}

	DBG("Video_Live_Proc[%d] exit\n", index);
	pthread_exit(0);
}

void Cam_Send_Audio(const void *data, const int len, const unsigned long pts)
{
	int ret, index;

	FRAMEINFO_t frameInfo;
	memset(&frameInfo, 0, sizeof(FRAMEINFO_t));
	frameInfo.timestamp = pts/1000;
	frameInfo.codec_id = MEDIA_CODEC_AUDIO_AAC_ADTS;
	frameInfo.flags = (AUDIO_SAMPLE_16K << 2) | (AUDIO_DATABITS_16 << 1) | AUDIO_CHANNEL_MONO;

	for(index = 0; index < MAX_CLIENT_NUMBER; index++){
		if (g_media_attr.g_media_info[index].is_audio == 0)
			continue;
		ret = avSendAudioData(g_media_attr.g_media_info[index].avIndex, (const char *)data, len, &frameInfo, sizeof(FRAMEINFO_t));
		if (Cam_Media_Err(ret) == 1){
			g_media_attr.g_media_info[index].is_audio = 0;
		}
	}
}

void *Audio_Live_Proc(void *args)
{
	int ret;
	int index;
    prctl(PR_SET_NAME, "Audio_Live_Proc");
	
	char *tmp_data = NULL;
	int tmp_size = 0;
	int tmp_keyframe = 0;
	unsigned long int tmp_timestamp = 0;
	unsigned char tmp_frame_num = 0;

	while(g_camenv_attr.running){
		pthread_mutex_lock(&audio_live_lock);
		Cam_FramePool_Read(audio_live_buffer, &tmp_data, &tmp_size, &tmp_keyframe, &tmp_timestamp, &tmp_frame_num);
		pthread_mutex_unlock(&audio_live_lock);
		
		if(tmp_size) {
			if(tmp_data) {
				Cam_Send_Audio(tmp_data, tmp_size, tmp_timestamp);
			}
		}
		else
			usleep(50*1000);
	}

	DBG("Video_Live_Proc[%d] exit\n", index);
	pthread_exit(0);
}

void *Receive_Audio_Proc(void *arg)
{
	SpeakerStruct  *st = (SpeakerStruct *)arg;
	FRAMEINFO_t frameInfo;
	unsigned int frmNo = 0;
	unsigned int servType;

	int avIndex = avClientStart(st->sid, NULL, NULL, 30, &servType, g_media_attr.talk_avIndex);
	DBG("SID %d av start channle %d avIndex %d\n", st->sid, g_media_attr.talk_avIndex, avIndex);
	if (st)
        free(st);

	if(avIndex < 0)
        goto function_err;

	unsigned char *buf = (unsigned char *)malloc(AUDIO_BUF_SIZE);
    if(buf == NULL){
        DBG("malloc Err with %s\n",strerror(errno));
        goto function_err;
    }

    aac_decoder_start();
	while (g_media_attr.is_talk){
		int ret = avCheckAudioBuf(avIndex);
		if (ret < 2){
			usleep(10000);
			continue;
		}

		ret = avRecvAudioData(avIndex, buf, (AUDIO_BUF_SIZE), (char *)&frameInfo, sizeof(FRAMEINFO_t), &frmNo);
		if(ret > 0){
            ret = aac_decode(buf, ret);
            if(ret){
//				DBG("### Receive_Audio len=%d\n", ret);
    			ipc_audio_play(aac_decoder_buf, ret);
            }
		}
	}

    if(buf)
        free(buf);
    aac_decoder_stop();

	avClientStop(avIndex);
	g_media_attr.is_talking = 0;
	DBG("Receive_Audio_Proc Exit\n");
	return (void *)0;
function_err:
	g_media_attr.is_talking = 0;
	DBG("Receive_Audio_Proc Exit\n");
	return (void *)-1;
}



int Cam_Stream_Init()
{
	int ret = 0;

	pthread_mutex_init(&audio_record_lock,NULL);
	pthread_mutex_init(&video_record_lock,NULL);
	pthread_mutex_init(&audio_live_lock,NULL);
	pthread_mutex_init(&video_live_lock,NULL);

	video_record_buffer = Cam_FramePool_Init(VIDEO_RECORD_BUFFER_SIZE, USED_VIDEO_RECORD_BUF_NUM, MAX_VIDEO_RECORD_BUF_NUM);
	CHECK(video_record_buffer != NULL, -1, "video_record_buffer init error \n");

	audio_record_buffer = Cam_FramePool_Init(AUDIO_RECORD_BUFFER_SIZE, USED_AUDIO_RECORD_BUF_NUM, MAX_AUDIO_RECORD_BUF_NUM);
	CHECK(audio_record_buffer != NULL, -1, "audio_record_buffer init error \n");

	video_live_buffer = Cam_FramePool_Init(VIDEO_LIVE_BUFFER_SIZE, USED_VIDEO_LIVE_BUF_NUM, MAX_VIDEO_LIVE_BUF_NUM);
	CHECK(video_live_buffer != NULL, -1, "video_live_buffer init error \n");

	audio_live_buffer = Cam_FramePool_Init(AUDIO_LIVE_BUFFER_SIZE, USED_AUDIO_LIVE_BUF_NUM, MAX_AUDIO_LIVE_BUF_NUM);
	CHECK(audio_live_buffer != NULL, -1, "audio_live_buffer init error \n");

	pthread_t video_live_pid;
	ret = pthread_create(&video_live_pid, NULL, &Video_Live_Proc, NULL);
	CHECK(!ret, -1, "Video_Live_Proc create fail, ret=[%d]\n", ret);	
	pthread_detach(video_live_pid);

	pthread_t audio_live_pid;
	ret = pthread_create(&audio_live_pid, NULL, &Audio_Live_Proc, NULL);
	CHECK(!ret, -1, "Audio_Live_Proc create fail, ret=[%d]\n", ret);	
	pthread_detach(audio_live_pid);
	
	return 0;
}

int Cam_Media_Init()
{
	int ret = 0;
	int i = 0;
	for(i = 0; i < MEDIA_USER_NUM; i++){
		
		g_media_attr.g_media_info[i].avIndex = -1;
		g_media_attr.g_media_info[i].is_video = 0;
		g_media_attr.g_media_info[i].is_audio = 0;
		pthread_mutex_init(&g_media_attr.g_media_info[i].video_lock, NULL);
		pthread_mutex_init(&g_media_attr.g_media_info[i].audio_lock, NULL);
	}

	pthread_mutex_init(&g_media_attr.talk_lock, NULL);
	pthread_mutex_init(&g_media_attr.play_lock,NULL);	/*playback lock init*/
	g_media_attr.is_VideoNum = 0;

	ret = Cam_Stream_Init();
	CHECK(ret == 0, -1, "Cam_Stream_Init Init error \n");

	ret = Cam_Video_Init();
	CHECK(ret == 0, -1, "Cam_Video_Init Init error \n");	

	ret = Cam_Audio_Init();
	CHECK(ret == 0, -1, "Cam_Audio_Init Init error \n");
	
	ret = Cam_Record_Init();
	CHECK(ret == 0, -1, "Cam_Record_Init Init error \n");	
	
	return ret;
}

void Cam_Audio_UnInit()
{
	ipc_audio_exit();	
	
	Cam_FramePool_UnInit(audio_record_buffer);
	Cam_FramePool_UnInit(audio_live_buffer);

	pthread_mutex_destroy(&audio_record_lock);
	pthread_mutex_destroy(&audio_live_lock);
}

void Cam_Video_UnInit()
{
	ipc_video_exit();	
	
	Cam_FramePool_UnInit(video_record_buffer);
	Cam_FramePool_UnInit(video_live_buffer);

	pthread_mutex_destroy(&video_record_lock);
	pthread_mutex_destroy(&video_live_lock);
}

void Cam_Stream_UnInit()
{
	Cam_Audio_UnInit();
	Cam_Video_UnInit();
}

void Cam_Media_UnInit()
{
	DBG("###Start Cam_Media_UnInit###\n");
	Cam_Stream_UnInit();

	if(aac_decoder_buf)
		free(aac_decoder_buf);
	aac_decoder_buf = NULL;	

	DBG("###End Cam_Media_UnInit###\n");
}

void Cam_Media_Cmd(int SID, int avIndex, char *buf, int type, int iMediaNo)
{
	int ret;
	
	switch(type)
	{
		case IOTYPE_USER_IPCAM_START:
		{
			DBG("IOTYPE_USER_IPCAM_START Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			pthread_mutex_lock(&g_media_attr.g_media_info[iMediaNo].video_lock);
			g_media_attr.g_media_info[iMediaNo].is_video = 1;
			g_media_attr.g_media_info[iMediaNo].is_audio = 1;
			pthread_mutex_unlock(&g_media_attr.g_media_info[iMediaNo].video_lock);

			if(g_camenv_attr.online_user_num == 1){	
				pthread_mutex_lock(&video_live_lock);
				Cam_FramePool_Stop(video_live_buffer);
				Cam_FramePool_Clean(video_live_buffer);
				Cam_FramePool_Start(video_live_buffer);
				pthread_mutex_unlock(&video_live_lock);

				pthread_mutex_lock(&audio_live_lock);
				Cam_FramePool_Stop(audio_live_buffer);
				Cam_FramePool_Clean(audio_live_buffer);
				Cam_FramePool_Start(audio_live_buffer);
				pthread_mutex_unlock(&audio_live_lock);
			}

			if(get_master_index() == avIndex)
				set_master_flag(0);
			
			cam_force_Iframe();
		}break;
		case IOTYPE_USER_IPCAM_STOP:
		{
			DBG("IOTYPE_USER_IPCAM_STOP Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			pthread_mutex_lock(&g_media_attr.g_media_info[iMediaNo].video_lock);
			g_media_attr.g_media_info[iMediaNo].is_video = 0;
			g_media_attr.g_media_info[iMediaNo].is_audio = 0;
			pthread_mutex_unlock(&g_media_attr.g_media_info[iMediaNo].video_lock);
			DBG("IOTYPE_USER_IPCAM_STOP Enter\n");
		}break;
		case IOTYPE_USER_IPCAM_AUDIOSTART:
		{
			pthread_mutex_lock(&g_media_attr.g_media_info[iMediaNo].audio_lock);
			g_media_attr.g_media_info[iMediaNo].is_audio = 1;
			pthread_mutex_unlock(&g_media_attr.g_media_info[iMediaNo].audio_lock);
			DBG("IOTYPE_USER_IPCAM_AUDIOSTART Enter\n");
		}break;
		case IOTYPE_USER_IPCAM_AUDIOSTOP:
		{
			pthread_mutex_lock(&g_media_attr.g_media_info[iMediaNo].audio_lock);
			g_media_attr.g_media_info[iMediaNo].is_audio = 0;
			pthread_mutex_unlock(&g_media_attr.g_media_info[iMediaNo].audio_lock);
			DBG("IOTYPE_USER_IPCAM_AUDIOSTOP Enter\n");
		}break;	
		case IOTYPE_USER_IPCAM_SPEAKERSTART:
		{
			DBG("IOTYPE_USER_IPCAM_SPEAKERSTART Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;

			pthread_mutex_lock(&g_media_attr.talk_lock);
			if((g_media_attr.is_talk == 0)&&(g_media_attr.is_talking == 0)){
				g_media_attr.talk_avIndex = p->channel;
				g_media_attr.is_talk = 1;
			    g_media_attr.is_talking = 1;

				SpeakerStruct  *st;
				st = (SpeakerStruct *)malloc(sizeof(SpeakerStruct));
				if(!st){
					DBG("SpeakerStruct size %d error\n", sizeof(SpeakerStruct));
					pthread_mutex_unlock(&g_media_attr.talk_lock);
					break;
				}
				st->sid = SID;
				st->iIndex = iMediaNo;
				DBG("st.sid %d st index %d channel %d\n", st->sid, st->iIndex, p->channel);

				pthread_t talk_pid;
				if((ret = pthread_create(&talk_pid, NULL, &Receive_Audio_Proc, (void *)st))<0)
				{
					pthread_mutex_unlock(&g_media_attr.talk_lock);
					DBG("pthread_create ret=%d\n", ret);
					break;
				}
				pthread_detach(talk_pid);
			}
			pthread_mutex_unlock(&g_media_attr.talk_lock);
		}break;
		case IOTYPE_USER_IPCAM_SPEAKERSTOP:
		{
			DBG("IOTYPE_USER_IPCAM_SPEAKERSTOP Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			pthread_mutex_lock(&g_media_attr.talk_lock);
			g_media_attr.is_talk = 0;
			pthread_mutex_unlock(&g_media_attr.talk_lock);
		}
		break;
		case IOTYPE_USER_IPCAM_GETAUDIOOUTFORMAT_REQ:
		{
			DBG("IOTYPE_USER_IPCAM_GETAUDIOOUTFORMAT_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			SMsgAVIoctrlGetAudioOutFormatResp resp;

			resp.channel = 0;
			resp.format      = MEDIA_CODEC_AUDIO_AAC_ADTS;
            resp.sample_rate = AUDIO_SAMPLE_16K;
			resp.bitdata     = AUDIO_DATABITS_16;
			resp.channels    =  AUDIO_CHANNEL_MONO;

			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_GETAUDIOOUTFORMAT_RESP, (char *)&resp, sizeof(SMsgAVIoctrlGetAudioOutFormatResp)) < 0)
				break;
		}break;
		case IOTYPE_USER_IPCAM_SETSTREAMCTRL_REQ:
		{
			DBG("IOTYPE_USER_IPCAM_SETSTREAMCTRL_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			SMsgAVIoctrlSetStreamCtrlReq *p = (SMsgAVIoctrlSetStreamCtrlReq *)buf;
			SMsgAVIoctrlSetStreamCtrlResp resp;

			DBG("video quality %d\n", p->quality);
			if(Get_Cam_Attr(CAM_VQUALITY) != p->quality){
				switch_vquality = 1;
				Set_Cam_Attr(CAM_VQUALITY,p->quality);				
				pthread_mutex_lock(&video_live_lock);
				Cam_FramePool_Stop(video_live_buffer);
				Cam_FramePool_Clean(video_live_buffer);
				Cam_FramePool_Start(video_live_buffer);
				pthread_mutex_unlock(&video_live_lock);			
			}

			resp.result = 0;
			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_SETSTREAMCTRL_RESP, (char *)&resp, sizeof(SMsgAVIoctrlSetStreamCtrlResp)) < 0)
				break;
		}break;
		case IOTYPE_USER_IPCAM_GETSTREAMCTRL_REQ:
		{
			DBG("IOTYPE_USER_IPCAM_GETSTREAMCTRL_REQ Enter index %d iMediaNo %d\n", avIndex, iMediaNo);
			SMsgAVIoctrlGetStreamCtrlResq resp;
			resp.channel = avIndex;
			
			unsigned char video_quality = Get_Cam_Attr(CAM_VQUALITY);
			DBG("video qualit %d\n", video_quality);
			resp.quality = video_quality;
			
			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_GETSTREAMCTRL_RESP, (char *)&resp, sizeof(SMsgAVIoctrlGetStreamCtrlResq)) < 0)
				break;
		}break;
		case IOTYPE_USER_IPCAM_GET_VIDEOMODE_REQ:
		{
			SMsgAVIoctrlGetVideoModeResp resp;
			resp.channel = avIndex;
			resp.mode = Get_Cam_Attr(CAM_INVERSION);
			DBG("Video Flip Mode %d\n", resp.mode);
			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_GET_VIDEOMODE_RESP, (char *)&resp, sizeof(SMsgAVIoctrlGetVideoModeResp)) < 0)
				break;
		}break;
		case IOTYPE_USER_IPCAM_SET_VIDEOMODE_REQ:
		{
			SMsgAVIoctrlSetVideoModeReq *p = (SMsgAVIoctrlSetVideoModeReq *)buf;
			Set_Cam_Attr(CAM_INVERSION, p->mode);

			SMsgAVIoctrlSetVideoModeResp resp;
			resp.channel = avIndex;
			resp.result = 0;

			if(avSendIOCtrl(avIndex, IOTYPE_USER_IPCAM_SET_VIDEOMODE_RESP, (char *)&resp, sizeof(SMsgAVIoctrlSetVideoModeResp)) < 0)
				break;
		}break;
		
		default:
			DBG("NO Find Cmd index %d iMediaNo %d\n", avIndex, iMediaNo);
			break;
		
	}

	return ;
}

