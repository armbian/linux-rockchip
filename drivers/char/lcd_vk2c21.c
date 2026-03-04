#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/jiffies.h> 
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>

#define LCD_EXT_DEBUG_INFO
#ifdef LCD_EXT_DEBUG_INFO
#define DBG_PRINT(...)		printk(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif
#define DEVICE_NAME  "lcd_vk2c21"
/* ioctrl magic set */
#define CH455_IOC_MAGIC 'h'
#define IOCTL_CHAR_DISPLAY 	_IOWR(CH455_IOC_MAGIC, 0x10, unsigned char)
#define IOCTL_DOT_DISPLAY  	_IOWR(CH455_IOC_MAGIC, 0x11, unsigned char)
#define IOCTL_COLON_DISPLAY 	_IOWR(CH455_IOC_MAGIC, 0x12, unsigned char)
#define IOCTL_PWR_DISPLAY	_IOWR(CH455_IOC_MAGIC, 0x13, unsigned char)
#define IOCTL_LAN_DISPLAY	_IOWR(CH455_IOC_MAGIC, 0x14, unsigned char)
#define IOCTL_LAN_OFF	_IOWR(CH455_IOC_MAGIC, 0x15, unsigned char)
#define IOCTL_WIFI_LOW_DISPLAY	_IOWR(CH455_IOC_MAGIC, 0x16, unsigned char)
#define IOCTL_WIFI_FINE_DISPLAY	_IOWR(CH455_IOC_MAGIC, 0x17, unsigned char)
#define IOCTL_WIFI_OFF	_IOWR(CH455_IOC_MAGIC, 0x18, unsigned char)
#define IOCTL_LED_ON		_IOWR(CH455_IOC_MAGIC, 0x19, unsigned char)
#define	IOCTL_LED_OFF		_IOWR(CH455_IOC_MAGIC, 0x1a, unsigned char)

//以下管脚输出定义根据客户单片机做相应的修改
#define Vk2c21_SCL_H() 				gpio_set_value(gpio_i2c_scl,1) 
#define Vk2c21_SCL_L() 				gpio_set_value(gpio_i2c_scl,0) 

#define Vk2c21_SDA_H() 				gpio_set_value(gpio_i2c_sda,1) 
#define Vk2c21_SDA_L() 				gpio_set_value(gpio_i2c_sda,0) 

#define Vk2c21_GET_SDA() 			gpio_get_value(gpio_i2c_sda)
#define Vk2c21_SET_SDA_IN() 		gpio_direction_input(gpio_i2c_sda)
#define Vk2c21_SET_SDA_OUT() 		gpio_direction_output(gpio_i2c_sda, 1)

static int gpio_i2c_scl,gpio_i2c_sda;
/**
******************************************************************************
* @file    vk2c21.c
* @author  kevin_guo
* @version V1.2
* @date    05-17-2020
* @brief   This file contains all the vk2c21 functions. 
*          此文件适用于 VK2c21
******************************************************************************
* @attention
******************************************************************************
*/	
#include "lcd_vk2c21.h"
  
#define VK2c21_CLK 100 //SCL信号线频率,由delay_nus实现 50->10kHz 10->50kHz 5->100kHz
//驱动seg数
//4com 
//VK2C21A/B/C/D
#define 	Vk2c21_SEGNUM				13
#define 	Vk2c21A_MAXSEGNUM			20
#define 	Vk2c21B_MAXSEGNUM			16
#define 	Vk2c21C_MAXSEGNUM			12
#define 	Vk2c21D_MAXSEGNUM			8

//segtab[]数组对应实际的芯片到LCD连线，连线见-VK2c21参考电路
//4com 
//Vk2c21A 
unsigned char vk2c21_segtab[Vk2c21_SEGNUM]={
	18,17,16,15,14,13,12,11,10,
	9,8,7,6
};

//4com
unsigned char vk2c21_dispram[Vk2c21_SEGNUM/2];//4COM时每个字节数据对应2个SEG
//8com
//unsigned char vk2c21_dispram[Vk2c21_SEGNUM];//8COM时每个字节数据对应1个SEG

unsigned char shuzi_zimo[15]= //数字和字符字模
{
	//0    1    2    3    4    5    6    7    8    9    -    L    o    H    i 
	0xf5,0x05,0xb6,0x97,0x47,0xd3,0xf3,0x85,0xf7,0xd7,0x02,0x70,0x33,0x67,0x50
};
unsigned char vk2c21_segi,vk2c21_comi;
unsigned char vk2c21_maxcom;//驱动的com数VK2C23A可以是4com或者8com
unsigned char vk2c21_maxseg;//Vk2c21A=20 Vk2c21B=16 Vk2c21C=12 Vk2c21D=8
/* Private function prototypes -----------------------------------------------*/
unsigned char Vk2c21_InitSequence(void);
/* Private function ----------------------------------------------------------*/


/*******************************************************************************
* Function Name  : delay_nus
* Description    : 延时1uS程序
* Input          : n->延时时间nuS
* Output         : None
* Return         : None
*******************************************************************************/
static void delay_nus(unsigned int n)	   
{
	ndelay(n);
}
/*******************************************************************************
* Function Name			  : I2CStart   I2CStop  I2CSlaveAck
* I2CStart Description    : 时钟线高时，数据线由高到低的跳变，表示I2C开始信号
* I2CStop Description	  : 时钟线高时，数据线由低到高的跳变，表示I2C停止信号
* I2CSlaveAck Description : I2C从机设备应答查询
*******************************************************************************/
static void Vk2c21_I2CStart( void )
{
  Vk2c21_SCL_H();
  Vk2c21_SDA_H();
  delay_nus(VK2c21_CLK);
  Vk2c21_SDA_L();
  delay_nus(VK2c21_CLK);
}

static void Vk2c21_I2CStop( void )
{
  Vk2c21_SCL_H();
  Vk2c21_SDA_L();
  delay_nus(VK2c21_CLK);
  Vk2c21_SDA_H();
  delay_nus(VK2c21_CLK);
}

static unsigned char Vk2c21_I2CSlaveAck( void )
{
  unsigned int TimeOut;
  unsigned char RetValue;
	
  Vk2c21_SCL_L();
	//单片机SDA脚为单向IO要设为输入脚
	Vk2c21_SET_SDA_IN();
  delay_nus(VK2c21_CLK);
  Vk2c21_SCL_H();//第9个sclk上升沿

  TimeOut = 10000;
  while( TimeOut-- > 0 )
  {
    if( Vk2c21_GET_SDA()!=0 )//读取ack
    {
      RetValue = 1;
    }
    else
    {
      RetValue = 0;
      break;
    }
  } 
	Vk2c21_SCL_L();
	//单片机SDA脚为单向IO要设为输出脚
	Vk2c21_SET_SDA_OUT();
  //printk("---%s----ret=%d------\n",__func__,RetValue);
  return RetValue;
}
/*******************************************************************************
* Function Name  : I2CWriteByte
* Description    : I2C写一字节命令,命令先送高位
* Input          : byte-要写入的数据
* Output         : None
* Return         : None
*******************************************************************************/
static void Vk2c21_I2CWriteByte( unsigned char byte )
{
	unsigned char i=8;
	while (i--)
	{ 
		Vk2c21_SCL_L();
		if(byte&0x80)
			Vk2c21_SDA_H();
		else
			Vk2c21_SDA_L();
		byte<<=1; 
		delay_nus(VK2c21_CLK);
		Vk2c21_SCL_H();     
		delay_nus(VK2c21_CLK);
	}
}

/*************************************************************
*函数名称: WriteCmdVk2c21
*函数功能: 向Vk2C23写1个命令
*输入参数: addr  Dmod地址
           data  写入的数据
*输出参数：SET: 写入正常；RESET:写入错误
*备    注：
**************************************************************/
static unsigned char WriteCmdVk2c21(unsigned char cmd, unsigned char data )
{
	Vk2c21_I2CStart();

	Vk2c21_I2CWriteByte( Vk2c21_ADDR|0x00 );
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop(); 
		return 0;   
	}
	Vk2c21_I2CWriteByte( cmd );
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop(); 
		return 0;   
	}
	Vk2c21_I2CWriteByte( data );
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop(); 
		return 0;   
	}
  Vk2c21_I2CStop();
 
  return 0;    //返回操作成败标志
}
/*******************************************************************************
* Function Name  : Write1Data
* Description    : 写1字节数据到显示RAM
* Input          : Addr-写入ram的地址
*                : Dat->写入ram的数据
* Output         : None
* Return         : 0-ok 1-fail
*******************************************************************************/
static unsigned char Write1DataVk2c21(unsigned char Addr,unsigned char Dat)
{
	//START 信号
	Vk2c21_I2CStart(); 
	//SLAVE地址
	Vk2c21_I2CWriteByte(Vk2c21_ADDR); 
	if( 1 == Vk2c21_I2CSlaveAck() )
	{		
		Vk2c21_I2CStop();
		return 1; 
	}
	//写显示RAM命令
	Vk2c21_I2CWriteByte(Vk2c21_RWRAM); 						
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop();													
		return 0;
	}
	//显示RAM地址
	Vk2c21_I2CWriteByte(Addr); 
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop();
		return 1;
	}
	//显示数据，1字节数据包含2个SEG
	Vk2c21_I2CWriteByte(Dat);
	if( Vk2c21_I2CSlaveAck()==1 )
	{
		Vk2c21_I2CStop();
		return 1;
	}
	//STOP信号
 Vk2c21_I2CStop();
 return 0;   
}
/*******************************************************************************
* Function Name  : WritenData
* Description    : 写多个数据到显示RAM
* Input          : Addr-写入ram的起始地址
*                : Databuf->写入ram的数据buffer指针
*                : Cnt->写入ram的数据个数
* Output         : None
* Return         : 0-ok 1-fail
*******************************************************************************/
static unsigned char  WritenDataVk2c21(unsigned char Addr,unsigned char *Databuf,unsigned char Cnt)
{
	unsigned char n;
	
	//START信号	
	Vk2c21_I2CStart(); 									
	//SLAVE地址
	Vk2c21_I2CWriteByte(Vk2c21_ADDR); 	
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop();													
		return 0;										
	}
	//写显示RAM命令
	Vk2c21_I2CWriteByte(Vk2c21_RWRAM); 						
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop();													
		return 0;
	}
	//显示RAM起始地址
	Vk2c21_I2CWriteByte(Addr); 						
	if( 1 == Vk2c21_I2CSlaveAck() )
	{
		Vk2c21_I2CStop();													
		return 0;
	}
	//发送Cnt个数据到显示RAM
	for(n=0;n<Cnt;n++)
	{ 
		Vk2c21_I2CWriteByte(*Databuf++);
		if( Vk2c21_I2CSlaveAck()==1 )
		{
			Vk2c21_I2CStop();													
			return 0;
		}
	}
	//STOP信号
	 Vk2c21_I2CStop();											
	 return 0;    
}
/*******************************************************************************
* Function Name  : Vk2c21_DisAll
* Description    : 所有SEG显示同一个数据，bit7/bit3-COM3 bit6/bit2-COM2 bit5/bit1-COM1 bit4/bit0-COM0
* 					     : 例如：0xff全亮 0x00全灭 0x55灭亮灭亮 0xaa亮灭亮灭 0x33灭灭亮亮 
* Input          ：dat->写入ram的数据(1个字节数据对应2个SEG)  
* Output         : None
* Return         : None
*******************************************************************************/
static void Vk2c21_DisAll(unsigned char dat)
{
	unsigned char segi;
	unsigned char dispram[16];
	
	if(vk2c21_maxcom==4)
	{
		for(segi=0;segi<10;segi++)
		{				
			dispram[segi]=dat;
		}
		WritenDataVk2c21(0,dispram,10);//这里送8bit数据对应2个SEG，每8bit数据地址加1，每8位数据1个ACK
	}
	else
	{
		for(segi=0;segi<16;segi++)
		{
			dispram[segi]=dat;
		}
		WritenDataVk2c21(0,dispram,16);//这里送8bit数据对应1个SEG，每8bit数据地址加1，每8位数据1个ACK
	}
}
/*******************************************************************************
* Function Name  : DisSegComOn
* Description    : 点亮1个点(1个seg和1个com交叉对应的显示点)
* Input          ：seg->点对应的seg脚  
* 		           ：com->点对应com脚  
* Output         : None
* Return         : None
*******************************************************************************/
static void Vk2c21_DisSegComOn(unsigned char seg,unsigned char com)
{
	if(vk2c21_maxcom==4)
	{
		if(seg%2==0)
			Write1DataVk2c21(seg/2,(1<<(com)));//这里送8位数据低4bit有效，每8bit数据地址加1，每8位数据1个ACK)
		else
			Write1DataVk2c21(seg/2,(1<<(4+com)));//这里送8位数据高4bit有效，每8bit数据地址加1，每8位数据1个ACK
	}
	else
	{
		Write1DataVk2c21(seg,(1<<(com)));//这里送8位数据低4bit有效，每8bit数据地址加1，每8位数据1个ACK
	}
}
/*******************************************************************************
* Function Name  : DisSegComOff
* Description    : 关闭1个点(1个seg和1个com交叉对应的显示点)
* Input          ：seg->点对应的seg脚  
* 		           ：com->点对应com脚  
* Output         : None
* Return         : None
*******************************************************************************/
static void Vk2c21_DisSegComOff(unsigned char seg,unsigned char com)
{
	if(vk2c21_maxcom==4)
	{
		if(seg%2==0)
			Write1DataVk2c21(seg/2,~(1<<com));//这里送8位数据低4bit有效，每8bit数据地址加1，每8位数据1个ACK
		else
			Write1DataVk2c21(seg/2,~(1<<(4+com)));//这里送8位数据高4bit有效，每8bit数据地址加1，每8位数据1个ACK
	}
	else
	{
		Write1DataVk2c21(seg,~(1<<com));//这里送8位数据低4bit有效，每8bit数据地址加1，每8位数据1个ACK
	}
}

static void disp_3num(unsigned int dat)
{		
	unsigned dat8;
	
	if(vk2c21_maxcom==4)
	{
		dat8=dat/100%10;	//显示百位
		vk2c21_dispram[0]&=0xf0;
		vk2c21_dispram[0]|=shuzi_zimo[dat8]&0x0f;
		vk2c21_dispram[0]&=0x8f;
		vk2c21_dispram[0]|=shuzi_zimo[dat8]&0xf0;
		
		dat8=dat/10%10; 	//显示十位
		vk2c21_dispram[1]&=0xf0;
		vk2c21_dispram[1]|=shuzi_zimo[dat8]&0x0f;
		vk2c21_dispram[1]&=0x8f;
		vk2c21_dispram[1]|=shuzi_zimo[dat8]&0xf0;
		
		dat8=dat%10;			//显示个位
		vk2c21_dispram[2]&=0xf0;
		vk2c21_dispram[2]|=shuzi_zimo[dat8]&0x0f;
		vk2c21_dispram[2]&=0x8f;
		vk2c21_dispram[2]|=shuzi_zimo[dat8]&0xf0;
			
		if(dat<100)				//数字小于100，百位不显示
		{
			vk2c21_dispram[0]&=0xf0;
			vk2c21_dispram[0]&=0x8f;
		}
		if(dat<10) 	//数字小于10，十位不显示
		{
			vk2c21_dispram[1]&=0xf0;
			vk2c21_dispram[1]&=0x8f;
		}
		//SEG不连续1个1个数据送
		Write1DataVk2c21(vk2c21_segtab[2]/2,vk2c21_dispram[0]);
		Write1DataVk2c21(vk2c21_segtab[4]/2,vk2c21_dispram[1]);
		Write1DataVk2c21(vk2c21_segtab[6]/2,vk2c21_dispram[2]);
		Write1DataVk2c21(vk2c21_segtab[8]/2,vk2c21_dispram[0]);
		Write1DataVk2c21(vk2c21_segtab[10]/2,vk2c21_dispram[1]);
		Write1DataVk2c21(vk2c21_segtab[12]/2,vk2c21_dispram[2]);
		//SEG连续送多个数据
	}
	else
	{
		dat8=dat/100%10;	//显示百位
		vk2c21_dispram[8]&=0x80;
		vk2c21_dispram[8]|=shuzi_zimo[dat8];
		
		dat8=dat/10%10; 	//显示十位
		vk2c21_dispram[9]&=0x80;
		vk2c21_dispram[9]|=shuzi_zimo[dat8];
		
		dat8=dat%10;			//显示个位
		vk2c21_dispram[15]&=0x80;
		vk2c21_dispram[15]|=shuzi_zimo[dat8];
			
		if(dat<100)				//数字小于100，百位不显示
		{
			vk2c21_dispram[8]&=0x80;
		}
		if(dat<10) 	//数字小于10，十位不显示
		{
			vk2c21_dispram[9]&=0x80;
		}
		//SEG不连续1个1个数据送
		Write1DataVk2c21(8,vk2c21_dispram[8]);
		Write1DataVk2c21(9,vk2c21_dispram[9]);
		Write1DataVk2c21(15,vk2c21_dispram[15]);
		//SEG连续送多个数据
	}
}	

/*******************************************************************************
* Function Name  : Init
* Description    : 初始化配置
* Input          ：None 
* Output         : None
* Return         : None
*******************************************************************************/
static int Vk2c21_Init(void)
{	
	int ret=0;
	WriteCmdVk2c21(Vk2c21_MODESET,CCOM_1_3__4); 	//模式设置  1/3 Bais 1/4 Duty
	//WriteCmdVk2c21(Vk2c21_MODESET,CCOM_1_4__4); //模式设置  1/4 Bais 1/4 Duty
	vk2c21_maxcom=4;
	vk2c21_maxseg=Vk2c21A_MAXSEGNUM;//Vk2c21A=20 Vk2c21B=16 Vk2c21C=12 Vk2c21D=8
//	WriteCmdVk2c21(Vk2c21_MODESET,CCOM_1_3__8); //模式设置  1/3 Bais 1/8 Duty
//	WriteCmdVk2c21(Vk2c21_MODESET,CCOM_1_4__8); //模式设置  1/4 Bais 1/8 Duty
//	vk2c21_maxcom=8;
//	vk2c21_maxseg=Vk2c21A_MAXSEGNUM;//Vk2c21A=16 Vk2c21B=12 Vk2c21C=8 Vk2c21D=4
	WriteCmdVk2c21(Vk2c21_SYSSET,SYSON_LCDON); 		//内部系统振荡器开，lcd开显示
	WriteCmdVk2c21(Vk2c21_FRAMESET,FRAME_80HZ); 	//帧频率80Hz
//	WriteCmdVk2c21(Vk2c21_FRAMESET,FRAME_160HZ);//帧频率160Hz
	WriteCmdVk2c21(Vk2c21_BLINKSET,BLINK_OFF); 		//闪烁关闭	
//	WriteCmdVk2c21(Vk2c21_BLINKSET,BLINK_2HZ); 		//闪烁2HZ
//	WriteCmdVk2c21(Vk2c21_BLINKSET,BLINK_1HZ); 		//闪烁1HZ
//	WriteCmdVk2c21(Vk2c21_BLINKSET,BLINK_0D5HZ); 	//闪烁0.5HZ
	//SEG/VLCD共用脚设为VLCD，内部电压调整功能关闭,VLCD和VDD短接VR=0偏置电压=VDD
	//WriteCmdVk2c21(Vk2c21_IVASET,VLCDSEL_IVAOFF_R0); 
	//SEG/VLCD共用脚设为VLCD，内部电压调整功能关闭,VLCD和VDD串接电阻VR>0偏置电压=VLCD
	WriteCmdVk2c21(Vk2c21_IVASET,VLCDSEL_IVAOFF_R1); 
	//SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.652VDD 1/4bias=0.714VDD
	//WriteCmdVk2c21(Vk2c21_IVASET,SEGSEL_IVA02H);	

	//test
	Vk2c21_DisAll(0x00);
	disp_3num(456);
	//Vk2c21_DisAll(0xff);			//LCD全显
	//disp_3num(1234);
	return ret;
}

static int Vk2c21_open(struct inode *inode, struct file *file) 
{                                                                                                                                   
	int ret;                                                                                                                  
	ret = nonseekable_open(inode, file);                                                                                       
	if(ret < 0)                                                                                                          
		return ret;            
	return 0;                                                                                                     
}

static int Vk2c21_release(struct inode *inode, struct file *file) 
{
	return 0;
}

static long Vk2c21_ioctl(struct file *file, unsigned int cmd, unsigned long args) 
{
	int ret = 0 , i=0,pos_temp=0;
	void __user *argp = (void __user *)args;
	unsigned char display_arg[6]={0};
	switch (cmd){
		case IOCTL_CHAR_DISPLAY:
			if (args) {  
				ret = copy_from_user(display_arg, argp,sizeof(display_arg)/sizeof(display_arg[0]));
			}
			for(i=0;i<6;i++)
			{
				if(display_arg[i] <= '9' && display_arg[i] >= '0')
				{
					pos_temp = display_arg[i] - '0';  
					vk2c21_dispram[i]&=0xf0;
					vk2c21_dispram[i]|=shuzi_zimo[pos_temp]&0x0f;
					vk2c21_dispram[i]&=0x8f;
					vk2c21_dispram[i]|=shuzi_zimo[pos_temp]&0xf0;
					Write1DataVk2c21(vk2c21_segtab[2*i+2]/2,vk2c21_dispram[i]);
				}else if(display_arg[i] <= 'z' && display_arg[i] >= 'a'){	
				}else if(display_arg[i] <= 'Z' && display_arg[i] >= 'A'){
				}
			}
			break;
		case IOCTL_DOT_DISPLAY:
			break;
		case IOCTL_COLON_DISPLAY:
			break;
		case IOCTL_PWR_DISPLAY:
			break;
		case IOCTL_LAN_DISPLAY:
			Vk2c21_DisSegComOn(vk2c21_segtab[6],0x1);
			break;
		case IOCTL_LAN_OFF:
			Vk2c21_DisSegComOff(vk2c21_segtab[6],0x1);
			break;
		case IOCTL_WIFI_LOW_DISPLAY:
			break;
		case IOCTL_WIFI_FINE_DISPLAY:
			break;
		case IOCTL_WIFI_OFF:
			break;
		case IOCTL_LED_ON:
			Vk2c21_DisAll(0xff);			//LCD全显
			break;
		case IOCTL_LED_OFF:
			Vk2c21_DisAll(0x00);			//LCD全关
			break;
		default:
			printk("ERROR: IOCTL CMD NOT FOUND!!!\n");
			break;
	}
	return 0;
}
static struct file_operations Vk2c21_fops ={
	.owner =THIS_MODULE,
	.open  =Vk2c21_open,
	.release =Vk2c21_release,
	.unlocked_ioctl =Vk2c21_ioctl,
};



static int Vk2c21_probe(struct platform_device *pdev)
{
	static struct class * scull_class;
	struct device_node *node = pdev->dev.of_node;
	enum of_gpio_flags flags;
	int ret; 

	ret =register_chrdev(0,DEVICE_NAME,&Vk2c21_fops);
	if(ret<0){
		printk("can't register device lcd_vk2c21.\n");
		return ret;
	}
	printk("register device lcd_vk2c21 success.\n");

	scull_class = class_create(THIS_MODULE,DEVICE_NAME);
	if(IS_ERR(scull_class))
	{
		printk(KERN_ALERT "Err:faile in scull_class!\n");
		return -1;
	}
	device_create(scull_class, NULL, MKDEV(ret,0), NULL, DEVICE_NAME);


	//---------------------------
	gpio_i2c_scl = of_get_named_gpio_flags(node, "i2c_scl", 0, &flags);
	if (gpio_is_valid(gpio_i2c_scl)){
		if (gpio_request(gpio_i2c_scl, "i2c_scl_gpio")<0) {
			printk("%s: failed to get gpio_i2c_scl.\n", __func__);
			return -1;
		}
		gpio_direction_output(gpio_i2c_scl, 1);
		printk("%s: get property: gpio,i2c_scl = %d\n", __func__, gpio_i2c_scl);
	}else{
		printk("get property gpio,i2c vk2c21 failed \n");
		return -1;
	}

	gpio_i2c_sda = of_get_named_gpio_flags(node, "i2c_sda", 0, &flags);
	
	if (gpio_is_valid(gpio_i2c_sda)){
		if (gpio_request(gpio_i2c_sda, "i2c_sda_gpio")<0) {
			printk("%s: failed to get gpio_i2c_sda.\n", __func__);
			return -1;
		}
		gpio_direction_output(gpio_i2c_sda, 1);
		printk("%s: get property: gpio,i2c_sda = %d\n", __func__, gpio_i2c_sda);
	}else{
		printk("get property gpio,i2c vk2c21 failed \n");
		return -1;
	} 

	printk("==========%s probe ok========\n", DEVICE_NAME);

	ret = Vk2c21_Init();	
	if(ret < 0)
		return -1;


	return 0;
}

static int Vk2c21_remove(struct platform_device *pdev)
{
	unregister_chrdev(0,DEVICE_NAME);
	return 0;
}


static void Vk2c21_shutdown (struct platform_device *pdev)
{
		WriteCmdVk2c21(Vk2c21_SYSSET,SYSOFF_LCDOFF);
}

#ifdef CONFIG_OF
static const struct of_device_id Vk2c21_dt_match[]={
	{ .compatible = "lcd_vk2c21",},
	{}
};
MODULE_DEVICE_TABLE(of, Vk2c21_dt_match);
#endif

static struct platform_driver Vk2c21_driver = {
	.probe  = Vk2c21_probe,
	.remove = Vk2c21_remove,
	.shutdown     = Vk2c21_shutdown,
	.driver = {
		.name  = DEVICE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(Vk2c21_dt_match),
#endif
	},
};


static int __init led_vk2c21_init(void)
{
	int ret;
	DBG_PRINT("%s\n=============================================\n", __FUNCTION__);
	ret = platform_driver_register(&Vk2c21_driver);
	if (ret) {
		printk("[error] %s failed to register vk2c21 driver module\n", __FUNCTION__);
		return -ENODEV;
	}
	return ret;
}

static void __exit led_vk2c21_exit(void)
{
	platform_driver_unregister(&Vk2c21_driver);
}

module_init(led_vk2c21_init);
module_exit(led_vk2c21_exit);

MODULE_AUTHOR("Hugsun");
MODULE_DESCRIPTION("LCD Extern driver for lcd_vk2c21");
MODULE_LICENSE("GPL");
