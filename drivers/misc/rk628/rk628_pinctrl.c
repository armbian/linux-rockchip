// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 *
 * Author: Wyon Bi <bivvy.bi@rock-chips.com>
 */

#include "rk628.h"
#include "rk628_gpio.h"

struct rk628_pin_iomux_group {
	unsigned int pins;
	int bank;
	int mux;
	int iomux_base;
	int gpio_base;
	int pull_reg;
};

#define PINCTRL_GROUP(a, b, c, d, e, f) \
	{.pins = a, .bank = b, .mux = c, .iomux_base = d, .gpio_base = e, .pull_reg = f}

static const struct rk628_pin_iomux_group rk628_pin_iomux_groups[] = {
	PINCTRL_GROUP(GPIO0_A0, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON,
		      RK628_GPIO0_BASE, GRF_GPIO0A_P_CON),
	PINCTRL_GROUP(GPIO0_A1, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON,
		      RK628_GPIO0_BASE, GRF_GPIO0A_P_CON),
	PINCTRL_GROUP(GPIO0_A2, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON, RK628_GPIO0_BASE, 0),
	PINCTRL_GROUP(GPIO0_A3, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON,
		      RK628_GPIO0_BASE, GRF_GPIO0A_P_CON),
	PINCTRL_GROUP(GPIO0_A4, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON,
		      RK628_GPIO0_BASE, GRF_GPIO0A_P_CON),
	PINCTRL_GROUP(GPIO0_A5, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON,
		      RK628_GPIO0_BASE, GRF_GPIO0A_P_CON),
	PINCTRL_GROUP(GPIO0_A6, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON,
		      RK628_GPIO0_BASE, GRF_GPIO0A_P_CON),
	PINCTRL_GROUP(GPIO0_A7, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON,
		      RK628_GPIO0_BASE, GRF_GPIO0A_P_CON),
	PINCTRL_GROUP(GPIO0_B0, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON, RK628_GPIO0_BASE, 0),
	PINCTRL_GROUP(GPIO0_B1, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON, RK628_GPIO0_BASE, 0),
	PINCTRL_GROUP(GPIO0_B2, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON, RK628_GPIO0_BASE, 0),
	PINCTRL_GROUP(GPIO0_B3, GPIO_BANK0, 1, GRF_GPIO0AB_SEL_CON, RK628_GPIO0_BASE, 0),

	PINCTRL_GROUP(GPIO1_A0, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON,
		      RK628_GPIO1_BASE, GRF_GPIO1A_P_CON),
	PINCTRL_GROUP(GPIO1_A1, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON,
		      RK628_GPIO1_BASE, GRF_GPIO1A_P_CON),
	PINCTRL_GROUP(GPIO1_A2, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON,
		      RK628_GPIO1_BASE, GRF_GPIO1A_P_CON),
	PINCTRL_GROUP(GPIO1_A3, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON,
		      RK628_GPIO1_BASE, GRF_GPIO1A_P_CON),
	PINCTRL_GROUP(GPIO1_A4, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON,
		      RK628_GPIO1_BASE, GRF_GPIO1A_P_CON),
	PINCTRL_GROUP(GPIO1_A5, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON,
		      RK628_GPIO1_BASE, GRF_GPIO1A_P_CON),
	PINCTRL_GROUP(GPIO1_A6, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON,
		      RK628_GPIO1_BASE, GRF_GPIO1A_P_CON),
	PINCTRL_GROUP(GPIO1_A7, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON,
		      RK628_GPIO1_BASE, GRF_GPIO1A_P_CON),
	PINCTRL_GROUP(GPIO1_B0, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON, RK628_GPIO1_BASE, 0),
	PINCTRL_GROUP(GPIO1_B1, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON, RK628_GPIO1_BASE, 0),
	PINCTRL_GROUP(GPIO1_B2, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON, RK628_GPIO1_BASE, 0),
	PINCTRL_GROUP(GPIO1_B3, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON, RK628_GPIO1_BASE, 0),
	PINCTRL_GROUP(GPIO1_B4, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON, RK628_GPIO1_BASE, 0),
	PINCTRL_GROUP(GPIO1_B5, GPIO_BANK1, 1, GRF_GPIO1AB_SEL_CON, RK628_GPIO1_BASE, 0),

	PINCTRL_GROUP(GPIO2_A0, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2A_P_CON),
	PINCTRL_GROUP(GPIO2_A1, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2A_P_CON),
	PINCTRL_GROUP(GPIO2_A2, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2A_P_CON),
	PINCTRL_GROUP(GPIO2_A3, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2A_P_CON),
	PINCTRL_GROUP(GPIO2_A4, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2A_P_CON),
	PINCTRL_GROUP(GPIO2_A5, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2A_P_CON),
	PINCTRL_GROUP(GPIO2_A6, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2A_P_CON),
	PINCTRL_GROUP(GPIO2_A7, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2A_P_CON),
	PINCTRL_GROUP(GPIO2_B0, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2B_P_CON),
	PINCTRL_GROUP(GPIO2_B1, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2B_P_CON),
	PINCTRL_GROUP(GPIO2_B2, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2B_P_CON),
	PINCTRL_GROUP(GPIO2_B3, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2B_P_CON),
	PINCTRL_GROUP(GPIO2_B4, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2B_P_CON),
	PINCTRL_GROUP(GPIO2_B5, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2B_P_CON),
	PINCTRL_GROUP(GPIO2_B6, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2B_P_CON),
	PINCTRL_GROUP(GPIO2_B7, GPIO_BANK2, 1, GRF_GPIO2AB_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2B_P_CON),
	PINCTRL_GROUP(GPIO2_C0, GPIO_BANK2, 1, GRF_GPIO2C_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2C_P_CON),
	PINCTRL_GROUP(GPIO2_C1, GPIO_BANK2, 1, GRF_GPIO2C_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2C_P_CON),
	PINCTRL_GROUP(GPIO2_C2, GPIO_BANK2, 1, GRF_GPIO2C_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2C_P_CON),
	PINCTRL_GROUP(GPIO2_C3, GPIO_BANK2, 1, GRF_GPIO2C_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2C_P_CON),
	PINCTRL_GROUP(GPIO2_C4, GPIO_BANK2, 1, GRF_GPIO2C_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2C_P_CON),
	PINCTRL_GROUP(GPIO2_C5, GPIO_BANK2, 1, GRF_GPIO2C_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2C_P_CON),
	PINCTRL_GROUP(GPIO2_C6, GPIO_BANK2, 1, GRF_GPIO2C_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2C_P_CON),
	PINCTRL_GROUP(GPIO2_C7, GPIO_BANK2, 1, GRF_GPIO2C_SEL_CON,
		      RK628_GPIO2_BASE, GRF_GPIO2C_P_CON),

	PINCTRL_GROUP(GPIO3_A0, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, GRF_GPIO3A_P_CON),
	PINCTRL_GROUP(GPIO3_A1, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, GRF_GPIO3A_P_CON),
	PINCTRL_GROUP(GPIO3_A2, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, GRF_GPIO3A_P_CON),
	PINCTRL_GROUP(GPIO3_A3, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, GRF_GPIO3A_P_CON),
	PINCTRL_GROUP(GPIO3_A4, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, 0),
	PINCTRL_GROUP(GPIO3_A5, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, 0),
	PINCTRL_GROUP(GPIO3_A6, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, 0),
	PINCTRL_GROUP(GPIO3_A7, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, 0),
	PINCTRL_GROUP(GPIO3_B0, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, GRF_GPIO3B_P_CON),
	PINCTRL_GROUP(GPIO3_B1, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, GRF_GPIO3B_P_CON),
	PINCTRL_GROUP(GPIO3_B2, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, GRF_GPIO3B_P_CON),
	PINCTRL_GROUP(GPIO3_B3, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, GRF_GPIO3B_P_CON),
	PINCTRL_GROUP(GPIO3_B4, GPIO_BANK3, 1, GRF_GPIO3AB_SEL_CON,
		      RK628_GPIO3_BASE, GRF_GPIO3B_P_CON),

	PINCTRL_GROUP(PIN_I2SM_SCK, GPIO_BANKX, 1, GRF_SYSTEM_CON3, 0, 0),
	PINCTRL_GROUP(PIN_I2SM_D, GPIO_BANKX, 1, GRF_SYSTEM_CON3, 0, 0),
	PINCTRL_GROUP(PIN_I2SM_LR, GPIO_BANKX, 1, GRF_SYSTEM_CON3, 0, 0),
	PINCTRL_GROUP(PIN_RXDDC_SCL, GPIO_BANKX, 1, GRF_SYSTEM_CON3, 0, 0),
	PINCTRL_GROUP(PIN_RXDDC_SDA, GPIO_BANKX, 1, GRF_SYSTEM_CON3, 0, 0),
	PINCTRL_GROUP(PIN_HDMIRX_CE, GPIO_BANKX, 1, GRF_SYSTEM_CON3, 0, 0),
	PINCTRL_GROUP(PIN_JTAG_EN, GPIO_BANKX, 1, GRF_SYSTEM_CON3, 0, 0),
	PINCTRL_GROUP(PIN_UART_SEL, GPIO_BANKX, 1, GRF_SYSTEM_CON3, 0, 0),
	PINCTRL_GROUP(PIN_UART_RTS_EN, GPIO_BANKX, 1, GRF_SYSTEM_CON3, 0, 0),
	PINCTRL_GROUP(PIN_UART_CTS_EN, GPIO_BANKX, 1, GRF_SYSTEM_CON3, 0, 0),

};

static int rk628_calc_mux_offset(struct rk628 *rk628, int mux, int reg, int offset)
{
	int val = 0, orig;

	switch (reg) {
	case GRF_SYSTEM_CON3:
		rk628_i2c_read(rk628, reg, &orig);
		if (mux)
			val = BIT(offset) | orig;
		else
			val = ~BIT(offset) & orig;
		break;
	case GRF_GPIO0AB_SEL_CON:
		if (offset >= 4 && offset < 8) {
			offset += offset - 4;
			val = 0x3 << (offset + 16) | (mux << offset);
		} else if (offset > 7) {
			offset += 4;
			val = BIT(offset + 16) | (mux << offset);
		} else {
			val = BIT(offset + 16) | (mux << offset);
		}
		break;
	case GRF_GPIO1AB_SEL_CON:
		if (offset == 13)
			offset++;
		if (offset > 11)
			val = 0x3 << (offset + 16) | (mux << offset);
		else
			val = BIT(offset + 16) | (mux << offset);
		break;
	case GRF_GPIO2AB_SEL_CON:
		val = BIT(offset + 16) | (mux << offset);
		break;
	case GRF_GPIO2C_SEL_CON:
		offset -= 16;
		val = 0x3 << ((offset*2) + 16) | (mux << (offset*2));
		break;
	case GRF_GPIO3AB_SEL_CON:
		if (offset > 11)
			val = 0x3 << (offset + 16) | (mux << offset);
		else
			val = BIT(offset + 16) | (mux << offset);
		break;
	default:
		break;
	}

	return val;
}

int rk628_misc_pinctrl_set_mux(struct rk628 *rk628, int gpio, int mux)
{
	int i, iomux_base, offset, val;

	mux &= 0x3;

	for (i = 0; i < ARRAY_SIZE(rk628_pin_iomux_groups); i++) {
		if (rk628_pin_iomux_groups[i].pins == gpio) {
			iomux_base = rk628_pin_iomux_groups[i].iomux_base;
			offset = rk628_pin_iomux_groups[i].pins % BANK_OFFSET;
			break;
		}
	}

	if ((i == ARRAY_SIZE(rk628_pin_iomux_groups)) || (!iomux_base)) {
		pr_info("%s invalid gpio or iomux_base\n", __func__);
		return -1;
	}

	val = rk628_calc_mux_offset(rk628, mux, iomux_base, offset);

	rk628_i2c_write(rk628, iomux_base, val);

	return 0;

}


/* generic gpio chip */
int rk628_misc_gpio_get_value(struct rk628 *rk628, int gpio)
{
	int i, data_reg, offset, val;

	for (i = 0; i < ARRAY_SIZE(rk628_pin_iomux_groups); i++) {
		if (rk628_pin_iomux_groups[i].pins == gpio) {
			data_reg = rk628_pin_iomux_groups[i].gpio_base + GPIO_EXT_PORT;
			offset = rk628_pin_iomux_groups[i].pins % BANK_OFFSET;
			break;
		}
	}

	if ((i == ARRAY_SIZE(rk628_pin_iomux_groups)) || (!data_reg)) {
		pr_info("%s invalid gpio or data_reg\n", __func__);
		return -1;
	}

	rk628_i2c_read(rk628, data_reg, &val);

	val >>= offset;
	val &= 1;

	return val;
}

int rk628_misc_gpio_set_value(struct rk628 *rk628, int gpio, int value)
{
	int i, data_reg, offset, val;

	for (i = 0; i < ARRAY_SIZE(rk628_pin_iomux_groups); i++) {
		if (rk628_pin_iomux_groups[i].pins == gpio) {
			offset = rk628_pin_iomux_groups[i].pins % BANK_OFFSET;
			if (offset >= 16) {
				data_reg = rk628_pin_iomux_groups[i].gpio_base + GPIO_SWPORT_DR_H;
				offset -= 16;
			} else {
				data_reg = rk628_pin_iomux_groups[i].gpio_base + GPIO_SWPORT_DR_L;
			}
			break;
		}
	}

	if ((i == ARRAY_SIZE(rk628_pin_iomux_groups)) || (!data_reg)) {
		pr_info("%s invalid gpio or data_reg\n", __func__);
		return -1;
	}

	if (value)
		val = BIT(offset + 16) | BIT(offset);
	else
		val = BIT(offset + 16) | (0xffff & ~BIT(offset));

	rk628_i2c_write(rk628, data_reg, val);

	return 0;
}



int rk628_misc_gpio_set_direction(struct rk628 *rk628, int gpio, int direction)
{
	int i, dir_reg, offset, val;

	for (i = 0; i < ARRAY_SIZE(rk628_pin_iomux_groups); i++) {
		if (rk628_pin_iomux_groups[i].pins == gpio) {
			offset = rk628_pin_iomux_groups[i].pins % BANK_OFFSET;
			if (offset >= 16) {
				dir_reg = rk628_pin_iomux_groups[i].gpio_base + GPIO_SWPORT_DDR_H;
				offset -= 16;
			} else {
				dir_reg = rk628_pin_iomux_groups[i].gpio_base + GPIO_SWPORT_DDR_L;
			}
			break;
		}
	}

	if ((i == ARRAY_SIZE(rk628_pin_iomux_groups)) || (!dir_reg)) {
		pr_info("%s invalid gpio or dir_reg\n", __func__);
		return -1;
	}

	if (!direction)
		val = BIT(offset + 16) | (0xffff & ~BIT(offset));
	else
		val = BIT(offset + 16) | BIT(offset);

	rk628_i2c_write(rk628, dir_reg, val);

	return 0;
}

#ifdef RK628_MISC_GPIO_TEST
int rk628_misc_iomux_init(struct rk628 *rk628)
{
	int i, iomux_base, offset, val, mux;

	for (i = 0; i < ARRAY_SIZE(rk628_pin_iomux_groups); i++) {
		mux = rk628_pin_iomux_groups[i].mux;
		iomux_base = rk628_pin_iomux_groups[i].iomux_base;
		offset = rk628_pin_iomux_groups[i].pins % BANK_OFFSET;
		if (iomux_base) {
			val = rk628_calc_mux_offset(rk628, mux, iomux_base, offset);
			rk628_i2c_write(rk628, iomux_base, val);
		}
	}

	return 0;
}
#endif

int rk628_misc_gpio_direction_input(struct rk628 *rk628, int gpio)
{
	rk628_misc_pinctrl_set_mux(rk628, gpio, GPIO_FUNC);

	rk628_misc_gpio_set_direction(rk628, gpio, GPIO_DIRECTION_IN);

	return 0;
}


int rk628_misc_gpio_direction_output(struct rk628 *rk628, int gpio, int value)
{

	rk628_misc_pinctrl_set_mux(rk628, gpio, GPIO_FUNC);
	rk628_misc_gpio_set_value(rk628, gpio, value);
	rk628_misc_gpio_set_direction(rk628, gpio, GPIO_DIRECTION_OUT);
	return 0;
}


int rk628_misc_gpio_set_pull_highz_up_down(struct rk628 *rk628, int gpio, int pull)
{
	int i, bank, pull_reg = 0, offset, val = 0;
	int valid_pinnum[] = { 8, 8, 24, 13 };

	for (i = 0; i < ARRAY_SIZE(rk628_pin_iomux_groups); i++) {
		if (rk628_pin_iomux_groups[i].pins == gpio) {
			bank = rk628_pin_iomux_groups[i].bank;
			pull_reg = rk628_pin_iomux_groups[i].pull_reg;
			offset = rk628_pin_iomux_groups[i].pins % BANK_OFFSET;
			break;
		}
	}

	if ((i == ARRAY_SIZE(rk628_pin_iomux_groups))  || (!pull_reg)) {
		pr_info("rk628_gpio_pull_highz_up_down invalid gpio or pull_reg\n");
		return -1;
	}

	switch (bank) {
	case GPIO_BANK0:
		if (pull == GPIO_PULL_UP)
			return -1;

		if (offset == 2)
			return -1;

		if (offset < valid_pinnum[bank])
			val = 0x3 << (2 * offset + 16) | pull << (2 * offset);
		break;
	case GPIO_BANK1:
		if (pull == GPIO_PULL_UP)
			return -1;

		if (offset == 2)
			return -1;

		if (offset < valid_pinnum[bank])
			val = 0x3 << (2 * offset + 16) | pull << (2 * offset);
		break;
	case GPIO_BANK2:
		if (pull == GPIO_PULL_UP)
			pull = GPIO_PULL_DOWN;
		else if (pull == GPIO_PULL_DOWN)
			pull = GPIO_PULL_UP;

		if (offset < valid_pinnum[bank]) {
			offset = offset % 8;
			val = 0x3 << (2 * offset + 16) | pull << (2 * offset);
		}
		break;
	case GPIO_BANK3:
		if (pull == GPIO_PULL_UP && (offset == 2 || offset == 11 || offset == 12))
			return -1;
		else if (pull == GPIO_PULL_DOWN && (offset == 9 || offset == 10))
			return -1;

		if (offset == 0 || offset == 1 || offset == 3 || offset == 8) {
			if (pull == GPIO_PULL_UP)
				pull = GPIO_PULL_DOWN;
			else if (pull == GPIO_PULL_DOWN)
				pull = GPIO_PULL_UP;
		}

		if ((offset > 7 && offset < valid_pinnum[bank]) || offset < 4) {
			offset = offset % 8;
			val = 0x3 << (2 * offset + 16) | pull << (2 * offset);
		}
		break;
	default:
		break;
	}

	rk628_i2c_write(rk628, pull_reg, val);

	return 0;
}

#ifdef RK628_MISC_GPIO_TEST
int rk628_misc_gpio_test_all(struct rk628 *rk628)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rk628_pin_iomux_groups); i++) {
		if (rk628_pin_iomux_groups[i].pins && (rk628_pin_iomux_groups[i].pins != GPIO1_A1)
		    && (rk628_pin_iomux_groups[i].pins != GPIO2_C0)
		    && (rk628_pin_iomux_groups[i].pins != GPIO2_C1)
		    && (rk628_pin_iomux_groups[i].pins != GPIO2_C2)
		    && (rk628_pin_iomux_groups[i].pins != GPIO2_C3)
		    && (rk628_pin_iomux_groups[i].pins != GPIO2_C4))
			rk628_misc_gpio_direction_output(rk628, rk628_pin_iomux_groups[i].pins, 1);
	}

	for (i = 0; i < ARRAY_SIZE(rk628_pin_iomux_groups); i++) {
		if (rk628_pin_iomux_groups[i].pins && (rk628_pin_iomux_groups[i].pins != GPIO1_A1)
		    && (rk628_pin_iomux_groups[i].pins != GPIO2_C0)
		    && (rk628_pin_iomux_groups[i].pins != GPIO2_C1)
		    && (rk628_pin_iomux_groups[i].pins != GPIO2_C2)
		    && (rk628_pin_iomux_groups[i].pins != GPIO2_C3)
		    && (rk628_pin_iomux_groups[i].pins != GPIO2_C4))
			rk628_misc_gpio_direction_output(rk628, rk628_pin_iomux_groups[i].pins, 0);
	}

	return 0;
}
#endif

