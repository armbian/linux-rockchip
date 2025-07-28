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

#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "vehicle_core.h"
#include "vehiclehalproto.pb.h"
#include "vehicle_protocol_callback.h"

//VehiclePropertyType
#define VEHICLEPROPERTYTYPE_STRING    1048576
#define VEHICLEPROPERTYTYPE_BOOLEAN   2097152
#define VEHICLEPROPERTYTYPE_INT32     4194304
#define VEHICLEPROPERTYTYPE_INT32_VEC 4259840
#define VEHICLEPROPERTYTYPE_INT64     5242880
#define VEHICLEPROPERTYTYPE_INT64_VEC 5308416
#define VEHICLEPROPERTYTYPE_FLOAT     6291456
#define VEHICLEPROPERTYTYPE_FLOAT_VEC 6356992
#define VEHICLEPROPERTYTYPE_BYTES     7340032
#define VEHICLEPROPERTYTYPE_MIXED     14680064


bool decode_prop_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	emulator_VehiclePropGet propget;

	if (!pb_decode(stream, emulator_VehiclePropGet_fields, &propget)) {
		pr_info("%s decode failed\n", __func__);
		return false;
	}

	return true;
}

bool decode_config_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	emulator_VehiclePropConfig propconfigure;

	propconfigure.area_configs.funcs.decode = &decode_area_configs_callback;
	propconfigure.config_array.funcs.decode = &decode_config_array_callback;
	propconfigure.config_string.funcs.decode = &decode_config_string_callback;

	if (!pb_decode(stream, emulator_VehiclePropGet_fields, &propconfigure))
		return false;

	return true;
}

bool decode_int32_values_callback(pb_istream_t *stream, const pb_field_t *field,
				  void **arg)
{
	uint64_t value;

	if (!pb_decode_svarint(stream, &value))
		return false;
	*(u32 *)*arg = (u32)value;
	return true;
}

bool decode_int64_values_callback(pb_istream_t *stream, const pb_field_t *field,
				  void **arg)
{
	uint64_t value;

	if (!pb_decode_svarint(stream, &value))
		return false;
	*(u64 *)*arg = (u64)value;
	return true;
}

bool decode_float_values_callback(pb_istream_t *stream, const pb_field_t *field,
				  void **arg)
{
	uint32_t value;

	if (!pb_decode_fixed32(stream, &value)) {
		pr_info("float_values_callback failed\n");
		return false;
	}

	*(u32 *)*arg = value;
	return true;
}

bool decode_value_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	struct vehicle_property_set *data = (struct vehicle_property_set *)(*arg);
	emulator_VehiclePropValue propvalue;

	propvalue.int32_values.funcs.decode = &decode_int32_values_callback;
	propvalue.int32_values.arg = &data->value;
	propvalue.float_values.funcs.decode = &decode_float_values_callback;
	propvalue.float_values.arg = &data->value;
	propvalue.string_value.funcs.decode = &decode_config_string_callback;
	propvalue.string_value.arg = &data->value;
	propvalue.int64_values.funcs.decode = &decode_int64_values_callback;
	propvalue.int64_values.arg = &data->value;
	// Callback list must be extended if vhal starts to
	// using new property variable type.

	if (!pb_decode(stream, emulator_VehiclePropValue_fields, &propvalue))
		return false;

	data->prop = propvalue.prop;
	data->area_id = propvalue.area_id;

	return true;
}

bool decode_area_configs_callback(pb_istream_t *stream, const pb_field_t *field,
				  void **arg)
{
	emulator_VehicleAreaConfig vehicle_area;

	if (!pb_decode(stream, emulator_VehicleAreaConfig_fields, &vehicle_area))
		return false;
	return true;
}

bool decode_config_array_callback(pb_istream_t *stream, const pb_field_t *field,
				  void **arg)
{
	uint64_t value;

	if (!pb_decode_varint(stream, &value))
		return false;
	return true;
}

bool decode_config_string_callback(pb_istream_t *stream, const pb_field_t *field,
				   void **arg)
{
	char *buffer;

	buffer = kmalloc(1024, GFP_KERNEL);
	if (!buffer) {
		pr_warn("Failed allocating memory for decode\n");
		return false;
	}

	if (stream->bytes_left >= 1024) {
		kfree(buffer);
		return false;
	}

	if (!pb_read(stream, buffer, stream->bytes_left)) {
		kfree(buffer);
		return false;
	}

	kfree(buffer);

	return true;
}

bool encode_prop_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	emulator_VehiclePropGet propget;

	//need fill arg emulator_VehiclePropGet if have this filed
	if (!pb_encode(stream, emulator_VehiclePropGet_fields, &propget)) {
		pr_info("%s encode failed\n", __func__);
		return false;
	}

	return true;
}

bool encode_area_configs_callback(pb_ostream_t *stream, const pb_field_t *field,
				  void * const *arg)
{
	emulator_VehicleAreaConfig vehicle_area;

	if (!pb_encode(stream, emulator_VehicleAreaConfig_fields, &vehicle_area))
		return false;
	return true;
}

bool encode_config_array_callback(pb_ostream_t *stream, const pb_field_t *field,
				  void * const *arg)
{
	return pb_encode_tag_for_field(stream, field) &&
		pb_encode_fixed32(stream, *arg);
}

bool encode_config_string_callback(pb_ostream_t *stream, const pb_field_t *field,
				   void * const *arg)
{
	return pb_encode_tag_for_field(stream, field) &&
		pb_encode_string(stream, *arg, strlen(*arg));
}

bool encode_config_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	emulator_VehiclePropConfig propconfigure;

	propconfigure.area_configs.funcs.encode = &encode_area_configs_callback;
	propconfigure.config_array.funcs.encode = &encode_config_array_callback;
	propconfigure.config_string.funcs.encode = &encode_config_string_callback;

	if (!pb_encode(stream, emulator_VehiclePropGet_fields, &propconfigure))
		return false;

	return true;
}

bool encode_int32_values_callback(pb_ostream_t *stream, const pb_field_t *field,
				  void * const *arg)
{
	u32 *value = (u32 *)*arg;

	if (!pb_encode_tag_for_field(stream, field))
		return false;

	return pb_encode_svarint(stream, *value);
}

bool encode_power_state_value_callback(pb_ostream_t *stream, const pb_field_t *field,
				       void * const *arg)
{
	u32 *value = (u32 *)*arg;

	if (!pb_encode_tag_for_field(stream, field))
		return false;
	if (!pb_encode_svarint(stream, *value))
		return false;
	value++;
	if (!pb_encode_tag_for_field(stream, field))
		return false;
	if (!pb_encode_svarint(stream, *value))
		return false;

	return true;
}

bool encode_int64_values_callback(pb_ostream_t *stream, const pb_field_t *field,
				  void * const *arg)
{
	u64 *value = (u64 *)*arg;

	if (!pb_encode_tag_for_field(stream, field))
		return false;
	return pb_encode_svarint(stream, *value);
}

bool encode_fix32_values_callback(pb_ostream_t *stream, const pb_field_t *field,
				  void * const *arg)
{
	if (!pb_encode_tag_for_field(stream, field))
		return false;
	return pb_encode_fixed32(stream, (u32 *)*arg);
}

bool encode_power_state_callback(pb_ostream_t *stream, const pb_field_t *field,
				 void * const *arg)
{
	struct vehicle_power_req *data = (struct vehicle_power_req *)(*arg);
	emulator_VehiclePropValue propvalue = {};

	propvalue.prop = data->prop;
	propvalue.has_value_type = true;
	propvalue.has_timestamp = true;
	propvalue.timestamp = 0;
#ifndef CONFIG_VEHICLE_DRIVER_OREO
	propvalue.has_status = true;
	propvalue.status = 0;
#endif
	propvalue.int32_values.funcs.encode = &encode_power_state_value_callback;
	propvalue.int32_values.arg = &data->state;
	propvalue.value_type = VEHICLEPROPERTYTYPE_INT32;

	pb_encode_tag_for_field(stream, field);
	if (!pb_encode_submessage(stream, emulator_VehiclePropValue_fields, &propvalue)) {
		pr_info("%s encode submessage failed\n", __func__);
		return false;
	}
	return true;
}

bool encode_value_callback(pb_ostream_t *stream, const pb_field_t *field,
			   void * const *arg)
{
	struct vehicle_property_set *data = (struct vehicle_property_set *)(*arg);
	emulator_VehiclePropValue propvalue = {};

	propvalue.prop = data->prop;
	propvalue.has_value_type = true;
	propvalue.has_timestamp = true;
	propvalue.timestamp = 0;
	propvalue.has_area_id = true;
	propvalue.area_id = data->area_id;
#ifndef CONFIG_VEHICLE_DRIVER_OREO
	propvalue.has_status = true;
	propvalue.status = 0;
#endif
	if (data->prop == HVAC_TEMPERATURE_SET) {
		propvalue.float_values.funcs.encode = &encode_fix32_values_callback;
		propvalue.float_values.arg = &data->value;
		propvalue.value_type = VEHICLEPROPERTYTYPE_FLOAT;
	} else if (HVAC_FAN_SPEED == data->prop || HVAC_FAN_DIRECTION == data->prop ||
		HVAC_AUTO_ON == data->prop || HVAC_AC_ON == data->prop ||
		HVAC_RECIRC_ON == data->prop || HVAC_POWER_ON == data->prop ||
		HVAC_DEFROSTER == data->prop || TURN_SIGNAL_STATE == data->prop ||
		GEAR_SELECTION == data->prop || HVAC_SEAT_TEMPERATURE == data->prop) {
		propvalue.int32_values.funcs.encode = &encode_int32_values_callback;
		propvalue.int32_values.arg = &data->value;
		propvalue.value_type = VEHICLEPROPERTYTYPE_INT32;
	}

	//propvalue.int64_values.funcs.encode = &encode_int64_values_callback;
	//propvalue.string_value.funcs.encode = &encode_config_string_callback;
	//propvalue.bytes_value.funcs.encode = &encode_config_string_callback;
	pb_encode_tag_for_field(stream, field);
	if (!pb_encode_submessage(stream, emulator_VehiclePropValue_fields, &propvalue)) {
		pr_info("%s encode submessage failed\n", __func__);
		return false;
	}
	return true;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP Semiconductor");

