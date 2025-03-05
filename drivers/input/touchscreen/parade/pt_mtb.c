/*
 * pt_mtb.c
 * Parade TrueTouch(TM) Standard Product Multi-Touch Protocol B Module.
 * For use with Parade touchscreen controllers.
 * Supported parts include:
 * TMA5XX
 * TMA448
 * TMA445A
 * TT21XXX
 * TT31XXX
 * TT4XXXX
 * TT7XXX
 * TC3XXX
 *
 * Copyright (C) 2015-2020 Parade Technologies
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Parade Technologies at www.paradetech.com <ttdrivers@paradetech.com>
 */

#include "pt_regs.h"
#include <linux/input/mt.h>
#include <linux/version.h>

/*******************************************************************************
 * FUNCTION: pt_final_sync
 *
 * SUMMARY: Function to create SYN_REPORT
 *
 * PARAMETERS:
 *        *input - pointer to input device structure
 *     max_slots - max support touch number
 * mt_sync_count - current valid touch number
 *           ids - bit map value
 ******************************************************************************/
static void pt_final_sync(struct input_dev *input, int max_slots,
		int mt_sync_count, unsigned long *ids)
{
	int t;

	for (t = 0; t < max_slots; t++) {
		if (test_bit(t, ids))
			continue;
		input_mt_slot(input, t);
		input_mt_report_slot_state(input, MT_TOOL_FINGER, false);
	}

	input_sync(input);
}

/*******************************************************************************
 * FUNCTION: pt_input_report
 *
 * SUMMARY: Function to report coordinate information of touch point
 * protocol
 *
 * PARAMETERS:
 *        *input - pointer to input device structure
 *           sig - track id to allow tracking of a touch
 *             t - event id to indicate an event associated with touch instance
 *          type - indicate the touch object
 ******************************************************************************/
static void pt_input_report(struct input_dev *input, int sig,
		int t, int type)
{
	input_mt_slot(input, t);

	if (type == PT_OBJ_STANDARD_FINGER || type == PT_OBJ_GLOVE)
		input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
	else if (type == PT_OBJ_STYLUS)
		input_mt_report_slot_state(input, MT_TOOL_PEN, true);
}

/*******************************************************************************
 * FUNCTION: pt_report_slot_liftoff
 *
 * SUMMARY: Function to report all touches are lifted
 * protocol
 *
 * PARAMETERS:
 *        *md - pointer to input device structure
 *  max_slots - indicate max number of touch id
 ******************************************************************************/
static void pt_report_slot_liftoff(struct pt_mt_data *md,
		int max_slots)
{
	int t;

	if (md->num_prv_rec == 0)
		return;

	for (t = 0; t < max_slots; t++) {
		input_mt_slot(md->input, t);
		input_mt_report_slot_state(md->input,
			MT_TOOL_FINGER, false);
	}
}

/*******************************************************************************
 * FUNCTION: pt_input_register_device
 *
 * SUMMARY: Function to register input device
 * protocol
 *
 * PARAMETERS:
 *     *input - pointer to input device structure
 *  max_slots - indicate max number of touch id
 ******************************************************************************/
static int pt_input_register_device(struct input_dev *input, int max_slots)
{
#if (KERNEL_VERSION(3, 7, 0) <= LINUX_VERSION_CODE)
	input_mt_init_slots(input, max_slots, 0);
#else
	input_mt_init_slots(input, max_slots);
#endif
	return input_register_device(input);
}

/*******************************************************************************
 * FUNCTION: pt_init_function_ptrs
 *
 * SUMMARY: Function to init function pointer
 *
 * PARAMETERS:
 *     *md - pointer to touch data structure
 ******************************************************************************/
void pt_init_function_ptrs(struct pt_mt_data *md)
{
	md->mt_function.report_slot_liftoff = pt_report_slot_liftoff;
	md->mt_function.final_sync = pt_final_sync;
	md->mt_function.input_sync = NULL;
	md->mt_function.input_report = pt_input_report;
	md->mt_function.input_register_device = pt_input_register_device;
}

