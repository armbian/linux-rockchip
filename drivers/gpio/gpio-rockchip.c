// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013 MundoReader S.L.
 * Author: Heiko Stuebner <heiko@sntech.de>
 *
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#include <linux/acpi.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/cpuhotplug.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/driver.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/rockchip/rockchip_sip.h>

#include "../pinctrl/core.h"
#include "../pinctrl/pinctrl-rockchip.h"

#define GPIO_TYPE_V1		(0)           /* GPIO Version ID reserved */
#define GPIO_TYPE_V2		(0x01000C2B)  /* GPIO Version ID 0x01000C2B */
#define GPIO_TYPE_V2_1		(0x0101157C)  /* GPIO Version ID 0x0101157C */
#define GPIO_TYPE_V2_2		(0x010219C8)  /* GPIO Version ID 0x010219C8 */
#define GPIO_TYPE_V2_6		(0x010637A6)  /* GPIO Version ID 0x010637A6 */
#define GPIO_TYPE_V2_6_1	(0x01063F6E)  /* GPIO Version ID 0x01063F6E */
#define GPIO_TYPE_V2_6_2	(0x01064899)  /* GPIO Version ID 0x01064899 */
#define GPIO_TYPE_V2_7		(0x01074D2E)  /* GPIO Version ID 0x01074D2E */

#define GPIO_MAX_PINS	(32)

struct rockchip_gpio_irq_affinity {
	int			irq;
	int			cpu;
	int			bank_num;
	struct list_head	list;
};

static DEFINE_MUTEX(irq_affinity_mutex);
static LIST_HEAD(irq_affinity_list);
static bool cpuhp_state_initialized;
static enum cpuhp_state gpio_hp_state = CPUHP_INVALID;

#define ROCKCHIP_PIN_EXP_IRQ_DEMUX(_N) \
static void rockchip_single_pin_exp_irq_demux##_N(struct irq_desc *desc) \
{ \
	struct irq_chip *chip = irq_desc_get_chip(desc); \
	struct rockchip_pin_bank *bank = irq_desc_get_handler_data(desc); \
\
	chained_irq_enter(chip, desc); \
	generic_handle_domain_irq(bank->domain, bank->irq_pin_id[_N][0]); \
	chained_irq_exit(chip, desc); \
} \
\
static void rockchip_dual_pin_exp_irq_demux##_N(struct irq_desc *desc) \
{ \
	struct irq_chip *chip = irq_desc_get_chip(desc); \
	struct rockchip_pin_bank *bank = irq_desc_get_handler_data(desc); \
	u32 value; \
\
	value = readl_relaxed(bank->reg_base + bank->gpio_regs->int_status); \
	chained_irq_enter(chip, desc); \
	if (value & (1 << bank->irq_pin_id[_N][0])) \
		generic_handle_domain_irq(bank->domain, bank->irq_pin_id[_N][0]); \
	if (value & (1 << bank->irq_pin_id[_N][1])) \
		generic_handle_domain_irq(bank->domain, bank->irq_pin_id[_N][1]); \
	chained_irq_exit(chip, desc); \
}

static const struct rockchip_gpio_regs gpio_regs_v1 = {
	.port_dr = 0x00,
	.port_ddr = 0x04,
	.int_en = 0x30,
	.int_mask = 0x34,
	.int_type = 0x38,
	.int_polarity = 0x3c,
	.int_status = 0x40,
	.int_rawstatus = 0x44,
	.debounce = 0x48,
	.port_eoi = 0x4c,
	.ext_port = 0x50,
};

static const struct rockchip_gpio_regs gpio_regs_v2 = {
	.port_dr = 0x00,
	.port_ddr = 0x08,
	.int_en = 0x10,
	.int_mask = 0x18,
	.int_type = 0x20,
	.int_polarity = 0x28,
	.int_bothedge = 0x30,
	.int_status = 0x50,
	.int_rawstatus = 0x58,
	.debounce = 0x38,
	.dbclk_div_en = 0x40,
	.dbclk_div_con = 0x48,
	.port_eoi = 0x60,
	.ext_port = 0x70,
	.version_id = 0x78,
};

static enum rockchip_pinctrl_type chip_type;

static inline bool is_rk3506_bank4(struct rockchip_pin_bank *bank)
{
	return IS_ENABLED(CONFIG_CPU_RK3506) && chip_type == RK3506 && bank->bank_num == 4;
}

static inline void gpio_writel_v2(u32 val, void __iomem *reg)
{
	writel((val & 0xffff) | 0xffff0000, reg);
	writel((val >> 16) | 0xffff0000, reg + 0x4);
}

static inline u32 gpio_readl_v2(void __iomem *reg)
{
	return readl(reg + 0x4) << 16 | readl(reg);
}

static inline void rockchip_gpio_writel(struct rockchip_pin_bank *bank,
					u32 value, unsigned int offset)
{
	void __iomem *reg = bank->reg_base + offset;

	if (is_rk3506_bank4(bank)) {
		u32 tmp = value & 0x3f;

		value &= 0xffffffc0;
		value |= (tmp >> 1) & 0x15;
		value |= (tmp << 1) & 0x2a;
	}

	if (bank->gpio_type >= GPIO_TYPE_V2)
		gpio_writel_v2(value, reg);
	else
		writel(value, reg);
}

static inline u32 rockchip_gpio_readl(struct rockchip_pin_bank *bank,
				      unsigned int offset)
{
	void __iomem *reg = bank->reg_base + offset;
	u32 value;

	if (bank->gpio_type >= GPIO_TYPE_V2)
		value = gpio_readl_v2(reg);
	else
		value = readl(reg);

	if (is_rk3506_bank4(bank)) {
		u32 tmp = value & 0x3f;

		value &= 0xffffffc0;
		value |= (tmp >> 1) & 0x15;
		value |= (tmp << 1) & 0x2a;
	}

	return value;
}

static inline void rockchip_gpio_writel_bit(struct rockchip_pin_bank *bank,
					    u32 bit, u32 value,
					    unsigned int offset)
{
	void __iomem *reg = bank->reg_base + offset;
	u32 data;

	if (is_rk3506_bank4(bank) && bit < 6)
		bit ^= 0x1;

	if (bank->gpio_type >= GPIO_TYPE_V2) {
		if (value)
			data = BIT(bit % 16) | BIT(bit % 16 + 16);
		else
			data = BIT(bit % 16 + 16);
		writel(data, bit >= 16 ? reg + 0x4 : reg);
	} else {
		data = readl(reg);
		data &= ~BIT(bit);
		if (value)
			data |= BIT(bit);
		writel(data, reg);
	}
}

static inline u32 rockchip_gpio_readl_bit(struct rockchip_pin_bank *bank,
					  u32 bit, unsigned int offset)
{
	void __iomem *reg = bank->reg_base + offset;
	u32 data;

	if (is_rk3506_bank4(bank) && bit < 6)
		bit ^= 0x1;

	if (bank->gpio_type >= GPIO_TYPE_V2) {
		data = readl(bit >= 16 ? reg + 0x4 : reg);
		data >>= bit % 16;
	} else {
		data = readl(reg);
		data >>= bit;
	}

	return data & (0x1);
}

static int rockchip_gpio_get_direction(struct gpio_chip *chip,
				       unsigned int offset)
{
	struct rockchip_pin_bank *bank = gpiochip_get_data(chip);
	u32 data;

	data = rockchip_gpio_readl_bit(bank, offset, bank->gpio_regs->port_ddr);
	if (data)
		return GPIO_LINE_DIRECTION_OUT;

	return GPIO_LINE_DIRECTION_IN;
}

static int rockchip_gpio_set_direction(struct gpio_chip *chip,
				       unsigned int offset, bool input)
{
	struct rockchip_pin_bank *bank = gpiochip_get_data(chip);
	unsigned long flags;
	u32 data = input ? 0 : 1;

	raw_spin_lock_irqsave(&bank->slock, flags);
	rockchip_gpio_writel_bit(bank, offset, data, bank->gpio_regs->port_ddr);
	raw_spin_unlock_irqrestore(&bank->slock, flags);

	return 0;
}

static void rockchip_gpio_set(struct gpio_chip *gc, unsigned int offset,
			      int value)
{
	struct rockchip_pin_bank *bank = gpiochip_get_data(gc);
	unsigned long flags;

	raw_spin_lock_irqsave(&bank->slock, flags);
	rockchip_gpio_writel_bit(bank, offset, value, bank->gpio_regs->port_dr);
	raw_spin_unlock_irqrestore(&bank->slock, flags);
}

static int rockchip_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	struct rockchip_pin_bank *bank = gpiochip_get_data(gc);
	u32 data;

	data = readl(bank->reg_base + bank->gpio_regs->ext_port);
	data >>= offset;
	data &= 1;

	return data;
}

static int rockchip_gpio_set_debounce(struct gpio_chip *gc,
				      unsigned int offset,
				      unsigned int debounce)
{
	struct rockchip_pin_bank *bank = gpiochip_get_data(gc);
	const struct rockchip_gpio_regs	*reg = bank->gpio_regs;
	unsigned long flags, div_reg, freq, max_debounce;
	bool div_debounce_support;
	unsigned int cur_div_reg;
	u64 div;

	div_debounce_support = (bank->gpio_type >= GPIO_TYPE_V2) && !IS_ERR(bank->db_clk);
	if (debounce && div_debounce_support) {
		freq = clk_get_rate(bank->db_clk);
		if (!freq)
			return -EINVAL;

		div = (u64)(GENMASK(23, 0) + 1) * 1000000;
		if (bank->gpio_type == GPIO_TYPE_V2)
			max_debounce = DIV_ROUND_CLOSEST_ULL(div, freq);
		else
			max_debounce = DIV_ROUND_CLOSEST_ULL(div, 2 * freq);
		if ((unsigned long)debounce > max_debounce)
			return -EINVAL;

		div = (u64)debounce * freq;
		if (bank->gpio_type == GPIO_TYPE_V2)
			div_reg = DIV_ROUND_CLOSEST_ULL(div, USEC_PER_SEC) - 1;
		else
			div_reg = DIV_ROUND_CLOSEST_ULL(div, USEC_PER_SEC / 2) - 1;
	}

	raw_spin_lock_irqsave(&bank->slock, flags);

	/* Only the v1 needs to configure div_en and div_con for dbclk */
	if (debounce) {
		if (div_debounce_support) {
			/* Configure the max debounce from consumers */
			cur_div_reg = readl(bank->reg_base +
					    reg->dbclk_div_con);
			if (cur_div_reg < div_reg)
				writel(div_reg, bank->reg_base +
				       reg->dbclk_div_con);
			rockchip_gpio_writel_bit(bank, offset, 1,
						 reg->dbclk_div_en);
		}

		rockchip_gpio_writel_bit(bank, offset, 1, reg->debounce);
	} else {
		if (div_debounce_support)
			rockchip_gpio_writel_bit(bank, offset, 0,
						 reg->dbclk_div_en);

		rockchip_gpio_writel_bit(bank, offset, 0, reg->debounce);
	}

	raw_spin_unlock_irqrestore(&bank->slock, flags);

	/* Enable or disable dbclk at last */
	if (div_debounce_support) {
		raw_spin_lock_irqsave(&bank->slock, flags);

		if (debounce) {
			if (bitmap_empty(bank->db_clk_bitmap, RK_GPIO_BANK_MAX_PIN)) {
				int ret = clk_enable(bank->db_clk);

				if (ret)
					dev_warn(bank->dev, "Failed to enable db_clk: %d\n", ret);
				else
					set_bit(offset, bank->db_clk_bitmap);
			} else {
				set_bit(offset, bank->db_clk_bitmap);
			}
		} else {
			if (!bitmap_empty(bank->db_clk_bitmap, RK_GPIO_BANK_MAX_PIN)) {
				clear_bit(offset, bank->db_clk_bitmap);
				if (bitmap_empty(bank->db_clk_bitmap, RK_GPIO_BANK_MAX_PIN))
					clk_disable(bank->db_clk);
			}
		}

		raw_spin_unlock_irqrestore(&bank->slock, flags);
	} else {
		return -EOPNOTSUPP;
	}

	return 0;
}

static int rockchip_gpio_direction_input(struct gpio_chip *gc,
					 unsigned int offset)
{
	return rockchip_gpio_set_direction(gc, offset, true);
}

static int rockchip_gpio_direction_output(struct gpio_chip *gc,
					  unsigned int offset, int value)
{
	rockchip_gpio_set(gc, offset, value);

	return rockchip_gpio_set_direction(gc, offset, false);
}

/*
 * gpiolib set_config callback function. The setting of the pin
 * mux function as 'gpio output' will be handled by the pinctrl subsystem
 * interface.
 */
static int rockchip_gpio_set_config(struct gpio_chip *gc, unsigned int offset,
				  unsigned long config)
{
	enum pin_config_param param = pinconf_to_config_param(config);

	switch (param) {
	case PIN_CONFIG_INPUT_DEBOUNCE:
		rockchip_gpio_set_debounce(gc, offset, 0);
		/*
		 * Since Rockchip's GPIO hardware debounce function does not
		 * support configuring individual pins, it will not be used.
		 */
		return -ENOTSUPP;
	default:
		return -ENOTSUPP;
	}
}

/*
 * gpiod_to_irq() callback function. Creates a mapping between a GPIO pin
 * and a virtual IRQ, if not already present.
 */
static int rockchip_gpio_to_irq(struct gpio_chip *gc, unsigned int offset)
{
	struct rockchip_pin_bank *bank = gpiochip_get_data(gc);
	unsigned int virq;
	int i;
	struct rockchip_gpio_irq_affinity *irq_affinity;

	if (!bank->domain)
		return -ENXIO;

	virq = irq_create_mapping(bank->domain, offset);

	for (i = 0; i < RK_GPIO_IRQ_MAX_NUM; i++) {
		if (bank->irq_pins[i] & (1 << offset)) {
			irq_affinity = kzalloc(sizeof(*irq_affinity), GFP_KERNEL);
			if (!irq_affinity)
				return -ENOMEM;
			irq_affinity->irq = virq;
			irq_affinity->cpu = bank->cpu_affinity[i];
			irq_affinity->bank_num = bank->bank_num;
			mutex_lock(&irq_affinity_mutex);
			list_add(&irq_affinity->list, &irq_affinity_list);
			mutex_unlock(&irq_affinity_mutex);
			irq_force_affinity(virq, cpumask_of(irq_affinity->cpu));
			break;
		}
	}

	return (virq) ? : -ENXIO;
}

static const struct gpio_chip rockchip_gpiolib_chip = {
	.request = gpiochip_generic_request,
	.free = gpiochip_generic_free,
	.set = rockchip_gpio_set,
	.get = rockchip_gpio_get,
	.get_direction	= rockchip_gpio_get_direction,
	.direction_input = rockchip_gpio_direction_input,
	.direction_output = rockchip_gpio_direction_output,
	.set_config = rockchip_gpio_set_config,
	.to_irq = rockchip_gpio_to_irq,
	.owner = THIS_MODULE,
};

static void rockchip_irq_demux(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct rockchip_pin_bank *bank = irq_desc_get_handler_data(desc);
	unsigned long pending;
	unsigned int irq;

	dev_dbg(bank->dev, "got irq for bank %s\n", bank->name);

	chained_irq_enter(chip, desc);

	pending = readl_relaxed(bank->reg_base + bank->gpio_regs->int_status);
	for_each_set_bit(irq, &pending, 32) {
		dev_dbg(bank->dev, "handling irq %d\n", irq);

		/*
		 * Triggering IRQ on both rising and falling edge
		 * needs manual intervention.
		 */
		if (bank->toggle_edge_mode & BIT(irq)) {
			u32 data, data_old, polarity;
			unsigned long flags;

			data = readl_relaxed(bank->reg_base +
					     bank->gpio_regs->ext_port);
			do {
				raw_spin_lock_irqsave(&bank->slock, flags);

				polarity = readl_relaxed(bank->reg_base +
							 bank->gpio_regs->int_polarity);
				if (data & BIT(irq))
					polarity &= ~BIT(irq);
				else
					polarity |= BIT(irq);
				writel(polarity,
				       bank->reg_base +
				       bank->gpio_regs->int_polarity);

				raw_spin_unlock_irqrestore(&bank->slock, flags);

				data_old = data;
				data = readl_relaxed(bank->reg_base +
						     bank->gpio_regs->ext_port);
			} while ((data & BIT(irq)) != (data_old & BIT(irq)));
		}

		generic_handle_domain_irq(bank->domain, irq);
	}

	chained_irq_exit(chip, desc);
}

ROCKCHIP_PIN_EXP_IRQ_DEMUX(1);
ROCKCHIP_PIN_EXP_IRQ_DEMUX(2);
ROCKCHIP_PIN_EXP_IRQ_DEMUX(3);

static irq_flow_handler_t rockchip_irq_s[RK_GPIO_IRQ_MAX_NUM - 1] = {
	rockchip_single_pin_exp_irq_demux1,
	rockchip_single_pin_exp_irq_demux2,
	rockchip_single_pin_exp_irq_demux3
};

static irq_flow_handler_t rockchip_irq_d[RK_GPIO_IRQ_MAX_NUM - 1] = {
	rockchip_dual_pin_exp_irq_demux1,
	rockchip_dual_pin_exp_irq_demux2,
	rockchip_dual_pin_exp_irq_demux3
};

static int rockchip_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct rockchip_pin_bank *bank = gc->private;
	u32 mask = BIT(d->hwirq);
	u32 polarity;
	u32 level;
	u32 data;
	unsigned long flags;
	int ret = 0;

	raw_spin_lock_irqsave(&bank->slock, flags);

	rockchip_gpio_writel_bit(bank, d->hwirq, 0,
				 bank->gpio_regs->port_ddr);

	raw_spin_unlock_irqrestore(&bank->slock, flags);

	if (type & IRQ_TYPE_EDGE_BOTH)
		irq_set_handler_locked(d, handle_edge_irq);
	else
		irq_set_handler_locked(d, handle_level_irq);

	raw_spin_lock_irqsave(&bank->slock, flags);

	level = rockchip_gpio_readl(bank, bank->gpio_regs->int_type);
	polarity = rockchip_gpio_readl(bank, bank->gpio_regs->int_polarity);

	if (type == IRQ_TYPE_EDGE_BOTH) {
		if (bank->gpio_type >= GPIO_TYPE_V2) {
			rockchip_gpio_writel_bit(bank, d->hwirq, 1,
						 bank->gpio_regs->int_bothedge);
			goto out;
		} else {
			bank->toggle_edge_mode |= mask;
			level &= ~mask;

			/*
			 * Determine gpio state. If 1 next interrupt should be
			 * low otherwise high.
			 */
			data = readl(bank->reg_base + bank->gpio_regs->ext_port);
			if (data & mask)
				polarity &= ~mask;
			else
				polarity |= mask;
		}
	} else {
		if (bank->gpio_type >= GPIO_TYPE_V2) {
			rockchip_gpio_writel_bit(bank, d->hwirq, 0,
						 bank->gpio_regs->int_bothedge);
		} else {
			bank->toggle_edge_mode &= ~mask;
		}
		switch (type) {
		case IRQ_TYPE_EDGE_RISING:
			level |= mask;
			polarity |= mask;
			break;
		case IRQ_TYPE_EDGE_FALLING:
			level |= mask;
			polarity &= ~mask;
			break;
		case IRQ_TYPE_LEVEL_HIGH:
			level &= ~mask;
			polarity |= mask;
			break;
		case IRQ_TYPE_LEVEL_LOW:
			level &= ~mask;
			polarity &= ~mask;
			break;
		default:
			ret = -EINVAL;
			goto out;
		}
	}

	rockchip_gpio_writel(bank, level, bank->gpio_regs->int_type);
	rockchip_gpio_writel(bank, polarity, bank->gpio_regs->int_polarity);
out:
	raw_spin_unlock_irqrestore(&bank->slock, flags);

	return ret;
}

static int rockchip_irq_reqres(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct rockchip_pin_bank *bank = gc->private;

	rockchip_gpio_direction_input(&bank->gpio_chip, d->hwirq);

	return gpiochip_reqres_irq(&bank->gpio_chip, d->hwirq);
}

static void rockchip_irq_relres(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct rockchip_pin_bank *bank = gc->private;

	gpiochip_relres_irq(&bank->gpio_chip, d->hwirq);
}

static void rockchip_irq_suspend(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct rockchip_pin_bank *bank = gc->private;

	bank->saved_masks = irq_reg_readl(gc, bank->gpio_regs->int_mask);
	irq_reg_writel(gc, ~gc->wake_active, bank->gpio_regs->int_mask);
}

static void rockchip_irq_resume(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct rockchip_pin_bank *bank = gc->private;

	irq_reg_writel(gc, bank->saved_masks, bank->gpio_regs->int_mask);
}

static void rockchip_irq_enable(struct irq_data *d)
{
	irq_gc_mask_clr_bit(d);
}

static void rockchip_irq_disable(struct irq_data *d)
{
	irq_gc_mask_set_bit(d);
}

static int rockchip_irq_set_affinity(struct irq_data *data,
				     const struct cpumask *mask_val, bool force)
{
	unsigned int cpu;

	if (!force)
		cpu = cpumask_any_and(mask_val, cpu_online_mask);
	else
		cpu = cpumask_first(mask_val);

	if (cpu >= nr_cpu_ids)
		return -EINVAL;

	irq_data_update_effective_affinity(data, cpumask_of(cpu));

	return IRQ_SET_MASK_OK;
}

static int rockchip_interrupts_register(struct rockchip_pin_bank *bank)
{
	unsigned int clr = IRQ_NOREQUEST | IRQ_NOPROBE | IRQ_NOAUTOEN;
	struct irq_chip_generic *gc;
	int ret;

	bank->domain = irq_domain_create_linear(dev_fwnode(bank->dev), 32,
					&irq_generic_chip_ops, NULL);
	if (!bank->domain) {
		dev_warn(bank->dev, "could not init irq domain for bank %s\n",
			 bank->name);
		return -EINVAL;
	}

	ret = irq_alloc_domain_generic_chips(bank->domain, 32, 1,
					     "rockchip_gpio_irq",
					     handle_level_irq,
					     clr, 0, 0);
	if (ret) {
		dev_err(bank->dev, "could not alloc generic chips for bank %s\n",
			bank->name);
		irq_domain_remove(bank->domain);
		return -EINVAL;
	}

	gc = irq_get_domain_generic_chip(bank->domain, 0);
	if (bank->gpio_type >= GPIO_TYPE_V2) {
		gc->reg_writel = gpio_writel_v2;
		gc->reg_readl = gpio_readl_v2;
	}

	gc->reg_base = bank->reg_base;
	gc->private = bank;
	gc->chip_types[0].regs.mask = bank->gpio_regs->int_mask;
	gc->chip_types[0].regs.ack = bank->gpio_regs->port_eoi;
	gc->chip_types[0].chip.irq_ack = irq_gc_ack_set_bit;
	gc->chip_types[0].chip.irq_mask = irq_gc_mask_set_bit;
	gc->chip_types[0].chip.irq_unmask = irq_gc_mask_clr_bit;
	gc->chip_types[0].chip.irq_enable = rockchip_irq_enable;
	gc->chip_types[0].chip.irq_disable = rockchip_irq_disable;
	gc->chip_types[0].chip.irq_set_wake = irq_gc_set_wake;
	gc->chip_types[0].chip.irq_suspend = rockchip_irq_suspend;
	gc->chip_types[0].chip.irq_resume = rockchip_irq_resume;
	gc->chip_types[0].chip.irq_set_type = rockchip_irq_set_type;
	gc->chip_types[0].chip.irq_request_resources = rockchip_irq_reqres;
	gc->chip_types[0].chip.irq_release_resources = rockchip_irq_relres;
	gc->wake_enabled = IRQ_MSK(bank->nr_pins);

	gc->chip_types[0].chip.irq_set_affinity = rockchip_irq_set_affinity;
	/*
	 * Linux assumes that all interrupts start out disabled/masked.
	 * Our driver only uses the concept of masked and always keeps
	 * things enabled, so for us that's all masked and all enabled.
	 */
	rockchip_gpio_writel(bank, 0xffffffff, bank->gpio_regs->int_mask);
	rockchip_gpio_writel(bank, 0xffffffff, bank->gpio_regs->port_eoi);
	rockchip_gpio_writel(bank, 0xffffffff, bank->gpio_regs->int_en);
	gc->mask_cache = 0xffffffff;

	irq_set_chained_handler_and_data(bank->irq[0],
					 rockchip_irq_demux, bank);

	for (int i = 1; i < RK_GPIO_IRQ_MAX_NUM; i++) {
		int pin_num = hweight32(bank->irq_pins[i]);

		if (!bank->irq_pins[i])
			continue;
		if (pin_num == 1)
			irq_set_chained_handler_and_data(bank->irq[i],
							 rockchip_irq_s[i - 1],
							 bank);
		else if (pin_num == 2)
			irq_set_chained_handler_and_data(bank->irq[i],
							 rockchip_irq_d[i - 1],
							 bank);
		else
			irq_set_chained_handler_and_data(bank->irq[i],
							 rockchip_irq_demux,
							 bank);
	}
	return 0;
}

static int rockchip_gpiolib_register(struct rockchip_pin_bank *bank)
{
	struct gpio_chip *gc;
	int ret;

	bank->gpio_chip = rockchip_gpiolib_chip;

	gc = &bank->gpio_chip;
	if (IS_ENABLED(CONFIG_GPIO_SYSFS))
		gc->base = bank->pin_base;
	else
		gc->base = -1; /* Dynamic allocation */
	gc->ngpio = bank->nr_pins;
	gc->label = bank->name;
	gc->parent = bank->dev;

	if (!gc->base)
		gc->base = GPIO_MAX_PINS * bank->bank_num;
	if (!gc->ngpio)
		gc->ngpio = GPIO_MAX_PINS;
	if (!gc->label) {
		gc->label = kasprintf(GFP_KERNEL, "gpio%d", bank->bank_num);
		if (!gc->label)
			return -ENOMEM;
	}

	ret = gpiochip_add_data(gc, bank);
	if (ret) {
		dev_err(bank->dev, "failed to add gpiochip %s, %d\n",
			gc->label, ret);
		return ret;
	}

	ret = rockchip_interrupts_register(bank);
	if (ret) {
		dev_err(bank->dev, "failed to register interrupt, %d\n", ret);
		goto fail;
	}

	return 0;

fail:
	gpiochip_remove(&bank->gpio_chip);

	return ret;
}

static void rockchip_gpio_get_ver(struct rockchip_pin_bank *bank)
{
	int id = readl(bank->reg_base + gpio_regs_v2.version_id);

	switch (id) {
	case GPIO_TYPE_V2:
	case GPIO_TYPE_V2_1:
		bank->gpio_regs = &gpio_regs_v2;
		bank->gpio_type = GPIO_TYPE_V2;
		break;
	case GPIO_TYPE_V2_2:
		bank->gpio_regs = &gpio_regs_v2;
		bank->gpio_type = GPIO_TYPE_V2_2;
		break;
	case GPIO_TYPE_V2_6:
	case GPIO_TYPE_V2_6_1:
	case GPIO_TYPE_V2_6_2:
		bank->gpio_regs = &gpio_regs_v2;
		bank->gpio_type = GPIO_TYPE_V2_6;
		break;
	case GPIO_TYPE_V2_7:
		bank->gpio_regs = &gpio_regs_v2;
		bank->gpio_type = GPIO_TYPE_V2_7;
		break;
	default:
		bank->gpio_regs = &gpio_regs_v1;
		bank->gpio_type = GPIO_TYPE_V1;
		pr_info("Note: Use default GPIO_TYPE_V1!\n");
	}
}

static struct rockchip_pin_bank *
rockchip_gpio_find_bank(struct pinctrl_dev *pctldev, int id)
{
	struct rockchip_pinctrl *info;
	struct rockchip_pin_bank *bank;
	int i, found = 0;

	info = pinctrl_dev_get_drvdata(pctldev);
	if (IS_ENABLED(CONFIG_CPU_RK3506))
		chip_type = info->ctrl->type;

	bank = info->ctrl->pin_banks;
	for (i = 0; i < info->ctrl->nr_banks; i++, bank++) {
		if (bank->bank_num == id) {
			found = 1;
			break;
		}
	}

	return found ? bank : NULL;
}

static int rockchip_gpio_of_get_bank_id(struct device *dev)
{
	static int gpio;
	int bank_id = -1;

	if (IS_ENABLED(CONFIG_OF) && dev->of_node) {
		bank_id = of_alias_get_id(dev->of_node, "gpio");
		if (bank_id < 0)
			bank_id = gpio++;
	}

	return bank_id;
}

#ifdef CONFIG_ACPI
static int rockchip_gpio_acpi_get_bank_id(struct device *dev)
{
	struct acpi_device *adev;
	unsigned long bank_id = -1;
	const char *uid;
	int ret;

	adev = ACPI_COMPANION(dev);
	if (!adev)
		return -ENXIO;

	uid = acpi_device_uid(adev);
	if (!uid || !(*uid)) {
		dev_err(dev, "Cannot retrieve UID\n");
		return -ENODEV;
	}

	ret = kstrtoul(uid, 0, &bank_id);

	return !ret ? bank_id : -ERANGE;
}
#else
static int rockchip_gpio_acpi_get_bank_id(struct device *dev)
{
	return -ENOENT;
}
#endif /* CONFIG_ACPI */

static bool rockchip_gpio_has_irq_affinity(struct device_node *node)
{
	return !!of_find_property(node, "interrupt-affinity", NULL);
}

static bool rockchip_gpio_has_irq_pins(struct device_node *node)
{
	return !!of_find_property(node, "interrupt-pins", NULL);
}

static int rockchip_gpio_set_irq_affinity(int irq, int cpu)
{
	int ret;

	ret = irq_force_affinity(irq, cpumask_of(cpu));
	if (ret) {
		pr_err("unable to set irq affinity (irq=%d, cpu=%u)\n", irq, cpu);
		return ret;
	}

	return 0;
}

static int rockchip_gpio_init_irq_affinity(const struct device *dev, int index,
					   struct rockchip_pin_bank *bank)
{
	int cpu, ret;
	struct device_node *node = dev->of_node, *dn;
	struct rockchip_gpio_irq_affinity *irq_affinity;

	if (!bank->irq_pins[index])
		return 0;

	dn = of_parse_phandle(node, "interrupt-affinity", index);
	if (!dn) {
		dev_err(dev, "failed to parse interrupt-affinity[%d]\n", index);
		return -EINVAL;
	}

	cpu = of_cpu_node_to_id(dn);
	of_node_put(dn);
	if (cpu < 0)
		return cpu;

	if (cpu == 0)
		return 0;

	ret = rockchip_gpio_set_irq_affinity(bank->irq[index], cpu);
	if (ret)
		return ret;

	bank->cpu_affinity[index] = cpu;

	irq_affinity = kzalloc(sizeof(*irq_affinity), GFP_KERNEL);
	if (!irq_affinity)
		return -ENOMEM;
	irq_affinity->irq = bank->irq[index];
	irq_affinity->cpu = cpu;
	irq_affinity->bank_num = bank->bank_num;
	mutex_lock(&irq_affinity_mutex);
	list_add(&irq_affinity->list, &irq_affinity_list);
	mutex_unlock(&irq_affinity_mutex);

	return 0;
}

static int rockchip_gpio_set_irq_pins(const struct device *dev, int index,
				      struct rockchip_pin_bank *bank)
{
	struct arm_smccc_res res;
	int ret = 0;

	if (rockchip_gpio_has_irq_pins(dev->of_node)) {
		ret = of_property_read_u32_index(dev->of_node, "interrupt-pins",
						 index, &bank->irq_pins[index]);
		if (ret) {
			dev_err(dev, "Failed to read interrupt-pin at index %d\n", index);
			return ret;
		}

		if (!bank->irq_pins[index])
			return 0;
		res = sip_smc_gpio_config(GPIO_SET_GROUP_INFO, bank->bank_num,
					  index, bank->irq_pins[index]);

		switch (res.a0) {
		case SIP_RET_SUCCESS:
			return 0;
		case SIP_RET_NOT_SUPPORTED:
			dev_err(dev, "Failed to set interrupt pins, no support\n");
			return -EOPNOTSUPP;
		case SIP_RET_INVALID_PARAMS:
			dev_err(dev, "gpio id or group id invalid\n");
			return -EINVAL;
		case SIP_RET_DENIED:
			dev_warn(dev, "Failed to set interrupt pins, please modify syscfg.dts\n");
			break;
		default:
			break;
		}
	}
	res = sip_smc_gpio_config(GPIO_GET_GROUP_INFO, bank->bank_num, index, 0);
	if (res.a0 == 0)
		bank->irq_pins[index] = res.a1;

	return 0;
}

static int rockchip_gpio_init_irq_pins(const struct device *dev, int index,
				       struct rockchip_pin_bank *bank)
{
	int i;
	unsigned int pin;
	unsigned long pending;

	if (index == 0)
		return 0;

	for (i = 0; i < RK_GPIO_EXP_IRQ_MAX_PIN_NUM; i++)
		bank->irq_pin_id[index][i] = -1;
	rockchip_gpio_set_irq_pins(dev, index, bank);

	if (bank->irq_pins[index]) {
		i = 0;
		pending = bank->irq_pins[index];
		for_each_set_bit(pin, &pending, 32) {
			bank->irq_pin_id[index][i++] = pin;
		}
	}

	return 0;
}

static int rockchip_gpio_parse_irqs(struct platform_device *pdev,
				    struct rockchip_pin_bank *bank)
{
	int num_irqs, i, ret;
	struct device *dev = &pdev->dev;
	bool has_affinity = rockchip_gpio_has_irq_affinity(dev->of_node);

	num_irqs = platform_irq_count(pdev);
	if (num_irqs < 0)
		return dev_err_probe(dev, num_irqs, "unable to count GPIO IRQs\n");
	else if (num_irqs == 0)
		return dev_err_probe(dev, -EINVAL, "no available GPIO IRQs found\n");
	else if (num_irqs > RK_GPIO_IRQ_MAX_NUM)
		return dev_err_probe(dev, -EINVAL, "GPIO IRQs number must not more than %d\n",
				     RK_GPIO_IRQ_MAX_NUM);

	for (i = 0; i < num_irqs; i++) {
		bank->irq[i] = platform_get_irq(pdev, i);
		if (bank->irq[i] < 0)
			return dev_err_probe(dev, -EINVAL, "failed to get gpio irq %d\n", i);

		if (!has_affinity)
			continue;

		ret = rockchip_gpio_init_irq_pins(dev, i, bank);
		if (ret)
			return ret;
		ret = rockchip_gpio_init_irq_affinity(dev, i, bank);
		if (ret)
			return ret;
	}

	return 0;
}

static int gpio_cpu_on(unsigned int cpu)
{
	struct rockchip_gpio_irq_affinity *entry;

	mutex_lock(&irq_affinity_mutex);
	list_for_each_entry(entry, &irq_affinity_list, list) {
		if (entry->cpu == cpu)
			rockchip_gpio_set_irq_affinity(entry->irq, cpu);
	}
	mutex_unlock(&irq_affinity_mutex);

	return 0;
}

static int gpio_cpu_off(unsigned int cpu)
{
	return 0;
}

static void rockchip_gpio_init_cpuhp(void)
{
	int ret;

	mutex_lock(&irq_affinity_mutex);
	if (!cpuhp_state_initialized && !list_empty(&irq_affinity_list)) {
		ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "gpio",
						gpio_cpu_on, gpio_cpu_off);
		if (ret < 0) {
			pr_err("ERROR: gpio failed to register hotplug callbacks\n");
			mutex_unlock(&irq_affinity_mutex);
			return;
		}
		cpuhp_state_initialized = true;
		gpio_hp_state = ret;
	}

	mutex_unlock(&irq_affinity_mutex);
}

static void rockchip_gpio_remove_cpuhp(void)
{
	mutex_lock(&irq_affinity_mutex);
	if (cpuhp_state_initialized && list_empty(&irq_affinity_list)) {
		if (gpio_hp_state != CPUHP_INVALID) {
			cpuhp_remove_state_nocalls(gpio_hp_state);
			gpio_hp_state = CPUHP_INVALID;
			cpuhp_state_initialized = false;
		}
	}
	mutex_unlock(&irq_affinity_mutex);
}

static int rockchip_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pinctrl_dev *pctldev = NULL;
	struct rockchip_pin_bank *bank = NULL;
	int bank_id = 0;
	int ret;
	struct rockchip_gpio_irq_affinity *entry, *tmp;

	bank_id = rockchip_gpio_acpi_get_bank_id(dev);
	if (bank_id < 0) {
		bank_id = rockchip_gpio_of_get_bank_id(dev);
		if (bank_id < 0)
			return bank_id;
	}

	if (!ACPI_COMPANION(dev)) {
		struct device_node *pctlnp = of_get_parent(dev->of_node);

		pctldev = of_pinctrl_get(pctlnp);
		of_node_put(pctlnp);
		if (!pctldev)
			return -EPROBE_DEFER;

		bank = rockchip_gpio_find_bank(pctldev, bank_id);
		if (!bank)
			return -ENODEV;
	}

	if (!bank) {
		bank = devm_kzalloc(dev, sizeof(*bank), GFP_KERNEL);
		if (!bank)
			return -ENOMEM;
	}

	bank->bank_num = bank_id;
	bank->dev = dev;

	bank->reg_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(bank->reg_base))
		return PTR_ERR(bank->reg_base);

	rockchip_gpio_get_ver(bank);

	raw_spin_lock_init(&bank->slock);

	if (!ACPI_COMPANION(dev)) {
		bank->clk = devm_clk_get(dev, "bus");
		if (IS_ERR(bank->clk)) {
			bank->clk = of_clk_get(dev->of_node, 0);
			if (IS_ERR(bank->clk)) {
				dev_err(dev, "fail to get apb clock\n");
				return PTR_ERR(bank->clk);
			}
		}

		bank->db_clk = devm_clk_get(dev, "db");
		if (IS_ERR(bank->db_clk)) {
			bank->db_clk = of_clk_get(dev->of_node, 1);
			if (IS_ERR(bank->db_clk))
				bank->db_clk = NULL;
		}
	}

	ret = clk_prepare_enable(bank->clk);
	if (ret) {
		dev_err(bank->dev, "Failed to enable GPIO clock: %d\n", ret);
		return ret;
	}
	ret = clk_prepare(bank->db_clk);
	if (ret) {
		dev_warn(bank->dev, "Failed to prepare db_clk: %d\n", ret);
		bank->db_clk = NULL;
	}

	ret = rockchip_gpio_parse_irqs(pdev, bank);
	if (ret < 0)
		goto err_clk;
	rockchip_gpio_init_cpuhp();

	/*
	 * Prevent clashes with a deferred output setting
	 * being added right at this moment.
	 */
	mutex_lock(&bank->deferred_lock);

	ret = rockchip_gpiolib_register(bank);
	if (ret) {
		dev_err(bank->dev, "Failed to register gpio %d\n", ret);
		goto err_unlock;
	}

	if (!device_property_read_bool(bank->dev, "gpio-ranges") && pctldev) {
		struct gpio_chip *gc = &bank->gpio_chip;

		ret = gpiochip_add_pin_range(gc, dev_name(pctldev->dev), 0,
					     gc->base, gc->ngpio);
		if (ret) {
			dev_err(bank->dev, "Failed to add pin range\n");
			goto err_unlock;
		}
	}

	while (!list_empty(&bank->deferred_pins)) {
		struct rockchip_pin_deferred *cfg;

		cfg = list_first_entry(&bank->deferred_pins,
				       struct rockchip_pin_deferred, head);
		if (!cfg)
			break;

		list_del(&cfg->head);

		switch (cfg->param) {
		case PIN_CONFIG_OUTPUT:
			ret = rockchip_gpio_direction_output(&bank->gpio_chip, cfg->pin, cfg->arg);
			if (ret)
				dev_warn(dev, "setting output pin %u to %u failed\n", cfg->pin,
					 cfg->arg);
			break;
		case PIN_CONFIG_INPUT_ENABLE:
			ret = rockchip_gpio_direction_input(&bank->gpio_chip, cfg->pin);
			if (ret)
				dev_warn(dev, "setting input pin %u failed\n", cfg->pin);
			break;
		default:
			dev_warn(dev, "unknown deferred config param %d\n", cfg->param);
			break;
		}
		kfree(cfg);
	}

	mutex_unlock(&bank->deferred_lock);

	platform_set_drvdata(pdev, bank);
	dev_info(dev, "probed %pfw\n", dev_fwnode(dev));

	return 0;
err_unlock:
	mutex_lock(&irq_affinity_mutex);
	list_for_each_entry_safe(entry, tmp, &irq_affinity_list, list) {
		if (entry->bank_num == bank->bank_num) {
			list_del(&entry->list);
			kfree(entry);
		}
	}
	mutex_unlock(&irq_affinity_mutex);
	rockchip_gpio_remove_cpuhp();

	mutex_unlock(&bank->deferred_lock);
err_clk:
	clk_disable_unprepare(bank->clk);
	clk_unprepare(bank->db_clk);

	return ret;
}

static void rockchip_gpio_remove(struct platform_device *pdev)
{
	struct rockchip_pin_bank *bank = platform_get_drvdata(pdev);
	struct rockchip_gpio_irq_affinity *entry, *tmp;

	mutex_lock(&irq_affinity_mutex);
	list_for_each_entry_safe(entry, tmp, &irq_affinity_list, list) {
		if (entry->bank_num == bank->bank_num) {
			list_del(&entry->list);
			kfree(entry);
		}
	}
	mutex_unlock(&irq_affinity_mutex);
	rockchip_gpio_remove_cpuhp();

	clk_disable_unprepare(bank->clk);
	if (bitmap_empty(bank->db_clk_bitmap, RK_GPIO_BANK_MAX_PIN))
		clk_unprepare(bank->db_clk);
	else
		clk_disable_unprepare(bank->db_clk);
	gpiochip_remove(&bank->gpio_chip);
}

static const struct of_device_id rockchip_gpio_match[] = {
	{ .compatible = "rockchip,gpio-bank", },
	{ .compatible = "rockchip,rk3188-gpio-bank0" },
	{ },
};

static struct platform_driver rockchip_gpio_driver = {
	.probe		= rockchip_gpio_probe,
	.remove_new	= rockchip_gpio_remove,
	.driver		= {
		.name	= "rockchip-gpio",
		.of_match_table = rockchip_gpio_match,
	},
};

static int __init rockchip_gpio_init(void)
{
	return platform_driver_register(&rockchip_gpio_driver);
}
postcore_initcall(rockchip_gpio_init);

static void __exit rockchip_gpio_exit(void)
{
	platform_driver_unregister(&rockchip_gpio_driver);
}
module_exit(rockchip_gpio_exit);

MODULE_DESCRIPTION("Rockchip gpio driver");
MODULE_ALIAS("platform:rockchip-gpio");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, rockchip_gpio_match);
