/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * vehicle_spi_protocol.h -- define MCU protocol
 *
 * Copyright (c) 2023-2028 Rockchip Electronics Co., Ltd.
 *
 * Author: Tom Song <tom.song@rock-chips.com>
 *
 */
#ifndef VEHICLE_SPI_PROTOCOL_H
#define VEHICLE_SPI_PROTOCOL_H
#include "core.h"

#define DATE_SOC_SET        0x91
#define DATE_SOC_GET        0x90
#define DATE_MCU            0x80
#define SIZE_DATA           32
#define DATA_LEN            4
#define SET_GPIO_START      0x01
#define SET_GPIO_END        0x20
#define GPIO_DIR_OFFSET     0x20
#define GPIO_DIR_START      (SET_GPIO_START + GPIO_DIR_OFFSET)
#define GPIO_DIR_END        (SET_GPIO_END   + GPIO_DIR_OFFSET)
#define GET_GPIO_START      0x90
#define GET_GPIO_END        0xCF
#define WRITE_RET_LEN       5
#define BOARD               0x40
#define CAN_SOC_TO_MCU      0x41
#define HEART               0x42
#define CAN_MCU_TO_SOC      0xd0
#define VERSION             0x0
#define VERSION_ID          0x21

#define ERROR     0X00
#define SUCCEED   0X01

int vehicle_analyze_write_data(struct vehicle *device, unsigned char cmd,
					 unsigned char *data, size_t len);
int vehicle_analyze_read_data(struct vehicle *device, unsigned char *rxbuf, size_t len);
int vehicle_analyze_read_reg(struct vehicle *device, unsigned int reg, unsigned int *val);
#endif
