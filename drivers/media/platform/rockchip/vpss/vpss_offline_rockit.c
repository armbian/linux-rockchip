// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#include "vpss.h"
#include "common.h"
#include "stream.h"
#include "dev.h"
#include "vpss_offline.h"
#include "hw.h"
#include "procfs.h"
#include "regs.h"

#include "vpss_offline_rockit.h"
#include "vpss_offline_v20.h"

static struct rkvpss_offline_dev *global_ofl;

void rkvpss_ofl_rockit_init(struct rkvpss_offline_dev *ofl)
{
	global_ofl = ofl;
}

static long rkvpss_ofl_rockit_open(int *file_id)
{
	void *temp_file = kmalloc(sizeof(void *), GFP_KERNEL);
	long ret = 0;

	*file_id = rkvpss_ofl_add_file_id(global_ofl, temp_file);
	if (*file_id <= 0) {
		ret = -EINVAL;
		kfree(temp_file);
		goto out;
	}

	mutex_lock(&global_ofl->hw->dev_lock);
	pm_runtime_get_sync(global_ofl->hw->dev);
	mutex_unlock(&global_ofl->hw->dev_lock);

	v4l2_dbg(1, rkvpss_debug, &global_ofl->v4l2_dev,
		 "%s file_id:%d\n", __func__, *file_id);
out:
	return ret;
}

static long rkvpss_ofl_rockit_release(int *file_id)
{
	void *idr_entity = NULL;
	long ret = 0;

	rkvpss_ofl_buf_del_by_file(global_ofl, *file_id);
	idr_entity = idr_remove(&global_ofl->file_idr, *file_id);
	if (!idr_entity)
		goto out;

	kfree(idr_entity);

	mutex_lock(&global_ofl->hw->dev_lock);
	pm_runtime_put_sync(global_ofl->hw->dev);
	mutex_unlock(&global_ofl->hw->dev_lock);

	v4l2_dbg(1, rkvpss_debug, &global_ofl->v4l2_dev,
		 "%s file_id:%d\n", __func__, *file_id);
out:
	return ret;
}

static void *rkvpss_ofl_check_file_id(struct rkvpss_offline_dev *ofl,
				      int file_id)
{
	void *idr_entity = NULL;

	mutex_lock(&ofl->idr_lock);
	idr_entity = idr_find(&ofl->file_idr, file_id);
	mutex_unlock(&ofl->idr_lock);

	return idr_entity;
}

long vpss_rockit_action(int *file_id, unsigned int cmd, void *arg)
{
	long ret = 0;

	switch (cmd) {
	case RKVPSS_CMD_OPEN:
		ret = rkvpss_ofl_rockit_open(file_id);
		break;
	case RKVPSS_CMD_RELEASE:
		ret = rkvpss_ofl_rockit_release(file_id);
		break;
	case RKVPSS_CMD_MODULE_SEL:
	case RKVPSS_CMD_MODULE_GET:
	case RKVPSS_CMD_BUF_ADD:
	case RKVPSS_CMD_BUF_DEL:
	case RKVPSS_CMD_FRAME_HANDLE:
	case RKVPSS_CMD_CHECKPARAMS:
	case RKVPSS_CMD_WRAP_DVBM_INIT:
	case RKVPSS_CMD_WRAP_DVBM_DEINIT:
	case RKVPSS_CMD_GET_WRAP_SEQ:
		if (!rkvpss_ofl_check_file_id(global_ofl, *file_id)) {
			v4l2_err(&global_ofl->v4l2_dev, "file_id error\n");
			ret = -EINVAL;
			goto out;
		}
		ret = rkvpss_ofl_action(global_ofl, *file_id, cmd, arg);
		break;
	default:
		break;
	}

out:
	return ret;
}
EXPORT_SYMBOL(vpss_rockit_action);

