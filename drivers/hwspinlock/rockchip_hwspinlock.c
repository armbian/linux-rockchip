// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2018 Fuzhou Rockchip Electronics Co., Ltd

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/hwspinlock.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include "hwspinlock_internal.h"

struct rockchip_hwspinlock {
	void __iomem *io_base;
	struct hwspinlock_device bank;
};

/* Number of Hardware Spinlocks*/
#define	HWSPINLOCK_NUMBER	64

/* Hardware spinlock register offsets */
#define HWSPINLOCK_OFFSET(x)	(0x4 * (x))
#define HWSPINLOCK_ID_MASK	0x0F

#define HWLOCK_DEFAULT_USER	0x01

static u32 hwlock_user_id;

static int rockchip_hwspinlock_trylock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;

	writel(hwlock_user_id, lock_addr);

	/*
	 * Get only first 4 bits and compare to HWSPINLOCK_OWNER_ID,
	 * if equal, we attempt to acquire the lock, otherwise,
	 * someone else has it.
	 */
	return (hwlock_user_id == (readl(lock_addr) & HWSPINLOCK_ID_MASK));
}

static void rockchip_hwspinlock_unlock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;
	u32 lock_owner = readl(lock_addr) & HWSPINLOCK_ID_MASK;

	if (lock_owner != hwlock_user_id) {
		dev_warn(lock->bank->dev,
			 "WARNING: against user %u release a lock held by %u\n",
			 hwlock_user_id, lock_owner);
		return;
	}

	/* Release the lock by writing 0 to it */
	writel(0, lock_addr);
}

static const struct hwspinlock_ops rockchip_hwspinlock_ops = {
	.trylock = rockchip_hwspinlock_trylock,
	.unlock = rockchip_hwspinlock_unlock,
};

static int rockchip_hwspinlock_probe(struct platform_device *pdev)
{
	struct rockchip_hwspinlock *hwspin;
	struct hwspinlock *hwlock;
	u32 num_locks = 0;
	int idx, ret;

	ret = device_property_read_u32(&pdev->dev, "rockchip,hwlock-num-locks",
				       &num_locks);
	if (ret || !num_locks)
		num_locks = HWSPINLOCK_NUMBER;

	hwspin = devm_kzalloc(&pdev->dev,
			      struct_size(hwspin, bank.lock, num_locks),
			      GFP_KERNEL);
	if (!hwspin)
		return -ENOMEM;

	hwspin->io_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(hwspin->io_base))
		return PTR_ERR(hwspin->io_base);

	ret = device_property_read_u32(&pdev->dev, "rockchip,hwlock-user-id",
				       &hwlock_user_id);
	if (ret || !hwlock_user_id || hwlock_user_id > HWSPINLOCK_ID_MASK)
		hwlock_user_id = HWLOCK_DEFAULT_USER;
	dev_info(&pdev->dev, "hwlock user id %u, locks %u\n", hwlock_user_id, num_locks);

	for (idx = 0; idx < num_locks; idx++) {
		hwlock = &hwspin->bank.lock[idx];
		hwlock->priv = hwspin->io_base + HWSPINLOCK_OFFSET(idx);
	}

	platform_set_drvdata(pdev, hwspin);

	return devm_hwspin_lock_register(&pdev->dev, &hwspin->bank,
					 &rockchip_hwspinlock_ops, 0,
					 num_locks);
}

static const struct of_device_id rockchip_hwpinlock_ids[] = {
	{ .compatible = "rockchip,hwspinlock", },
	{},
};
MODULE_DEVICE_TABLE(of, rockchip_hwpinlock_ids);

static struct platform_driver rockchip_hwspinlock_driver = {
	.probe = rockchip_hwspinlock_probe,
	.driver = {
		.name = "rockchip_hwspinlock",
		.of_match_table = of_match_ptr(rockchip_hwpinlock_ids),
	},
};

static int __init rockchip_hwspinlock_init(void)
{
	return platform_driver_register(&rockchip_hwspinlock_driver);
}
postcore_initcall(rockchip_hwspinlock_init);

static void __exit rockchip_hwspinlock_exit(void)
{
	platform_driver_unregister(&rockchip_hwspinlock_driver);
}
module_exit(rockchip_hwspinlock_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Rockchip Hardware spinlock driver");
MODULE_AUTHOR("Frank Wang <frank.wang@rock-chips.com>");
