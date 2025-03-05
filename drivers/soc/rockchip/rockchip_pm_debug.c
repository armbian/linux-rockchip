// SPDX-License-Identifier: GPL-2.0-only
/*
 * Rockchip Power Management Debug Support.
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/rockchip/cpu.h>
#include <linux/syscore_ops.h>

#define RK_IOC_CFG(NAME, REG, LEN, ID, TABLE)	\
	{					\
		.name = NAME,			\
		.reg_base = REG,		\
		.len = LEN,			\
		.bank_id = ID,			\
		.table = TABLE,			\
	}

struct rk_gpio_table {
	u32 offset;
	u32 len;
};

struct rk_gpio_chip {
	char *name;
	u32 reg_base;
	u32 len;
	int bank_id;
	const struct rk_gpio_table *table;
};

static const struct rk_gpio_table rk3506_gpio_table[] = {
	{ .offset = 0, .len = 0x20,},
	{ .offset = 0x100, .len = 0x40,},
	{ .offset = 0x200, .len = 0x10,},
	{ .offset = 0x300, .len = 0x10,},
	{ .offset = 0x400, .len = 0x10,},
	{ .offset = 0x500, .len = 0x10,},
	{ .offset = 0x600, .len = 0x10,},
	{ },
};

static const struct rk_gpio_chip rk3506_table[] = {
	RK_IOC_CFG("gpio0_ioc", 0xff950000, 0x510, 0, rk3506_gpio_table),
	RK_IOC_CFG("gpio1_ioc", 0xff660000, 0x510, 1, rk3506_gpio_table),
	RK_IOC_CFG("gpio2_ioc", 0xff4d8000, 0x510, 2, rk3506_gpio_table),
	RK_IOC_CFG("gpio3_ioc", 0xff4d8000, 0x510, 3, rk3506_gpio_table),
	{ .name = "gpio4_ioc", .reg_base = 0xff4d8840, .len = 0x10},
	{ .name = "rm_io", .reg_base = 0xff910080, .len = 0x80},
	{ .name = "gpio0", .reg_base = 0xff940000, .len = 0x80},
	{ .name = "gpio1", .reg_base = 0xff870000, .len = 0x80},
	{ .name = "gpio2", .reg_base = 0xff1c0000, .len = 0x80},
	{ .name = "gpio3", .reg_base = 0xff1d0000, .len = 0x80},
	{ .name = "gpio4", .reg_base = 0xff1e0000, .len = 0x80},
	{ },
};

static const struct rk_gpio_chip *chip_table;

static void rk_gpio_dump(const struct rk_gpio_chip *chip)
{
	const struct rk_gpio_table *table = chip->table;
	void __iomem *reg = ioremap(chip->reg_base, chip->len);
	char prefix[16];
	int cnt, end;

	if (!reg) {
		pr_err("Failed to map registers\n");
		return;
	}
	pr_info("%s:\n", chip->name);

	if (!table) {
		for (cnt = 0; cnt < chip->len; cnt += 0x10) {
			sprintf(prefix, "%08x: ", chip->reg_base + cnt);
			print_hex_dump(KERN_INFO, prefix, DUMP_PREFIX_NONE, 16,
				       4, (u8 *)reg + cnt, 16, false);
		}
	}

	for (; table && table->len; table++) {
		cnt = table->offset + table->len * chip->bank_id;
		end = cnt + table->len;
		for (; cnt < end; cnt += 0x10) {
			sprintf(prefix, "%08x: ", chip->reg_base + cnt);
			print_hex_dump(KERN_INFO, prefix, DUMP_PREFIX_NONE, 16,
				       4, (u8 *)reg + cnt, 16, false);
		}

	}
	iounmap(reg);
}

static int rockchip_pm_syscore_suspend(void)
{
	for (; chip_table && chip_table->name; chip_table++)
		rk_gpio_dump(chip_table);

	return 0;
}

static struct syscore_ops rockchip_pm_syscore_ops = {
	.suspend = rockchip_pm_syscore_suspend,
};

static int __init rockchip_pm_syscore_init(void)
{
	if (of_machine_is_compatible("rockchip,rk3502") ||
	    of_machine_is_compatible("rockchip,rk3506"))
		chip_table = rk3506_table;

	if (chip_table)
		register_syscore_ops(&rockchip_pm_syscore_ops);

	return 0;
}

late_initcall(rockchip_pm_syscore_init);
MODULE_DESCRIPTION("Rockchip pm debug");
MODULE_AUTHOR("Rockchip, Inc.");
MODULE_LICENSE("GPL");
