
#ifndef __IPC_MONITOR_H__
#define __IPC_MONITOR_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#include "ipc_comm.h"

#define APP_NAME_LEN	16
#define APP_PID_LEN		16

typedef enum ipc_net_status{
   	IPC_CONNECT_INIT   	= -1,    
    IPC_CONNECT_OK     	= 0,
    IPC_CONNECT_LAN	   	= 1,
    IPC_CONNECT_WAN    	= 2,
    IPC_ONLINE		   	= 3,	
    IPC_CONNECT_FAIL   	= 4,    
}IPC_NET_STATUS_S;  

typedef enum ipc_bind_status{
    DEV_NOT_BIND        = 0,     
    DEV_BIND_OK         = 1,      
}IPC_BIND_STATUS_S;  

typedef enum ipc_net_mode{
    WIRELESS_MODE      	= 0,          //无线模式
    WIRED_MODE       	= 1,          //有线模式
	4G_MDOE        		= 2,		  //4G模式
	AP_MODE            	= 3,          //AP模式
}IPC_NET_MODE_S;  

typedef enum app_work_status{
    APP_NOT_WORK      = 0,       
    APP_IS_WORK       = 1,       
}APP_WORK_STATUS_S; 

typedef struct ipc_monitor_info{
	char app_name[APP_NAME_LEN];
	char process_id[APP_PID_LEN];
	
    APP_WORK_STATUS_S  app_work_status;                 
    IPC_NET_MODE_S 	last_net_mode;         
    IPC_NET_MODE_S 	current_net_mode;    
	
    IPC_NET_STATUS_S  dev_connect_status;
    IPC_BIND_STATUS_S dev_bind_status;

	pthread_t StatusControl;
	pthread_t SyncTime;
	pthread_t NetworkStatus;
	
	unsigned char main_running;
	unsigned char child_running;
	
}IPC_MONITOR_INFO_S;

extern IPC_MONITOR_INFO_S monitor_info;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif


