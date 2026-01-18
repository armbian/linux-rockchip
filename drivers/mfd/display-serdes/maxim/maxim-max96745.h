/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * maxim-max96745.h -- register define for max96745 chip
 *
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 * Author: luowei <lw@rock-chips.com>
 *
 */

#ifndef __MFD_SERDES_MAXIM_MAX96745_H__
#define __MFD_SERDES_MAXIM_MAX96745_H__

#include <linux/bitfield.h>

#define GPIO_A_REG(gpio)	(0x0200 + ((gpio) * 8))
#define GPIO_B_REG(gpio)	(0x0201 + ((gpio) * 8))
#define GPIO_C_REG(gpio)	(0x0202 + ((gpio) * 8))
#define GPIO_D_REG(gpio)	(0x0203 + ((gpio) * 8))

/* 0005h */
#define PU_LF3			BIT(3)
#define PU_LF2			BIT(2)
#define PU_LF1			BIT(1)
#define PU_LF0			BIT(0)

/* 0010h */
#define RESET_ALL		BIT(7)
#define SLEEP			BIT(3)

/* 0011h */
#define CXTP_B			BIT(2)
#define CXTP_A			BIT(0)

/* 0013h */
#define LOCKED			BIT(3)
#define ERROR			BIT(2)

/* 0026h */
#define LF_0			GENMASK(2, 0)
#define LF_1			GENMASK(6, 4)

/* 0027h */
#define LF_2			GENMASK(2, 0)
#define LF_3			GENMASK(6, 4)

/* 0028h, 0032h */
#define LINK_EN			BIT(7)
#define TX_RATE			GENMASK(3, 2)

/* 0029h, 0033h */
#define RESET_LINK		BIT(0)
#define RESET_ONESHOT		BIT(1)

/* 002Ah, 0034h */
#define LINK_LOCKED		BIT(0)

/* 0076h, 0086h */
#define DIS_REM_CC		BIT(7)

/* 0100h */
#define VID_LINK_SEL		GENMASK(2, 1)
#define VID_TX_EN		BIT(0)

/* 0101h */
#define BPP			GENMASK(5, 0)

/* 0102h */
#define PCLKDET_A		BIT(7)
#define DRIFT_ERR_A		BIT(6)
#define OVERFLOW_A		BIT(5)
#define FIFO_WARN_A		BIT(4)
#define LIM_HEART		BIT(2)

/* 0107h */
#define VID_TX_ACTIVE_B		BIT(7)
#define VID_TX_ACTIVE_A		BIT(6)

/* 0108h */
#define PCLKDET_B		BIT(7)
#define DRIFT_ERR_B		BIT(6)
#define OVERFLOW_B		BIT(5)
#define FIFO_WARN_B		BIT(4)

/* 0200h */
#define RES_CFG			BIT(7)
#define TX_COM_EN		BIT(5)
#define GPIO_OUT		BIT(4)
#define GPIO_IN			BIT(3)
#define GPIO_OUT_DIS		BIT(0)

/* 0201h */
#define PULL_UPDN_SEL		GENMASK(7, 6)
#define OUT_TYPE		BIT(5)
#define GPIO_TX_ID		GENMASK(4, 0)

/* 0202h */
#define OVR_RES_CFG		BIT(7)
#define IO_EDGE_RATE		GENMASK(6, 5)
#define GPIO_RX_ID		GENMASK(4, 0)

/* 0203h */
#define GPIO_IO_RX_EN		BIT(5)
#define GPIO_OUT_LGC		BIT(4)
#define GPIO_RX_EN_B		BIT(3)
#define GPIO_TX_EN_B		BIT(2)
#define GPIO_RX_EN_A		BIT(1)
#define GPIO_TX_EN_A		BIT(0)

/* 0750h */
#define FRCZEROPAD		GENMASK(7, 6)
#define FRCZPEN			BIT(5)
#define FRCSDGAIN		BIT(4)
#define FRCSDEN			BIT(3)
#define FRCGAIN			GENMASK(2, 1)
#define FRCEN			BIT(0)

/* 0751h */
#define FRCDATAWIDTH		BIT(3)
#define FRCASYNCEN		BIT(2)
#define FRCHSPOL		BIT(1)
#define FRCVSPOL		BIT(0)

/* 0752h */
#define FRCDCMODE		GENMASK(1, 0)

/* 641Ah */
#define DPRX_TRAIN_STATE	GENMASK(7, 4)

/* 7000h */
#define LINK_ENABLE		BIT(0)

/* 7070h */
#define MAX_LANE_COUNT		GENMASK(7, 0)

/* 7074h */
#define MAX_LINK_RATE		GENMASK(7, 0)

enum link_mode {
	DUAL_LINK,
	LINKA,
	LINKB,
	SPLITTER_MODE,
};

struct serdes_function_data {
	u8 gpio_out_dis:1;
	u8 gpio_io_rx_en:1;
	u8 gpio_tx_en_a:1;
	u8 gpio_tx_en_b:1;
	u8 gpio_rx_en_a:1;
	u8 gpio_rx_en_b:1;
	u8 gpio_tx_id;
	u8 gpio_rx_id;
};

struct config_desc {
	u16 reg;
	u8 mask;
	u8 val;
};

struct serdes_group_data {
	const struct config_desc *configs;
	int num_configs;
};

static int MAX96745_MFP0_pins[] = {0};
static int MAX96745_MFP1_pins[] = {1};
static int MAX96745_MFP2_pins[] = {2};
static int MAX96745_MFP3_pins[] = {3};
static int MAX96745_MFP4_pins[] = {4};
static int MAX96745_MFP5_pins[] = {5};
static int MAX96745_MFP6_pins[] = {6};
static int MAX96745_MFP7_pins[] = {7};

static int MAX96745_MFP8_pins[] = {8};
static int MAX96745_MFP9_pins[] = {9};
static int MAX96745_MFP10_pins[] = {10};
static int MAX96745_MFP11_pins[] = {11};
static int MAX96745_MFP12_pins[] = {12};
static int MAX96745_MFP13_pins[] = {13};
static int MAX96745_MFP14_pins[] = {14};
static int MAX96745_MFP15_pins[] = {15};

static int MAX96745_MFP16_pins[] = {16};
static int MAX96745_MFP17_pins[] = {17};
static int MAX96745_MFP18_pins[] = {18};
static int MAX96745_MFP19_pins[] = {19};
static int MAX96745_MFP20_pins[] = {20};
static int MAX96745_MFP21_pins[] = {21};
static int MAX96745_MFP22_pins[] = {22};
static int MAX96745_MFP23_pins[] = {23};

static int MAX96745_MFP24_pins[] = {24};
static int MAX96745_MFP25_pins[] = {25};
static int MAX96745_I2C_pins[] = {3, 7};
static int MAX96745_UART_pins[] = {3, 7};

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static const char *serdes_gpio_groups[] = {
	"MAX96745_MFP0", "MAX96745_MFP1", "MAX96745_MFP2", "MAX96745_MFP3",
	"MAX96745_MFP4", "MAX96745_MFP5", "MAX96745_MFP6", "MAX96745_MFP7",

	"MAX96745_MFP8", "MAX96745_MFP9", "MAX96745_MFP10", "MAX96745_MFP11",
	"MAX96745_MFP12", "MAX96745_MFP13", "MAX96745_MFP14", "MAX96745_MFP15",

	"MAX96745_MFP16", "MAX96745_MFP17", "MAX96745_MFP18", "MAX96745_MFP19",
	"MAX96745_MFP20", "MAX96745_MFP21", "MAX96745_MFP22", "MAX96745_MFP23",

	"MAX96745_MFP24", "MAX96745_MFP25",
};

static const char *MAX96745_I2C_groups[] = { "MAX96745_I2C" };
static const char *MAX96745_UART_groups[] = { "MAX96745_UART" };
#else
static const char * const serdes_gpio_groups[] = {
	"MAX96745_MFP0", "MAX96745_MFP1", "MAX96745_MFP2", "MAX96745_MFP3",
	"MAX96745_MFP4", "MAX96745_MFP5", "MAX96745_MFP6", "MAX96745_MFP7",

	"MAX96745_MFP8", "MAX96745_MFP9", "MAX96745_MFP10", "MAX96745_MFP11",
	"MAX96745_MFP12", "MAX96745_MFP13", "MAX96745_MFP14", "MAX96745_MFP15",

	"MAX96745_MFP16", "MAX96745_MFP17", "MAX96745_MFP18", "MAX96745_MFP19",
	"MAX96745_MFP20", "MAX96745_MFP21", "MAX96745_MFP22", "MAX96745_MFP23",

	"MAX96745_MFP24", "MAX96745_MFP25",
};

static const char * const MAX96745_I2C_groups[] = { "MAX96745_I2C" };
static const char * const MAX96745_UART_groups[] = { "MAX96745_UART" };
#endif

#if KERNEL_VERSION(6, 12, 0) > LINUX_VERSION_CODE
#define GROUP_DESC(nm) \
{ \
	.name = #nm, \
	.pins = nm ## _pins, \
	.num_pins = ARRAY_SIZE(nm ## _pins), \
} \

#define FUNCTION_DESC(nm) \
{ \
	.name = #nm, \
	.group_names = nm##_groups, \
	.num_group_names = ARRAY_SIZE(nm##_groups), \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_A(id) \
{ \
	.name = "SER_TXID"#id"_TO_DES_LINKA", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 1, .gpio_tx_en_a = 1, \
		  .gpio_io_rx_en = 1, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_B(id) \
{ \
	.name = "SER_TXID"#id"_TO_DES_LINKB", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 1, .gpio_tx_en_b = 1, \
		  .gpio_io_rx_en = 1, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_AB(id) \
{ \
	.name = "SER_TXID"#id"_TO_DES", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 1, .gpio_tx_en_a = 1, .gpio_tx_en_b = 1, \
		  .gpio_io_rx_en = 1, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_INPUT_A(id) \
{ \
	.name = "DES_RXID"#id"_TO_SER_LINKA", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_rx_en_a = 1, .gpio_rx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_INPUT_B(id) \
{ \
	.name = "DES_RXID"#id"_TO_SER_LINKB", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_rx_en_b = 1, .gpio_rx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO() \
{ \
	.name = "MAX96745_GPIO", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ } \
	}, \
} \

#else
#define GROUP_DESC(nm) \
{ \
	.grp = { \
		.name = #nm, \
		.pins = nm ## _pins, \
		.npins = ARRAY_SIZE(nm ## _pins), \
	}, \
} \

#define GROUP_DESC_CONFIG(nm) \
{ \
	.grp = { \
		.name = #nm, \
		.pins = nm ## _pins, \
		.npins = ARRAY_SIZE(nm ## _pins), \
	}, \
	.data = (void *)(const struct serdes_group_data []) { \
		{ \
			.configs = nm ## _configs, \
			.num_configs = ARRAY_SIZE(nm ## _configs), \
		} \
	}, \
} \

#define FUNCTION_DESC(nm) \
{ \
	.func = { \
		.name = #nm, \
		.groups = nm##_groups, \
		.ngroups = ARRAY_SIZE(nm##_groups), \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_A(id) \
{ \
	.func = { \
		.name = "SER_TXID"#id"_TO_DES_LINKA", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 1, .gpio_tx_en_a = 1, \
		  .gpio_io_rx_en = 1, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_B(id) \
{ \
	.func = { \
		.name = "SER_TXID"#id"_TO_DES_LINKB", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 1, .gpio_tx_en_b = 1, \
		  .gpio_io_rx_en = 1, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_AB(id) \
{ \
	.func = { \
		.name = "SER_TXID"#id"_TO_DES", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 1, .gpio_tx_en_a = 1, .gpio_tx_en_b = 1, \
		  .gpio_io_rx_en = 1, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_INPUT_A(id) \
{ \
	.func = { \
		.name = "DES_RXID"#id"_TO_SER_LINKA", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_rx_en_a = 1, .gpio_rx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_INPUT_B(id) \
{ \
	.func = { \
		.name = "DES_RXID"#id"_TO_SER_LINKB", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_rx_en_b = 1, .gpio_rx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO() \
{ \
	.func = { \
		.name = "MAX96745_GPIO", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ } \
	}, \
} \

#endif
#endif
