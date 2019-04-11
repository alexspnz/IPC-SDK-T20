#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>    
#include <sys/stat.h>    
#include <fcntl.h>
#include <curl/curl.h>
#include <pthread.h>
#include <sys/prctl.h>

#include "json/json.h"
#include "fw_download.h"
#include "fw_curl.h"
#include "fw_md5.h"

char url_JsonHead[] 	= "Content-type:application/json;charset='utf-8'"; 
char url_DefaultHead[] 	= "application/x-www-form-urlencoded";

typedef struct g_OTAStruct
{
	char token[1024];
	char last_version[32];
	char md5[64];
	pthread_t pid;
    pthread_mutex_t mutex;
	G_DownloadStruct user;
	char auth_url[256];
	char key_url[256];
	char ServiceSurvey[256];
	int running;
}G_OTAStruct;

static G_OTAStruct *g_fwdownload_args = NULL;

typedef struct g_AWS3Struct
{
	unsigned char AccessKeyID[256]; 
	unsigned char EndPoint[256]; 
	unsigned char ExpTime[256]; 
	unsigned char Path[256]; 
	unsigned char Region[256]; 
	unsigned char SecretAccessKey[256];
	unsigned char Servicename[256];	
}G_AWS3Struct;

static G_AWS3Struct g_aws3_args;

unsigned char *ysx_Hash256(unsigned char* in)
{
	SHA256_CTX ctx;
	unsigned char *out = malloc(SHA256_DIGEST_LENGTH);
	SHA256((unsigned char *)in, strlen(in), out);

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, in, strlen(in));
	SHA256_Final(out, &ctx);
	OPENSSL_cleanse(&ctx, sizeof(ctx));

	return out;
}

unsigned char *ysx_Hmac256(unsigned char* key, char *in)
{
	int out_len = 0;
	unsigned char *out = malloc(EVP_MAX_MD_SIZE);

	HMAC_CTX ctx;
	HMAC_CTX_init(&ctx);
	HMAC_Init_ex(&ctx, key, strlen(key), EVP_sha256(), NULL);
	HMAC_Update(&ctx, in, strlen(in));
	HMAC_Final(&ctx, out, &out_len);
	HMAC_CTX_cleanup(&ctx);

	return out;
}

static int Printf_Fwdownload_Status(int status)
{
	switch(status)
	{
		case INIT_ERR:
			LOG("Fwdownload INIT_ERR\n");
			break;
		case AUTH_OK:
			LOG("Fwdownload AUTH_OK\n");
			break;
		case AUTH_ERR:
			LOG("Fwdownload AUTH_ERR\n");
			break;
		case GETKEY_OK:
			LOG("Fwdownload GETKEY_OK\n");
			break;
		case GETKEY_ERR:
			LOG("Fwdownload GETKEY_ERR\n");
			break;	
		case SERVICE_OK:
			LOG("Fwdownload SERVICE_OK\n");
			break;
		case SERVICE_ERR:
			LOG("Fwdownload SERVICE_ERR\n");
			break;
		case DOWNLOAD_OK:
			LOG("Fwdownload DOWNLOAD_OK\n");
			break;
		case DOWNLOAD_ERR:
			LOG("Fwdownload DOWNLOAD_ERR\n");
			break;
		case NEED_UPDATE:
			LOG("Fwdownload NEED_UPDATE\n");
			break;
		case NO_NEED_UPDATE:
			LOG("Fwdownload NO_NEED_UPDATE\n");
			break;
		case MD5_OK:
			LOG("Fwdownload MD5_OK\n");
			break;
		case MD5_ERR:
			LOG("Fwdownload MD5_ERR\n");
			break;
		case FLATFORM_ERR:
			LOG("Fwdownload FLATFORM_ERR\n");
			break;
		case DOWNLOAD_TXT_OK:
			LOG("Fwdownload DOWNLOAD_TXT_OK\n");
			break;
		case DOWNLOAD_TXT_ERR:
			LOG("Fwdownload DOWNLOAD_TXT_ERR\n");
			break;
		case DOWNLOAD_BIN_OK:
			LOG("Fwdownload DOWNLOAD_BIN_OK\n");
			break;
		case DOWNLOAD_BIN_ERR:
			LOG("Fwdownload DOWNLOAD_BIN_ERR\n");
			break;
		default:
			status = INIT_ERR;
	}

	return status;
}

unsigned char *hmac_sha256(unsigned char *key,const unsigned char *data, int key_len)
{
	return HMAC(EVP_sha256(), key, key_len, data, strlen(data), NULL, NULL);
}

int strToHex(unsigned char *ch,unsigned char *hex )
{
	if(ch == NULL)
		return -1;

	int i =0;
	for(i=0; i< 32; i++)
	{
		sprintf(hex,"%02x", ch[i]);
		hex+=2;
	}
	if(hex == NULL)
		return -1;

	return 0;
}

void get_utc_time(char *outdata,int mode)
{
	struct timeval time;

	struct tm *tm;

	gettimeofday(&time, NULL);

	tm = gmtime(&time.tv_sec);

	if(mode == 1)
	{
		sprintf(outdata,"%04d%02d%02dT%02d%02d%02dZ"
				,tm->tm_year+1900
				,tm->tm_mon+1
				,tm->tm_mday
				,tm->tm_hour
				,tm->tm_min
				,tm->tm_sec);
	}
	if(mode == 2)
	{
		sprintf(outdata,"%04d%02d%02d"
				,tm->tm_year+1900
				,tm->tm_mday
				,tm->tm_mon+1);
	}
}

static int md5_check(char *file)
{
	unsigned char file_md5[64];
	memset(file_md5, 0, sizeof(file_md5));
	
	if(0 == md5_filesum(file, file_md5))
	{
		if(memcmp(file_md5, g_fwdownload_args->md5, strlen((char*)file_md5)))
		{
			LOG("md5 check fail %s : %s\n", file_md5, g_fwdownload_args->md5);
			return MD5_ERR;
		}
		else{
			
			LOG("md5 check ok %s : %s\n", file_md5, g_fwdownload_args->md5);
			return MD5_OK;
		}
	}
	else{
		
		LOG("md5_filesun err.\n");
		return MD5_ERR;
	}

}

static int compare_version_number()
{
	char update_version[64] = {0}; 
	
	if(g_fwdownload_args->user.platform == Ingenic)
	{
		sprintf(update_version, "%s", Ingenic_Version);	
	}else if(g_fwdownload_args->user.platform == Sonix)
	{
		sprintf(update_version, "%s", Sonix_Version);	
	}
	else
		return NO_NEED_UPDATE;	
	
	FILE *fp;
	char buf[64] = {0};
	
	fp = fopen(update_version ,"r");
	if(!fp)
	{
		perror("open file update version error");
		return NO_NEED_UPDATE;
	}

    if(fgets(buf, sizeof(buf), fp))
    {
    	memset(g_fwdownload_args->last_version, 0, sizeof(g_fwdownload_args->last_version));
		sscanf(buf, "%[0-9A-Za-z.]", g_fwdownload_args->last_version);
       	LOG("update version : %s\n", g_fwdownload_args->last_version);
    }
	else{
        perror("get buf of update version error");
        fclose(fp);
        return NO_NEED_UPDATE;
    }
	
	memset(buf, 0, sizeof(buf));	
    fread(buf,1,sizeof(buf),fp);
    fclose(fp);
	LOG("update_file=%s\n",buf);
	
    char *start = strstr(buf,"md5:\"");
    if(start)
    {
        start += strlen("md5:\"");
        int size = strcspn(start,"\"");
        memset(g_fwdownload_args->md5, 0, sizeof(g_fwdownload_args->md5));
        memcpy(g_fwdownload_args->md5, start, size);
        LOG("md5:--%s--\n",g_fwdownload_args->md5);
    }else{

        LOG("get md5 in %s error \n",update_version);
        return MD5_ERR;
    }

    if((memcmp(g_fwdownload_args->user.cur_version, g_fwdownload_args->last_version, strlen(g_fwdownload_args->last_version)) < 0)) 
	{
		return NEED_UPDATE;
    }
	else
	{
		return NO_NEED_UPDATE;		
	}
}

static char *convert_to_aes_base(json_object *indata)
{
    unsigned char *cbc_data = NULL; 
    int cbc_len = 0;	
    int base_len = 0 , json_len = 0;	
    unsigned char *json_data = NULL;	
    unsigned char key[] = AES_KEY;	

    if ((json_data = json_object_to_json_string(indata)) == NULL) 
    { 
        printf("failed to convert json object to string\n");
        return NULL;
    }       	

    json_len = strlen(json_data);

    cbc_len = json_len+256;
    cbc_data = (unsigned char *)malloc(cbc_len);
    if(!cbc_data) 
    {
        printf("failed to malloc len = %d \n", cbc_len);
		return NULL;
    } 

    memset(cbc_data, 0x00, cbc_len);
    cbc_len = 0;
    ysx_aes_cbc_encrypt((unsigned char *)json_data, json_len, cbc_data, &cbc_len, (char *)key);
	
    char *base_data = base64_encode((const char*)cbc_data, cbc_len, &base_len);

	free(cbc_data); 
	
    return base_data;
}

static int download_GetHttpCode(char* input)
{
    if(strstr(input,"200 OK"))
    {
        return 1;
    }    
    return 0;
}

static int aws_download_file(char* download_path)
{
	int ret = DOWNLOAD_ERR; 
	
	char formattedTime[17] = {'\0'};
	char formattedShortTime[9] = {'\0'};
	char *authHeaderPrefix = "AWS4-HMAC-SHA256";
	char *signedHeaders = "host;x-amz-content-sha256;x-amz-date";

	unsigned char key[64] = {'\0'};
	unsigned char *sha_data = NULL; 
	unsigned char *hash_data=NULL;
	unsigned char *hex = NULL;
	handle g_hndCurlWrapper = NULL;
	
	get_utc_time(formattedTime, 1);
	get_utc_time(formattedShortTime, 2);

	printf("[%s:%d] formattedTime:%s, len = %d \n", __func__, __LINE__, formattedTime, strlen(formattedTime));
	printf("[%s:%d] formattedShortTime:%s, len = %d \n", __func__, __LINE__, formattedShortTime, strlen(formattedShortTime));
	sprintf(key,"AWS4%s",g_aws3_args.SecretAccessKey);

	/* generate sha key */
	sha_data = hmac_sha256(key,formattedShortTime,strlen(key));
	CLEAR(key);
	memcpy(key,sha_data,32);
	free(sha_data);
	sha_data=NULL;

	sha_data = hmac_sha256(key,g_aws3_args.Region,32);
	CLEAR(key);
	memcpy(key,sha_data,32);
	free(sha_data);
	sha_data=NULL;

	sha_data = hmac_sha256(key,g_aws3_args.Servicename,32);
	CLEAR(key);
	memcpy(key,sha_data,32);
	free(sha_data);
	sha_data=NULL;
	
	sha_data = hmac_sha256(key,"aws4_request",32);
	CLEAR(key);
	memcpy(key,sha_data,32);
	free(sha_data);
	sha_data=NULL;

	char url[256];
	memset(url, 0, 256);
	sprintf(url, "https://%s%s",g_aws3_args.EndPoint,download_path);
	printf("[%s:%d] url:%s\n", __func__, __LINE__, url);
	
	hex = (unsigned char *)malloc(64);
	memset(hex, 0, 64);
	hash_data = ysx_Hash256("");	
	if(hash_data == NULL)
	{
		printf("[%s:%d] ysx_Hash256 error \n", __func__, __LINE__);
		free(hex);
		return -1;
	}
	
	if(-1 == (strToHex(hash_data , hex)))
	{
		printf("[%s:%d] strToHex error \n", __func__, __LINE__);
		free(hex);
		return -1;
	}
	
	if(hash_data)
	{
		free(hash_data);hash_data = NULL;
	}

	char credentialString[256];
	CLEAR(credentialString);
	snprintf(credentialString,sizeof(credentialString), "%s/%s/%s/aws4_request", formattedShortTime, g_aws3_args.Region, g_aws3_args.Servicename);

	char canonicalString[512];
	CLEAR(canonicalString);
	sprintf(canonicalString, "GET\n%s\n\nhost:%s\nx-amz-content-sha256:%s\nx-amz-date:%s\n\n%s\n%s", download_path, g_aws3_args.EndPoint, hex, formattedTime, signedHeaders, hex);
	
	unsigned char *p_hash = (unsigned char *)malloc(64);
	hash_data = ysx_Hash256(canonicalString);
	if(hash_data == NULL)
	{
		printf("[%s:%d] ysx_Hash256 error \n", __func__, __LINE__);
		free(hex);
		free(p_hash);
		return -1;
	}
	
	if(-1 == (strToHex(hash_data, p_hash)))
	{
		printf("[%s:%d] strToHex error \n", __func__, __LINE__);
		free(hex);
		free(p_hash);
		return -1;

	}
	
	if(hash_data)
	{
		free(hash_data);
		hash_data = NULL;
	}

	unsigned char *stringToSign = (unsigned char *)malloc(512);
	memset(stringToSign,0,512);
	sprintf(stringToSign, "%s\n%s\n%s\n%s", authHeaderPrefix, formattedTime, credentialString, p_hash);

	unsigned char *signature = hmac_sha256(key,stringToSign,32);
	
	unsigned char *hexEncod_signature = (unsigned char *)malloc(512);
	memset(hexEncod_signature,0,512);	
	if(strToHex(signature,hexEncod_signature) == -1)
	{
		printf("[%s:%d] strToHex error \n", __func__, __LINE__);
		free(hexEncod_signature);
		goto finally;
	}	
	
	char header[2*1024];
	memset(header, 0, 2*1024);
	sprintf(header, "Authorization:%s Credential=%s/%s,SignedHeaders=%s,Signature=%s", authHeaderPrefix, g_aws3_args.AccessKeyID, credentialString, signedHeaders, hexEncod_signature);
	free(hexEncod_signature);
	strcat(header,"\r\n");
	strcat(header,"Content-Type:text/plain; charset=utf-8\r\n");
	strcat(header,"X-Amz-Date:");
	strcat(header,formattedTime);
	strcat(header,"\r\n");
	strcat(header,"X-Amz-Content-Sha256:");
	strcat(header,hex);
	strcat(header,"\r\n");
	printf("[%s:%d] download_header:%s", __func__, __LINE__, header);

    g_hndCurlWrapper = curl_wrapper_init(g_fwdownload_args->user.timeout*1000);
	curl_wrapper_SetPostHead(g_hndCurlWrapper, header);
	ret = curl_wrapper_StartDownload(g_hndCurlWrapper, url, NULL, NULL, DOWNLOAD_FILE);

	CURL_STATUS_S stStatus;
    memset(&stStatus, 0, sizeof(stStatus));
	
    while (1)
    {
        curl_wrapper_GetProgress(g_hndCurlWrapper, &stStatus);
    	LOG("DownloadProgress(): %d\n", stStatus.u32Progress);
        if (!stStatus.bRunning)
        {
            break;
        }

        usleep(1000*1000);
    }

	if (stStatus.enResult == CURL_RESULT_OK && download_GetHttpCode((char*)stStatus.pu8Recv))
	{
		LOG("pu8Recv(): %s\n", stStatus.pu8Recv);
		LOG("u32RecvHeaderTotal(): %d\n", stStatus.u32RecvHeaderTotal);
		LOG("u32RecvBodyTotal(): %d\n", stStatus.u32RecvBodyTotal);
	
		ret = DOWNLOAD_OK;

	}else{
		
		LOG("Download fail.\n");
		ret = DOWNLOAD_ERR;
	}
	

finally:
	if(hex)
		free(hex);
	
	if(p_hash);
		free(p_hash);
		
	if(stringToSign)	
		free(stringToSign);
	
	if(signature)
		free(signature);
	
	if(header)
		free(header);
	
	if(g_hndCurlWrapper)
	{
		curl_wrapper_Stop(g_hndCurlWrapper);
    	curl_wrapper_destroy(g_hndCurlWrapper);
	}
	
	return ret;

}
					
static int aws_download_fw_version()
{
	int ret = DOWNLOAD_TXT_ERR;    
	char device_type[64] = {'\0'};
	char download_path[128] = {'\0'};
	char rename_file[64] = {'\0'};
	
	if(g_fwdownload_args->user.platform == Ingenic)
	{
		memcpy(device_type, g_fwdownload_args->user.cur_version, strlen(g_fwdownload_args->user.cur_version));
		LOG("current_version :%s\n", device_type);
		memset(device_type+4, 0, sizeof(device_type)-4);

		LOG("Path:--%s--\n", g_aws3_args.Path);
		sprintf(download_path, "%s%sFW_VERSION", g_aws3_args.Path, device_type);
		LOG("download_path:--%s--\n", download_path);	

		sprintf(rename_file, "%s", Ingenic_Version);
	}
	else if(g_fwdownload_args->user.platform == Sonix)
	{
		memcpy(device_type, g_fwdownload_args->user.cur_version, strlen(g_fwdownload_args->user.cur_version));
		LOG("current_version :%s\n", device_type);
		memset(device_type+2, 0, sizeof(device_type)-2);

		LOG("Path:--%s--\n", g_aws3_args.Path);
		sprintf(download_path, "%s%sTXRX_VERSION", g_aws3_args.Path, device_type);
		LOG("download_path:--%s--\n", download_path);

		sprintf(rename_file, "%s", Sonix_Version);
	}
	else
	{
		LOG("No find flatform!\n");
		ret = FLATFORM_ERR;
	}	

	ret = aws_download_file(download_path);
	if(ret < 0)
	{
    	ret = DOWNLOAD_TXT_ERR;
	}
	else
	{
		ret =  DOWNLOAD_TXT_OK;	
		LOG("rename_file:--%s--\n",rename_file);
    	rename(DOWNLOAD_FILE, rename_file);
	}
	
	return ret;
}

static int aws_download_fw()
{
    int ret = DOWNLOAD_BIN_ERR;
    char download_path[128] = {'\0'};
	
	sprintf(download_path, "%s%s", g_aws3_args.Path, g_fwdownload_args->last_version);
    LOG("download_path:--%s--\n", download_path);
	
    ret = aws_download_file(download_path);
	if(ret < 0)
		return DOWNLOAD_BIN_ERR;	

	if(md5_check(DOWNLOAD_FILE) == MD5_OK)
	{
		if(g_fwdownload_args->user.platform == Ingenic)
		{
        	rename(DOWNLOAD_FILE,Ingenic_Bin);
		}
		else if(g_fwdownload_args->user.platform == Sonix)
		{
			rename(DOWNLOAD_FILE,Sonix_Bin);
		}
		
		ret = DOWNLOAD_BIN_OK;
	}
	else
	{	
		remove(DOWNLOAD_FILE);
//		remove(Ingenic_Version); 	
		remove(Ingenic_Bin); 		
//	    remove(Sonix_Version); 		
		remove(Sonix_Bin);

		ret	= DOWNLOAD_BIN_ERR; 
	}
	
	return ret;
}

static int parse_auth_result(char *data, int data_size, char *token_str)
{
	int ret = AUTH_ERR;
	struct json_object *json = NULL; 
	unsigned char key[] = AES_KEY;
	unsigned char decrypt_data[1024]={0};
	int exptime; 
	int data_len , decode_len = 0;
	char *token = NULL; 
	
	char *decode_data = base64_decode(data, data_size, &decode_len);
	ysx_aes_cbc_decrypt((unsigned char *)decode_data, decode_len, decrypt_data, key);
	printf("[%s:%d] data:%s\n", __func__, __LINE__, decrypt_data);

	json = json_tokener_parse((const char*)decrypt_data);
	if(is_error(json)){
		  ret = errno ? errno : -1;
		  fprintf(stderr,"json_tokener_parse: %s", strerror(ret));
		  goto finally;
	} 	  

	if ((ret = jsoon_get_str(json, "token", (const char **)&token))) {
		  LOG("failed to find token in json: %s", jsoon_to_str(json));
		  goto finally;
	} 	  
	strcpy(token_str, token);

	if ((ret = jsoon_get_int(json, "exptime", (const char **)&exptime))) {
		  LOG("failed to find exptime in json: %s", jsoon_to_str(json));
		  goto finally;
	} 
	
finally:

	if(json)
		json_object_put(json); 				
		
	if(decode_data)		
		free(decode_data);

	return ret;
}

static int YSX_Authenticate(char *uid, char *token)
{
	int ret = AUTH_ERR;
    struct json_object *input_data = NULL;
	handle g_hndCurlWrapper = NULL;
	
    input_data = json_object_new_object();  
    if (NULL == input_data)  
    {   
        printf("json_object_new_object err.\n");	
        ret = AUTH_ERR;
		goto finally;
    }   

    json_object_object_add(input_data, "type", json_object_new_string("camera"));
    json_object_object_add(input_data, "uid",  json_object_new_string(uid));
    json_object_object_add(input_data, "appid",json_object_new_string(""));    

	char *enc_data =  convert_to_aes_base(input_data);
	if(!strlen(enc_data))
	{
		LOG("aes enc_data err.\n");
		ret = AUTH_ERR;
		goto finally;
	}

    g_hndCurlWrapper = curl_wrapper_init(g_fwdownload_args->user.timeout *1000);
	curl_wrapper_SetPostHead(g_hndCurlWrapper, url_DefaultHead);
	curl_wrapper_StartHttpPost(g_hndCurlWrapper, g_fwdownload_args->auth_url, enc_data, strlen(enc_data));

	CURL_STATUS_S stStatus;
    memset(&stStatus, 0, sizeof(stStatus));
	
    while (1)
    {
        ret = curl_wrapper_GetProgress(g_hndCurlWrapper, &stStatus);
    
        if (!stStatus.bRunning)
        {
            break;
        }

        usleep(1000*1000);
    }
	
	if (stStatus.enResult == CURL_RESULT_OK && download_GetHttpCode((char*)stStatus.pu8Recv))
	{
		LOG("pu8Recv(): %s\n", stStatus.pu8Recv);
		LOG("u32RecvHeaderTotal(): %d\n", stStatus.u32RecvHeaderTotal);
		LOG("u32RecvBodyTotal(): %d\n", stStatus.u32RecvBodyTotal);
	
		if(parse_auth_result(stStatus.pu8Recv+stStatus.u32RecvHeaderTotal, stStatus.u32RecvBodyTotal, token) == 0)
		{
			ret = AUTH_OK;
		}
		else{
			ret = AUTH_ERR;
		}

	}else{
		ret = AUTH_ERR;
	}

finally:
	
	if(input_data)
		json_object_put(input_data);					

	if(enc_data)
		free(enc_data);

	if(g_hndCurlWrapper)
	{
		curl_wrapper_Stop(g_hndCurlWrapper);
    	curl_wrapper_destroy(g_hndCurlWrapper);
	}	
	
    return ret;	
}

static int YSX_GetKey()
{
	int ret = GETKEY_ERR;
	
	handle g_hndCurlWrapper = NULL;
    g_hndCurlWrapper = curl_wrapper_init(g_fwdownload_args->user.timeout*1000);
	curl_wrapper_StartDownload(g_hndCurlWrapper, g_fwdownload_args->key_url, NULL, NULL, ACPUBLICKEY);

	CURL_STATUS_S stStatus;
    memset(&stStatus, 0, sizeof(stStatus));
	
    while (1)
    {
        curl_wrapper_GetProgress(g_hndCurlWrapper, &stStatus);
    
        if (!stStatus.bRunning)
        {
            break;
        }

        usleep(1000*1000);
    }
	
	if (stStatus.enResult == CURL_RESULT_OK && download_GetHttpCode((char*)stStatus.pu8Recv))
	{
		LOG("pu8Recv(): %s\n", __func__, __LINE__, stStatus.pu8Recv);
		LOG("u32RecvHeaderTotal(): %d\n", __func__, __LINE__, stStatus.u32RecvHeaderTotal);
		LOG("u32RecvBodyTotal(): %d\n", __func__, __LINE__, stStatus.u32RecvBodyTotal);

		ret = GETKEY_OK;
	}else{
		ret = GETKEY_ERR;
	}

	if(g_hndCurlWrapper)
	{
		curl_wrapper_Stop(g_hndCurlWrapper);
    	curl_wrapper_destroy(g_hndCurlWrapper);
	}
	
    return ret;
}

static int parse_servicesuvey_result(char *indata , int data_size , char *token_str)
{
	int ret = SERVICE_ERR;
	struct json_object *json = NULL; 
	struct json_object *data_json = NULL; 
	struct json_object *digital_json = NULL; 
	
	char* data = NULL; 
	char* digital = NULL;
	char* data_str = NULL;

	unsigned char* AccessKeyID = NULL; 
	unsigned char* EndPoint = NULL; 
	unsigned char* ExpTime = NULL; 
	unsigned char* Path = NULL; 
	unsigned char* Region = NULL; 
	unsigned char* SecretAccessKey = NULL;
	unsigned char* Servicename = NULL;	

	memset(&g_aws3_args, 0, sizeof(G_AWS3Struct));

	json = json_tokener_parse((const char*)indata);
	if (is_error(json)) {
		fprintf(stderr,"json_tokener_parse: %s", strerror(ret));
		ret = SERVICE_ERR;
		goto finally;
	}	   
	if ((ret = jsoon_get_str(json, "Data", (const char **)&data))) {
		LOG("failed to find Data in json: %s", jsoon_to_str(json));
		ret = SERVICE_ERR;
		goto finally;
	}

	data_str = ysx_rsa_pub_decrypt(data, ACPUBLICKEY); 	
	if(data_str == NULL)
	{
		LOG("ysx_rsa_pub_decrypt error\n");
		ret = SERVICE_ERR;
		goto finally;
	}				

	data_json = json_tokener_parse((char*)data_str);
	if (is_error(data_json)) {
		fprintf(stderr,"json_tokener_parse: %s", strerror(ret));
		ret = SERVICE_ERR;
		goto finally;
	}	   
	if ((ret = jsoon_get_str(data_json, "Digital", (const char **)&digital))) {
		LOG("failed to find Digital in json: %s", jsoon_to_str(data_json));
		ret = SERVICE_ERR;		
		goto finally;
	}

	char *digital_str = ysx_rsa_pri_decrypt(digital, OPENSSLKEY);   
	if(digital_str == NULL)
	{
		ret = SERVICE_ERR;		
		goto finally;
	}	

	LOG("data:%s\n", digital_str);
	
	digital_json = json_tokener_parse((char*)digital_str);
	if (is_error(digital_json)) {
		fprintf(stderr,"json_tokener_parse: %s", strerror(ret));
		ret = SERVICE_ERR;
		goto finally;
	} 

	if ((ret = jsoon_get_str(digital_json, "AccessKeyID", (const unsigned char **)&AccessKeyID))) {
		   LOG("failed to find AccessKeyID in json: %s", jsoon_to_str(digital_json));
		   ret = SERVICE_ERR;
		   goto finally;
	}

	if ((ret = jsoon_get_str(digital_json, "EndPoint", (const unsigned char **)&EndPoint))) {
		   LOG("failed to find EndPoint in json: %s", jsoon_to_str(digital_json));
		   ret = SERVICE_ERR;
		   goto finally;
	}

	if ((ret = jsoon_get_str(digital_json, "ExpTime", (const unsigned char **)&ExpTime))) {
		   LOG("failed to find ExpTime in json: %s", jsoon_to_str(digital_json));
		   ret = SERVICE_ERR;
		   goto finally;
	}

	if ((ret = jsoon_get_str(digital_json, "Path", (const unsigned char **)&Path))) {
		   LOG("failed to find Path in json: %s", jsoon_to_str(digital_json));
		   ret = SERVICE_ERR;
		   goto finally;
	}

	if ((ret = jsoon_get_str(digital_json, "Region", (const unsigned char **)&Region))) {
		   LOG("failed to find Region in json: %s", jsoon_to_str(digital_json));
		   ret = SERVICE_ERR;
		   goto finally;
	}

	if ((ret = jsoon_get_str(digital_json, "SecretAccessKey", (const unsigned char **)&SecretAccessKey))) {
		   LOG("failed to find SecretAccessKey in json: %s", jsoon_to_str(digital_json));
		   ret = SERVICE_ERR;
		   goto finally;
	}

	if ((ret = jsoon_get_str(digital_json, "Servicename", (const unsigned char **)&Servicename))) {
		   LOG("failed to find Servicename in json: %s", jsoon_to_str(digital_json));
		   ret = SERVICE_ERR;
		   goto finally;
	}

	strcpy(g_aws3_args.AccessKeyID, AccessKeyID);
	strcpy(g_aws3_args.EndPoint, EndPoint);
	strcpy(g_aws3_args.ExpTime, ExpTime);
	strcpy(g_aws3_args.Path, Path);
	strcpy(g_aws3_args.Region, Region);
	strcpy(g_aws3_args.SecretAccessKey, SecretAccessKey);
	strcpy(g_aws3_args.Servicename, Servicename);

	LOG("AccessKeyID:%s\n", g_aws3_args.AccessKeyID);
	LOG("EndPoint:%s\n", g_aws3_args.EndPoint);
	LOG("ExpTime:%s\n", g_aws3_args.ExpTime);
	LOG("Path:%s\n", g_aws3_args.Path);
	LOG("Region:%s\n", g_aws3_args.Region);
	LOG("SecretAccessKey:%s\n", g_aws3_args.SecretAccessKey);
	LOG("Servicename:%s\n", g_aws3_args.Servicename);
	
	ret = SERVICE_OK;
		
finally:

	if(digital_json)
		json_object_put(digital_json);

	if(json)
		json_object_put(json);
	
	if(data_json)
		json_object_put(data_json);		

	if(data_str)
		free(data_str);

	if(digital_str)
		free(digital_str);

	return ret;
}

static int YSX_ServiceSurvey(char *token)
{
	FILE *file;
    int rsa_len, ret = SERVICE_ERR;
    struct json_object *digital_object = NULL; 
	struct json_object *indata_object = NULL;   
    struct json_object *data_object = NULL; 
	struct json_object *outdata = NULL;
	char *digital_data = NULL;
	char *json_data = NULL;	
	char *enc_data = NULL; 
	char *en_digital = NULL;
	char *ptr_en = NULL;
	handle g_hndCurlWrapper = NULL;
	
    if((file = fopen(PUBLICKEY,"r")) == NULL){
		perror("open key file error");
		return SERVICE_ERR;    
    }

    fseek(file, 0, SEEK_END);
    rsa_len = ftell(file);
    char *rsa_key = (char *)malloc(rsa_len);
    fseek(file, 0, SEEK_SET);
    fread(rsa_key, rsa_len, sizeof(char), file);
	rsa_key[rsa_len]='\0';
    fclose(file);

    if (!(indata_object = json_object_new_object()))  
    {  
        LOG("new json object failed.\n");  
        goto finally;  
    }  			

    if (!(digital_object = json_object_new_object()))  
    {  
        LOG("new json object failed.\n");  
        goto finally;  
    }
	
    json_object_object_add(digital_object, "token", json_object_new_string(token));  
    json_object_object_add(digital_object, "servicename", json_object_new_string("AWS_FW"));  

    if ((digital_data = json_object_to_json_string(digital_object)) == NULL) 
    { 
        LOG("failed to convert json object to string\n");
        goto finally;  
    }       	

    en_digital = ysx_rsa_pri_encrypt(digital_data, OPENSSLKEY, strlen(digital_data));	
	if(!en_digital){
		LOG("ysx_rsa_pri_encrypt failed.\n");
		goto finally;
	}

    if (!(data_object = json_object_new_object()))  
    {  
        LOG("new json object failed.\n");  
        goto finally;  
    }
	
    json_object_object_add(data_object, "Digital", json_object_new_string(en_digital));  
    json_object_object_add(data_object, "Key", json_object_new_string(rsa_key));  
	
	if( !(json_data = json_object_to_json_string(data_object))) 
	{ 
		LOG("failed to convert json object to string\n");
		goto finally;  
	}			
	
	ptr_en = ysx_rsa_pub_encrypt(json_data, ACPUBLICKEY, strlen(json_data));
	if(!ptr_en){
		goto finally;
	}
	
	if (!(outdata = json_object_new_object()))  
	{  
		LOG("new json object failed.\n");  
		goto finally;  
	} 
	
	json_object_object_add(outdata, "data", json_object_new_string(ptr_en));  
	
	if ((enc_data = json_object_to_json_string(outdata)) == NULL) 
	{ 
		LOG("failed to convert json object to string\n");
		goto finally;  
	}		

    g_hndCurlWrapper = curl_wrapper_init(g_fwdownload_args->user.timeout*1000);
	curl_wrapper_SetPostHead(g_hndCurlWrapper,url_JsonHead);	
	curl_wrapper_StartHttpPost(g_hndCurlWrapper, g_fwdownload_args->ServiceSurvey, enc_data, strlen(enc_data));

	CURL_STATUS_S stStatus;
    memset(&stStatus, 0, sizeof(stStatus));
	
    while (1)
    {
        curl_wrapper_GetProgress(g_hndCurlWrapper, &stStatus);
    
        if (!stStatus.bRunning)
        {
            break;
        }

        usleep(1000*1000);
    }
	
	if (stStatus.enResult == CURL_RESULT_OK && download_GetHttpCode((char*)stStatus.pu8Recv))
	{
		LOG("pu8Recv(): %s\n", stStatus.pu8Recv);
		LOG("u32RecvHeaderTotal(): %d\n", stStatus.u32RecvHeaderTotal);
		LOG("u32RecvBodyTotal(): %d\n", stStatus.u32RecvBodyTotal);

		ret = parse_servicesuvey_result(stStatus.pu8Recv+stStatus.u32RecvHeaderTotal, stStatus.u32RecvBodyTotal, token);
		if(ret > 0)
		{
			ret = SERVICE_OK;	
		}else{
			ret = SERVICE_ERR;	
		}
	
	}

finally:
	if(outdata)
		json_object_put(outdata);	
	
	if(indata_object)
		json_object_put(indata_object);	
	
	if(digital_object)
		json_object_put(digital_object);
	
	if(data_object)
		json_object_put(data_object);					

	if(ptr_en)
		free(ptr_en);
	
	if(en_digital)
		free(en_digital);
	
	if(rsa_key)
		free(rsa_key);

	if(g_hndCurlWrapper)
	{
		curl_wrapper_Stop(g_hndCurlWrapper);
    	curl_wrapper_destroy(g_hndCurlWrapper);
	}
	
    return ret;
}

static void del_download_file()
{
	if(!access(Ingenic_Bin, F_OK))
		remove(Ingenic_Bin);
	
	if(!access(Ingenic_Version, F_OK))
		remove(Ingenic_Version);

	if(!access(Sonix_Bin, F_OK))
		remove(Sonix_Bin);

	if(!access(Sonix_Version, F_OK))
		remove(Sonix_Version);

	if(!access(DOWNLOAD_FILE, F_OK))
		remove(DOWNLOAD_FILE);

	if(!access(ACPUBLICKEY, F_OK))
		remove(ACPUBLICKEY);
}

static void* fw_download_proc(void* args)
{
    prctl(PR_SET_NAME, __FUNCTION__);
	int retry_time;
	int ret = 0;
	
	while(g_fwdownload_args->running)
	{		
		for(retry_time = 0; retry_time < RETRY_TIME; retry_time++)
		{
			LOG("[YSX_Authenticate:%d] retry_time:NO.%d\n", __LINE__, retry_time);
			ret = YSX_Authenticate(g_fwdownload_args->user.uid, g_fwdownload_args->token);
			if(ret == AUTH_OK)
			{
				LOG("[YSX_Authenticate:%d] OK!\n", __LINE__);
				break;
			}
		}
		if(ret != AUTH_OK)
		{
			LOG("[YSX_Authenticate:%d] ERR!\n", __LINE__);
			break;
		}
		
		for(retry_time = 0; retry_time < RETRY_TIME; retry_time++)
		{
			LOG("[YSX_GetKey:%d] retry_time:NO.%d\n", __LINE__, retry_time);
			ret = YSX_GetKey();
			if(ret == GETKEY_OK)
			{
				LOG("[YSX_GetKey:%d] OK!\n", __LINE__);
				break;
			}
		}
		if(ret != GETKEY_OK)
		{	
			LOG("[YSX_GetKey:%d] ERR!\n", __LINE__);
			break;
		}
		
		for(retry_time = 0; retry_time < RETRY_TIME; retry_time++)
		{
			LOG("[YSX_ServiceSurvey:%d] retry_time:NO.%d\n", __LINE__, retry_time);
			ret = YSX_ServiceSurvey(g_fwdownload_args->token);
			if(ret == SERVICE_OK)
			{
				LOG("[YSX_ServiceSurvey:%d] OK!\n", __LINE__);
				break;
			}
		}
		if(ret != SERVICE_OK)
		{
			LOG("[YSX_ServiceSurvey:%d] ERR!\n", __LINE__);
			break;
		}

		for(retry_time = 0; retry_time < RETRY_TIME; retry_time++)
		{
			LOG("[YSX_Download_FWVERSION:%d] retry_time:NO.%d\n", __LINE__, retry_time);
			ret = aws_download_fw_version();
			if(ret == DOWNLOAD_TXT_OK)
			{
				LOG("[YSX_Download_FWVERSION:%d] OK!\n", __LINE__);
				break;
			}
		}
		if(ret != DOWNLOAD_TXT_OK)
		{
			LOG("[YSX_Download_FWVERSION:%d] ERR!\n", __LINE__);
			break;
		}
		
		ret = compare_version_number();
		if(ret == NEED_UPDATE)
		{
			for(retry_time = 0; retry_time < RETRY_TIME; retry_time++)
			{
				printf("[YSX_Download_FW:%d] retry_time:NO.%d\n", __LINE__, retry_time);
				ret = aws_download_fw();
				if(ret == DOWNLOAD_BIN_OK)
				{
					LOG("[YSX_Download_FW:%d] OK!\n", __LINE__);
					break;
				}
			}
			if(ret != DOWNLOAD_BIN_OK)
			{
				LOG("[YSX_Download_FW:%d] ERR!\n", __LINE__);
				break;
			}
		}

		break;
		
	}

	g_fwdownload_args->user.cb(Printf_Fwdownload_Status(ret));
	
	return NULL;
}

static void choose_fw_download_env()
{
	if(g_fwdownload_args->user.download_env == Prod_Abroad)
	{
		strcpy(g_fwdownload_args->auth_url, PROD_ABROAD_AUTH_URL);
		strcpy(g_fwdownload_args->key_url, PROD_ABROAD_GETKEY_URL);
		strcpy(g_fwdownload_args->ServiceSurvey, PROD_ABROAD_SERVICESURVEY_URL);
	}
	else if(g_fwdownload_args->user.download_env == Test_Abroad)
	{
		strcpy(g_fwdownload_args->auth_url, TEST_ABROAD_AUTH_URL);
		strcpy(g_fwdownload_args->key_url, TEST_ABROAD_GETKEY_URL);
		strcpy(g_fwdownload_args->ServiceSurvey, TEST_ABROAD_SERVICESURVEY_URL);
	}
	else if(g_fwdownload_args->user.download_env == Prod_Inland)
	{
		strcpy(g_fwdownload_args->auth_url, PROD_INLAND_AUTH_URL);
		strcpy(g_fwdownload_args->key_url, PROD_INLAND_GETKEY_URL);
		strcpy(g_fwdownload_args->ServiceSurvey, PROD_INLAND_SERVICESURVEY_URL);
	}
	else
	{
		strcpy(g_fwdownload_args->auth_url, TEST_INLAND_AUTH_URL);
		strcpy(g_fwdownload_args->key_url, TEST_INLAND_GETKEY_URL);
		strcpy(g_fwdownload_args->ServiceSurvey, TEST_INLAND_SERVICESURVEY_URL);	
	}
}

int fw_download_start(G_DownloadStruct *fw_download)
{
	int ret = 0;

	if(g_fwdownload_args != NULL)
	{
		LOG("g_fwdownload_args is err.\n");
		return INIT_ERR;
	}

	if(fw_download == NULL)
	{
		LOG("fw_download is err.\n");
		return INIT_ERR;	
	}
	
	g_fwdownload_args = (G_OTAStruct*)malloc(sizeof(G_OTAStruct));	
	if(g_fwdownload_args == NULL)
	{
		LOG("g_fwdownload_args malloc is err.\n");
		return INIT_ERR;
	}
	
	del_download_file();

    memset(g_fwdownload_args, 0, sizeof(G_OTAStruct));
    memcpy(&g_fwdownload_args->user, fw_download, sizeof(*fw_download));

	choose_fw_download_env();

	if(!g_fwdownload_args->user.timeout)
		g_fwdownload_args->user.timeout = 15;
	
	if(!strlen(g_fwdownload_args->user.uid))
	{
		LOG("uid is err.\n");
		return INIT_ERR;
	}

	g_fwdownload_args->running = 1;
	ret = pthread_create(&g_fwdownload_args->pid, NULL, fw_download_proc, NULL);
	if(ret < 0)
	{
		ret = INIT_ERR;
		LOG("fw_download_proc is err.\n");
	}
	pthread_detach(g_fwdownload_args->pid);
	
	return ret;
}

int fw_download_exit()
{
	g_fwdownload_args->running = 0;

	if(g_fwdownload_args)
	{
		free(g_fwdownload_args);
		g_fwdownload_args = NULL;
	}
}

