// SPDX-License-Identifier: GPL-2.0-only
/*
 * vehicle_spi_protocol.c -- define MCU protocol
 *
 * Copyright (c) 2023-2028 Rockchip Electronics Co., Ltd.
 *
 * Author: Tom Song <tom.song@rock-chips.com>
 *
 */
#include "vehicle_spi_protocol.h"

static unsigned char GetChkSum_CRC8(unsigned char *dptr, unsigned short len)
{
	unsigned char crc = 0;
	unsigned short i;

	for (i = 0; i < len; i++)
		crc ^= dptr[i];
	return crc;
}

static int IS_GPIO_CMD(unsigned char cmd)
{
	int ret = ERROR;

	if (cmd <= SET_GPIO_END && cmd >= SET_GPIO_START)
		ret = SUCCEED;
	else if (cmd <= GET_GPIO_END && cmd >= GET_GPIO_START)
		ret = SUCCEED;
	else if (cmd <= GPIO_DIR_END && cmd >= GPIO_DIR_START)
		ret = SUCCEED;
	return ret;
}

static int IS_REMSG_CORRECT(unsigned char *rxbuf)
{
	int len, ret = ERROR;

	len = rxbuf[1] - 1;

	if (rxbuf[0] == DATE_MCU) {
		if (rxbuf[len] == GetChkSum_CRC8(rxbuf, len))
			ret = SUCCEED;
	}

	return ret;
}

static int HandleGPIO(unsigned char *txbuf, unsigned char *rxbuf)
{
	int ret = -1;

	if (rxbuf[2] <= GET_GPIO_END && rxbuf[2] >= GET_GPIO_START) {
		if (rxbuf[3] == 1)
			ret = -1;
		else if (rxbuf[3] == 0)
			ret = 0;
	}

	if (rxbuf[2] <= SET_GPIO_END && rxbuf[2] >= SET_GPIO_START) {
		if (rxbuf[3] == txbuf[3])
			ret = 0;
		else
			ret = -1;
	}

	if (rxbuf[2] <= GPIO_DIR_END && rxbuf[2] >= GPIO_DIR_START)
		ret = 0;

	return ret;
}

static int HandleCanMSG(struct vehicle *device, unsigned char *rxbuf)
{
	int ret = 0;
	u16 value =  rxbuf[5];
	u16 state =  rxbuf[4];

	switch (rxbuf[3]) {
	case VEHICLE_AC:
		device->vehicle_data.ac_on = value;
		break;
	case VEHICLE_AUTO_ON:
		device->vehicle_data.auto_on = value;
		break;
	case VEHICLE_FAN_SPEED:
		device->vehicle_data.fan_speed = value;
		break;
	case VEHICLE_FAN_DIRECTION:
		device->vehicle_data.fan_direction = value;
		break;
	case VEHICLE_RECIRC_ON:
		device->vehicle_data.recirc_on = value;
		break;
	case VEHICLE_GEAR:
		device->vehicle_data.gear = value;
		break;
	case VEHICLE_TURN_SIGNAL:
		device->vehicle_data.turn = value;
		break;
	case VEHICLE_POWER_STATE_REQ:
		vehicle_set_property(VEHICLE_POWER_STATE_REQ, 0, state, value);
		ret = 1;
		break;
	default:
		ret = -1;
		break;
	}
	if (!ret)
		vehicle_set_property(rxbuf[3], 0, value, 0);
	return SUCCEED;

}

static int HandleOtherMsg(struct vehicle *device, unsigned char *rxbuf)
{
	int ret = -1;

	switch (rxbuf[2]) {
	case BOARD:
	case CAN_SOC_TO_MCU:
	case HEART:
		ret = rxbuf[3];
		break;

	case CAN_MCU_TO_SOC:
		ret = HandleCanMSG(device, rxbuf);
		break;

	default:
		break;
	}

	return ret;
}

int vehicle_analyze_read_data(struct vehicle *device, unsigned char *rxbuf, size_t len)
{
	int ret = -1;

	vehicle_spi_read_slt(device, rxbuf, len);
	if (IS_REMSG_CORRECT(rxbuf)) {
		if (IS_GPIO_CMD(rxbuf[2]))
			dev_info(device->vehicle_spi->dev, "read gpio");
		else if (HandleOtherMsg(device, rxbuf) == SUCCEED)
			ret = 0;
	}

	return ret;
}

int vehicle_analyze_read_reg(struct vehicle *device, unsigned int reg, unsigned int *val)
{
	unsigned char *txbuf = NULL, *rxbuf = NULL;
	int ret = -1;
	struct device *dev = device->dev;

	*val = 0;
	txbuf = kzalloc(SIZE_DATA, GFP_KERNEL);
	if (!txbuf) {
		dev_err(dev, "spi write alloc buf size %d fail\n", SIZE_DATA);
		goto err_ret;
	}
	txbuf[0] = DATE_SOC_GET;
	txbuf[1] = WRITE_RET_LEN;
	txbuf[2] = reg;
	txbuf[3] = 0;
	txbuf[4] = GetChkSum_CRC8(txbuf, WRITE_RET_LEN-1);
	rxbuf = kzalloc(SIZE_DATA, GFP_KERNEL);
	if (!rxbuf)
		goto err_free_txbuf;

	disable_irq(device->vehicle_spi->irq);
	ret = vehicle_spi_write_slt(device, txbuf, WRITE_RET_LEN);
	if (ret < 0)
		goto err_irq;
	ret = vehicle_spi_read_slt(device, rxbuf, WRITE_RET_LEN);
	if (ret < 0)
		goto err_irq;

	*val = rxbuf[3];

err_irq:
	enable_irq(device->vehicle_spi->irq);
	kfree(rxbuf);
err_free_txbuf:
	kfree(txbuf);
err_ret:
	return ret;
}

int vehicle_analyze_write_data(struct vehicle *device, unsigned char cmd,
				 unsigned char *data, size_t len)
{
	int i, ret = -1;
	unsigned char *txbuf = NULL, *rxbuf = NULL;

	if (len + DATA_LEN > SIZE_DATA)
		goto err_ret;

	txbuf = kzalloc(SIZE_DATA, GFP_KERNEL);
	if (!txbuf)
		goto err_ret;

	if (cmd < GET_GPIO_START)
		txbuf[0] = DATE_SOC_SET;
	else
		txbuf[0] = DATE_SOC_GET;

	txbuf[2] = cmd;
	for (i = 0; i < len; i++)
		txbuf[3 + i] = data[i];

	len = len + DATA_LEN;
	txbuf[1] = len;
	txbuf[len - 1] = GetChkSum_CRC8(txbuf, len - 1);

	rxbuf = kzalloc(SIZE_DATA, GFP_KERNEL);
	if (!rxbuf)
		goto err_free_txbuf;

	disable_irq(device->vehicle_spi->irq);
	ret = vehicle_spi_write_slt(device, txbuf, len);
	if (ret < 0)
		goto err_irq;
	ret = vehicle_spi_read_slt(device, rxbuf, WRITE_RET_LEN);
	if (ret < 0)
		goto err_irq;
#if defined(VEHICLE_DEBUG)
	print_hex_dump(KERN_ERR, "SPI RX: ",
		DUMP_PREFIX_OFFSET,
		16,
		1,
		rxbuf,
		WRITE_RET_LEN,
		1);
#endif

	if (IS_REMSG_CORRECT(rxbuf)) {
		if (IS_GPIO_CMD(rxbuf[2]))
			ret = HandleGPIO(txbuf, rxbuf);
		else if (rxbuf[3] == SUCCEED)
			ret = 0;
	}
err_irq:
	enable_irq(device->vehicle_spi->irq);
	kfree(rxbuf);
err_free_txbuf:
	kfree(txbuf);
err_ret:
	return ret;
}

