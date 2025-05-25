/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_OFFLINE_V20_H
#define _RKVPSS_OFFLINE_V20_H

struct rkvpss_offline_dev;

//void buf_del(struct rkvpss_offline_dev *ofl,
//	 struct dma_buf *dmabuf,
//	 int file_id, int id, int fd, bool is_all,
//	 bool running);
int rkvpss_module_sel(struct rkvpss_offline_dev *ofl,
		       struct rkvpss_module_sel *sel);
int rkvpss_module_get(struct rkvpss_offline_dev *ofl,
		       struct rkvpss_module_sel *get);
int rkvpss_ofl_buf_add(struct rkvpss_offline_dev *ofl, int file_id, struct rkvpss_buf_info *info);
void rkvpss_ofl_buf_del(struct rkvpss_offline_dev *ofl, int file_id, struct rkvpss_buf_info *info);
void rkvpss_ofl_buf_del_by_file(struct rkvpss_offline_dev *ofl, int file_id);
int rkvpss_prepare_run(struct rkvpss_offline_dev *ofl, int file_id, struct rkvpss_frame_cfg *cfg);
int rkvpss_check_params(struct rkvpss_offline_dev *ofl, struct rkvpss_frame_cfg *cfg, bool *unite);
long rkvpss_ofl_action(struct rkvpss_offline_dev *ofl, int file_id, unsigned int cmd,  void *arg);
int rkvpss_ofl_add_file_id(struct rkvpss_offline_dev *ofl, void *idr_entity);
void *rkvpss_ofl_del_file_id(struct rkvpss_offline_dev *ofl, struct file *file);



#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_VPSS_V20)
int rkvpss_register_offline_v20(struct rkvpss_hw_dev *hw);
void rkvpss_unregister_offline_v20(struct rkvpss_hw_dev *hw);
void rkvpss_offline_irq_v20(struct rkvpss_hw_dev *hw, u32 irq);
#else
static inline int rkvpss_register_offline_v20(struct rkvpss_hw_dev *hw) {return -EINVAL; }
static inline void rkvpss_unregister_offline_v20(struct rkvpss_hw_dev *hw) {}
static inline void rkvpss_offline_irq_v20(struct rkvpss_hw_dev *hw, u32 irq) {}
#endif


#endif

