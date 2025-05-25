/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_OFFLINE_H
#define _RKVPSS_OFFLINE_H
#define DEV_NUM_MAX 256
#define UNITE_ENLARGE 16
#define UNITE_LEFT_ENLARGE 16

extern char rkvpss_regfile[RKVPSS_REGFILE_LEN];

struct rkvpss_ofl_incfginfo {
	int width;
	int height;
	int format;
	int buf_fd;
};

struct rkvpss_ofl_outcfginfo {
	int enable;
	int format;
	int buf_fd;

	int crop_h_offs;
	int crop_v_offs;
	int crop_width;
	int crop_height;

	int scl_width;
	int scl_height;
};

struct rkvpss_ofl_cfginfo {
	int dev_id;
	int sequence;
	struct rkvpss_ofl_incfginfo input;
	struct rkvpss_ofl_outcfginfo output[RKVPSS_OUTPUT_MAX];
	struct list_head list;
};

struct rkvpss_dev_rate {
	u32 sequence;
	u32 in_rate;
	u64 in_timestamp;
	u64 out_timestamp;
	u32 out_rate;
	u32 delay;
};

struct rkvpss_unite_scl_params {
	u32 y_w_fac;
	u32 c_w_fac;
	u32 y_h_fac;
	u32 c_h_fac;
	u32 y_w_phase;
	u32 c_w_phase;
	u32 quad_crop_w;
	u32 scl_in_crop_w_y;
	u32 scl_in_crop_w_c;
	u32 right_scl_need_size_y;
	u32 right_scl_need_size_c;
};

struct rkvpss_offline_dev {
	struct rkvpss_hw_dev *hw;
	struct v4l2_device v4l2_dev;
	struct video_device vfd;
	struct mutex apilock;
	struct completion cmpl;
	struct list_head list;
	struct proc_dir_entry *procfs;
	struct list_head cfginfo_list;
	struct mutex ofl_lock;
	struct rkvpss_dev_rate dev_rate[DEV_NUM_MAX];
	struct rkvpss_unite_scl_params unite_params[RKVPSS_OUTPUT_MAX];
	struct completion pm_cmpl;
	u32 unite_right_enlarge;
	struct idr file_idr;
	struct mutex idr_lock;
	struct mutex handle_lock;
	bool mode_sel_en;
	bool pm_need_wait;
};

int rkvpss_register_offline(struct rkvpss_hw_dev *hw);
void rkvpss_unregister_offline(struct rkvpss_hw_dev *hw);
void rkvpss_offline_irq(struct rkvpss_hw_dev *hw, u32 irq);
void rkvpss_dump_reg(struct rkvpss_offline_dev *ofl, int sequence, int size);

#endif
