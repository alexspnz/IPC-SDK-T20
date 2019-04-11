/*
 * ipc_debug.h
 * Author:yuanhao@yingshixun.com
 * Date:2019.1
 */

#ifndef __IPC_DEBUG_H__
#define __IPC_DEBUG_H__

typedef enum _IPC_LOG_LEVEL
{
	IPC_LOG_EMERG = 0,
	IPC_LOG_ERR,
	IPC_LOG_WARN,
	IPC_LOG_INFO,
	IPC_LOG_DEBUG,
	IPC_LOG_BTN = 0xFF,
    
}IPC_LOG_LEVEL;

#define NONE "\033[m"
#define RED "\033[0;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[0;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[0;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;33m"
#define LIGHT_GRAY "\033[0;37m"
#define WHITE "\033[1;37m"
#define SIZE (1024)

#ifndef IPC_DBG
#define printf(__fmt, ...)  ipc_printf(IPC_LOG_DEBUG,"[%s: %d]: "__fmt"", __func__, __LINE__, ##__VA_ARGS__)
#endif

#define DBG(fmt, ...) do\
{\
    ipc_printf(IPC_LOG_DEBUG,"[%s: %d]: "fmt"", __func__, __LINE__, ##__VA_ARGS__);\
}while(0)

#define ERR(fmt, ...) do\
{\
    ipc_printf(IPC_LOG_ERR,"[%s: %d]: "fmt"", __func__, __LINE__, ##__VA_ARGS__);\
}while(0)

#define WRN(fmt, ...) do\
{\
    ipc_printf(IPC_LOG_WARN,"[%s: %d]: "fmt"", __func__, __LINE__, ##__VA_ARGS__);\
}while(0) 

#define INFO(fmt, ...) do\
{\
    ipc_printf(IPC_LOG_INFO,"[%s: %d]: "fmt"", __func__, __LINE__, ##__VA_ARGS__);\
}while(0) 

#define CHECK(exp, ret, fmt...) do\
{\
    if (!(exp))\
    {\
        ERR(fmt);\
        return ret;\
    }\
}while(0)

#define ASSERT(exp, fmt...) do\
{\
    if (!(exp))\
    {\
        ERR(fmt);\
        assert(exp);\
    }\
}while(0)

int  ipc_log_init(const char *logname);
int  ipc_log_exit();
int  ipc_log_level(IPC_LOG_LEVEL ilevel); 
int  ipc_printf(IPC_LOG_LEVEL level,const char* format, ...);

#endif
