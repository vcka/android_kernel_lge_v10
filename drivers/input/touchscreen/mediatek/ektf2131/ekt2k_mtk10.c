/* touchscreen/ektf2k_kthread_mtk.c - ELAN EKTF2K touchscreen driver
 * for MTK65xx serial platform.
 *
 * Copyright (C) 2012 Elan Microelectronics Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 2011/12/06: The first release, version 0x0001
 * 2012/2/15:  The second release, version 0x0002 for new bootcode
 * 2012/5/8:   Release version 0x0003 for china market
 *             Integrated 2 and 5 fingers driver code together and
 *             auto-mapping resolution.
 * 2012/8/24:  MTK version
 * 2013/2/1:   Release for MTK6589/6577/6575/6573/6513 Platform
 *             For MTK6575/6573/6513, please disable both of ELAN_MTK6577 and MTK6589DMA.
 *                          It will use 8+8+2 received packet protocol
 *             For MTK6577, please enable ELAN_MTK6577 and disable MTK6589DMA.
 *                          It will use Elan standard protocol (18bytes of protocol).
 *             For MTK6589, please enable both of ELAN_MTK6577 and MTK6589DMA.
 * 2013/5/15   Fixed MTK6589_DMA issue.
 */

//#define SOFTKEY_AXIS_VER
#define ELAN_TEN_FINGERS
#define MTK6589_DMA
#define ELAN_MTK6577
//#define ELAN_BUTTON
//#define TPD_HAVE_BUTTON

#ifdef ELAN_TEN_FINGERS
#define PACKET_SIZE		34 //44		/* support 10 fingers packet */
#else
//#define PACKET_SIZE		8 		/* support 2 fingers packet  */
#define PACKET_SIZE		18			/* support 5 fingers packet  */
#endif

#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
//#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/hrtimer.h>
//<2014/03/12 ShermanWei, for KnockOn wakeup
#include <linux/wakelock.h>
//>2014/03/12 ShermanWei

#include <linux/dma-mapping.h>

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#ifndef TPD_NO_GPIO
#include "cust_gpio_usage.h"
#endif

// for linux 2.6.36.3
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/ioctl.h>

//dma
#include <linux/dma-mapping.h>


//#include "ekth3250.h"
#include "tpd.h"
#include "mach/eint.h"

//#include "tpd_custom_ekth3250.h"
#include "ektf2k_mtk.h"

#include <cust_eint.h>

//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#define ELAN_DEBUG
//>2014/11/18-Y46170-Lever.Ni
//<2014/11/07-Y45861-Lever.Ni,[Y20][Feature][Common][BSP][][]add touch panel knock code function
#define KNOCK 
//>2014/11/07-Y45861-Lever.Ni
#define PWR_STATE_DEEP_SLEEP	0
#define PWR_STATE_NORMAL		1
#define PWR_STATE_MASK			BIT(3)

#define CMD_S_PKT			0x52
#define CMD_R_PKT			0x53
#define CMD_W_PKT			0x54

#define HELLO_PKT			0x55
#define FIVE_FINGERS_PKT			0x5D
#define   MTK_FINGERS_PKT                  0x6D    /** 2 Fingers: 5A 5 Fingers 5D, 10 Fingers: 62 **/

#define TWO_FINGERS_PKT      0x5A
#define MTK_FINGERS_PKT       0x6D
#define TEN_FINGERS_PKT	0x62

#define RESET_PKT			0x77
#define CALIB_PKT			0xA8


#define TPD_OK 0
//<2014/11/07-Y45861-Lever.Ni,[Y20][Feature][Common][BSP][][]add touch panel knock code function
#ifdef KNOCK
enum {
	KCODE_EN = 0,
	KCODE_FLAG = 1,
	KON_FLAG = 2,
	KSLEEP_FLAG = 3,
	LOCK_FLAG=4,
};
#define KNOCK_ON 0x54
#define KNOCK_CODE 0x5E
uint8_t knock_cmd[4] = {};
uint8_t knock_code[4] = {};
ulong knock_flag = 0;
//struct wake_lock tpKnockcode_lock;
#endif
//>2014/11/07-Y45861-Lever.Ni
//#define HAVE_TOUCH_KEY
//<2014/03/10 ShermanWei,
///#define LCT_VIRTUAL_KEY
//>2014/03/10 ShermanWei,

#ifdef MTK6589_DMA
static uint8_t *gpDMABuf_va = NULL;
static uint32_t gpDMABuf_pa = NULL;
#endif



#ifdef TPD_HAVE_BUTTON
#define TPD_KEY_COUNT           3
#define TPD_KEYS                { KEY_MENU, KEY_HOMEPAGE, KEY_BACK}
#define TPD_KEYS_DIM            {{107,1370,109,TPD_BUTTON_HEIGH},{365,1370,109,TPD_BUTTON_HEIGH},{617,1370,102,TPD_BUTTON_HEIGH}}

static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

// modify
#define SYSTEM_RESET_PIN_SR 	135

//Add these Define

#define IAP_PORTION            	1
#define PAGERETRY  30
#define IAPRESTART 5
//<2014/03/17 ShermanWei, for TP Recovery mode download
#define CMD_54001234	   0
//>2014/03/17 ShermanWei,
//<2014/06/12 ShermanWei, 2nd source TP
//<2014/12/09-Lever.Ni, [Y20][Feature][Common][BSP][][]show vendor id
  #define PRIMARY_SENSOR 0x0059 /*(3.5")*/ 
  #define SECOND_SENSOR 0x005b    /*(3.5")*/
//>2014/12/09-Lever.Ni
//>2014/06/12 ShermanWei,

// For Firmware Update 
#define ELAN_IOCTLID	0xD0
#define IOCTL_I2C_SLAVE	_IOW(ELAN_IOCTLID,  1, int)
#define IOCTL_MAJOR_FW_VER  _IOR(ELAN_IOCTLID, 2, int)
#define IOCTL_MINOR_FW_VER  _IOR(ELAN_IOCTLID, 3, int)
#define IOCTL_RESET  _IOR(ELAN_IOCTLID, 4, int)
#define IOCTL_IAP_MODE_LOCK  _IOR(ELAN_IOCTLID, 5, int)
#define IOCTL_CHECK_RECOVERY_MODE  _IOR(ELAN_IOCTLID, 6, int)
#define IOCTL_FW_VER  _IOR(ELAN_IOCTLID, 7, int)
#define IOCTL_X_RESOLUTION  _IOR(ELAN_IOCTLID, 8, int)
#define IOCTL_Y_RESOLUTION  _IOR(ELAN_IOCTLID, 9, int)
#define IOCTL_FW_ID  _IOR(ELAN_IOCTLID, 10, int)
#define IOCTL_ROUGH_CALIBRATE  _IOR(ELAN_IOCTLID, 11, int)
#define IOCTL_IAP_MODE_UNLOCK  _IOR(ELAN_IOCTLID, 12, int)
#define IOCTL_I2C_INT  _IOR(ELAN_IOCTLID, 13, int)
#define IOCTL_RESUME  _IOR(ELAN_IOCTLID, 14, int)
#define IOCTL_POWER_LOCK  _IOR(ELAN_IOCTLID, 15, int)
#define IOCTL_POWER_UNLOCK  _IOR(ELAN_IOCTLID, 16, int)
#define IOCTL_FW_UPDATE  _IOR(ELAN_IOCTLID, 17, int)
#define IOCTL_BC_VER  _IOR(ELAN_IOCTLID, 18, int)
#define IOCTL_2WIREICE  _IOR(ELAN_IOCTLID, 19, int)
#define IOCTL_GET_UPDATE_PROGREE	_IOR(CUSTOMER_IOCTLID,  2, int)


#define CUSTOMER_IOCTLID	0xA0
#define IOCTL_CIRCUIT_CHECK  _IOR(CUSTOMER_IOCTLID, 1, int)


extern struct tpd_device *tpd;

uint8_t RECOVERY=0x00;
int FW_VERSION=0x00;
//<2014/03/10 ShermanWei,

int X_RESOLUTION=704;  
int Y_RESOLUTION=960;

//>2014/03/10 ShermanWei,
int FW_ID=0x00;
int BC_VERSION = 0x00;
//<2014/06/12 ShermanWei, 2nd source TP
int SENSOR_OPT = 0x00;
//>2014/06/12 ShermanWei,
int work_lock=0x00;
int power_lock=0x00;
int circuit_ver=0x01;
int button_state = 0;
/*++++i2c transfer start+++++++*/
int file_fops_addr=0x10;
/*++++i2c transfer end+++++++*/
int tpd_down_flag=0;

struct i2c_client *i2c_client = NULL;
struct task_struct *thread = NULL;

//<2014/03/12 ShermanWei, for KnockOn wakeup
struct wake_lock tpwakeupThread_lock;
int tp_wakeup_thread_timeout=0;
static DEFINE_MUTEX(tpwakeup_mutex);
static DECLARE_WAIT_QUEUE_HEAD(tp_wakeup_thread_wq);
//>2014/03/12 ShermanWei,

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static inline int elan_ktf2k_ts_parse_xy(uint8_t *data,
			uint16_t *x, uint16_t *y);
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
#if 0
extern void mt_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif
//-----variables---------
//#define ESD_CHECK
#ifdef ESD_CHECK
	static int have_interrupts = 0;
	static struct workqueue_struct *esd_wq = NULL;
	static struct delayed_work esd_work;
    //<2014/12/18-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]fix touch panel can't use caused by esd check
	static unsigned long delay = 2*HZ;
    //>2014/12/18-Lever.Ni

    //declare function
    static void elan_touch_esd_func(struct work_struct *work);
#endif


static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);


static int tpd_flag = 0;

#if 0
static int key_pressed = -1;

struct osd_offset{
	int left_x;
	int right_x;
	unsigned int key_event;
};

static struct osd_offset OSD_mapping[] = { // Range need define by Case!
	{35, 290,  KEY_MENU},	//menu_left_x, menu_right_x, KEY_MENU
	{303, 467, KEY_HOME},	//home_left_x, home_right_x, KEY_HOME
	{473, 637, KEY_BACK},	//back_left_x, back_right_x, KEY_BACK
	{641, 905, KEY_SEARCH},	//search_left_x, search_right_x, KEY_SEARCH
};
#endif 

//<2014/03/19 ShermanWei, for TP attributr interface
uint8_t u8knockon_enable =1;
//>2014/03/19 ShermanWei,

#if IAP_PORTION
uint8_t ic_status=0x00;	//0:OK 1:master fail 2:slave fail
int update_progree=0;
uint8_t I2C_DATA[3] = {0x10, 0x20, 0x21};/*I2C devices address*/  
int is_OldBootCode = 0; // 0:new 1:old



/*The newest firmware, if update must be changed here*/
static uint8_t file_fw_data[] = {
//<2015/1/19-leverni, [V10][DRV]update tp firmware
//#include "LG_V10_5501.i",
//#include "LG_V10_5502.i",
//#include "LG_V10_V5504.i",
//#include "LG_V10_V5505.i",
//#include "LG_V10_V5507.i",
//#include "LG_V10_V5509.i",
//#include "LG_V10_V550B.i",
//#include "LG_V10_V550C.i",
//#include "LG_V10_V550D.i",
//#include "LG_V10_V550E.i",
//#include "LG_V10_V5511.i",
//#include "LG_V10_V5512.i",
//#include "LG_V10_5516.i",
//#include "LG_V10_5517.i",
//#include "LG_V10_V5518.i",
//#include "LG_V10_V5520.i",
//#include "LG_V10_V5521.i",
#include "LG_V10_V5522.i",
//>2015/1/19-leverni
};

//<2014/06/12 ShermanWei, 2nd source TP
static uint8_t file_fw_data_second[] = {
//<2014/11/07-Y45861-Lever.Ni,[Y20][Feature][Common][BSP][][]add touch panel knock code function

//>2014/11/07-Y45861-Lever.Ni

};
//>2014/06/12 ShermanWei

//<2014/06/11-samhuang, temp solution for knockon with P-sensor
#define KNOCK_ON_WITH_P_SENSOR
//>2014/06/11-samhuang

enum
{
	PageSize		= 132,
	PageNum		        = 249,
	ACK_Fail		= 0x00,
	ACK_OK			= 0xAA,
	ACK_REWRITE		= 0x55,
};

enum
{
	E_FD			= -1,
};
#endif

//Add 0821 start
static const struct i2c_device_id tpd_id[] = 
{
	{ "ektf2k_mtk", 0 },
	{ }
};

#ifdef ELAN_MTK6577
	static struct i2c_board_info __initdata i2c_tpd={ I2C_BOARD_INFO("ektf2k_mtk", (0x2A>>1))};
#else
	unsigned short force[] = {0, 0x20, I2C_CLIENT_END, I2C_CLIENT_END};
	static const unsigned short *const forces[] = { force, NULL };
	//static struct i2c_client_address_data addr_data = { .forces = forces, };
#endif

static struct i2c_driver tpd_i2c_driver =
{
    .driver = {
        .name = "ektf2k_mtk",
        .owner = THIS_MODULE,
    },
    .probe = tpd_probe,
    .remove =  tpd_remove,
    .id_table = tpd_id,
    .detect = tpd_detect,
    //.address_data = &addr_data,
};
//Add 0821 end



struct elan_ktf2k_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct workqueue_struct *elan_wq;
	struct work_struct work;
	struct early_suspend early_suspend;
	int intr_gpio;
// Firmware Information
	int fw_ver;
	int fw_id;
	int bc_ver;
	int x_resolution;
	int y_resolution;
// For Firmare Update 
	struct miscdevice firmware;
	struct hrtimer timer;
};

static struct elan_ktf2k_ts_data *private_ts;
static int __fw_packet_handler(struct i2c_client *client);
static int elan_ktf2k_ts_rough_calibrate(struct i2c_client *client);
static int tpd_resume(struct i2c_client *client);

#if IAP_PORTION
int Update_FW_One(/*struct file *filp,*/ struct i2c_client *client, int recovery);
static int __hello_packet_handler(struct i2c_client *client);
int IAPReset();
#endif


//<2014/03/19 ShermanWei, for TP attributr interface
static ssize_t show_fwversion_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = private_ts->client;
//	long *tmp = (long*)buf;
	__fw_packet_handler(client);
#if 0
	tmp[0] = FW_VERSION;
printk("[elan] %s,FW_VERSION=0x%x \n", __func__,tmp[0]);
	return sizeof(int);	
#else
//<2014/06/12 ShermanWei, 2nd source TP
//<2014/11/27-Y46445-Lever.Ni, [Y20][Feature][Common][BSP][][]add show firmware_id function
	return snprintf(buf, PAGE_SIZE, "0x%x;0x%x;\n", FW_VERSION,FW_ID);
//>2014/11/27-Y46445-Lever.Ni
//>2014/06/12 ShermanWei
#endif
/*
	char strbuf[10];
	strbuf[0]='2';
	strbuf[1]='3';
	strbuf[2]='4';
	strbuf[3]='5';
	strbuf[4]='6';
	strbuf[4]='\0';
	if(NULL == client)
	{
		printk("i2c client is null!!\n");
		return 0;
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
*/

}

static ssize_t store_knockon_value(struct device_driver *ddri, const char *buf,size_t size)
{
//<2014/03/28-samhuang, enable attr for control knockon
#if 0
	u8knockon_enable = buf[0];	
	
#else
	int mode;	
	sscanf(buf,"%d\n",&mode);
	if (mode == 0)
	{
		u8knockon_enable = 0;
	}
	else
	{
		u8knockon_enable = 1;
	}
#endif
//>2014/03/28-samhuang
printk("[elan] %s,u8knockon_enable=0x%x \n", __func__,u8knockon_enable);
	return size;
}
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(fwversion,   S_IWUSR | S_IRUGO, show_fwversion_value,      NULL);
static DRIVER_ATTR(knockon,   S_IWUSR | S_IRUGO, NULL,      store_knockon_value);
//<2014/03/30-samhuang, sync knockon setting with TP driver
static ssize_t set_knock_on(struct device_driver *ddri, char *buf)
{
	u8knockon_enable = 1;
	return sprintf(buf, "%u\n", u8knockon_enable);
}
static ssize_t set_knock_off(struct device_driver *ddri, char *buf)
{
	u8knockon_enable = 0;
	return sprintf(buf, "%u\n", u8knockon_enable);
}
//<2014/06/11-samhuang, temp solution for knockon with P-sensor
#if defined(KNOCK_ON_WITH_P_SENSOR)
static ssize_t set_knockon_reset(struct device_driver *ddri, char *buf)
{
#if 0
    int retval = TPD_OK;
    static char data = 0x3;
    uint8_t cmd[] = {CMD_W_PKT, 0x50, 0x00, 0x01};
    uint8_t cmd1[] = {CMD_W_PKT, 0x54, 0x00, 0x01};///standby mode
    uint8_t cmd2[] = {CMD_W_PKT, 0x5E, 0x10, 0x00};///knock on

    //mt_eint_unmask(CUST_EINT_TP_WAKEUP_NUM);
  
	private_ts->client->addr &= ~I2C_DMA_FLAG;

	if (u8knockon_enable == 1)
	{    
		if ((i2c_master_send(private_ts->client, cmd2, sizeof(cmd2))) != sizeof(cmd2))
		{
			printk("[elan] %s: i2c_master_send 2 failed\n", __func__);
			retval = -retval;
		}
		msleep(2);
		if ((i2c_master_send(private_ts->client, cmd1, sizeof(cmd1))) != sizeof(cmd1)) 
		{
			printk("[elan] %s: i2c_master_send 1 failed\n", __func__);
			retval = -retval;
		}
	}
	else
	{
		if ((i2c_master_send(private_ts->client, cmd, sizeof(cmd))) != sizeof(cmd)) 
		{
			printk("[elan] %s: i2c_master_send failed\n", __func__);
			retval = -retval;
		}
	}

    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    #else 
     int retval = TPD_OK;
     static char data = 0x3;
    //<2014/12/12-Lever.Ni, [Y20][BUG][Common][BSP][][]release touch event when tp suspend
    struct input_dev *idev = tpd->dev;
    //>2014/12/12-Lever.Ni
    //<2014/11/07-Y45861-Lever.Ni,[Y20][Feature][Common][BSP][][]add touch panel knock code function
        uint8_t cmd[] = {0x54,0x50,0x00,0x01};
    //>2014/11/07-Y45861-Lever.Ni
#ifdef ESD_CHECK	
        cancel_delayed_work_sync(&esd_work);
#endif
    //<2014/12/12-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]release all touch event when tp suspend
        input_mt_sync(idev);
        input_sync(idev);
    //>2014/12/12-Lever.Ni
    //<2014/11/07-Y45861-Lever.Ni,[Y20][Feature][Common][BSP][][]add touch panel knock code function
        if (test_bit(KCODE_EN, &knock_flag) == 0) {
    //<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#ifdef ELAN_DEBUG
            printk(KERN_ERR "tp_reset [elan] TP enter into sleep mode...\n");
#endif
    //>2014/11/18-Y46170-Lever.Ni
            if ((i2c_master_send(private_ts->client, cmd, sizeof(cmd))) != sizeof(cmd)) 
            {
                printk(KERN_ERR "tp_reset [elan] %s: i2c_master_send failed\n", __func__);
                return sprintf(buf, "%u\n", -retval);
            }
            mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
            set_bit(KSLEEP_FLAG, &knock_flag);
        }
        else if (test_bit(KCODE_EN, &knock_flag) == 1) {
            int ret = 0;
    //<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#ifdef ELAN_DEBUG
            printk(KERN_ERR "tp_reset suspend knock_flag==1\n\r");
#endif
    //>2014/11/18-Y46170-Lever.Ni
            if (knock_cmd[1] == KNOCK_CODE) {
    
                ret = i2c_master_send(private_ts->client, knock_cmd,  sizeof(knock_cmd));
                if (ret < 0) {
                    printk(KERN_ERR "tp_reset send error\n\r");
                    return sprintf(buf, "%u\n", -retval);
                }
                msleep(2);
                ret = i2c_master_send(private_ts->client, knock_code,  sizeof(knock_code));
                if (ret < 0) {
                    printk(KERN_ERR "tp_reset send error 2\n\r");
                    return sprintf(buf, "%u\n", -retval);
                }
                set_bit(KCODE_FLAG, &knock_flag);
                clear_bit(KON_FLAG, &knock_flag);
            } 
            else if (knock_cmd[1] == KNOCK_ON) {
    //<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#ifdef ELAN_DEBUG
                printk("tp_reset suspend knock on\n\r");
#endif
    //>2014/11/18-Y46170-Lever.Ni
                ret = i2c_master_send(private_ts->client, knock_cmd,  sizeof(knock_cmd));
                if (ret < 0) {
                    printk(KERN_ERR "tp_resete send error\n\r");
                    return sprintf(buf, "%u\n", -retval);
                }
                set_bit(KON_FLAG, &knock_flag);
                clear_bit(KCODE_FLAG, &knock_flag);
            }
        }
    //>2014/03/12 ShermanWei,
        //mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    //>2014/11/07-Y45861-Lever.Ni

    #endif
    
	return sprintf(buf, "%u\n", retval);
}
#endif
//>2014/06/11-samhuang
static DRIVER_ATTR(knock_on, 0664, set_knock_on, NULL);
static DRIVER_ATTR(knock_off, 0664, set_knock_off, NULL);
//<2014/06/11-samhuang, temp solution for knockon with P-sensor
#if defined(KNOCK_ON_WITH_P_SENSOR)
static DRIVER_ATTR(knockon_reset, 0664, set_knockon_reset, NULL);
#endif
//>2014/06/11-samhuang
//>2014/03/30-samhuang

/*----------------------------------------------------------------------------*/
static struct driver_attribute *elan_ts_attr_list[] = {
	&driver_attr_fwversion,     /*chip information*/
	&driver_attr_knockon,     /*chip information*/
	//<2014/03/30-samhuang, sync knockon setting with TP driver
	&driver_attr_knock_on,
	&driver_attr_knock_off
//<2014/06/11-samhuang, temp solution for knockon with P-sensor
#if defined(KNOCK_ON_WITH_P_SENSOR)
	,&driver_attr_knockon_reset
#endif
//>2014/06/11-samhuang
	//>2014/03/30-samhuang

};

static int touch_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(elan_ts_attr_list)/sizeof(elan_ts_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		err = driver_create_file(driver, elan_ts_attr_list[idx]);
		if(err)
		{            
			printk("driver_create_file (%s) = %d\n", elan_ts_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
//>2014/03/19 ShermanWei

#ifdef MTK6589_DMA

static int elan_i2c_dma_recv_data(struct i2c_client *client, uint8_t *buf,uint8_t len)
{
	int rc;
	uint8_t *pReadData = 0;
	unsigned short addr = 0;
	pReadData = gpDMABuf_va;
	addr = client->addr ;
	client->addr |= I2C_DMA_FLAG;	
  if(!pReadData){
		printk("[elan] dma_alloc_coherent failed!\n");
		return -1;
  }
	rc = i2c_master_recv(client, gpDMABuf_pa, len);
//<2014/04/11 ShermanWei,
	client->addr = addr;
//>2014/04/11 ShermanWei,
//	copy_to_user(buf, pReadData, len);
	return rc;
}

static int elan_i2c_dma_send_data(struct i2c_client *client, uint8_t *buf,uint8_t len)
{
	int rc;
	unsigned short addr = 0;
	uint8_t *pWriteData = gpDMABuf_va;
	addr = client->addr ;
	client->addr |= I2C_DMA_FLAG;	
	
  if(!pWriteData){
		printk("[elan] dma_alloc_coherent failed!\n");
		return -1;
  }
//  copy_from_user(pWriteData, ((void*)buf), len);

	rc = i2c_master_send(client, gpDMABuf_pa, len);
	client->addr = addr;
//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
	#ifdef ELAN_DEBUG
	printk("[elan] elan_i2c_dma_send_data rc=%d!\n",rc);
	#endif
//>2014/11/18-Y46170-Lever.Ni
	return rc;
}
#endif
// For Firmware Update 
int elan_iap_open(struct inode *inode, struct file *filp){ 

	printk("[ELAN]into elan_iap_open\n");
		if (private_ts == NULL)  printk("private_ts is NULL~~~");
		
	return 0;
}

int elan_iap_release(struct inode *inode, struct file *filp){    
	return 0;
}

static ssize_t elan_iap_write(struct file *filp, const char *buff, size_t count, loff_t *offp){  
    int ret;
    char *tmp;

    printk("[ELAN]into elan_iap_write\n");
#ifdef ESD_CHECK
have_interrupts = 1;
#endif
    if (count > 8192)
        count = 8192;

    tmp = kmalloc(count, GFP_KERNEL);
    
    if (tmp == NULL)
        return -ENOMEM;

#ifdef MTK6589_DMA    
    if (copy_from_user(gpDMABuf_va, buff, count)) {
        return -EFAULT;
    }
    ret = elan_i2c_dma_send_data(private_ts->client, tmp, count);
#else
    if (copy_from_user(tmp, buff, count)) {
        return -EFAULT;
    }    
    ret = i2c_master_send(private_ts->client, tmp, count);
#endif    
    kfree(tmp);
    return (ret == 1) ? count : ret;

}

ssize_t elan_iap_read(struct file *filp, char *buff, size_t count, loff_t *offp){    
    char *tmp;
    int ret;  
    long rc;
#ifdef ESD_CHECK
have_interrupts = 1;
#endif
    printk("[ELAN]into elan_iap_read\n");
    if (count > 8192)
        count = 8192;

    tmp = kmalloc(count, GFP_KERNEL);

    if (tmp == NULL)
        return -ENOMEM;
#ifdef MTK6589_DMA
    ret = elan_i2c_dma_recv_data(private_ts->client, tmp, count);
    if (ret >= 0)
        rc = copy_to_user(buff, gpDMABuf_va, count);    
#else    
    ret = i2c_master_recv(private_ts->client, tmp, count);
    if (ret >= 0)
        rc = copy_to_user(buff, tmp, count);    
#endif  

    kfree(tmp);

    //return ret;
    return (ret == 1) ? count : ret;
	
}

static long elan_iap_ioctl(/*struct inode *inode,*/ struct file *filp,    unsigned int cmd, unsigned long arg){

	int __user *ip = (int __user *)arg;
	printk("[ELAN]into elan_iap_ioctl\n");
	printk("cmd value %x\n",cmd);
	
	switch (cmd) {        
		case IOCTL_I2C_SLAVE: 
			private_ts->client->addr = (int __user)arg;
			private_ts->client->addr &= I2C_MASK_FLAG; 
			private_ts->client->addr |= I2C_ENEXT_FLAG;
			//file_fops_addr = 0x15;
			break;   
		case IOCTL_MAJOR_FW_VER:            
			break;        
		case IOCTL_MINOR_FW_VER:            
			break;        
		case IOCTL_RESET:

	   		mt_set_gpio_mode( GPIO_CTP_RST_PIN, GPIO_MODE_00 );
 			mt_set_gpio_dir( GPIO_CTP_RST_PIN, GPIO_DIR_OUT );
    			mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
			mdelay(10);
		//	#if !defined(EVB)
    				mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
		//	#endif
		        mdelay(10);
    			mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );

			break;
		case IOCTL_IAP_MODE_LOCK:
			if(work_lock==0)
			{
				printk("[elan]%s %x=IOCTL_IAP_MODE_LOCK\n", __func__,IOCTL_IAP_MODE_LOCK);
				work_lock=1;
				disable_irq(CUST_EINT_TOUCH_PANEL_NUM);
				mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#ifdef ESD_CHECK	
	cancel_delayed_work_sync(&esd_work);
#endif
			}
			break;
		case IOCTL_IAP_MODE_UNLOCK:
			if(work_lock==1)
			{			
				work_lock=0;
				enable_irq(CUST_EINT_TOUCH_PANEL_NUM);
				mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#ifdef ESD_CHECK
	queue_delayed_work(esd_wq, &esd_work, delay);	
#endif

			}
			break;
		case IOCTL_CHECK_RECOVERY_MODE:
			return RECOVERY;
			break;
		case IOCTL_FW_VER:
			__fw_packet_handler(private_ts->client);
			return FW_VERSION;
			break;
		case IOCTL_X_RESOLUTION:
			__fw_packet_handler(private_ts->client);
			return X_RESOLUTION;
			break;
		case IOCTL_Y_RESOLUTION:
			__fw_packet_handler(private_ts->client);
			return Y_RESOLUTION;
			break;
		case IOCTL_FW_ID:
			__fw_packet_handler(private_ts->client);
			return FW_ID;
			break;
		case IOCTL_ROUGH_CALIBRATE:
			return elan_ktf2k_ts_rough_calibrate(private_ts->client);
		case IOCTL_I2C_INT:
			put_user(mt_get_gpio_in(GPIO_CTP_EINT_PIN),ip);
			printk("[elan]GPIO_CTP_EINT_PIN = %d\n", mt_get_gpio_in(GPIO_CTP_EINT_PIN));

			break;	
		case IOCTL_RESUME:
			tpd_resume(private_ts->client);
			break;	
		case IOCTL_CIRCUIT_CHECK:
			return circuit_ver;
			break;
		case IOCTL_POWER_LOCK:
			power_lock=1;
			break;
		case IOCTL_POWER_UNLOCK:
			power_lock=0;
			break;
#if IAP_PORTION		
		case IOCTL_GET_UPDATE_PROGREE:
			update_progree=(int __user)arg;
			break; 

		case IOCTL_FW_UPDATE:
			//RECOVERY = IAPReset(private_ts->client);
			RECOVERY=0;
			Update_FW_One(private_ts->client, RECOVERY);
#endif
		case IOCTL_BC_VER:
			__fw_packet_handler(private_ts->client);
			return BC_VERSION;
			break;
		default:            
			break;   
	}       
	return 0;
}

struct file_operations elan_touch_fops = {    
        .open =         elan_iap_open,    
        .write =        elan_iap_write,    
        .read = 	elan_iap_read,    
        .release =	elan_iap_release,    
	.unlocked_ioctl=elan_iap_ioctl, 
 };
#if IAP_PORTION
int EnterISPMode(struct i2c_client *client, uint8_t  *isp_cmd)
{
	char buff[4] = {0};
	int len = 0;
	//<2015/02/03-Lever.ni [V10][BUG][Common][BSP][Internal][]fix can't enter isp mode issue
    mt_set_gpio_out(GPIO_CTP_RST_PIN, 1);
    msleep(10);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, 0);
    msleep(20);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, 1);
    msleep(10);
	//>2015/02/03-Lever.ni
    len = i2c_master_send(private_ts->client, isp_cmd,  sizeof(isp_cmd));
	if (len != sizeof(buff)) {
		printk("[ELAN] ERROR: EnterISPMode fail! len=%d\r\n", len);
		return -1;
	}
	else
		printk("[ELAN] IAPMode write data successfully! cmd = [%2x, %2x, %2x, %2x]\n", isp_cmd[0], isp_cmd[1], isp_cmd[2], isp_cmd[3]);
	return 0;
}

int ExtractPage(struct file *filp, uint8_t * szPage, int byte)
{
	int len = 0;

	len = filp->f_op->read(filp, szPage,byte, &filp->f_pos);
	if (len != byte) 
	{
		printk("[ELAN] ExtractPage ERROR: read page error, read error. len=%d\r\n", len);
		return -1;
	}

	return 0;
}

int WritePage(uint8_t * szPage, int byte)
{
	int len = 0;

	len = i2c_master_send(private_ts->client, szPage,  byte);
	if (len != byte) 
	{
		printk("[ELAN] ERROR: write page error, write error. len=%d\r\n", len);
		return -1;
	}

	return 0;
}

int GetAckData(struct i2c_client *client)
{
	int len = 0;

	char buff[2] = {0};
	
	len=i2c_master_recv(private_ts->client, buff, sizeof(buff));
	if (len != sizeof(buff)) {
		printk("[ELAN] ERROR: read data error, write 50 times error. len=%d\r\n", len);
		return -1;
	}

	printk("[ELAN] GetAckData:%x,%x\n",buff[0],buff[1]);
	if (buff[0] == 0xaa/* && buff[1] == 0xaa*/) 
		return ACK_OK;
	else if (buff[0] == 0x55 && buff[1] == 0x55)
		return ACK_REWRITE;
	else
		return ACK_Fail;

	return 0;
}

void print_progress(int page, int ic_num, int j)
{
	int i, percent,page_tatol,percent_tatol;
	char str[256];
	str[0] = '\0';
	for (i=0; i<((page)/10); i++) {
		str[i] = '#';
		str[i+1] = '\0';
	}
	
	page_tatol=page+377*(ic_num-j);
	percent = ((100*page)/(377));
	percent_tatol = ((100*page_tatol)/(377*ic_num));

	if ((page) == (377))
		percent = 100;

	if ((page_tatol) == (377*ic_num))
		percent_tatol = 100;		

	printk("\rprogress %s| %d%%", str, percent);
	
	if (page == (377))
		printk("\n");
}
/* 
* Restet and (Send normal_command ?)
* Get Hello Packet
*/
int  IAPReset()
{
			int res;

	   		mt_set_gpio_mode( GPIO_CTP_RST_PIN, GPIO_MODE_00 );
 			mt_set_gpio_dir( GPIO_CTP_RST_PIN, GPIO_DIR_OUT );
    			mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
			mdelay(10);
		//	#if !defined(EVB)
    			mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	//		#endif
	   		mdelay(10);
    			mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
			return 1;

#if 0
	printk("[ELAN] read Hello packet data!\n"); 	  
	res= __hello_packet_handler(client);
	return res;
#endif 
}

/* Check Master & Slave is "55 aa 33 cc" */
int CheckIapMode(void)
{
	char buff[4] = {0},len = 0;
	//WaitIAPVerify(1000000);
	//len = read(fd, buff, sizeof(buff));
	len=i2c_master_recv(private_ts->client, buff, sizeof(buff));
	if (len != sizeof(buff)) 
	{
		printk("[ELAN] CheckIapMode ERROR: read data error,len=%d\r\n", len);
		return -1;
	}
	else
	{
		
		if (buff[0] == 0x55 && buff[1] == 0xaa && buff[2] == 0x33 && buff[3] == 0xcc)
		{
			//printk("[ELAN] CheckIapMode is 55 aa 33 cc\n");
			return 0;
		}
		else// if ( j == 9 )
		{
			printk("[ELAN] Mode= 0x%x 0x%x 0x%x 0x%x\r\n", buff[0], buff[1], buff[2], buff[3]);
			printk("[ELAN] ERROR:  CheckIapMode error\n");
			return -1;
		}
	}
	printk("\n");	
}

int Update_FW_One(struct i2c_client *client, int recovery)
{
	int res = 0,ic_num = 1;
	int iPage = 0, rewriteCnt = 0; //rewriteCnt for PAGE_REWRITE
	int i = 0;
	uint8_t data;

	int restartCnt = 0, checkCnt = 0; // For IAP_RESTART
	//uint8_t recovery_buffer[4] = {0};
	int byte_count;
	uint8_t *szBuff = NULL;
	int curIndex = 0;
//<2014/03/10 ShermanWei,
	int PageNum =0;
//>2014/03/10 ShermanWei,
#if CMD_54001234
	uint8_t isp_cmd[] = {0x54, 0x00, 0x12, 0x34};	 //54 00 12 34
#else
	uint8_t isp_cmd[] = {0x45, 0x49, 0x41, 0x50};	 //45 49 41 50
#endif
	uint8_t recovery_buffer[4] = {0};

IAP_RESTART:	

	data=I2C_DATA[0];//Master
	dev_dbg(&client->dev, "[ELAN] %s: address data=0x%x \r\n", __func__, data);

//<2014/03/17 ShermanWei, for TP Recovery mode download without EnterISPMode()
	if(recovery != 0x80)
	{
		printk("[ELAN] Firmware upgrade normal mode !\n");

		////IAPReset();
	        mdelay(20);	

		res = EnterISPMode(private_ts->client, isp_cmd);	 //enter ISP mode

////	res = i2c_master_recv(private_ts->client, recovery_buffer, 4);   //55 aa 33 cc 
////	printk("[ELAN] recovery byte data:%x,%x,%x,%x \n",recovery_buffer[0],recovery_buffer[1],recovery_buffer[2],recovery_buffer[3]);			

        mdelay(10);
#if 1//0
		//Check IC's status is IAP mode(55 aa 33 cc) or not
		res = CheckIapMode();	 //Step 1 enter ISP mode
		if (res == -1) //CheckIapMode fail
		{	
			checkCnt ++;
			if (checkCnt >= 5)
			{
				printk("[ELAN] ERROR: CheckIapMode %d times fails!\n", IAPRESTART);
				return E_FD;
			}
			else
			{
				printk("[ELAN] CheckIapMode retry %dth times! And restart IAP~~~\n\n", checkCnt);
				goto IAP_RESTART;
			}
		}
		else
			printk("[ELAN]  CheckIapMode ok!\n");
#endif
	} else
		printk("[ELAN] Firmware upgrade recovery mode !\n");
//>2014/03/17 ShermanWei,
	// Send Dummy Byte	
	printk("[ELAN] send one byte data:%x,%x",private_ts->client->addr,data);
	res = i2c_master_send(private_ts->client, &data,  sizeof(data));
	if(res!=sizeof(data))
	{
		printk("[ELAN] dummy error code = %d\n",res);
	}	
	mdelay(50);///mdelay(50);
//<2014/03/10 ShermanWei,
  PageNum = sizeof(file_fw_data)/sizeof(uint8_t)/132;
  printk("[ELAN] PageNum= %d", PageNum);
//>2014/03/10 ShermanWei,
	// Start IAP
	for( iPage = 1; iPage <= PageNum; iPage++ ) 
	{
PAGE_REWRITE:
#if 1 
		// 8byte mode
		//szBuff = fw_data + ((iPage-1) * PageSize); 
		for(byte_count=1;byte_count<=17;byte_count++)
		{
			if(byte_count!=17)
			{		
	//			printk("[ELAN] byte %d\n",byte_count);	
	//			printk("curIndex =%d\n",curIndex);
				szBuff = file_fw_data + curIndex;
				curIndex =  curIndex + 8;

				//ioctl(fd, IOCTL_IAP_MODE_LOCK, data);
				res = WritePage(szBuff, 8);
			}
			else
			{
	//			printk("byte %d\n",byte_count);
	//			printk("curIndex =%d\n",curIndex);
				szBuff = file_fw_data + curIndex;
				curIndex =  curIndex + 4;
				//ioctl(fd, IOCTL_IAP_MODE_LOCK, data);
				res = WritePage(szBuff, 4); 
			}
		} // end of for(byte_count=1;byte_count<=17;byte_count++)
#endif 
#if 0 // 132byte mode		
		szBuff = file_fw_data + curIndex;
		curIndex =  curIndex + PageSize;
		res = WritePage(szBuff, PageSize);
#endif
//<2015/01/28-Lever.ni [V10][BUG][Common][BSP][Internal][]fix can't update firmware issue
#if 1
		if(iPage==249 || iPage==1)
		{
			mdelay(500); 			 
		}
		else
		{
			mdelay(50); 			 
		}
#endif	
//>2015/01/28-Lever.ni
		res = GetAckData(private_ts->client);

		if (ACK_OK != res) 
		{
			mdelay(50); 
			printk("[ELAN] ERROR: GetAckData fail! res=%d\r\n", res);
			if ( res == ACK_REWRITE ) 
			{
				rewriteCnt = rewriteCnt + 1;
				if (rewriteCnt == PAGERETRY)
				{
					printk("[ELAN] ID 0x%02x %dth page ReWrite %d times fails!\n", data, iPage, PAGERETRY);
					return E_FD;
				}
				else
				{
					printk("[ELAN] ---%d--- page ReWrite %d times!\n",  iPage, rewriteCnt);
					curIndex = curIndex - PageSize;
					goto PAGE_REWRITE;
				}
			}
			else
			{
				restartCnt = restartCnt + 1;
				if (restartCnt >= 5)
				{
					printk("[ELAN] ID 0x%02x ReStart %d times fails!\n", data, IAPRESTART);
					return E_FD;
				}
				else
				{
					printk("[ELAN] ===%d=== page ReStart %d times!\n",  iPage, restartCnt);
//<2014/06/12 ShermanWei, 2nd source TP
					curIndex =  0;
//>2014/06/12 ShermanWei,
					goto IAP_RESTART;
				}
			}
		}
		else
		{       printk("  data : 0x%02x ",  data);  
			rewriteCnt=0;
			print_progress(iPage,ic_num,i);
		}

		mdelay(10);
	} // end of for(iPage = 1; iPage <= PageNum; iPage++)

	if(IAPReset() > 0)
		printk("[ELAN] Update ALL Firmware successfully!\n");
	return 0;
}

#endif
// End Firmware Update


#if 0
static void elan_ktf2k_ts_early_suspend(struct early_suspend *h);
static void elan_ktf2k_ts_late_resume(struct early_suspend *h);
#endif

static ssize_t elan_ktf2k_gpio_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct elan_ktf2k_ts_data *ts = private_ts;

	//ret = gpio_get_value(ts->intr_gpio);
	ret = mt_get_gpio_in(GPIO_CTP_EINT_PIN);
	printk(KERN_DEBUG "GPIO_TP_INT_N=%d\n", ts->intr_gpio);
	sprintf(buf, "GPIO_TP_INT_N=%d\n", ret);
	ret = strlen(buf) + 1;
	return ret;
}

static DEVICE_ATTR(gpio, S_IRUGO, elan_ktf2k_gpio_show, NULL);

static ssize_t elan_ktf2k_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct elan_ktf2k_ts_data *ts = private_ts;

	sprintf(buf, "%s_x%4.4x\n", "ELAN_KTF2K", ts->fw_ver);
	ret = strlen(buf) + 1;
	return ret;
}

//<2014/12/18-Lever.Ni, [Y20][Feature][Common][BSP][][]add touch lock attribute file
static ssize_t touchpanel_lock_show(struct device_driver *ddri, char *buf) {
    uint8_t  ret = 0;
    if (test_bit(LOCK_FLAG, &knock_flag) == 1) {
        ret = 1;
    }
    else if (test_bit(LOCK_FLAG, &knock_flag) == 0) {
        ret = 0;
    }
    return sprintf(buf, "%d", ret);
}

int tp_face_enable(uint8_t val)
{
    int retval = TPD_OK;
    uint8_t cmd_lock[] = {0x54,0xC1,0x00,0x01};
    uint8_t cmd_dis[] = {0x54,0xC0,0x00,0x01};
    if (1 == val) {
        if (i2c_master_send(private_ts->client, cmd_lock, sizeof(cmd_lock)) != sizeof(cmd_lock)) {
            printk("lock touchpanel lock enable failed!\n\r");
            return -retval;
        }
    }
    else if (0 == val) {
        if (i2c_master_send(private_ts->client, cmd_dis, sizeof(cmd_dis)) != sizeof(cmd_dis)) {
            printk("touchpanel lock disable failed!\n\r");
            return -retval;
        }
    }
    return 0;
}

static ssize_t touchpanel_lock_store(struct device_driver *ddri, const char *buf, size_t size) {
    int flag = 0;
    sscanf(buf, "%d", &flag);
    printk("lock flag=%d\n\r", flag);
    tp_face_enable(flag);
    if (1 == flag) {
        set_bit(LOCK_FLAG, &knock_flag);
    }
    else if (0 == flag) {
        clear_bit(LOCK_FLAG, &knock_flag);
    }
    return size;
}
static DRIVER_ATTR(lock_enable,0644,touchpanel_lock_show, touchpanel_lock_store);
//>2014/12/18-Lever.Ni
//<2014/11/07-Y45861-Lever.Ni,[Y20][Feature][Common][BSP][][]add touch panel knock code function
#ifdef KNOCK
static ssize_t show_tp(struct device *dev, struct device_attribute *attr, char *buf) {
#if 0
	knock_cmd[0]=0x54;
	knock_cmd[1]=0x5E;
	knock_cmd[2]=0x00;
	knock_cmd[3]=0xd2;
	knock_code[0]=0x54;
	knock_code[1]=0x54;
	knock_code[2]=0x40;
	knock_code[3]=0x00;
	set_bit(KCODE_FLAG, &knock_flag);
#endif 
	knock_cmd[0] = 0x54;
	knock_cmd[1] = 0x54;
	knock_cmd[2] = 0x10;
	knock_cmd[3] = 0x00;
	set_bit(KON_FLAG, &knock_flag);
	return sprintf(buf, "%s\n\r", "tp_knock");
}

static ssize_t store_tp_pswd(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
		#if 0
		static int loop = 0;
		int i = 0;
		unsigned long rece = 0;
		strict_strtoul(buf, 0,&rece);
		knock_cmd[loop++] =  rece;
		//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
		#ifdef ELAN_DEBUG
		printk("knock_code""knock_cmd[%d]=%x\n\r",loop-1,knock_cmd[loop-1]);
		#endif
		//>2014/11/18-Y46170-Lever.Ni
		if (loop == 4) {
			loop = 0;
		}
		#else
		unsigned int rece = 0;
		int i = 0;
		strict_strtoul(buf, 0,&rece);
		printk("lever_tp rece=%x\n\r", rece);
		for (i = 0; i < 4; i++) {
			knock_cmd[3 - i] = (rece >> 8*i) & 0xff;
			//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
			#ifdef ELAN_DEBUG
			printk("knock_code knock_cmd[%d]=%x\n\r", i, knock_cmd[i]);
			#endif
			//>2014/11/18-Y46170-Lever.Ni
		}
		#endif
		return size;
}
static ssize_t store_tp_num(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	#if 0
		static int loop = 0;
		int i = 0;
		unsigned long rece = 0;
		strict_strtoul(buf, 0,&rece);
		knock_code[loop++] =  rece;
		//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
		#ifdef ELAN_DEBUG
		printk("knock_code""knock_code[%d]=%x\n\r",loop-1,knock_code[loop-1]);
		#endif
		//>2014/11/18-Y46170-Lever.Ni
		if (loop == 4) {
			loop = 0;
		}
	#else 
		unsigned int rece = 0;
		int i = 0;
		strict_strtoul(buf, 0,&rece);
		printk("lever_tp rece=%x\n\r", rece);
		for (i = 0; i < 4; i++) {
			knock_code[3 - i] = (rece >> 8*i) & 0xff;		
			//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
			#ifdef ELAN_DEBUG
			printk("knock_code knock_code[%d]=%x\n\r", i, knock_code[i]);
			#endif
			//>2014/11/18-Y46170-Lever.Ni
		}
	#endif
	return size;
}
static ssize_t store_tp_flag (struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
	static int mode = 0;
	sscanf(buf, "%d", &mode);
	if(mode) {
		//knock_flag = 1;
		set_bit(KCODE_EN, &knock_flag);
		//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
		#ifdef ELAN_DEBUG
		printk("knock_code open knock code success\n\r");
		#endif
		//>2014/11/18-Y46170-Lever.Ni
	}else if (mode == 0) {
		//knock_flag = 0;
		clear_bit(KCODE_EN, &knock_flag);
		//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
		#ifdef ELAN_DEBUG
		printk("knock_code close knock code success\n\r");
		#endif
		//>2014/11/18-Y46170-Lever.Ni
	}
	return size;
}

static DEVICE_ATTR(knock_pswd,0644,show_tp, store_tp_pswd);
static DEVICE_ATTR(knock_num,0644,NULL, store_tp_num);
static DEVICE_ATTR(knock_enable,0644,NULL,store_tp_flag);

static struct attribute *knock_attr[] = {
	&dev_attr_knock_pswd.attr,
	&dev_attr_knock_num.attr,
	&dev_attr_knock_enable.attr,
	NULL,
};
static struct attribute_group attr_group = {
	.name = NULL,
	.attrs = knock_attr,
};
static struct kobject *tp_knock_kobj;
static int elan_tp_knock_init(void) {
	int ret;
	tp_knock_kobj = kobject_create_and_add("tp_knock",NULL);
	if (tp_knock_kobj == NULL) {
		printk("[elan]%s: subsystem_register failed\n", __func__);
		ret = -ENOMEM;
		return ret;
	}
	ret = sysfs_create_group(tp_knock_kobj, &attr_group);
	if (ret) {
		printk("[elan]%s: sysfs_create_file failed\n", __func__);
		return ret;
	}	
	return 0;
}
#endif
//>2014/11/07-Y45861-Lever.Ni
#if 0
static DEVICE_ATTR(vendor, S_IRUGO, elan_ktf2k_vendor_show, NULL);

static struct kobject *android_touch_kobj;

static int elan_ktf2k_touch_sysfs_init(void)
{
	int ret ;

	android_touch_kobj = kobject_create_and_add("android_touch", NULL) ;
	if (android_touch_kobj == NULL) {
		printk(KERN_ERR "[elan]%s: subsystem_register failed\n", __func__);
		ret = -ENOMEM;
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_gpio.attr);
	if (ret) {
		printk(KERN_ERR "[elan]%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_vendor.attr);
	if (ret) {
		printk(KERN_ERR "[elan]%s: sysfs_create_group failed\n", __func__);
		return ret;
	}
	return 0 ;
}

static void elan_touch_sysfs_deinit(void)
{
	sysfs_remove_file(android_touch_kobj, &dev_attr_vendor.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_gpio.attr);
	kobject_del(android_touch_kobj);
}	
#endif


static int __elan_ktf2k_ts_poll(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int status = 0, retry = 10;

	do {
		//status = gpio_get_value(ts->intr_gpio);
		status = mt_get_gpio_in(GPIO_CTP_EINT_PIN);
		printk("[elan]: %s: status = %d\n", __func__, status);
		retry--;
		//<2014/03/11 ShermanWei,
		///mdelay(200);
		mdelay(3);
		//<2014/03/11 ShermanWei,
	} while (status == 1 && retry > 0);

	printk( "[elan]%s: poll interrupt status %s\n",
			__func__, status == 1 ? "high" : "low");
	return (status == 0 ? 0 : -ETIMEDOUT);
}

static int elan_ktf2k_ts_poll(struct i2c_client *client)
{
	return __elan_ktf2k_ts_poll(client);
}

static int elan_ktf2k_ts_get_data(struct i2c_client *client, uint8_t *cmd,
			uint8_t *buf, size_t size)
{
	int rc =1;

	dev_dbg(&client->dev, "[elan]%s: enter\n", __func__);

	if (buf == NULL)
		return -EINVAL;


	if ((i2c_master_send(client, cmd, 4)) != 4) {
		dev_err(&client->dev,
			"[elan]%s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}


	rc = elan_ktf2k_ts_poll(client);
	if (rc < 0)
		return -EINVAL;
	else {

		if (i2c_master_recv(client, buf, size) != size ||
		    buf[0] != CMD_S_PKT)
			return -EINVAL;
	}

	return 0;
}

static int __hello_packet_handler(struct i2c_client *client)
{
	int rc;
	uint8_t buf_recv[8] = { 0 };
	//uint8_t buf_recv1[4] = { 0 };

	mdelay(1500);

	rc = elan_ktf2k_ts_poll(client);
	if (rc < 0) {
		printk( "[elan] %s: Int poll failed!\n", __func__);
		RECOVERY=0x80;
		return RECOVERY;	
	}
//<2014/06/12 ShermanWei, 2nd source TP
////	rc = i2c_master_recv(client, buf_recv, 4);
	rc = i2c_master_recv(client, buf_recv, 8);
//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#ifdef ELAN_DEBUG
	printk("[elan] %s: Hello Packet %2x:%2X:%2x:%2x\n", __func__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);
#endif
//>2014/11/18-Y46170-Lever.Ni
/*  Received 8 bytes data will let TP die on old firmware on ektf21xx carbon player and MK5     
    rc = i2c_master_recv(client, buf_recv, 8);
	printk("[elan] %s: hello packet %2x:%2X:%2x:%2x:%2x:%2X:%2x:%2x\n", __func__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3] , buf_recv[4], buf_recv[5], buf_recv[6], buf_recv[7]);
*/
	if(buf_recv[0]==0x55 && buf_recv[1]==0x55 && buf_recv[2]==0x80 && buf_recv[3]==0x80)
	{
        RECOVERY=0x80;

////        rc = i2c_master_recv(client, buf_recv, 4);
//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#ifdef ELAN_DEBUG
        printk("[elan] %s: Sensor Opt %2x:%2X, Bootcode Verson %2x:%2x\n", __func__, buf_recv[4], buf_recv[5], buf_recv[6], buf_recv[7]);
#endif
//>2014/11/18-Y46170-Lever.Ni
        SENSOR_OPT =  buf_recv[5]<<8 | buf_recv[4]; 
//>2014/06/12 ShermanWei
        return RECOVERY; 
	}

	return 0;
}

static int __fw_packet_handler(struct i2c_client *client)
{
	int rc;
	int major, minor;
	uint8_t cmd[] = {CMD_R_PKT, 0x00, 0x00, 0x01};/* Get Firmware Version*/
	uint8_t cmd_x[] = {0x53, 0x60, 0x00, 0x00}; /*Get x resolution*/
	uint8_t cmd_y[] = {0x53, 0x63, 0x00, 0x00}; /*Get y resolution*/
	uint8_t cmd_id[] = {0x53, 0xf0, 0x00, 0x01}; /*Get firmware ID*/
	uint8_t cmd_bc[] = {CMD_R_PKT, 0x01, 0x00, 0x01};/* Get BootCode Version*/
//<2014/06/12 ShermanWei, 2nd source TP
	uint8_t cmd_sensoropt[] = {0x53, 0xd3, 0x00, 0x01};/* Get Sensor Option */
//>2014/06/12 ShermanWei
	uint8_t buf_recv[4] = {0};
	//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#ifdef ELAN_DEBUG
printk( "[elan] %s: \n", __func__);
#endif
//>2014/11/18-Y46170-Lever.Ni
// Firmware version
	rc = elan_ktf2k_ts_get_data(client, cmd, buf_recv, 4);
	if (rc < 0)
		return rc;
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
//	ts->fw_ver = major << 8 | minor;
	FW_VERSION = major << 8 | minor;
// Firmware ID
	rc = elan_ktf2k_ts_get_data(client, cmd_id, buf_recv, 4);
	if (rc < 0)
		return rc;
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	//ts->fw_id = major << 8 | minor;
	FW_ID = major << 8 | minor;
// X Resolution
	rc = elan_ktf2k_ts_get_data(client, cmd_x, buf_recv, 4);
	if (rc < 0)
		return rc;
	minor = ((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4);
	//ts->x_resolution =minor;
	X_RESOLUTION = minor;
	
// Y Resolution	
	rc = elan_ktf2k_ts_get_data(client, cmd_y, buf_recv, 4);
	if (rc < 0)
		return rc;
	minor = ((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4);
	//ts->y_resolution =minor;
	Y_RESOLUTION = minor;

// Bootcode version
	rc = elan_ktf2k_ts_get_data(client, cmd_bc, buf_recv, 4);
	if (rc < 0)
		return rc;
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	//ts->bc_ver = major << 8 | minor;
	BC_VERSION = major << 8 | minor;
	
//<2014/06/12 ShermanWei, 2nd source TP
// Sensor Option 
	rc = elan_ktf2k_ts_get_data(client, cmd_sensoropt, buf_recv, 4);
	if (rc < 0)
		return rc;
	major = buf_recv[2];
	minor = buf_recv[3];
	//ts->bc_ver = major << 8 | minor;
	SENSOR_OPT = major << 8 | minor;	
//>2014/06/12 ShermanWei
//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#ifdef ELAN_DEBUG
	printk( "[elan] %s: firmware version: 0x%4.4x\n",
			__func__, FW_VERSION);
	printk( "[elan] %s: firmware ID: 0x%4.4x\n",
			__func__, FW_ID);
	printk( "[elan] %s: x resolution: %d, y resolution: %d\n",
			__func__, X_RESOLUTION, Y_RESOLUTION);
	printk( "[elan] %s: sensor option: 0x%4.4x ,bootcode version: 0x%4.4x\n",
			__func__, SENSOR_OPT, BC_VERSION);  
#endif
//>2014/11/18-Y46170-Lever.Ni
	return 0;
}

static inline int elan_ktf2k_ts_parse_xy(uint8_t *data,
			uint16_t *x, uint16_t *y)
{
	*x = *y = 0;

	*x = (data[0] & 0xf0);
	*x <<= 4;
	*x |= data[1];

	*y = (data[0] & 0x0f);
	*y <<= 8;
	*y |= data[2];

	return 0;
}

static int elan_ktf2k_ts_setup(struct i2c_client *client)
{
	int rc;
   
	rc = __hello_packet_handler(client);
	printk("[elan] hellopacket's rc = %d\n",rc);

	mdelay(10);
	if (rc != 0x80){
	    rc = __fw_packet_handler(client);
	    if (rc < 0)
		    printk("[elan] %s, fw_packet_handler fail, rc = %d", __func__, rc);
		  else
	    	printk("[elan] %s: firmware checking done.\n", __func__);
			/* Check for FW_VERSION, if 0x0000 means FW update fail! */
	    if ( FW_VERSION == 0x00)
	    {
				rc = 0x80;
				printk("[elan] FW_VERSION = %d, last FW update fail\n", FW_VERSION);
	    }
	}
	return rc; /* Firmware need to be update if rc equal to 0x80(Recovery mode)   */
}

static int elan_ktf2k_ts_rough_calibrate(struct i2c_client *client){
      uint8_t cmd[] = {CMD_W_PKT, 0x29, 0x00, 0x01};

	//dev_info(&client->dev, "[elan] %s: enter\n", __func__);
	printk("[elan] %s: enter\n", __func__);
	dev_info(&client->dev,
		"[elan] dump cmd: %02x, %02x, %02x, %02x\n",
		cmd[0], cmd[1], cmd[2], cmd[3]);

	if ((i2c_master_send(client, cmd, sizeof(cmd))) != sizeof(cmd)) {
		dev_err(&client->dev,
			"[elan] %s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int elan_ktf2k_ts_set_power_state(struct i2c_client *client, int state)
{
	uint8_t cmd[] = {CMD_W_PKT, 0x50, 0x00, 0x01};

	dev_dbg(&client->dev, "[elan] %s: enter\n", __func__);

	cmd[1] |= (state << 3);

	dev_dbg(&client->dev,
		"[elan] dump cmd: %02x, %02x, %02x, %02x\n",
		cmd[0], cmd[1], cmd[2], cmd[3]);

	if ((i2c_master_send(client, cmd, sizeof(cmd))) != sizeof(cmd)) {
		dev_err(&client->dev,
			"[elan] %s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int elan_ktf2k_ts_get_power_state(struct i2c_client *client)
{
	int rc = 0;
	uint8_t cmd[] = {CMD_R_PKT, 0x50, 0x00, 0x01};
	uint8_t buf[4], power_state;

	rc = elan_ktf2k_ts_get_data(client, cmd, buf, 4);
	if (rc)
		return rc;

	power_state = buf[1];
	dev_dbg(&client->dev, "[elan] dump repsponse: %0x\n", power_state);
	power_state = (power_state & PWR_STATE_MASK) >> 3;
	dev_dbg(&client->dev, "[elan] power state = %s\n",power_state == PWR_STATE_DEEP_SLEEP ? "Deep Sleep" : "Normal/Idle");

	return power_state;
}

static int elan_ktf2k_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
    int err;
    u8 beg = addr; 
    struct i2c_msg msgs[2] = {
        {
            .addr = client->addr,    .flags = 0,
            .len = 1,                .buf= &beg
        },
        {
            .addr = client->addr,    .flags = I2C_M_RD,
            .len = len,             .buf = data,
            .ext_flag = I2C_DMA_FLAG,
        }
    };
   
    if (!client)
        return -EINVAL;

    err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
    if (err != len) {
        printk("[elan] elan_ktf2k_read_block err=%d\n", err);
        err = -EIO;
    } else {
		printk("[elan] elan_ktf2k_read_block ok\n");
        err = 0;    /*no error*/
    }
    return err;


}


static int elan_ktf2k_ts_recv_data(struct i2c_client *client, uint8_t *buf)
{
	int rc, bytes_to_recv=PACKET_SIZE;
	uint8_t *pReadData = 0;
	unsigned short addr = 0;

	if (buf == NULL)
		return -EINVAL;
	memset(buf, 0, bytes_to_recv);

//#ifdef ELAN_MTK6577
#ifdef MTK6589_DMA
	addr = client->addr ;
	client->addr |= I2C_DMA_FLAG;
	pReadData = gpDMABuf_va;
  if(!pReadData){
		printk("[elan] dma_alloc_coherent failed!\n");
  }
	rc = i2c_master_recv(client, gpDMABuf_pa, bytes_to_recv);
	copy_to_user(buf, pReadData, bytes_to_recv);
	client->addr = addr;
	#ifdef ELAN_DEBUG
	printk("[elan_debug] %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15],buf[16], buf[17]);
	#endif
	
#else	
	rc = i2c_master_recv(client, buf, 8);
	if (rc != 8)
		printk("[elan_debug] The first package error.\n");
	//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
	#ifdef ELAN_DEBUG
	printk("[elan_recv] %x %x %x %x %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
	#endif
	//>2014/11/18-Y46170-Lever.Ni
	mdelay(1);
	
	if (buf[0] == FIVE_FINGERS_PKT){    //for five finger
		rc = i2c_master_recv(client, buf+ 8, 8);	
		if (rc != 8)
			printk("[elan_debug] The second package error.\n");
		//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
		#ifdef ELAN_DEBUG
		printk("[elan_recv] %x %x %x %x %x %x %x %x\n", buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
		#endif
		//>2014/11/18-Y46170-Lever.Ni
		rc = i2c_master_recv(client, buf+ 16, 2);
		if (rc != 2)
			printk("[elan_debug] The third package error.\n");
		mdelay(1);
		//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
		#ifdef ELAN_DEBUG
		printk("[elan_recv] %x %x \n", buf[16], buf[17]);
		#endif
		//>2014/11/18-Y46170-Lever.Ni
	}
#endif	
	
	return rc;
}

#ifdef SOFTKEY_AXIS_VER //SOFTKEY is reported via AXI
static void elan_ktf2k_ts_report_data(struct i2c_client *client, uint8_t *buf)
{
	//struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	struct input_dev *idev = tpd->dev;
	uint16_t x, y;
	uint16_t fbits=0;
	uint8_t i, num, reported = 0;
	uint8_t idx, btn_idx;
	int finger_num;
	int limitY = ELAN_Y_MAX -100; // limitY need define by Case!
/* for 10 fingers	*/
	if (buf[0] == TEN_FINGERS_PKT){
	    	finger_num = 10;
	    	num = buf[2] & 0x0f; 
	    	fbits = buf[2] & 0x30;	
		    fbits = (fbits << 4) | buf[1]; 
	    	idx=3;
		    btn_idx=33;
      }
// for 5 fingers	
	else if ((buf[0] == MTK_FINGERS_PKT) || (buf[0] == FIVE_FINGERS_PKT)){
	    	finger_num = 5;
	    	num = buf[1] & 0x07; 
        fbits = buf[1] >>3;
	    	idx=2;
	    	btn_idx=17;
	}else{
// for 2 fingers      
		finger_num = 2;
		num = buf[7] & 0x03; 
		fbits = buf[7] & 0x03;
		idx=1;
		btn_idx=7;
	}

	switch (buf[0]) {
		case MTK_FINGERS_PKT:
		case TWO_FINGERS_PKT:
		case FIVE_FINGERS_PKT:	
		case TEN_FINGERS_PKT:
			//input_report_key(idev, BTN_TOUCH, 1);
			if (num == 0)
			{
				//dev_dbg(&client->dev, "no press\n");
				if(key_pressed < 0){
					input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 0);
					input_report_abs(idev, ABS_MT_WIDTH_MAJOR, 0);
					input_mt_sync(idev);
				}
				else{
					//dev_err(&client->dev, "[elan] KEY_RELEASE: key_code:%d\n",OSD_mapping[key_pressed].key_event);
					input_report_key(idev, OSD_mapping[key_pressed].key_event, 0);
					key_pressed = -1;
				}
			}
			else 
			{			
				//dev_dbg(&client->dev, "[elan] %d fingers\n", num);                        
				//input_report_key(idev, BTN_TOUCH, 1);
				for (i = 0; i < finger_num; i++) 
				{	
					if ((fbits & 0x01)) 
					{
						elan_ktf2k_ts_parse_xy(&buf[idx], &x, &y);  
						//elan_ktf2k_ts_parse_xy(&buf[idx], &y, &x);
						//x = X_RESOLUTION-x;	 
						//y = Y_RESOLUTION-y; 
#if 1
	if(X_RESOLUTION > 0 && Y_RESOLUTION > 0)
	{
		x = ( x * LCM_X_MAX )/X_RESOLUTION;
		y = ( y * LCM_Y_MAX )/Y_RESOLUTION;
	}
	else
	{
		x = ( x * LCM_X_MAX )/ELAN_X_MAX;
		y = ( y * LCM_Y_MAX )/ELAN_Y_MAX;
	}
#endif 		 
						printk("[elan_debug SOFTKEY_AXIS_VER] %s, x=%d, y=%d\n",__func__, x , y);
									     
						if (!((x<=0) || (y<=0) || (x>=X_RESOLUTION) || (y>=Y_RESOLUTION))) 
						{   
							if ( y < limitY )
						     	{
			    					input_report_abs(idev, ABS_MT_TRACKING_ID, i);
								input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 8);
								input_report_abs(idev, ABS_MT_POSITION_X, x);
								input_report_abs(idev, ABS_MT_POSITION_Y, y);
								input_mt_sync(idev);
							}
							else
							{
							    	int i=0;
							    	for(i=0;i<4;i++)
							    	{
							    		if((x > OSD_mapping[i].left_x) && (x < OSD_mapping[i].right_x))
							    		{
										//dev_err(&client->dev, "[elan] KEY_PRESS: key_code:%d\n",OSD_mapping[i].key_event);
										//printk("[elan] %d KEY_PRESS: key_code:%d\n", i, OSD_mapping[i].key_event);
							    			input_report_key(idev, OSD_mapping[i].key_event, 1);
							    			key_pressed = i;
							    		}
							    	}
							}
							reported++;
							
					  	} // end if border
					} // end if finger status
				  	fbits = fbits >> 1;
				  	idx += 3;
				} // end for
			}

			if (reported)
				input_sync(idev);
			else 
			{
				input_mt_sync(idev);
				input_sync(idev);
			}

			break;
	   	default:
				dev_err(&client->dev,
					"[elan] %s: unknown packet type: %0x\n", __func__, buf[0]);
				break;
	} // end switch
	return;
}
#else //SOFTKEY is reported via BTN bit
static void elan_ktf2k_ts_report_data(struct i2c_client *client, uint8_t *buf)
{
	/*struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);*/
	struct input_dev *idev = tpd->dev;
	uint16_t x, y;
	uint16_t fbits=0;
	uint8_t i, num, reported = 0;
	uint8_t idx, btn_idx;
	int finger_num;
/* for 10 fingers	*/
	if (buf[0] == TEN_FINGERS_PKT){
	    	finger_num = 10;
	    	num = buf[2] & 0x0f; 
	    	fbits = buf[2] & 0x30;	
		    fbits = (fbits << 4) | buf[1]; 
	    	idx=3;
		    btn_idx=33;
      }
// for 5 fingers	
	else if ((buf[0] == MTK_FINGERS_PKT) || (buf[0] == FIVE_FINGERS_PKT)){
	    	finger_num = 5;
	    	num = buf[1] & 0x07; 
        fbits = buf[1] >>3;
	    	idx=2;
	    	btn_idx=17;
	}else{
// for 2 fingers      
		finger_num = 2;
		num = buf[7] & 0x03; 
		fbits = buf[7] & 0x03;
		idx=1;
		btn_idx=7;
	}
		
	switch (buf[0]) {
		case MTK_FINGERS_PKT:
		case TWO_FINGERS_PKT:
		case FIVE_FINGERS_PKT:	
		case TEN_FINGERS_PKT:
			//input_report_key(idev, BTN_TOUCH, 1);
			if (num == 0)
			{
				dev_dbg(&client->dev, "no press\n");
				#ifdef ELAN_DEBUG
				printk("tp button_state0 = %x\n",button_state);
        printk("tp buf[btn_idx] = %x KEY_MENU=%x KEY_HOME=%x KEY_BACK=%x KEY_SEARCH =%x\n",buf[btn_idx], KEY_MENU, KEY_HOME, KEY_BACK, KEY_SEARCH);
				#endif
	
						
					switch (buf[btn_idx]) {
				//<2014/03/10 ShermanWei,
				#if 1
				
				
				    	case ELAN_KEY_1:
						//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
						#ifdef ELAN_DEBUG
						printk("KEY back 1\n");
						#endif
						//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Intern
						input_report_key(idev, KEY_BACK, 1);
						button_state = ELAN_KEY_1;
						break;
				    	case ELAN_KEY_4:
					
						//<2014/11/07-Y44692-Lever.Ni,[Y20][BUG][Common][BSP][Internal][]fix touch panel button issue
						input_report_key(idev, /*KEY_RECENT*/KEY_HOMEPAGE, 1);

						button_state = ELAN_KEY_4;
						break;
				    	case ELAN_KEY_5:
						
						
						input_report_key(idev, KEY_RECENT/*KEY_MENU*/, 1);

						button_state = ELAN_KEY_5;
						break;
						//>2014/11/07-Y44692-Lever.Ni
					default: ///release key		
					//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
					#ifdef ELAN_DEBUG
				  		printk("[ELAN ] test tpd up\n");
					#endif
					//>2014/11/18-Y46170-Lever.Ni
						if (button_state == ELAN_KEY_1)
						{
							input_report_key(idev, KEY_BACK, 0);
						}
						else if (button_state == ELAN_KEY_4)
						{
							//<2014/03/28-samhuang, change menu key to recent key
							input_report_key(idev, /*KEY_RECENT*/KEY_HOMEPAGE, 0);
							//>2014/03/28-samhuang
						}
						//<2014/11/07-Y44692-Lever.Ni,[Y20][BUG][Common][BSP][Internal][]fix touch panel button issue
						else if (button_state == ELAN_KEY_5)
						{
							input_report_key(idev, KEY_RECENT/*KEY_MENU*/, 0);
						}
						//>2014/11/07-Y44692-Lever.Ni
						else
						{						
							input_report_key(idev, BTN_TOUCH, 0);
							input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 0);
							input_report_abs(idev, ABS_MT_WIDTH_MAJOR, 0);
						}
						input_mt_sync(idev);
                				button_state = 0;
               					break;
				#else

				    	case ELAN_KEY_BACK:
						printk("KEY back 1\n");
									#ifndef LCT_VIRTUAL_KEY
			                        input_report_key(idev, KEY_BACK, 1);
									#else
									input_report_key(idev, BTN_TOUCH, 1);
									input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 8);
									input_report_abs(idev, ABS_MT_POSITION_X, 617);
									input_report_abs(idev, ABS_MT_POSITION_Y, 1360);
									#endif
			            button_state = KEY_BACK;
						break;
						
					case ELAN_KEY_HOME:
						printk("KEY home 1\n");
									#ifndef LCT_VIRTUAL_KEY
			                        input_report_key(idev, KEY_HOMEPAGE, 1);
									#else
									input_report_key(idev, BTN_TOUCH, 1);
									input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 8);
									input_report_abs(idev, ABS_MT_POSITION_X, 365);
									input_report_abs(idev, ABS_MT_POSITION_Y, 1360);
									#endif
			                        button_state = KEY_HOMEPAGE;
						break;
						
					case ELAN_KEY_MENU:
						printk("KEY menu 1\n");
									#ifndef LCT_VIRTUAL_KEY
			                        input_report_key(idev, KEY_MENU, 1);
									#else
									input_report_key(idev, BTN_TOUCH, 1);
									input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 8);
									input_report_abs(idev, ABS_MT_POSITION_X, 107);
									input_report_abs(idev, ABS_MT_POSITION_Y, 1360);
									#endif
			                        button_state = KEY_MENU;
							break;
				
			     // TOUCH release
						default: 		
				  			printk("[ELAN ] test tpd up\n");
								input_report_key(idev, BTN_TOUCH, 0);
								input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 0);
								input_report_abs(idev, ABS_MT_WIDTH_MAJOR, 0);
								input_mt_sync(idev);
                tpd_down_flag = 0;
               break;
				 #endif
				    }
				//<2014/11/07-Y44692-Lever.Ni,[Y20][BUG][Common][BSP][Internal][]fix touch panel button issue				  
                input_sync(idev);   
				//>2014/11/07-Y44692-Lever.Ni
		     
		}
			else 
			{			
				//dev_dbg(&client->dev, "[elan] %d fingers\n", num);                        
				input_report_key(idev, BTN_TOUCH, 1);
				for (i = 0; i < finger_num; i++) 
				{	
					if ((fbits & 0x01)) 
					{
						elan_ktf2k_ts_parse_xy(&buf[idx], &x, &y);  
						//elan_ktf2k_ts_parse_xy(&buf[idx], &y, &x); 
  				//		printk("[elan_debug liulanqing] %s, x=%d, y=%d\n",__func__, x , y);
  				//<2015/02/11-Lever.ni [V10][BUG][Common][BSP][Internal][]add firmware 5505 
  				             printk("tp fwversion=%x\n\r", FW_VERSION);
  				             if (FW_VERSION == 0x5501 || FW_VERSION == 0x5502 || FW_VERSION == 0x5504) {
						x = X_RESOLUTION-x;	 
						//y = Y_RESOLUTION-y; 
                                             }
						#if 1
                        if(X_RESOLUTION > 0 && Y_RESOLUTION > 0)
                        {
                            x = ( x * LCM_X_MAX )/X_RESOLUTION;
                            y = ( y * LCM_Y_MAX )/Y_RESOLUTION;
                            //printk("touchpanel x=%d, y=%d\n\r", x, y);
                        }
                        //>2015/02/11-Lever.ni 
                        else
                        {
                            x = ( x * LCM_X_MAX )/ELAN_X_MAX;
                            y = ( y * LCM_Y_MAX )/ELAN_Y_MAX;
                        }
						#endif 		 

						//x = ( x * LCM_X_MAX )/ELAN_X_MAX;
						//y = ( y * LCM_Y_MAX )/ELAN_Y_MAX;
						#ifdef ELAN_DEBUG
						printk("[elan_debug  BTN bit] %s, x=%d, y=%d\n",__func__, x , y);
						#endif
						///x = LCM_X_MAX-x;	 
						///y = LCM_Y_MAX-y;
							#if 0
							{
								x = LCM_X_MAX-x;	 
								
								y = LCM_Y_MAX-y;
							}
							#endif
						//if (!((x<=0) || (y<=0) || (x>=LCM_X_MAX) || (y>=LCM_Y_MAX))) 
						{   
							input_report_key(idev, BTN_TOUCH, 1);
							input_report_abs(idev, ABS_MT_TRACKING_ID, i);
							input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 8);
							input_report_abs(idev, ABS_MT_POSITION_X, x);
							input_report_abs(idev, ABS_MT_POSITION_Y, y);
							input_mt_sync(idev);
							reported++;
							tpd_down_flag=1;
					  	} // end if border
					} // end if finger status
				  	fbits = fbits >> 1;
				  	idx += 3;
				} // end for
			}
			if (reported)
				input_sync(idev);
			else 
			{
				input_mt_sync(idev);
				input_sync(idev);
			}
			break;
	   	default:
					printk("[elan] %s: unknown packet type: %0x\n", __func__, buf[0]);
				break;
	} // end switch
	//<2014/12/18-Lever.Ni, [Y20][Feature][Common][BSP][][]add touch lock attribute file
    if (buf[0] == 0xFA && buf[1] == 0xCE) { 
        if (buf[2] == 0xAA && buf[3] == 0xAA) {  //face close key-E
            input_report_key(idev, KEY_E, 1);
            input_report_key(idev, KEY_E, 0);
            input_sync(idev);
            printk("touch lock face close\n\r");
        }
        else if (buf[2] == 0x55 && buf[3] == 0x55) {  //face away key-F
            input_report_key(idev, KEY_F, 1);
            input_report_key(idev, KEY_F, 0);
            input_sync(idev);
            printk("touch lock face away\n\r");
        }
    }
    //>2014/12/18-Lever.Ni
//<2015/02/03-Lever.Ni,[V10][Feature][Common][BSP][][]add touch panel knock code function
#ifdef KNOCK 
    if(test_bit(KCODE_EN,  &knock_flag) && buf[0] == 0x88 && buf[2] == 0xff && buf[3]==0xff && buf[4]==0xff &&
		buf[5]==0xff && buf[6] == 0xff && buf[7]==0xff) {		
	    if (test_bit(KON_FLAG, &knock_flag) == 1) {
		if (buf[1] == 0x0a) {
                        printk("knock_code knock on success\n\r");
            //<2014/12/17-Lever.Ni, [Y20][Feature][Common][BSP][][]modify knock on report key
			input_report_key(idev,KEY_YEN,1);
			input_report_key(idev,KEY_YEN,0);
            //<2014/12/17-Lever.Ni
			input_sync(idev);
		}
		else if (buf[1] == 0x0F) {
                        printk("knock_code knock on fail\n\r");
			input_report_key(idev,KEY_D,1);
			input_report_key(idev,KEY_D,0);
			input_sync(idev);
		}
	    }
//<2014/11/10-Y45993-Lever.Ni,[Y20][Feature][Common][BSP][][]Add touch panel knock code & on function
		if (test_bit(KCODE_FLAG, &knock_flag) == 1) {
			if(buf[1]==0x0A) {
                                printk("knock_code knock code on success\n\r");
                //<2014/12/17-Lever.Ni, [Y20][Feature][Common][BSP][][]modify knock on report key
				input_report_key(idev,KEY_YEN,1);
				input_report_key(idev,KEY_YEN,0);
                //>2014/12/17-Lever.Ni
				input_sync(idev);
			}
			else if(buf[1] == 0x0B) {		
                                printk("knock_code knock code success\n\r");
				input_report_key(idev,KEY_HANGEUL,1);
				input_report_key(idev,KEY_HANGEUL,0);  //A
				input_sync(idev);
			}
			else if (buf[1] == 0x0F) {
                                printk("knock_code knock code fail\n\r");
				input_report_key(idev,KEY_HANJA,1);
				input_report_key(idev,KEY_HANJA,0);
				input_sync(idev);
			}
		}
	}
//>2014/11/10-Y45993-Lever.Ni
#endif 
//>2015/02/03-Lever.Ni
        printk("touch panel isr end!\n\r");
	return;
}
#endif
static void elan_ktf2k_ts_work_func(struct work_struct *work)
{
	int rc;
	struct elan_ktf2k_ts_data *ts =
	container_of(work, struct elan_ktf2k_ts_data, work);
	uint8_t buf[PACKET_SIZE] = { 0 };

//		if (gpio_get_value(ts->intr_gpio))
		if (mt_get_gpio_in(GPIO_CTP_EINT_PIN))
		{
			//enable_irq(ts->client->irq);
			printk("[elan]: Detected Jitter at INT pin. \n");
			mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
			return;
		}
	
		rc = elan_ktf2k_ts_recv_data(ts->client, buf);
 
		if (rc < 0)
		{
			//enable_irq(ts->client->irq);
			printk("[elan] elan_ktf2k_ts_recv_data Error, Error code %d \n", rc);
			mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
			return;
		}

		//printk("[elan] %2x,%2x,%2x,%2x,%2x,%2x\n",buf[0],buf[1],buf[2],buf[3],buf[5],buf[6]);
		elan_ktf2k_ts_report_data(ts->client, buf);

		//enable_irq(ts->client->irq);
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	return;
}

static irqreturn_t elan_ktf2k_ts_irq_handler(int irq, void *dev_id)
{
	struct elan_ktf2k_ts_data *ts = dev_id;
	struct i2c_client *client = ts->client;

	dev_dbg(&client->dev, "[elan] %s\n", __func__);
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
 	tpd_flag = 1;
 	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}

static int elan_ktf2k_ts_register_interrupt(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int err = 0;

	err = request_irq(client->irq, elan_ktf2k_ts_irq_handler,
											IRQF_TRIGGER_LOW, client->name, ts);
	if (err)
		dev_err(&client->dev, "[elan] %s: request_irq %d failed\n",
				__func__, client->irq);

	return err;
}

static int touch_event_handler(void *unused)
{
	int rc;
	uint8_t buf[PACKET_SIZE] = { 0 };
        uint8_t cmd_lock[] = {0x54,0xc1,0x00,0x01};
	int touch_state = 3;
//	int button_state = 0;
	unsigned long time_eclapse;
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);
	int last_key = 0;
	int key;
	int index = 0;
	int i =0;

	do
	{
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		enable_irq(CUST_EINT_TOUCH_PANEL_NUM);
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		set_current_state(TASK_RUNNING);
		disable_irq(CUST_EINT_TOUCH_PANEL_NUM);
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
		rc = elan_ktf2k_ts_recv_data(private_ts->client, buf);

		if (rc < 0)
		{
			printk("[elan] rc<0\n");
	
			continue;
		}
        //<2014/12/26-Lever.Ni, [Y20][Feature][Common][BSP][][]add touch panel detect wider place  
            if (buf[0] == 0x55 && buf[1] == 0x55 && buf[2] == 0x55 && buf[3] == 0x55) {
                if (test_bit(LOCK_FLAG, &knock_flag) == 1) {
                    if (i2c_master_send(private_ts->client, cmd_lock, sizeof(cmd_lock) ) != sizeof(cmd_lock)) {
                        printk("touchpanel lock command send failed!\n\r");
                        return;
                    }
                }
            }
            //>2014/12/26-Lever.Ni
		elan_ktf2k_ts_report_data(/*ts*/private_ts->client, buf);

	}while(!kthread_should_stop());

	return 0;
}

static int tpd_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
    strcpy(info->type, TPD_DEVICE);
    return 0;
}

static void tpd_eint_interrupt_handler(void)
{
//    printk("[elan]TPD int\n");
#ifdef ESD_CHECK	
		have_interrupts = 1;
#endif	
    tpd_flag = 1;
    printk("touch panel isr begin!\n\r");
    wake_up_interruptible(&waiter);
}



//==============================================================================
// KnockOn related function
//==============================================================================
//<2014/03/12 ShermanWei, for KnockOn wakeup
void wakeup_touchpanel(void)
{
    printk("[elan] ELAN wakeup_touchpanel...\n");
    tp_wakeup_thread_timeout = 1;
    wake_up(&tp_wakeup_thread_wq);
    wake_lock(&tpwakeupThread_lock);
}

void touch_wakeup_eint_irq(void)
{
    printk("[elan] ELAN touch_wakeup_eint_irq...\n");
    wakeup_touchpanel();
}

static int tpwakeup_thread_kthread(void *x)
{
    struct input_dev *idev = tpd->dev;
    printk("[elan] ELAN tpwakeup_thread_kthread...\n");

    /* Run on a process content */
    while (1) {
        mutex_lock(&tpwakeup_mutex);
	
        printk("[elan] ELAN tpwakeup_thread_kthread running...\n");
        //--------------------------------------------------------------------------------
	input_report_key(idev, KEY_POWER, 1);
	input_sync(idev);
        mdelay(1);                     
        //--------------------------------------------------------------------------------
	input_report_key(idev, KEY_POWER, 0);
	input_sync(idev);
        
     //   mt_eint_unmask(CUST_EINT_TP_WAKEUP_NUM);
	
        mutex_unlock(&tpwakeup_mutex);
	wake_unlock(&tpwakeupThread_lock);
        wait_event(tp_wakeup_thread_wq, tp_wakeup_thread_timeout);
        tp_wakeup_thread_timeout=0;
    }
    return 0;
}
//>2014/03/12 ShermanWei,
//==============================================================================

static int tpd_local_init(void)
{
    printk("[elan]: I2C Touchscreen Driver init\n");
    if(i2c_add_driver(&tpd_i2c_driver) != 0)
    {
        printk("[elan]: unable to add i2c driver.\n");
        return -1;
    }

#ifdef TPD_HAVE_BUTTON
#ifdef LCT_VIRTUAL_KEY
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
 #endif
#endif
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif
//<2014/03/12 ShermanWei, for KnockOn wakeup
    wake_lock_init(&tpwakeupThread_lock, WAKE_LOCK_SUSPEND, "tpwakeupThread wakelock");
//>2014/03/12 ShermanWei,
    printk("end %s, %d\n", __FUNCTION__, __LINE__);
    tpd_type_cap = 1;
    return 0;
}

static int tpd_suspend(struct i2c_client *client, pm_message_t message)
{
    int retval = TPD_OK;
    static char data = 0x3;
//<2014/12/12-Lever.Ni, [Y20][BUG][Common][BSP][][]release touch event when tp suspend
    struct input_dev *idev = tpd->dev;
//>2014/12/12-Lever.Ni
//<2014/11/07-Y45861-Lever.Ni,[Y20][Feature][Common][BSP][][]add touch panel knock code function
#if 0
    uint8_t cmd[] = {CMD_W_PKT, 0x50, 0x00, 0x01};
//<2014/03/12 ShermanWei, for KnockOn wakeup
    uint8_t cmd1[] = {CMD_W_PKT, 0x54, 0x00, 0x01};///standby mode
    uint8_t cmd2[] = {CMD_W_PKT, 0x5E, 0x10, 0x00};///knock on
#endif
	uint8_t cmd[] = {0x54,0x50,0x00,0x01};
//>2014/11/07-Y45861-Lever.Ni
#if 0//defined(ARIMA_LO1_HW) ||  defined(ARIMA_LO2_HW)  
//<2014/04/11 ShermanWei,
	private_ts->client->addr &= ~I2C_DMA_FLAG;
//>2014/04/11 ShermanWei,
if (u8knockon_enable ==1)
{    
    printk("[elan] TP enter into tpwakeup mode...\n");
    if ((i2c_master_send(private_ts->client, cmd2, sizeof(cmd2))) != sizeof(cmd2))
    {
			printk("[elan] %s: i2c_master_send 2 failed\n", __func__);
			return -retval;
    }
    msleep(2);
    if ((i2c_master_send(private_ts->client, cmd1, sizeof(cmd1))) != sizeof(cmd1)) 
    {
			printk("[elan] %s: i2c_master_send 1 failed\n", __func__);
			return -retval;
    }
}
else
{
    printk("[elan] TP enter into sleep mode...\n");
    if ((i2c_master_send(private_ts->client, cmd, sizeof(cmd))) != sizeof(cmd)) 
    {
			printk("[elan] %s: i2c_master_send failed\n", __func__);
			return -retval;
    }
}
#else
#ifdef ESD_CHECK	
	cancel_delayed_work_sync(&esd_work);
#endif
//<2014/12/12-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]release all touch event when tp suspend
    input_mt_sync(idev);
    input_sync(idev);
//>2014/12/12-Lever.Ni
//<2014/11/07-Y45861-Lever.Ni,[Y20][Feature][Common][BSP][][]add touch panel knock code function
	if (test_bit(KCODE_EN, &knock_flag) == 0) {
//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#ifdef ELAN_DEBUG
	    printk(KERN_ERR "knock_code [elan] TP enter into sleep mode...\n");
#endif
//>2014/11/18-Y46170-Lever.Ni
	    if ((i2c_master_send(private_ts->client, cmd, sizeof(cmd))) != sizeof(cmd)) 
	    {
			printk(KERN_ERR "knock_code [elan] %s: i2c_master_send failed\n", __func__);
			return -retval;
	    }
	    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	    set_bit(KSLEEP_FLAG, &knock_flag);
	}
	else if (test_bit(KCODE_EN, &knock_flag) == 1) {
		int ret = 0;
//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#ifdef ELAN_DEBUG
		printk(KERN_ERR "knock_code suspend knock_flag==1\n\r");
#endif
//>2014/11/18-Y46170-Lever.Ni
		if (knock_cmd[1] == KNOCK_CODE) {

			ret = i2c_master_send(private_ts->client, knock_cmd,  sizeof(knock_cmd));
			if (ret < 0) {
				printk(KERN_ERR "knock_code send error\n\r");
				return -retval;
			}
			msleep(2);
			ret = i2c_master_send(private_ts->client, knock_code,  sizeof(knock_code));
			if (ret < 0) {
				printk(KERN_ERR "knock_code send error 2\n\r");
				return -retval;
			}
			set_bit(KCODE_FLAG, &knock_flag);
			clear_bit(KON_FLAG, &knock_flag);
		} 
		else if (knock_cmd[1] == KNOCK_ON) {
//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#ifdef ELAN_DEBUG
			printk("knock_code suspend knock on\n\r");
#endif
//>2014/11/18-Y46170-Lever.Ni
			ret = i2c_master_send(private_ts->client, knock_cmd,  sizeof(knock_cmd));
			if (ret < 0) {
				printk(KERN_ERR "knock_code send error\n\r");
				return -retval;
			}
			set_bit(KON_FLAG, &knock_flag);
			clear_bit(KCODE_FLAG, &knock_flag);
		}
	}
#endif
//>2014/03/12 ShermanWei,
    //mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
//>2014/11/07-Y45861-Lever.Ni
    return retval;
}


static int tpd_resume(struct i2c_client *client)
{
    int retval = TPD_OK;
    uint8_t cmd[] = {CMD_W_PKT, 0x58, 0x00, 0x01};
//<2014/11/18-Y46170-Lever.Ni, [Y20][BUG][Common][BSP][Internal][]remove frequency report log
#ifdef ELAN_DEBUG
    printk("[elan] TPD wake up\n");
#endif
//>2014/11/18-Y46170-Lever.Ni
#ifdef ESD_CHECK
	queue_delayed_work(esd_wq, &esd_work, delay);	
#endif
#if 1	
	// Reset Touch Pannel
    mt_set_gpio_mode( GPIO_CTP_RST_PIN, GPIO_MODE_00 );
    mt_set_gpio_dir( GPIO_CTP_RST_PIN, GPIO_DIR_OUT );
    mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
    mdelay(10);
//#if !defined(EVB)
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
//#endif
    mdelay(10);
    mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );

#else 
    if ((i2c_master_send(private_ts->client, cmd, sizeof(cmd))) != sizeof(cmd)) 
    {
		printk("[elan] %s: i2c_master_send failed\n", __func__);
		return -retval;
    }

    msleep(200);
#endif
//<2014/11/07-Y45861-Lever.Ni,[Y20][Feature][Common][BSP][][]add touch panel knock code function
   if (test_bit(KSLEEP_FLAG, &knock_flag) == 1)
   	{
   		clear_bit(KSLEEP_FLAG, &knock_flag);
    		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
   	}
//>2014/11/07-Y45861-Lever.Ni
    return retval;
}


static struct tpd_driver_t tpd_device_driver =
{
    .tpd_device_name = "ektf2k_mtk",       
    .tpd_local_init = tpd_local_init,
    .suspend = tpd_suspend,
    .resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
#else
    .tpd_have_button = 0,
#endif
};


static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	int fw_err = 0;
	int New_FW_ID;	
	int New_FW_VER;	
	int i;
	int retval = TPD_OK;
	static struct elan_ktf2k_ts_data ts;

	client->addr |= I2C_ENEXT_FLAG;

	
	printk("[elan] %s:client addr is %x, TPD_DEVICE = %s\n",__func__,client->addr,TPD_DEVICE);
	printk("[elan] %s:I2C_WR_FLAG=%x,I2C_MASK_FLAG=%x,I2C_ENEXT_FLAG =%x\n",__func__,I2C_WR_FLAG,I2C_MASK_FLAG,I2C_ENEXT_FLAG);
	//client->timing =  100;

	printk("[elan]%x=IOCTL_I2C_INT\n",IOCTL_I2C_INT);
	printk("[elan]%x=IOCTL_IAP_MODE_LOCK\n",IOCTL_IAP_MODE_LOCK);
	printk("[elan]%x=IOCTL_IAP_MODE_UNLOCK\n",IOCTL_IAP_MODE_UNLOCK);

#if 1
	client->timing = 400;
	i2c_client = client;
	private_ts = &ts;
	private_ts->client = client;
	//private_ts->addr = 0x2a;
#endif

	#if 0
	hwPowerOn(MT65XX_POWER_LDO_VGP5, VOL_2800, "TP");
  hwPowerOn(MT65XX_POWER_LDO_VGP5, VOL_1800, "TP_ENT");

	#endif
	//<2014/02/25 ShermanWei,
#if 1
	hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_3300, "TP");
#endif
	//>2014/02/25 ShermanWei,
	msleep(10);
#if 0
	/*LDO enable*/
	mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);
	msleep(50);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
#endif
	printk("[elan] ELAN enter tpd_probe_ ,the i2c addr=0x%x", client->addr);
	printk("GPIO43 =%d,GPIO_CTP_EINT_PIN =%d,GPIO_DIR_IN=%d,CUST_EINT_TOUCH_PANEL_NUM=%d",GPIO43,GPIO_CTP_EINT_PIN,GPIO_DIR_IN,CUST_EINT_TOUCH_PANEL_NUM);

// Reset Touch Pannel
    mt_set_gpio_mode( GPIO_CTP_RST_PIN, GPIO_MODE_00 );
    mt_set_gpio_dir( GPIO_CTP_RST_PIN, GPIO_DIR_OUT );
    mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
    mdelay(10);
//#if !defined(EVB)
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
//#endif
    mdelay(10);
    mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
// End Reset Touch Pannel	

    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
//<2014/03/12 ShermanWei, for KnockOn wakeup
#if 0
    mt_set_gpio_mode(GPIO_CTP_WAKEUP_PIN, GPIO_CTP_WAKEUP_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_WAKEUP_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_WAKEUP_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_WAKEUP_PIN, GPIO_PULL_UP);
#endif
//>2014/03/12 ShermanWei,
    msleep( 200 );

#ifdef HAVE_TOUCH_KEY
int retry;
	for(retry = 0; retry <3; retry++)
	{
		input_set_capability(tpd->dev,EV_KEY,tpd_keys_local[retry]);
	}
#endif

// Setup Interrupt Pin
#ifdef KNOCK
//<2014/11/07-Y45861-Lever.Ni,[Y20][Feature][Common][BSP][][]add touch panel knock code function
//<2014/12/17-Lever.Ni, [Y20][Feature][Common][BSP][][]modify knock on report key
	set_bit( KEY_POWER,  tpd->dev->keybit );
        set_bit( KEY_YEN,  tpd->dev->keybit );
//<2014/12/17-Lever.Ni
	set_bit(KEY_HANGEUL,tpd->dev->keybit);
	set_bit(KEY_HANJA,tpd->dev->keybit);
	set_bit(KEY_C,tpd->dev->keybit);
	set_bit(KEY_D,tpd->dev->keybit);
    set_bit(KEY_F, tpd->dev->keybit);
    set_bit(KEY_E, tpd->dev->keybit);
    //<2015/3/5-V0089-Chongyang.Hu, [V10][Bug][Common][KnockCode][CQ][NJSCM00053152]Phone will be automatically locked when tap "A" or "B" on BT keyboard
    set_bit(KEY_HANGEUL, tpd->dev->keybit);
    set_bit(KEY_HANJA, tpd->dev->keybit);
    //>2015/3/5-V0089-Chongyang.Hu
	elan_tp_knock_init();
//>2014/11/07-Y45861-Lever.Ni
#endif
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);


	mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE);
	mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINTF_TRIGGER_FALLING, tpd_eint_interrupt_handler, 1);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
//<2014/03/12 ShermanWei, for KnockOn wakeup
#if 0
    //EINT setting
    mt_eint_set_sens(       CUST_EINT_TP_WAKEUP_NUM,
                                CUST_EINT_TP_WAKEUP_SENSITIVE);
    mt_eint_set_polarity(   CUST_EINT_TP_WAKEUP_NUM,
                                CUST_EINT_TP_WAKEUP_POLARITY );        // set positive polarity
    mt_eint_set_hw_debounce(CUST_EINT_TP_WAKEUP_NUM,
                                CUST_EINT_TP_WAKEUP_DEBOUNCE_CN);     // set debounce time
    mt_eint_registration(   CUST_EINT_TP_WAKEUP_NUM,
                                CUST_EINT_TP_WAKEUP_DEBOUNCE_EN,
                                CUST_EINT_TP_WAKEUP_POLARITY ,
                                touch_wakeup_eint_irq,
                                0);
#endif
//>2014/03/12 ShermanWei,
	msleep(100);
// End Setup Interrupt Pin	
	tpd_load_status = 1;

#ifdef MTK6589_DMA    
    gpDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 4096, &gpDMABuf_pa, GFP_KERNEL);
    if(!gpDMABuf_va){
		printk(KERN_INFO "[elan] Allocate DMA I2C Buffer failed\n");
    }
#endif



mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	fw_err = elan_ktf2k_ts_setup(client);
	if (fw_err < 0) {
		printk(KERN_INFO "[elan] No Elan chip inside\n");
	}
mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
//<2014/03/12 ShermanWei, for KnockOn wakeup
//mt_eint_unmask(CUST_EINT_TP_WAKEUP_NUM);
//>2014/03/12 ShermanWei,

#if 1 /*RESET RESOLUTION: tmp use ELAN_X_MAX & ELAN_Y_MAX*/ 
	printk("[elan] RESET RESOLUTION\n");
	input_set_abs_params(tpd->dev, ABS_X, 0,  ELAN_X_MAX, 0, 0);
	input_set_abs_params(tpd->dev, ABS_Y, 0,  ELAN_Y_MAX, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_POSITION_X, 0, ELAN_X_MAX, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_POSITION_Y, 0, ELAN_Y_MAX, 0, 0);
	input_set_abs_params( tpd->dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0); //0210
#endif 

	#ifndef LCT_VIRTUAL_KEY
	set_bit( KEY_BACK,  tpd->dev->keybit );
    set_bit( KEY_HOMEPAGE,  tpd->dev->keybit );
    set_bit( KEY_MENU,  tpd->dev->keybit );
    set_bit( KEY_RECENT,  tpd->dev->keybit );
//<2014/03/12 ShermanWei, for KnockOn wakeup
	set_bit( KEY_POWER,  tpd->dev->keybit );
//>2014/03/12 ShermanWei,
	#endif
	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
//<2014/03/12 ShermanWei, for KnockOn wakeup
//<2014/11/07-Y45861-Lever.Ni,[Y20][Feature][Common][BSP][][]add touch panel knock code function
//	kthread_run(tpwakeup_thread_kthread, 0, TPD_DEVICE);
//>2014/11/07-Y45861-Lever.Ni
//>2014/03/12 ShermanWei,
//<2014/03/19 ShermanWei, for TP attributr interface
	err = touch_create_attr(&(tpd_device_driver.platform_driv_addr->driver));
	if(err)
	printk("[elan] ELAN creates attr fail\n");
    //<2014/12/18-Lever.Ni, [Y20][Feature][Common][BSP][][]add touch lock attribute file
        err = driver_create_file(&(tpd_device_driver.platform_driv_addr->driver), &driver_attr_lock_enable);
        if (err) {
            printk(KERN_ERR "touch panel lock driver create file failed\n\r");
            return err;
        }
        //>2014/12/18-Lever.Ni
//>2014/03/19 ShermanWei,
	if(IS_ERR(thread))
	{
		retval = PTR_ERR(thread);
		printk(TPD_DEVICE "[elan]  failed to create kernel thread: %d\n", retval);
	}

	printk("[elan]  ELAN Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
// Firmware Update
	// MISC
  	ts.firmware.minor = MISC_DYNAMIC_MINOR;
  	ts.firmware.name = "elan-iap";
  	ts.firmware.fops = &elan_touch_fops;
  	ts.firmware.mode = S_IRWXUGO; 
   	
  	if (misc_register(&ts.firmware) < 0)
  		printk("[elan] misc_register failed!!");
  	else
  	  printk("[elan] misc_register finished!!"); 
	


#if IAP_PORTION
	if(1)
	{
		work_lock=1;
		disable_irq(CUST_EINT_TOUCH_PANEL_NUM);
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
		power_lock = 1;
//<2014/06/12 ShermanWei, 2nd source TP
	//	printk("[elan] start fw update,sensor option: 0x%4.4x", SENSOR_OPT);
//<2014/12/09-Lever.Ni, [Y20][Feature][Common][BSP][][]show vendor id		
		if (FW_ID == SECOND_SENSOR)
		    memcpy( file_fw_data, file_fw_data_second, sizeof(file_fw_data_second) );
//>2014/12/09-Lever.Ni
//>2014/06/12 ShermanWei,

/* FW ID & FW VER*/
#if 1  /* For ektf21xx and ektf20xx  */
//<2015/02/03-Lever.ni [V10][Feature][Common][BSP][][]read firmware id & version from .i file
    printk("[ELAN]  [7bd0]=0x%02x,  [7bd1]=0x%02x, [7bd2]=0x%02x, [7bd3]=0x%02x\n",  file_fw_data[0x7d64],file_fw_data[0x7d65],file_fw_data[0x7d66],file_fw_data[0x7d67]);
		New_FW_ID = file_fw_data[0x7d67]<<8  | file_fw_data[0x7d66] ;	       
		New_FW_VER = file_fw_data[0x7d65]<<8  | file_fw_data[0x7d64] ;
//>2015/02/03-Lever.ni
#endif
		
#if 0   /* for ektf31xx 2 wire ice ex: 2wireice -b xx.bin */
    printk(" [7c16]=0x%02x,  [7c17]=0x%02x, [7c18]=0x%02x, [7c19]=0x%02x\n",  file_fw_data[31766],file_fw_data[31767],file_fw_data[31768],file_fw_data[31769]);
		New_FW_ID = file_fw_data[31769]<<8  | file_fw_data[31768] ;	       
		New_FW_VER = file_fw_data[31767]<<8  | file_fw_data[31766] ;
#endif	
#if 0
    /* for ektf31xx iap ekt file   */	
    printk(" [7bd8]=0x%02x,  [7bd9]=0x%02x, [7bda]=0x%02x, [7bdb]=0x%02x\n",  file_fw_data[31704],file_fw_data[31705],file_fw_data[31706],file_fw_data[31707]);
		New_FW_ID = file_fw_data[31707]<<8  | file_fw_data[31708] ;	       
		New_FW_VER = file_fw_data[31705]<<8  | file_fw_data[31704] ;
#endif
#if 0
		/*For ektf31xx iap ekt file   */       
		printk("[ELAN] [0x7d64]=0x%02x,  [0x7d65]=0x%02x, [0x7d66]=0x%02x, [0x7d67]=0x%02x\n",  file_fw_data[32100],file_fw_data[32101],file_fw_data[32102],file_fw_data[32103]);    
	         New_FW_ID = file_fw_data[32103]<<8  | file_fw_data[32102] ;		
	         New_FW_VER = 0x55 <<8	| file_fw_data[32100] ;
#endif
	  	printk(" FW_ID=0x%x,   New_FW_ID=0x%x \n",  FW_ID, New_FW_ID);   	       
		printk(" FW_VERSION=0x%x,   New_FW_VER=0x%x \n",  FW_VERSION  , New_FW_VER);  
		
		#if 1
		if (New_FW_VER   ==  FW_VERSION)
		{		      
			printk("[elan] FW_ID  is same , no need update\n\r");	
		} 
		else if (New_FW_VER > FW_VERSION) {          //fw_Version is ic, New_fw_version is .i              
//<2014/06/12 ShermanWei, 2nd source TP
			//if (SENSOR_OPT != SECOND_SENSOR)	{
			//<2015/02/11-Lever.ni [V10][BUG][Common][BSP][Internal][]compatible previous firmware
			FW_VERSION = New_FW_VER;
                        //>2015/02/11-Lever.ni
		        printk("[elan] FW_IDupdate!");	
                        printk("[elan] recover=%d\n\r", RECOVERY);
		        Update_FW_One(client, RECOVERY);
			//}
//>2014/06/12 ShermanWei,
		}
                else {
                    printk("[elan] ic firmware is lastest, no need to update\n\r");
                }
		#endif
		
        // End Firmware Update  
        //---------add in probe----------
#ifdef ESD_CHECK	
            INIT_DELAYED_WORK(&esd_work, elan_touch_esd_func);
            esd_wq = create_singlethread_workqueue("esd_wq");   
            if (!esd_wq) {
                retval = -ENOMEM;
        
            }
            
            queue_delayed_work(esd_wq, &esd_work, delay);
            
#endif

		work_lock=0;
		enable_irq(CUST_EINT_TOUCH_PANEL_NUM);
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	}
#endif
    
    return 0;

}

//-----workqueue-----
#ifdef ESD_CHECK
static void elan_touch_esd_func(struct work_struct *work)
{	
	int res;	
    int retval = TPD_OK;
	//uint8_t cmd[] = {0x53, 0x00, 0x00, 0x01};	
	//struct i2c_client *client = private_ts->client;	
	printk("[elan esd] %s: enter.......\n", __FUNCTION__); 		/* elan_dlx */
	
	if(have_interrupts == 1){
		printk("[elan esd] %s: had interrup not need check\n", __func__);
	}
	else{
/*
		res = i2c_master_send(client, cmd, sizeof(cmd));
		if (res != sizeof(cmd)){
			printk("[elan esd] %s: i2c_master_send failed reset now\n", __func__);
*/
	printk("[elan esd] %s: i2c_master_send successful, no response need reset\n", __func__);
            //reset here
			mt_set_gpio_mode(GPIO_CTP_RST_PIN, 0);
            mt_set_gpio_dir(GPIO_CTP_RST_PIN, 1);
            mt_set_gpio_out(GPIO_CTP_RST_PIN, 0);
            msleep(10);

            // for enable/reset pin
            mt_set_gpio_mode(GPIO_CTP_RST_PIN, 0);
            mt_set_gpio_dir(GPIO_CTP_RST_PIN, 1);
            mt_set_gpio_out(GPIO_CTP_RST_PIN, 1);
            msleep(100);
/*
		}
		else{
			printk("[elan esd] %s: i2c_master_send successful\n", __func__);
			
			msleep(20);
			
			if(have_interrupts == 1){
				printk("[elan esd] %s: i2c_master_send successful, had response\n", __func__);
			}
			else{
				printk("[elan esd] %s: i2c_master_send successful, no response need reset\n", __func__);
                //reset here
                mt_set_gpio_mode(GPIO_CTP_RST_PIN, 0);
                mt_set_gpio_dir(GPIO_CTP_RST_PIN, 1);
                mt_set_gpio_out(GPIO_CTP_RST_PIN, 0);
                msleep(10);

                // for enable/reset pin
                mt_set_gpio_mode(GPIO_CTP_RST_PIN, 0);
                mt_set_gpio_dir(GPIO_CTP_RST_PIN, 1);
                mt_set_gpio_out(GPIO_CTP_RST_PIN, 1);
                msleep(100);
			}
		}
*/
	}
	
	have_interrupts = 0;	
	queue_delayed_work(esd_wq, &esd_work, delay);
	printk("[elan esd] %s: enter.......\n", __FUNCTION__); 		/* elan_dlx */
}
#endif

static int tpd_remove(struct i2c_client *client)

{
    printk("[elan] TPD removed\n");
    
	#ifdef MTK6589_DMA    
    if(gpDMABuf_va){
        dma_free_coherent(NULL, 4096, gpDMABuf_va, gpDMABuf_pa);
        gpDMABuf_va = NULL;
        gpDMABuf_pa = NULL;
    }
	#endif    

    return 0;
}


static int __init tpd_driver_init(void)
{
    printk("[elan]: Driver Verison MTK0005 for MTK65xx serial\n");
#ifdef ELAN_MTK6577		
		printk("[elan] Enable ELAN_MTK6577\n");
	//<2014/02/25 ShermanWei,
	#if 1
	i2c_register_board_info(0, &i2c_tpd, 1);
	#endif
	//>2014/02/25 ShermanWei,
		
#endif		
    if(tpd_driver_add(&tpd_device_driver) < 0)
    {
        printk("[elan]: %s driver failed\n", __func__);
    }
    return 0;
}


static void __exit tpd_driver_exit(void)
{
    printk("[elan]: %s elan touch panel driver exit\n", __func__);
    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);




