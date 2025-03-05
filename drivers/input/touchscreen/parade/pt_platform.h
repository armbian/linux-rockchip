/*
 * pt_platform.h
 * Parade TrueTouch(TM) Standard Product Platform Module.
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
 * Contact Parade Technologies at www.parade.com <ttdrivers@paradetech.com>
 */

#ifndef _LINUX_PT_PLATFORM_H
#define _LINUX_PT_PLATFORM_H

#include "pt_core.h"

#if defined(CONFIG_TOUCHSCREEN_PARADE) \
	|| defined(CONFIG_TOUCHSCREEN_PARADE_MODULE)
extern struct pt_loader_platform_data _pt_loader_platform_data;

int pt_xres(struct pt_core_platform_data *pdata, struct device *dev);
int pt_init(struct pt_core_platform_data *pdata, int on,
		struct device *dev);
int pt_power(struct pt_core_platform_data *pdata, int on,
		struct device *dev, atomic_t *ignore_irq);
#ifdef PT_DETECT_HW
int pt_detect(struct pt_core_platform_data *pdata,
		struct device *dev, pt_platform_read read);
#else
#define pt_detect      NULL
#endif
int pt_irq_stat(struct pt_core_platform_data *pdata,
		struct device *dev);
int pt_setup_power(struct pt_core_platform_data *pdata, int on,
		struct device *dev);
int pt_setup_irq(struct pt_core_platform_data *pdata, int on,
		struct device *dev);
#else /* !CONFIG_TOUCHSCREEN_PARADE */
static struct pt_loader_platform_data _pt_loader_platform_data;
#define pt_xres         NULL
#define pt_init         NULL
#define pt_power        NULL
#define pt_irq_stat     NULL
#define pt_detect       NULL
#define pt_setup_power  NULL
#define pt_setup_irq    NULL
#endif /* CONFIG_TOUCHSCREEN_PARADE */

#endif /* _LINUX_PT_PLATFORM_H */
