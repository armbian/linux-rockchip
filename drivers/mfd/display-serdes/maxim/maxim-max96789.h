/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * maxim-max96789.h -- register define for max96789 chip
 *
 * Copyright (c) 2023-2028 Rockchip Electronics Co., Ltd.
 *
 * Author:
 *
 */

#ifndef __MFD_SERDES_MAXIM_MAX96789_H__
#define __MFD_SERDES_MAXIM_MAX96789_H__

#include <linux/bitfield.h>

#define GPIO_A_REG(gpio)	(0x02be + ((gpio) * 3))
#define GPIO_B_REG(gpio)	(0x02bf + ((gpio) * 3))
#define GPIO_C_REG(gpio)	(0x02c0 + ((gpio) * 3))

/* 0000h */
#define DEV_ADDR		GENMASK(7, 1)
#define CFG_BLOCK		BIT(0)

/* 0001h */
#define IIC_2_EN		BIT(7)
#define IIC_1_EN		BIT(6)
#define DIS_REM_CC		BIT(4)
#define TX_RATE			GENMASK(3, 2)

/* 0002h */
#define VID_TX_EN_U		BIT(7)
#define VID_TX_EN_Z		BIT(6)
#define VID_TX_EN_Y		BIT(5)
#define VID_TX_EN_X		BIT(4)
#define AUD_TX_EN_Y		BIT(3)
#define AUD_TX_EN_X		BIT(2)

/* 0003h */
#define UART_2_EN		BIT(5)
#define UART_1_EN		BIT(4)

/* 0004h */
#define GMSL2_B			BIT(7)
#define GMSL2_A			BIT(6)
#define LINK_EN_B		BIT(5)
#define LINK_EN_A		BIT(4)
#define AUD_TX_SRC_Y	BIT(1)
#define AUD_TX_SRC_X	BIT(0)


/* 0005h */
#define LOCK_EN			BIT(7)
#define ERRB_EN			BIT(6)
#define PU_LF3			BIT(3)
#define PU_LF2			BIT(2)
#define PU_LF1			BIT(1)
#define PU_LF0			BIT(0)

/* 0006h */
#define RCLKEN			BIT(5)

/* 0010h */
#define RESET_ALL		BIT(7)
#define RESET_LINK		BIT(6)
#define RESET_ONESHOT		BIT(5)
#define AUTO_LINK		BIT(4)
#define SLEEP			BIT(3)
#define REG_ENABLE		BIT(2)
#define LINK_CFG		GENMASK(1, 0)

/* 0013h */
#define LINK_MODE		GENMASK(5, 4)
#define	LOCKED			BIT(3)

/* 001fh */
#define	LINKA_LOCKED	BIT(3)
#define	LINKB_LOCKED	BIT(4)

/* 0026h */
#define LF_1			GENMASK(6, 4)
#define LF_0			GENMASK(2, 0)

/* 0048h */
#define REM_MS_EN		BIT(5)
#define LOC_MS_EN		BIT(4)

/* 0053h */
#define TX_SPLIT_MASK_B		BIT(5)
#define TX_SPLIT_MASK_A		BIT(4)
#define TX_STR_SEL		GENMASK(1, 0)

/* 0140h */
#define AUD_RX_EN		BIT(0)

/* 0170h */
#define SPI_EN			BIT(0)

/* 01e5h */
#define PATGEN_MODE		GENMASK(1, 0)

/* 02beh */
#define RES_CFG			BIT(7)
#define TX_PRIO			BIT(6)
#define TX_COMP_EN		BIT(5)
#define GPIO_OUT		BIT(4)
#define GPIO_IN			BIT(3)
#define GPIO_RX_EN		BIT(2)
#define GPIO_TX_EN		BIT(1)
#define GPIO_OUT_DIS		BIT(0)

/* 02bfh */
#define PULL_UPDN_SEL		GENMASK(7, 6)
#define OUT_TYPE		BIT(5)
#define GPIO_TX_ID		GENMASK(4, 0)

/* 02c0h */
#define OVR_RES_CFG		BIT(7)
#define GPIO_RX_ID		GENMASK(4, 0)

/* 0311h */
#define START_PORTBU		BIT(7)
#define START_PORTBZ		BIT(6)
#define START_PORTBY		BIT(5)
#define START_PORTBX		BIT(4)
#define START_PORTAU		BIT(3)
#define START_PORTAZ		BIT(2)
#define START_PORTAY		BIT(1)
#define START_PORTAX		BIT(0)

/* 032ah */
#define DV_LOCK			BIT(7)
#define DV_SWP_AB		BIT(6)
#define LINE_ALT		BIT(5)
#define DV_CONV			BIT(2)
#define DV_SPL			BIT(1)
#define DV_EN			BIT(0)

/* 0330h */
#define PHY_CONFIG		GENMASK(2, 0)
#define MIPI_RX_RESET		BIT(3)

/* 0331h */
#define NUM_LANES		GENMASK(1, 0)

/* 0385h */
#define DPI_HSYNC_WIDTH_L	GENMASK(7, 0)

/* 0386h */
#define DPI_VYSNC_WIDTH_L	GENMASK(7, 0)

/* 0387h */
#define	DPI_HSYNC_WIDTH_H	GENMASK(3, 0)
#define DPI_VSYNC_WIDTH_H	GENMASK(7, 4)

/* 03a4h */
#define DPI_DE_SKEW_SEL		BIT(1)
#define DPI_DESKEW_EN		BIT(0)

/* 03a5h */
#define DPI_VFP_L		GENMASK(7, 0)

/* 03a6h */
#define DPI_VFP_H		GENMASK(3, 0)
#define DPI_VBP_L		GENMASK(7, 4)

/* 03a7h */
#define DPI_VBP_H		GENMASK(7, 0)

/* 03a8h */
#define DPI_VACT_L		GENMASK(7, 0)

/* 03a9h */
#define DPI_VACT_H		GENMASK(3, 0)

/* 03aah */
#define DPI_HFP_L		GENMASK(7, 0)

/* 03abh */
#define DPI_HFP_H		GENMASK(3, 0)
#define DPI_HBP_L		GENMASK(7, 4)

/* 03ach */
#define DPI_HBP_H		GENMASK(7, 0)

/* 03adh */
#define DPI_HACT_L		GENMASK(7, 0)

/* 03aeh */
#define DPI_HACT_H		GENMASK(4, 0)

/* 055dh */
#define VS_DET			BIT(5)
#define HS_DET			BIT(4)

enum link_mode {
	DUAL_LINK,
	LINKA,
	LINKB,
	SPLITTER_MODE,
};

struct serdes_function_data {
	u8 gpio_out_dis:1;
	u8 gpio_tx_en:1;
	u8 gpio_rx_en:1;
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

static int MAX96789_MFP0_pins[] = {0};
static int MAX96789_MFP1_pins[] = {1};
static int MAX96789_MFP2_pins[] = {2};
static int MAX96789_MFP3_pins[] = {3};
static int MAX96789_MFP4_pins[] = {4};
static int MAX96789_MFP5_pins[] = {5};
static int MAX96789_MFP6_pins[] = {6};
static int MAX96789_MFP7_pins[] = {7};

static int MAX96789_MFP8_pins[] = {8};
static int MAX96789_MFP9_pins[] = {9};
static int MAX96789_MFP10_pins[] = {10};
static int MAX96789_MFP11_pins[] = {11};
static int MAX96789_MFP12_pins[] = {12};
static int MAX96789_MFP13_pins[] = {13};
static int MAX96789_MFP14_pins[] = {14};
static int MAX96789_MFP15_pins[] = {15};

static int MAX96789_MFP16_pins[] = {16};
static int MAX96789_MFP17_pins[] = {17};
static int MAX96789_MFP18_pins[] = {18};
static int MAX96789_MFP19_pins[] = {19};
static int MAX96789_MFP20_pins[] = {20};
static int MAX96789_I2C_pins[] = {19, 20};
static int MAX96789_UART_pins[] = {19, 20};

static const struct config_desc MAX96789_MFP0_configs[] = {
	{ 0x0005, LOCK_EN, 0 },
	{ 0x0048, LOC_MS_EN, 0 },
};

static const struct config_desc MAX96789_MFP1_configs[] = {
	{ 0x0005, ERRB_EN, 0 },
};

static const struct config_desc MAX96789_MFP4_configs[] = {
	{ 0x070, SPI_EN, 0 },
};

static const struct config_desc MAX96789_MFP5_configs[] = {
	{ 0x006, RCLKEN, 0 },
};

static const struct config_desc MAX96789_MFP7_configs[] = {
	{ 0x0002, AUD_TX_EN_X, 0 },
	{ 0x0002, AUD_TX_EN_Y, 0 }
};

static const struct config_desc MAX96789_MFP8_configs[] = {
	{ 0x0002, AUD_TX_EN_X, 0 },
	{ 0x0002, AUD_TX_EN_Y, 0 }
};

static const struct config_desc MAX96789_MFP9_configs[] = {
	{ 0x0002, AUD_TX_EN_X, 0 },
	{ 0x0002, AUD_TX_EN_Y, 0 }
};

static const struct config_desc MAX96789_MFP10_configs[] = {
	{ 0x0001, IIC_2_EN, 0 },
	{ 0x0003, UART_2_EN, 0 },
	{ 0x0140, AUD_RX_EN, 0 },
};

static const struct config_desc MAX96789_MFP11_configs[] = {
	{ 0x0001, IIC_2_EN, 0 },
	{ 0x0003, UART_2_EN, 0 },
	{ 0x0140, AUD_RX_EN, 0 },
};

static const struct config_desc MAX96789_MFP12_configs[] = {
	{ 0x0140, AUD_RX_EN, 0 },
};

static const struct config_desc MAX96789_MFP13_configs[] = {
	{ 0x0005, PU_LF0, 0 },
};

static const struct config_desc MAX96789_MFP14_configs[] = {
	{ 0x0005, PU_LF1, 0 },
};

static const struct config_desc MAX96789_MFP15_configs[] = {
	{ 0x0005, PU_LF2, 0 },
};

static const struct config_desc MAX96789_MFP16_configs[] = {
	{ 0x0005, PU_LF3, 0 },
};

static const struct config_desc MAX96789_MFP17_configs[] = {
	{ 0x0001, IIC_1_EN, 0 },
	{ 0x0003, UART_1_EN, 0 },
};

static const struct config_desc MAX96789_MFP18_configs[] = {
	{ 0x0001, IIC_1_EN, 0 },
	{ 0x0003, UART_1_EN, 0 },
};

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static const char *serdes_gpio_groups[] = {
	"MAX96789_MFP0", "MAX96789_MFP1", "MAX96789_MFP2", "MAX96789_MFP3",
	"MAX96789_MFP4", "MAX96789_MFP5", "MAX96789_MFP6", "MAX96789_MFP7",

	"MAX96789_MFP8", "MAX96789_MFP9", "MAX96789_MFP10", "MAX96789_MFP11",
	"MAX96789_MFP12", "MAX96789_MFP13", "MAX96789_MFP14", "MAX96789_MFP15",

	"MAX96789_MFP16", "MAX96789_MFP17", "MAX96789_MFP18", "MAX96789_MFP19",
	"MAX96789_MFP20",
};

static const char *MAX96789_I2C_groups[] = { "MAX96789_I2C" };
static const char *MAX96789_UART_groups[] = { "MAX96789_UART" };
#else
static const char * const serdes_gpio_groups[] = {
	"MAX96789_MFP0", "MAX96789_MFP1", "MAX96789_MFP2", "MAX96789_MFP3",
	"MAX96789_MFP4", "MAX96789_MFP5", "MAX96789_MFP6", "MAX96789_MFP7",

	"MAX96789_MFP8", "MAX96789_MFP9", "MAX96789_MFP10", "MAX96789_MFP11",
	"MAX96789_MFP12", "MAX96789_MFP13", "MAX96789_MFP14", "MAX96789_MFP15",

	"MAX96789_MFP16", "MAX96789_MFP17", "MAX96789_MFP18", "MAX96789_MFP19",
	"MAX96789_MFP20",
};

static const char * const MAX96789_I2C_groups[] = { "MAX96789_I2C" };
static const char * const MAX96789_UART_groups[] = { "MAX96789_UART" };
#endif

#if KERNEL_VERSION(6, 12, 0) > LINUX_VERSION_CODE
#define GROUP_DESC(nm) \
{ \
	.name = #nm, \
	.pins = nm ## _pins, \
	.num_pins = ARRAY_SIZE(nm ## _pins), \
} \

#define GROUP_DESC_CONFIG(nm) \
{ \
	.name = #nm, \
	.pins = nm ## _pins, \
	.num_pins = ARRAY_SIZE(nm ## _pins), \
	.data = (void *)(const struct serdes_group_data []) { \
		{ \
			.configs = nm ## _configs, \
			.num_configs = ARRAY_SIZE(nm ## _configs), \
		} \
	}, \
} \

#define FUNCTION_DESC(nm) \
{ \
	.name = #nm, \
	.group_names = nm##_groups, \
	.num_group_names = ARRAY_SIZE(nm##_groups), \
} \

#define FUNCTION_DESC_GPIO_INPUT(id) \
{ \
	.name = "DES_RXID"#id"_TO_SER", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 0, .gpio_rx_en = 1, .gpio_rx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT(id) \
{ \
	.name = "SER_TXID"#id"_TO_DES", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 1, .gpio_tx_en = 1, .gpio_tx_id = id } \
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

#define FUNCTION_DESC_GPIO_INPUT(id) \
{ \
	.func = { \
		.name = "DES_RXID"#id"_TO_SER", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 0, .gpio_rx_en = 1, .gpio_rx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT(id) \
{ \
	.func = { \
		.name = "SER_TXID"#id"_TO_DES", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 1, .gpio_tx_en = 1, .gpio_tx_id = id } \
	}, \
} \

#endif
#endif
