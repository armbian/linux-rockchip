/*
 * Copyright 2018 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/netlink.h>
#include <net/sock.h>

#include "core.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "pb.h"
#include "vehiclehalproto.pb.h"
#include "vehicle_protocol_callback.h"
#include "vehicle_core.h"

#define PROTOCOL_ID 30

struct vehicle *g_vehicle_hw;
EXPORT_SYMBOL(g_vehicle_hw);
static struct sock *nlsk;
static int g_user_pid;

struct vehicle_core_drvdata {
	const struct hw_prop_ops *prop_ops;
};

/* param in command PWR_REQ */
enum vehicle_power_request_param {
	AP_POWER_REQUEST_PARAM_SHUTDOWN_IMMEDIATELY = 1,
	AP_POWER_REQUEST_PARAM_CAN_SLEEP,
	AP_POWER_REQUEST_PARAM_SHUTDOWN_ONLY,
	AP_POWER_REQUEST_PARAM_SLEEP_IMMEDIATELY,
	AP_POWER_REQUEST_PARAM_HIBERNATE_IMMEDIATELY,
	AP_POWER_REQUEST_PARAM_CAN_HIBERNATE,
};

/* state in command PWR_REQ */
enum vehicle_power_request_state {
	AP_POWER_REQUEST_STATE_ON = 0,
	AP_POWER_REQUEST_STATE_SHUTDOWN_PREPARE,
	AP_POWER_REQUEST_STATE_CANCEL_SHUTDOWN,
	AP_POWER_REQUEST_STATE_FINISHED,
};

static struct vehicle_core_drvdata *vehicle_core;
static struct vehicle_property_set property_encode;
static struct vehicle_property_set property_decode;
static struct vehicle_power_req power_req_encode;

void vehicle_hw_prop_ops_register(const struct hw_prop_ops *prop_ops)
{
	if (!prop_ops)
		return;

	if (vehicle_core)
		vehicle_core->prop_ops = prop_ops;
}
EXPORT_SYMBOL_GPL(vehicle_hw_prop_ops_register);

static int vehicle_send_message_core(u32 prop, u32 area, u32 value)
{
	if (vehicle_core && vehicle_core->prop_ops &&
	    vehicle_core->prop_ops->set_control_commands)
		vehicle_core->prop_ops->set_control_commands(prop, area, value);
	return 0;
}

static int send_usrmsg(char *pbuf, uint16_t len)
{
	struct sk_buff *nl_skb;
	struct nlmsghdr *nlh;

	int ret;

	if (nlsk == NULL || pbuf == NULL) {
		pr_err("Invalid parameters nlsk %p pbuf %p\n", nlsk, pbuf);
		return -1;
	}

	nl_skb = nlmsg_new(len, GFP_ATOMIC);
	if (!nl_skb) {
		pr_err("netlink alloc failure\n");
		return -1;
	}

	nlh = nlmsg_put(nl_skb, 0, 0, 0, len, 0);
	if (nlh == NULL) {
		pr_err("nlmsg_put failaure\n");
		nlmsg_free(nl_skb);
		return -1;
	}

	memcpy(nlmsg_data(nlh), pbuf, len);
	ret = netlink_unicast(nlsk, nl_skb, g_user_pid, MSG_DONTWAIT);

	return ret;
}

void vehicle_set_property(u16 prop, u8 index, u32 value, u32 param)
{
	char *buffer;

	buffer = kmalloc(128, GFP_KERNEL);
	pr_debug("%s: prop %d, index %d, value %d\n", __func__, prop, index, value);
	property_encode.value = value;
	switch (prop) {
	case VEHICLE_FAN_SPEED:
		property_encode.prop = HVAC_FAN_SPEED;
		property_encode.area_id = HVAC_ALL;
		if (property_encode.value != FAN_SPEED_0 &&
		    property_encode.value != FAN_SPEED_1 &&
		    property_encode.value != FAN_SPEED_2 &&
		    property_encode.value != FAN_SPEED_3 &&
		    property_encode.value != FAN_SPEED_4 &&
		    property_encode.value != FAN_SPEED_5) {
			pr_err("input value is not correct, please type correct one\n");
			goto exit;
		}
		break;
	case VEHICLE_FAN_DIRECTION:
		property_encode.prop = HVAC_FAN_DIRECTION;
		property_encode.area_id = HVAC_ALL;
		if (property_encode.value != FAN_DIRECTION_0 &&
		    property_encode.value != FAN_DIRECTION_1 &&
		    property_encode.value != FAN_DIRECTION_2 &&
		    property_encode.value != FAN_DIRECTION_3) {
			pr_err("input value is not correct, please type correct one\n");
			goto exit;
		}
		break;
	case VEHICLE_AUTO_ON:
		property_encode.prop = HVAC_AUTO_ON;
		property_encode.area_id = (u32)index;
		if (property_encode.value != AUTO_ON && property_encode.value != AUTO_OFF) {
			pr_err("input value is not correct, please type correct one\n");
			goto exit;
		}
		break;
	case VEHICLE_AC:
		property_encode.prop = HVAC_AC_ON;
		property_encode.area_id = HVAC_ALL;
		if (property_encode.value != AC_ON && property_encode.value != AC_OFF) {
			pr_err("input value is not correct, please type correct one\n");
			goto exit;
		}
		break;
	case VEHICLE_RECIRC_ON:
		property_encode.prop = HVAC_RECIRC_ON;
		property_encode.area_id = HVAC_ALL;
		if (property_encode.value != RECIRC_ON && property_encode.value != RECIRC_OFF) {
			pr_err("input value is not correct, please type correct one\n");
			goto exit;
		}
		break;
	case VEHICLE_DEFROST:
		property_encode.prop = HVAC_DEFROSTER;
		property_encode.area_id = (u32)index;
		if (property_encode.value != DEFROST_ON && property_encode.value != DEFROST_OFF) {
			pr_err("input value is not correct, please type correct one\n");
			goto exit;
		}
		break;
	case VEHICLE_AC_TEMP:
		property_encode.prop = HVAC_TEMPERATURE_SET;
		property_encode.area_id = (u32)index;
		break;
	case VEHICLE_HVAC_POWER_ON:
		property_encode.prop = HVAC_POWER_ON;
		property_encode.area_id = (u32)index;
		if (property_encode.value != HVAC_ON && property_encode.value != HVAC_OFF) {
			pr_err("input value is not correct, please type correct one\n");
			goto exit;
		}
		break;
	case VEHICLE_SEAT_TEMPERATURE:
		property_encode.prop = HVAC_SEAT_TEMPERATURE;
		property_encode.area_id = (u32)index;
		if (property_encode.value != SEAT_TEMP_0 &&
		    property_encode.value != SEAT_TEMP_1 &&
		    property_encode.value != SEAT_TEMP_2 &&
		    property_encode.value != SEAT_TEMP_3) {
			pr_err("input value is not correct, please type correct one\n");
			goto exit;
		}
		break;
	case VEHICLE_GEAR:
		property_encode.prop = GEAR_SELECTION;
		if (value == VEHICLE_GEAR_DRIVE)
			property_encode.value = VEHICLE_GEAR_DRIVE_CLIENT;
		else if (value == VEHICLE_GEAR_REVERSE)
			property_encode.value = VEHICLE_GEAR_REVERSE_CLIENT;
		else if (value == VEHICLE_GEAR_PARKING)
			property_encode.value = VEHICLE_GEAR_PARK_CLIENT;
		break;
	case VEHICLE_TURN_SIGNAL:
		property_encode.prop = TURN_SIGNAL_STATE;
		break;
	case VEHICLE_POWER_STATE_REQ:
		if (value != AP_POWER_REQUEST_STATE_ON &&
		    value != AP_POWER_REQUEST_STATE_SHUTDOWN_PREPARE &&
		    value != AP_POWER_REQUEST_STATE_CANCEL_SHUTDOWN &&
		    value != AP_POWER_REQUEST_STATE_FINISHED) {
			pr_err("AP_POWER_STATE_REQ: invalid state\n");
			goto exit;
		}
		if (param != AP_POWER_REQUEST_PARAM_SHUTDOWN_IMMEDIATELY &&
		    param != AP_POWER_REQUEST_PARAM_CAN_SLEEP &&
		    param != AP_POWER_REQUEST_PARAM_SHUTDOWN_ONLY &&
		    param != AP_POWER_REQUEST_PARAM_SLEEP_IMMEDIATELY &&
		    param != AP_POWER_REQUEST_PARAM_HIBERNATE_IMMEDIATELY &&
		    param != AP_POWER_REQUEST_PARAM_CAN_HIBERNATE) {
			pr_err("AP_POWER_STATE_REQ: invalid param\n");
			goto exit;
		}

		power_req_encode.prop = AP_POWER_STATE_REQ;
		power_req_encode.state = value;
		power_req_encode.param = param;
		break;
	default:
		pr_err("property %d is not supported\n", prop);
	}

	pb_ostream_t stream;

	stream = pb_ostream_from_buffer(buffer, 128);

	emulator_EmulatorMessage send_message = {};

	send_message.msg_type = emulator_MsgType_SET_PROPERTY_CMD;
	send_message.has_status = true;
	send_message.status = emulator_Status_RESULT_OK;
	if (prop == VEHICLE_POWER_STATE_REQ) {
		send_message.value.funcs.encode = &encode_power_state_callback;
		send_message.value.arg = &power_req_encode;
	} else {
		send_message.value.funcs.encode = &encode_value_callback;
		send_message.value.arg = &property_encode;
	}
	if (!pb_encode(&stream, emulator_EmulatorMessage_fields, &send_message))
		pr_err("vehicle protocol encode fail\n");
	send_usrmsg(buffer, stream.bytes_written);
exit:
	kfree(buffer);
}
EXPORT_SYMBOL_GPL(vehicle_set_property);

static void netlink_rcv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	char *umsg = NULL;
	char *buffer;
	bool status;
	size_t len;
	int i;
	emulator_EmulatorMessage emulator_message;

	buffer = kmalloc(128, GFP_KERNEL);
	if (skb->len >= nlmsg_total_size(0)) {
		nlh = nlmsg_hdr(skb);
		g_user_pid = nlh->nlmsg_pid;
		umsg = NLMSG_DATA(nlh);
		len = nlh->nlmsg_len - NLMSG_LENGTH(0);
		if (umsg) {
			for (i = 0; i < len; i++)
				pr_debug("%s raw byte %d %d\n", __func__, i, umsg[i]);
			memcpy(buffer, umsg, len);
			pb_istream_t stream = pb_istream_from_buffer(buffer, len);

			emulator_message.prop.funcs.decode = &decode_prop_callback;
			emulator_message.config.funcs.decode = &decode_config_callback;
			emulator_message.value.funcs.decode = &decode_value_callback;
			emulator_message.value.arg = &property_decode;

			status = pb_decode(&stream, emulator_EmulatorMessage_fields,
					   &emulator_message);
			if (!status)
				pr_err("pb_decode failed\n");

			vehicle_send_message_core(property_decode.prop, property_decode.area_id,
						  property_decode.value);
		}
	}
	kfree(buffer);
}

static void create_netlink_vehicle(void)
{
	struct netlink_kernel_cfg cfg = {
		.input  = netlink_rcv_msg,
	};

	nlsk = netlink_kernel_create(&init_net, PROTOCOL_ID, &cfg);
	if (nlsk == NULL) {
		pr_err("netlink_kernel_create error !\n");
		return;
	}
}

static struct vehicle_core_drvdata *
vehicle_get_devtree_pdata(struct device *dev)
{
	struct vehicle_core_drvdata *ddata;

	ddata = devm_kzalloc(dev,
				sizeof(*ddata),
				GFP_KERNEL);

	if (!ddata)
		return ERR_PTR(-ENOMEM);

	return ddata;
}

static struct vehicle *
vehicle_get_dev_pdata(struct device *dev)
{
	struct vehicle *ddata;

	ddata = devm_kzalloc(dev,
			     sizeof(*ddata),
			     GFP_KERNEL);
	if (!ddata)
		return ERR_PTR(-ENOMEM);

	return ddata;
}

static int vehicle_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vehicle_core_drvdata *ddata;

	ddata = vehicle_get_devtree_pdata(dev);
	if (IS_ERR(ddata))
		return PTR_ERR(ddata);

	g_vehicle_hw = vehicle_get_dev_pdata(dev);
	if (IS_ERR(g_vehicle_hw))
		return PTR_ERR(g_vehicle_hw);

	vehicle_core = ddata;
	platform_set_drvdata(pdev, ddata);
	platform_set_drvdata(pdev, g_vehicle_hw);

	create_netlink_vehicle();
	return 0;
}

static int vehicle_remove(struct platform_device *pdev)
{
	if (nlsk)
		netlink_kernel_release(nlsk);
	return 0;
}

static struct platform_driver vehicle_device_driver = {
	.probe          = vehicle_probe,
	.remove         = vehicle_remove,
	.driver         =  {
		.name   = "vehicle-core",
	}
};
static struct platform_device *core_pdev;

static int vehicle_init(void)
{
	int err;

	core_pdev = platform_device_alloc("vehicle-core", -1);
	if (!core_pdev) {
		pr_err("Failed to allocate core vehicle device\n");
		return -ENODEV;
	}

	err = platform_device_add(core_pdev);
	if (err != 0) {
		pr_err("Failed to register core device: %d\n", err);
		platform_device_put(core_pdev);
		return err;
	}

	err = platform_driver_register(&vehicle_device_driver);
	if (err)
		pr_err("Failed to register vehicle driver\n");

	return err;
}

static void __exit vehicle_exit(void)
{
	platform_driver_unregister(&vehicle_device_driver);
	platform_device_unregister(core_pdev);
}

postcore_initcall(vehicle_init);
module_exit(vehicle_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP Semiconductor");
MODULE_DESCRIPTION("VEHICLE core driver");
