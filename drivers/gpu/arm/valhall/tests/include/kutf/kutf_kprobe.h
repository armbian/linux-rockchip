/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 *
 * (C) COPYRIGHT 2023-2024 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

#ifndef _KUTF_KPROBE_H_
#define _KUTF_KPROBE_H_

struct dentry;

int kutf_kprobe_init(struct dentry *base_dir);
void kutf_kprobe_exit(void);

typedef void (*kutf_kp_handler)(int argc, char **argv);

void kutf_kp_sample_handler(int argc, char **argv);
void kutf_kp_sample_kernel_function(void);

void kutf_kp_delay_handler(int argc, char **argv);

#endif /* _KUTF_KPROBE_H_ */
