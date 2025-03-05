/* SPDX-License-Identifier: GPL-2.0
 * aw882xx_spin.h spin_module
 *
 * Copyright (c) 2019 AWINIC Technology CO., LTD
 *
 * Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __AW882XX_SPIN_H__
#define __AW882XX_SPIN_H__


enum {
	AW_SPIN_OFF_MODE = 0,
	AW_ADSP_SPIN_MODE,
	AW_REG_SPIN_MODE,
	AW_REG_MIXER_SPIN_MODE,
	AW_SPIN_MODE_MAX,
};

int aw882xx_spin_init(struct aw_spin_desc *spin_desc);
int aw882xx_spin_value_set(struct aw_device *aw_dev, uint32_t spin_val, bool pstream);
int aw882xx_spin_value_get(struct aw_device *aw_dev, uint32_t *spin_val, bool pstream);
int aw882xx_spin_set_record_val(struct aw_device *aw_dev);

#endif
