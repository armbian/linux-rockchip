#ifndef __LCD_VK2C21_H__
#define __LCD_VK2C21_H__

#define Vk2c21_ADDR   			0x70    // IIC地址
//基本设置
#define Vk2c21_RWRAM				0x80    // 读写显示RAM
#define Vk2c21_MODESET		  0x82    // BIAS,COM设置
#define CCOM_1_3__4 			0x00  	// 1/3bias 4com
#define CCOM_1_4__4 			0x01  	// 1/4bias 4com
#define CCOM_1_3__8 			0x02  	// 1/3bias 8com
#define CCOM_1_4__8 			0x03  	// 1/4bias 8com
#define Vk2c21_SYSSET				0x84    // IRC,LCD开关设置
#define SYSOFF_LCDOFF  		0x00  	// IRC off,LCD off
#define SYSON_LCDOFF  		0x02  	// IRC on,LCD off
#define SYSON_LCDON  			0x03  	// IRC on,LCD on
#define Vk2c21_FRAMESET			0x86    // 帧频设置
#define FRAME_80HZ  			0x00  	// 帧频80HZ
#define FRAME_160HZ  			0x01  	// 帧频160HZ
#define Vk2c21_BLINKSET			0x88    // 闪烁频率设置
#define BLINK_OFF  				0x00  	// 闪烁关闭
#define BLINK_2HZ  				0x01  	// 闪烁2HZ
#define BLINK_1HZ  				0x02  	// 闪烁1HZ
#define BLINK_0D5HZ  			0x03  	// 闪烁0.5HZ
#define Vk2c21_IVASET				0x8A    // SEG/VLCD共用脚设置和内部电压调整设置
#define VLCDSEL_IVAOFF_R0 0x00  	// SEG/VLCD共用脚设为VLCD，内部电压调整功能关闭,VLCD和VDD短接VR=0偏置电压=VDD
#define VLCDSEL_IVAOFF_R1 0x0F  	// SEG/VLCD共用脚设为VLCD，内部电压调整功能关闭,VLCD和VDD串接电阻VR>0偏置电压=VLCD
	
#define VLCDSEL_IVA00H 		0x10  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=VDD 			1/4bias=VDD
#define VLCDSEL_IVA01H 		0x11  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.944VDD 1/4bias=0.957VDD
#define VLCDSEL_IVA02H 		0x12  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.894VDD 1/4bias=0.918VDD
#define VLCDSEL_IVA03H 		0x13  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.849VDD 1/4bias=0.882VDD
#define VLCDSEL_IVA04H 		0x14  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.808VDD 1/4bias=0.849VDD
#define VLCDSEL_IVA05H 		0x15  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.771VDD 1/4bias=0.818VDD
#define VLCDSEL_IVA06H 		0x16  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.738VDD 1/4bias=0.789VDD
#define VLCDSEL_IVA07H 		0x17  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.707VDD 1/4bias=0.763VDD
#define VLCDSEL_IVA08H 		0x18  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.678VDD 1/4bias=0.738VDD
#define VLCDSEL_IVA09H 		0x19  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.652VDD 1/4bias=0.714VDD
#define VLCDSEL_IVA0AH 		0x1A  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.628VDD 1/4bias=0.692VDD
#define VLCDSEL_IVA0BH 		0x1B  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.605VDD 1/4bias=0.672VDD
#define VLCDSEL_IVA0CH 		0x1C  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.584VDD 1/4bias=0.652VDD
#define VLCDSEL_IVA0DH 		0x1D  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.565VDD 1/4bias=0.634VDD
#define VLCDSEL_IVA0EH 		0x1E  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.547VDD 1/4bias=0.616VDD
#define VLCDSEL_IVA0FH 		0x1F  	// SEG/VLCD共用脚设为VLCD输出，内部偏置电压调整：1/3bias=0.529VDD 1/4bias=0.600VDD
	
#define SEGSEL_IVAOFF			0x20  	// SEG/VLCD共用脚设为SEG，内部电压调整功能关闭,VDD提供偏置电压
#define SEGSEL_IVA00H 		0x30  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=VDD 			1/4bias=VDD
#define SEGSEL_IVA01H 		0x31  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.944VDD 1/4bias=0.957VDD
#define SEGSEL_IVA02H 		0x32  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.894VDD 1/4bias=0.918VDD
#define SEGSEL_IVA03H 		0x33  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.849VDD 1/4bias=0.882VDD
#define SEGSEL_IVA04H 		0x34  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.808VDD 1/4bias=0.849VDD
#define SEGSEL_IVA05H 		0x35  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.771VDD 1/4bias=0.818VDD
#define SEGSEL_IVA06H 		0x36  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.738VDD 1/4bias=0.789VDD
#define SEGSEL_IVA07H 		0x37  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.707VDD 1/4bias=0.763VDD
#define SEGSEL_IVA08H 		0x38  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.678VDD 1/4bias=0.738VDD
#define SEGSEL_IVA09H 		0x39  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.652VDD 1/4bias=0.714VDD
#define SEGSEL_IVA0AH 		0x3A  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.628VDD 1/4bias=0.692VDD
#define SEGSEL_IVA0BH 		0x3B  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.605VDD 1/4bias=0.672VDD
#define SEGSEL_IVA0CH 		0x3C  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.584VDD 1/4bias=0.652VDD
#define SEGSEL_IVA0DH 		0x3D  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.565VDD 1/4bias=0.634VDD
#define SEGSEL_IVA0EH 		0x3E  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.547VDD 1/4bias=0.616VDD
#define SEGSEL_IVA0FH 		0x3F  	// SEG/VLCD共用脚设为SEG，内部偏置电压调整：1/3bias=0.529VDD 1/4bias=0.600VDD	

#endif
