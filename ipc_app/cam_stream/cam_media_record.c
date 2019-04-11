
#include "ipc_util.h"
#include "cam_media_record.h"
#include "cam_media_framepool.h"

#define SPS_LEN                         23
#define PPS_LEN                         4

typedef enum{
    CAM_RES_INFO_360P_W     = 640,
    CAM_RES_INFO_360P_H     = 360,
    CAM_RES_INFO_480P_W     = 720,
    CAM_RES_INFO_480P_H     = 480,
    CAM_RES_INFO_680_480_W  = 680,
    CAM_RES_INFO_680_480_H  = 480,
    CAM_RES_INFO_800_600_W  = 800,
    CAM_RES_INFO_800_600_H  = 600,
    CAM_RES_INFO_720P_W     = 1280,
    CAM_RES_INFO_720P_H     = 720,
    CAM_RES_INFO_1080P_W    = 1920,
    CAM_RES_INFO_1080P_H    = 1080,
}sd_record_dpi_info_e;

static const unsigned char sps_1080[SPS_LEN] = {
                0x67, 0x64, 0x00, 0x28, 0xac, 0x3b, 0x50, 0x3c,
                0x01, 0x13, 0xf2, 0xc2, 0x00, 0x00, 0x03, 0x00,
                0x02, 0x00, 0x00, 0x03, 0x00, 0x3d, 0x08};
static const unsigned char sps_720[SPS_LEN] = {
                0x67, 0x64, 0x00, 0x1F, 0xAC, 0x3B, 0x50, 0x28,
                0x02, 0xDD, 0x08, 0x00, 0x00, 0x00, 0x03, 0x00,
                0x08, 0x00, 0x00, 0x03, 0x00, 0xF4, 0x20};
static const unsigned char sps_480[SPS_LEN] = {
                0x67, 0x64, 0x00, 0x16, 0xAC, 0x3B, 0x50, 0x5A,
                0x1E, 0xD0, 0x80, 0x00, 0x00, 0x00, 0x03, 0x00,
                0x80, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x42};
static const unsigned char sps[SPS_LEN] = {
                0x67, 0x64, 0x00, 0x16, 0xAC, 0x3B, 0x50, 0x5A,
                0x1E, 0xD0, 0x80, 0x00, 0x00, 0x00, 0x03, 0x00,
                0x80, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x42};
static const char pps[PPS_LEN] = {0x68, 0xee, 0x3c, 0x80};

static const char confBUF[2] = {0x15,0x88};   //for 8K
//static const char confBUF[2] = {0x14,0x8};  //for 16k

long long t_sdtotal = 0;
long long t_sdfree = 0;
struct tm curr_time={0};

typedef struct media_record_attr_s
{
	s_record_info record_info;
	av_buffer *video_pool;
	av_buffer *audio_pool;
    pthread_t audio_pid;
	pthread_t video_pid;
    pthread_mutex_t audio_mutex;
	pthread_mutex_t video_mutex;
	MP4FileHandle mp4_handle;
	FILE *aac_fp;
	MP4TrackId video_tk;
	MP4TrackId audio_tk;
	unsigned long int lst_vid;
	unsigned long int lst_aud;
	unsigned long int file_start_time;
	unsigned long int record_start_time;
	int err_cnt;
	char filename[128];
    unsigned char running;
	unsigned char start;
	unsigned char wait_keyframe;
	unsigned char cur_record_type;
	unsigned char lst_record_type;
}
MEDIA_RECORD_ATTR_S;
static MEDIA_RECORD_ATTR_S g_record_args[RECORD_CHANNEL3];

#define STR_SIZE sizeof("rwxrwxrwx")
static int Check_Record_FilePerm(char *filename, e_record_type itype)
{
	struct stat file_stat;
	struct tm tb;
	char file_name[128] = {'\0'};
	char file_path[32] = {'\0'};

	if(itype == RECORD_TYPE_MP4)
		strcpy(file_path, SD_RECORD_PATH);
	else if(itype == RECORD_TYPE_AAC)
		strcpy(file_path, FLASH_RECORD_PATH);
	
	const char fmt[] = "%Y_%m_%d_%H_%M_%S";  
	if (strptime(filename, fmt, &tb)) {
		sprintf(file_name, "%s/YsxCam/%04d-%02d-%02d/%02d/%s",
							file_path, 1900+tb.tm_year, tb.tm_mon+1, 
							tb.tm_mday, tb.tm_hour, filename);
	}

	stat(file_name, &file_stat);

	static char str[STR_SIZE];
    snprintf(str, STR_SIZE, "%c%c%c%c%c%c%c%c%c",
        (file_stat.st_mode & S_IRUSR) ? 'r' : '-', (file_stat.st_mode & S_IWUSR) ? 'w' : '-',
        (file_stat.st_mode & S_IXUSR) ? 'x' : '-',
       
        (file_stat.st_mode& S_IRGRP) ? 'r' : '-', (file_stat.st_mode & S_IWGRP) ? 'w' : '-',
        (file_stat.st_mode & S_IXGRP) ? 'x' : '-',
    
        (file_stat.st_mode & S_IROTH) ? 'r' : '-', (file_stat.st_mode & S_IWOTH) ? 'w' : '-',
        (file_stat.st_mode & S_IXOTH) ? 'x' : '-');

	if(strncmp("r-xr-xr-x", str, STR_SIZE) == 0)
		return 1;
	else
		return 0;
	
}

//for rindex start
/*按时间顺序索引文件*/
int Create_Record_Index(char *basePath, FILE *fIndex, e_record_type itype)
{
    struct dirent **namelist,*ptr;
    struct stat s_buf;
    int n;
    char nameTemp[128];
    DBG("%s\n", basePath);
    n = scandir(basePath, &namelist, NULL, alphasort);
    if (n < 0) {
       	DBG("scan dir error:%s\n",strerror(errno));
        return -1;
    }

    int i;
    for(i=0;i<n;i++) {
        ptr = namelist[i];
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)     ///current dir OR parrent dir
            continue;

        memset(nameTemp,0,sizeof(nameTemp));
        strcpy(nameTemp,basePath);
        strcat(nameTemp,"/");
        strcat(nameTemp,ptr->d_name);

        stat(nameTemp,&s_buf);
        if(S_ISDIR(s_buf.st_mode)) {
//            YSX_LOG(LOG_APP_DEBUG, "### %s\n",ptr->d_name);
            Create_Record_Index(nameTemp, fIndex, itype);
        }
        else if(S_ISREG(s_buf.st_mode)) {
			if(Check_Record_FilePerm(ptr->d_name, itype)){
            	RIndex rtmp;
            	struct tm tb;
				DBG("###%s###\n", ptr->d_name);
            	const char fmt[] = "%Y_%m_%d_%H_%M_%S";                 //"2000_02_01_00_09_33.mp4";
            	if (strptime(ptr->d_name, fmt, &tb)) {
                    rtmp.itype = itype;
                    rtmp.iy = 1900+tb.tm_year;
                    rtmp.im = tb.tm_mon+1;
                    rtmp.id = tb.tm_mday;
                    rtmp.ih = tb.tm_hour;
                    rtmp.imi = tb.tm_min;
                    rtmp.isec = tb.tm_sec;
                    fwrite(&rtmp, sizeof(RIndex), 1, fIndex);
            	}
			}
        }

        free(ptr);
    }
    free(namelist);
    return 0;
}

int Sync_Record_Index(char *path, e_record_type itype, int index)
{
	DBG("Creat New record index\n");
    g_record_args[index].err_cnt = 0;
    char tmp[128];
    FILE *fp = NULL;
    int i_ret = -1;

    fp = fopen(path,"wb");
    if(NULL == fp) {
        DBG("#Create Record Index File error#\n");
		DBG("#Record Path %s#\n", path);
        return -1;
    }

    memset(tmp, 0, sizeof(tmp));
    sprintf(tmp,"%s/YsxCam/", SD_RECORD_PATH);
	if(itype == RECORD_TYPE_AAC){
		memset(tmp, 0, sizeof(tmp));
    	sprintf(tmp,"%s/YsxCam/", FLASH_RECORD_PATH);
	}
		
    i_ret = access(tmp, F_OK);
    if(i_ret == 0) {
        Create_Record_Index(tmp, fp, itype) ;
    }
    fclose(fp);

    return 0;
}

void Printf_Record_Index(RIndex temp)
{
    DBG("iy = %d ",temp.itype);
    DBG("iy = %d ",temp.iy);
    DBG("im = %d ",temp.im);
    DBG("id = %d ",temp.id);
    DBG("ih = %d ",temp.ih);
    DBG("imi = %d ",temp.imi);
    DBG("timelen = %d ",temp.isec);
    DBG("\n");
    return;
}

int Save_Record_Index(e_record_type itype, int index)
{
    FILE *fp = NULL;
    char filename[128];

    if(strlen(g_record_args[index].filename) == 0)
        return 0;
	
	chmod(g_record_args[index].filename, S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);

    /*sync record*/
    struct tm *p = &curr_time;
    RIndex tmp;
    tmp.itype  = itype;
    tmp.iy  = (1900+p->tm_year);
    tmp.im  = (1+p->tm_mon);
    tmp.id  = p->tm_mday;
    tmp.ih  = p->tm_hour;
    tmp.imi = p->tm_min;
    tmp.isec = p->tm_sec;

    memset(filename, 0, sizeof(filename));
    if(itype == RECORD_TYPE_MP4)
        sprintf(filename,"%s",RECORD_INDEX_SD);
    else if(itype == RECORD_TYPE_AAC)
        sprintf(filename,"%s",RECORD_INDEX_FLASH);
	
    fp = fopen(filename,"ab+");
    if(fp){
        fwrite(&tmp,sizeof(RIndex),1,fp);
        fclose(fp);
    }
    return 0;
}

int Get_Record_FilePath(int file_type, RIndex *file_time, char *file_name)
{
    int ret = 0;
    char record_path[32] = {'\0'};

    if(file_type == RECORD_TYPE_MP4){
        strcpy(record_path, SD_RECORD_PATH);
        sprintf(file_name, "%s/YsxCam/%04d-%02d-%02d/%02d/%04d_%02d_%02d_%02d_%02d_%02d.mp4",
            record_path,file_time->iy,file_time->im,file_time->id,
            file_time->ih,
            file_time->iy,file_time->im,file_time->id,
            file_time->ih,file_time->imi,file_time->isec);

        if(access(file_name,F_OK) != 0){
            DBG("file %s no exist #\n",file_name);
            ret = -1;
        }
    }else if(file_type == RECORD_TYPE_AAC){
        strcpy(record_path, FLASH_RECORD_PATH);
        sprintf(file_name, "%s/YsxCam/%04d-%02d-%02d/%02d/%04d_%02d_%02d_%02d_%02d_%02d.aac",
            record_path,file_time->iy,file_time->im,file_time->id,
            file_time->ih,
            file_time->iy,file_time->im,file_time->id,
            file_time->ih,file_time->imi,file_time->isec);

        if(access(file_name,F_OK) != 0){
            DBG("file %s no exist #\n",file_name);
            ret = -1;
        }
    }else{
        DBG("file_type = %d is err \n",file_type);
        ret = -1;
    }

    return ret;
}

int Media_Record_Stop(void)
{
	int index;
	for(index = 0; index < RECORD_CHANNEL3; index++)
		Media_Record_UnInit(RECORD_UNINIT_PRESERVE, index);

	return 0;
}

int Check_SDCard_Exist()
{
	if(access("/tmp/sd_ok",F_OK) == 0)
		return 1;
	else
		return 0;
}

int Record_AAC_Start(int index)
{
	if(NULL == g_record_args[index].filename){
        DBG("file name is NULL !\n");
        return -1;
    }

    if(g_record_args[index].aac_fp){
        fclose(g_record_args[index].aac_fp);
        g_record_args[index].aac_fp = NULL;
    }

    g_record_args[index].aac_fp = fopen(g_record_args[index].filename, "wb");
    if(g_record_args[index].aac_fp == NULL){
        DBG("fopen %s err !\n", g_record_args[index].filename);
    }

    return 0;
}

void Record_AAC_Stop(int index)
{
    pthread_mutex_lock(&g_record_args[index].audio_mutex);
    if(g_record_args[index].aac_fp){
		fclose(g_record_args[index].aac_fp);	
		g_record_args[index].aac_fp = NULL;	
		g_record_args[index].start = 0;
	}
    pthread_mutex_unlock(&g_record_args[index].audio_mutex);
}

int	RecordAAC_Write_Audio(int index, char *buf, int buf_len)
{
	int ret = 0;

    pthread_mutex_lock(&g_record_args[index].audio_mutex);
    if(g_record_args[index].aac_fp){
        fwrite(buf, buf_len, 1, g_record_args[index].aac_fp);
        fflush(g_record_args[index].aac_fp);
        ret = 0;
    }else{
//        YSX_LOG(LOG_APP_ERROR, "pfile is NULL !\n");
        ret = -1;
    }
    pthread_mutex_unlock(&g_record_args[index].audio_mutex);

	return ret;
}

int Check_Record_SavePath(char *record_path)
{
	if(access(record_path, F_OK) == 0 ) {
		return 0;
    }

	return 1;
}

int Check_Record_Readonly()
{
	char buffer[64] = {'\0'};

	FILE *read_fp = popen("mount | grep mmc | grep \"(ro\"","r");
	if (read_fp != NULL)
	{
		fread(buffer, sizeof(char), 64, read_fp);
		if (strlen(buffer) != 0)
		{
			pclose(read_fp);
			DBG("## SD Readonly ## , Disable mp4 record\n");
			DBG("## SD Readonly ## , Disable mp4 record\n");
			DBG("## SD Readonly ## , Disable mp4 record\n");

			remove("/tmp/sd_ok");
			return 1;
		}
	}
	pclose(read_fp);
	return 0;
}

int Record_Delete_OldFile(const char *path, e_record_type itype)
{
	int ret;
	char index_name[64],buf[128],index_tmp[64];
	FILE *fp , *fp_tmp;
	RIndex tm_info;
	DBG("##### DEL RECORD FILE ##### \n");

    memset(index_name, 0, sizeof(index_name));
    memset(index_tmp, 0, sizeof(index_tmp));
    if(itype == RECORD_TYPE_MP4){
        snprintf(index_name,sizeof(index_name),"%s", RECORD_INDEX_SD);
        snprintf(index_tmp,sizeof(index_tmp),"%s", RECORD_INDEX_SD_TMP);
    }
    else{
        snprintf(index_name,sizeof(index_name),"%s", RECORD_INDEX_FLASH);
        snprintf(index_tmp,sizeof(index_tmp),"%s", RECORD_INDEX_FLASH_TMP);
    }
	
	fseek(fp,0,SEEK_SET);
	ret = fread(&tm_info, sizeof(RIndex), 1, fp);
	if(ret <= 0){
		DBG("Empty index file\n");
		return -1;
	};

	char temp[512] = {'\0'};
	while(1){
		ret = fread(temp,1,512,fp);
		if(ret <= 0 )
			break;
		else
			fwrite(temp,ret,1,fp_tmp);
	}

	fclose(fp);
	fclose(fp_tmp);

	remove(index_name);
	rename(index_tmp,index_name);

	memset(buf, 0, sizeof(buf));
	snprintf(buf,sizeof(buf),"%s/YsxCam/%04d-%02d-%02d/%02d/%04d_%02d_%02d_%02d_%02d_%02d.mp4",
			path,tm_info.iy,tm_info.im,tm_info.id,tm_info.ih,tm_info.iy,tm_info.im,tm_info.id,tm_info.ih,tm_info.imi,tm_info.isec);

	chmod(buf, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);		
	DBG("remove file %s\n",buf);
	if(remove(buf) != 0)
		perror("remove file\n");

	return 0;
}

int Check_Record_RemainSize(char *record_path, e_record_type itype)
{
	struct statfs diskInfo; 												//系统stat的结构体
	static int check_cnt = 0;
	
	while(1)
	{
		if(itype == RECORD_TYPE_MP4){
			if(Check_Record_Readonly())
				return RECORD_READONLY;
		}
		
		if (statfs(record_path, &diskInfo) == -1){   						//获取分区的状态
			if(itype == RECORD_TYPE_MP4){
				DBG("stafs failed: %s\n",strerror(errno));
				DBG("statfs failed for path->[%s]\n", record_path);
			}
			return -1;
		}
		if((check_cnt++) > 5)
		{
			DBG("No Space For Record !\n");
			check_cnt = 0;
			return -1;
		}

	    unsigned long long blocksize = diskInfo.f_bsize;    				//每个block里包含的字节数
	    unsigned long long totalsize = diskInfo.f_blocks * blocksize >> 20;
	    unsigned long long freesize  = diskInfo.f_bfree  * blocksize >> 20; //剩余空间的大小

		t_sdtotal = totalsize;
        t_sdfree  = freesize;
//		g_enviro_struct.t_sdtotal = totalsize;
//		g_enviro_struct.t_sdfree  = freesize;

		if(freesize > 50)
		{
			check_cnt = 0;
			break;
		}

	    DBG("Total_size = %llu MB , free_size = %llu MB \n",   totalsize , freesize);
		Record_Delete_OldFile(record_path, itype);

	}

	return 0;
}

static int Create_Multi_Dir(const char *path)
{
    int i, len;

    len = strlen(path);
    char dir_path[len+1];
    dir_path[len] = '\0';

    strncpy(dir_path, path, len);

    for (i=0; i<len; i++) {
        if (dir_path[i] == '/' && i > 0) {
            dir_path[i]='\0';
            if(access(dir_path, F_OK) != 0) {
                    if (mkdir(dir_path, 0755) < 0) {
                            DBG("mkdir=%s:msg=%s\n", dir_path, strerror(errno));
                            return -1;
                    }
            }
            dir_path[i]='/';
        }
    }

    return 0;
}

int Get_Record_FileName(char *path, char *filename, e_record_type file_type)
{
    time_t timep;
    char dir_path[128];
    struct tm *p;

    if(NULL == filename){
        DBG("file name is NULL\n");
        return -1;
    }

    time(&timep);
    struct tm *t= localtime(&timep); //?????????
    memcpy(&curr_time,t,sizeof(struct tm));
    p = t;

    memset(dir_path, 0, sizeof(dir_path));
    sprintf(dir_path,"%sYsxCam/%04d-%02d-%02d/%02d/", path, (1900+p->tm_year), (1+p->tm_mon), p->tm_mday,p->tm_hour);
    Create_Multi_Dir(dir_path);

    /*2016_12_14_10_22_49.mp4*/
	if(file_type == RECORD_TYPE_MP4){
    	sprintf(filename,"%s%04d_%02d_%02d_%02d_%02d_%02d.mp4",dir_path,(1900+p->tm_year),
                    (1+p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
	}else if(file_type == RECORD_TYPE_AAC){
		sprintf(filename,"%s%04d_%02d_%02d_%02d_%02d_%02d.aac",dir_path,(1900+p->tm_year),
        			(1+p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
	}
		
    return 0;
}

char *Get_Record_Dpi(e_video_dpi dpi, unsigned int *dpi_w, unsigned int *dpi_h)
{
    char *sps_dpi = NULL;
    switch(dpi) {
        case RECORD_DPI_1080P: {
            sps_dpi = (char *)sps_1080;
            *dpi_w = CAM_RES_INFO_1080P_W;
            *dpi_h = CAM_RES_INFO_1080P_H;
        }break;
        case RECORD_DPI_720P: {
            sps_dpi = (char *)sps_720;
            *dpi_w = CAM_RES_INFO_720P_W;
            *dpi_h = CAM_RES_INFO_720P_H;
        }break;
        case RECORD_DPI_480P: {
            sps_dpi = (char *)sps_480;
            *dpi_w = CAM_RES_INFO_480P_W;
            *dpi_h = CAM_RES_INFO_480P_H;
        }break;
//        case RECORD_DPI_360: {
//            sps_dpi = (char *)sps_360;
//            *dpi_w = CAM_RES_INFO_360P_W;
//            *dpi_h = CAM_RES_INFO_360P_H;
//        }break;
        default:
            DBG("snap dpi not support, set to default, %s\n", dpi);
    }
    return sps_dpi;
}

int Record_MP4_Start(int index)
{
    unsigned dpi_w = 0, dpi_h = 0;
    char *sps_tmp = Get_Record_Dpi(g_record_args[index].record_info.dpi, &dpi_w, &dpi_h);

    if(NULL == g_record_args[index].filename){
        DBG("file name is null\n");
        return -1;
    }

	pthread_mutex_lock(&g_record_args[index].video_mutex);
    g_record_args[index].mp4_handle = MP4CreateEx(g_record_args[index].filename, 0, 1, 1, 0, 0, 0, 0);
    if (g_record_args[index].mp4_handle == MP4_INVALID_FILE_HANDLE) {
    	DBG("open file fialed.\n");
        return -1;
    }
		
    MP4SetTimeScale(g_record_args[index].mp4_handle, 90000);
	g_record_args[index].video_tk = MP4AddH264VideoTrack(g_record_args[index].mp4_handle, 90000, 90000 / 15, dpi_w, dpi_h,
                                        0x64, //sps[1],// AVCProfileIndication
                                        0x00, //sps[2],// profile_compat
                                        0x1f, //sps[3],// AVCLevelIndication
                                        3); // 4 bytes length before each NAL unit
	MP4SetVideoProfileLevel(g_record_args[index].mp4_handle, 0x7F);

	if (g_record_args[index].video_tk == MP4_INVALID_TRACK_ID) {
      	DBG("add video track fialed.\n");
      	return -1;
	}

	int audio_time_scale = 16000;  // samplerate
	g_record_args[index].audio_tk = MP4AddAudioTrack(g_record_args[index].mp4_handle, audio_time_scale, 1024, MP4_MPEG4_AUDIO_TYPE);
	if (g_record_args[index].audio_tk == MP4_INVALID_TRACK_ID) {
    	DBG("add audio track fialed.\n");
    	return -1;
	}
    MP4SetAudioProfileLevel(g_record_args[index].mp4_handle, 0x2);
	  
    g_record_args[index].lst_vid = 0;
	g_record_args[index].lst_aud = 0;
    g_record_args[index].wait_keyframe = 1;
	pthread_mutex_unlock(&g_record_args[index].video_mutex);
	
    return 0;
}

void Record_MP4_Stop(int index)
{
    pthread_mutex_lock(&g_record_args[index].video_mutex);
    if(g_record_args[index].mp4_handle){
		MP4Close(g_record_args[index].mp4_handle, 0);
		g_record_args[index].mp4_handle = NULL;	
		g_record_args[index].start = 0;
	}
    pthread_mutex_unlock(&g_record_args[index].video_mutex);
}

int RecordMP4_Write_Video(const int index, const int keyframe, 
			uint8_t *vid_data, const int vid_len, const unsigned long int timestamp)
{
 	int ret = 0;
	pthread_mutex_lock(&g_record_args[index].video_mutex);
	
	if(g_record_args[index].mp4_handle != NULL){
		if(vid_len >= 4){
			uint32_t cov_len  = htonl(vid_len -4);
			int i = 0;
			for(i = 0; i < 4; i++)
				vid_data[i] = (cov_len >> (8*i)) & 0xff;
		}

		MP4Duration tmp = 0;
		unsigned long int pts_sec = timestamp/1000000;
		unsigned long int t_diff;

		t_diff = abs((pts_sec-g_record_args[index].lst_vid/1000000)*1000000+timestamp-g_record_args[index].lst_vid);

		if(g_record_args[index].lst_vid/1000000 <= 0 )
			tmp = 90000*1/15;
		else if(t_diff>=(1000000 * 1)){
			tmp = 90000*1/10;
		}
		else
			tmp = 90*t_diff/1000 ;//   90000 / 100000 = 90 / 1000

		g_record_args[index].lst_vid = timestamp;
		if(!MP4WriteSample(g_record_args[index].mp4_handle, g_record_args[index].video_tk, vid_data, vid_len, tmp, 0, 1)){
			DBG("Error while writing video frame \n");
			if(g_record_args[index].err_cnt++ > 10 )
				ret = -1;
		}
	}
	else
		ret = -2;
		
	pthread_mutex_unlock(&g_record_args[index].video_mutex);
	return ret;
}

int RecordMP4_Write_Audio(const int index, const uint8_t *aud_buf, const int aud_len, const unsigned long int timestamp)
{
	int ret = 0;
	pthread_mutex_lock(&g_record_args[index].audio_mutex);

	if(g_record_args[index].mp4_handle != NULL){
		if(!MP4WriteSample(g_record_args[index].mp4_handle, g_record_args[index].audio_tk, aud_buf, aud_len, MP4_INVALID_DURATION, 0, 1)){
			DBG("Error while writing audio frame \n");
			if(g_record_args[index].err_cnt++ > 10 )
				ret = -1;
		}
	}
	else
		ret = -2;
	
	pthread_mutex_unlock(&g_record_args[index].audio_mutex);
	return ret;
}

void *Audio_Record_Proc(void *args)
{
	int ret;
	int index;
	index = (int) (*((int*)args));

	char pr_name[64];
    memset(pr_name,0,sizeof(pr_name));
    sprintf(pr_name,"Audio_Record_Proc[%d]", index);
    prctl(PR_SET_NAME,pr_name);

	char *tmp_data = NULL;
	int tmp_size = 0;
	int tmp_keyframe = 0;
	unsigned long int tmp_timestamp = 0;
	unsigned char tmp_frame_num = 0;

	while(g_record_args[index].running){
		pthread_mutex_lock(&g_record_args[index].audio_mutex);
		Cam_FramePool_Read(g_record_args[index].audio_pool, &tmp_data, &tmp_size, &tmp_keyframe, &tmp_timestamp, &tmp_frame_num);
		pthread_mutex_unlock(&g_record_args[index].audio_mutex);

		if(!Check_SDCard_Exist()){
			usleep(50*1000);
			continue;
		}
		
		if(tmp_size) {
			if(tmp_data) {
				ret = RecordMP4_Write_Audio(index, tmp_data, tmp_size, tmp_timestamp);
				if(ret < 0){
					Record_MP4_Stop(index);
					g_record_args[index].record_info.status_cb(RECORD_WRITE_ERR);
				}
			}
		}
		else
			usleep(50*1000);
		
	}

	DBG("Audio_Record_Proc[%d] exit\n", index);
	pthread_exit(0);
}

void *Video_Record_Proc(void *args)
{
	int ret;
	int index;
	index = (int) (*((int*)args));
	int start_flag = 1;

	char pr_name[64];
    memset(pr_name,0,sizeof(pr_name));
    sprintf(pr_name,"Video_Record_Proc[%d]", index);
    prctl(PR_SET_NAME,pr_name);

	char *tmp_data = NULL;
	int tmp_size = 0;
	int tmp_keyframe = 0;
	unsigned long int tmp_timestamp = 0;
	unsigned char tmp_frame_num = 0;

	g_record_args[index].mp4_handle = NULL;
	g_record_args[index].aac_fp = NULL;
	g_record_args[index].video_tk = MP4_INVALID_TRACK_ID;
	g_record_args[index].audio_tk = MP4_INVALID_TRACK_ID;

	while(g_record_args[index].running){
		if(access("/tmp/index_sync",F_OK) == 0){
        	remove("/tmp/index_sync");
			Sync_Record_Index(RECORD_INDEX_SD, RECORD_TYPE_MP4, index);
		
			pthread_mutex_lock(&g_record_args[index].video_mutex);
			Cam_FramePool_Stop(g_record_args[index].video_pool);
			Cam_FramePool_Clean(g_record_args[index].video_pool);
			Cam_FramePool_Start(g_record_args[index].video_pool);
			pthread_mutex_unlock(&g_record_args[index].video_mutex);

			pthread_mutex_lock(&g_record_args[index].audio_mutex);
			Cam_FramePool_Stop(g_record_args[index].audio_pool);
			Cam_FramePool_Clean(g_record_args[index].audio_pool);
			Cam_FramePool_Start(g_record_args[index].audio_pool);
			pthread_mutex_unlock(&g_record_args[index].audio_mutex);
    	}

		if(!Check_SDCard_Exist()){
			usleep(50*1000);
			continue;
		}

		pthread_mutex_lock(&g_record_args[index].video_mutex);
		Cam_FramePool_Read(g_record_args[index].video_pool, &tmp_data, &tmp_size, &tmp_keyframe, &tmp_timestamp, &tmp_frame_num);
		pthread_mutex_unlock(&g_record_args[index].video_mutex);
		
		if(tmp_size) {
			if(tmp_data) {
				if(!g_record_args[index].record_start_time)	
					g_record_args[index].record_start_time = tmp_timestamp/1000000;
					
				if(g_record_args[index].record_info.total_len){
					if(abs(tmp_timestamp/1000000 - g_record_args[index].record_start_time) >= (g_record_args[index].record_info.total_len-1)){
						Record_MP4_Stop(index);
						Save_Record_Index(RECORD_TYPE_MP4, index);
						g_record_args[index].record_info.status_cb(RECORD_END);
						break;
					}
				}
				
				if(!g_record_args[index].start){
					if(tmp_keyframe){
						ret = Check_Record_RemainSize(SD_RECORD_PATH, RECORD_TYPE_MP4);
						if(!ret){
							memset(g_record_args[index].filename, 0, 128);
							g_record_args[index].file_start_time = tmp_timestamp/1000000;
							Get_Record_FileName(SD_RECORD_PATH, g_record_args[index].filename, RECORD_TYPE_MP4);
        					DBG("### new record %s ###\n", g_record_args[index].filename);
        					if(!Record_MP4_Start(index)){
								g_record_args[index].start = 1;
								ret = RecordMP4_Write_Video(index, tmp_keyframe, tmp_data, tmp_size, tmp_timestamp);
								if(ret < 0)
									g_record_args[index].record_info.status_cb(RECORD_WRITE_ERR);
        					}
						}
						else
							g_record_args[index].record_info.status_cb(ret);
					}
				}
				else{
					if(abs(tmp_timestamp/1000000 - g_record_args[index].file_start_time) >= (g_record_args[index].record_info.file_len-1)){
						Record_MP4_Stop(index);
						Save_Record_Index(RECORD_TYPE_MP4, index);
						DBG("### record over %s ###\n", g_record_args[index].filename);
					}else{
						ret = RecordMP4_Write_Video(index, tmp_keyframe, tmp_data, tmp_size, tmp_timestamp);
						if(ret < 0){
							Record_MP4_Stop(index);
							g_record_args[index].record_info.status_cb(RECORD_WRITE_ERR);
						}
					}
				}
			}
		}
		else{
			usleep(50*1000);
		}
	}

	DBG("Video_Record_Proc[%d] exit\n", index);
	pthread_exit(0);
}

int Media_Record_Init(s_record_info info, av_buffer *video_handle, av_buffer *audio_handle)
{
	int ret;

	if (MEDIA_RECORD_DBG){
		DBG("record.total_len = %ld\n", info.total_len);
		DBG("record.file_len = %ld\n", info.file_len);
		DBG("record.dpi = %d\n", info.dpi);
	}
	
	memset(&g_record_args[info.channel], 0, sizeof(MEDIA_RECORD_ATTR_S));
	g_record_args[info.channel].audio_pool = audio_handle;
	g_record_args[info.channel].video_pool = video_handle;
	memcpy(&g_record_args[info.channel].record_info, &info, sizeof(s_record_info));

	g_record_args[info.channel].running = 1;
	g_record_args[info.channel].cur_record_type = RECORD_TYPE_MIN;
	pthread_mutex_init(&g_record_args[info.channel].audio_mutex, NULL);
	pthread_mutex_init(&g_record_args[info.channel].video_mutex, NULL);
	
	ret = pthread_create(&g_record_args[info.channel].video_pid, NULL, Video_Record_Proc, &g_record_args[info.channel].record_info.channel);
	if(ret < 0){
		DBG("Creat Video_record_proc failed\n");
		return -1;
	}

	ret = pthread_create(&g_record_args[info.channel].audio_pid, NULL, Audio_Record_Proc, &g_record_args[info.channel].record_info.channel);
	if(ret < 0){
		DBG("Creat Audio_record_proc failed\n");
		return -1;
	}

	return 0;
}

int Media_Record_UnInit(e_record_uninit_type type, e_record_channel index)
{
	g_record_args[index].running = 0;
	pthread_join(g_record_args[index].audio_pid, NULL);
	pthread_join(g_record_args[index].video_pid, NULL);

	Record_MP4_Stop(index);
	Record_AAC_Stop(index);

	pthread_mutex_destroy(&g_record_args[index].audio_mutex);
	pthread_mutex_destroy(&g_record_args[index].video_mutex);	

	memset(&g_record_args[index], 0, sizeof(MEDIA_RECORD_ATTR_S));
}

