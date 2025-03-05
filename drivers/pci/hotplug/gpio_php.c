// SPDX-License-Identifier: GPL-2.0+
/*
 * GPIO based PCI Hotplug Driver.
 *
 */

#include <linux/gpio/consumer.h>
#include <linux/libfdt.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include "../pci.h"
#include "../pcie/portdrv.h"

#include "gpiophp.h"

static DEFINE_MUTEX(slot_mutex);

static inline struct gpio_hotplug_slot *to_gpio_hotplug_slot(struct hotplug_slot *slot)
{
	return container_of(slot, struct gpio_hotplug_slot, hotplug_slot);
}

static int gpio_hp_get_power_status(struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct gpio_hotplug_slot *slot = to_gpio_hotplug_slot(hotplug_slot);
	struct pci_dev *dev;

	pci_config_pm_runtime_get(slot->pdev);
	dev = pci_get_slot(slot->pdev->subordinate, PCI_DEVFN(0, 0));
	if (dev) {
		pci_dev_put(dev);
		*value = 1;
	} else {
		*value = 0;
	}
	pci_dbg(slot->pdev, "Power Status = %u\n", *value);
	pci_config_pm_runtime_put(slot->pdev);

	return 0;
}

static int gpio_hp_enable_slot(struct hotplug_slot *hotplug_slot)
{
	struct gpio_hotplug_slot *slot = to_gpio_hotplug_slot(hotplug_slot);
	int ret = 0;
	u8 value;

	mutex_lock(&slot_mutex);

	gpio_hp_get_power_status(hotplug_slot, &value);
	if (value) {
		pci_info(slot->pdev, "Device is already plugged-in\n");
		goto exit;
	}

	if (slot->plat_ops && slot->plat_ops->enable)
		slot->plat_ops->enable(slot);

	pm_runtime_get_sync(&slot->pdev->dev);

	pci_lock_rescan_remove();
	pci_rescan_bus(slot->pdev->bus);
	pci_unlock_rescan_remove();

	pm_runtime_put(&slot->pdev->dev);

exit:
	mutex_unlock(&slot_mutex);
	return ret;
}

static int gpio_hp_disable_slot(struct hotplug_slot *hotplug_slot)
{
	struct gpio_hotplug_slot *slot = to_gpio_hotplug_slot(hotplug_slot);
	struct pci_dev *dev, *temp;
	u8 value;

	mutex_lock(&slot_mutex);

	gpio_hp_get_power_status(hotplug_slot, &value);
	if (!value) {
		pci_info(slot->pdev, "Device is already removed\n");
		goto exit;
	}

	pci_lock_rescan_remove();

	list_for_each_entry_safe_reverse(dev, temp, &slot->pdev->subordinate->devices, bus_list) {
		pci_dev_get(dev);
		pci_stop_and_remove_bus_device(dev);
		pci_dev_put(dev);
	}

	pci_unlock_rescan_remove();

exit:
	if (slot->plat_ops && slot->plat_ops->disable)
		slot->plat_ops->disable(slot);
	mutex_unlock(&slot_mutex);
	return 0;
}

static const struct hotplug_slot_ops gpio_hotplug_slot_ops = {
	.enable_slot = gpio_hp_enable_slot,
	.disable_slot = gpio_hp_disable_slot,
	.get_power_status = gpio_hp_get_power_status,
};

static irqreturn_t pcie_gpio_hp_irq(int irq, void *arg)
{
	struct gpio_hotplug_slot *slot = arg;

	if (gpiod_get_value(slot->gpiod)) {
		pci_dbg(slot->pdev, "Hot-Plug Event\n");
		gpio_hp_enable_slot(&slot->hotplug_slot);
	} else {
		pci_dbg(slot->pdev, "Hot-UnPlug Event\n");
		gpio_hp_disable_slot(&slot->hotplug_slot);
	}

	return IRQ_HANDLED;
}

int register_gpio_hotplug_slot(struct gpio_hotplug_slot *slot)
{
	struct device *dev = &slot->pdev->dev;
	struct pci_dev *pdev = slot->pdev;
	struct gpio_desc *gpiod;
	unsigned int irq;
	char *name;
	int ret;

	gpiod = devm_gpiod_get(&pdev->bus->dev, "hotplug", GPIOD_IN);
	if (IS_ERR(gpiod)) {
		ret = PTR_ERR(gpiod);
		pci_err(pdev, "Failed to find GPIO for Hot-Plug functionality: %d\n", ret);
		return ret;
	}

	ret = gpiod_to_irq(gpiod);
	if (ret < 0) {
		pci_err(pdev, "Failed to get IRQ for Hot_Plug GPIO: %d\n", ret);
		return ret;
	}
	irq = (unsigned int)ret;

	slot->gpiod = gpiod;
	slot->hotplug_slot.ops = &gpio_hotplug_slot_ops;
	slot->irq = irq;

	name = devm_kasprintf(dev, GFP_KERNEL, "slot_%u", slot->slot_nr);
	if (!name) {
		ret = -ENOMEM;
		goto exit;
	}

	ret = pci_hp_register(&slot->hotplug_slot, pdev->subordinate, 0, name);
	if (ret) {
		pci_err(pdev, "Failed to register hotplug slot: %d\n", ret);
		goto exit;
	}

	name = devm_kasprintf(dev, GFP_KERNEL, "pcie_gpio_hp_irq_%u", slot->slot_nr);
	if (!name) {
		ret = -ENOMEM;
		goto exit;
	}

	ret = devm_request_threaded_irq(dev, slot->irq,
					NULL, pcie_gpio_hp_irq,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					name, slot);
	if (ret < 0) {
		pci_err(pdev, "Failed to request IRQ for Hot-Plug: %d\n", ret);
		goto exit;
	}

	pci_dbg(pdev, "Hot-Plug Slot registered for %s\n", pci_name(pdev));

	gpio_hp_enable_slot(&slot->hotplug_slot);

	return 0;

exit:
	return ret;
}

void unregister_gpio_hotplug_slot(struct gpio_hotplug_slot *slot)
{
	struct device *dev = &slot->pdev->dev;
	struct pci_dev *pdev = slot->pdev;

	gpio_hp_disable_slot(&slot->hotplug_slot);
	devm_free_irq(dev, slot->irq, slot);
	pci_hp_deregister(&slot->hotplug_slot);
	devm_gpiod_put(&pdev->bus->dev, slot->gpiod);
}

