/*
 * ipc_audio.c
 * Author:yuanhao@yingshixun.com
 * Date:2019.2
 */

#include "imp_log.h"
#include "imp_common.h"
#include "imp_audio.h"

#include "ipc_comm.h"
#include "ipc_audio.h"
#include "ipc_debug.h"
#include "aw8733_ctl.h"
#include "ipc_frame_pool.h"
 
#define TAG "Sample-Video"

#define AW8733A_FILE     "/dev/adc"
#define AO_TEST_SAMPLE_RATE 16000
#define AO_TEST_SAMPLE_TIME 40
#define AO_TEST_BUF_SIZE (AO_TEST_SAMPLE_RATE * sizeof(short) * AO_TEST_SAMPLE_TIME / 1000)

typedef struct ipc_audio_info_s
{
	IPC_AUDIO_S audio; 
    unsigned char last_aw8733a_stat;
    struct timeval last_aw8733a_time;
	
	pthread_t pthread_id[IPC_AUDIO_SUM];
	pthread_mutex_t pthread_mutex;
	unsigned char running;
}IPC_AUDIO_INFO_S;
static IPC_AUDIO_INFO_S* g_audio_args = NULL;


static IMPAudioSoundMode ipc_get_soundmode(IPC_AUDIO_DEVID_E device_id)
{
	IMPAudioSoundMode enSoundMode = AUDIO_SOUND_MODE_MONO;   

	switch(g_audio_args->audio.audio_device[device_id].soundMode)
    {
        case 1:
        {
            enSoundMode = AUDIO_SOUND_MODE_MONO;
            break;
        }
        case 2:
        {
            enSoundMode = AUDIO_SOUND_MODE_STEREO; 
            break;
        }
        default:
        {
            enSoundMode = AUDIO_SOUND_MODE_MONO; 
            break;
        }
    }

    return enSoundMode;
}

static IMPAudioSampleRate ipc_get_samplerate(IPC_AUDIO_DEVID_E device_id)
{
	IMPAudioSampleRate enSamplerate = AUDIO_SAMPLE_RATE_16000;  

	switch(g_audio_args->audio.audio_device[device_id].sampleRate)
    {
        case 8000:
        {
            enSamplerate = AUDIO_SAMPLE_RATE_8000;
            break;
        }
        case 16000:
        {
            enSamplerate = AUDIO_SAMPLE_RATE_16000;
            break;
        }
		case 44100:
        {
            enSamplerate = AUDIO_SAMPLE_RATE_44100;
            break;
        }
        default:
        {
            enSamplerate = AUDIO_SAMPLE_RATE_16000;
            break;
        }
    }

    return enSamplerate;
}

static int ipc_ao_enable(void)
{
	int ret;

	ret = IMP_AO_Enable(IPC_AUDIO_AO);
	if (ret != 0) {
		IMP_LOG_ERR(TAG, "enable ao %d err\n", IPC_AUDIO_AO);
		return -1;
	}

	ret = IMP_AO_EnableChn(IPC_AUDIO_AO, 0);
	if (ret != 0) {
		IMP_LOG_ERR(TAG, "Audio play enable channel failed\n");
		return -1;
	}

	return 0;
}

static int ipc_ao_init(void)
{
	int ret;

	IMPAudioIOAttr attr;
	attr.samplerate = ipc_get_samplerate(IPC_AUDIO_AO);
	attr.bitwidth = AUDIO_BIT_WIDTH_16;
	attr.soundmode = ipc_get_soundmode(IPC_AUDIO_AO);
	attr.frmNum = 1000/(g_audio_args->audio.audio_device[IPC_AUDIO_AO].sampleRate/g_audio_args->audio.audio_device[IPC_AUDIO_AO].ptNumPerFrm);
	attr.numPerFrm = g_audio_args->audio.audio_device[IPC_AUDIO_AO].ptNumPerFrm;
	attr.chnCnt = 1;
	
	ret = IMP_AO_SetPubAttr(IPC_AUDIO_AO, &attr);
	if (ret != 0) {
		IMP_LOG_ERR(TAG, "set ao %d attr err: %d\n", IPC_AUDIO_AO, ret);
		return -1;
	}

	memset(&attr, 0x0, sizeof(attr));
	ret = IMP_AO_GetPubAttr(IPC_AUDIO_AO, &attr);
	if (ret != 0) {
		IMP_LOG_ERR(TAG, "get ao %d attr err: %d\n", IPC_AUDIO_AO, ret);
		return -1;
	}

	DBG("Audio Out GetPubAttr samplerate:%d\n", attr.samplerate);
	DBG("Audio Out GetPubAttr   bitwidth:%d\n", attr.bitwidth);
	DBG("Audio Out GetPubAttr  soundmode:%d\n", attr.soundmode);
	DBG("Audio Out GetPubAttr     frmNum:%d\n", attr.frmNum);
	DBG("Audio Out GetPubAttr  numPerFrm:%d\n", attr.numPerFrm);
	DBG("Audio Out GetPubAttr     chnCnt:%d\n", attr.chnCnt);

	return 0;
}

int ipc_ao_set_volume(int output_vol)
{
	int ret;
	
	ret = IMP_AO_SetVol(IPC_AUDIO_AO, 0, output_vol);
	if (ret != 0) {
		IMP_LOG_ERR(TAG, "Audio Play set volume failed\n");
		return -1;
	}

	int aogain = 28;
	ret = IMP_AO_SetGain(IPC_AUDIO_AO, 0, aogain);
	if (ret != 0) {
		IMP_LOG_ERR(TAG, "Audio Record Set Gain failed\n");
		return -1;
	}

	return 0;		
}

int ipc_set_spkvol(int value)
{
	return ipc_ao_set_volume(value);
}

static int ipc_ai_enable(void)
{
	int ret;

	/* Step 2: enable AI device. */
	ret = IMP_AI_Enable(IPC_AUDIO_AI);
	if(ret != 0) {
		IMP_LOG_ERR(TAG, "enable ai %d err\n", IPC_AUDIO_AI);
		return -1;
	}

	/* Step 3: set audio channel attribute of AI device. */
	IMPAudioIChnParam chnParam;
	chnParam.usrFrmDepth = 20;
	ret = IMP_AI_SetChnParam(IPC_AUDIO_AI, 0, &chnParam);
	if(ret != 0) {
		IMP_LOG_ERR(TAG, "set ai %d channel 0 attr err: %d\n", IPC_AUDIO_AI, ret);
		return -1;
	}

	memset(&chnParam, 0x0, sizeof(chnParam));
	ret = IMP_AI_GetChnParam(IPC_AUDIO_AI, 0, &chnParam);
	if(ret != 0) {
		IMP_LOG_ERR(TAG, "get ai %d channel 0 attr err: %d\n", IPC_AUDIO_AI, ret);
		return -1;
	}

	IMP_LOG_INFO(TAG, "Audio In GetChnParam usrFrmDepth : %d\n", chnParam.usrFrmDepth);

	/* Step 4: enable AI channel. */
	ret = IMP_AI_EnableChn(IPC_AUDIO_AI, 0);
	if(ret != 0) {
		IMP_LOG_ERR(TAG, "Audio Record enable channel failed\n");
		return -1;
	}

	return 0;
}

static int ipc_ai_init(void)
{
	int ret;

	IMPAudioIOAttr attr;
	attr.samplerate = ipc_get_samplerate(IPC_AUDIO_AI);
	attr.bitwidth = AUDIO_BIT_WIDTH_16;
	attr.soundmode = ipc_get_soundmode(IPC_AUDIO_AI);
	attr.frmNum = 1000/(g_audio_args->audio.audio_device[IPC_AUDIO_AI].sampleRate/g_audio_args->audio.audio_device[IPC_AUDIO_AI].ptNumPerFrm);
	attr.numPerFrm = g_audio_args->audio.audio_device[IPC_AUDIO_AI].ptNumPerFrm;
	attr.chnCnt = 1;
	
	ret = IMP_AI_SetPubAttr(IPC_AUDIO_AI, &attr);
	if(ret != 0) {
		IMP_LOG_ERR(TAG, "set ai %d attr err: %d\n", IPC_AUDIO_AI, ret);
		return -1;
	}

	memset(&attr, 0x0, sizeof(attr));
	ret = IMP_AI_GetPubAttr(IPC_AUDIO_AI, &attr);
	if(ret != 0) {
		IMP_LOG_ERR(TAG, "get ai %d attr err: %d\n", IPC_AUDIO_AI, ret);
		return -1;
	}

	DBG("Audio In GetPubAttr samplerate : %d\n", attr.samplerate);
	DBG("Audio In GetPubAttr   bitwidth : %d\n", attr.bitwidth);
	DBG("Audio In GetPubAttr  soundmode : %d\n", attr.soundmode);
	DBG("Audio In GetPubAttr     frmNum : %d\n", attr.frmNum);
	DBG("Audio In GetPubAttr  numPerFrm : %d\n", attr.numPerFrm);
	DBG("Audio In GetPubAttr     chnCnt : %d\n", attr.chnCnt);

	return 0;
}

int ipc_ai_set_volume(int input_vol)
{
	int ret;
	/* Step 5: Set audio channel volume. */
	ret = IMP_AI_SetVol(IPC_AUDIO_AI, 0, input_vol);
	if(ret != 0) {
		IMP_LOG_ERR(TAG, "Audio Record set volume failed\n");
		return -1;
	}

	int aigain = 28;
	ret = IMP_AI_SetGain(IPC_AUDIO_AI, 0, aigain);
	if(ret != 0) {
		IMP_LOG_ERR(TAG, "Audio Record Set Gain failed\n");
		return -1;
	}

	ret = IMP_AI_GetGain(IPC_AUDIO_AI, 0, &aigain);
	if(ret != 0) {
		IMP_LOG_ERR(TAG, "Audio Record Get Gain failed\n");
		return -1;
	}
	IMP_LOG_INFO(TAG, "Audio In GetGain    gain : %d\n", aigain);

	return 0;
}

static int audio_aw8733a(int arg)
{
    int fd = -1;
    int ret = 0;

    fd = open(AW8733A_FILE, O_RDWR);
    if(fd <= 0)
	{
		DBG("can't open file[%s]: %s\n", AW8733A_FILE, strerror(errno));
		close(fd);
		return -1;
	}

    ret = ioctl(fd, DRV_CMD_AW8733A, arg);
    CHECK(ret == 0, -1, "Error with %#x.\n", ret);

    close(fd);
    DBG("aw8733a status: %d\n", arg);
    return 0;
}

static void *audio_play_proc(void *args)
{
	int ret;

	char pr_name[64];
    memset(pr_name,0,sizeof(pr_name));
    sprintf(pr_name,"audio_play_proc");
    prctl(PR_SET_NAME,pr_name);
	
	while(g_audio_args->running) {
	
		int pass_time = util_time_pass(&g_audio_args->last_aw8733a_time);
        if (pass_time > 3*1000)
        {
            if (g_audio_args->last_aw8733a_stat > AW8733A_STATUS_CLOSE)
            {
                g_audio_args->last_aw8733a_stat = AW8733A_STATUS_CLOSE;
                ret = audio_aw8733a(g_audio_args->last_aw8733a_stat);
                CHECK(ret == 0, NULL, "Error with %#x.\n", ret);
            }
        }

        usleep(100*1000);
	}

	DBG("audio_play_proc exit OK\n");
	pthread_exit(0);
}

static int ipc_audio_cb(char *frame, unsigned long len, unsigned long int timestamp)
{
	g_audio_args->audio.audio_cb(frame, len, timestamp);
	return 0;
}

static void *audio_cap_proc(void *args)
{
	int ret;

	char pr_name[64];
    memset(pr_name,0,sizeof(pr_name));
    sprintf(pr_name,"audio_cap_proc");
    prctl(PR_SET_NAME,pr_name);
	
	while(g_audio_args->running) {
		/* Step 6: get audio record frame. */
		ret = IMP_AI_PollingFrame(IPC_AUDIO_AI, 0, 1000);
		if (ret != 0 ) {
			IMP_LOG_ERR(TAG, "Audio Polling Frame Data error\n");
		}
		
		IMPAudioFrame frm;
		ret = IMP_AI_GetFrame(IPC_AUDIO_AI, 0, &frm, BLOCK);
		if(ret != 0) {
			IMP_LOG_ERR(TAG, "Audio Get Frame Data error\n");
			return (void *)-1;
		}

		ipc_audio_cb((char *)frm.virAddr, frm.len, (unsigned long int)frm.timeStamp);
//		DBG("###ipc_audio_cb pts:%ld###\n", frm.timeStamp);

		/* Step 8: release the audio record frame. */
		ret = IMP_AI_ReleaseFrame(IPC_AUDIO_AI, 0, &frm);
		if(ret != 0) {
			IMP_LOG_ERR(TAG, "Audio release frame data error\n");
			return (void *)-1;
		}

	}
	/* Step 9: disable the audio channel. */
	ret = IMP_AI_DisableChn(IPC_AUDIO_AI, 0);
	if(ret != 0) {
		IMP_LOG_ERR(TAG, "Audio channel disable error\n");
		return (void *)-1;
	}

	/* Step 10: disable the audio devices. */
	ret = IMP_AI_Disable(IPC_AUDIO_AI);
	if(ret != 0) {
		IMP_LOG_ERR(TAG, "Audio device disable error\n");
		return (void *)-1;
	}

	DBG("audio_cap_proc exit OK\n");
	pthread_exit(0);
}

int ipc_audio_init(IPC_AUDIO_S *audio)
{
	int ret;

	CHECK(g_audio_args == NULL, -1, "ipc_audio_init failed\n");
	CHECK(audio != NULL, -1, "ipc_audio_init failed\n");

	g_audio_args = (IPC_AUDIO_INFO_S*)malloc(sizeof(IPC_AUDIO_INFO_S));
	CHECK(g_audio_args != NULL, -1, "ipc_audio_init failed\n");

	memset(g_audio_args, 0, sizeof(IPC_AUDIO_INFO_S));	
	pthread_mutex_init(&g_audio_args->pthread_mutex, NULL);
	memcpy(&g_audio_args->audio, audio, sizeof(IPC_AUDIO_S));

	/* set public attribute of AO device. */
	ret = ipc_ao_init();
	CHECK(ret == 0, -1, "ipc_ao_init failed\n");

	/* enable AO device and channel. */
	ret = ipc_ao_enable();
	CHECK(ret == 0, -1, "ipc_ao_enable failed\n");

	/* set AO channel volume. */
	ret = ipc_ao_set_volume(g_audio_args->audio.audio_device[IPC_AUDIO_AO].volume);
	CHECK(ret == 0, -1, "ipc_ao_set_volume failed\n");

	ret = ipc_ai_init();
	CHECK(ret == 0, -1, "ipc_ai_init failed\n");

	ret = ipc_ai_enable();
	CHECK(ret == 0, -1, "ipc_ai_enable failed\n");

	ret = ipc_ai_set_volume(g_audio_args->audio.audio_device[IPC_AUDIO_AI].volume);
	CHECK(ret == 0, -1, "ipc_ai_set_volume failed\n");

	g_audio_args->running = 1;
	g_audio_args->last_aw8733a_stat = AW8733A_STATUS_CLOSE;
    ret = audio_aw8733a(g_audio_args->last_aw8733a_stat);
    CHECK(ret == 0, -1, "audio_aw8733a failed\n");
    util_time_abs(&g_audio_args->last_aw8733a_time);

	ret = pthread_create(&g_audio_args->pthread_id[IPC_AUDIO_AO], NULL, audio_play_proc, NULL);
	CHECK(ret == 0, -1, "create audio_play_proc failed\n");

    ret = pthread_create(&g_audio_args->pthread_id[IPC_AUDIO_AI], NULL, audio_cap_proc, NULL);
	CHECK(ret == 0, -1, "create audio_cap_proc failed\n");
	
	return 0;
}

int ipc_audio_exit(void)
{
	int ret;

	g_audio_args->running = 0;
	pthread_join(g_audio_args->pthread_id[IPC_AUDIO_AO], NULL);
	pthread_join(g_audio_args->pthread_id[IPC_AUDIO_AI], NULL);
	audio_aw8733a(AW8733A_STATUS_CLOSE);

	ret = IMP_AO_FlushChnBuf(IPC_AUDIO_AO, 0);
	if (ret != 0) {
		IMP_LOG_ERR(TAG, "IMP_AO_FlushChnBuf error\n");
		return -1;
	}
	/* Step 6: disable the audio channel. */
	ret = IMP_AO_DisableChn(IPC_AUDIO_AO, 0);
	if (ret != 0) {
		IMP_LOG_ERR(TAG, "Audio channel disable error\n");
		return -1;
	}

	/* Step 7: disable the audio devices. */
	ret = IMP_AO_Disable(IPC_AUDIO_AO);
	if (ret != 0) {
		IMP_LOG_ERR(TAG, "Audio device disable error\n");
		return -1;
	}

	if(g_audio_args){
		free(g_audio_args);
		g_audio_args = NULL;
	}

	return 0;
}

int ipc_audio_play(char *frame, int frame_len)
{
	int ret;

    if (g_audio_args->last_aw8733a_stat != AW8733A_STATUS_16DB)
    {
        g_audio_args->last_aw8733a_stat = AW8733A_STATUS_16DB;
        ret = audio_aw8733a(g_audio_args->last_aw8733a_stat);
        CHECK(ret == 0, -1, "Error with %#x.\n", ret);
    }
    util_time_abs(&g_audio_args->last_aw8733a_time);

	uint8_t *start;
    uint32_t has_left, totel;
    IMPAudioFrame frm;
	char buf[2048] = {0};

    start = (uint8_t *)frame;
    has_left = totel = frame_len;

	while(1){
        if(has_left >= AO_TEST_BUF_SIZE){
            memcpy(buf, start+(totel-has_left), AO_TEST_BUF_SIZE);
            has_left -= AO_TEST_BUF_SIZE;
            frm.virAddr = (uint32_t *)buf;
            frm.len = AO_TEST_BUF_SIZE;
            ret = IMP_AO_SendFrame(IPC_AUDIO_AO, 0, &frm, BLOCK);
			if (ret != 0) {
				IMP_LOG_ERR(TAG, "send Frame Data error\n");
				return -1;
			}
        }else{
            memcpy(buf, start+(totel-has_left), has_left);
            frm.virAddr = (uint32_t *)buf;
            frm.len = has_left;
            ret = IMP_AO_SendFrame(IPC_AUDIO_AO, 0, &frm, BLOCK);
  			if (ret != 0) {
				IMP_LOG_ERR(TAG, "send Frame Data error\n");
				return -1;
			}
            break;
        }
    }

	return 0;	
}

int ipc_audio_play_file(char *filepath)
{
	FILE* fp = fopen(filepath, "r");
    CHECK(fp, -1, "error with %s\n", strerror(errno));

    char buf[1024] = {0};
    int buf_len = 0;
    
    while (!feof(fp))
    {
        memset(buf, 0, sizeof(buf));
        buf_len = fread(buf, 1, AO_TEST_BUF_SIZE, fp);
        if(buf_len == AO_TEST_BUF_SIZE)
  			ipc_audio_play(buf, buf_len);
    }

    usleep(500*1000);
    fclose(fp);
    DBG("play %s done.\n", filepath);

	return 0;
}

