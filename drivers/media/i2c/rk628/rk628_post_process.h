/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 */

#ifndef POST_PROCESS_H
#define POST_PROCESS_H

void rk628_post_process_csc_en(struct rk628 *rk628, bool input_full_range, bool output_full_range);
void rk628_post_process_csc_dis(struct rk628 *rk628);
void rk628_post_process_pattern_node(struct rk628 *rk628);
#endif
