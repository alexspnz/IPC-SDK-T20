/*
 * ipc_ircut.c
 * Author:yuanhao@yingshixun.com
 * Date:2019.4
 */

#include "ipc_comm.h"
#include "ipc_ircut.h"
#include "ipc_debug.h"
#include "ipc_util.h"
#include "imp_log.h"
#include "imp_common.h"
#include "imp_isp.h"
#include "ipc_video.h"

typedef struct {
	IPC_IR_MODE ir_mode;
    int running;
    pthread_t ircut_pid;
    pthread_mutex_t ircut_lock;
} IPC_IRCUT_INFO_S;

static IPC_IRCUT_INFO_S *g_ircut_args = NULL;

static int ircut_gpio_write(int pin , int value)
{
    int fd;
	char buf[128];
	
    memset(buf, 0, sizeof(buf));
    snprintf(buf, 128,"/sys/class/gpio/gpio%d/value",pin);
    fd = open(buf,O_WRONLY);
	CHECK(fd >= 0, -1, "set gpio %d direction error %s!\n", pin, strerror(errno));

    if (value == 0)
        write(fd,"0",sizeof("0"));
    else
        write(fd,"1",sizeof("1"));
    close(fd);
   
    return 0;
}

static int ipc_ircut_switch_mode(DAY_NIGHT_SWITCH enCurrentStatus)
{
    int ret = -1;
    if (enCurrentStatus == DAY_MODE){
	    ircut_gpio_write(IRCUT_N, 1);
        ircut_gpio_write(IRCUT_P, 0); 
        usleep(300*1000);
        ircut_gpio_write(IRCUT_N, 0);  // IRCUT --> open
        ircut_gpio_write(IRCUT_P, 0); 
        usleep(100*1000);
        ircut_gpio_write(IR_LED, 0);

		DBG("IMP_ISP_Tuning_SetSensorFPS mode Day, fps %d\n", SENSOR_FPS_DAY);

		if (IMP_ISP_Tuning_SetSensorFPS(SENSOR_FPS_DAY, 1) != 0) {
			DBG("IMP_ISP_Tuning_SetSensorFPS failed");
			return ;
		}	   
		if (IMP_ISP_Tuning_SetISPRunningMode(IMPISP_RUNNING_MODE_DAY) != 0) {
			DBG("IMP_ISP_Tuning_GetISPRunningMode Error");
			return ;
		}
    }
    else{
		DBG("IMP_ISP_Tuning_SetSensorFPS mode Night, fps %d\n", SENSOR_FPS_NIGHT);
		
		if (IMP_ISP_Tuning_SetSensorFPS(SENSOR_FPS_NIGHT, 1) != 0) {
			DBG("IMP_ISP_Tuning_SetSensorFPS failed");
			return -1;
		}	   
		if (IMP_ISP_Tuning_SetISPRunningMode(IMPISP_RUNNING_MODE_NIGHT) != 0) {
			DBG("IMP_ISP_Tuning_GetISPRunningMode Error");
			return -1;
		}

	    ircut_gpio_write(IRCUT_N, 0);
        ircut_gpio_write(IRCUT_P, 1);  
        usleep(300*1000);
        ircut_gpio_write(IRCUT_N, 0);  // IRCUT --> open
        ircut_gpio_write(IRCUT_P, 0);   
        usleep(100*1000);
        ircut_gpio_write(IR_LED, 1);

    }

    return 0;
}

static void* ipc_ircut_thread(void* arg)
{
    prctl(PR_SET_NAME, __FUNCTION__);

    int ret = -1; 
    DAY_NIGHT_SWITCH enCurrentStatus = UNKNOW_MODE;
    DAY_NIGHT_SWITCH enLastStatus = DAY_MODE;
    DAY_NIGHT_SWITCH enLastStatusTmp = enLastStatus;
	
    int iADCVal = 0;
    int iCnt = 0;
   
    while (g_ircut_args->running){
        if (g_ircut_args->ir_mode == IPC_IR_MODE_AUTO){
			ret = SU_ADC_GetChnValue(ADC_CHN, &iADCVal);
//			DBG("######iADCVal=%d######\n", iADCVal);
			if (ret < 0) {
				DBG("SU_ADC_GetChnValue failed !\n");
			}

			if (iADCVal < NIGHT_THRESHOLD){
				enLastStatusTmp = NIGHT_MODE;
			}
			else if (iADCVal > DAY_THRESHOLD){
				enLastStatusTmp = DAY_MODE;
			}

			if(enCurrentStatus != enLastStatusTmp){
				iCnt ++;
				if(iCnt > 10){
					enLastStatus = enLastStatusTmp;
					iCnt = 0;
				}
			}
			else if(iCnt){
				iCnt = 0;
			}
        }
        else if (g_ircut_args->ir_mode == IPC_IR_MODE_ON){
        	enLastStatus = NIGHT_MODE;
        }
        else if (g_ircut_args->ir_mode == IPC_IR_MODE_OFF){
        	enLastStatus = DAY_MODE;
        }       

        if (enLastStatus != enCurrentStatus){
			enCurrentStatus = enLastStatus;
			ret = ipc_ircut_switch_mode(enCurrentStatus);
			CHECK(ret == 0, NULL, "Error with %#x.\n", ret);
        }

        usleep(200*1000);
    }

    return NULL;
}

static int ircut_gpio_open(int pin)
{
	char buf[128];

	if(pin < 0 || pin > 96){
		printf("illegal pin number!\n");
		return -1;
	}

	memset(buf, 0, sizeof(buf));
	snprintf(buf,128,"echo %d > /sys/class/gpio/export",pin);
	util_system(buf);    

	return 0;
}

static int ircut_gpio_output(int pin)
{
	char buf[128];
	int fd;
	memset(buf, 0, sizeof(buf));	
	snprintf(buf,128,"/sys/class/gpio/gpio%d/direction",pin);
	fd = open(buf,O_RDWR);
	CHECK(fd >= 0, -1, "set gpio %d output error %s!\n", pin, strerror(errno));

	write(fd,"out",sizeof("out"));
	close(fd);

	return 0;
}

static int ircut_gpio_init()
{
	int ret;
	ret = ircut_gpio_open(IRCUT_P);
	CHECK(ret >= 0, -1, "open IRCUT_P %d faield\n",IRCUT_P);

	ret = ircut_gpio_output(IRCUT_P);
	CHECK(ret >= 0, -1, "gpio_output IRCUT_P %d faield\n",IRCUT_P);

	ret = ircut_gpio_open(IRCUT_N);
	CHECK(ret >= 0, -1, "open IRCUT_N %d faield\n",IRCUT_N);

	ret = ircut_gpio_output(IRCUT_N);
	CHECK(ret >= 0, -1, "gpio_output IRCUT_N %d faield\n",IRCUT_N);

	ret = ircut_gpio_open(IR_LED);
	CHECK(ret >= 0, -1, "open IR_LED %d faield\n",IR_LED);

	ret = ircut_gpio_output(IR_LED);
	CHECK(ret >= 0, -1, "gpio_output IR_LED %d faield\n",IR_LED);

    ircut_gpio_write(IRCUT_N, 0);  
    ircut_gpio_write(IRCUT_P, 0); 
    ircut_gpio_write(IR_LED, 0);

	return 0;
}

static int ircut_ADC_init()
{
	int ret;

    ret = SU_ADC_Init();
	CHECK(ret >= 0, -1, "SU_ADC_Init failed !\n");

    ret = SU_ADC_EnableChn(ADC_CHN);
	CHECK(ret >= 0, -1, "SU_ADC_EnableChn failed !\n");

	return 0;
}

int ipc_ircut_init(void)
{
	CHECK(NULL == g_ircut_args, -1, "reinit error, please exit first.\n");
	int ret = 0;

    g_ircut_args = (IPC_IRCUT_INFO_S *)malloc(sizeof(IPC_IRCUT_INFO_S));
    CHECK(g_ircut_args, -1, "failed to malloc %d bytes.\n", sizeof(IPC_IRCUT_INFO_S));
    memset(g_ircut_args, 0, sizeof(IPC_IRCUT_INFO_S));

    pthread_mutex_init(&g_ircut_args->ircut_lock, NULL);
    g_ircut_args->ir_mode = IPC_IR_MODE_UNSUPPORT;

    ret = ircut_gpio_init();
    CHECK(ret == 0, -1, "Error with %#x.\n", ret);

    ret = ircut_ADC_init();
    CHECK(ret == 0, -1, "Error with %#x.\n", ret);

    g_ircut_args->running = 1;
    ret = pthread_create(&g_ircut_args->ircut_pid, NULL, ipc_ircut_thread, NULL);
    CHECK(ret == 0, -1, "Error with %s.\n", strerror(errno));

    return 0;
}

int ipc_ircut_exit()
{
    CHECK(NULL != g_ircut_args, -1,"moudle is not inited.\n");

    if (g_ircut_args->running){
        g_ircut_args->running = 0;
        pthread_join(g_ircut_args->ircut_pid, NULL);
    }

	SU_ADC_DisableChn(ADC_CHN);
    SU_ADC_Exit();

    pthread_mutex_destroy(&g_ircut_args->ircut_lock);
    free(g_ircut_args);
    g_ircut_args = NULL;

    return 0;
}

int ipc_ircut_mode_set(IPC_IR_MODE mode)
{
    CHECK(NULL != g_ircut_args, -1, "moudle is not inited.\n");

    pthread_mutex_lock(&g_ircut_args->ircut_lock);
    g_ircut_args->ir_mode = mode;
    pthread_mutex_unlock(&g_ircut_args->ircut_lock);

    return 0;
}

