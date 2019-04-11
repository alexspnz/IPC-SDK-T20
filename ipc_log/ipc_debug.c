/*
 * ipc_debug.c
 * Author:yuanhao@yingshixun.com
 * Date:2019.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>   
#include <fcntl.h>    
#include <sys/select.h>    
#include <sys/ioctl.h>

#include "ipc_debug.h"

static char text[1152];
static int  _log_level = IPC_LOG_DEBUG;
static int  dprint = 0;

static void ipc_debug_timestr(char *out)
{
	if(NULL == out)
	{
		return ;
	}
    struct timeval tv;
    struct tm *ptm = NULL;
    gettimeofday(&tv, NULL);
    ptm = localtime(&tv.tv_sec);
	if(NULL == ptm)
	{	
		return ;
	}
	sprintf(out,"%04d_%02d_%02d_%02d_%02d_%02d",
            (1900 + ptm->tm_year),(1 + ptm->tm_mon),
            ptm->tm_mday,ptm->tm_hour,
            ptm->tm_min,ptm->tm_sec);
    return ;
}

int ipc_log_init(const char *logname)
{
    int ret = 0;
    if(NULL == logname)
    {
        logname = "ipc_app";
    }    
    openlog(logname, LOG_PID|LOG_CONS, LOG_USER); //LOG_CONS|LOG_PERROR
    if(ret != 0)
    {
        perror("open syslog error.");
        return -1;
    } 
    if(access("/etc/syslog.conf",F_OK))
    {
        dprint = 1;
    }    
    return 0;
}    

int ipc_printf(IPC_LOG_LEVEL level,const char* format,...)
{
    va_list ap;
	int priority = 0;
    va_start(ap, format);
    memset(text , 0x00, sizeof(text));
    vsnprintf(text, sizeof(text), format, ap);
    va_end(ap);
    
	switch(level)
	{
    	case IPC_LOG_EMERG:
    	{
    		priority = LOG_EMERG;
    	}break;
    	case IPC_LOG_ERR:
    	{
    		priority = LOG_ERR;
    	}break;
    	case IPC_LOG_WARN:
    	{
    		priority = LOG_WARNING;
    	}break;
    	case IPC_LOG_INFO:
    	{
    		priority = LOG_INFO;
    	}break;
    	case IPC_LOG_DEBUG:
    	{
    		priority = LOG_DEBUG;
    	}break;
    	default:
        {
    		priority = LOG_DEBUG;
        }
    	break;
	} 
    
    if(dprint)
    {
        fprintf(stderr,"%s",text);       
    }   
    
    if(level <= _log_level)
    {
        syslog(priority,"%s",text);
    }    
    
	return 0;
}

int ipc_log_level(IPC_LOG_LEVEL ilevel)
{
    if(IPC_LOG_BTN != ilevel)
    {
        _log_level = ilevel;
    } 
    return _log_level;
}

int ipc_log_exit()
{
    int ret = 0;
    closelog(); 
    if(ret != 0)
    {
        perror("close syslog error.");
        return -1;
    } 
    return 0;
}

