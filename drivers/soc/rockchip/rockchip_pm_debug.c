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

#define RK_SUB_REG_RGN(OFF, LEN)		\
	{					\
		.offset = OFF,			\
		.len = LEN,			\
	}

#define RK_REG_RGN(NAME, REG, LEN, ID, TABLE)	\
	{					\
		.name = NAME,			\
		.reg_base = REG,		\
		.len = LEN,			\
		.bank_id = ID,			\
		.table = TABLE,			\
	}

#define RK_REG_CFG_SIMPLE(NAME, REG, LEN)	\
		RK_REG_RGN(NAME, REG, LEN, 0, NULL)

struct rk_sub_rgn {
	u32 offset;
	u32 len;
};

struct rk_reg_rgn {
	char *name;
	u32 reg_base;
	u32 len;
	int bank_id;
	const struct rk_sub_rgn *table;
};

static const struct rk_sub_rgn rk3506_gpio_table[] = {
	RK_SUB_REG_RGN(0, 0x20),
	RK_SUB_REG_RGN(0x100, 0x40),
	RK_SUB_REG_RGN(0x200, 0x10),
	RK_SUB_REG_RGN(0x300, 0x10),
	RK_SUB_REG_RGN(0x400, 0x10),
	RK_SUB_REG_RGN(0x500, 0x10),
	RK_SUB_REG_RGN(0x600, 0x10),
	{ },
};

static const struct rk_reg_rgn rk3506_table[] = {
	RK_REG_RGN("gpio0_ioc", 0xff950000, 0x700, 0, rk3506_gpio_table),
	RK_REG_RGN("gpio1_ioc", 0xff660000, 0x700, 1, rk3506_gpio_table),
	RK_REG_RGN("gpio2_ioc", 0xff4d8000, 0x700, 2, rk3506_gpio_table),
	RK_REG_RGN("gpio3_ioc", 0xff4d8000, 0x700, 3, rk3506_gpio_table),
	RK_REG_CFG_SIMPLE("gpio4_ioc", 0xff4d8840, 0x10),
	RK_REG_CFG_SIMPLE("rm_io", 0xff910080, 0x80),
	RK_REG_CFG_SIMPLE("gpio0", 0xff940000, 0x80),
	RK_REG_CFG_SIMPLE("gpio1", 0xff870000, 0x80),
	RK_REG_CFG_SIMPLE("gpio2", 0xff1c0000, 0x80),
	RK_REG_CFG_SIMPLE("gpio3", 0xff1d0000, 0x80),
	RK_REG_CFG_SIMPLE("gpio4", 0xff1e0000, 0x80),
	{ },
};

static const struct rk_reg_rgn *chip_table;

static void rk_regs_dump(const struct rk_reg_rgn *chip)
{
	const struct rk_sub_rgn *table = chip->table;
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
		rk_regs_dump(chip_table);

	return 0;
}

static struct syscore_ops rockchip_pm_syscore_ops = {
	.suspend = rockchip_pm_syscore_suspend,
};

static const struct chip_data_t {
	const char *compat;
	const struct rk_reg_rgn *table;
} compat_list[] = {
#ifdef CONFIG_CPU_RK3506
	{ .compat = "rockchip,rk3502", .table = rk3506_table },
	{ .compat = "rockchip,rk3506", .table = rk3506_table },
#endif
};

static int __init rockchip_pm_syscore_init(void)
{
	int i;

	if (ARRAY_SIZE(compat_list) > 0) {
		for (i = 0; i < ARRAY_SIZE(compat_list); i++) {
			if (of_machine_is_compatible(compat_list[i].compat)) {
				chip_table = compat_list[i].table;
				break;
			}
		}

		if (chip_table)
			register_syscore_ops(&rockchip_pm_syscore_ops);
	}

	return 0;
}

late_initcall(rockchip_pm_syscore_init);
MODULE_DESCRIPTION("Rockchip pm debug");
MODULE_AUTHOR("Rockchip, Inc.");
MODULE_LICENSE("GPL");
