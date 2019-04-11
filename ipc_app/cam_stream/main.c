
#include "ipc_comm.h"
#include "ipc_debug.h"
#include "cam_media.h"
#include "cam_env.h"
#include "cam_cmdctl.h"

CAM_ENV_S g_camenv_attr;
CAM_MEDIA_S g_media_attr;


void sighandle(int signo)
{
	DBG("Start to Uninit Media!\n");

	Cam_Media_UnInit();

	DBG("###Cam Stream End###\n");
	_exit(0);
}

void Cam_Signals_Init(void)
{
    struct sigaction sa;

    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGINT);

    sa.sa_handler = sighandle;
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = sighandle;
    sigaction(SIGINT, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);
}

void Print_Err_Handling (int nErr)
{
	switch (nErr)
	{
	case IOTC_ER_SERVER_NOT_RESPONSE :
		//-1 IOTC_ER_SERVER_NOT_RESPONSE
		DBG ("[Error code : %d]\n", IOTC_ER_SERVER_NOT_RESPONSE );
		DBG ("Master doesn't respond.\n");
		DBG ("Please check the network wheather it could connect to the Internet.\n");
		break;
	case IOTC_ER_FAIL_RESOLVE_HOSTNAME :
		//-2 IOTC_ER_FAIL_RESOLVE_HOSTNAME
		DBG ("[Error code : %d]\n", IOTC_ER_FAIL_RESOLVE_HOSTNAME);
		DBG ("Can't resolve hostname.\n");
		break;
	case IOTC_ER_ALREADY_INITIALIZED :
		//-3 IOTC_ER_ALREADY_INITIALIZED
		DBG ("[Error code : %d]\n", IOTC_ER_ALREADY_INITIALIZED);
		DBG("Already initialized.\n");
		break;
	case IOTC_ER_FAIL_CREATE_MUTEX :
		//-4 IOTC_ER_FAIL_CREATE_MUTEX
		DBG ("[Error code : %d]\n", IOTC_ER_FAIL_CREATE_MUTEX);
		DBG("Can't create mutex.\n");
		break;
	case IOTC_ER_FAIL_CREATE_THREAD :
		//-5 IOTC_ER_FAIL_CREATE_THREAD
		DBG ("[Error code : %d]\n", IOTC_ER_FAIL_CREATE_THREAD);
		DBG ("Can't create thread.\n");
		break;
	case IOTC_ER_UNLICENSE :
		//-10 IOTC_ER_UNLICENSE
		DBG ("[Error code : %d]\n", IOTC_ER_UNLICENSE);
		DBG ("This UID is unlicense.\n");
		DBG ("Check your UID.\n");
		break;
	case IOTC_ER_NOT_INITIALIZED :
		//-12 IOTC_ER_NOT_INITIALIZED
		DBG ("[Error code : %d]\n", IOTC_ER_NOT_INITIALIZED);
		DBG ("Please initialize the IOTCAPI first.\n");
		break;
	case IOTC_ER_TIMEOUT :
		//-13 IOTC_ER_TIMEOUT
		break;
	case IOTC_ER_INVALID_SID :
		//-14 IOTC_ER_INVALID_SID
		DBG ("[Error code : %d]\n", IOTC_ER_INVALID_SID);
		DBG ("This SID is invalid.\n");
		DBG ("Please check it again.\n");
		break;
	case IOTC_ER_EXCEED_MAX_SESSION :
		//-18 IOTC_ER_EXCEED_MAX_SESSION
		DBG ("[Error code : %d]\n", IOTC_ER_EXCEED_MAX_SESSION);
		DBG ("[Warning]\n");
		DBG ("The amount of session reach to the maximum.\n");
		DBG ("It cannot be connected unless the session is released.\n");
		break;
	case IOTC_ER_CAN_NOT_FIND_DEVICE :
		//-19 IOTC_ER_CAN_NOT_FIND_DEVICE
		DBG ("[Error code : %d]\n", IOTC_ER_CAN_NOT_FIND_DEVICE);
		DBG ("Device didn't register on server, so we can't find device.\n");
		DBG ("Please check the device again.\n");
		DBG ("Retry...\n");
		break;
	case IOTC_ER_SESSION_CLOSE_BY_REMOTE :
		//-22 IOTC_ER_SESSION_CLOSE_BY_REMOTE
		DBG ("[Error code : %d]\n", IOTC_ER_SESSION_CLOSE_BY_REMOTE);
		DBG ("Session is closed by remote so we can't access.\n");
		DBG ("Please close it or establish session again.\n");
		break;
	case IOTC_ER_REMOTE_TIMEOUT_DISCONNECT :
		//-23 IOTC_ER_REMOTE_TIMEOUT_DISCONNECT
		DBG ("[Error code : %d]\n", IOTC_ER_REMOTE_TIMEOUT_DISCONNECT);
		DBG ("We can't receive an acknowledgement character within a TIMEOUT.\n");
		DBG ("It might that the session is disconnected by remote.\n");
		DBG ("Please check the network wheather it is busy or not.\n");
		DBG ("And check the device and user equipment work well.\n");
		break;
	case IOTC_ER_DEVICE_NOT_LISTENING :
		//-24 IOTC_ER_DEVICE_NOT_LISTENING
		DBG ("[Error code : %d]\n", IOTC_ER_DEVICE_NOT_LISTENING);
		DBG ("Device doesn't listen or the sessions of device reach to maximum.\n");
		DBG ("Please release the session and check the device wheather it listen or not.\n");
		break;
	case IOTC_ER_CH_NOT_ON :
		//-26 IOTC_ER_CH_NOT_ON
		DBG ("[Error code : %d]\n", IOTC_ER_CH_NOT_ON);
		DBG ("Channel isn't on.\n");
		DBG ("Please open it by IOTC_Session_Channel_ON() or IOTC_Session_Get_Free_Channel()\n");
		DBG ("Retry...\n");
		break;
	case IOTC_ER_SESSION_NO_FREE_CHANNEL :
		//-31 IOTC_ER_SESSION_NO_FREE_CHANNEL
		DBG ("[Error code : %d]\n", IOTC_ER_SESSION_NO_FREE_CHANNEL);
		DBG ("All channels are occupied.\n");
		DBG ("Please release some channel.\n");
		break;
	case IOTC_ER_TCP_TRAVEL_FAILED :
		//-32 IOTC_ER_TCP_TRAVEL_FAILED
		DBG ("[Error code : %d]\n", IOTC_ER_TCP_TRAVEL_FAILED);
		DBG ("Device can't connect to Master.\n");
		DBG ("Don't let device use proxy.\n");
		DBG ("Close firewall of device.\n");
		DBG ("Or open device's TCP port 80, 443, 8080, 8000, 21047.\n");
		break;
	case IOTC_ER_TCP_CONNECT_TO_SERVER_FAILED :
		//-33 IOTC_ER_TCP_CONNECT_TO_SERVER_FAILED
		DBG ("[Error code : %d]\n", IOTC_ER_TCP_CONNECT_TO_SERVER_FAILED);
		DBG ("Device can't connect to server by TCP.\n");
		DBG ("Don't let server use proxy.\n");
		DBG ("Close firewall of server.\n");
		DBG ("Or open server's TCP port 80, 443, 8080, 8000, 21047.\n");
		DBG ("Retry...\n");
		break;
	case IOTC_ER_NO_PERMISSION :
		//-40 IOTC_ER_NO_PERMISSION
		DBG ("[Error code : %d]\n", IOTC_ER_NO_PERMISSION);
		DBG ("This UID's license doesn't support TCP.\n");
		break;
	case IOTC_ER_NETWORK_UNREACHABLE :
		//-41 IOTC_ER_NETWORK_UNREACHABLE
		DBG ("[Error code : %d]\n", IOTC_ER_NETWORK_UNREACHABLE);
		DBG ("Network is unreachable.\n");
		DBG ("Please check your network.\n");
		DBG ("Retry...\n");
		break;
	case IOTC_ER_FAIL_SETUP_RELAY :
		//-42 IOTC_ER_FAIL_SETUP_RELAY
		DBG ("[Error code : %d]\n", IOTC_ER_FAIL_SETUP_RELAY);
		DBG ("Client can't connect to a device via Lan, P2P, and Relay mode\n");
		break;
	case IOTC_ER_NOT_SUPPORT_RELAY :
		//-43 IOTC_ER_NOT_SUPPORT_RELAY
		DBG ("[Error code : %d]\n", IOTC_ER_NOT_SUPPORT_RELAY);
		DBG ("Server doesn't support UDP relay mode.\n");
		DBG ("So client can't use UDP relay to connect to a device.\n");
		break;

	default :
		break;
	}
}


void LoginInfoCB(unsigned int nLoginInfo)
{
	DBG("var: 0x%x\n", nLoginInfo);
	if((nLoginInfo & 0x04))
	{
		DBG("LoginInfoCB 1\n");
		g_camenv_attr.online_status = 1;
		DBG("LoginInfoCB 2\n");
		DBG("I can be connected via Internet\n");
	}
	else if((nLoginInfo & 0x08))
	{
		DBG("LoginInfoCB 3\n");
		g_camenv_attr.online_status = 0;
		DBG("I am be banned by IOTC Server because UID multi-login\n");
	}
	else
		g_camenv_attr.online_status = 0;
}

void *Login_Tutk_Proc(void *arg)
{
	int ret;
	prctl(PR_SET_NAME,"Login_Tutk_Proc");
	
	if((access(RELAY_INVAILD_F,F_OK) != 0) &&
		(g_camenv_attr.login_type == 1)){
		DBG("client call this will connect to device only via LAN or TCP relay mode, %d\n", g_camenv_attr.login_type);
		IOTC_TCPRelayOnly_TurnOn();
		}
	while(1)
	{
		DBG("IOTC_Device_Login() start\n");
		ret = IOTC_Device_Login((char *)arg, NULL, NULL);
		DBG("IOTC_Device_Login() ret = %d\n", ret);
		if(ret == IOTC_ER_NoERROR)
		{
			break;
		}
		else
		{
			DBG("thread_Login Error\n");
			sleep(10);
		}
	}

	pthread_exit(0);
}

int AuthCallBackFn(char *viewAcc,char *viewPwd)
{
	int len ,ret = 0;
	unsigned char key[] = AES_KEY;
	unsigned char de[32];

	char *dePass = NULL;
	char sPCPass[] = "YSX_PCCON";
	
	DBG("client %s pass %s \n", viewAcc, viewPwd);
	if(strlen(viewPwd) < 128){	//APP Client
		char *base_data = ipc_base64_decode(viewPwd, strlen(viewPwd),&len);
		ipc_aes_cbc_decrypt((unsigned char *)base_data, len, de, key);
		free(base_data);
	}
#if 0	
	else{	//PC Client
		dePass = ipc_rsa_pri_decrypt(viewPwd, RSAPRIKEY);
		if(dePass == NULL){
			ret = 0;
			return ret;
		}
	}
#endif
	DBG("client %s pass %s \n", viewAcc, de);

	if(strcmp(viewAcc, "admin") == 0 && strcmp((char *)de, g_camenv_attr.admin_pwd) == 0){
		g_camenv_attr.admin_pid = 0;
		ret = 1;
	}

	if(strcmp(viewAcc, "guest") == 0 && strcmp((char *)de, g_camenv_attr.guest_pwd) == 0){
		ret = 1;
	}
	
if(!dePass)
	return ret;

	// Add for PC Connect
	if(strcmp(viewAcc, "guest") == 0 && strcmp((char *)dePass, sPCPass) == 0){
		ret = 1;
	}

	return ret;
}

void *AVServerStart_Proc(void *arg)
{
	int SID = *(int *)arg;
	free(arg);

	char ioCtrlBuf[MAX_SIZE_IOCTRL_BUF];
	int nResend=0;
	struct st_SInfo Sinfo;
	unsigned int ioType;
	int ret, index;
	int iGNo = -1;
	int isMaster = 0;

	pthread_t current_pid = pthread_self();
	pthread_mutex_lock(&g_camenv_attr.client_connect_lock);
	int avIndex = avServStart3(SID, AuthCallBackFn, 10, SERVTYPE_STREAM_SERVER, 0, &nResend);

	if(!g_camenv_attr.admin_pid){
		g_camenv_attr.admin_pid = current_pid;
		set_master_index(avIndex);
		isMaster = 1;
	}
	pthread_mutex_unlock(&g_camenv_attr.client_connect_lock);

	if(avIndex < 0){
		DBG("avServStart3 failed!! SID[%d] code[%d]!!!\n", SID, avIndex);
		DBG("thread_ForAVServerStart: exit!! SID[%d]\n", SID);
		goto ServerStartQuit;
	}

	/*保存连接到数组空闲成员中*/
	for(index = 0; index < MEDIA_USER_NUM; index++){
		if (g_media_attr.g_media_info[index].avIndex == -1){
			iGNo = index;
			break;
		}
	}
	
	g_media_attr.g_media_info[iGNo].avIndex = avIndex;
	pthread_mutex_lock(&g_camenv_attr.avserver_pthread_lock);
	g_camenv_attr.online_user_num++;
	DBG("use coming total[%d] No %d avindex %d\n", g_camenv_attr.online_user_num, iGNo, avIndex);
	pthread_mutex_unlock(&g_camenv_attr.avserver_pthread_lock);

	/*客人超过4个禁止登录 , 防止主人不能登录*/
	if(!isMaster)
		g_camenv_attr.online_guest_num++;

	if(g_camenv_attr.online_guest_num > 3){
		DBG("guest reach max count %d\n", g_camenv_attr.online_guest_num);
		goto ServerStartQuit0;
	}


	if(IOTC_Session_Check(SID, &Sinfo) == IOTC_ER_NoERROR){
		const char *mode[3] = {"P2P", "RLY", "LAN"};
		// print session information(not a must)
		if(isdigit( Sinfo.RemoteIP[0])){
			DBG("Client is from[IP:%s, Port:%d] Mode[%s] VPG[%d:%d:%d] VER[%lX] NAT[%d] AES[%d]\n",
                Sinfo.RemoteIP, Sinfo.RemotePort, mode[(int)Sinfo.Mode], Sinfo.VID, Sinfo.PID, Sinfo.GID, Sinfo.IOTCVersion, Sinfo.NatType, Sinfo.isSecure);
		}
	}

	while(1)
	{
		ret = avRecvIOCtrl(avIndex, &ioType, (char *)&ioCtrlBuf, MAX_SIZE_IOCTRL_BUF, 2000);
		if(ret >= 0){
			Handle_IOCTRL_Cmd(SID, avIndex, ioCtrlBuf, ioType, iGNo);
		}
		else{
			if(ret != AV_ER_TIMEOUT){
				DBG("avIndex[%d], avRecvIOCtrl error, code[%d]\n",avIndex, ret);
				break;
			}
#if 0			
			if(!GetYSXCtl(YSX_SHARE) && !isMaster){	/*结束线程共享*/
				DBG("Disable Share IPC %d %d %d!\n", GetYSXCtl(YSX_SHARE), current_pid, g_camenv_attr.admin_pid);
				break;
			}
#endif			
		}
	}
ServerStartQuit0:
	avServStop(avIndex);

ServerStartQuit:
	IOTC_Session_Close(SID);
	if (iGNo >= 0)
	{	/*关闭video传输*/
		pthread_mutex_lock(&g_media_attr.g_media_info[iGNo].video_lock);
		g_media_attr.g_media_info[iGNo].is_video = 0;
		pthread_mutex_unlock(&g_media_attr.g_media_info[iGNo].video_lock);

		/*关闭audio传输*/
		pthread_mutex_lock(&g_media_attr.g_media_info[iGNo].audio_lock);
		g_media_attr.g_media_info[iGNo].is_audio = 0;
		pthread_mutex_unlock(&g_media_attr.g_media_info[iGNo].audio_lock);

		/*关闭push_talk传输*/
		pthread_mutex_lock(&g_media_attr.talk_lock);
		g_media_attr.is_talk = 0;
		pthread_mutex_unlock(&g_media_attr.talk_lock);

		/*关闭play_view传输*/
		pthread_mutex_lock(&g_media_attr.play_lock);
		g_media_attr.is_play = 0;
		pthread_mutex_unlock(&g_media_attr.play_lock);

		/*删除连接记录*/
		g_media_attr.g_media_info[iGNo].avIndex = -1;
		pthread_mutex_lock(&g_camenv_attr.avserver_pthread_lock);
		if (g_camenv_attr.online_user_num > 0)
			g_camenv_attr.online_user_num--;
		pthread_mutex_unlock(&g_camenv_attr.avserver_pthread_lock);
		DBG("user leave total[%d] No %d avindex %d\n", g_camenv_attr.online_user_num, iGNo, avIndex);
	}

	if(!isMaster){
		g_camenv_attr.online_guest_num--;
		if(g_camenv_attr.online_guest_num < 0)
			g_camenv_attr.online_guest_num = 0;
	}
	DBG("guest_online_num = %d\n", g_camenv_attr.online_guest_num);
	return 0;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int SID;
	
	Cam_Signals_Init();

	if(argc >1 && strlen(argv[1]))
		ipc_log_init(argv[1]);
	
	DBG("###Cam Stream Start###\n");
	Cam_Attr_Init();
	Cam_Media_Init();

	IOTC_Set_Max_Session_Number(MAX_CLIENT_NUMBER);
	ret = IOTC_Initialize2(0);
	if(ret != IOTC_ER_NoERROR)
	{
		Cam_Media_UnInit();
		return -1;
	}

	// Versoin of IOTCAPIs & AVAPIs
	unsigned int iotcVer;
	IOTC_Get_Version(&iotcVer);
	int avVer = avGetAVApiVer();
	unsigned char *p = (unsigned char *)&iotcVer;
	unsigned char *p2 = (unsigned char *)&avVer;
	char szIOTCVer[16], szAVVer[16];
	sprintf(szIOTCVer, "%d.%d.%d.%d", p[3], p[2], p[1], p[0]);
	sprintf(szAVVer, "%d.%d.%d.%d", p2[3], p2[2], p2[1], p2[0]);
	DBG("IOTCAPI version[%s] AVAPI version[%s]\n", szIOTCVer, szAVVer);

	IOTC_Get_Login_Info_ByCallBackFn(LoginInfoCB);

	avInitialize(MAX_CLIENT_NUMBER*2);
	// create thread to login because without WAN still can work on LAN
	pthread_t Login_pid;
	ret = pthread_create(&Login_pid, NULL, &Login_Tutk_Proc, (void *)g_camenv_attr.cam_uid);
	CHECK(!ret, -1, "Login_Tutk_Proc create fail, ret=[%d]\n", ret);	
	pthread_detach(Login_pid);

	while(1)
	{
		int *sid = NULL;
		// Accept connection only when IOTC_Listen() calling
		SID = IOTC_Listen(0);
		if(SID < 0){
			Print_Err_Handling (SID);
			if(SID == IOTC_ER_EXCEED_MAX_SESSION){
				sleep(5);
			}
			continue;
		}

		sid = (int *)malloc(sizeof(int));
		*sid = SID;
		pthread_t AVServer_pid;
		int ret = pthread_create(&AVServer_pid, NULL, &AVServerStart_Proc, (void *)sid);
		if(ret < 0)
			DBG("AVServerStart_Proc create failed ret[%d]\n", ret);
		else{
			pthread_detach(AVServer_pid);
		}
	}

	Cam_Media_UnInit();
	IOTC_DeInitialize();
	
	return 0;
}
