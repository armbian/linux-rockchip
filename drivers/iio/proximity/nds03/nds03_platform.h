/*!
 @file nds03_platform.h
 @brief
 @author lull
 @date 2025-06
 @copyright Copyright (c) 2025  Shenzhen Nephotonics  Semiconductor Technology Co., Ltd.
 @license BSD 3-Clause License
          This file is part of the Nephotonics sensor SDK.
          It is licensed under the BSD 3-Clause License.
          A copy of the license can be found in the project root directory, in the file named LICENSE.
 */
#ifndef __NDS03_PLATFORM__H__
#define __NDS03_PLATFORM__H__

#include "nds03_stdint.h"
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
/**
  * @struct NDS03_Platform_t
  *
  * @brief NDS03平台相关定义 \n
  * 定义i2c地址等，注意必须定义i2c地址
  */
typedef struct{
	//文件描述符
	int fd;
	/** 用户不可更改以下变量  @{ */
	uint8_t     i2c_dev_addr;   // i2c设备地址
	/** @} */
	/** 用户可根据需要添加变量 */
	struct i2c_client *client;
	////// /* nds03 device io */
	struct gpio_desc *intr_gpio;
	/*!< xsdn reset (low active) gpio number to device */
	struct gpio_desc *xshut_gpio;

} NDS03_Platform_t;

/**
 * @brief NDS03平台初始化
 * 
 * @param   pDev        平台设备指针
 * @param   void*       拓展指针
 * @return  int8_t
 * @retval  0:成功, 其他:失败
 */
int8_t nds03_platform_init(NDS03_Platform_t *pdev, void *);

/**
 * @brief NDS03平台释放
 *
 * @param   pDev        平台设备指针
 * @param   void*       拓展指针
 * @return  int8_t
 * @retval  0:成功, 其他:失败
 */
int8_t nds03_platform_uninit(NDS03_Platform_t *pdev, void *);

/**
 * @brief I2C读一个字节
 *
 * @param   pDev        平台设备指针
 * @param   i2c_raddr   读寄存器地址
 * @param   i2c_rdata   读数据
 * @return  int8_t
 * @retval  0:成功, 其他:失败
 */
int8_t nds03_i2c_read_nbytes(NDS03_Platform_t *pDev, uint8_t i2c_raddr, uint8_t *i2c_rdata, uint16_t len);

/**
 * @brief I2C写一个字节
 *
 * @param   pDev        平台设备指针
 * @param   i2c_waddr   写寄存器地址
 * @param   i2c_wdata   写数据
 * @return  int8_t
 * @retval  0:成功, 其他:失败
 */
int8_t nds03_i2c_write_nbytes(NDS03_Platform_t *pDev, uint8_t i2c_waddr, uint8_t *i2c_wdata, uint16_t len);

/**
 * @brief 延时wait_ms毫秒
 *
 * @param   pDev        平台设备指针
 * @param   wait_ms     输入需要延时时长
 * @return  int8_t
 * @retval  0:成功, 其他:失败
 */
int8_t nds03_delay_1ms(NDS03_Platform_t *pDev, uint32_t wait_ms);

/**
 * @brief 延时10*wait_10us微秒
 *
 * @param   pDev        平台设备指针
 * @param   wait_10us   输入需要延时时长
 * @return  int8_t
 * @retval  0:成功, 其他:失败
 */
int8_t nds03_delay_10us(NDS03_Platform_t *pDev, uint32_t wait_10us);

/**
 * @brief 设置nds03 xshut引脚电平
 *
 * @param   pDev        平台设备指针
 * @param   level       引脚电平，0为低电平，1为高电平
 * @return  int8_t
 * @retval  0:成功, 其他:失败
 */
int8_t nds03_set_xshut_pin_level(NDS03_Platform_t *pDev, int8_t level);

/*!
 @brief NDS03获取当前i2c时钟频率
 @param   pDev 平台设备指针
 @param   clock_frequency 时钟频率指针
 @return int8_t
 */
int8_t nds03_i2c_get_clock_frequency(NDS03_Platform_t *pDev, uint32_t *clock_frequency);

/*!
 @brief NDS03设置当前i2c时钟频率
 @param   pDev 平台设备指针
 @param   clock_frequency 时钟频率指针
 @return int8_t
 */
int8_t nds03_i2c_set_clock_frequency(NDS03_Platform_t *pDev, uint32_t clock_frequency);
/*!
 @brief 获取当前系统时间，单位ms
 @param   pDev
 @param   time_ms
 @return int8_t
 */
int8_t nds03_get_system_clk_ms(NDS03_Platform_t *pDev, int32_t *time_ms);

#endif

