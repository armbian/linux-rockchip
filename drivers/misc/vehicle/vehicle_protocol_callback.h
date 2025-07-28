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
#ifndef VEHICLE_PROTOCOL_CALLBACK_H_
#define VEHICLE_PROTOCOL_CALLBACK_H_

bool decode_prop_callback(pb_istream_t *stream,
			  const pb_field_t *field,
			  void **arg);
bool decode_config_callback(pb_istream_t *stream,
			    const pb_field_t *field,
			    void **arg);
bool decode_value_callback(pb_istream_t *stream,
			   const pb_field_t *field,
			   void **arg);
bool decode_config_string_callback(pb_istream_t *stream,
				   const pb_field_t *field,
				   void **arg);
bool decode_config_array_callback(pb_istream_t *stream,
				  const pb_field_t *field,
				  void **arg);
bool decode_area_configs_callback(pb_istream_t *stream,
				  const pb_field_t *field,
				  void **arg);
bool decode_float_values_callback(pb_istream_t *stream,
				  const pb_field_t *field,
				  void **arg);
bool decode_int64_values_callback(pb_istream_t *stream,
				  const pb_field_t *field,
				  void **arg);
bool decode_int32_values_callback(pb_istream_t *stream,
				  const pb_field_t *field,
				  void **arg);
bool encode_value_callback(pb_ostream_t *stream,
			   const pb_field_t *field,
			   void * const *arg);
bool encode_prop_callback(pb_ostream_t *stream,
			  const pb_field_t *field,
			  void * const *arg);
bool encode_area_configs_callback(pb_ostream_t *stream,
				  const pb_field_t *field,
				  void * const *arg);
bool encode_config_array_callback(pb_ostream_t *stream,
				  const pb_field_t *field,
				  void * const *arg);
bool encode_config_string_callback(pb_ostream_t *stream,
				   const pb_field_t *field,
				   void * const *arg);
bool encode_config_callback(pb_ostream_t *stream,
			    const pb_field_t *field,
			    void * const *arg);
bool encode_int32_values_callback(pb_ostream_t *stream,
				  const pb_field_t *field,
				  void * const *arg);
bool encode_int64_values_callback(pb_ostream_t *stream,
				  const pb_field_t *field,
				  void * const *arg);
bool encode_fix32_values_callback(pb_ostream_t *stream,
				  const pb_field_t *field,
				  void * const *arg);
bool encode_power_state_callback(pb_ostream_t *stream,
				 const pb_field_t *field,
				 void * const *arg);
bool encode_power_state_value_callback(pb_ostream_t *stream,
				       const pb_field_t *field,
				       void * const *arg);

#endif
