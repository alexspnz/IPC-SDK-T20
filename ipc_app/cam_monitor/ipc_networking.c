#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>   
#include <fcntl.h>    
#include <sys/select.h>    
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <net/if.h>

#include "../include/mk_dbg.h"
#include "../config/sal_devconf.h"
#include "../common/sal_util.h"
#include "../debug/sal_debug.h"
#include "../net/sal_curl.h"
#include "../sal/sal_audio.h"

#include "mk_func.h"
#include "mk_networking.h"

DeviceEnv mkipc_env;

int mk_check_network_mode(void)
{
#ifdef MQ4
	return LTE_MDOE;
#else
    struct ifreq ifr;
    int skfd = socket(AF_INET, SOCK_DGRAM, 0);

    strcpy(ifr.ifr_name, "eth0");
    if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
    {
        close(skfd);
        return  WLAN0_MODE;
    }
    if(ifr.ifr_flags & IFF_RUNNING)
    {
        close(skfd);
        return ETH0_MODE;
    }
    close(skfd);
    return WLAN0_MODE;
#endif
}

int mk_gethttpcode(char* input)
{
    if(strstr(input,"200 OK"))
    {
        return 1;
    }    
    return 0;
} 

int mk_curl_internet(char *url)  
{   
    handle g_hndCurlWrapper = NULL;
    
    CURL_STATUS_S stStatus;
    memset(&stStatus, 0, sizeof(stStatus));
    
    g_hndCurlWrapper = curl_wrapper_init(2*1000);

    curl_wrapper_StartDownload2Mem(g_hndCurlWrapper, url, NULL, NULL);

    while(1)
    {
        curl_wrapper_GetProgress(g_hndCurlWrapper, &stStatus);
        //printf("[%s:%d] DownloadProgress(): %d\n", __func__, __LINE__, stStatus.u32Progress);
        if (!stStatus.bRunning)
        {
            break;
        }

        usleep(1000*1000);

    }

    if (stStatus.enResult == CURL_RESULT_OK && mk_gethttpcode((char*)stStatus.pu8Recv))
    {
        //printf("[%s:%d] Download OK!!!\n", __func__, __LINE__);
        curl_wrapper_Stop(g_hndCurlWrapper);
        curl_wrapper_destroy(g_hndCurlWrapper);
        return 0;
    }
    else
    {
        printf("[%s:%d] Download %s  Fail!!!\n", __func__, __LINE__, url);
        curl_wrapper_Stop(g_hndCurlWrapper);
        curl_wrapper_destroy(g_hndCurlWrapper);
        return -1;
    }
}

int mk_connect_internet()
{
	char url[128];
	memset(url, 0, sizeof(url));
	sprintf(url, "www.baidu.com");
	if(mk_curl_internet(url) == 0)
		return 0;
	memset(url, 0, sizeof(url));
	sprintf(url, "www.qq.com");
	if(mk_curl_internet(url) == 0)
		return 0;
	memset(url, 0, sizeof(url));
	sprintf(url, "www.quora.com");
	if(mk_curl_internet(url) == 0)
		return 0;
	memset(url, 0, sizeof(url));
	sprintf(url, "www.stackoverflow.com");
	if(mk_curl_internet(url) == 0)
		return 0;
	return -1;
}

void *detect_connect_routes(void *arg)
{
	prctl(PR_SET_NAME, __FUNCTION__);
    char gw[16];
    char cmd[128];
    int ret = 0;

    FILE *gw_fp = fopen("/etc/router_ip","r");
    if(gw_fp != NULL)
    {
        memset(gw, 0, sizeof(gw));
        fread(gw, 16, 1, gw_fp);
        mkprintf(MK_LOG_INFO,"###gw_ip:%s###\n", gw);
        fclose(gw_fp);
    }
   
    while(1)
    {  
        ret = mk_connect_internet();  
        if(ret == 0)
        {
            printf("[%s:%d] CONNECT OK!\n",__func__,__LINE__);
            break;
        }
        else{
            printf("[%s:%d] CONNECT ERR!\n",__func__,__LINE__);
			if(mkipc_env.current_network_mode == WLAN0_MODE)
		 	{
            	system("ifconfig wlan0 down");
            	system("ifconfig wlan0 up");
			}
			else if(mkipc_env.current_network_mode == ETH0_MODE || mkipc_env.current_network_mode == LTE_MDOE)
			{
            	system("ifconfig eth0 down");
            	system("ifconfig eth0 up");
		 	}
            memset(cmd, 0, sizeof(cmd));
            sprintf(cmd,"route add default gw %s", gw);
            system(cmd);
        }

        sleep(2);
    }
    return NULL;
}

void *network_status_proc(void *arg)
{
	prctl(PR_SET_NAME, __FUNCTION__);
    int ret = 0;   
	int ret_last = 0;
    while(1)
    { 
    	sleep(3);
       	if(mkipc_env.app_status == APP_NOT_WORK || sal_devconf_getint("ap_mode") == 1)
            continue;
		ret = mk_connect_internet();
        if(ret == 0 && ret_last !=ret)
        {
			ret_last = ret;
			#ifdef MQ2
            led_ctrl(SAL_LED_COLOUR_RED, SAL_LED_MODE_OFF);
			led_ctrl(SAL_LED_COLOUR_RED, SAL_LED_MODE_ON);
			#else
			mk_led_off();              
            led_ctrl(SAL_LED_COLOUR_BLUE, SAL_LED_MODE_ON);    
			#endif        
        }
        else if(mkipc_env.app_status == APP_IS_WORK && ret == -1 && ret_last != ret)
        {
        	sleep(1);
			if(mk_connect_internet() == 0)
				continue;
			ret_last = ret;
			#ifdef MQ2
            led_ctrl(SAL_LED_COLOUR_RED, SAL_LED_MODE_OFF);
			led_ctrl(SAL_LED_COLOUR_RED, SAL_LED_MODE_QUICKFLASH);
			#else
			mk_led_off();
            led_ctrl(SAL_LED_COLOUR_RED, SAL_LED_MODE_FLASH);
			#endif
    	}
    }
}

void mk_4g_lte_start(void)
{
	if(access("/lib/modules/GobiNet.ko", F_OK) || access("/usr/bin/quectel-CM", F_OK))
	{
		mkprintf(MK_LOG_ERR, "GobiNet or quectel-CM not find, exit !\n");
		return;
	}
	system("insmod /lib/modules/GobiNet.ko");
	while(access("/dev/qcqmi0", F_OK))
	{
		usleep(10*1000);
	}
	system("/usr/bin/quectel-CM &");
}

void mk_4g_lte_stop(void)
{
	system("killall quectel-CM");
	system("killall udhcpc");
	system("rmmod GobiNet");
}

void mk_wlan0_start()
{
    system("insmod /lib/modules/8188eu.ko");
    usleep (100000);
    system("ifconfig wlan0 up");
    system("ifconfig wlan0 0.0.0.0");
	#ifndef MQ2
    mk_led_off();
    led_ctrl(SAL_LED_COLOUR_BLUE, SAL_LED_MODE_FLASH); 
	#endif
    system("wpa_supplicant -i wlan0 -Dwext -c /etc/wpa_supplicant.conf -B");
    system("udhcpc -i wlan0 -s /etc/udhcpc.script &"); 
}

void mk_wlan0_stop()
{
    system("killall -9 wpa_supplicant");
    system("killall -9 udhcpc");
    system("ifconfig wlan0 down");
    system("rmmod 8188eu");
    return;
}

void mk_wifiAP_stop()
{
	system("killall -9 hostapd");
    system("killall -9 udhcpd");
	remove(CONF_AP_TMP);
	system("ifconfig wlan0 down");
    system("rmmod 8188eu");
}

void mk_wifiAP_start()
{
	char sys_cmd[128];
	char ssid[64];
	sscanf(mkipc_env.dev_uid+5, "%12s", ssid);
    memset(sys_cmd, 0x0, 128);
    sprintf(sys_cmd,"sed 's/MKTECH_IPC/%s/g' %s > %s", ssid, CONF_AP, CONF_AP_TMP);
	system(sys_cmd);

	//system("mkdir -p /var/run/hostapd");
	system("insmod /lib/modules/8188eu.ko");
	system("ifconfig wlan0 up");
	system("ifconfig wlan0 192.168.1.1 netmask 255.255.255.0");
	system("hostapd /etc/hostapd_tmp.conf -B");
    system("udhcpd /etc/udhcpd.conf &");
}

void mk_eth0_start()
{ 
    char buf[64]= {0};
	int ret = 0;
	FILE *fp = popen("nvram_get X_STEPS", "r");
    if(fp != NULL)
	{
		fread(buf, sizeof(char), 64, fp);
		if(buf != NULL)
		{
			if(strcmp(buf, "1") == 0)
				ret = 1;
		}
	}
    pclose(fp);
    fp = NULL;
	
	system("ifconfig eth0 up");
    system("killall -9 udhcpc");
    if(ret == 1)  //prod test OK,start dhcp
    {
        system("udhcpc -i eth0 -s /etc/udhcpc.script &");
    }
    else
    {
        system("ifconfig eth0 192.168.1.66 netmask 255.255.255.0");
        system("route add default gw 192.168.1.1");
    }    
}

void mk_eth0_stop(void)
{
    system("ifconfig eth0 0.0.0.0");
    system("ifconfig eth0 down");
    system("killall -9 udhcpc");
}

void mk_proc_recycling()
{
	int proc_id;
	for(proc_id = 0; proc_id < sizeof(mkipc_env.app_name)/sizeof(mkipc_env.app_name[0]); proc_id++)
	{
		if(strlen(mkipc_env.app_name[proc_id]))
		{
			char cmd[32];
			sprintf(cmd,"killall -9 %s", mkipc_env.app_name[proc_id]);
			system(cmd);
		}
	}
}

void *mk_connect_network(void *arg)
{
	prctl(PR_SET_NAME, __FUNCTION__);
	#ifdef MQ2
    system("ifconfig eth0 up");
	sleep(3);
	#endif
    mkipc_env.current_network_mode = mk_check_network_mode();
    if(mkipc_env.current_network_mode == ETH0_MODE)
    {
        mk_wlan0_stop();
        mk_eth0_start();
    }
    else if(mkipc_env.current_network_mode == WLAN0_MODE)
	{
        mk_eth0_stop();
        mk_wlan0_start();
    }
    
    mkipc_env.dev_connect_status = DEV_CONNECT_INIT;     
    char cmd[128];
    int time = 0;
    int connt = 0; 
    int ret;
    int flag = 1;

    while(
#ifdef MQ4
    		access("/dev/qcqmi0", F_OK) == 0
#else
    		access(CONF_STA,F_OK) == 0
#endif
			)
    {
        if((mkipc_env.dev_connect_status == DEV_CONNECT_INIT)&&(time % 5 == 0))
        {
            memset(cmd, 0, sizeof(cmd));
            if(mkipc_env.current_network_mode == ETH0_MODE || mkipc_env.current_network_mode == LTE_MDOE)
                sprintf(cmd, "ifconfig %s", "eth0");
            else
                sprintf(cmd, "ifconfig %s", "wlan0");
            mkprintf(MK_LOG_INFO,"check ipaddr .\n");
            if(mk_popen_tag(cmd,"inet addr:"))
            {
                mkipc_env.dev_connect_status = DEV_CONNECT_OK;
                pthread_t CheckRoutes;
                if((ret = pthread_create(&CheckRoutes, NULL, &detect_connect_routes, NULL)))
                { 
                    mkprintf(MK_LOG_ERR,"Detect_connect_routes pthread_create ret=%d\n", ret);
                    pthread_exit(NULL);
                } 
                pthread_join(CheckRoutes, 0);
                if(mkipc_env.app_status== APP_NOT_WORK)       //app正在运行时，无需触发
                    sem_post(&mkipc_env.sem);
                if(mkipc_env.dev_bind_status != DEV_BIND_OK)  //第一次绑定修改绑定标志   
                    mk_set_bindflag(DEV_BIND_OK);
                mk_led_off();
                led_ctrl(SAL_LED_COLOUR_BLUE, SAL_LED_MODE_ON); 
			
                flag = 1;
            }
            else{
                mkprintf(MK_LOG_ERR,"wifi conning ...time = %d\n",connt);
                connt ++;
                if(connt > 3 && (time % 60 == 0))
                {
                    mkipc_env.dev_connect_status = DEV_CONNECT_FAIL;
                }
            }
        }

        if(mkipc_env.dev_connect_status == DEV_CONNECT_FAIL)
        { 
            mkipc_env.dev_connect_status = DEV_CONNECT_INIT;  
            if(flag)
            {
                flag = 0;
				#ifdef MQ2
                led_ctrl(SAL_LED_COLOUR_RED, SAL_LED_MODE_OFF);
                led_ctrl(SAL_LED_COLOUR_RED, SAL_LED_MODE_QUICKFLASH);
				system("buzzer 50 0 &");
				#else
                mk_led_off();
                led_ctrl(SAL_LED_COLOUR_RED, SAL_LED_MODE_FLASH);
                sal_opus_play_by_type(AUDIO_TYPE_CONN_WIFI_FAILE);
				#endif
            }
            connt = 0; 
            if(mkipc_env.dev_bind_status != DEV_BIND_OK)
            {
                sleep(2);
                mk_ipc_reset(0);
            }
        }

		#ifdef MQ2
        if((mkipc_env.dev_connect_status == DEV_CONNECT_OK || mkipc_env.dev_connect_status == DEV_ONLINE)&&(time % 4 == 0))
        {   
            mkipc_env.last_network_mode = mkipc_env.current_network_mode;
			system("ifconfig eth0 up");
            mkipc_env.current_network_mode = mk_check_network_mode();

            if(mkipc_env.current_network_mode != mkipc_env.last_network_mode)
            {
				mk_proc_recycling();
				mkipc_env.app_status = APP_NOT_WORK;
                mkipc_env.dev_connect_status = DEV_CONNECT_INIT;     
                if(mkipc_env.current_network_mode == ETH0_MODE)
                {
                    mk_wlan0_stop();
                    mk_eth0_start();
                }
                else{
                    mk_eth0_stop();
                    mk_wlan0_start();
                }
            }
        }
		#endif

        usleep(500*1000);
        time ++;
        if(time > 60)
            time = 0;
    }
    return NULL;
}
