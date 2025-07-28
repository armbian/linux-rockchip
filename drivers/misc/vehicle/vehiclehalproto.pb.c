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

#include "vehiclehalproto.pb.h"

const pb_field_t emulator_VehicleAreaConfig_fields[8] = {
	PB_FIELD2(1, INT32, REQUIRED, STATIC, FIRST,
		  emulator_VehicleAreaConfig, area_id, area_id, 0),
	PB_FIELD2(2, SINT32, OPTIONAL, STATIC, OTHER,
		  emulator_VehicleAreaConfig, min_int32_value, area_id, 0),
	PB_FIELD2(3, SINT32, OPTIONAL, STATIC, OTHER,
		  emulator_VehicleAreaConfig, max_int32_value, min_int32_value, 0),
	PB_FIELD2(4, SINT64, OPTIONAL, STATIC, OTHER,
		  emulator_VehicleAreaConfig, min_int64_value, max_int32_value, 0),
	PB_FIELD2(5, SINT64, OPTIONAL, STATIC, OTHER,
		  emulator_VehicleAreaConfig, max_int64_value, min_int64_value, 0),
	PB_FIELD2(6, FLOAT, OPTIONAL, STATIC, OTHER,
		  emulator_VehicleAreaConfig, min_float_value, max_int64_value, 0),
	PB_FIELD2(7, FLOAT, OPTIONAL, STATIC, OTHER,
		  emulator_VehicleAreaConfig, max_float_value, min_float_value, 0),
	PB_LAST_FIELD
};

const pb_field_t emulator_VehiclePropConfig_fields[12] = {
	PB_FIELD2(1, INT32, REQUIRED, STATIC, FIRST,
		  emulator_VehiclePropConfig, prop, prop, 0),
	PB_FIELD2(2, INT32, OPTIONAL, STATIC, OTHER,
		  emulator_VehiclePropConfig, access, prop, 0),
	PB_FIELD2(3, INT32, OPTIONAL, STATIC, OTHER,
		  emulator_VehiclePropConfig, change_mode, access, 0),
	PB_FIELD2(4, INT32, OPTIONAL, STATIC, OTHER,
		  emulator_VehiclePropConfig, value_type, change_mode, 0),
	PB_FIELD2(5, INT32, OPTIONAL, STATIC, OTHER,
		  emulator_VehiclePropConfig, supported_areas, value_type, 0),
	PB_FIELD2(6, MESSAGE, REPEATED, CALLBACK, OTHER, emulator_VehiclePropConfig,
		  area_configs, supported_areas, &emulator_VehicleAreaConfig_fields),
	PB_FIELD2(7, INT32, OPTIONAL, STATIC, OTHER,
		  emulator_VehiclePropConfig, config_flags, area_configs, 0),
	PB_FIELD2(8, INT32, REPEATED, CALLBACK, OTHER,
		  emulator_VehiclePropConfig, config_array, config_flags, 0),
	PB_FIELD2(9, STRING, OPTIONAL, CALLBACK, OTHER,
		  emulator_VehiclePropConfig, config_string, config_array, 0),
	PB_FIELD2(10, FLOAT, OPTIONAL, STATIC, OTHER,
		  emulator_VehiclePropConfig, min_sample_rate, config_string, 0),
	PB_FIELD2(11, FLOAT, OPTIONAL, STATIC, OTHER,
		  emulator_VehiclePropConfig, max_sample_rate, min_sample_rate, 0),
	PB_LAST_FIELD
};

#ifdef CONFIG_VEHICLE_DRIVER_OREO
const pb_field_t emulator_VehiclePropValue_fields[10] = {
	PB_FIELD2(1, INT32, REQUIRED, STATIC, FIRST,
		  emulator_VehiclePropValue, prop, prop, 0),
	PB_FIELD2(2, INT32, OPTIONAL, STATIC, OTHER,
		  emulator_VehiclePropValue, value_type, prop, 0),
	PB_FIELD2(3, INT64, OPTIONAL, STATIC, OTHER,
		  emulator_VehiclePropValue, timestamp, value_type, 0),
	PB_FIELD2(4, INT32, OPTIONAL, STATIC, OTHER,
		  emulator_VehiclePropValue, area_id, timestamp, 0),
	PB_FIELD2(5, SINT32, REPEATED, CALLBACK, OTHER,
		  emulator_VehiclePropValue, int32_values, area_id, 0),
	PB_FIELD2(6, SINT64, REPEATED, CALLBACK, OTHER,
		  emulator_VehiclePropValue, int64_values, int32_values, 0),
	PB_FIELD2(7, FLOAT, REPEATED, CALLBACK, OTHER,
		  emulator_VehiclePropValue, float_values, int64_values, 0),
	PB_FIELD2(8, STRING, OPTIONAL, CALLBACK, OTHER,
		  emulator_VehiclePropValue, string_value, float_values, 0),
	PB_FIELD2(9, BYTES, OPTIONAL, CALLBACK, OTHER,
		  emulator_VehiclePropValue, bytes_value, string_value, 0),
	PB_LAST_FIELD
};
#else
const pb_field_t emulator_VehiclePropValue_fields[11] = {
	PB_FIELD2(1, INT32, REQUIRED, STATIC, FIRST, emulator_VehiclePropValue,
		  prop, prop, 0),
	PB_FIELD2(2, INT32, OPTIONAL, STATIC, OTHER, emulator_VehiclePropValue,
		  value_type, prop, 0),
	PB_FIELD2(3, INT64, OPTIONAL, STATIC, OTHER, emulator_VehiclePropValue,
		  timestamp, value_type, 0),
	PB_FIELD2(4, INT32, OPTIONAL, STATIC, OTHER, emulator_VehiclePropValue,
		  area_id, timestamp, 0),
	PB_FIELD2(5, SINT32, REPEATED, CALLBACK, OTHER, emulator_VehiclePropValue,
		  int32_values, area_id, 0),
	PB_FIELD2(6, SINT64, REPEATED, CALLBACK, OTHER, emulator_VehiclePropValue,
		  int64_values, int32_values, 0),
	PB_FIELD2(7, FLOAT, REPEATED, CALLBACK, OTHER, emulator_VehiclePropValue,
		  float_values, int64_values, 0),
	PB_FIELD2(8, STRING, OPTIONAL, CALLBACK, OTHER, emulator_VehiclePropValue,
		  string_value, float_values, 0),
	PB_FIELD2(9, BYTES, OPTIONAL, CALLBACK, OTHER, emulator_VehiclePropValue,
		  bytes_value, string_value, 0),
	PB_FIELD2(10, ENUM, OPTIONAL, STATIC, OTHER, emulator_VehiclePropValue,
		  status, bytes_value, 0),
	PB_LAST_FIELD
};
#endif

const pb_field_t emulator_VehiclePropGet_fields[3] = {
	PB_FIELD2(1, INT32, REQUIRED, STATIC, FIRST, emulator_VehiclePropGet,
		  prop, prop, 0),
	PB_FIELD2(2, INT32, OPTIONAL, STATIC, OTHER, emulator_VehiclePropGet,
		  area_id, prop, 0),
	PB_LAST_FIELD
};

const pb_field_t emulator_EmulatorMessage_fields[6] = {
	PB_FIELD2(1, ENUM, REQUIRED, STATIC, FIRST, emulator_EmulatorMessage, msg_type,
		  msg_type, 0),
	PB_FIELD2(2, ENUM, OPTIONAL, STATIC, OTHER, emulator_EmulatorMessage, status,
		  msg_type, 0),
	PB_FIELD2(3, MESSAGE, REPEATED, CALLBACK, OTHER, emulator_EmulatorMessage,
		  prop, status, &emulator_VehiclePropGet_fields),
	PB_FIELD2(4, MESSAGE, REPEATED, CALLBACK, OTHER, emulator_EmulatorMessage,
		  config, prop, &emulator_VehiclePropConfig_fields),
	PB_FIELD2(5, MESSAGE, REPEATED, CALLBACK, OTHER, emulator_EmulatorMessage,
		  value, config, &emulator_VehiclePropValue_fields),
	PB_LAST_FIELD
};

/* Check that field information fits in pb_field_t */
#if !defined(PB_FIELD_32BIT)
/* If you get an error here, it means that you need to define PB_FIELD_32BIT
 * compile-time option. You can do that in pb.h or on compiler command line.
 *
 * The reason you need to do this is that some of your messages contain tag
 * numbers or field sizes that are larger than what can fit in 8 or 16 bit
 * field descriptors.
 */
STATIC_ASSERT((pb_membersize(emulator_VehiclePropConfig, area_configs) < 65536
&& pb_membersize(emulator_EmulatorMessage, prop) < 65536
&& pb_membersize(emulator_EmulatorMessage, config) < 65536
&& pb_membersize(emulator_EmulatorMessage, value) < 65536),
YOU_MUST_DEFINE_PB_FIELD_32BIT_FOR_MESSAGES_emulator_VehicleAreaConfig_emulator_\
VehiclePropConfig_emulator_VehiclePropValue_emulator_VehiclePropGet_emulator_EmulatorMessage)
#endif

#if !defined(PB_FIELD_16BIT) && !defined(PB_FIELD_32BIT)
/* If you get an error here, it means that you need to define PB_FIELD_16BIT
 * compile-time option. You can do that in pb.h or on compiler command line.
 *
 * The reason you need to do this is that some of your messages contain tag
 * numbers or field sizes that are larger than what can fit in the default
 * 8 bit descriptors.
 */
STATIC_ASSERT((pb_membersize(emulator_VehiclePropConfig, area_configs) < 256
&& pb_membersize(emulator_EmulatorMessage, prop) < 256
&& pb_membersize(emulator_EmulatorMessage, config) < 256
&& pb_membersize(emulator_EmulatorMessage, value) < 256),
YOU_MUST_DEFINE_PB_FIELD_16BIT_FOR_MESSAGES_emulator_VehicleAreaConfig_emulator_\
VehiclePropConfig_emulator_VehiclePropValue_emulator_VehiclePropGet_emulator_EmulatorMessage)
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP Semiconductor");
