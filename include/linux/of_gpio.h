/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * OF helpers for the GPIO API
 *
 * Copyright (c) 2007-2008  MontaVista Software, Inc.
 *
 * Author: Anton Vorontsov <avorontsov@ru.mvista.com>
 */

#ifndef __LINUX_OF_GPIO_H
#define __LINUX_OF_GPIO_H

#include <linux/compiler.h>
#include <linux/gpio/driver.h>
#include <linux/gpio.h>		/* FIXME: Shouldn't be here */
#include <linux/of.h>

struct device_node;

enum of_gpio_flags {
	OF_GPIO_ACTIVE_LOW = 0x1,
	OF_GPIO_SINGLE_ENDED = 0x2,
	OF_GPIO_OPEN_DRAIN = 0x4,
	OF_GPIO_TRANSITORY = 0x8,
	OF_GPIO_PULL_UP = 0x10,
	OF_GPIO_PULL_DOWN = 0x20,
	OF_GPIO_PULL_DISABLE = 0x40,
};

#ifdef CONFIG_OF_GPIO

extern int of_get_named_gpio(const struct device_node *np,
			     const char *list_name, int index);

static inline int of_get_named_gpio_flags(const struct device_node *np,
					  const char *list_name, int index,
					  enum of_gpio_flags *flags)
{
	struct of_phandle_args gpiospec;
	int ret;

	if (!flags)
		return of_get_named_gpio(np, list_name, index);

	ret = of_parse_phandle_with_args((struct device_node *)np, list_name,
					 "#gpio-cells", index, &gpiospec);
	if (ret)
		return ret;

	*flags = 0;
	if (gpiospec.args_count)
		*flags = gpiospec.args[gpiospec.args_count - 1];
	of_node_put(gpiospec.np);

	return of_get_named_gpio(np, list_name, index);
}

#else /* CONFIG_OF_GPIO */

#include <linux/errno.h>

/* Drivers may not strictly depend on the GPIO support, so let them link. */
static inline int of_get_named_gpio(const struct device_node *np,
                                   const char *propname, int index)
{
	return -ENOSYS;
}

static inline int of_get_named_gpio_flags(const struct device_node *np,
					  const char *list_name, int index,
					  enum of_gpio_flags *flags)
{
	return -ENOSYS;
}

#endif /* CONFIG_OF_GPIO */

#endif /* __LINUX_OF_GPIO_H */
