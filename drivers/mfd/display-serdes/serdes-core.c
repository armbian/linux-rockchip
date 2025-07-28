// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * serdes-core.c  --  Device access for different serdes chips
 *
 * Copyright (c) 2023-2028 Rockchip Electronics Co., Ltd.
 *
 * Author: luowei <lw@rock-chips.com>
 */

#include "core.h"

static unsigned long serdes_log_level;
static struct dentry *serdes_debugfs_root;

static const struct mfd_cell serdes_bu18tl82_devs[] = {
	{
		.name = "serdes-pinctrl",
		.of_compatible = "rohm,bu18tl82-pinctrl",
	},
	{
		.name = "serdes-bridge",
		.of_compatible = "rohm,bu18tl82-bridge",
	},
};

static const struct mfd_cell serdes_bu18rl82_devs[] = {
	{
		.name = "serdes-pinctrl",
		.of_compatible = "rohm,bu18rl82-pinctrl",
	},
	{
		.name = "serdes-bridge",
		.of_compatible = "rohm,bu18rl82-bridge",
	},
};

static const struct mfd_cell serdes_max96745_devs[] = {
	{
		.name = "serdes-pinctrl",
		.of_compatible = "maxim,max96745-pinctrl",
	},
	{
		.name = "serdes-bridge",
		.of_compatible = "maxim,max96745-bridge",
	},
	{
		.name = "serdes-bridge-split",
		.of_compatible = "maxim,max96745-bridge-split",
	},
};

static const struct mfd_cell serdes_max96749_devs[] = {
	{
		.name = "serdes-pinctrl",
		.of_compatible = "maxim,max96749-pinctrl",
	},
	{
		.name = "serdes-bridge",
		.of_compatible = "maxim,max96749-bridge",
	},
	{
		.name = "serdes-bridge-split",
		.of_compatible = "maxim,max96749-bridge-split",
	},
};

static const struct mfd_cell serdes_max96755_devs[] = {
	{
		.name = "serdes-pinctrl",
		.of_compatible = "maxim,max96755-pinctrl",
	},
	{
		.name = "serdes-bridge",
		.of_compatible = "maxim,max96755-bridge",
	},
};

static const struct mfd_cell serdes_max96789_devs[] = {
	{
		.name = "serdes-pinctrl",
		.of_compatible = "maxim,max96789-pinctrl",
	},
	{
		.name = "serdes-bridge",
		.of_compatible = "maxim,max96789-bridge",
	},
	{
		.name = "serdes-bridge-split",
		.of_compatible = "maxim,max96789-bridge-split",
	},
};

static const struct mfd_cell serdes_max96752_devs[] = {
	{
		.name = "serdes-pinctrl",
		.of_compatible = "maxim,max96752-pinctrl",
	},
	{
		.name = "serdes-panel",
		.of_compatible = "maxim,max96752-panel",
	},
	{
		.name = "serdes-panel-split",
		.of_compatible = "maxim,max96752-panel-split",
	},
};

static const struct mfd_cell serdes_max96772_devs[] = {
	{
		.name = "serdes-pinctrl",
		.of_compatible = "maxim,max96772-pinctrl",
	},
	{
		.name = "serdes-panel",
		.of_compatible = "maxim,max96772-panel",
	},
};

static const struct mfd_cell serdes_rkx111_devs[] = {
	{
		.name = "serdes-pinctrl",
		.of_compatible = "rockchip,rkx111-pinctrl",
	},
	{
		.name = "serdes-bridge",
		.of_compatible = "rockchip,rkx111-bridge",
	},
};

static const struct mfd_cell serdes_rkx121_devs[] = {
	{
		.name = "serdes-pinctrl",
		.of_compatible = "rockchip,rkx121-pinctrl",
	},
	{
		.name = "serdes-bridge",
		.of_compatible = "rockchip,rkx121-bridge",
	},
};

static const struct mfd_cell serdes_nca9539_devs[] = {
	{
		.name = "serdes-pinctrl",
		.of_compatible = "novo,nca9539-pinctrl",
	},
};

/**
 * serdes_reg_read: Read a single serdes register.
 *
 * @serdes: Device to read from.
 * @reg: Register to read.
 * @val: Data from register.
 */
int serdes_reg_read(struct serdes *serdes, unsigned int reg, unsigned int *val)
{
	int ret;

	ret = regmap_read(serdes->regmap, reg, val);
	SERDES_DBG_I2C("%s %s %s Read Reg%04x %04x ret=%d\n", __func__, dev_name(serdes->dev),
		       serdes->chip_data->name, reg, *val, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(serdes_reg_read);

/**
 * serdes_bulk_read: Read multiple serdes registers
 *
 * @serdes: Device to read from
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to fill.
 */
int serdes_bulk_read(struct serdes *serdes, unsigned int reg,
		     int count, u16 *buf)
{
	int i = 0, ret = 0;

	ret = regmap_bulk_read(serdes->regmap, reg, buf, count);
	for (i = 0; i < count; i++) {
		SERDES_DBG_I2C("%s %s %s Read Reg%04x %04x ret=%d\n",
			       __func__, dev_name(serdes->dev),
			       serdes->chip_data->name, reg + i, buf[i], ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(serdes_bulk_read);

int serdes_bulk_write(struct serdes *serdes, unsigned int reg,
		      int count, void *src)
{
	u16 *buf = src;
	int i, ret;

	if (serdes->debug == SERDES_CLOSE_I2C_WRITE)
		return 0;

	WARN_ON(count <= 0);

	mutex_lock(&serdes->io_lock);
	for (i = 0; i < count; i++) {
		ret = regmap_write(serdes->regmap, reg, buf[i]);
		SERDES_DBG_I2C("%s %s %s Write Reg%04x %04x ret=%d\n",
			       __func__, dev_name(serdes->dev),
			       serdes->chip_data->name, reg, buf[i], ret);
		if (ret != 0) {
			mutex_unlock(&serdes->io_lock);
			return ret;
		}
	}
	mutex_unlock(&serdes->io_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(serdes_bulk_write);

/**
 * serdes_multi_reg_write: Write many serdes register.
 *
 * @serdes: Device to write to.
 * @regs: Registers to write to.
 * @num_regs: Number of registers to write.
 */
int serdes_multi_reg_write(struct serdes *serdes, const struct reg_sequence *regs,
			   int num_regs)
{
	int i, ret;

	if (serdes->debug == SERDES_CLOSE_I2C_WRITE)
		return 0;

	SERDES_DBG_I2C("%s %s %s num=%d\n", __func__, dev_name(serdes->dev),
		       serdes->chip_data->name, num_regs);
	ret = regmap_multi_reg_write(serdes->regmap, regs, num_regs);
	for (i = 0; i < num_regs; i++) {
		SERDES_DBG_I2C("serdes %s Write Reg%04x %04x ret=%d\n",
			       serdes->chip_data->name, regs[i].reg, regs[i].def, ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(serdes_multi_reg_write);

/**
 * serdes_reg_write: Write a single serdes register.
 *
 * @serdes: Device to write to.
 * @reg: Register to write to.
 * @val: Value to write.
 */
int serdes_reg_write(struct serdes *serdes, unsigned int reg,
		     unsigned int val)
{
	int ret;

	if (serdes->debug == SERDES_CLOSE_I2C_WRITE)
		return 0;

	ret = regmap_write(serdes->regmap, reg, val);
	SERDES_DBG_I2C("%s %s %s Write Reg%04x %04x ret=%d\n", __func__, dev_name(serdes->dev),
		       serdes->chip_data->name, reg, val, ret);
	if (ret != 0)
		return ret;

	return ret;
}
EXPORT_SYMBOL_GPL(serdes_reg_write);

/**
 * serdes_set_bits: Set the value of a bitfield in a serdes register
 *
 * @serdes: Device to write to.
 * @reg: Register to write to.
 * @mask: Mask of bits to set.
 * @val: Value to set (unshifted)
 */
int serdes_set_bits(struct serdes *serdes, unsigned int reg,
		    unsigned int mask, unsigned int val)
{
	int ret;

	if (serdes->debug == SERDES_CLOSE_I2C_WRITE)
		return 0;

	SERDES_DBG_I2C("%s %s %s Write Reg%04x %04x) mask=%04x\n", __func__,
		       dev_name(serdes->dev), serdes->chip_data->name, reg, val, mask);
	ret = regmap_update_bits(serdes->regmap, reg, mask, val);

	return ret;
}
EXPORT_SYMBOL_GPL(serdes_set_bits);

/**
 * serdes_mfd_add: Manage serdes device resources
 *
 * Returns 0 on success.
 *
 * @dev: Pointer to parent device.
 * @serdes_dev: Array of describing serdes child devices.
 * @mfd_num: Number of serdes child devices to register.
 */
static int serdes_mfd_add(struct device *dev,
			  const struct mfd_cell *serdes_dev, int mfd_num)
{
	int i, ret, num = 0;
	const char *compatible;
	struct mfd_cell *obj, *mfd_dev;
	struct device_node *child = NULL;
	int size = sizeof(struct mfd_cell);
	struct device_node *parent_node = dev->of_node;

	obj = kcalloc(mfd_num, size, GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	for (i = 0; i < mfd_num; i++, serdes_dev++) {
		compatible = serdes_dev->of_compatible;

		for_each_available_child_of_node(parent_node, child) {
			if (!of_device_is_compatible(child, compatible))
				continue;

			memcpy(&obj[num], serdes_dev, size);
			num++;
			of_node_put(child);
			SERDES_DBG_MFD("%s: serdes child %s match\n", __func__, serdes_dev->name);

			break;
		}
	}

	if (num == 0) {
		kfree(obj);
		return 0;
	}

	mfd_dev = devm_kmemdup(dev, obj, num * size, GFP_KERNEL);
	kfree(obj);

	if (!mfd_dev)
		return -ENOMEM;

	ret = devm_mfd_add_devices(dev, PLATFORM_DEVID_AUTO, mfd_dev,
				   num, NULL, 0, NULL);

	if (ret != 0) {
		dev_err(dev, "Failed to add serdes child device\n");
		goto err;
	}

	return 0;

err:
	devm_kfree(dev, mfd_dev);

	return ret;
}

/*
 * Instantiate the generic non-control parts of the device.
 */
int serdes_device_init(struct serdes *serdes)
{
	struct serdes_chip_data *chip_data = serdes->chip_data;
	int ret = 0;
	const struct mfd_cell *serdes_devs = NULL;
	int mfd_num = 0;

	switch (chip_data->serdes_id) {
	case ROHM_ID_BU18TL82:
		serdes_devs = serdes_bu18tl82_devs;
		mfd_num = ARRAY_SIZE(serdes_bu18tl82_devs);
		break;
	case ROHM_ID_BU18RL82:
		serdes_devs = serdes_bu18rl82_devs;
		mfd_num = ARRAY_SIZE(serdes_bu18rl82_devs);
		break;
	case MAXIM_ID_MAX96745:
		serdes_devs = serdes_max96745_devs;
		mfd_num = ARRAY_SIZE(serdes_max96745_devs);
		break;
	case MAXIM_ID_MAX96749:
		serdes_devs = serdes_max96749_devs;
		mfd_num = ARRAY_SIZE(serdes_max96749_devs);
		break;
	case MAXIM_ID_MAX96752:
		serdes_devs = serdes_max96752_devs;
		mfd_num = ARRAY_SIZE(serdes_max96752_devs);
		break;
	case MAXIM_ID_MAX96755:
		serdes_devs = serdes_max96755_devs;
		mfd_num = ARRAY_SIZE(serdes_max96755_devs);
		break;
	case MAXIM_ID_MAX96772:
		serdes_devs = serdes_max96772_devs;
		mfd_num = ARRAY_SIZE(serdes_max96772_devs);
		break;
	case MAXIM_ID_MAX96789:
		serdes_devs = serdes_max96789_devs;
		mfd_num = ARRAY_SIZE(serdes_max96789_devs);
		break;
	case ROCKCHIP_ID_RKX111:
		serdes_devs = serdes_rkx111_devs;
		mfd_num = ARRAY_SIZE(serdes_rkx111_devs);
		break;
	case ROCKCHIP_ID_RKX121:
		serdes_devs = serdes_rkx121_devs;
		mfd_num = ARRAY_SIZE(serdes_rkx121_devs);
		break;
	case NOVO_ID_NCA9539:
		serdes_devs = serdes_nca9539_devs;
		mfd_num = ARRAY_SIZE(serdes_nca9539_devs);
		break;
	default:
		dev_info(serdes->dev, "%s: unknown device\n", __func__);
		break;
	}

	ret = serdes_mfd_add(serdes->dev, serdes_devs, mfd_num);
	if (!ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(serdes_device_init);

static int log_level_show(struct seq_file *m, void *data)
{
	seq_printf(m, "%lu\n", serdes_log_level);

	return 0;
}

static int log_level_open(struct inode *inode, struct file *file)
{
	return single_open(file, log_level_show, NULL);
}

static ssize_t log_level_write(struct file *file, const char __user *ubuf,
			       size_t len, loff_t *offp)
{
	char buf[12];
	unsigned long value;

	if (len > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;

	buf[len] = '\0';

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	serdes_log_level = value;

	return len;
}

static int debug_show(struct seq_file *m, void *data)
{
	struct serdes *serdes = m->private;

	seq_printf(m, "%d\n", serdes->debug);

	return 0;
}

static int debug_open(struct inode *inode, struct file *file)
{
	struct serdes *serdes = inode->i_private;

	return single_open(file, debug_show, serdes);
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
			       size_t len, loff_t *offp)
{
	struct seq_file *m = file->private_data;
	struct serdes *serdes = m->private;
	char buf[12];

	if (!serdes)
		return -EINVAL;

	if (len > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;

	buf[len] = '\0';

	if (sysfs_streq(buf, "on"))
		serdes->debug = SERDES_OPEN_I2C_WRITE;
	else if (sysfs_streq(buf, "off"))
		serdes->debug = SERDES_CLOSE_I2C_WRITE;
	else if (sysfs_streq(buf, "default")) {
		serdes->debug = SERDES_SET_PINCTRL_DEFAULT;
		serdes_set_pinctrl_default(serdes);
	} else if (sysfs_streq(buf, "sleep")) {
		serdes->debug = SERDES_SET_PINCTRL_SLEEP;
		serdes_set_pinctrl_sleep(serdes);
	} else if (sysfs_streq(buf, "seq")) {
		serdes->debug = SERDES_SET_SEQUENCE;
		serdes_i2c_set_sequence(serdes);
	} else
		return -EINVAL;

	return len;
}

static const struct file_operations log_level_fops = {
	.owner = THIS_MODULE,
	.open = log_level_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = log_level_write
};

static const struct file_operations debug_fops = {
	.owner = THIS_MODULE,
	.open = debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = debug_write
};

void serdes_create_debugfs(struct serdes *serdes)
{

	snprintf(serdes->dir_name, sizeof(serdes->dir_name), "%s-%s",
				     dev_name(serdes->dev), serdes->chip_data->name);

	serdes->debugfs_dentry = debugfs_create_dir(serdes->dir_name, serdes_debugfs_root);
	debugfs_create_file("debug", 0664, serdes->debugfs_dentry, serdes,
				     &debug_fops);
}
EXPORT_SYMBOL(serdes_create_debugfs);

void serdes_destroy_debugfs(struct serdes *serdes)
{
	debugfs_remove_recursive(serdes->debugfs_dentry);
}
EXPORT_SYMBOL(serdes_destroy_debugfs);

void serdes_debugfs_init(void)
{
	serdes_debugfs_root = debugfs_create_dir("serdes", NULL);

	debugfs_create_file("log_level", 0664, serdes_debugfs_root, NULL,
				     &log_level_fops);
}
EXPORT_SYMBOL(serdes_debugfs_init);

void serdes_debugfs_exit(void)
{
	debugfs_remove_recursive(serdes_debugfs_root);
}
EXPORT_SYMBOL(serdes_debugfs_exit);

void serdes_dev_dbg(enum serdes_log_category category, const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	if (!unlikely(serdes_log_level & BIT(category)))
		return;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	pr_info("%pV", &vaf);

	va_end(args);
}
EXPORT_SYMBOL(serdes_dev_dbg);

int serdes_set_pinctrl_default(struct serdes *serdes)
{
	int ret = 0;

	if ((!IS_ERR_OR_NULL(serdes->pinctrl_node)) && (!IS_ERR_OR_NULL(serdes->pins_init))) {
		ret = pinctrl_select_state(serdes->pinctrl_node, serdes->pins_init);
		if (ret)
			dev_err(serdes->dev, "could not set init pins\n");
		SERDES_DBG_MFD("%s: name=%s init\n", __func__, dev_name(serdes->dev));
	}

	return ret;
}
EXPORT_SYMBOL_GPL(serdes_set_pinctrl_default);

int serdes_set_pinctrl_sleep(struct serdes *serdes)
{
	int ret = 0;

	if ((!IS_ERR_OR_NULL(serdes->pinctrl_node)) && (!IS_ERR_OR_NULL(serdes->pins_sleep))) {
		ret = pinctrl_select_state(serdes->pinctrl_node, serdes->pins_sleep);
		if (ret)
			dev_err(serdes->dev, "could not set sleep pins\n");
		SERDES_DBG_MFD("%s: name=%s\n", __func__, dev_name(serdes->dev));
	}

	return ret;
}
EXPORT_SYMBOL_GPL(serdes_set_pinctrl_sleep);

int serdes_device_suspend(struct serdes *serdes)
{
	int ret = 0;

	if (!IS_ERR(serdes->vpower)) {
		ret = regulator_disable(serdes->vpower);
		if (ret) {
			dev_err(serdes->dev, "fail to disable vpower regulator\n");
			return ret;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(serdes_device_suspend);

int serdes_device_resume(struct serdes *serdes)
{
	int ret = 0;

	if (!IS_ERR(serdes->vpower)) {
		ret = regulator_enable(serdes->vpower);
		if (ret) {
			dev_err(serdes->dev, "fail to enable vpower regulator\n");
			return ret;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(serdes_device_resume);

void serdes_device_poweroff(struct serdes *serdes)
{
	int ret = 0;

	if ((!IS_ERR_OR_NULL(serdes->pinctrl_node)) && (!IS_ERR_OR_NULL(serdes->pins_sleep))) {
		ret = pinctrl_select_state(serdes->pinctrl_node, serdes->pins_sleep);
		if (ret)
			dev_err(serdes->dev, "could not set sleep pins\n");
	}

	if (!IS_ERR(serdes->vpower)) {
		ret = regulator_disable(serdes->vpower);
		if (ret)
			dev_err(serdes->dev, "fail to disable vpower regulator\n");
	}

}
EXPORT_SYMBOL_GPL(serdes_device_poweroff);

int serdes_device_shutdown(struct serdes *serdes)
{
	int ret = 0;

	if (!IS_ERR(serdes->vpower)) {
		ret = regulator_disable(serdes->vpower);
		if (ret) {
			dev_err(serdes->dev, "fail to disable vpower regulator\n");
			return ret;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(serdes_device_shutdown);

MODULE_LICENSE("GPL");
