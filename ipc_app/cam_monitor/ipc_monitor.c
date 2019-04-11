
#include "ipc_monitor.h"
#include "ipc_debug.h"
#include "ipc_util.h"
 

extern IPC_MONITOR_INFO_S monitor_info;

int monitor_get_bind_status()
{ 
    FILE *fp = fopen(BIND_INFO,"rb");
    if(NULL == fp){
        return -1;
    } 

    int val = 0;
    fscanf(fp,"%1d",&val);
    fclose(fp);

    return val;
}    

int monitor_set_bind_status(int status)
{
    FILE *fp = fopen(BIND_INFO,"w+");
    if(NULL == fp){
        return -1;
    }     
    fprintf(fp,"%d",status);
    fclose(fp);
    
    monitor_info.dev_bind_status = status;
    
    return 0;
}   

static int check_process_zombie(char *pstate)
{
    if(NULL == pstate){
        return 0;
    }
	
    FILE *fp = fopen(pstate,"r");
    if(NULL == fp){
        DBG("open process file(%s) failed.\n",pstate);
        return 0;
    }
	
    char buf[1024] = {0};
    fread(buf,1,sizeof(buf),fp);
    if(strstr(buf,"zombie")){
        fclose(fp);
        return 1;
    }    
    fclose(fp);
    return 0;
}    

int monitor_get_pid_byname()
{
    FILE *fp = NULL;
    char cmd[64] = {0};
    char buf[1024] = {0};
	
	sprintf(cmd,"pgrep -lo %s", monitor_info.app_name);
    if((fp = popen(cmd, "r")) == NULL){
        return 0;
    }
    while(fgets(buf, sizeof(buf), fp) != NULL){
		if(strstr(buf, monitor_info.app_name)){
            char tmp[32] = {0};
            sscanf(buf,"%8s %8s", monitor_info.process_id, tmp);  
		    pclose(fp);
            fp = NULL;
			return atoi(monitor_info.process_id);
		}
    }
    pclose(fp);
    fp = NULL;
    return 0;
}

static void monitor_app_restart()
{
	if(strlen(monitor_info.app_name)){
		char cmd[256] = {0};
		sprintf(cmd,"%s &", monitor_info.app_name);
		DBG("###cmd:%s###\n",cmd);
		util_system(cmd);

		while(monitor_info.child_running){ 
			if(monitor_get_pid_byname()){
				DBG("### %s pid:%s###\n", monitor_info.app_name, monitor_info.process_id);
				break;
			}
			usleep(100);
		}
	}
}

static void monitor_app_start(void)
{
	if(strlen(monitor_info.app_name)){
		char cmd[256] = {0};
		sprintf(cmd,"%s &", monitor_info.app_name);
		DBG("###cmd:%s###\n",cmd);
		util_system(cmd);

		while(monitor_info.child_running){ 
			if(monitor_get_pid_byname()){
				DBG("### %s pid:%s###\n", monitor_info.app_name, monitor_info.process_id);
				break;
			}
			usleep(100);
		}
	}
}

int mk_check_task()
{
	if(strlen(monitor_info.app_name))
	{
		char pstate[64] = {0};
		sprintf(pstate,"/proc/%s/status", monitor_info.process_id);
		if(access(pstate,F_OK) != 0){
			if(access("/etc/sysupdate",F_OK) != 0){    
				DBG("%s start.\n", monitor_info.app_name);
				monitor_app_restart();
			}   
		}    
		else if(check_process_zombie(pstate)){
			util_system("reboot");
		}
	}	

	return 0;
}

void sigint(int dummy)
{
	DBG("Monitor got int signal,exit!\n");
}

void monitor_signal_init(void)
{
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGTERM);
	sigaddset(&sa.sa_mask, SIGINT);

	sa.sa_handler = sigint;
	sigaction(SIGTERM, &sa, NULL);

	sa.sa_handler = sigint;
	sigaction(SIGINT, &sa, NULL);

	signal(SIGPIPE, SIG_IGN);
}

void *sync_time_proc(void *arg)
{
	prctl(PR_SET_NAME, __FUNCTION__);
    char cmd[256];

    while(monitor_info.main_running){
        if (monitor_info.dev_connect_status == IPC_ONLINE){
            memset(cmd, 0, sizeof(cmd));
            snprintf(cmd, sizeof(cmd), "ntpc &");
            DBG("run cmd: %s\n", cmd);
            util_system(cmd);
        }

        sleep(10*60);
    }   
    
}

static void monitor_app_stop()
{
	if(strlen(monitor_info.app_name)){
		char cmd[32] = {0};
		sprintf(cmd,"killall %s", monitor_info.app_name);
		util_system(cmd);
	}

	monitor_info.app_work_status = APP_NOT_WORK;
}

void *wait_bind_proc(void *arg)
{
	prctl(PR_SET_NAME, __FUNCTION__);

	while(monitor_info.child_running){
		if(access(CONF_STA,F_OK) == 0){
			break;
		}
		else{
			usleep(500*1000);
		}
	}
	return NULL;
}

void *status_control_proc(void *arg)
{
	prctl(PR_SET_NAME, __FUNCTION__);
	int ret;

	if(monitor_info.dev_bind_status != DEV_BIND_OK){
		pthread_t WaitBind;
        if((ret = pthread_create(&WaitBind, NULL, &wait_bind_proc, NULL)))
        { 
            DBG("WaitBind pthread_create ret=%d\n", ret);
            pthread_exit(NULL);
        } 
        pthread_join(WaitBind, 0);
	}

    pthread_t ConnectNetworK;
    if((ret = pthread_create(&ConnectNetworK, NULL, &monitor_connect_network, NULL)))
    { 
        DBG("ConnectNetworK pthread_create ret=%d\n", ret);
        pthread_exit(NULL);
    } 
    pthread_detach(ConnectNetworK);
		
    while(monitor_info.child_running){		
        if(monitor_info.dev_connect_status == IPC_CONNECT_OK){
            monitor_info.dev_connect_status = IPC_ONLINE;
            mk_app_start();
            monitor_info.app_work_status = APP_IS_WORK;
        }
        
        usleep(500*1000);
    }
    return NULL;
}

static void monitor_ipc_init()
{
    monitor_info.dev_bind_status = monitor_get_bind_status();
    monitor_info.dev_connect_status = IPC_CONNECT_INIT;
    monitor_info.app_work_status= APP_NOT_WORK;
}

void *ap_access_proc(void *arg)
{
	while(thread_loop_child)
	{
		char buf[256]= {'\0'};
		FILE *fp = popen("hostapd_cli all_sta | grep -v interface", "r");
    	if(fp != NULL)
		{
			int len = fread(buf, sizeof(char), 256, fp);
			if(len > 0 && mkipc_env.app_status == APP_NOT_WORK)
			{
				mkprintf(MK_LOG_INFO, "access a point, start mkipc\n");
				mkipc_env.app_status = APP_IS_WORK;
				memset(mkipc_env.app_name[1], 0 ,sizeof(mkipc_env.app_name[1]));
				mk_app_start();
				led_ctrl(SAL_LED_COLOUR_RED, SAL_LED_MODE_OFF);
				led_ctrl(SAL_LED_COLOUR_RED, SAL_LED_MODE_ON);
			}
			if(len == 0 && mkipc_env.app_status == APP_IS_WORK)
			{
				mkprintf(MK_LOG_INFO, "none access point, stop mkipc\n");
				mk_app_stop();
			}
		}
    	pclose(fp);
    	fp = NULL;
		usleep(500*1000);
	}
	return NULL;
}

void mk_ipc_reset(int mode)
{
    thread_loop_child = HI_FALSE;
	strcpy(mkipc_env.app_name[1], "web_server");
    mk_app_stop();
    mk_qrscan_stop();
    if(mode)
    {
		#ifdef MQ2
		system("buzzer 50 0 &");
		mk_eth0_stop();
		mk_eth0_start();
		#else
		sal_opus_play_by_type(AUDIO_TYPE_RESET);
		#endif
    }
	mk_set_bindflag(DEV_NOT_BIND);
	mk_ipc_init();
    remove(CONF_STA);
    remove(USER_INFO);
	sem_post(&mkipc_env.sem);  //exit status_control_proc
    mk_conf_file_reset();    
    mkprintf(MK_LOG_EMERG,"system reset now.\n");
	sal_devconf_setint("ap_mode", 0);
#ifndef MQ4
    mk_wlan0_stop(); 
	mk_wifiAP_stop();
#endif
    int ret;
    thread_loop_child = HI_TRUE;
    pthread_t StatusControl_reset;
    if((ret = pthread_create(&StatusControl_reset, NULL, &status_control_proc, NULL)))
    { 
        mkprintf(MK_LOG_ERR,"StatusControl_reset pthread_create ret=%d\n", ret);
        return;
    } 
    pthread_detach(StatusControl_reset);
}

static void ipc_monitor_init()
{
	memset(&monitor_info, 0, sizeof(IPC_MONITOR_INFO_S);
	monitor_info.dev_bind_status = monitor_get_bind_status();
	monitor_info.main_running = 1;
	monitor_info.child_running = 1;
}

static int mk_reset_button(void)
{

}

int main(int argc,char *argv[])
{
	CHECK(argc >= 2, -1, "monitor findn't app\n");

	int ret;
	ipc_monitor_init();
	CHECK(monitor_info, -1, "monitor init error\n");
	strncpy(monitor_info.app_name, argv[1], APP_NAME_LEN);
    monitor_signal_init();
		
	if((ret = pthread_create(&monitor_info.StatusControl, NULL, &status_control_proc, NULL))){ 
   	 	DBG("StatusControl pthread_create ret=%d\n", ret);
    	return -1;
	} 
	pthread_detach(monitor_info.StatusControl);
	
    if((ret = pthread_create(&monitor_info.SyncTime, NULL, &sync_time_proc, NULL))){ 
        DBG("SyncTime pthread_create ret=%d\n", ret);
        return -1;
    } 
    pthread_detach(monitor_info.SyncTime);

    if((ret = pthread_create(&monitor_info.NetworkStatus, NULL, &network_status_proc, NULL))){ 
        DBG("NetworkStatus pthread_create ret=%d\n", ret);
        return -1;
    } 
    pthread_detach(monitor_info.NetworkStatus);
	
	while(monitor_info.main_running){
		usleep(100*1000);
 	}

    mk_qrscan_stop();
    mk_eth0_stop();
    mk_wlan0_stop();
    mk_wifiAP_stop();
    mk_app_stop();
	
	return 0;
}
