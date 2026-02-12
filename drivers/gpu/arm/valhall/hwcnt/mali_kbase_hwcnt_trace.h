/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 *
 * (C) COPYRIGHT 2025 ARM Limited. All rights reserved.
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
#undef TRACE_SYSTEM
#define TRACE_SYSTEM hwcnt
#if !defined(_HWCNT_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _HWCNT_TRACE_H
#include <linux/tracepoint.h>
TRACE_EVENT(hwcnt_schedule_next_sample, TP_PROTO(u64 ts, u64 dts), TP_ARGS(ts, dts),
	    TP_STRUCT__entry(__field(u64, ts) __field(u64, dts)),
	    TP_fast_assign(__entry->ts = ts; __entry->dts = dts;),
	    TP_printk("Schedule next sample at %llu [%llu from now]", __entry->ts, __entry->dts));
TRACE_EVENT(hwcnt_skip_rearming, TP_PROTO(u64 ts), TP_ARGS(ts), TP_STRUCT__entry(__field(u64, ts)),
	    TP_fast_assign(__entry->ts = ts;),
	    TP_printk("hwcnt_skip_rearming ts = %llu", __entry->ts));
TRACE_EVENT(hwcnt_client_dump, TP_PROTO(u64 ts, u64 dump_ts, u8 dump), TP_ARGS(ts, dump_ts, dump),
	    TP_STRUCT__entry(__field(u64, ts) __field(u64, dump_ts) __field(u8, dump)),
	    TP_fast_assign(__entry->ts = ts; __entry->dump_ts = dump_ts; __entry->dump = dump),
	    TP_printk("client dump? ts = %llu, dump_time = %llu, %d", __entry->ts, __entry->dump_ts,
		      __entry->dump));
#endif /* _HWCNT_TRACE_H */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH hwcnt
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mali_kbase_hwcnt_trace
#include <trace/define_trace.h>
