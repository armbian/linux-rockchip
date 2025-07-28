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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/extcon.h>
#include <linux/extcon-provider.h>
#include <linux/of_platform.h>
#include "vehicle_core.h"
#include "core.h"

/* Miscellaneous external connector */
#define EXTCON_VEHICLE_RPMSG_REGISTER 58
#define EXTCON_VEHICLE_RPMSG_EVENT 59
//VEHICLE is in state parking
#define GEAR_0 1
//VEHICLE is in state reverse
#define GEAR_1 2
//VEHICLE is in state neutral
#define GEAR_3 3
//VEHICLE is in state driver
#define GEAR_2 4
// no turn signal
#define TURN_0 0
// left turn signal
#define TURN_1 1
// right turn signal
#define TURN_2 2

#define POWER_REQ_STATE_ON 0
#define POWER_REQ_STATE_SHUTDOWN_PREPARE 1
#define POWER_REQ_STATE_CANCEL_SHUTDOWN 2
#define POWER_REQ_STATE_FINISHED 3

#define POWER_REQ_PARAM_SHUTDOWN_IMMEDIATELY 1
#define POWER_REQ_PARAM_CAN_SLEEP 2
#define POWER_REQ_PARAM_SHUTDOWN_ONLY 3
#define POWER_REQ_PARAM_SLEEP_IMMEDIATELY 4
#define POWER_REQ_PARAM_HIBERNATE_IMMEDIATELY 5
#define POWER_REQ_PARAM_CAN_HIBERNATE 6

// temperature set from hardware on Android OREO and PIE uses below indexes
#define AC_TEMP_LEFT_INDEX 1
#define AC_TEMP_RIGHT_INDEX 4

// temperature set from APP on Android PIE uses below indexes
#define PIE_AC_TEMP_LEFT_INDEX 49
#define PIE_AC_TEMP_RIGHT_INDEX 68

#ifdef CONFIG_EXTCON
static const unsigned int rk_vehicle_dummy_extcon_register_cables[] = {
	EXTCON_VEHICLE_RPMSG_REGISTER,
	EXTCON_NONE,
};
static const unsigned int rk_vehicle_dummy_extcon_event_cables[] = {
	EXTCON_VEHICLE_RPMSG_EVENT,
	EXTCON_NONE,
};
static struct extcon_dev *rg_edev;
static struct extcon_dev *ev_edev;
#endif

static struct class *vehicle_dummy_class;

static void mcu_set_control_commands(u32 prop, u32 area, u32 value)
{
	pr_debug("%s: prop %d, area %d, value %d\n", __func__, prop, area, value);
	switch (prop) {
	case HVAC_FAN_SPEED:
		pr_info("set fan speed with value %d\n", value);
		g_vehicle_hw->vehicle_data.fan_speed = value;
		break;
	case HVAC_FAN_DIRECTION:
		pr_info("set fan direction with value %d\n", value);
		g_vehicle_hw->vehicle_data.fan_direction = value;
		break;
	case HVAC_AUTO_ON:
		pr_info("set fan auto on with value %d\n", value);
		g_vehicle_hw->vehicle_data.auto_on = value;
		break;
	case HVAC_AC_ON:
		pr_info("set fan ac on with value %d\n", value);
		g_vehicle_hw->vehicle_data.ac_on = value;
		break;
	case HVAC_RECIRC_ON:
		pr_info("set fan recirc on with value %d\n", value);
		g_vehicle_hw->vehicle_data.recirc_on = value;
		break;
	case HVAC_DEFROSTER:
		pr_info("set defroster index %d with value %d\n", area, value);
		if (area == 1)
			g_vehicle_hw->vehicle_data.defrost_left = value;
		else
			g_vehicle_hw->vehicle_data.defrost_right = value;
		break;
	case HVAC_TEMPERATURE_SET:
		pr_info("set temp index %d with value %d\n", area, value);
		if (area == AC_TEMP_LEFT_INDEX)
			g_vehicle_hw->vehicle_data.temp_left = value;
		else if (area == AC_TEMP_RIGHT_INDEX)
			g_vehicle_hw->vehicle_data.temp_right = value;
		break;
	case HVAC_POWER_ON:
		pr_info("set hvac power on with value %d\n", value);
		g_vehicle_hw->vehicle_data.hvac_on = value;
		break;
	case AP_POWER_STATE_REPORT:
		pr_info("receive power state report with value %d\n", value);
		break;
	case AP_POWER_STATE_REQ:
		// Proper action is TBD
		break;
	case WATCHDOG_ALIVE:
	case DISPLAY_BRIGHTNESS:
		// Proper action is TBD
		break;
	case HVAC_SEAT_TEMPERATURE:
		pr_info("set seat temperature index %d with value %d\n", area, value);
		if (area == SEAT_TEMP_LEFT_INDEX)
			g_vehicle_hw->vehicle_data.seat_temp_left = value;
		else if (area == SEAT_TEMP_RIGHT_INDEX)
			g_vehicle_hw->vehicle_data.seat_temp_right = value;
		else
			pr_err("unknown index: %d:%d:%d!\n", prop, area, value);
		break;
	case GEAR_SELECTION:
	case TURN_SIGNAL_STATE:
	case CURRENT_GEAR:
		// GEAR is handled in mxc vehicle driver
		break;
	case INFO_MAKE:
	case INFO_MODEL:
	case POWER_POLICY_REQ:
	case FUEL_DOOR_OPEN:
	case EV_CHARGE_PORT_OPEN:
	case EV_CHARGE_PORT_CONNECTED:
	case PARKING_BRAKE_ON:
	case FUEL_LEVEL_LOW:
	case NIGHT_MODE:
	case ABS_ACTIVE:
	case TRACTION_CONTROL_ACTIVE:
	case SUPPORT_CUSTOMIZE_VENDOR_PERMISSION:
	case INFO_MODEL_YEAR:
	case INFO_FUEL_DOOR_LOCATION:
	case INFO_EV_PORT_LOCATION:
	case ENGINE_OIL_LEVEL:
	case IGNITION_STATE:
	case HVAC_STEERING_WHEEL_HEAT:
	case HVAC_TEMPERATURE_DISPLAY_UNITS:
	case DISTANCE_DISPLAY_UNITS:
	case TIRE_PRESSURE_DISPLAY_UNITS:
	case VEHICLE_SPEED_DISPLAY_UNITS:
	case HEADLIGHTS_STATE:
	case HIGH_BEAM_LIGHTS_STATE:
	case FOG_LIGHTS_STATE:
	case PARKING_BRAKE_AUTO_APPLY:
	case HAZARD_LIGHTS_STATE:
	case HEADLIGHTS_SWITCH:
	case HIGH_BEAM_LIGHTS_SWITCH:
	case FOG_LIGHTS_SWITCH:
	case HAZARD_LIGHTS_SWITCH:
	case CLUSTER_SWITCH_UI:
	case CLUSTER_REQUEST_DISPLAY:
	case POWER_POLICY_GROUP_REQ:
	case CURRENT_POWER_POLICY:
	case ELECTRONIC_TOLL_COLLECTION_CARD_TYPE:
	case ELECTRONIC_TOLL_COLLECTION_CARD_STATUS:
	case INFO_FUEL_TYPE:
	case INFO_EV_CONNECTOR_TYPE:
	case INFO_DRIVER_SEAT:
	case INFO_EXTERIOR_DIMENSIONS:
	case INFO_MULTI_EV_PORT_LOCATIONS:
	case HW_KEY_INPUT:
	case HW_ROTARY_INPUT:
	case HW_CUSTOM_INPUT:
	case EVS_SERVICE_REQUEST:
	case CLUSTER_DISPLAY_STATE:
	case EPOCH_TIME:
	case VHAL_HEARTBEAT:
	case WHEEL_TICK:
	case INFO_FUEL_CAPACITY:
	case INFO_EV_BATTERY_CAPACITY:
	case PERF_ODOMETER:
	case PERF_VEHICLE_SPEED:
	case PERF_STEERING_ANGLE:
	case PERF_REAR_STEERING_ANGLE:
	case ENGINE_OIL_TEMP:
	case ENGINE_RPM:
	case FUEL_LEVEL:
	case RANGE_REMAINING:
	case EV_BATTERY_LEVEL:
	case EV_BATTERY_INSTANTANEOUS_CHARGE_RATE:
	case ENV_OUTSIDE_TEMPERATURE:
	case HVAC_TEMPERATURE_VALUE_SUGGESTION:
	case STORAGE_ENCRYPTION_BINDING_SEED:
	case CLUSTER_NAVIGATION_STATE:
	case INITIAL_USER_INFO:
	case SWITCH_USER:
	case CREATE_USER:
	case REMOVE_USER:
	case USER_IDENTIFICATION_ASSOCIATION:
	case WATCHDOG_TERMINATED_PROCESS:
	case CLUSTER_REPORT_STATE:
	case WINDOW_LOCK:
	case VEHICLE_MAP_SERVICE:
	case WINDOW_POS:
	case HVAC_MAX_AC_ON:
	case HVAC_MAX_DEFROST_ON:
	case HVAC_DUAL_ON:
	case HVAC_AUTO_RECIRC_ON:
	case HVAC_SEAT_VENTILATION:
	case HVAC_ELECTRIC_DEFROSTER_ON:
	case SEAT_OCCUPANCY:
	case HVAC_FAN_DIRECTION_AVAILABLE:
	case DOOR_LOCK:
	case DOOR_POS:
	case TIRE_PRESSURE:
	case CRITICALLY_LOW_TIRE_PRESSURE:
	case FRONT_FOG_LIGHTS_STATE:
	case FRONT_FOG_LIGHTS_SWITCH:
	case REAR_FOG_LIGHTS_STATE:
	case REAR_FOG_LIGHTS_SWITCH:
	case EV_CHARGE_CURRENT_DRAW_LIMIT:
	case EV_CHARGE_PERCENT_LIMIT:
	case EV_CHARGE_STATE:
	case EV_CHARGE_SWITCH:
	case EV_CHARGE_TIME_REMAINING:
	case EV_REGENERATIVE_BRAKING_STATE:
	case TRAILER_PRESENT:
	case VEHICLE_CURB_WEIGHT:
		// Proper action is TBD
		break;
	case VENDOR_EXTENSION_STRING_PROPERTY:
	case VENDOR_EXTENSION_BOOLEAN_PROPERTY:
	case VENDOR_EXTENSION_FLOAT_PROPERTY:
	case VENDOR_EXTENSION_INT_PROPERTY:
	case READING_LIGHTS_SWITCH:
	case SEAT_BELT_HEIGHT_POS:
	case WINDOW_MOVE:
	case MIRROR_AUTO_TILT_ENABLED:
	case SEAT_FORE_AFT_POS:
	case EV_BATTERY_DISPLAY_UNITS:
	case SEAT_HEIGHT_MOVE:
	case INFO_VIN:
	case SEAT_HEADREST_ANGLE_POS:
	case SEAT_HEADREST_ANGLE_MOVE:
	case ENGINE_COOLANT_TEMP:
	case MIRROR_LOCK:
	case HVAC_ACTUAL_FAN_SPEED_RPM:
	case SEAT_BELT_HEIGHT_MOVE:
	case SEAT_FORE_AFT_MOVE:
	case SEAT_BACKREST_ANGLE_2_MOVE:
	case SEAT_HEIGHT_POS:
	case SEAT_TILT_POS:
	case SEAT_DEPTH_MOVE:
	case SEAT_LUMBAR_FORE_AFT_POS:
	case SEAT_EASY_ACCESS_ENABLED:
	case SEAT_LUMBAR_FORE_AFT_MOVE:
	case SEAT_AIRBAG_ENABLED:
	case AUTOMATIC_EMERGENCY_BRAKING_ENABLED:
	case SEAT_LUMBAR_SIDE_SUPPORT_POS:
	case SEAT_LUMBAR_SIDE_SUPPORT_MOVE:
	case SEAT_MEMORY_SELECT:
	case HANDS_ON_DETECTION_DRIVER_STATE:
	case SEAT_HEADREST_HEIGHT_MOVE:
	case SEAT_HEADREST_FORE_AFT_POS:
	case MIRROR_FOLD:
	case SEAT_HEADREST_FORE_AFT_MOVE:
	case GLOVE_BOX_LOCKED:
	case SEAT_TILT_MOVE:
	case MIRROR_Z_POS:
	case HVAC_SIDE_MIRROR_HEAT:
	case SEAT_LUMBAR_VERTICAL_POS:
	case SEAT_FOOTWELL_LIGHTS_SWITCH:
	case WINDSHIELD_WIPERS_STATE:
	case SEAT_DEPTH_POS:
	case SEAT_HEADREST_HEIGHT_POS_V2:
	case EV_CURRENT_BATTERY_CAPACITY:
	case EV_BRAKE_REGENERATION_LEVEL:
	case SEAT_LUMBAR_VERTICAL_MOVE:
	case SEAT_WALK_IN_POS:
	case SEAT_BELT_BUCKLED:
	case SEAT_BACKREST_ANGLE_1_POS:
	case LANE_KEEP_ASSIST_STATE:
	case LANE_CENTERING_ASSIST_ENABLED:
	case SEAT_BACKREST_ANGLE_1_MOVE:
	case SEAT_BACKREST_ANGLE_2_POS:
	case MIRROR_AUTO_FOLD_ENABLED:
	case BLIND_SPOT_WARNING_ENABLED:
	case EV_STOPPING_MODE:
	case STEERING_WHEEL_LIGHTS_STATE:
	case MIRROR_Z_MOVE:
	case SEAT_CUSHION_SIDE_SUPPORT_POS:
	case MIRROR_Y_MOVE:
	case AUTOMATIC_EMERGENCY_BRAKING_STATE:
	case FORWARD_COLLISION_WARNING_ENABLED:
	case SEAT_MEMORY_SET:
	case HANDS_ON_DETECTION_WARNING:
	case WINDSHIELD_WIPERS_PERIOD:
	case SEAT_FOOTWELL_LIGHTS_STATE:
	case DOOR_CHILD_LOCK_ENABLED:
	case DOOR_MOVE:
	case WINDSHIELD_WIPERS_SWITCH:
	case STEERING_WHEEL_DEPTH_POS:
	case GLOVE_BOX_DOOR_POS:
	case STEERING_WHEEL_HEIGHT_POS:
	case STEERING_WHEEL_THEFT_LOCK_ENABLED:
	case SEAT_CUSHION_SIDE_SUPPORT_MOVE:
	case CABIN_LIGHTS_STATE:
	case STEERING_WHEEL_EASY_ACCESS_ENABLED:
	case CABIN_LIGHTS_SWITCH:
	case STEERING_WHEEL_HEIGHT_MOVE:
	case STEERING_WHEEL_LIGHTS_SWITCH:
	case FUEL_CONSUMPTION_UNITS_DISTANCE_OVER_VOLUME:
	case MIRROR_Y_POS:
	case LOCATION_CHARACTERIZATION:
	case EMERGENCY_LANE_KEEP_ASSIST_STATE:
	case CRUISE_CONTROL_TYPE:
	case CRUISE_CONTROL_STATE:
	case STEERING_WHEEL_LOCKED:
	case ADAPTIVE_CRUISE_CONTROL_TARGET_TIME_GAP:
	case ADAPTIVE_CRUISE_CONTROL_LEAD_VEHICLE_MEASURED_DISTANCE:
	case HANDS_ON_DETECTION_ENABLED:
	case FORWARD_COLLISION_WARNING_STATE:
	case STEERING_WHEEL_DEPTH_MOVE:
	case EMERGENCY_LANE_KEEP_ASSIST_ENABLED:
	case GENERAL_SAFETY_REGULATION_COMPLIANCE:
	case LANE_CENTERING_ASSIST_STATE:
	case CRUISE_CONTROL_ENABLED:
	case PERF_VEHICLE_SPEED_DISPLAY:
	case HVAC_TEMPERATURE_CURRENT:
	case LANE_KEEP_ASSIST_ENABLED:
	case READING_LIGHTS_STATE:
	case CRUISE_CONTROL_TARGET_SPEED:
	case ENGINE_IDLE_AUTO_STOP_ENABLED:
	case BLIND_SPOT_WARNING_STATE:
	case LANE_DEPARTURE_WARNING_ENABLED:
	case FUEL_VOLUME_DISPLAY_UNITS:
	case LANE_DEPARTURE_WARNING_STATE:
	case kMixedTypePropertyForTest:
		// Proper action is TBD
		break;
	default:
		pr_err("this type is not correct: %d:%d:%d!\n", prop, area, value);
	}
}

static ssize_t turn_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%u\n", g_vehicle_hw->vehicle_data.turn);
}

/* echo 0/1/2(none/left/right) > /sys/devices/platform/vehicle-dummy/turn*/
static ssize_t turn_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t size)
{
	u32 turn;

	if (!size)
		return -EINVAL;
	turn = strtoul(buf, NULL, 10);
	if (turn != TURN_0 && turn != TURN_1 && turn != TURN_2) {
		pr_err("input value is not correct, please type correct one\n");
		return -EINVAL;
	}
	if (turn != g_vehicle_hw->vehicle_data.turn) {
		g_vehicle_hw->vehicle_data.turn = turn;
		vehicle_set_property(VEHICLE_TURN_SIGNAL, 0, turn, 0);
	}
	return size;
}
static DEVICE_ATTR_RW(turn);

static ssize_t gear_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%u\n", g_vehicle_hw->vehicle_data.gear);
}

/*echo 1/2/4(parking/reverse/drive) > /sys/devices/platform/vehicle-dummy/gear*/
static ssize_t gear_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t size)
{
	u32 gear;

	if (!size)
		return -EINVAL;
	gear = strtoul(buf, NULL, 10);
	if (gear != g_vehicle_hw->vehicle_data.gear) {
		g_vehicle_hw->vehicle_data.gear = gear;
		vehicle_set_property(VEHICLE_GEAR, 0, gear, 0);
		if (gear != GEAR_0 && gear != GEAR_1 && gear != GEAR_2) {
			pr_err("input value is not correct, please type correct one\n");
			return -EINVAL;
		}
#ifdef CONFIG_EXTCON
		if (gear == VEHICLE_GEAR_DRIVE)
			extcon_set_state_sync(ev_edev, EXTCON_VEHICLE_RPMSG_EVENT, 0);
		else if (gear == VEHICLE_GEAR_REVERSE)
			extcon_set_state_sync(ev_edev, EXTCON_VEHICLE_RPMSG_EVENT, 1);
#endif
	}
	return size;
}

static DEVICE_ATTR_RW(gear);

static ssize_t temp_left_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%d\n", g_vehicle_hw->vehicle_data.temp_left);
}

/*echo 1100713529 > /sys/devices/platform/vehicle-dummy/temp_left*/
static ssize_t temp_left_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t size)
{
	u32 temp;

	if (!size)
		return -EINVAL;
	temp = strtoul(buf, NULL, 10);
	if (temp != g_vehicle_hw->vehicle_data.temp_left) {
		g_vehicle_hw->vehicle_data.temp_left = temp;
		vehicle_set_property(VEHICLE_AC_TEMP, AC_TEMP_LEFT_INDEX, temp, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(temp_left);

static ssize_t temp_right_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	return sprintf(buf, "%d\n", g_vehicle_hw->vehicle_data.temp_right);
}

/*echo 1100713529 > /sys/devices/platform/vehicle-dummy/temp_right*/
static ssize_t temp_right_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	u32 temp;

	if (!size)
		return -EINVAL;
	temp = strtoul(buf, NULL, 10);
	if (temp != g_vehicle_hw->vehicle_data.temp_right) {
		g_vehicle_hw->vehicle_data.temp_right = temp;
		vehicle_set_property(VEHICLE_AC_TEMP, AC_TEMP_RIGHT_INDEX, temp, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(temp_right);

static ssize_t seat_temp_left_show(struct device *dev, struct device_attribute *attr,
				   char *buf)
{
	return sprintf(buf, "%d\n", g_vehicle_hw->vehicle_data.seat_temp_left);
}

/*echo 0/1/2/3 > /sys/devices/platform/vehicle-dummy/seat_temp_left*/
static ssize_t seat_temp_left_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	u32 seat_temp;

	if (!size)
		return -EINVAL;
	seat_temp = strtoul(buf, NULL, 10);

	if (seat_temp != g_vehicle_hw->vehicle_data.seat_temp_left) {
		g_vehicle_hw->vehicle_data.seat_temp_left = seat_temp;
		vehicle_set_property(VEHICLE_SEAT_TEMPERATURE, SEAT_TEMP_LEFT_INDEX, seat_temp, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(seat_temp_left);

static ssize_t seat_temp_right_show(struct device *dev, struct device_attribute *attr,
				    char *buf)
{
	return sprintf(buf, "%d\n", g_vehicle_hw->vehicle_data.seat_temp_right);
}

/*echo 0/1/2/3 > /sys/devices/platform/vehicle-dummy/seat_temp_right*/
static ssize_t seat_temp_right_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	u32 seat_temp;

	if (!size)
		return -EINVAL;
	seat_temp = strtoul(buf, NULL, 10);
	if (seat_temp != SEAT_TEMP_0 && seat_temp != SEAT_TEMP_1 &&
		seat_temp != SEAT_TEMP_2 && seat_temp != SEAT_TEMP_3) {
		pr_err("input value is not correct, please type correct one\n");
		return -EINVAL;
	}
	if (seat_temp != g_vehicle_hw->vehicle_data.seat_temp_right) {
		g_vehicle_hw->vehicle_data.seat_temp_right = seat_temp;
		vehicle_set_property(VEHICLE_SEAT_TEMPERATURE, SEAT_TEMP_RIGHT_INDEX, seat_temp, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(seat_temp_right);

static ssize_t fan_direction_show(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "%u\n", g_vehicle_hw->vehicle_data.fan_direction);
}

/*echo 1/2/3/6 > /sys/devices/platform/vehicle-dummy/fan_direction*/
static ssize_t fan_direction_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t size)
{
	u32 fan_direction;

	if (!size)
		return -EINVAL;
	fan_direction = strtoul(buf, NULL, 10);

	if (fan_direction != g_vehicle_hw->vehicle_data.fan_direction) {
		g_vehicle_hw->vehicle_data.fan_direction = fan_direction;
		vehicle_set_property(VEHICLE_FAN_DIRECTION, 0, fan_direction, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(fan_direction);

static ssize_t fan_speed_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%u\n", g_vehicle_hw->vehicle_data.fan_speed);
}

/*echo 1/2/3/4/5/6 > /sys/devices/platform/vehicle-dummy/fan_speed*/
static ssize_t fan_speed_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t size)
{
	u32 fan_speed;

	if (!size)
		return -EINVAL;
	fan_speed = strtoul(buf, NULL, 10);

	if (fan_speed != g_vehicle_hw->vehicle_data.fan_speed) {
		g_vehicle_hw->vehicle_data.fan_speed = fan_speed;
		vehicle_set_property(VEHICLE_FAN_SPEED, 0, fan_speed, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(fan_speed);

static ssize_t defrost_left_show(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	return sprintf(buf, "%u\n", g_vehicle_hw->vehicle_data.defrost_left);
}

/*echo 0/1 > /sys/devices/platform/vehicle-dummy/defrost_left*/
static ssize_t defrost_left_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
{
	u32 defrost;

	if (!size)
		return -EINVAL;

	defrost = strtoul(buf, NULL, 10);

	if (defrost != g_vehicle_hw->vehicle_data.defrost_left) {
		g_vehicle_hw->vehicle_data.defrost_left = defrost;
		vehicle_set_property(VEHICLE_DEFROST, 1, defrost, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(defrost_left);

static ssize_t defrost_right_show(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "%u\n", g_vehicle_hw->vehicle_data.defrost_right);
}

/*echo 0/1 > /sys/devices/platform/vehicle-dummy/defrost_right*/
static ssize_t defrost_right_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t size)
{
	u32 defrost;

	if (!size)
		return -EINVAL;

	defrost = strtoul(buf, NULL, 10);
	if (defrost != DEFROST_ON && defrost != DEFROST_OFF) {
		pr_err("input value is not correct, please type correct one\n");
		return -EINVAL;
	}
	if (defrost != g_vehicle_hw->vehicle_data.defrost_right) {
		g_vehicle_hw->vehicle_data.defrost_right = defrost;
		vehicle_set_property(VEHICLE_DEFROST, 2, defrost, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(defrost_right);

static ssize_t ac_on_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%u\n", g_vehicle_hw->vehicle_data.ac_on);
}

/*echo 0/1 > /sys/devices/platform/vehicle-dummy/ac_on*/
static ssize_t ac_on_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	u32 ac_on;

	if (!size)
		return -EINVAL;

	ac_on = strtoul(buf, NULL, 10);

	if (ac_on != g_vehicle_hw->vehicle_data.ac_on) {
		g_vehicle_hw->vehicle_data.ac_on = ac_on;
		vehicle_set_property(VEHICLE_AC, 0, ac_on, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(ac_on);

static ssize_t auto_on_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%u\n", g_vehicle_hw->vehicle_data.auto_on);
}

/*echo 0/1 > /sys/devices/platform/vehicle-dummy/auto_on*/
static ssize_t auto_on_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	u32 auto_on;

	if (!size)
		return -EINVAL;

	auto_on = strtoul(buf, NULL, 10);

	if (auto_on != g_vehicle_hw->vehicle_data.auto_on) {
		g_vehicle_hw->vehicle_data.auto_on = auto_on;
		vehicle_set_property(VEHICLE_AUTO_ON, VEHICLE_AREA_SEAT_ROW_1_LEFT,
					 auto_on, 0);
		vehicle_set_property(VEHICLE_AUTO_ON, VEHICLE_AREA_SEAT_ROW_1_RIGHT,
					 auto_on, 0);
		vehicle_set_property(VEHICLE_AUTO_ON, VEHICLE_AREA_SEAT_ROW_2_LEFT,
					 auto_on, 0);
		vehicle_set_property(VEHICLE_AUTO_ON, VEHICLE_AREA_SEAT_ROW_2_RIGHT,
					 auto_on, 0);
		vehicle_set_property(VEHICLE_AUTO_ON, VEHICLE_AREA_SEAT_ROW_2_CENTER,
					 auto_on, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(auto_on);

static ssize_t hvac_on_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%u\n", g_vehicle_hw->vehicle_data.hvac_on);
}

/*echo 0/1 > /sys/devices/platform/vehicle-dummy/hvac_on*/
static ssize_t hvac_on_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	u32 hvac_on;

	if (!size)
		return -EINVAL;

	hvac_on = strtoul(buf, NULL, 10);

	if (hvac_on != g_vehicle_hw->vehicle_data.hvac_on) {
		g_vehicle_hw->vehicle_data.hvac_on = hvac_on;
		vehicle_set_property(VEHICLE_HVAC_POWER_ON, VEHICLE_AREA_SEAT_ROW_1_LEFT,
					 hvac_on, 0);
		vehicle_set_property(VEHICLE_HVAC_POWER_ON, VEHICLE_AREA_SEAT_ROW_1_RIGHT,
					 hvac_on, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(hvac_on);

static ssize_t recirc_on_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%u\n", g_vehicle_hw->vehicle_data.recirc_on);
}

/* echo 0/1 > /sys/devices/platform/vehicle-dummy/recirc_on*/
static ssize_t recirc_on_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t size)
{
	u32 recirc_on;

	if (!size)
		return -EINVAL;

	recirc_on = strtoul(buf, NULL, 10);

	if (recirc_on != g_vehicle_hw->vehicle_data.recirc_on) {
		g_vehicle_hw->vehicle_data.recirc_on = recirc_on;
		vehicle_set_property(VEHICLE_RECIRC_ON, 0, recirc_on, 0);
	}
	return size;
}

static DEVICE_ATTR_RW(recirc_on);

static ssize_t power_req_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%u %u\n", g_vehicle_hw->vehicle_data.power_req_state,
		       g_vehicle_hw->vehicle_data.power_req_param);
}

/* echo "1 1" > /sys/devices/platform/vehicle-dummy/power_req */
static ssize_t power_req_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t size)
{
	char *ret;
	u32 state;
	u32 param;

	if (size < 4) {
		pr_err("input command format is not correct, please type command like this: \"1 1\"\n");
		return -EINVAL;
	}

	state = strtoul(buf, NULL, 10);
	ret = strrchr(buf, ' ');
	param = strtoul(ret + 1, NULL, 10);

	if (state != POWER_REQ_STATE_ON &&
		state != POWER_REQ_STATE_SHUTDOWN_PREPARE &&
		state != POWER_REQ_STATE_CANCEL_SHUTDOWN &&
		state != POWER_REQ_STATE_FINISHED) {
		pr_err("input power request state is not correct, please type correct one\n");
		return -EINVAL;
	}

	if (param != POWER_REQ_PARAM_SHUTDOWN_IMMEDIATELY &&
		param != POWER_REQ_PARAM_CAN_SLEEP &&
		param != POWER_REQ_PARAM_SHUTDOWN_ONLY &&
		param != POWER_REQ_PARAM_SLEEP_IMMEDIATELY &&
		param != POWER_REQ_PARAM_HIBERNATE_IMMEDIATELY &&
		param != POWER_REQ_PARAM_CAN_HIBERNATE) {
		pr_err("input power request param is not correct, please type correct one\n");
		return -EINVAL;
	}

	g_vehicle_hw->vehicle_data.power_req_state = state;
	g_vehicle_hw->vehicle_data.power_req_param = param;
	vehicle_set_property(VEHICLE_POWER_STATE_REQ, 0, state, param);
	pr_info("power control with state: %d, param: %d\n", state, param);
	return size;
}

static DEVICE_ATTR_RW(power_req);

static const struct hw_prop_ops hw_prop_mcu_ops = {
	.set_control_commands = mcu_set_control_commands,
};

static int vehicle_dummy_hw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vehicle_dummy *vehicle_dummy;
	int err;

	if (IS_ERR(g_vehicle_hw))
		return PTR_ERR(g_vehicle_hw);

	vehicle_dummy = devm_kzalloc(dev, sizeof(*vehicle_dummy), GFP_KERNEL);
	if (vehicle_dummy == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, vehicle_dummy);

	vehicle_dummy_class = class_create(THIS_MODULE, "vehicle_dummy_hw");
	if (IS_ERR(vehicle_dummy_class)) {
		dev_err(dev, "failed to create class.\n");
		return PTR_ERR(vehicle_dummy_class);
	}

	err = device_create_file(dev, &dev_attr_recirc_on) ||
		device_create_file(dev, &dev_attr_hvac_on) ||
		device_create_file(dev, &dev_attr_auto_on) ||
		device_create_file(dev, &dev_attr_ac_on) ||
		device_create_file(dev, &dev_attr_defrost_right) ||
		device_create_file(dev, &dev_attr_defrost_left) ||
		device_create_file(dev, &dev_attr_fan_speed) ||
		device_create_file(dev, &dev_attr_fan_direction) ||
		device_create_file(dev, &dev_attr_temp_left) ||
		device_create_file(dev, &dev_attr_temp_right) ||
		device_create_file(dev, &dev_attr_gear) ||
		device_create_file(dev, &dev_attr_power_req) ||
		device_create_file(dev, &dev_attr_seat_temp_left) ||
		device_create_file(dev, &dev_attr_seat_temp_right) ||
		device_create_file(dev, &dev_attr_turn);
	if (err)
		return err;

	vehicle_hw_prop_ops_register(&hw_prop_mcu_ops);

#ifdef CONFIG_EXTCON
	rg_edev = devm_extcon_dev_allocate(dev, rk_vehicle_dummy_extcon_register_cables);
	if (IS_ERR(rg_edev))
		dev_err(dev, "failed to allocate extcon device\n");

	err = devm_extcon_dev_register(dev, rg_edev);
	if (err < 0)
		dev_err(dev, "failed to register extcon device\n");

	ev_edev = devm_extcon_dev_allocate(dev, rk_vehicle_dummy_extcon_event_cables);
	if (IS_ERR(ev_edev))
		dev_err(dev, "failed to allocate extcon device\n");

	err = devm_extcon_dev_register(dev, ev_edev);
	if (err < 0)
		dev_err(dev, "failed to register extcon device\n");
#endif

	return 0;
}

static int vehicle_dummy_hw_remove(struct platform_device *pdev)
{
	class_destroy(vehicle_dummy_class);

	return 0;
}

static struct platform_driver vehicle_dummy_hw_driver = {
	.probe          = vehicle_dummy_hw_probe,
	.remove         = vehicle_dummy_hw_remove,
	.driver         =  {
		.name   = "vehicle-dummy-hw",
	}
};

static struct platform_device *dummy_pdev;

static int vehicle_dummy_init(void)
{
	int err;

	dummy_pdev = platform_device_alloc("vehicle-dummy-hw", -1);
	if (!dummy_pdev) {
		pr_err("Failed to allocate dummy vehicle device\n");
		return -ENODEV;
	}

	err = platform_device_add(dummy_pdev);
	if (err != 0) {
		pr_err("Failed to register dummy regulator device: %d\n", err);
		platform_device_put(dummy_pdev);
		return err;
	}

	err = platform_driver_register(&vehicle_dummy_hw_driver);
	if (err) {
		pr_err("Failed to register dummy vehicle driver\n");
		platform_device_unregister(dummy_pdev);
		return err;
	}
	return 0;
}

static void __exit vehicle_dummy_exit(void)
{
	platform_driver_unregister(&vehicle_dummy_hw_driver);
	platform_device_unregister(dummy_pdev);
}

late_initcall(vehicle_dummy_init);
module_exit(vehicle_dummy_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP Semiconductor");
MODULE_DESCRIPTION("VEHICLE DUMMY HW");
