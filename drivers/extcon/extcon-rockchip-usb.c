// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 *
 * Rockchip USB Extcon Driver
 */

#include <linux/extcon-provider.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/usb/role.h>
#include <linux/usb/typec_mux.h>

#define RK_USB_EXTCON_PORT_NUM	2

enum rk_usb_port_orien {
	USB_PORT_ORIEN_NONE = -1,
	USB_PORT_ORIEN_NORMAL = TYPEC_ORIENTATION_NORMAL,
	USB_PORT_ORIEN_REVERSE = TYPEC_ORIENTATION_REVERSE,
};

struct rk_usb_extcon_port {
	struct device dev;
	struct extcon_dev *edev; /* Register and manage in this driver */
	struct extcon_dev *phy_edev; /* Get DT resource registered by the usb2-phy driver */
	enum rk_usb_port_orien orien;
};

struct rk_usb_extcon {
	struct device *dev;
	struct typec_switch_dev *orien_sw;
	struct usb_role_switch *role_sw;
	enum typec_orientation pre_orien;
	enum typec_orientation new_orien;
	enum usb_role role;
	unsigned int port_cnt;
	struct rk_usb_extcon_port ports[RK_USB_EXTCON_PORT_NUM];
};

static const unsigned int rk_usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static void rk_usb_extcon_set_state_sync(struct rk_usb_extcon_port *port, enum usb_role role)
{
	extcon_set_state(port->edev, EXTCON_USB, role == USB_ROLE_DEVICE);
	extcon_set_state(port->edev, EXTCON_USB_HOST, role == USB_ROLE_HOST);

	if (port->phy_edev) {
		extcon_set_state(port->phy_edev, EXTCON_USB, role == USB_ROLE_DEVICE);
		extcon_set_state(port->phy_edev, EXTCON_USB_HOST, role == USB_ROLE_HOST);
	}

	extcon_sync(port->edev, EXTCON_USB);
	extcon_sync(port->edev, EXTCON_USB_HOST);
}

static int rk_usb_extcon_role_notify(struct rk_usb_extcon *usb_ext,
				     enum usb_role role)
{
	int i;

	for (i = 0; i < usb_ext->port_cnt; i++) {
		struct rk_usb_extcon_port *port = &usb_ext->ports[i];
		enum rk_usb_port_orien port_fixed_orien = port->orien;

		switch (port_fixed_orien) {
		case USB_PORT_ORIEN_NORMAL:
			if (usb_ext->new_orien == TYPEC_ORIENTATION_NORMAL ||
			    (usb_ext->new_orien == TYPEC_ORIENTATION_NONE &&
			     usb_ext->pre_orien == TYPEC_ORIENTATION_NORMAL)) {
				dev_dbg(usb_ext->dev, "set normal side usb mode %d\n", role);
				rk_usb_extcon_set_state_sync(port, role);
			}
			break;

		case USB_PORT_ORIEN_REVERSE:
			if (usb_ext->new_orien == TYPEC_ORIENTATION_REVERSE ||
			    (usb_ext->new_orien == TYPEC_ORIENTATION_NONE &&
			     usb_ext->pre_orien == TYPEC_ORIENTATION_REVERSE)) {
				dev_dbg(usb_ext->dev, "set reverse side usb mode %d\n", role);
				rk_usb_extcon_set_state_sync(port, role);
			}
			break;

		default:
			dev_dbg(usb_ext->dev, "set %d side usb mode %d\n", usb_ext->new_orien, role);
			rk_usb_extcon_set_state_sync(port, role);
			break;
		}
	}

	return 0;
}

static void rk_usb_extcon_role_sw_unregister(void *data)
{
	struct rk_usb_extcon *usb_ext = data;

	usb_role_switch_unregister(usb_ext->role_sw);
}

static int rk_usb_extcon_role_sw_set(struct usb_role_switch *sw,
				     enum usb_role role)
{
	struct rk_usb_extcon *usb_ext = usb_role_switch_get_drvdata(sw);
	int ret;

	dev_dbg(usb_ext->dev, "new usb role: %d\n", role);

	ret = rk_usb_extcon_role_notify(usb_ext, role);
	if (ret)
		return ret;

	usb_ext->role = role;

	return 0;
}

static int rk_usb_extcon_role_sw_setup(struct rk_usb_extcon *usb_ext)
{
	struct usb_role_switch_desc usb_role_switch = { };

	usb_role_switch.fwnode = dev_fwnode(usb_ext->dev);
	usb_role_switch.set = rk_usb_extcon_role_sw_set;
	usb_role_switch.driver_data = usb_ext;

	usb_ext->role_sw = usb_role_switch_register(usb_ext->dev, &usb_role_switch);
	if (IS_ERR(usb_ext->role_sw))
		return PTR_ERR(usb_ext->role_sw);

	return 0;
}

static int rk_usb_extcon_role_sw_init(struct rk_usb_extcon *usb_ext)
{
	int ret;

	ret = device_property_present(usb_ext->dev, "usb-role-switch");
	if (!ret) {
		dev_err(usb_ext->dev, "usb-role-switch property is not present\n");
		return -EINVAL;
	}

	ret = rk_usb_extcon_role_sw_setup(usb_ext);
	if (ret)
		return ret;

	return devm_add_action_or_reset(usb_ext->dev, rk_usb_extcon_role_sw_unregister, usb_ext);
}

static void rk_usb_extcon_orien_sw_unregister(void *data)
{
	struct rk_usb_extcon *usb_ext = data;

	typec_switch_unregister(usb_ext->orien_sw);
}

static int rk_usb_extcon_orien_sw_set(struct typec_switch_dev *sw, enum typec_orientation orien)
{
	struct rk_usb_extcon *usb_ext = typec_switch_get_drvdata(sw);

	dev_dbg(usb_ext->dev, "usbc pre orien %d, new orien: %d\n", usb_ext->pre_orien, orien);

	usb_ext->pre_orien = usb_ext->new_orien;
	usb_ext->new_orien = orien;

	return 0;
}

static int rk_usb_extcon_orien_sw_setup(struct rk_usb_extcon *usb_ext)
{
	struct typec_switch_desc orien_sw_desc = { };
	struct device *dev = usb_ext->dev;

	orien_sw_desc.drvdata = usb_ext;
	orien_sw_desc.fwnode = dev_fwnode(dev);
	orien_sw_desc.set = rk_usb_extcon_orien_sw_set;

	usb_ext->orien_sw = typec_switch_register(dev, &orien_sw_desc);
	if (IS_ERR(usb_ext->orien_sw)) {
		dev_err(dev, "error register typec orientation switch: %ld\n",
			PTR_ERR(usb_ext->orien_sw));
		return PTR_ERR(usb_ext->orien_sw);
	}

	return 0;
}

static int rk_usb_extcon_orien_sw_init(struct rk_usb_extcon *usb_ext)
{
	int ret;

	ret = device_property_present(usb_ext->dev, "orientation-switch");
	if (!ret) {
		dev_err(usb_ext->dev, "orientation-switch property is not present\n");
		return -EINVAL;
	}

	usb_ext->pre_orien = TYPEC_ORIENTATION_NONE;
	usb_ext->new_orien = TYPEC_ORIENTATION_NONE;

	ret = rk_usb_extcon_orien_sw_setup(usb_ext);
	if (ret)
		return ret;

	return devm_add_action_or_reset(usb_ext->dev, rk_usb_extcon_orien_sw_unregister, usb_ext);
}

static int rk_usb_extcon_port_edev_register(struct rk_usb_extcon_port *port)
{
	struct device *dev = &port->dev;
	struct extcon_dev *edev;
	int ret;

	edev = devm_extcon_dev_allocate(dev, rk_usb_extcon_cable);
	if (IS_ERR(edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(dev, edev);
	if (ret < 0) {
		dev_err(dev, "failed to register extcon device, ret %d\n", ret);
		return ret;
	}

	ret = extcon_set_property_capability(edev, EXTCON_USB, EXTCON_PROP_USB_SS);
	ret |= extcon_set_property_capability(edev, EXTCON_USB_HOST, EXTCON_PROP_USB_SS);
	if (ret) {
		dev_err(dev, "failed to register extcon props ret %d\n", ret);
		return ret;
	}

	dev_dbg(dev, "registered extcon device %s\n", extcon_get_edev_name(edev));
	port->edev = edev;

	return 0;
}

static void rk_usb_extcon_port_dev_remove(void *data)
{
	struct rk_usb_extcon_port *port = data;

	device_unregister(&port->dev);
}

static void rk_usb_extcon_port_dev_release(struct device *dev)
{
	if (dev->of_node)
		of_node_put(dev->of_node);
}

static int rk_usb_extcon_port_dev_create(struct rk_usb_extcon *usb_ext,
					 struct device_node *child_np,
					 int index)
{
	struct device *dev = usb_ext->dev;
	struct rk_usb_extcon_port *port = &usb_ext->ports[index];
	int ret;

	device_initialize(&port->dev);
	port->dev.of_node = of_node_get(child_np);
	port->dev.release = rk_usb_extcon_port_dev_release;
	port->dev.parent = dev;

	ret = dev_set_name(&port->dev, "%s-port%d", dev_name(dev), index);
	if (ret < 0)
		goto put_dev;

	ret = device_add(&port->dev);
	if (ret < 0)
		goto put_dev;

	dev_set_drvdata(&port->dev, usb_ext);

	return devm_add_action_or_reset(usb_ext->dev, rk_usb_extcon_port_dev_remove, port);

put_dev:
	put_device(&port->dev);
	return ret;
}

static int rk_usb_extcon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	struct rk_usb_extcon *usb_ext;
	struct rk_usb_extcon_port *port;
	u32 index, orien;
	int ret;

	if (!np)
		return -EINVAL;

	usb_ext = devm_kzalloc(&pdev->dev, sizeof(*usb_ext), GFP_KERNEL);
	if (!usb_ext)
		return -ENOMEM;

	usb_ext->dev = dev;
	platform_set_drvdata(pdev, usb_ext);

	ret = rk_usb_extcon_orien_sw_init(usb_ext);
	if (ret)
		goto err_exit;

	ret = rk_usb_extcon_role_sw_init(usb_ext);
	if (ret)
		goto err_exit;

	index = 0;
	for_each_available_child_of_node(np, child_np) {
		if (index >= RK_USB_EXTCON_PORT_NUM) {
			dev_err(dev, "too many port nodes, max is %d\n", RK_USB_EXTCON_PORT_NUM);
			ret = -EINVAL;
			goto put_child;
		}

		port = &usb_ext->ports[index];

		ret = rk_usb_extcon_port_dev_create(usb_ext, child_np, index);
		if (ret)
			goto put_child;

		ret = rk_usb_extcon_port_edev_register(port);
		if (ret)
			goto put_child;

		ret = of_property_read_u32(child_np, "rockchip,usbc-orientation", &orien);
		if (ret) {
			dev_info(&port->dev, "this port has no usbc-orientation property\n");
			port->orien = USB_PORT_ORIEN_NONE;
		} else {
			if (orien != USB_PORT_ORIEN_NORMAL && orien != USB_PORT_ORIEN_REVERSE) {
				dev_err(&port->dev, "invalid usbc-orientation value %d\n", orien);
				ret = -EINVAL;
				goto put_child;
			}
			port->orien = orien;
		}

		if (device_property_present(&port->dev, "extcon")) {
			port->phy_edev = extcon_get_edev_by_phandle(&port->dev, 0);
			if (IS_ERR(port->phy_edev)) {
				dev_err(&port->dev, "failed to get phy extcon device\n");
				ret = PTR_ERR(port->phy_edev);
				goto put_child;
			}

			dev_dbg(&port->dev, "got phy edev: %s\n", extcon_get_edev_name(port->phy_edev));
		}

		index++;
	}

	if (!index) {
		dev_err(dev, "no port nodes found\n");
		ret = -EINVAL;
		goto err_exit;
	}

	usb_ext->port_cnt = index;

	return 0;

put_child:
	of_node_put(child_np);
err_exit:
	return ret;
}

static const struct of_device_id rk_usb_extcon_dt_match[] = {
	{ .compatible = "rockchip,extcon-usb", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rk_usb_extcon_dt_match);

static struct platform_driver rk_usb_extcon_driver = {
	.probe		= rk_usb_extcon_probe,
	.driver		= {
		.name		= "extcon-rockchip-usb",
		.of_match_table	= rk_usb_extcon_dt_match,
	},
};

module_platform_driver(rk_usb_extcon_driver);

MODULE_AUTHOR("Frank Wang <frank.wang@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip USB extcon driver");
MODULE_LICENSE("GPL");
