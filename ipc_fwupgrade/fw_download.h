#ifndef FW_DOWNLOAD_H_
#define FW_DOWNLOAD_H_

#define CLEAR(x) memset(x,0,sizeof(x))

#include "ipc_debug.h"

#define OPENSSLKEY   "/etc/ysx/pri.key"
#define PUBLICKEY    "/etc/ysx/pub.key"
#define ACPUBLICKEY  "/tmp/ca_pub.key"

#define RETRY_TIME 5
#define HOST "coding.debuntu.org"
#define PAGE "/"
#define PORT 80
#define USERAGENT 			"HTMLGET 1.1"
#define Ingenic_Bin 		"/tmp/Ingenic.bin"
#define Ingenic_Version 	"/tmp/Ingenic.txt"
#define Sonix_Bin 			"/tmp/Sonix.bin"
#define Sonix_Version 		"/tmp/Sonix.txt"
#define CONFIG_FW_ENCRYPT_KEY 0x63245709
#define SHIFT_LEFT  0
#define SHIFT_RIGHT 1
#define DOWNLOAD_FILE 		"/tmp/download.bin"
#define AES_KEY 			"Qi=+-ho!&(wniyeK"

#define TEST_ABROAD_AUTH_URL			"http://casvr.prod.optjoy.io:23451/v1/Authenticate"
#define TEST_ABROAD_GETKEY_URL  		"http://casvr.prod.optjoy.io:23451/v1/Getkey"
#define TEST_ABROAD_SERVICESURVEY_URL 	"http://casvr.prod.optjoy.io:23451/v1/Servicesuvey"

#define PROD_ABROAD_AUTH_URL			"http://casvr.prod.optjoy.io:23451/v1/Authenticate"
#define PROD_ABROAD_GETKEY_URL  		"http://casvr.prod.optjoy.io:23451/v1/Getkey"
#define PROD_ABROAD_SERVICESURVEY_URL 	"http://casvr.prod.optjoy.io:23451/v1/Servicesuvey"

#define TEST_INLAND_AUTH_URL 			"http://casvr.test.optjoy.cn:23451/v1/Authenticate"
#define TEST_INLAND_GETKEY_URL			"http://casvr.test.optjoy.cn:23451/v1/Getkey"
#define TEST_INLAND_SERVICESURVEY_URL 	"http://casvr.test.optjoy.cn:23451/v1/Servicesuvey"

#define PROD_INLAND_AUTH_URL 			"http://casvr.prod.optjoy.cn:23451/v1/Authenticate"
#define PROD_INLAND_GETKEY_URL			"http://casvr.prod.optjoy.cn:23451/v1/Getkey"
#define PROD_INLAND_SERVICESURVEY_URL 	"http://casvr.prod.optjoy.cn:23451/v1/Servicesuvey"

typedef enum
{
	INIT_ERR 		=  0,          
	AUTH_OK  		=  1,
	AUTH_ERR 		= -1,
	GETKEY_OK 		=  2,
	GETKEY_ERR  	= -2,
	SERVICE_OK  	=  3,
	SERVICE_ERR 	= -3,
	DOWNLOAD_OK 	=  4, 
	DOWNLOAD_ERR	= -4,
	NEED_UPDATE     =  5,
	NO_NEED_UPDATE  = -5, 
	MD5_OK			=  6,
	MD5_ERR			= -6,
	FLATFORM_ERR    = -7,
	DOWNLOAD_TXT_OK =  8,
	DOWNLOAD_TXT_ERR= -8,
	DOWNLOAD_BIN_OK =  9,	
	DOWNLOAD_BIN_ERR= -9,
	
}E_DOWNLOAD_STATUS;

typedef enum
{
	Ingenic = 0,
	Sonix 	= 1,
	
}E_DOWNLOAD_FLATFORM;

typedef enum
{
	Test_Abroad = 0,
	Prod_Abroad = 1,
	Test_Inland = 2,
	Prod_Inland = 3,
	
}E_DOWNLOAD_ENV;

typedef void *handle;
typedef int (*fw_download_cb)(int status);		//E_DOWNLOAD_STATUS

typedef struct g_DownloadStruct
{
	char cur_version[32];
	char uid[64];
	int timeout;								//请求超时时间 秒
	E_DOWNLOAD_ENV download_env;
	E_DOWNLOAD_FLATFORM platform;
	fw_download_cb cb;
}G_DownloadStruct;

extern G_DownloadStruct g_download_struct;

int fw_download_start(G_DownloadStruct *fw_download);
int fw_download_exit();

#endif /* FW_DOWNLOAD_H_ */

