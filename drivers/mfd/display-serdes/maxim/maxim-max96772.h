/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * include/linux/mfd/serdes/gpio.h -- GPIO for different serdes chip
 *
 * Copyright (c) 2023-2028 Rockchip Electronics Co., Ltd.
 *
 * Author: luowei <lw@rock-chips.com>
 *
 */

#ifndef __MFD_SERDES_MAXIM_MAX96772_H__
#define __MFD_SERDES_MAXIM_MAX96772_H__

#define GPIO_A_REG(gpio)      (0x02b0 + ((gpio) * 3))
#define GPIO_B_REG(gpio)      (0x02b1 + ((gpio) * 3))
#define GPIO_C_REG(gpio)      (0x02b2 + ((gpio) * 3))

/* 0002h */
#define VID_EN_U              BIT(7)
#define VID_EN_Z              BIT(6)
#define VID_EN_Y              BIT(5)
#define VID_EN_X              BIT(4)
#define AUD_TX_EN             BIT(2)

/* 0004h */
#define LINK_EN_B             BIT(5)
#define LINK_EN_A             BIT(4)

/* 0010h */
#define RESET_ALL             BIT(7)
#define RESET_LINK            BIT(6)
#define RESET_ONESHOT         BIT(5)
#define AUTO_LINK             BIT(4)
#define SLEEP                 BIT(3)
#define REG_ENABLE            BIT(2)
#define LINK_CFG              GENMASK(1, 0)

/* 0013h */
#define LINK_MODE             BIT(4)
#define LOCKED                BIT(3)

/* 001fh */
#define LINKA_LOCKED          BIT(4)
#define LINKB_LOCKED          BIT(5)

/* 02b0h */
#define RES_CFG               BIT(7)
#define RSVD                  BIT(6)
#define TX_COMP_EN            BIT(5)
#define GPIO_OUT              BIT(4)
#define GPIO_IN               BIT(3)
#define GPIO_RX_EN            BIT(2)
#define GPIO_TX_EN            BIT(1)
#define GPIO_OUT_DIS          BIT(0)

/* 02b1h */
#define PULL_UPDN_SEL         GENMASK(7, 6)
#define OUT_TYPE              BIT(5)
#define GPIO_TX_ID            GENMASK(4, 0)

/* 02b2h */
#define OVR_RES_CFG           BIT(7)
#define GPIO_RX_ID            GENMASK(4, 0)

struct config_desc {
	u16 reg;
	u8 mask;
	u8 val;
};

struct serdes_group_data {
	const struct config_desc *configs;
	int num_configs;
};

static int MAX96772_GPIO0_pins[] = {0};
static int MAX96772_GPIO1_pins[] = {1};
static int MAX96772_GPIO2_pins[] = {2};
static int MAX96772_GPIO3_pins[] = {3};
static int MAX96772_GPIO4_pins[] = {4};
static int MAX96772_GPIO5_pins[] = {5};
static int MAX96772_GPIO6_pins[] = {6};
static int MAX96772_GPIO7_pins[] = {7};

static int MAX96772_GPIO8_pins[] = {8};
static int MAX96772_GPIO9_pins[] = {9};
static int MAX96772_GPIO10_pins[] = {10};
static int MAX96772_GPIO11_pins[] = {11};
static int MAX96772_GPIO12_pins[] = {12};
static int MAX96772_GPIO13_pins[] = {13};
static int MAX96772_GPIO14_pins[] = {14};
static int MAX96772_GPIO15_pins[] = {15};

static const struct config_desc MAX96772_GPIO7_configs[] = {
	{ 0x02, AUD_TX_EN, 0},
};

struct serdes_function_data {
	u8 gpio_out_dis:1;
	u8 gpio_tx_en:1;
	u8 gpio_rx_en:1;
	u8 gpio_in_level:1;
	u8 gpio_out_level:1;
	u8 gpio_tx_id;
	u8 gpio_rx_id;
	u16 mdelay;
};

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static const char *serdes_gpio_groups[] = {
	"MAX96772_GPIO0", "MAX96772_GPIO1", "MAX96772_GPIO2", "MAX96772_GPIO3",
	"MAX96772_GPIO4", "MAX96772_GPIO5", "MAX96772_GPIO6", "MAX96772_GPIO7",

	"MAX96772_GPIO8", "MAX96772_GPIO9", "MAX96772_GPIO10", "MAX96772_GPIO11",
	"MAX96772_GPIO12", "MAX96772_GPIO13", "MAX96772_GPIO14", "MAX96772_GPIO15",
};
#else
static const char * const serdes_gpio_groups[] = {
	"MAX96772_GPIO0", "MAX96772_GPIO1", "MAX96772_GPIO2", "MAX96772_GPIO3",
	"MAX96772_GPIO4", "MAX96772_GPIO5", "MAX96772_GPIO6", "MAX96772_GPIO7",

	"MAX96772_GPIO8", "MAX96772_GPIO9", "MAX96772_GPIO10", "MAX96772_GPIO11",
	"MAX96772_GPIO12", "MAX96772_GPIO13", "MAX96772_GPIO14", "MAX96772_GPIO15",
};
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

#define FUNCTION_DESC_GPIO_INPUT_BYPASS(id) \
{ \
	.name = "SER_TO_DES_RXID"#id, \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_rx_en = 1, .gpio_rx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_BYPASS(id) \
{ \
	.name = "DES_TXID"#id"_TO_SER", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 1, .gpio_tx_en = 1, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_LOW(id) \
{ \
	.name = "DES_TXID"#id"_OUTPUT_LOW", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 0, .gpio_tx_en = 0, \
		  .gpio_rx_en = 0, .gpio_out_level = 0, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_HIGH(id) \
{ \
	.name = "DES_TXID"#id"_OUTPUT_HIGH", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 0, .gpio_tx_en = 0, \
		  .gpio_rx_en = 0, .gpio_out_level = 1, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DES_DELAY_MS(ms) \
{ \
	.name = "DELAY_"#ms"MS", \
	.group_names = serdes_gpio_groups, \
	.num_group_names = ARRAY_SIZE(serdes_gpio_groups), \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .mdelay = ms, } \
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

#define FUNCTION_DESC_GPIO_INPUT_BYPASS(id) \
{ \
	.func = { \
		.name = "SER_TO_DES_RXID"#id, \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_rx_en = 1, .gpio_rx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_BYPASS(id) \
{ \
	.func = { \
		.name = "DES_TXID"#id"_TO_SER", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 1, .gpio_tx_en = 1, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_LOW(id) \
{ \
	.func = { \
		.name = "DES_TXID"#id"_OUTPUT_LOW", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 0, .gpio_tx_en = 0, \
		  .gpio_rx_en = 0, .gpio_out_level = 0, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DESC_GPIO_OUTPUT_HIGH(id) \
{ \
	.func = { \
		.name = "DES_TXID"#id"_OUTPUT_HIGH", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .gpio_out_dis = 0, .gpio_tx_en = 0, \
		  .gpio_rx_en = 0, .gpio_out_level = 1, .gpio_tx_id = id } \
	}, \
} \

#define FUNCTION_DES_DELAY_MS(ms) \
{ \
	.func = { \
		.name = "DELAY_"#ms"MS", \
		.groups = serdes_gpio_groups, \
		.ngroups = ARRAY_SIZE(serdes_gpio_groups), \
	}, \
	.data = (void *)(const struct serdes_function_data []) { \
		{ .mdelay = ms, } \
	}, \
} \

#endif
#endif
