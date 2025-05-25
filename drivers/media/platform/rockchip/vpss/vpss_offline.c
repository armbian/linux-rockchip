// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#include <linux/fs.h>
#include <linux/module.h>
#include <uapi/asm-generic/fcntl.h>

#include "vpss.h"
#include "common.h"
#include "stream.h"
#include "dev.h"
#include "vpss_offline.h"
#include "hw.h"
#include "regs.h"

#include "vpss_offline_v10.h"
#include "vpss_offline_v20.h"

void rkvpss_dump_reg(struct rkvpss_offline_dev *ofl, int sequence, int size)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	struct file *filep = NULL;
	char buf[256] = {0};
	loff_t pos = 0;
	int i, j;

	if (!IS_ENABLED(CONFIG_NO_GKI))
		return;

	filep = filp_open(rkvpss_regfile, O_RDWR | O_APPEND | O_CREAT, 0644);
	if (IS_ERR(filep)) {
		v4l2_err(&ofl->v4l2_dev, "Open file %s error\n",
			 rkvpss_regfile);
		return;
	} else {
		memset(buf, 0, sizeof(buf));
		sprintf(buf, "sequence:%d\n", sequence);
		kernel_write(filep, buf, strlen(buf), &pos);
		if (pos < 0)
			v4l2_err(&ofl->v4l2_dev, "Write data to %s failed\n",
				 rkvpss_regfile);

		for (i = 0; i < size; i = i + 16) {
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "%04x:", i);
			kernel_write(filep, buf, strlen(buf), &pos);
			if (pos < 0)
				v4l2_err(&ofl->v4l2_dev, "Write data to %s failed\n",
					 rkvpss_regfile);

			for (j = 0; j < 16; j = j + 4) {
				memset(buf, 0, sizeof(buf));
				sprintf(buf, "  %08x", rkvpss_hw_read(hw, i + j));
				kernel_write(filep, buf, strlen(buf), &pos);
				if (pos < 0)
					v4l2_err(&ofl->v4l2_dev, "Write data to %s failed\n",
						 rkvpss_regfile);
			}
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "%s\n", "");
			kernel_write(filep, buf, strlen(buf), &pos);
			if (pos < 0) {
				v4l2_err(&ofl->v4l2_dev, "Write data to %s failed\n",
					rkvpss_regfile);
			}
		}
		filp_close(filep, NULL);
	}
}


void rkvpss_offline_irq(struct rkvpss_hw_dev *hw, u32 irq)
{
	if (is_vpss_v10(hw))
		rkvpss_offline_irq_v10(hw, irq);
	else if (is_vpss_v20(hw))
		rkvpss_offline_irq_v20(hw, irq);
}

int rkvpss_register_offline(struct rkvpss_hw_dev *hw)
{
	int ret = -EINVAL;

	if (is_vpss_v10(hw))
		ret = rkvpss_register_offline_v10(hw);
	else if (is_vpss_v20(hw))
		ret = rkvpss_register_offline_v20(hw);

	return ret;
}

void rkvpss_unregister_offline(struct rkvpss_hw_dev *hw)
{
	if (is_vpss_v10(hw))
		rkvpss_unregister_offline_v10(hw);
	else if (is_vpss_v20(hw))
		rkvpss_unregister_offline_v20(hw);
}

