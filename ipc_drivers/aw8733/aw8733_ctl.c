#include <linux/init.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <jz_proc.h>
#include <soc/gpio.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/mutex.h>

#include "aw8733_ctl.h"

#define ADC_CH0_MAJOR                   95
#define MAX_ADC_MINORS                  1
#define DRV_CMD_AW8733A 				0

static int mode_gpio = GPIO_PB(31);
module_param(mode_gpio, int, S_IRUGO);
MODULE_PARM_DESC(mode_gpio, "AW8733 GPIO NUM");

#define TOFF_TIME	550 //us
#define TH_TIME		2 //us
#define TL_TIME		2 //us
#define PACTL_PRINTK(fmt, args...) printk("[%s %d]"fmt"\n", __FUNCTION__, __LINE__, ##args)

static int aw8733a_switch_mode(unsigned long arg)
{
    unsigned int i = 0;

	if (arg == 0){
		gpio_direction_output(mode_gpio, 0);
		udelay(TOFF_TIME);
		printk("Close Aw8733!\n");
	} else if (arg > 0 && arg <= 4) {
		for (i = 0; i < arg; i++){
			gpio_direction_output(mode_gpio, 0);
			udelay(TL_TIME);
			gpio_direction_output(mode_gpio, 1);
			udelay(TH_TIME);
		}
		PACTL_PRINTK("Set Aw8733 mode%d OK!\n",arg);
	} else {
		PACTL_PRINTK("error mode num %d,please input 0~4\n",arg);
	}

    return 0;
}

static long aw8733_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
        case DRV_CMD_AW8733A:
        {
            aw8733a_switch_mode(arg);
            break;
        }
        default:
        {
            PACTL_PRINTK("Unknown cmd %d\n", cmd);
            break;
        }
    }

    return 0;
}

static int aw8733_open(struct inode *inode, struct file *filp)
{
    int minor = MINOR(inode->i_rdev);

    if (minor >= MAX_ADC_MINORS)
        return -ENODEV;

    filp->private_data = (void *)minor;

    return 0;
}

static int aw8733_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations aw8733_fops = {
        .unlocked_ioctl = aw8733_ioctl,
        .open           = aw8733_open,
        .release        = aw8733_release,
        .owner          = THIS_MODULE,
};

static struct cdev aw8733_cdev = {
        .kobj   =   {.name = "ADC_CH0", },
        .owner  =   THIS_MODULE,
};

static struct class *sys_class = NULL;
static dev_t aw8733_devno;
static int __init aw8733_init(void)
{
    PACTL_PRINTK("aw8733 init start.\n");

    aw8733_devno = MKDEV(ADC_CH0_MAJOR, 0);

    if(register_chrdev_region(aw8733_devno, MAX_ADC_MINORS, "ADC_CH0"))
        goto error;

    cdev_init(&aw8733_cdev, &aw8733_fops);
    if (cdev_add(&aw8733_cdev, aw8733_devno, MAX_ADC_MINORS))
    {
        kobject_put(&aw8733_cdev.kobj);
        unregister_chrdev_region(aw8733_devno, MAX_ADC_MINORS);
        goto error;
    }

	int ret = gpio_request(mode_gpio,"aw8733_mode");
	if(ret < 0){
		printk("gpio requrest fail GPIO_P%c(%d)\n",('A' + mode_gpio / 32),mode_gpio % 32);
		return;
	}

#define DEV_NAME "adc"
    sys_class = class_create(THIS_MODULE, DEV_NAME);
    device_create(sys_class, NULL, aw8733_devno, NULL, DEV_NAME);

    PACTL_PRINTK("insert aw8733.ko\n");

error:
    return 0;
}

static void aw8733_exit(void)
{
	gpio_free(mode_gpio);
    device_destroy(sys_class, aw8733_devno);
    class_destroy(sys_class);

    cdev_del(&aw8733_cdev);
    unregister_chrdev_region(aw8733_devno, MAX_ADC_MINORS);
    PACTL_PRINTK("remove aw8733.ko\n");

    return;
}

module_init(aw8733_init);
module_exit(aw8733_exit);

MODULE_LICENSE("GPL");

