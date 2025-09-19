/*!
 @file nds03_platform.c
 @brief
 @author lull
 @date 2025-06
 @copyright Copyright (c) 2025  Shenzhen Nephotonics  Semiconductor Technology Co., Ltd.
 @license BSD 3-Clause License
          This file is part of the Nephotonics sensor SDK.
          It is licensed under the BSD 3-Clause License.
          A copy of the license can be found in the project root directory, in the file named LICENSE.
 */
#include "nds03_platform.h"

int8_t nds03_platform_init(NDS03_Platform_t *pdev, void * client)
{
	pdev->client = (struct i2c_client *)client;
	pdev->i2c_dev_addr = 0X5C;

	return 0;
}

int8_t nds03_platform_uninit(NDS03_Platform_t *pdev, void * param)
{
	return 0;
}

int8_t nds03_i2c_read_nbytes(NDS03_Platform_t *pDev, uint8_t i2c_raddr, uint8_t *i2c_rdata, uint16_t len)
{
	int8_t ret;
	struct i2c_msg i2c_message[2];
	uint8_t i2c_buf[256];

	i2c_buf[0] = i2c_raddr;
	i2c_message[0].addr =  pDev->i2c_dev_addr;
	i2c_message[0].buf = i2c_buf;
	i2c_message[0].len = 1;
	i2c_message[0].flags = 0 ; //| I2C_M_STOP;//I2C_M_STOP;

	i2c_message[1].addr =  pDev->i2c_dev_addr;
	i2c_message[1].buf = i2c_rdata;
	i2c_message[1].len = len;
	i2c_message[1].flags = I2C_M_RD;
	ret = i2c_transfer(pDev->client->adapter, i2c_message, 2);
	if (ret != 2) {
		pr_err("NDS03: i2c read error, i2c_raddr: 0x%x, reg: 0x%x, ret: %d", pDev->i2c_dev_addr, i2c_raddr, ret);
		return -EIO;
	}
	return 0;

}

int8_t nds03_i2c_write_nbytes(NDS03_Platform_t *pDev, uint8_t i2c_waddr, uint8_t *i2c_wdata, uint16_t len)
{
	int32_t ret;
	struct i2c_msg i2c_message;
	//For user implement
	uint8_t i2c_buf[256];

	memcpy(&i2c_buf[1], i2c_wdata, len);
	i2c_buf[0] = i2c_waddr;

	i2c_message.addr = pDev->i2c_dev_addr;
	i2c_message.buf = i2c_buf;
	i2c_message.len = len + 1;
	i2c_message.flags = 0;
	ret = i2c_transfer(pDev->client->adapter, &i2c_message, 1);
	if (ret != 1) {
		pr_err("NDS03: i2c write error, i2c_waddr: 0x%x, reg: 0x%x, ret: %d", pDev->i2c_dev_addr, i2c_waddr, ret);
		return -EIO;
	}
	return 0;

}

int8_t nds03_delay_10us(NDS03_Platform_t *pDev, uint32_t wait_10us)
{
	wait_10us *= 10;
	if (wait_10us < 10)
		udelay(wait_10us);
	else if (wait_10us < 20000)
		usleep_range(wait_10us, wait_10us + 1);
	else
		msleep(wait_10us / 1000);
	return 0;
}

int8_t nds03_delay_1ms(NDS03_Platform_t *pDev, uint32_t wait_ms)
{
	nds03_delay_10us(pDev, wait_ms * 100);
	return 0;
}

int8_t nds03_set_xshut_pin_level(NDS03_Platform_t *pDev, int8_t level)
{
	if (pDev->xshut_gpio == NULL) {
		pr_err("xshut_gpiod is not init , not setting xshut\n");
		return -1;
	}
	gpiod_set_value(pDev->xshut_gpio, level);
	// pr_info("xshut set to %d\n", level);
	return 0;
}

int8_t nds03_i2c_get_clock_frequency(NDS03_Platform_t *pDev, uint32_t *clock_frequency)
{
	return 0;
}

int8_t nds03_i2c_set_clock_frequency(NDS03_Platform_t *pDev, uint32_t clock_frequency)
{
	return 0;
}

int8_t nds03_get_system_clk_ms(NDS03_Platform_t *pDev, int32_t *time_ms)
{
	return 0;
}
