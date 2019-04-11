#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <malloc.h>

#include "json/json.h"
#include "curl/curl.h"
#include "openssl/rsa.h"
#include "openssl/pem.h"
#include "openssl/err.h"

#include "fw_curl.h"
#include "fw_download.h"
#include "ipc_util.h"

#define CURL_WRAPPER_VALID_MAGIC (0x74246F94)
static int g_bCurlWrapperHttpPostImgShowDebug = 0;
static int g_bCurlWrapperHttpPostShowDebug = 0;
static int g_bCurlWrapperHttpGetShowDebug = 0;

typedef struct curl_wrapper_s
{
    unsigned int u32ValidMagie;

    CURL* pstCurl;
    CURLcode enCurlRet;

    pthread_t tPid;
    int bRunning;
    CURL_RESULT_E enResult;

    char szUrl[256];
    char szUsrPwd[32];

    double d32LastTotal;
    double d32LastNow;
    struct timeval stLastChangeTimev;
    int timeout; //超时时间（毫秒）
    FILE* pFile;

    unsigned char* pu8PostSend;
    unsigned int u32PostSendTotal;
    unsigned int u32PostSendCurr;

    unsigned char au8PostRecv[20*1024];
    unsigned int u32PostRecvTotal;
    unsigned int u32PostRecvCurr;
    unsigned int u32PostRecvHeaderTotal;
    unsigned int u32PostRecvBodyTotal;
	unsigned char au8PostHead[2*1024];
	
    struct curl_httppost *formpost;
    struct curl_httppost *lastptr;
    struct curl_slist *headerlist;

    //新增下载到内存，不写文件
    unsigned char* pu8GetRecv; //malloc 申请
    unsigned int u32Size;      //buffer 大小
    unsigned int u32Offset;    //可写的offset
}
curl_wrapper_s;

static size_t _Onrecv(void* buffer, size_t size, size_t nmemb, curl_wrapper_s* pstCurlWrapper)
{
    int s32Ret;

    s32Ret = fwrite(buffer, size, nmemb, pstCurlWrapper->pFile);
    if(s32Ret == nmemb)
    {
        fflush(pstCurlWrapper->pFile);
        return s32Ret;
    }
    else
    {
		printf("[%s:%d] Ent!\n", __func__, __LINE__);
        pstCurlWrapper->bRunning = 0;
        pstCurlWrapper->enResult = CURL_RESULT_WRITE_ERROR;
        return -1;
    }
    return -1;
}

static size_t _OnRecv2mem(void* buffer, size_t size, size_t nmemb, curl_wrapper_s* pstCurlWrapper)
{
    //初始化大小
    if (pstCurlWrapper->pu8GetRecv == NULL)
    {
        pstCurlWrapper->pu8GetRecv = malloc(size*nmemb);
        pstCurlWrapper->u32Size = size*nmemb;
        pstCurlWrapper->u32Offset = 0;
    }

    //已经知道总共的大小，那么申请整个文件的大小
    if (pstCurlWrapper->pu8GetRecv && pstCurlWrapper->d32LastTotal > 0
        && pstCurlWrapper->d32LastTotal != pstCurlWrapper->u32Size)
    {
        int new_size = pstCurlWrapper->d32LastTotal;
        pstCurlWrapper->pu8GetRecv = realloc(pstCurlWrapper->pu8GetRecv, new_size);
        pstCurlWrapper->u32Size = new_size;
    }

    //还不知道总共的大小，只能来一个回调就申请多一点内存去装
    if (pstCurlWrapper->pu8GetRecv
        && (pstCurlWrapper->u32Size-pstCurlWrapper->u32Offset) < (size*nmemb))
    {
        int new_size = pstCurlWrapper->u32Size + (size*nmemb);
        pstCurlWrapper->pu8GetRecv = realloc(pstCurlWrapper->pu8GetRecv, new_size);
        pstCurlWrapper->u32Size = new_size;
    }

    if (pstCurlWrapper->pu8GetRecv
        && (pstCurlWrapper->u32Size-pstCurlWrapper->u32Offset) >= (size*nmemb))
    {
        memcpy(pstCurlWrapper->pu8GetRecv+pstCurlWrapper->u32Offset, buffer, size*nmemb);
        pstCurlWrapper->u32Offset += size*nmemb;

        return size*nmemb;
    }
    else
    {
        pstCurlWrapper->bRunning = 0;
        pstCurlWrapper->enResult = CURL_RESULT_WRITE_ERROR;
        return -1;
    }
    return -1;
}

static size_t _Onsend(void *buffer, size_t size, size_t nmemb, curl_wrapper_s* pstCurlWrapper)
{
    int s32Ret;

    s32Ret = fread(buffer, size, nmemb, pstCurlWrapper->pFile);
    if(s32Ret >= 0)
    {
        return s32Ret;
    }
    else
    {
        pstCurlWrapper->bRunning = 0;
        pstCurlWrapper->enResult = CURL_RESULT_WRITE_ERROR;
        return -1;
    }
    return -1;
}

static size_t _OnDlProgress(curl_wrapper_s* pstCurlWrapper, double dltotal, double dlnow, double ultotal, double ulnow)
{
    if(pstCurlWrapper->bRunning == 0 && dlnow != dltotal)
    {
        pstCurlWrapper->enResult = CURL_RESULT_MANUAL_STOP;
        return -1;
    }

    size_t tRet = 0;
    if(pstCurlWrapper->stLastChangeTimev.tv_sec == 0 && pstCurlWrapper->stLastChangeTimev.tv_usec == 0)
    {
         util_time_abs(&pstCurlWrapper->stLastChangeTimev);
    }

    struct timeval stNow = {0, 0};
    util_time_abs(&stNow);
    if(pstCurlWrapper->d32LastNow == dlnow && pstCurlWrapper->d32LastTotal == dltotal)
    {
        int pass_time = util_time_pass(&pstCurlWrapper->stLastChangeTimev);
        if (pass_time > pstCurlWrapper->timeout)
        {
			printf("[%s:%d] Ent!\n", __func__, __LINE__);
            pstCurlWrapper->bRunning = 0;
            pstCurlWrapper->enResult = CURL_RESULT_READ_TIMEOUT;
            tRet = -1;
        }
    }
    else
    {
        pstCurlWrapper->d32LastNow = dlnow;
        pstCurlWrapper->d32LastTotal = dltotal;
        pstCurlWrapper->stLastChangeTimev = stNow;
    }

    return tRet;
}

static size_t _OnUlProgress(curl_wrapper_s* pstCurlWrapper, double dltotal, double dlnow, double ultotal, double ulnow)
{
    if(pstCurlWrapper->bRunning == 0 && ulnow != ultotal)
    {
        pstCurlWrapper->enResult = CURL_RESULT_MANUAL_STOP;
        return -1;
    }

    size_t tRet = 0;
    if(pstCurlWrapper->stLastChangeTimev.tv_sec == 0 && pstCurlWrapper->stLastChangeTimev.tv_usec == 0)
    {
        util_time_abs(&pstCurlWrapper->stLastChangeTimev);
    }

    struct timeval stNow = {0, 0};
    util_time_abs(&stNow);
    if(pstCurlWrapper->d32LastNow == ulnow && pstCurlWrapper->d32LastTotal == ultotal)
    {
        int pass_time = util_time_pass(&pstCurlWrapper->stLastChangeTimev);
        if (pass_time > 180*1000)
        {
            pstCurlWrapper->bRunning = 0;
            pstCurlWrapper->enResult = CURL_RESULT_READ_TIMEOUT;
            tRet = -1;
        }
    }
    else
    {
        pstCurlWrapper->d32LastNow = ulnow;
        pstCurlWrapper->d32LastTotal = ultotal;
        pstCurlWrapper->stLastChangeTimev = stNow;
    }

    return tRet;
}

static void* _Proc(void* _pArgs)
{
    prctl(PR_SET_NAME, __FUNCTION__);
    curl_wrapper_s* pstCurlWrapper = (curl_wrapper_s*)_pArgs;

    curl_easy_perform(pstCurlWrapper->pstCurl);
	
	CURLcode code = curl_easy_getinfo(pstCurlWrapper->pstCurl, CURLINFO_RESPONSE_CODE, &pstCurlWrapper->enCurlRet);
	LOG("CurlCode :%d\n", pstCurlWrapper->enCurlRet);
	if((code == CURLE_OK) && (pstCurlWrapper->enCurlRet == 200))
	{
		pstCurlWrapper->enResult = CURL_RESULT_OK;
	}
	else
	{
		pstCurlWrapper->enResult = CURL_RESULT_UNKNOWN;
	}

    pstCurlWrapper->bRunning = 0;

    return NULL;
}

static size_t _OnHttpPostHead(void* buffer, size_t size, size_t nmemb, curl_wrapper_s* pstCurlWrapper)
{
    size_t s32NeedCopy = size * nmemb;
    unsigned int u32Left = pstCurlWrapper->u32PostRecvTotal - pstCurlWrapper->u32PostRecvCurr;

    memcpy(pstCurlWrapper->au8PostRecv + pstCurlWrapper->u32PostRecvCurr, buffer, s32NeedCopy);
    pstCurlWrapper->u32PostRecvCurr += s32NeedCopy;
    pstCurlWrapper->u32PostRecvHeaderTotal += s32NeedCopy;

    return s32NeedCopy;
}

static size_t _OnHttpPostRecv(void* buffer, size_t size, size_t nmemb, curl_wrapper_s* pstCurlWrapper)
{
    size_t s32NeedCopy = size * nmemb;
    unsigned int u32Left = pstCurlWrapper->u32PostRecvTotal - pstCurlWrapper->u32PostRecvCurr;
	
    memcpy(pstCurlWrapper->au8PostRecv + pstCurlWrapper->u32PostRecvCurr, buffer, s32NeedCopy);
    pstCurlWrapper->u32PostRecvCurr += s32NeedCopy;
    pstCurlWrapper->u32PostRecvBodyTotal += s32NeedCopy;

    return s32NeedCopy;
}

static size_t _OnHttpPostProgress(curl_wrapper_s* pstCurlWrapper, double dltotal, double dlnow, double ultotal, double ulnow)
{
    if(pstCurlWrapper->bRunning == 0)
    {
        pstCurlWrapper->enResult = CURL_RESULT_MANUAL_STOP;
        return -1;
    }

    size_t tRet = 0;
    if(pstCurlWrapper->stLastChangeTimev.tv_sec == 0 && pstCurlWrapper->stLastChangeTimev.tv_usec == 0)
    {
        util_time_abs(&pstCurlWrapper->stLastChangeTimev);
    }

    struct timeval stNow = {0, 0};
    util_time_abs(&stNow);
    if(pstCurlWrapper->d32LastNow == ulnow && pstCurlWrapper->d32LastTotal == ultotal)
    {
        int pass_time = util_time_pass(&pstCurlWrapper->stLastChangeTimev);
        if (pass_time > 180*1000)
        {
            pstCurlWrapper->bRunning = 0;
            pstCurlWrapper->enResult = CURL_RESULT_READ_TIMEOUT;
            tRet = -1;
        }
    }
    else
    {
        pstCurlWrapper->d32LastNow = ulnow;
        pstCurlWrapper->d32LastTotal = ultotal;
        pstCurlWrapper->stLastChangeTimev = stNow;
    }

    return tRet;
}

static int _curl_wrapper_IsValid(curl_wrapper_s* _pstCurlWrapper)
{
	if(_pstCurlWrapper == NULL)
	{
		return 0;	
	}

	if(_pstCurlWrapper->u32ValidMagie != CURL_WRAPPER_VALID_MAGIC)
	{
		return 0;
	}
	
    return 1;
}

handle curl_wrapper_init(int timeout)
{
    static int bInitAll = 0; //todo:see curl_global_cleanup
    if(bInitAll == 1)
    {
        bInitAll = 1;
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    curl_wrapper_s* pstCurlWrapper = (curl_wrapper_s*)malloc(sizeof(curl_wrapper_s));
    memset(pstCurlWrapper, 0, sizeof(curl_wrapper_s));

    pstCurlWrapper->timeout = timeout;
    pstCurlWrapper->u32ValidMagie = CURL_WRAPPER_VALID_MAGIC;
    return pstCurlWrapper;
}

int curl_wrapper_destroy(handle _hndCurlWrapper)
{
    curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;
    pstCurlWrapper->u32ValidMagie = ~CURL_WRAPPER_VALID_MAGIC;

    free(_hndCurlWrapper);

    return 0;
}

int curl_wrapper_StartHttpPostImg(handle _hndCurlWrapper, char* _szUrl, char* _szFilename)
{
    int s32Ret;
    curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;

    strcpy(pstCurlWrapper->szUrl, _szUrl);

    memset(pstCurlWrapper->au8PostRecv, 0, sizeof(pstCurlWrapper->au8PostRecv));
    pstCurlWrapper->u32PostRecvTotal = sizeof(pstCurlWrapper->au8PostRecv);
    pstCurlWrapper->u32PostRecvCurr = 0;
    pstCurlWrapper->u32PostRecvHeaderTotal = 0;
    pstCurlWrapper->u32PostRecvBodyTotal = 0;

    pstCurlWrapper->formpost = NULL;
    pstCurlWrapper->lastptr = NULL;
    pstCurlWrapper->headerlist = NULL;

    curl_formadd(&pstCurlWrapper->formpost,
                 &pstCurlWrapper->lastptr,
                 CURLFORM_COPYNAME, "file1",
                 CURLFORM_FILE, _szFilename,
                 CURLFORM_END);

    /* Fill in the submit field too, even if this is rarely needed */
    curl_formadd(&pstCurlWrapper->formpost,
                 &pstCurlWrapper->lastptr,
                 CURLFORM_COPYNAME, "submit",
                 CURLFORM_COPYCONTENTS, "Submit",
                 CURLFORM_END);

    pstCurlWrapper->pstCurl = curl_easy_init();
    pstCurlWrapper->headerlist = curl_slist_append(pstCurlWrapper->headerlist, "Expect:");

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_URL, pstCurlWrapper->szUrl);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HTTPHEADER, pstCurlWrapper->headerlist);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HTTPPOST, pstCurlWrapper->formpost);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_WRITEFUNCTION, _OnHttpPostRecv);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_WRITEDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSFUNCTION, _OnHttpPostProgress);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_NOSIGNAL, 1L);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_VERBOSE, 1L);

    pstCurlWrapper->bRunning = 1;
    s32Ret = pthread_create(&pstCurlWrapper->tPid, NULL, _Proc, pstCurlWrapper);
    return 0;
}

int curl_wrapper_StartHttpPost(handle _hndCurlWrapper, char* _szUrl, unsigned char* _pUlData, unsigned int _u32UlDataSize)
{
    int s32Ret;
    curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;

    strcpy(pstCurlWrapper->szUrl, _szUrl);

    pstCurlWrapper->pu8PostSend = (unsigned char*)malloc(_u32UlDataSize);
    memcpy(pstCurlWrapper->pu8PostSend, _pUlData, _u32UlDataSize);
    pstCurlWrapper->u32PostSendTotal = _u32UlDataSize;
    pstCurlWrapper->u32PostSendCurr = _u32UlDataSize;

    memset(pstCurlWrapper->au8PostRecv, 0, sizeof(pstCurlWrapper->au8PostRecv));
    pstCurlWrapper->u32PostRecvTotal = sizeof(pstCurlWrapper->au8PostRecv);
    pstCurlWrapper->u32PostRecvCurr = 0;
    pstCurlWrapper->u32PostRecvHeaderTotal = 0;
    pstCurlWrapper->u32PostRecvBodyTotal = 0;

    pstCurlWrapper->headerlist = NULL;

    pstCurlWrapper->pstCurl = curl_easy_init();

    pstCurlWrapper->headerlist = curl_slist_append(pstCurlWrapper->headerlist, pstCurlWrapper->au8PostHead);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_URL, pstCurlWrapper->szUrl);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HTTPHEADER, pstCurlWrapper->headerlist);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_POST, 1L);
  	curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_TIMEOUT, pstCurlWrapper->timeout/1000);
  	curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_CONNECTTIMEOUT, pstCurlWrapper->timeout/1000);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_POSTFIELDS, pstCurlWrapper->pu8PostSend);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_POSTFIELDSIZE, pstCurlWrapper->u32PostSendCurr);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)pstCurlWrapper->u32PostSendCurr);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HEADERFUNCTION, _OnHttpPostHead);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HEADERDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_WRITEFUNCTION, _OnHttpPostRecv);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_WRITEDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSFUNCTION, _OnHttpPostProgress);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_NOSIGNAL, 1L);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_VERBOSE, 1L);

    pstCurlWrapper->bRunning = 1;
    s32Ret = pthread_create(&pstCurlWrapper->tPid, NULL, _Proc, pstCurlWrapper);

    return 0;
}

int curl_wrapper_StartHttpGet(handle _hndCurlWrapper, char* _szUrl)
{
    int s32Ret;
    curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;

    //todo:check _szUrl, _pUlData, _u32UlDataSize, _pDlData, _u32DlDataSize
    strcpy(pstCurlWrapper->szUrl, _szUrl);

    memset(pstCurlWrapper->au8PostRecv, 0, sizeof(pstCurlWrapper->au8PostRecv));
    pstCurlWrapper->u32PostRecvTotal = sizeof(pstCurlWrapper->au8PostRecv);
    pstCurlWrapper->u32PostRecvCurr = 0;
    pstCurlWrapper->u32PostRecvHeaderTotal = 0;
    pstCurlWrapper->u32PostRecvBodyTotal = 0;

    pstCurlWrapper->pstCurl = curl_easy_init();

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_URL, pstCurlWrapper->szUrl);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HTTPGET, 1L);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HEADERFUNCTION, _OnHttpPostHead);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HEADERDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_WRITEFUNCTION, _OnHttpPostRecv);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_WRITEDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSFUNCTION, _OnHttpPostProgress);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_NOSIGNAL, 1L);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_VERBOSE, 1L);

    pstCurlWrapper->bRunning = 1;
    s32Ret = pthread_create(&pstCurlWrapper->tPid, NULL, _Proc, pstCurlWrapper);

    return 0;
}

int curl_wrapper_StartUpload(handle _hndCurlWrapper, char* _szUrl, char* _szUsr, char* _szPwd, char* _szLocalPath)
{
    int s32Ret;
    curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;

    //todo:check _szUrl, _szUsr, _szPwd, _szLocalPath
    pstCurlWrapper->pFile = fopen(_szLocalPath, "rb");

    fseek(pstCurlWrapper->pFile, 0, SEEK_END);
    unsigned int u32FileSize = ftell(pstCurlWrapper->pFile);
    fseek(pstCurlWrapper->pFile, 0, SEEK_SET);

    strcpy(pstCurlWrapper->szUrl, _szUrl);
    if(strlen(_szUsr) > 0 || strlen(_szPwd) > 0)
    {
        sprintf(pstCurlWrapper->szUsrPwd, "%s:%s", _szUsr, _szPwd);
    }

    pstCurlWrapper->pstCurl = curl_easy_init();

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_URL, pstCurlWrapper->szUrl);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_USERPWD, pstCurlWrapper->szUsrPwd);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_READFUNCTION, _Onsend);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_READDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)u32FileSize);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSFUNCTION, _OnUlProgress);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSDATA, pstCurlWrapper);

	curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_VERBOSE, 1L);

    pstCurlWrapper->bRunning = 1;
    s32Ret = pthread_create(&pstCurlWrapper->tPid, NULL, _Proc, pstCurlWrapper);

    return 0;
}

int curl_wrapper_StartDownload(handle _hndCurlWrapper, char* _szUrl, char* _szUsr, char* _szPwd, char* _szLocalPath)
{
    int s32Ret;
    curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;

    //todo:check _szUrl, _szUsr, _szPwd, _szLocalPath
    pstCurlWrapper->pFile = fopen(_szLocalPath, "wb+");

    strcpy(pstCurlWrapper->szUrl, _szUrl);
    pstCurlWrapper->szUsrPwd[0] = 0;
    if(_szUsr && strlen(_szUsr) > 0 && _szPwd && strlen(_szPwd) > 0)
    {
        sprintf(pstCurlWrapper->szUsrPwd, "%s:%s", _szUsr, _szPwd);
    }

    memset(pstCurlWrapper->au8PostRecv, 0, sizeof(pstCurlWrapper->au8PostRecv));
    pstCurlWrapper->u32PostRecvTotal = sizeof(pstCurlWrapper->au8PostRecv);
    pstCurlWrapper->u32PostRecvCurr = 0;
    pstCurlWrapper->u32PostRecvHeaderTotal = 0;
    pstCurlWrapper->u32PostRecvBodyTotal = 0;

	pstCurlWrapper->headerlist = NULL;
	
	pstCurlWrapper->pstCurl = curl_easy_init();

	if(pstCurlWrapper->au8PostHead){
		pstCurlWrapper->headerlist = curl_slist_append(pstCurlWrapper->headerlist, pstCurlWrapper->au8PostHead);
	}
	
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_TIMEOUT, pstCurlWrapper->timeout/1000);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_CONNECTTIMEOUT, pstCurlWrapper->timeout/1000);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_URL, pstCurlWrapper->szUrl);
    if(strlen(pstCurlWrapper->szUsrPwd) > 0)
    {
        curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_USERPWD, pstCurlWrapper->szUsrPwd);
    }

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HEADERFUNCTION, _OnHttpPostHead);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HEADERDATA, pstCurlWrapper);

	if(pstCurlWrapper->headerlist){
		curl_easy_setopt (pstCurlWrapper->pstCurl, CURLOPT_HTTPHEADER, pstCurlWrapper->headerlist);	
	}
	
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_WRITEFUNCTION, _Onrecv);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_WRITEDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSFUNCTION, _OnDlProgress);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_SSL_VERIFYHOST, 0);

 	curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_VERBOSE, 1L);

    pstCurlWrapper->bRunning = 1;
    s32Ret = pthread_create(&pstCurlWrapper->tPid, NULL, _Proc, pstCurlWrapper);

    return 0;
}

int curl_wrapper_StartDownload2Mem(handle _hndCurlWrapper, char* _szUrl, char* _szUsr, char* _szPwd)
{
    int s32Ret;
    curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;

    pstCurlWrapper->pu8GetRecv = NULL;
    pstCurlWrapper->u32Size = 0;
    pstCurlWrapper->u32Offset = 0;

    strcpy(pstCurlWrapper->szUrl, _szUrl);
    pstCurlWrapper->szUsrPwd[0] = 0;
    if(_szUsr && strlen(_szUsr) > 0 && _szPwd && strlen(_szPwd) > 0)
    {
        sprintf(pstCurlWrapper->szUsrPwd, "%s:%s", _szUsr, _szPwd);
    }

    memset(pstCurlWrapper->au8PostRecv, 0, sizeof(pstCurlWrapper->au8PostRecv));
    pstCurlWrapper->u32PostRecvTotal = sizeof(pstCurlWrapper->au8PostRecv);
    pstCurlWrapper->u32PostRecvCurr = 0;
    pstCurlWrapper->u32PostRecvHeaderTotal = 0;
    pstCurlWrapper->u32PostRecvBodyTotal = 0;

    pstCurlWrapper->pstCurl = curl_easy_init();

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_TIMEOUT, pstCurlWrapper->timeout/1000);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_CONNECTTIMEOUT, pstCurlWrapper->timeout/1000);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_URL, pstCurlWrapper->szUrl);
    if (strlen(pstCurlWrapper->szUsrPwd) > 0)
    {
        curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_USERPWD, pstCurlWrapper->szUsrPwd);
    }

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HEADERFUNCTION, _OnHttpPostHead);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_HEADERDATA, pstCurlWrapper);

    /*abort if slower than 30 bytes/sec during 60 seconds*/
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_LOW_SPEED_LIMIT, 30L);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_WRITEFUNCTION, _OnRecv2mem);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_WRITEDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSFUNCTION, _OnDlProgress);
    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_PROGRESSDATA, pstCurlWrapper);

    curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_NOSIGNAL, 1L);

	curl_easy_setopt(pstCurlWrapper->pstCurl, CURLOPT_VERBOSE, 1L);

    pstCurlWrapper->bRunning = 1;
    s32Ret = pthread_create(&pstCurlWrapper->tPid, NULL, _Proc, pstCurlWrapper);

    return 0;
}

int curl_wrapper_Download2MemGet(handle _hndCurlWrapper, char** output, int* len)
{
    curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;

    *output = (char*)pstCurlWrapper->pu8GetRecv;
    *len = pstCurlWrapper->u32Size;

    return 0;
}

int curl_wrapper_GetProgress(handle _hndCurlWrapper, CURL_STATUS_S* _pstStatus)
{
    curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;

    double d32Progress = 0.0;
    if(((unsigned int)pstCurlWrapper->d32LastTotal) == 0)
    {
        d32Progress = 0;
    }
    else
    {
        d32Progress = pstCurlWrapper->d32LastNow / pstCurlWrapper->d32LastTotal;
    }

    _pstStatus->u32Progress = (unsigned int)((double)100 * d32Progress);
    _pstStatus->bRunning = pstCurlWrapper->bRunning;
    _pstStatus->enResult = pstCurlWrapper->enResult;

    _pstStatus->pu8Recv = pstCurlWrapper->au8PostRecv;
    _pstStatus->u32RecvHeaderTotal = pstCurlWrapper->u32PostRecvHeaderTotal;
    _pstStatus->u32RecvBodyTotal = pstCurlWrapper->u32PostRecvBodyTotal;

    return 0;
}

int curl_wrapper_Stop(handle _hndCurlWrapper)
{
    int s32Ret;
    curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;

    pstCurlWrapper->bRunning = 0;

    if(pstCurlWrapper->pu8PostSend != NULL)
    {
        free(pstCurlWrapper->pu8PostSend);
    }

    if(pstCurlWrapper->pFile != NULL)
    {
        fclose(pstCurlWrapper->pFile);
    }

    s32Ret = pthread_join(pstCurlWrapper->tPid, NULL);

    if (pstCurlWrapper->pu8GetRecv != NULL)
    {
        free(pstCurlWrapper->pu8GetRecv);
        pstCurlWrapper->pu8GetRecv = NULL;
    }

    curl_easy_cleanup(pstCurlWrapper->pstCurl);

    if(pstCurlWrapper->formpost != NULL)
    {
        curl_formfree(pstCurlWrapper->formpost);
    }
    if(pstCurlWrapper->headerlist != NULL)
    {
        curl_slist_free_all(pstCurlWrapper->headerlist);
    }

    return 0;
}

int curl_wrapper_GetPostData(handle _hndCurlWrapper, char* _szRetUrl, unsigned char* _au8RetData, unsigned int* _u32RetDataSize)
{
    curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;
    strcpy(_szRetUrl, pstCurlWrapper->szUrl);
    memcpy(_au8RetData, pstCurlWrapper->pu8PostSend, pstCurlWrapper->u32PostSendCurr);
    *_u32RetDataSize = pstCurlWrapper->u32PostSendCurr;

    return 0;
}

int curl_wrapper_SetPostHead(handle _hndCurlWrapper, char* head)
{		
	curl_wrapper_s* pstCurlWrapper = _hndCurlWrapper;
	memcpy(pstCurlWrapper->au8PostHead, head, strlen(head));

	return 0;
}

