/*
 * Driver for CAM_CAL
 *
 *
 */

#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "kd_camera_hw.h"
#include "cam_cal.h"
#include "cam_cal_define.h"
#include "s5k4h5otp.h"
#include <asm/system.h>  // for SMP

//#define CAM_CALGETDLT_DEBUG
#define CAM_CAL_DEBUG_4H5
#ifdef CAM_CAL_DEBUG_4H5
#define CAM_CALDB_4H5 printk
#else
#define CAM_CALDB_4H5(x,...)
#endif


static DEFINE_SPINLOCK(g_CAM_CALLock_4h5); // for SMP
#define CAM_CAL_I2C_BUSNUM 1


/*******************************************************************************
*
********************************************************************************/
#define CAM_CAL_ICS_REVISION 1 //seanlin111208
/*******************************************************************************
*
********************************************************************************/
#define CAM_CAL_DRVNAME_S5K4H5 "H5YX_CAM_CAL_DRV"
#define CAM_CAL_I2C_GROUP_ID 0
/*******************************************************************************
*
********************************************************************************/
static struct i2c_board_info __initdata kd_cam_cal_dev={ I2C_BOARD_INFO(CAM_CAL_DRVNAME_S5K4H5, 0x6c>>1)};

static struct i2c_client * g_pstI2Cclient = NULL;

//81 is used for V4L driver
static dev_t g_CAM_CALdevno = MKDEV(CAM_CAL_DEV_MAJOR_NUMBER,0);
static struct cdev * g_pCAM_CAL_CharDrv = NULL;
//static spinlock_t g_CAM_CALLock_4h5;
//spin_lock(&g_CAM_CALLock_4h5);
//spin_unlock(&g_CAM_CALLock_4h5);

static struct class *CAM_CAL_class = NULL;
static atomic_t g_CAM_CALatomic;

static u32 otp_Start_Addr = 0x3A04;
//static DEFINE_SPINLOCK(kdcam_cal_drv_lock);
//spin_lock(&kdcam_cal_drv_lock);
//spin_unlock(&kdcam_cal_drv_lock);

/*******************************************************************************
*
********************************************************************************/
//Address: 2Byte, Data: 1Byte
static int iWriteCAM_CAL_4h5(u16 a_u2Addr  , u32 a_u4Bytes, u8 puDataInBytes)
{
    int  i4RetValue = 0;
    u32 u4Index = 0;
	int retry = 3;
    char puSendCmd[3] = {(char)(a_u2Addr >> 8) , (char)(a_u2Addr & 0xFF) ,puDataInBytes};
	do {
        CAM_CALDB_4H5("[CAM_CAL_4H5][iWriteCAM_CAL_4h5] Write 0x%x=0x%x \n",a_u2Addr, puDataInBytes);
		i4RetValue = i2c_master_send(g_pstI2Cclient, puSendCmd, 3);
        if (i4RetValue != 3) {
            CAM_CALDB_4H5("[CAM_CAL_4H5] I2C send failed!!\n");
        }
        else {
            break;
    	}
        mdelay(10);
    } while ((retry--) > 0);    
   //CAM_CALDB_4H5("[CAM_CAL_4H5] iWriteCAM_CAL_4h5 done!! \n");
    return 0;
}


//Address: 2Byte, Data: 1Byte
static int iReadCAM_CAL_4h5(u16 a_u2Addr, u32 ui4_length, u8 * a_puBuff)
{
    int  i4RetValue = 0;
    char puReadCmd[2] = {(char)(a_u2Addr >> 8) , (char)(a_u2Addr & 0xFF)};

    //CAM_CALDB_4H5("[CAM_CAL_4H5] iReadCAM_CAL_4h5!! \n");   
    //CAM_CALDB_4H5("[CAM_CAL_4H5] i2c_master_send \n");
    i4RetValue = i2c_master_send(g_pstI2Cclient, puReadCmd, 2);
	
    if (i4RetValue != 2)
    {
        CAM_CALDB_4H5("[CAM_CAL_4H5] I2C send read address failed!! \n");
        return -1;
    }

    //CAM_CALDB_4H5("[CAM_CAL_4H5] i2c_master_recv \n");
    i4RetValue = i2c_master_recv(g_pstI2Cclient, (char *)a_puBuff, ui4_length);
	CAM_CALDB_4H5("[CAM_CAL_4H5][iReadCAM_CAL_4h5] Read 0x%x=0x%x \n", a_u2Addr, a_puBuff[0]);
    if (i4RetValue != ui4_length)
    {
        CAM_CALDB_4H5("[CAM_CAL_4H5] I2C read data failed!! \n");
        return -1;
    } 

    //CAM_CALDB_4H5("[CAM_CAL_4H5] iReadCAM_CAL_4h5 done!! \n");
    return 0;
}

//Burst Write Data
static int iWriteData(unsigned int  ui4_offset, unsigned int  ui4_length, unsigned char * pinputdata)
{
   int  i4RetValue = 0;
   int  i4ResidueDataLength;
   u32 u4IncOffset = 0;
   u32 u4CurrentOffset;
   u8 * pBuff;


   i4ResidueDataLength = (int)ui4_length;
   u4CurrentOffset = ui4_offset;
   pBuff = pinputdata;   


   do 
   {
       CAM_CALDB_4H5("[CAM_CAL_4H5][iWriteData] Write 0x%x=0x%x \n",u4CurrentOffset, pBuff[0]);
       i4RetValue = iWriteCAM_CAL_4h5((u16)u4CurrentOffset, 1, pBuff[0]);
       if (i4RetValue != 0)
       {
            CAM_CALDB_4H5("[CAM_CAL_4H5] I2C iWriteData failed!! \n");
            return -1;
       }           
       u4IncOffset ++;
       i4ResidueDataLength --;
       u4CurrentOffset = ui4_offset + u4IncOffset;
       pBuff = pinputdata + u4IncOffset;
   }while (i4ResidueDataLength > 0);
   CAM_CALDB_4H5("[CAM_CAL_4H5] iWriteData done\n" );
 
   return 0;
}

//Burst Read Data
static int iReadData(unsigned int  ui4_offset, unsigned int  ui4_length, unsigned char * pinputdata)
{
   int  i4RetValue = 0;
   int  i4ResidueDataLength;
   u32 incOffset = 1;
   u32 u4CurrentOffset;
   u8 * pBuff;
//   CAM_CALDB_4H5("[S24EEPORM] iReadData \n" );
   
/*   if (ui4_offset + ui4_length >= 0x2000)
   {
      CAM_CALDB_4H5("[CAM_CAL_4H5] Read Error!! S-24CS64A not supprt address >= 0x2000!! \n" );
      return -1;
   }
   */

   i4ResidueDataLength = (int)ui4_length;
   u4CurrentOffset = ui4_offset;
   pBuff = pinputdata;
   do 
   {
       i4RetValue = iReadCAM_CAL_4h5((u16)u4CurrentOffset, 1, pBuff);
	   //<2015/02/09-joubert.she-[V10][Feature][Common][BSP][][] disable iReadData log 

	   //<2015/02/28-joubert.she-[V10][Feature][Common][BSP][][] function log add 
	   CAM_CALDB_4H5("[CAM_CAL_4H5][iReadData] Read 0x%x=0x%x,pBuff addr 0x%p \n", u4CurrentOffset, *pBuff,pBuff);
		//>2015/02/28-joubert.she
	   //>2015/02/09-joubert.she
	   if (i4RetValue != 0)
       {
            CAM_CALDB_4H5("[CAM_CAL_4H5] I2C iReadData failed!! \n");
            return -1;
       }           
       i4ResidueDataLength --;
       u4CurrentOffset = u4CurrentOffset+incOffset;
       pBuff = pBuff+incOffset;
   }while (i4ResidueDataLength > 0);
//   CAM_CALDB_4H5("[S24EEPORM] iReadData finial address is %d length is %d buffer address is 0x%x\n",u4CurrentOffset, i4ResidueDataLength, pBuff);   
//   CAM_CALDB_4H5("[S24EEPORM] iReadData done\n" );
	
   return 0;
}
//<2015/03/05-joubert.she-[V10][Feature][Common][BSP][][] enable lsc otp
static int Enb_lsc_data(int enb)
{
        u8* pBuff = NULL;
        pBuff = (u8*)kmalloc(1,GFP_KERNEL);
        CAM_CALDB_4H5("[CAM_CAL]Enb_lsc_data=%d\n",enb);
      if(enb)
        {
        *pBuff = 0x00;//enable
	iWriteData(0x3300, 1, pBuff ); //start 
	CAM_CALDB_4H5("[CAM_CAL]Enb_lsc_data OK ~\n");
        }
        else
        {
        *pBuff = 0x01;//disable
	iWriteData(0x3300, 1, pBuff ); //start 
	CAM_CALDB_4H5("[CAM_CAL]Enb_lsc_data disable ~\n");
      }   
    return 0;
}
//>2015/03/05-joubert.she
static int Enb_OTP_Read(int enb){
	u8 flag;
	int	page_count;
	u8* pBuff = NULL;
	pBuff = (u8*)kmalloc(1,GFP_KERNEL);
	
	CAM_CALDB_4H5("[CAM_CAL]Enb_OTP_Read=%d\n",enb);
	if(enb){
	//<2015/02/28-joubert.she-[V10][Feature][Common][BSP][][] read page change
     //   for(page_count=3; page_count>=2; page_count--)
     	page_count=0;
		{
			*pBuff = page_count;
			iWriteData(0x3A02, 1, pBuff ); //set page of otp
			
			*pBuff = 0x01;
			iWriteData(0x3A00, 1, pBuff ); //start 
			mdelay(2);



   

            
            		otp_Start_Addr = 0x3A04;
			iReadCAM_CAL_4h5(otp_Start_Addr, 1, &flag);
			CAM_CALDB_4H5("[CAM_CAL]flag=%x\n",flag);
			if(flag==0x01)
                            {         
                            	CAM_CALDB_4H5("[CAM_CAL_4H5] enable otp successfully ! ");
                				return 1;
                            }
                            else
			{
			        otp_Start_Addr = 0x3A1C;
                                     iReadCAM_CAL_4h5(otp_Start_Addr, 1, &flag);
                                     if(flag==0x01)
                                     CAM_CALDB_4H5("[CAM_CAL_4H5] enable otp successfully ! ");
                				return 1;
				//CAM_CALDB_4H5("[CAM_CAL_4H5] enable otp fail !!!!! ");
				//return 0;
			}
		}
		return 0;
	} 
	else {
		CAM_CALDB_4H5("[CAM_CAL_4H5] disable otp !!!!! ");
                *pBuff = 0x00;
                iWriteData(0x3A00, 1, pBuff ); //ed

	}
}
//>2015/02/28-joubert.she
/*
static void Clear_OTP_Buff(){
	u8 AllZero[OTP_SIZE]={0};
	iWriteData(OTP_START_ADDR, OTP_SIZE, AllZero);
}
*/
/*******************************************************************************
*
********************************************************************************/
#define NEW_UNLOCK_IOCTL
#ifndef NEW_UNLOCK_IOCTL
static int CAM_CAL_Ioctl(struct inode * a_pstInode,struct file * a_pstFile,unsigned int a_u4Command,unsigned long a_u4Param)
#else 
static long CAM_CAL_Ioctl(struct file *file, unsigned int a_u4Command, unsigned long a_u4Param)
#endif
{
    int i4RetValue = 0;
    u8 * pBuff = NULL;
    u8 * pWorkingBuff = NULL;
    stCAM_CAL_INFO_STRUCT *ptempbuf;

#ifdef CAM_CALGETDLT_DEBUG
    struct timeval ktv1, ktv2;
    unsigned long TimeIntervalUS;
#endif

    if(_IOC_NONE == _IOC_DIR(a_u4Command))
    {
    }
    else
    {
        pBuff = (u8 *)kmalloc(sizeof(stCAM_CAL_INFO_STRUCT),GFP_KERNEL);

        if(NULL == pBuff)
        {
            CAM_CALDB_4H5("[CAM_CAL_4H5] ioctl allocate mem failed\n");
            return -ENOMEM;
        }

        if(_IOC_WRITE & _IOC_DIR(a_u4Command))
        {
            if(copy_from_user((u8 *) pBuff , (u8 *) a_u4Param, sizeof(stCAM_CAL_INFO_STRUCT)))
            {    //get input structure address
                kfree(pBuff);
                CAM_CALDB_4H5("[CAM_CAL_4H5] ioctl copy from user failed\n");
                return -EFAULT;
            }
        }
    }

    ptempbuf = (stCAM_CAL_INFO_STRUCT *)pBuff;
    pWorkingBuff = (u8*)kmalloc(ptempbuf->u4Length,GFP_KERNEL); 
    if(NULL == pWorkingBuff)
    {
        kfree(pBuff);
        CAM_CALDB_4H5("[CAM_CAL_4H5] ioctl allocate mem failed\n");
        return -ENOMEM;
    }
     CAM_CALDB_4H5("[CAM_CAL_4H5] init Working buffer address 0x%8x  command is 0x%8x\n", (u32)pWorkingBuff, (u32)a_u4Command);

 
    if(copy_from_user((u8*)pWorkingBuff ,  (u8*)ptempbuf->pu1Params, ptempbuf->u4Length))
    {
        kfree(pBuff);
        kfree(pWorkingBuff);
        CAM_CALDB_4H5("[CAM_CAL_4H5] ioctl copy from user failed\n");
        return -EFAULT;
    } 
    
    switch(a_u4Command)
    {
        case CAM_CALIOC_S_WRITE:    
            CAM_CALDB_4H5("[CAM_CAL_4H5] Write CMD \n");
#ifdef CAM_CALGETDLT_DEBUG
            do_gettimeofday(&ktv1);
#endif            
            i4RetValue = iWriteData((u16)ptempbuf->u4Offset, ptempbuf->u4Length, pWorkingBuff);
#ifdef CAM_CALGETDLT_DEBUG
            do_gettimeofday(&ktv2);
            if(ktv2.tv_sec > ktv1.tv_sec)
            {
                TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
            }
            else
            {
                TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;
            }
            printk("Write data %d bytes take %lu us\n",ptempbuf->u4Length, TimeIntervalUS);
#endif            
            break;
        case CAM_CALIOC_G_READ:
            CAM_CALDB_4H5("[CAM_CAL_4H5] Read CMD \n");
#ifdef CAM_CALGETDLT_DEBUG            
            do_gettimeofday(&ktv1);
#endif 
            CAM_CALDB_4H5("[CAM_CAL_4H5] offset %d \n", ptempbuf->u4Offset);
            CAM_CALDB_4H5("[CAM_CAL_4H5] length %d \n", ptempbuf->u4Length);
			//<2015/02/09-joubert.she-[V10][Feature][Common][BSP][][] disable iReadData log 
			//<2015/02/28-joubert.she-[V10][Feature][Common][BSP][][] log add 
		   CAM_CALDB_4H5("[CAM_CAL_4H5] Before read Working buffer address 0x%p \n", pWorkingBuff);
		   //>2015/02/28-joubert.she
			//>2015/02/09-joubert.she
	   		if(Enb_OTP_Read(1)) //Enable OTP Read	   			
            	i4RetValue = iReadData((u16)(ptempbuf->u4Offset+otp_Start_Addr), ptempbuf->u4Length, pWorkingBuff);
			Enb_OTP_Read(0); //Disable OTP Read

            CAM_CALDB_4H5("[CAM_CAL_4H5] After read Working buffer data  0x%x \n", *pWorkingBuff);

#ifdef CAM_CALGETDLT_DEBUG
            do_gettimeofday(&ktv2);
            if(ktv2.tv_sec > ktv1.tv_sec)
            {
                TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
            }
            else
            {
                TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;
            }
            printk("Read data %d bytes take %lu us\n",ptempbuf->u4Length, TimeIntervalUS);
#endif            

            break;
        default :
      	     CAM_CALDB_4H5("[CAM_CAL_4H5] No CMD \n");
            i4RetValue = -EPERM;
        break;
    }

    if(_IOC_READ & _IOC_DIR(a_u4Command))
    {
        //copy data to user space buffer, keep other input paremeter unchange.
        CAM_CALDB_4H5("[CAM_CAL_4H5] to user length %d \n", ptempbuf->u4Length);
        CAM_CALDB_4H5("[CAM_CAL_4H5] to user  Working buffer value 0x%x \n", *(u32*)pWorkingBuff);
        if(copy_to_user((u8 __user *) ptempbuf->pu1Params , (u8 *)pWorkingBuff , ptempbuf->u4Length))
        {
            kfree(pBuff);
            kfree(pWorkingBuff);
            CAM_CALDB_4H5("[CAM_CAL_4H5] ioctl copy to user failed\n");
            return -EFAULT;
        }
    }

    kfree(pBuff);
    kfree(pWorkingBuff);
    return i4RetValue;
}


static u32 g_u4Opened = 0;
//#define
//Main jobs:
// 1.check for device-specified errors, device not ready.
// 2.Initialize the device if it is opened for the first time.
static int CAM_CAL_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    CAM_CALDB_4H5("[CAM_CAL_4H5] CAM_CAL_Open\n");
	//<2015/03/05-joubert.she-[V10][Feature][Common][BSP][][] enable lsc otp
    Enb_lsc_data(1);//joubertshe
	//>2015/03/05-joubert.she
    spin_lock(&g_CAM_CALLock_4h5);
    if(g_u4Opened)
    {
        spin_unlock(&g_CAM_CALLock_4h5);
		CAM_CALDB_4H5("[CAM_CAL_4H5] Opened, return -EBUSY\n");
        return -EBUSY;
    }
    else
    {
        g_u4Opened = 1;
        atomic_set(&g_CAM_CALatomic,0);
    }
    spin_unlock(&g_CAM_CALLock_4h5);

    return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int CAM_CAL_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    spin_lock(&g_CAM_CALLock_4h5);

    g_u4Opened = 0;

    atomic_set(&g_CAM_CALatomic,0);

    spin_unlock(&g_CAM_CALLock_4h5);

    return 0;
}

static const struct file_operations g_stCAM_CAL_fops =
{
    .owner = THIS_MODULE,
    .open = CAM_CAL_Open,
    .release = CAM_CAL_Release,
    //.ioctl = CAM_CAL_Ioctl
    .unlocked_ioctl = CAM_CAL_Ioctl
};

#define CAM_CAL_DYNAMIC_ALLOCATE_DEVNO 1
inline static int RegisterCAM_CALCharDrv(void)
{
    struct device* CAM_CAL_device = NULL;

#if CAM_CAL_DYNAMIC_ALLOCATE_DEVNO
    if( alloc_chrdev_region(&g_CAM_CALdevno, 0, 1,CAM_CAL_DRVNAME_S5K4H5) )
    {
        CAM_CALDB_4H5("[CAM_CAL_4H5] Allocate device no failed\n");

        return -EAGAIN;
    }
#else
    if( register_chrdev_region(  g_CAM_CALdevno , 1 , CAM_CAL_DRVNAME_S5K4H5) )
    {
        CAM_CALDB_4H5("[CAM_CAL_4H5] Register device no failed\n");

        return -EAGAIN;
    }
#endif

    //Allocate driver
    g_pCAM_CAL_CharDrv = cdev_alloc();

    if(NULL == g_pCAM_CAL_CharDrv)
    {
        unregister_chrdev_region(g_CAM_CALdevno, 1);

        CAM_CALDB_4H5("[CAM_CAL_4H5] Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pCAM_CAL_CharDrv, &g_stCAM_CAL_fops);

    g_pCAM_CAL_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pCAM_CAL_CharDrv, g_CAM_CALdevno, 1))
    {
        CAM_CALDB_4H5("[CAM_CAL_4H5] Attatch file operation failed\n");

        unregister_chrdev_region(g_CAM_CALdevno, 1);

        return -EAGAIN;
    }

    CAM_CAL_class = class_create(THIS_MODULE, "4H5_CAM_CALdrv");
    if (IS_ERR(CAM_CAL_class)) {
        int ret = PTR_ERR(CAM_CAL_class);
        CAM_CALDB_4H5("Unable to create class, err = %d\n", ret);
        return ret;
    }
    CAM_CAL_device = device_create(CAM_CAL_class, NULL, g_CAM_CALdevno, NULL, CAM_CAL_DRVNAME_S5K4H5);

    return 0;
}

inline static void UnregisterCAM_CALCharDrv(void)
{
    //Release char driver
    cdev_del(g_pCAM_CAL_CharDrv);

    unregister_chrdev_region(g_CAM_CALdevno, 1);

    device_destroy(CAM_CAL_class, g_CAM_CALdevno);
    class_destroy(CAM_CAL_class);
}


//////////////////////////////////////////////////////////////////////
#ifndef CAM_CAL_ICS_REVISION
static int CAM_CAL_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
#elif 0
static int CAM_CAL_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#else
#endif
static int CAM_CAL_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int CAM_CAL_i2c_remove(struct i2c_client *);

static const struct i2c_device_id CAM_CAL_i2c_id[] = {{CAM_CAL_DRVNAME_S5K4H5,0},{}};   
#if 0 //test110314 Please use the same I2C Group ID as Sensor
static unsigned short force[] = {CAM_CAL_I2C_GROUP_ID, s5k4h5otp_DEVICE_ID, I2C_CLIENT_END, I2C_CLIENT_END};   
#else
//static unsigned short force[] = {IMG_SENSOR_I2C_GROUP_ID, s5k4h5otp_DEVICE_ID, I2C_CLIENT_END, I2C_CLIENT_END};   
#endif
//static const unsigned short * const forces[] = { force, NULL };              
//static struct i2c_client_address_data addr_data = { .forces = forces,}; 


static struct i2c_driver CAM_CAL_i2c_driver = {
    .probe = CAM_CAL_i2c_probe,                                   
    .remove = CAM_CAL_i2c_remove,                           
//   .detect = CAM_CAL_i2c_detect,                           
    .driver.name = CAM_CAL_DRVNAME_S5K4H5,
    .id_table = CAM_CAL_i2c_id,                             
};

#ifndef CAM_CAL_ICS_REVISION
static int CAM_CAL_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {         
    strcpy(info->type, CAM_CAL_DRVNAME_S5K4H5);
    return 0;
}
#endif
static int CAM_CAL_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {             
int i4RetValue = 0;
    CAM_CALDB_4H5("[CAM_CAL_4H5] Attach I2C \n");
//    spin_lock_init(&g_CAM_CALLock_4h5);

    //get sensor i2c client
    spin_lock(&g_CAM_CALLock_4h5); //for SMP
    g_pstI2Cclient = client;
    g_pstI2Cclient->addr = s5k4h5otp_DEVICE_ID>>1;
    spin_unlock(&g_CAM_CALLock_4h5); // for SMP    
    
    CAM_CALDB_4H5("[CAM_CAL_4H5] g_pstI2Cclient->addr = 0x%8x \n",g_pstI2Cclient->addr);
    //Register char driver
    i4RetValue = RegisterCAM_CALCharDrv();

    if(i4RetValue){
        CAM_CALDB_4H5("[CAM_CAL_4H5] register char device failed!\n");
        return i4RetValue;
    }


    CAM_CALDB_4H5("[CAM_CAL_4H5] Attached!! \n");
    return 0;                                                                                       
} 

static int CAM_CAL_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static int CAM_CAL_probe(struct platform_device *pdev)
{
    return i2c_add_driver(&CAM_CAL_i2c_driver);
}

static int CAM_CAL_remove(struct platform_device *pdev)
{
    i2c_del_driver(&CAM_CAL_i2c_driver);
    return 0;
}

// platform structure
static struct platform_driver g_stCAM_CAL_Driver = {
    .probe		= CAM_CAL_probe,
    .remove	= CAM_CAL_remove,
    .driver		= {
        .name	= CAM_CAL_DRVNAME_S5K4H5,
        .owner	= THIS_MODULE,
    }
};


static struct platform_device g_stCAM_CAL_Device = {
    .name = CAM_CAL_DRVNAME_S5K4H5,
    .id = 0,
    .dev = {
    }
};

static int __init CAM_CAL_i2C_init_s5k4h5(void)
{

	if (platform_device_register(&g_stCAM_CAL_Device))
	   {
		   CAM_CALDB_4H5("failed to register S24CAM_CAL driver, 2nd time\n");
		   return -ENODEV;
	   }   

    i2c_register_board_info(CAM_CAL_I2C_BUSNUM, &kd_cam_cal_dev, 1);
    if(platform_driver_register(&g_stCAM_CAL_Driver)){
        CAM_CALDB_4H5("failed to register S24CAM_CAL driver\n");
        return -ENODEV;
    }

   

    return 0;
}

static void __exit CAM_CAL_i2C_exit_s5k4h5(void)
{
	platform_driver_unregister(&g_stCAM_CAL_Driver);
}

module_init(CAM_CAL_i2C_init_s5k4h5);
module_exit(CAM_CAL_i2C_exit_s5k4h5);

MODULE_DESCRIPTION("CAM_CAL driver");
MODULE_AUTHOR("Sean Lin <Sean.Lin@Mediatek.com>");
MODULE_LICENSE("GPL");


