
#ifndef __IPC_IRCUT_H__
#define __IPC_IRCUT_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#define IR_LED     90
#define IRCUT_P    72 
#define IRCUT_N    73
#define ADC_CHN    0

#define NIGHT_THRESHOLD		40
#define DAY_THRESHOLD		800

typedef enum
{
	NIGHT_MODE,
    DAY_MODE,
	UNKNOW_MODE,
} DAY_NIGHT_SWITCH;

typedef enum
{
    IPC_IR_MODE_UNSUPPORT = -1,
    IPC_IR_MODE_AUTO = 0,		
    IPC_IR_MODE_ON   = 1,		
    IPC_IR_MODE_OFF  = 2,		
}IPC_IR_MODE;


int ipc_ircut_init();

int ipc_ircut_exit();

int ipc_ircut_mode_set(IPC_IR_MODE mode);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif


#endif

