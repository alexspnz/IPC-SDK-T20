
#include "ipc_rtsp_process.h"
#include "ipc_rtp.h"
#include "ipc_frame_pool.h"
#include "ipc_select.h"
#include "ipc_debug.h"
#include "ipc_malloc.h"
#include "ipc_util.h"
#include "ipc_rtsp_server.h"
#include "ipc_base64.h"

#define GENERAL_RWTIMEOUT (10*1000)
#define RTSP_INTERLEAVED_FRAME_SIZE (4)

#define GOTO(exp, to, fmt...) do\
{\
    if (!(exp))\
    {\
        ERR(fmt);\
        goto to;\
    }\
}while(0)

typedef enum RTSP_SERVER_STATUS_E
{
    RTSP_SERVER_STATUS_RTSP,
    RTSP_SERVER_STATUS_RTP,
    RTSP_SERVER_STATUS_TEARDOWN,
    RTSP_SERVER_STATUS_ERROR,
} RTSP_SERVER_STATUS_E;

typedef enum TRANS_TYPE_E
{
    TRANS_TYPE_TCP,
    TRANS_TYPE_UDP,
} TRANS_TYPE_E;

typedef enum STREAM_ID_E
{
	SUB_STREAM,
    MAIN_STREAM,
} STREAM_ID_E;

typedef struct RTSP_SERVER_S
{
    RTSP_SERVER_STATUS_E enStatus;

    handle hndSocket;
    handle hndReader;
    handle hndAReader;

    TRANS_TYPE_E enTranType;

	STREAM_ID_E stream_id;

    //rtsp
    char szSession[32];
    unsigned int u32SetupCount;
    char szUrl[128];

    unsigned char au8Sps[64];
    unsigned int u32SpsSize;
    unsigned char au8Pps[64];
    unsigned int u32PpsSize;

    //rtp
    unsigned int u32LastAVUniqueIndex;
    unsigned long long u64LastVTimeStamp;
    unsigned long long u64LastATimeStamp;

    unsigned short u16VSeqNum;
    unsigned short u16ASeqNum;

    unsigned int u32VTimeStamp;
    unsigned int u32ATimeStamp;

    // audio and video enable switch
    int bAEnable;
    int bVEnable;
}
RTSP_SERVER_S;

static int _RtspComplete(void* _pData, int _u32DataLen, int* _pu32CompleteLen)
{
    //DBG("_u32DataLen=%u\n", _u32DataLen);
    *_pu32CompleteLen = 0;
    char* szLine = (char*)_pData;
    szLine[_u32DataLen] = 0;

    if (memcmp(_pData, "OPTIONS", strlen("OPTIONS"))
        && memcmp(_pData, "DESCRIBE", strlen("DESCRIBE"))
        && memcmp(_pData, "SETUP", strlen("SETUP"))
        && memcmp(_pData, "PLAY", strlen("PLAY"))
        && memcmp(_pData, "TEARDOWN", strlen("TEARDOWN")))
    {
        CHECK(0, -1, "Unknown cmd: %s\n", szLine);
    }

    char* szHeaderReturn = strstr(szLine, "\r\n\r\n");
    char* szContentLength;
    if (szHeaderReturn != NULL)
    {
        szContentLength = strstr(szLine, "Content-Length: ");
        if (szContentLength == NULL)
        {
            *_pu32CompleteLen = szHeaderReturn - szLine + 4;
        }

        if (szContentLength != NULL)
        {
            int s32ContentLength = atoi(szContentLength + strlen("Content-Length: "));
            if (szHeaderReturn - szLine + 4 + s32ContentLength <= _u32DataLen)
            {
                *_pu32CompleteLen = szHeaderReturn - szLine + 4 + s32ContentLength;
            }
        }
    }

    return 0;
}

static int _RtspOrRtcpComplete(void* _pData, int _u32DataLen, int* _pu32CompleteLen)
{
    //DBG("_u32DataLen=%u\n", _u32DataLen);
    *_pu32CompleteLen = 0;

    unsigned char u8Start;
    unsigned short u16Size;
    memcpy(&u8Start, _pData, sizeof(unsigned char));
    if (u8Start == 0x24) // $
    {

        if (_u32DataLen >= RTSP_INTERLEAVED_FRAME_SIZE)
        {
            memcpy(&u16Size, _pData + 2, sizeof(unsigned short));
            u16Size = ntohs(u16Size);
            if (_u32DataLen >= RTSP_INTERLEAVED_FRAME_SIZE + u16Size)
            {
                *_pu32CompleteLen = RTSP_INTERLEAVED_FRAME_SIZE + u16Size;
            }
        }
    }
    else
    {
        _RtspComplete(_pData, _u32DataLen, _pu32CompleteLen);
    }

    //DBG("_pu32CompleteLen=%u\n", *_pu32CompleteLen);

    return 0;
}

static int _CheckBuf(char* _szFormat, ...)
{
    va_list pArgs;
    va_start(pArgs, _szFormat);
    int s32Ret = vsnprintf(NULL, 0, _szFormat, pArgs);
    va_end(pArgs);
    return s32Ret;
}

static char* _SafeFind(char* _pBegin, char* _pEnd, char* _szKey)
{
    int i = 0, j = 0, k = 0;
    int s32CmpLen = strlen(_szKey);
    int Len = _pEnd - _pBegin;
    CHECK(Len > 0, NULL, "size is invalid[%d]\n", Len);

    for (i = 0; i < Len; i++)
    {
        if (_pBegin[i] == _szKey[j])
        {
            for (j = 0, k = i; j < s32CmpLen; j++, k++)
            {
                if (_pBegin[k] != _szKey[j])
                {
                    j = 0;
                    break;
                }
                if (j == s32CmpLen - 1)
                {
                    return _pBegin + i;
                }
            }
        }
    }

    return NULL;
}

static int _GetKeyValue(char* _szBegin, char* _szEnd, char* _szStartKey,
                        char* _szEndKey, char* _szValue, int _len)
{
    int ret = -1;
    char* szValue;
    char* szReturn;

    memset(_szValue, 0, _len);
    //szValue = strstr(_szBegin, _szStartKey);
    szValue = _SafeFind(_szBegin, _szEnd, _szStartKey);
    if (szValue != NULL)
    {
        szValue += strlen(_szStartKey);
        //szReturn = strstr(szValue, _szEndKey);
        szReturn = _SafeFind(szValue, _szEnd, _szEndKey);
        if (szReturn != NULL)
        {
            CHECK(_len > szReturn - szValue, -1, "buffer is too small[%d bytes]\n", _len);
            memcpy(_szValue, szValue, szReturn - szValue);
            _szValue[szReturn - szValue] = 0;
            ret = 0;
        }
    }
    return ret;
}

static int _GetRtspValue(char* _szRtsp, char* _szKey, char* _szValue)
{
    int ret = -1;
    char* szValue;
    char* szReturn;

    szValue = strstr(_szRtsp, _szKey);
    if (szValue != NULL)
    {
        szValue += strlen(_szKey);
        szReturn = strstr(szValue, "\r\n");
        if (szReturn != NULL)
        {
            memcpy(_szValue, szValue, szReturn - szValue);
            _szValue[szReturn - szValue] = 0;
            ret = 0;
        }
    }
    return ret;
}

static int _GetStreamId(char* _szRtspReq, RTSP_SERVER_S* _pstRtspServer)
{
    int ret = -1;
    char szStreamId[12];
    int s32Id = -1;

    ret = _GetRtspValue(_szRtspReq, "streamid=", szStreamId);
    if (ret == 0)
    {
        if (!strncmp(szStreamId, "0", 1))
        {
            s32Id = 0;
        }
        else if (!strncmp(szStreamId, "1", 1))
        {
            s32Id = 1;
        }
    }
    else
    {
        WRN("not found streamid\n");
    }

    return s32Id;
}

static int __SessionIsValid(char* _szRtspReq, RTSP_SERVER_S* _pstRtspServer)
{
    int ret = -1;
    char szSession[32];

    ret = _GetRtspValue(_szRtspReq, "Session: ", szSession);
    if (ret == 0)
    {
        if (!strcmp(_pstRtspServer->szSession, szSession))
        {
            return 0;
        }
        else
        {
            WRN("Not my session[%s]: %s\n", _pstRtspServer->szSession, szSession);
        }
    }
    else
    {
        WRN("not found session\n");
    }

    return -1;
}

static char* __MakeSPS(unsigned char* _pu8Sps, unsigned int _u32SpsSize, unsigned char* _pu8Pps, unsigned int _u32PpsSize)
{
    int s32Ret, len;
    char* szRet;

    char* szFormat = "%s,%s";
    char* szSpsBase64 = ipc_base64_encode(_pu8Sps, _u32SpsSize, &len);
    char* szPpsBase64 = ipc_base64_encode(_pu8Pps, _u32PpsSize, &len);


    s32Ret = _CheckBuf(szFormat, szSpsBase64, szPpsBase64);
    szRet = (char*)mem_malloc(s32Ret + 1);
    sprintf(szRet, szFormat, szSpsBase64, szPpsBase64);

    free(szPpsBase64);
    free(szSpsBase64);

    return szRet;
}

static char* __MakePLI(unsigned char* _pu8Sps)
{
    int s32Ret;
    char* szRet;

    unsigned int u32Tmp1 = _pu8Sps[1];
    unsigned int u32Tmp2 = _pu8Sps[2];
    unsigned int u32Tmp3 = _pu8Sps[3];
    char* szFormat = "%02x%02x%02x";

    s32Ret = _CheckBuf(szFormat, u32Tmp1, u32Tmp2, u32Tmp3);
    szRet = (char*)mem_malloc(s32Ret + 1);
    sprintf(szRet, szFormat, u32Tmp1, u32Tmp2, u32Tmp3);

    return szRet;
}

static unsigned const samplingFrequencyTable[16] = {
  96000, 88200, 64000, 48000,
  44100, 32000, 24000, 22050,
  16000, 12000, 11025, 8000,
  7350, 0, 0, 0
};

static char* __MakeConfig()
{
	int s32Ret;
	char* szRet;

	int samplingFrequencyIndex=0;
	int audioSamplingFrequency=8000;
  	int audioNumChannels=1;
	unsigned char audioSpecificConfig[2];
	char* szFormat = "%02x%02x";

	int i;
  	for(i=0; i<16; i++)
  	{
		if(samplingFrequencyTable[i] == audioSamplingFrequency)
		{
			samplingFrequencyIndex=i;
			break;
		}
  	}
			   
 	u_int8_t const audioObjectType = 2; //profile + 1, profile=1
  	u_int8_t channel_configuration = audioNumChannels;
			   
  	audioSpecificConfig[0] = (audioObjectType<<3) | (samplingFrequencyIndex>>1);
  	audioSpecificConfig[1] = (samplingFrequencyIndex<<7) | (channel_configuration<<3);

	s32Ret = _CheckBuf(szFormat, audioSpecificConfig[0], audioSpecificConfig[1]);
    szRet = (char*)mem_malloc(s32Ret + 1);
    sprintf(szRet, szFormat, audioSpecificConfig[0], audioSpecificConfig[1]);
			   
  	DBG("audioSamplingFrequency=%d, audioNumChannels=%d, fConfigStr=%s\n", 
		 audioSamplingFrequency,
		 audioNumChannels,
		 szRet);
	
	return szRet;
}

#ifdef PCMA
static char* __MakeSdp(char* _szIp, unsigned char* _pu8Sps, unsigned int _u32SpsSize, unsigned char* _pu8Pps, unsigned int _u32PpsSize)
{
    char* szRet;
    int s32Ret;
    char* szFormat =
        "v=0\r\n"
        "o=- 0 0 IN IP4 127.0.0.1\r\n"
        "s=No Name\r\n"
        "c=In IP4 %s\r\n"
        "t=0 0\r\n"
        "a=tool:libavformat 56.15.102\r\n"
        "m=video 0 RTP/AVP 96\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1;profile-level-id=%s;sprop-parameter-sets=%s\r\n"
        "a=control:streamid=0\r\n"
        "m=audio 0 RTP/AVP 8\r\n"
        "a=rtpmap:8 PCMA/8000/1\r\n"
        "a=fmtp:8\r\n"
       	"a=ptime:40\r\n"
		"a=control:streamid=1\r\n"
        ;
    char* szSPS = __MakeSPS(_pu8Sps, _u32SpsSize, _pu8Pps, _u32PpsSize);
    char* szPLI = __MakePLI(_pu8Sps);

    s32Ret = _CheckBuf(szFormat,
                       _szIp,
                       szPLI,
                       szSPS);
    szRet = (char*)mem_malloc(s32Ret + 1);
    sprintf(szRet, szFormat,
            _szIp,
            szPLI,
            szSPS);

    mem_free(szPLI);
    mem_free(szSPS);
	
    return szRet;
}

#elif defined (AAC)
static char* __MakeSdp(char* _szIp, unsigned char* _pu8Sps, unsigned int _u32SpsSize, unsigned char* _pu8Pps, unsigned int _u32PpsSize)
{
    char* szRet;
    int s32Ret;
    char* szFormat =
        "v=0\r\n"
        "o=- 0 0 IN IP4 127.0.0.1\r\n"
        "s=No Name\r\n"
        "c=In IP4 %s\r\n"
        "t=0 0\r\n"
        "a=tool:libavformat 56.15.102\r\n"
        "m=video 0 RTP/AVP 96\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1;profile-level-id=%s;sprop-parameter-sets=%s\r\n"
        "a=control:streamid=0\r\n"
        "m=audio 0 RTP/AVP 97\r\n"
       	"a=rtpmap:97 mpeg4-generic/8000/1\r\n"
        "a=fmtp:97 streamtype=5;profile-level-id=15;mode=AAC-hbr;config=%s; sizeLength=13;indexLength=3;indexDeltaLength=3;profile=1\r\n"
		"a=control:streamid=1\r\n"
        ;
    char* szSPS = __MakeSPS(_pu8Sps, _u32SpsSize, _pu8Pps, _u32PpsSize);
    char* szPLI = __MakePLI(_pu8Sps);
	char* szConfig = __MakeConfig();

    s32Ret = _CheckBuf(szFormat,
                       _szIp,
                       szPLI,
                       szSPS,
                       szConfig);
    szRet = (char*)mem_malloc(s32Ret + 1);
    sprintf(szRet, szFormat,
            _szIp,
            szPLI,
            szSPS,
            szConfig);

    mem_free(szPLI);
    mem_free(szSPS);
	mem_free(szConfig);
	
    return szRet;
}

#else
static char* __MakeSdp(char* _szIp, unsigned char* _pu8Sps, unsigned int _u32SpsSize, unsigned char* _pu8Pps, unsigned int _u32PpsSize)
{
    char* szRet;
    int s32Ret;
    char* szFormat =
        "v=0\r\n"
        "o=- 0 0 IN IP4 127.0.0.1\r\n"
        "s=No Name\r\n"
        "c=In IP4 %s\r\n"
        "t=0 0\r\n"
        "a=tool:libavformat 56.15.102\r\n"
        "m=video 0 RTP/AVP 96\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1;profile-level-id=%s;sprop-parameter-sets=%s\r\n"
        "a=control:streamid=0\r\n"
        ;
    char* szSPS = __MakeSPS(_pu8Sps, _u32SpsSize, _pu8Pps, _u32PpsSize);
    char* szPLI = __MakePLI(_pu8Sps);

    s32Ret = _CheckBuf(szFormat,
                       _szIp,
                       szPLI,
                       szSPS);
    szRet = (char*)mem_malloc(s32Ret + 1);
    sprintf(szRet, szFormat,
            _szIp,
            szPLI,
            szSPS);

    mem_free(szPLI);
    mem_free(szSPS);

    return szRet;
}
#endif

static int _InitResource(RTSP_SERVER_S* _pstRtspServer)
{
    if (NULL == _pstRtspServer->hndReader)
    {
        if (_pstRtspServer->stream_id)
        {
            do
            {
                _pstRtspServer->hndReader = frame_pool_register(gHndMainFramePool, 0, SAL_VIDEO_STREAM);
                usleep(20*1000);
            }
            while (_pstRtspServer->hndReader == NULL);	
        }
        else
        {
            do
            {
                _pstRtspServer->hndReader = frame_pool_register(gHndSubFramePool, 0, SAL_VIDEO_STREAM);
                usleep(20*1000);
            }
            while (_pstRtspServer->hndReader == NULL);			
        }

        char* data = NULL;
        int len = 0;
        int ret = frame_pool_sps_get(_pstRtspServer->hndReader, &data, &len);
        CHECK(ret == 0, -1, "Error with: %#x\n", ret);

        CHECK(len < sizeof(_pstRtspServer->au8Sps), -1, "Error with: %#x\n", -1);
        memcpy(_pstRtspServer->au8Sps, data, len);
        _pstRtspServer->u32SpsSize = len;

        ret = frame_pool_pps_get(_pstRtspServer->hndReader, &data, &len);
        CHECK(ret == 0, -1, "Error with: %#x\n", ret);

        CHECK(len < sizeof(_pstRtspServer->au8Pps), -1, "Error with: %#x\n", -1);
        memcpy(_pstRtspServer->au8Pps, data, len);
        _pstRtspServer->u32PpsSize = len;
    }

#if defined (PCMA) || defined (AAC)
	if (NULL == _pstRtspServer->hndAReader)
	{	
		do
		{
			_pstRtspServer->hndAReader = frame_pool_register(gHndAudioFramePool, 0, SAL_AUDIO_STREAM);
			usleep(20*1000);
		}
		while (_pstRtspServer->hndAReader == NULL);	
	}
#endif	

    return 0;
}

static int _SplitName(char* _szRtspUrl, char* _szRet, int _Len)
{
    int ret = -1;
    memset(_szRet, 0, _Len);

    char* Found = rindex(_szRtspUrl, '/');
    if (NULL != Found)
    {
        int NeedLen = _szRtspUrl + strlen(_szRtspUrl) - Found - strlen("/");
        CHECK(_Len > NeedLen, -1, "buffer is too small\n");
        strncpy(_szRet, Found + strlen("/"), NeedLen);
        ret = 0;
    }
    return ret;
}

static int _CheckUrl(char* _szRtspReq, RTSP_SERVER_S* _pstRtspServer, char* _szKey)
{
    int ret = -1;
    char szName[64];
    memset(szName, 0, sizeof(szName));

    ret = _GetKeyValue(_szRtspReq, _szRtspReq + strlen(_szRtspReq), _szKey, " RTSP/", _pstRtspServer->szUrl, sizeof(_pstRtspServer->szUrl));
    CHECK(ret == 0, -1, "url is invalid[%s]\n", _pstRtspServer->szUrl);
    DBG("url: %s\n", _pstRtspServer->szUrl);

    ret = _SplitName(_pstRtspServer->szUrl, szName, sizeof(szName));
    CHECK(ret == 0, -1, "url is invalid[%s]\n", _pstRtspServer->szUrl);

    if (!strcmp(szName, "MainStream"))
    {
    	_pstRtspServer->stream_id = MAIN_STREAM;
        ret = _InitResource(_pstRtspServer);
        CHECK(ret == 0, -1, "_InitResource failed.\n");
    }
    else if (!strcmp(szName, "SubStream"))
    {
    	_pstRtspServer->stream_id = SUB_STREAM;
        ret = _InitResource(_pstRtspServer);
        CHECK(ret == 0, -1, "_InitResource failed.\n");
    }
    else
    {
        CHECK(0, -1, "url is invalid[%s]\n", _pstRtspServer->szUrl);
    }

    return 0;
}

static int __RecvOptions(char* _szRtspReq, RTSP_SERVER_S* _pstRtspServer)
{
    int ret = -1;
    char szCseq[16];

    ret = _CheckUrl(_szRtspReq, _pstRtspServer, "OPTIONS ");
    CHECK(ret == 0, -1, "_CheckUrl failed.\n");

    ret = _GetRtspValue(_szRtspReq, "CSeq: ", szCseq);
    if (ret != 0)
    {
        ret = _GetRtspValue(_szRtspReq, "Cseq: ", szCseq);
    }
    CHECK(ret == 0, -1, "ret %d, _szRtspReq %s\n", ret, _szRtspReq);

    char* szTemplate =
        "RTSP/1.0 200 OK\r\n"
        "Server: myServer\r\n"
        "CSeq: %s\r\n"
        /*"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n"*/
        "Public: DESCRIBE, SETUP, PLAY, OPTIONS, TEARDOWN\r\n"
        "\r\n";

    void* pData;
    unsigned int u32DataLen;

    u32DataLen = _CheckBuf(szTemplate, szCseq);
    pData = mem_malloc(u32DataLen + 1);
    sprintf(pData, szTemplate, szCseq);
    DBG("send: \n%s\n", (char*)pData);

    ret = select_send(_pstRtspServer->hndSocket, pData, u32DataLen);
    CHECK(ret == 0, -1, "Error with %#x\n", ret);

    mem_free(pData);

    return 0;
}

static int __RecvDescribe(char* _szRtspReq, RTSP_SERVER_S* _pstRtspServer)
{
    int ret = -1;
    char szCseq[16];

    ret = _CheckUrl(_szRtspReq, _pstRtspServer, "DESCRIBE ");
    CHECK(ret == 0, -1, "_CheckUrl failed.\n");

    ret = _GetRtspValue(_szRtspReq, "CSeq: ", szCseq);
    if (ret != 0)
    {
        ret = _GetRtspValue(_szRtspReq, "Cseq: ", szCseq);
    }
    CHECK(ret == 0, -1, "ret %d, _szRtspReq %s\n", ret, _szRtspReq);

    char* szSdpContent = __MakeSdp("0.0.0.0", _pstRtspServer->au8Sps, _pstRtspServer->u32SpsSize,
                                       _pstRtspServer->au8Pps, _pstRtspServer->u32PpsSize);
    unsigned int u32ContentLength = strlen(szSdpContent);

    char* szTemplate =
        "RTSP/1.0 200 OK\r\n"
        "Server: myServer\r\n"
        "Cseq: %s\r\n"
        "Content-length: %u\r\n"
        "Content-Type: application/sdp\r\n"
        "\r\n"
        "%s";

    void* pData;
    unsigned int u32DataLen;

    u32DataLen = _CheckBuf(szTemplate, szCseq, u32ContentLength, szSdpContent);
    pData = mem_malloc(u32DataLen + 1);
    sprintf(pData, szTemplate, szCseq, u32ContentLength, szSdpContent);
    DBG("send: \n%s\n", (char*)pData);

    ret = select_send(_pstRtspServer->hndSocket, pData, u32DataLen);
    CHECK(ret == 0, -1, "Error with %#x\n", ret);

    mem_free(szSdpContent);
    mem_free(pData);

    return 0;
}

static int __RecvSetup(char* _szRtspReq, RTSP_SERVER_S* _pstRtspServer)
{
    int ret = -1;
    char szCseq[16];
    char szTransport[128];
    char szRespStatus[32];

    ret = _GetRtspValue(_szRtspReq, "CSeq: ", szCseq);
    if (ret != 0)
    {
        ret = _GetRtspValue(_szRtspReq, "Cseq: ", szCseq);
    }
    CHECK(ret == 0, -1, "ret Cseq %d, _szRtspReq %s\n", ret, _szRtspReq);

    ret = _GetRtspValue(_szRtspReq, "Transport: ", szTransport);
    CHECK(ret == 0, -1, "ret Transport %d, _szRtspReq %s\n", ret, _szRtspReq);

    if (strstr(szTransport, "TCP"))
    {
        _pstRtspServer->enTranType = TRANS_TYPE_TCP;
    }
    else
    {
        _pstRtspServer->enTranType = TRANS_TYPE_UDP;
    }

    int s32StreamId = _GetStreamId(_szRtspReq, _pstRtspServer);
    if (0 == s32StreamId)
    {
    	DBG("bVEnable\n");
        _pstRtspServer->bVEnable = 1;
    }
    else if (1 == s32StreamId)
    {
    	DBG("bAEnable\n");
        _pstRtspServer->bAEnable = 1;
    }
    else
    {
        CHECK(0, -1, "Error with s32StreamId %d\n", s32StreamId);
    }

    char* szTemplate =
        "RTSP/1.0 %s\r\n"
        "Server: myServer\r\n"
        "Cseq: %s\r\n"
        "Session: %s\r\n"
        "Transport: RTP/AVP/TCP;unicast;interleaved=%u-%u;mode=play\r\n"
        "\r\n";

    void* pData;
    unsigned int u32DataLen;

    if (TRANS_TYPE_UDP == _pstRtspServer->enTranType || -1 == s32StreamId)
    {
        strcpy(szRespStatus, "406 Not Acceptable");
        _pstRtspServer->enStatus = RTSP_SERVER_STATUS_ERROR;
    }
    else if (TRANS_TYPE_TCP == _pstRtspServer->enTranType)
    {
        strcpy(szRespStatus, "200 OK");
    }

    u32DataLen = _CheckBuf(szTemplate, szRespStatus, szCseq, _pstRtspServer->szSession, _pstRtspServer->u32SetupCount, _pstRtspServer->u32SetupCount + 1);
    pData = mem_malloc(u32DataLen + 1);
    sprintf(pData, szTemplate, szRespStatus, szCseq, _pstRtspServer->szSession, _pstRtspServer->u32SetupCount, _pstRtspServer->u32SetupCount + 1);
    DBG("send: \n%s\n", (char*)pData);

    _pstRtspServer->u32SetupCount += 2;

    ret = select_send(_pstRtspServer->hndSocket, pData, u32DataLen);
    CHECK(ret == 0, -1, "Error with %#x\n", ret);

    mem_free(pData);

    return 0;
}

static int __RecvPlay(char* _szRtspReq, RTSP_SERVER_S* _pstRtspServer)
{
    int ret = -1;
    char szCseq[16];

    ret = _GetRtspValue(_szRtspReq, "CSeq: ", szCseq);
    if (ret != 0)
    {
        ret = _GetRtspValue(_szRtspReq, "Cseq: ", szCseq);
    }
    CHECK(ret == 0, -1, "ret %d, _szRtspReq %s\n", ret, _szRtspReq);

    CHECK(!__SessionIsValid(_szRtspReq, _pstRtspServer), 0,
              "session is invalid, _szRtspReq %s\n", _szRtspReq);

    char* szTemplate =
        "RTSP/1.0 200 OK\r\n"
        "Server: myServer\r\n"
        "Cseq: %s\r\n"
        "Session: %s\r\n"
        "Range: npt=now-\r\n"
        /*"RTP-Info: url=rtsp://192.168.34.203:554/269ec94d-fe66-4b0d-b75e-d321f1a90aff.sdp/trackID=0,url=rtsp://192.168.34.203:554/269ec94d-fe66-4b0d-b75e-d321f1a90aff.sdp/trackID=1\r\n"*/
        "\r\n";

    void* pData;
    unsigned int u32DataLen;

    u32DataLen = _CheckBuf(szTemplate, szCseq, _pstRtspServer->szSession);
    pData = mem_malloc(u32DataLen + 1);
    sprintf(pData, szTemplate, szCseq, _pstRtspServer->szSession);
    DBG("send: \n%s\n", (char*)pData);

    _pstRtspServer->enStatus = RTSP_SERVER_STATUS_RTP;

    ret = select_send(_pstRtspServer->hndSocket, pData, u32DataLen);
    CHECK(ret == 0, -1, "Error with %#x\n", ret);

    mem_free(pData);

    return 0;
}

static int __RecvTearDown(char* _szRtspReq, RTSP_SERVER_S* _pstRtspServer)
{
    int ret = -1;
    char szCseq[16];

    ret = _GetRtspValue(_szRtspReq, "CSeq: ", szCseq);
    if (ret != 0)
    {
        ret = _GetRtspValue(_szRtspReq, "Cseq: ", szCseq);
    }
    CHECK(ret == 0, -1, "ret %d, _szRtspReq %s\n", ret, _szRtspReq);

    CHECK(!__SessionIsValid(_szRtspReq, _pstRtspServer), 0,
              "session is invalid, _szRtspReq %s\n", _szRtspReq);

    char* szTemplate =
        "RTSP/1.0 200 OK\r\n"
        "Server: myServer\r\n"
        "Cseq: %s\r\n"
        "Session: %s\r\n"
        "Range: npt=now-\r\n"
        "Connection: Close\r\n"
        /*"RTP-Info: url=rtsp://192.168.34.203:554/269ec94d-fe66-4b0d-b75e-d321f1a90aff.sdp/trackID=0,url=rtsp://192.168.34.203:554/269ec94d-fe66-4b0d-b75e-d321f1a90aff.sdp/trackID=1\r\n"*/
        "\r\n";

    void* pData;
    unsigned int u32DataLen;

    u32DataLen = _CheckBuf(szTemplate, szCseq, _pstRtspServer->szSession);
    pData = mem_malloc(u32DataLen + 1);
    sprintf(pData, szTemplate, szCseq, _pstRtspServer->szSession);
    DBG("send: \n%s\n", (char*)pData);

    _pstRtspServer->enStatus = RTSP_SERVER_STATUS_TEARDOWN;

    ret = select_send(_pstRtspServer->hndSocket, pData, u32DataLen);
    CHECK(ret == 0, -1, "Error with %#x\n", ret);

    mem_free(pData);

    return 0;
}

static int __SendBadRequest(RTSP_SERVER_S* _pstRtspServer)
{
    int ret = -1;

    _pstRtspServer->enStatus = RTSP_SERVER_STATUS_ERROR;
    char* szTemplate =
        "RTSP/1.0 400 Bad Request\r\n"
        "Server: myServer\r\n"
        "Connection: Close\r\n"
        /*"RTP-Info: url=rtsp://192.168.34.203:554/269ec94d-fe66-4b0d-b75e-d321f1a90aff.sdp/trackID=0,url=rtsp://192.168.34.203:554/269ec94d-fe66-4b0d-b75e-d321f1a90aff.sdp/trackID=1\r\n"*/
        "\r\n";

    void* pData;
    unsigned int u32DataLen;

    u32DataLen = _CheckBuf(szTemplate);
    pData = mem_malloc(u32DataLen + 1);
    sprintf(pData, szTemplate);
    DBG("send: \n%s\n", (char*)pData);

    ret = select_send(_pstRtspServer->hndSocket, pData, u32DataLen);
    CHECK(ret == 0, -1, "Error with %#x\n", ret);

    mem_free(pData);

    return 0;
}

static int __RecvRtcp(char* _szRtspReq, RTSP_SERVER_S* _pstRtspServer)
{
    // todo: parse rtcp
    //DBG("Recv: RTCP packet");

    return 0;
}

static int _RecvRtspReq(char* _szRtspReq, RTSP_SERVER_S* _pstRtspServer)
{
    if (memcmp(_szRtspReq, "$", strlen("$")))
    {
        DBG("recv: \n%s\n", _szRtspReq);
    }

    int ret = -1;
    if (memcmp(_szRtspReq, "OPTIONS", strlen("OPTIONS")) == 0)
    {
        ret = __RecvOptions(_szRtspReq, _pstRtspServer);
    }
    else if (memcmp(_szRtspReq, "DESCRIBE", strlen("DESCRIBE")) == 0)
    {
        ret = __RecvDescribe(_szRtspReq, _pstRtspServer);
    }
    else if (memcmp(_szRtspReq, "SETUP", strlen("SETUP")) == 0)
    {
        ret = __RecvSetup(_szRtspReq, _pstRtspServer);
    }
    else if (memcmp(_szRtspReq, "PLAY", strlen("PLAY")) == 0)
    {
        ret = __RecvPlay(_szRtspReq, _pstRtspServer);
    }
    else if (memcmp(_szRtspReq, "TEARDOWN", strlen("TEARDOWN")) == 0)
    {
        ret = __RecvTearDown(_szRtspReq, _pstRtspServer);
    }
    else if (memcmp(_szRtspReq, "$", strlen("$")) == 0)
    {
        ret = __RecvRtcp(_szRtspReq, _pstRtspServer);
    }

    if (ret != 0)
    {
        ret = __SendBadRequest(_pstRtspServer);
        CHECK(ret == 0, -1, "Error with %#x\n", ret);
    }

    return 0;
}

static int _SendAFrame(frame_info_s* _pstInfo, RTSP_SERVER_S* _pstRtspServer)
{
    int ret = -1;
    void* pData;
    unsigned int u32DataLen;
	
	if (_pstRtspServer->u64LastATimeStamp == 0)
	 {
		 _pstRtspServer->u64LastATimeStamp = _pstInfo->timestamp;
	 }
	
	 _pstRtspServer->u32ATimeStamp = (unsigned int)(_pstInfo->timestamp - _pstRtspServer->u64LastATimeStamp) * 8;
	
    unsigned char* pu8Frame;
    unsigned int u32FrameSize;
    unsigned char* pu8Tmp = _pstInfo->data;
    unsigned int u32TmpSize = _pstInfo->len;
	
	pu8Frame = pu8Tmp;
	u32FrameSize = u32TmpSize;

	RTP_SPLIT_S* pstRetARtpSplit = (RTP_SPLIT_S*)mem_malloc(sizeof(RTP_SPLIT_S));
	
#ifdef PCMA	
	ret = rtp_pcm_alloc(pu8Frame, u32FrameSize, &_pstRtspServer->u16ASeqNum,
						_pstRtspServer->u32ATimeStamp, pstRetARtpSplit);
	GOTO(ret == 0, _EXIT, "Error with %#x\n", ret); 	

#elif defined (AAC)
	ret = rtp_aac_alloc(pu8Frame, u32FrameSize, &_pstRtspServer->u16ASeqNum,
							 _pstRtspServer->u32ATimeStamp, pstRetARtpSplit);
	GOTO(ret == 0, _EXIT, "Error with: %#x\n", ret);
#endif

	u32DataLen = pstRetARtpSplit->u32BufSize;
	pData = pstRetARtpSplit->pu8Buf;
	
	ret = select_send(_pstRtspServer->hndSocket, pData, u32DataLen);
	GOTO(ret == 0, _EXIT, "Error with %#x\n", ret);
	
	ret = rtp_free(pstRetARtpSplit);
	GOTO(ret == 0, _EXIT, "Error with %#x\n", ret);
	
	mem_free(pstRetARtpSplit);

    return 0;

_EXIT:
	mem_free(pstRetARtpSplit);
	return -1;
	
}


static int _SendVFrame(frame_info_s* _pstInfo, RTSP_SERVER_S* _pstRtspServer)
{
    int ret = -1;
    void* pData;
    unsigned int u32DataLen;

    if (_pstRtspServer->u64LastVTimeStamp == 0)
    {
        _pstRtspServer->u64LastVTimeStamp = _pstInfo->timestamp;
    }

    _pstRtspServer->u32VTimeStamp = (unsigned int)(_pstInfo->timestamp - _pstRtspServer->u64LastVTimeStamp) * 90;

    unsigned char* pu8Frame;
    unsigned int u32FrameSize;
    unsigned char* pu8Tmp = _pstInfo->data;
    unsigned int u32TmpSize = _pstInfo->len;
    int offset;
    do
    {
        offset = rtp_h264_split(pu8Tmp, u32TmpSize);
        if (offset != -1)
        {
            pu8Frame = pu8Tmp;
            u32FrameSize = offset;

            pu8Tmp += offset;
            u32TmpSize -= offset;
        }
        else
        {
            pu8Frame = pu8Tmp;
            u32FrameSize = u32TmpSize;
        }

        RTP_SPLIT_S* pstRetVRtpSplit = (RTP_SPLIT_S*)mem_malloc(sizeof(RTP_SPLIT_S));
        ret = rtp_h264_alloc(pu8Frame, u32FrameSize, &_pstRtspServer->u16VSeqNum,
                            _pstRtspServer->u32VTimeStamp, pstRetVRtpSplit);
        CHECK(ret == 0, -1, "Error with %#x\n", ret);

        u32DataLen = pstRetVRtpSplit->u32BufSize;
        pData = pstRetVRtpSplit->pu8Buf;

        ret = select_send(_pstRtspServer->hndSocket, pData, u32DataLen);
        CHECK(ret == 0, -1, "Error with %#x\n", ret);

        ret = rtp_free(pstRetVRtpSplit);
        CHECK(ret == 0, -1, "Error with %#x\n", ret);

        mem_free(pstRetVRtpSplit);
    }
    while (offset != -1);

    return 0;
}


void* rtsp_process(void* _pstSession)
{
    prctl(PR_SET_NAME, __FUNCTION__);
    int ret = -1;
    client_s* pclient = (client_s*)_pstSession;

    struct timeval abstime = {0, 0};
    util_time_abs(&abstime);
    srand(abstime.tv_usec);

    unsigned int i = 0;
    RTSP_SERVER_S stRtspServer;
    memset(&stRtspServer, 0, sizeof(RTSP_SERVER_S));

    stRtspServer.enStatus = RTSP_SERVER_STATUS_RTSP;
    for(i = 0; i < 16; i++)
    {
        sprintf(stRtspServer.szSession + strlen(stRtspServer.szSession), "%d", rand() % 10);
    }

    unsigned int u32ExpectTimeout = 0;
    unsigned int u32Timeout = 0;

    stRtspServer.hndSocket = select_init(_RtspOrRtcpComplete, 4*1024, pclient->fd);
    CHECK(stRtspServer.hndSocket, NULL, "Error with: %#x\n", stRtspServer.hndSocket);

    ret = select_debug(stRtspServer.hndSocket, 0);
    CHECK(ret == 0, NULL, "Error with: %#x\n", ret);

    char* data = NULL;
    int len = 0;
    while (pclient->running)
    {
		switch(stRtspServer.enStatus)
		{
			case RTSP_SERVER_STATUS_RTSP:
				{
					u32ExpectTimeout = GENERAL_RWTIMEOUT;
            		u32Timeout = select_rtimeout(stRtspServer.hndSocket);
            		if (u32Timeout > u32ExpectTimeout)
            		{
                		WRN("Read  Timeout %u, expect %u\n", u32Timeout, u32ExpectTimeout);
                		break;
            		}

            		u32ExpectTimeout = GENERAL_RWTIMEOUT;
            		u32Timeout = select_wtimeout(stRtspServer.hndSocket);
            		if (u32Timeout > u32ExpectTimeout)
            		{
                		WRN("Write Timeout %u, expect %u\n", u32Timeout, u32ExpectTimeout);
                		break;
            		}
				}
				break;
			case RTSP_SERVER_STATUS_RTP:
				{	
					if(stRtspServer.bVEnable)
					{
						frame_info_s* video_frame = frame_pool_get(stRtspServer.hndReader);
        				if (video_frame)
        				{
        					ret = _SendVFrame(video_frame, &stRtspServer);
            				GOTO(ret == 0, _EXIT, "Error with: %#x\n", ret);

            				ret = frame_pool_release(stRtspServer.hndReader, video_frame);
            				GOTO(ret == 0, _EXIT, "Error with: %#x\n", ret);
        				}
						else
						{
							usleep(20*1000);
						}
					}
								
					if(stRtspServer.bAEnable)
					{
						frame_info_s* audio_frame = frame_pool_get(stRtspServer.hndAReader);
        				if (audio_frame)
        				{
        					ret = _SendAFrame(audio_frame, &stRtspServer);
            				GOTO(ret == 0, _EXIT, "Error with: %#x\n", ret);

            				ret = frame_pool_release(stRtspServer.hndAReader, audio_frame);
            				GOTO(ret == 0, _EXIT, "Error with: %#x\n", ret);
        				}
					}
				}
				break;
			case RTSP_SERVER_STATUS_ERROR:
				{
					WRN("EXIT: RTSP ERROR\n");
            		while (pclient->running && !select_wlist_empty(stRtspServer.hndSocket))
            		{
                		ret = select_rw(stRtspServer.hndSocket);
                		GOTO(ret == 0, _EXIT, "Error with: %#x\n", ret);
            		}
				}
				break;
			default:
				{
					DBG("EXIT: RTSP TEARDOWN");
					while (pclient->running && !select_wlist_empty(stRtspServer.hndSocket))
            		{
                		ret = select_rw(stRtspServer.hndSocket);
                		GOTO(ret == 0, _EXIT, "Error with: %#x\n", ret);
            		}
				}
				break;
		}
		
        do
        {
        	data = NULL;
            len = 0;
            ret = select_recv_get(stRtspServer.hndSocket, (void**)&data, &len);
            GOTO(ret == 0, _EXIT, "Error with: %#x\n", ret);
            if (data != NULL)
           	{
            	GOTO((((unsigned int)data)%4) == 0, _EXIT, "pData %p is not align\n", data);

                char szRtspReq[1024];
                GOTO(len < sizeof(szRtspReq), _EXIT, "u32DataLen %u is too small, expect %u\n", len, sizeof(szRtspReq));
                memcpy(szRtspReq, data, len);
                szRtspReq[len - 1] = 0;
                ret = _RecvRtspReq(szRtspReq, &stRtspServer);
                GOTO(ret == 0, _EXIT, "Error with: %#x\n", ret);

                ret = select_recv_release(stRtspServer.hndSocket, (void**)&data, &len);
                GOTO(ret == 0, _EXIT, "Error with: %#x\n", ret);
            }
       	}
        while (data != NULL && pclient->running);

        ret = select_rw(stRtspServer.hndSocket);
        GOTO(ret == 0, _EXIT, "Error with: %#x\n", ret);

    }

_EXIT:
    DBG("rtsp client[%d] ready to exit\n", pclient->fd);

    if (stRtspServer.hndSocket)
    {
        ret = select_destroy(stRtspServer.hndSocket);
        CHECK(ret == 0, NULL, "Error with %#x\n", ret);
        stRtspServer.hndSocket = NULL;
    }

    pclient->running = 0;
    DBG("rtsp client[%d] exit done.\n", pclient->fd);

    return NULL;
}


