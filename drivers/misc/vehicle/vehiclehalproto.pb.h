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

#ifndef _PB_VEHICLEHALPROTO_PB_H_
#define _PB_VEHICLEHALPROTO_PB_H_
#include <pb.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
typedef enum _emulator_MsgType {
	emulator_MsgType_GET_CONFIG_CMD = 0,
	emulator_MsgType_GET_CONFIG_RESP = 1,
	emulator_MsgType_GET_CONFIG_ALL_CMD = 2,
	emulator_MsgType_GET_CONFIG_ALL_RESP = 3,
	emulator_MsgType_GET_PROPERTY_CMD = 4,
	emulator_MsgType_GET_PROPERTY_RESP = 5,
	emulator_MsgType_GET_PROPERTY_ALL_CMD = 6,
	emulator_MsgType_GET_PROPERTY_ALL_RESP = 7,
	emulator_MsgType_SET_PROPERTY_CMD = 8,
	emulator_MsgType_SET_PROPERTY_RESP = 9,
	emulator_MsgType_SET_PROPERTY_ASYNC = 10
} emulator_MsgType;

typedef enum _emulator_Status {
	emulator_Status_RESULT_OK = 0,
	emulator_Status_ERROR_UNKNOWN = 1,
	emulator_Status_ERROR_UNIMPLEMENTED_CMD = 2,
	emulator_Status_ERROR_INVALID_PROPERTY = 3,
	emulator_Status_ERROR_INVALID_AREA_ID = 4,
	emulator_Status_ERROR_PROPERTY_UNINITIALIZED = 5,
	emulator_Status_ERROR_WRITE_ONLY_PROPERTY = 6,
	emulator_Status_ERROR_MEMORY_ALLOC_FAILED = 7,
	emulator_Status_ERROR_INVALID_OPERATION = 8
} emulator_Status;

#ifndef CONFIG_VEHICLE_DRIVER_OREO
typedef enum _emulator_VehiclePropStatus {
	emulator_VehiclePropStatus_AVAILABLE = 0,
	emulator_VehiclePropStatus_UNAVAILABLE = 1,
	emulator_VehiclePropStatus_ERROR = 2
} emulator_VehiclePropStatus;
#endif

/* Struct definitions */
typedef struct _emulator_VehicleAreaConfig {
	int32_t area_id;
	bool has_min_int32_value;
	int32_t min_int32_value;
	bool has_max_int32_value;
	int32_t max_int32_value;
	bool has_min_int64_value;
	int64_t min_int64_value;
	bool has_max_int64_value;
	int64_t max_int64_value;
	bool has_min_float_value;
	float min_float_value;
	bool has_max_float_value;
	float max_float_value;
} emulator_VehicleAreaConfig;

typedef struct _emulator_VehiclePropGet {
	int32_t prop;
	bool has_area_id;
	int32_t area_id;
} emulator_VehiclePropGet;

typedef struct _emulator_VehiclePropValue {
	int32_t prop;
	bool has_value_type;
	int32_t value_type;
	bool has_timestamp;
	int64_t timestamp;
	bool has_area_id;
	int32_t area_id;
	pb_callback_t int32_values;
	pb_callback_t int64_values;
	pb_callback_t float_values;
	pb_callback_t string_value;
	pb_callback_t bytes_value;
#ifndef CONFIG_VEHICLE_DRIVER_OREO
	bool has_status;
	emulator_VehiclePropStatus status;
#endif
} emulator_VehiclePropValue;

typedef struct _emulator_VehiclePropConfig {
	int32_t prop;
	bool has_access;
	int32_t access;
	bool has_change_mode;
	int32_t change_mode;
	bool has_value_type;
	int32_t value_type;
	bool has_supported_areas;
	int32_t supported_areas;
	pb_callback_t area_configs;
	bool has_config_flags;
	int32_t config_flags;
	pb_callback_t config_array;
	pb_callback_t config_string;
	bool has_min_sample_rate;
	float min_sample_rate;
	bool has_max_sample_rate;
	float max_sample_rate;
} emulator_VehiclePropConfig;

typedef struct _emulator_EmulatorMessage {
	emulator_MsgType msg_type;
	bool has_status;
	emulator_Status status;
	pb_callback_t prop;
	pb_callback_t config;
	pb_callback_t value;
} emulator_EmulatorMessage;

/* Default values for struct fields */

/* Field tags (for use in manual encoding/decoding) */
#define emulator_VehicleAreaConfig_area_id_tag   1
#define emulator_VehicleAreaConfig_min_int32_value_tag 2
#define emulator_VehicleAreaConfig_max_int32_value_tag 3
#define emulator_VehicleAreaConfig_min_int64_value_tag 4
#define emulator_VehicleAreaConfig_max_int64_value_tag 5
#define emulator_VehicleAreaConfig_min_float_value_tag 6
#define emulator_VehicleAreaConfig_max_float_value_tag 7
#define emulator_VehiclePropGet_prop_tag         1
#define emulator_VehiclePropGet_area_id_tag      2
#define emulator_VehiclePropValue_prop_tag       1
#define emulator_VehiclePropValue_value_type_tag 2
#define emulator_VehiclePropValue_timestamp_tag  3
#ifndef CONFIG_VEHICLE_DRIVER_OREO
#define emulator_VehiclePropValue_status_tag     10
#endif
#define emulator_VehiclePropValue_area_id_tag    4
#define emulator_VehiclePropValue_int32_values_tag 5
#define emulator_VehiclePropValue_int64_values_tag 6
#define emulator_VehiclePropValue_float_values_tag 7
#define emulator_VehiclePropValue_string_value_tag 8
#define emulator_VehiclePropValue_bytes_value_tag 9
#define emulator_VehiclePropConfig_prop_tag      1
#define emulator_VehiclePropConfig_access_tag    2
#define emulator_VehiclePropConfig_change_mode_tag 3
#define emulator_VehiclePropConfig_value_type_tag 4
#define emulator_VehiclePropConfig_supported_areas_tag 5
#define emulator_VehiclePropConfig_area_configs_tag 6
#define emulator_VehiclePropConfig_config_flags_tag 7
#define emulator_VehiclePropConfig_config_array_tag 8
#define emulator_VehiclePropConfig_config_string_tag 9
#define emulator_VehiclePropConfig_min_sample_rate_tag 10
#define emulator_VehiclePropConfig_max_sample_rate_tag 11
#define emulator_EmulatorMessage_msg_type_tag    1
#define emulator_EmulatorMessage_status_tag      2
#define emulator_EmulatorMessage_prop_tag        3
#define emulator_EmulatorMessage_config_tag      4
#define emulator_EmulatorMessage_value_tag       5

/* Struct field encoding specification for nanopb */
extern const pb_field_t emulator_VehicleAreaConfig_fields[8];
extern const pb_field_t emulator_VehiclePropConfig_fields[12];
#ifdef CONFIG_VEHICLE_DRIVER_OREO
extern const pb_field_t emulator_VehiclePropValue_fields[10];
#else
extern const pb_field_t emulator_VehiclePropValue_fields[11];
#endif
extern const pb_field_t emulator_VehiclePropGet_fields[3];
extern const pb_field_t emulator_EmulatorMessage_fields[6];

/* Maximum encoded size of messages (where known) */
#define emulator_VehicleAreaConfig_size          55
#define emulator_VehiclePropGet_size             22

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
