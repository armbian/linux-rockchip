/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * GPIO based PCI Hotplug Driver.
 *
 */

#ifndef _GPIOPHP_H
#define _GPIOPHP_H

#include <linux/pci_hotplug.h>

struct gpio_hotplug_slot;

struct gpio_hotplug_slot_plat_ops {
	int (*enable)(struct gpio_hotplug_slot *hp_slot);
	int (*disable)(struct gpio_hotplug_slot *hp_slot);
};

struct gpio_hotplug_slot {
	struct device_node *np;
	int slot_nr;
	const struct gpio_hotplug_slot_plat_ops *plat_ops;
	struct pci_dev *pdev;

	struct gpio_desc *gpiod;
	unsigned int irq;

	struct hotplug_slot hotplug_slot;
};

#ifdef CONFIG_HOTPLUG_PCI_GPIO
int register_gpio_hotplug_slot(struct gpio_hotplug_slot *hp_slot);
void unregister_gpio_hotplug_slot(struct gpio_hotplug_slot *hp_slot);
#else
static inline int register_gpio_hotplug_slot(struct gpio_hotplug_slot *hp_slot)
{ return 0; }
static inline void unregister_gpio_hotplug_slot(struct gpio_hotplug_slot *hp_slot) {}
#endif

#endif //_GPIOPHP_H
