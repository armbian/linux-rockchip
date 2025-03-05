/*
 * Debug/trace/assert driver definitions for Dongle Host Driver.
 *
 * Copyright (C) 2022, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id$
 */

#ifndef _dhd_dbg_
#define _dhd_dbg_

#if defined(NDIS)
#include "wl_nddbg.h"
#endif /* defined(NDIS) */
#ifdef DHD_LOG_DUMP
#include <dhd_log_dump.h>
#endif

#ifdef DHD_DEBUGABILITY_LOG_DUMP_RING
/* Only for writing to ring */
#define DHD_INFO_RING(args)	DHD_ERROR(args)
/* FW_VERBOSE RING */
#define DHD_LOG_DUMP_FWLOG	DHD_LOG_DUMP_WRITE_EX
#define DHD_LOG_DUMP_FWLOG_TS	DHD_LOG_DUMP_WRITE_EX_TS
#else
#define DHD_INFO_RING(args)
/* DLD_BUF_TYPE_GENERAL */
#define DHD_LOG_DUMP_FWLOG	DHD_LOG_DUMP_WRITE
#define DHD_LOG_DUMP_FWLOG_TS	DHD_LOG_DUMP_WRITE_TS
#endif

#ifdef CUSTOM_PREFIX
#define DBG_PRINT_PREFIX "[%s]"CUSTOM_PREFIX, OSL_GET_RTCTIME()
#elif defined(CUSTOM_PREFIX_NORTCTIME)
#define DBG_PRINT_PREFIX CUSTOM_PREFIX_NORTCTIME

#define DBG_PRINT_SYSTEM_TIME pr_cont(DBG_PRINT_PREFIX)
#define DHD_CONS_ONLY(args)	\
do {	\
	DBG_PRINT_SYSTEM_TIME;	\
	pr_cont args;		\
} while (0)
#else
#define DBG_PRINT_SYSTEM_TIME
#define DHD_CONS_ONLY(args) do { printf args;} while (0)
#endif /* CUSTOM_PREFIX */

#if defined(BCMDBG) || defined(DHD_DEBUG)

#if defined(NDIS)
#define DHD_ERROR(args)		do {if (dhd_msg_level & DHD_ERROR_VAL) \
					{printf args;  DHD_NDDBG_OUTPUT args;}} while (0)
#define DHD_TRACE(args)		do {if (dhd_msg_level & DHD_TRACE_VAL) \
					{printf args; DHD_NDDBG_OUTPUT args;}} while (0)
#define DHD_INFO(args)		do {if (dhd_msg_level & DHD_INFO_VAL) \
					{printf args; DHD_NDDBG_OUTPUT args;}} while (0)
#define DHD_ERROR_ROAM(args)	DHD_ERROR(args)
#else
/* NON-NDIS cases */
#ifdef DHD_LOG_DUMP
/* !defined(DHD_EFI) and defined(DHD_LOG_DUMP) */
#define DHD_ERROR(args)	\
do {	\
	if (dhd_msg_level & DHD_ERROR_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_ERROR_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)

#ifdef DHD_DEBUGABILITY_LOG_DUMP_RING
#define DHD_INFO(args)	\
do {	\
	if (dhd_msg_level & DHD_INFO_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_INFO_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)
#else
#define DHD_INFO(args)		do {if (dhd_msg_level & DHD_INFO_VAL) printf args;} while (0)
#endif /* DHD_DEBUGABILITY_LOG_DUMP_RING */
#else /* DHD_LOG_DUMP */
/* !defined(DHD_LOG_DUMP cases) */
#define DHD_ERROR(args)		do {if (dhd_msg_level & DHD_ERROR_VAL) printf args;} while (0)
#define DHD_INFO(args)		do {if (dhd_msg_level & DHD_INFO_VAL) printf args;} while (0)
#define DHD_ERROR_ROAM(args)	DHD_ERROR(args)
#endif /* DHD_LOG_DUMP */

#define DHD_TRACE(args)		do {if (dhd_msg_level & DHD_TRACE_VAL) printf args;} while (0)
#endif /* defined(NDIS) */

#ifdef DHD_LOG_DUMP
/* LOG_DUMP defines common to EFI and NON-EFI */
/* NON-EFI builds with LOG DUMP enabled */
#define DHD_ERROR_MEM(args) \
do {	\
	if (dhd_msg_level & DHD_ERROR_VAL) {	\
		if (dhd_msg_level & DHD_ERROR_MEM_VAL) {	\
			printf args;		\
		}	\
	}	\
	if (dhd_log_level & DHD_ERROR_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;		\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)
#define DHD_IOVAR_MEM(args) \
do {	\
	if (dhd_msg_level & DHD_ERROR_VAL) {	\
		if (dhd_msg_level & DHD_IOVAR_MEM_VAL) {	\
			printf args;		\
		}	\
	}	\
	if (dhd_log_level & DHD_ERROR_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;		\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)
#define DHD_LOG_MEM(args) \
do {	\
	if (dhd_log_level & DHD_ERROR_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;		\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)

#define DHD_EVENT(args) \
do {	\
	if (dhd_msg_level & DHD_EVENT_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_EVENT_VAL) {	\
		DHD_LOG_DUMP_WRITE_PRSRV_TS;	\
		DHD_LOG_DUMP_WRITE_PRSRV args;	\
	}	\
} while (0)
#define DHD_PRSRV_MEM(args) \
do {	\
	if (dhd_msg_level & DHD_EVENT_VAL) {	\
		if (dhd_msg_level & DHD_PRSRV_MEM_VAL) {	\
			printf args;		\
		}	\
	}	\
	if (dhd_log_level & DHD_EVENT_VAL) {	\
		DHD_LOG_DUMP_WRITE_PRSRV_TS;		\
		DHD_LOG_DUMP_WRITE_PRSRV args;	\
	}	\
} while (0)
/* Re-using 'DHD_MSGTRACE_VAL' for controlling printing of ecounter binary event
* logs to console and debug dump -- need to cleanup in the future to use separate
* 'DHD_ECNTR_VAL' bitmap flag. 'DHD_MSGTRACE_VAL' will be defined only
* for non-android builds.
*/
#define DHD_ECNTR_LOG(args) \
do {	\
	if (dhd_msg_level & DHD_EVENT_VAL) {	\
		if (dhd_msg_level & DHD_MSGTRACE_VAL) { \
			printf args;		\
		}	\
	}	\
	if (dhd_log_level & DHD_EVENT_VAL) {	\
		if (dhd_log_level & DHD_MSGTRACE_VAL) { \
			DHD_LOG_DUMP_WRITE_TS;		\
			DHD_LOG_DUMP_WRITE args;	\
		}	\
	}	\
} while (0)
#define DHD_ERROR_EX(args)					\
do {										\
	if (dhd_msg_level & DHD_ERROR_VAL) {    \
		printf args;		\
	}	\
	if (dhd_log_level & DHD_ERROR_VAL) {    \
		DHD_LOG_DUMP_WRITE_EX_TS;	\
		DHD_LOG_DUMP_WRITE_EX args;	\
	}	\
} while (0)
#define DHD_MSGTRACE_LOG(args)	\
do {	\
	if (dhd_msg_level & DHD_MSGTRACE_VAL) {	\
		printf args;		\
	}	\
	DHD_LOG_DUMP_WRITE_TS;		\
	DHD_LOG_DUMP_WRITE args;	\
} while (0)

#define DHD_ERROR_ROAM(args)	\
do {	\
	if (dhd_msg_level & DHD_ERROR_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_ERROR_VAL) {	\
		DHD_LOG_DUMP_WRITE_ROAM_TS;	\
		DHD_LOG_DUMP_WRITE_ROAM args;	\
	}	\
} while (0)

#define DHD_PKT_MON(args)	\
do {	\
	if (dhd_msg_level & DHD_PKT_MON_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_PKT_MON_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)

#define DHD_CTL(args)	\
do {	\
	if (dhd_msg_level & DHD_CTL_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_CTL_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)

#define DHD_TIMER(args)	\
do {	\
	if (dhd_msg_level & DHD_TIMER_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_TIMER_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)

#define DHD_INTR(args)	\
do {	\
	if (dhd_msg_level & DHD_INTR_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_INTR_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)

#define DHD_ISCAN(args)	\
do {	\
	if (dhd_msg_level & DHD_ISCAN_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_ISCAN_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)

#define DHD_ARPOE(args)	\
do {	\
	if (dhd_msg_level & DHD_ARPOE_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_ARPOE_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)

#define DHD_REORDER(args)	\
do {	\
	if (dhd_msg_level & DHD_REORDER_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_REORDER_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)

#define DHD_PNO(args)	\
do {	\
	if (dhd_msg_level & DHD_PNO_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_PNO_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)

#define DHD_RTT(args)	\
do {	\
	if (dhd_msg_level & DHD_RTT_VAL) {	\
		printf args;		\
	}	\
	if (dhd_log_level & DHD_RTT_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)

#define DHD_RPM(args)	\
do { \
	if (dhd_msg_level & DHD_RPM_VAL) {	\
		printf args;	\
	}	\
	if (dhd_log_level & DHD_RPM_VAL) {	\
		DHD_LOG_DUMP_WRITE_TS;	\
		DHD_LOG_DUMP_WRITE args;	\
	}	\
} while (0)
#else /* DHD_LOG_DUMP */
/* !DHD_LOG_DUMP */
#define DHD_MSGTRACE_LOG(args)  do {if (dhd_msg_level & DHD_MSGTRACE_VAL) printf args;} while (0)
#define DHD_ERROR_MEM(args)	DHD_ERROR(args)
#define DHD_IOVAR_MEM(args)	DHD_ERROR(args)
#define DHD_LOG_MEM(args)	DHD_ERROR(args)
#define DHD_EVENT(args)		do {if (dhd_msg_level & DHD_EVENT_VAL) printf args;} while (0)
#define DHD_ECNTR_LOG(args)	DHD_EVENT(args)
#define DHD_PRSRV_MEM(args)	DHD_EVENT(args)
#define DHD_ERROR_EX(args)	DHD_ERROR(args)
#define DHD_ERROR_ROAM(args)	DHD_ERROR(args)
#define DHD_PKT_MON(args)	DHD_ERROR(args)
#endif /* DHD_LOG_DUMP */

#if !defined(DHD_LOG_DUMP)
#define DHD_CTL(args)		do {if (dhd_msg_level & DHD_CTL_VAL) printf args;} while (0)
#define DHD_TIMER(args)		do {if (dhd_msg_level & DHD_TIMER_VAL) printf args;} while (0)
#define DHD_INTR(args)		do {if (dhd_msg_level & DHD_INTR_VAL) printf args;} while (0)
#define DHD_ISCAN(args)		do {if (dhd_msg_level & DHD_ISCAN_VAL) printf args;} while (0)
#define DHD_ARPOE(args)		do {if (dhd_msg_level & DHD_ARPOE_VAL) printf args;} while (0)
#define DHD_REORDER(args)	do {if (dhd_msg_level & DHD_REORDER_VAL) printf args;} while (0)
#define DHD_PNO(args)		do {if (dhd_msg_level & DHD_PNO_VAL) printf args;} while (0)
#define DHD_RTT(args)		do {if (dhd_msg_level & DHD_RTT_VAL) printf args;} while (0)
#define DHD_RPM(args)		do {if (dhd_msg_level & DHD_RPM_VAL) printf args;} while (0)
#endif

#define DHD_DATA(args)		do {if (dhd_msg_level & DHD_DATA_VAL) printf args;} while (0)
#define DHD_HDRS(args)		do {if (dhd_msg_level & DHD_HDRS_VAL) printf args;} while (0)
#define DHD_BYTES(args)		do {if (dhd_msg_level & DHD_BYTES_VAL) printf args;} while (0)
#define DHD_GLOM(args)		do {if (dhd_msg_level & DHD_GLOM_VAL) printf args;} while (0)
#define DHD_BTA(args)		do {if (dhd_msg_level & DHD_BTA_VAL) printf args;} while (0)

#if defined(DHD_LOG_DUMP)
#if defined(DHD_LOG_PRINT_RATE_LIMIT)

#define DHD_FWLOG(args)	\
do { \
	if (dhd_msg_level & DHD_FWLOG_VAL) { \
		if (control_logtrace && !log_print_threshold) \
			printf args; \
	} \
	if (dhd_log_level & DHD_FWLOG_VAL) { \
		DHD_LOG_DUMP_FWLOG_TS;	\
		DHD_LOG_DUMP_FWLOG args; \
	} \
} while (0)

#else
#define DHD_FWLOG(args)	\
	do { \
		if (dhd_msg_level & DHD_FWLOG_VAL) { \
			if (control_logtrace) \
				printf args; \
			DHD_LOG_DUMP_WRITE args; \
		} \
	} while (0)
#endif
#else /* DHD_LOG_DUMP */
#if defined(NDIS) && (NDISVER >= 0x0630)
#define DHD_FWLOG(args)		do {if (dhd_msg_level & DHD_FWLOG_VAL) \
					{printf args;  DHD_NDDBG_OUTPUT args;}} while (0)
#else
#define DHD_FWLOG(args)		do {if (dhd_msg_level & DHD_FWLOG_VAL) printf args;} while (0)
#endif /* defined(NDIS) && (NDISVER >= 0x0630) */
#endif /* DHD_LOG_DUMP */

#define DHD_DBGIF(args)		do {if (dhd_msg_level & DHD_DBGIF_VAL) printf args;} while (0)
#define DHD_TXFLOWCTL(args)     DHD_RPM(args)

#define DHD_TRACE_HW4	DHD_TRACE
#define DHD_INFO_HW4	DHD_INFO
#define DHD_ERROR_NO_HW4	DHD_ERROR

#define DHD_ERROR_ON()		(dhd_msg_level & DHD_ERROR_VAL)
#define DHD_TRACE_ON()		(dhd_msg_level & DHD_TRACE_VAL)
#define DHD_INFO_ON()		(dhd_msg_level & DHD_INFO_VAL)
#define DHD_DATA_ON()		(dhd_msg_level & DHD_DATA_VAL)
#define DHD_CTL_ON()		(dhd_msg_level & DHD_CTL_VAL)
#define DHD_TIMER_ON()		(dhd_msg_level & DHD_TIMER_VAL)
#define DHD_HDRS_ON()		(dhd_msg_level & DHD_HDRS_VAL)
#define DHD_BYTES_ON()		(dhd_msg_level & DHD_BYTES_VAL)
#define DHD_INTR_ON()		(dhd_msg_level & DHD_INTR_VAL)
#define DHD_GLOM_ON()		(dhd_msg_level & DHD_GLOM_VAL)
#define DHD_EVENT_ON()		(dhd_msg_level & DHD_EVENT_VAL)
#define DHD_BTA_ON()		(dhd_msg_level & DHD_BTA_VAL)
#define DHD_ISCAN_ON()		(dhd_msg_level & DHD_ISCAN_VAL)
#define DHD_ARPOE_ON()		(dhd_msg_level & DHD_ARPOE_VAL)
#define DHD_REORDER_ON()	(dhd_msg_level & DHD_REORDER_VAL)
#define DHD_NOCHECKDIED_ON()	(dhd_msg_level & DHD_NOCHECKDIED_VAL)
#define DHD_PNO_ON()		(dhd_msg_level & DHD_PNO_VAL)
#define DHD_RTT_ON()		(dhd_msg_level & DHD_RTT_VAL)
#define DHD_MSGTRACE_ON()	(dhd_msg_level & DHD_MSGTRACE_VAL)
#define DHD_FWLOG_ON()		(dhd_msg_level & DHD_FWLOG_VAL)
#define DHD_DBGIF_ON()		(dhd_msg_level & DHD_DBGIF_VAL)
#define DHD_PKT_MON_ON()	(dhd_msg_level & DHD_PKT_MON_VAL)
#define DHD_PKT_MON_DUMP_ON()	(dhd_msg_level & DHD_PKT_MON_DUMP_VAL)
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
#define DHD_RPM_ON()		(dhd_msg_level & DHD_RPM_VAL)
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */

#else /* defined(BCMDBG) || defined(DHD_DEBUG) */

#if defined(NDIS)
#define DHD_ERROR(args)		do {if (dhd_msg_level & DHD_ERROR_VAL) \
					{printf args;  DHD_NDDBG_OUTPUT args;}} while (0)
#define DHD_TRACE(args)		do {if (dhd_msg_level & DHD_TRACE_VAL) \
					{DHD_NDDBG_OUTPUT args;}} while (0)
#define DHD_INFO(args)		do {if (dhd_msg_level & DHD_INFO_VAL) \
					{DHD_NDDBG_OUTPUT args;}} while (0)
#else /* DHD_EFI && DHD_LOG_DUMP */

#define DHD_ERROR(args)		do {if (dhd_msg_level & DHD_ERROR_VAL) \
								printf args;} while (0)
#define DHD_TRACE(args)
#define DHD_INFO(args)
#define DHD_ERROR_ROAM(args)	DHD_ERROR(args)
#endif /* defined(NDIS) */

#define DHD_DATA(args)
#define DHD_CTL(args)
#define DHD_TIMER(args)
#define DHD_HDRS(args)
#define DHD_BYTES(args)
#define DHD_INTR(args)
#define DHD_GLOM(args)

#define DHD_EVENT(args)
#define DHD_ECNTR_LOG(args)	DHD_EVENT(args)

#define DHD_PRSRV_MEM(args)	DHD_EVENT(args)

#define DHD_BTA(args)
#define DHD_ISCAN(args)
#define DHD_ARPOE(args)
#define DHD_REORDER(args)
#define DHD_PNO(args)
#define DHD_RTT(args)
#define DHD_PKT_MON(args)

#define DHD_MSGTRACE_LOG(args)
#define DHD_FWLOG(args)

#define DHD_DBGIF(args)

#define DHD_ERROR_MEM(args)	DHD_ERROR(args)
#define DHD_IOVAR_MEM(args)	DHD_ERROR(args)
#define DHD_LOG_MEM(args)	DHD_ERROR(args)
#define DHD_ERROR_EX(args)	DHD_ERROR(args)
#define DHD_ERROR_ROAM(args)    DHD_ERROR(args)
#define DHD_RPM(args)		DHD_ERROR(args)
#define DHD_TXFLOWCTL(args)     DHD_ERROR(args)
#define DHD_TRACE_HW4	DHD_TRACE
#define DHD_INFO_HW4	DHD_INFO
#define DHD_ERROR_NO_HW4	DHD_ERROR

#define DHD_ERROR_ON()		0
#define DHD_TRACE_ON()		0
#define DHD_INFO_ON()		0
#define DHD_DATA_ON()		0
#define DHD_CTL_ON()		0
#define DHD_TIMER_ON()		0
#define DHD_HDRS_ON()		0
#define DHD_BYTES_ON()		0
#define DHD_INTR_ON()		0
#define DHD_GLOM_ON()		0
#define DHD_EVENT_ON()		0
#define DHD_BTA_ON()		0
#define DHD_ISCAN_ON()		0
#define DHD_ARPOE_ON()		0
#define DHD_REORDER_ON()	0
#define DHD_NOCHECKDIED_ON()	0
#define DHD_PNO_ON()		0
#define DHD_RTT_ON()		0
#define DHD_PKT_MON_ON()	0
#define DHD_PKT_MON_DUMP_ON()	0
#define DHD_MSGTRACE_ON()	0
#define DHD_FWLOG_ON()		0
#define DHD_DBGIF_ON()		0
#ifdef DHD_PCIE_NATIVE_RUNTIMEPM
#define DHD_RPM_ON()		0
#endif /* DHD_PCIE_NATIVE_RUNTIMEPM */
#endif /* defined(BCMDBG) || defined(DHD_DEBUG) */

#define PRINT_RATE_LIMIT_PERIOD 5000000u /* 5s in units of us */
#define DHD_ERROR_RLMT(args) \
do {	\
	if (dhd_msg_level & DHD_ERROR_VAL) {	\
		static uint64 __err_ts = 0; \
		static uint32 __err_cnt = 0; \
		uint64 __cur_ts = 0; \
		__cur_ts = OSL_SYSUPTIME_US(); \
		if (__err_ts == 0 || (__cur_ts > __err_ts && \
		(__cur_ts - __err_ts > PRINT_RATE_LIMIT_PERIOD))) { \
			__err_ts = __cur_ts; \
			DHD_ERROR(args);	\
			DHD_ERROR(("[Repeats %u times]\n", __err_cnt)); \
			__err_cnt = 0; \
		} else { \
			++__err_cnt; \
		} \
	}	\
} while (0)

/* even in non-BCMDBG builds, logging of dongle iovars should be available */
#define DHD_DNGL_IOVAR_SET(args) \
	do {if (dhd_msg_level & DHD_DNGL_IOVAR_SET_VAL) printf args;} while (0)

#ifdef BCMPERFSTATS
#define DHD_LOG(args)		do {if (dhd_msg_level & DHD_LOG_VAL) bcmlog args;} while (0)
#else
#define DHD_LOG(args)
#endif

#define DHD_BLOG(cp, size)

#define DHD_NONE(args)
extern int dhd_msg_level;
extern int dhd_log_level;
#ifdef DHD_LOG_PRINT_RATE_LIMIT
extern int log_print_threshold;
#endif /* DHD_LOG_PRINT_RATE_LIMIT */

#ifdef DHD_IOVAR_LOG_FILTER_DUMP
#define DHD_IOVAR_LOG_CHECK(dhd_pub, ioc_cmd, ioc_msg) \
	dhd_iovar_log_dump_check(dhd_pub, ioc_cmd, ioc_msg)
#else
#define DHD_IOVAR_LOG_CHECK(dhd_pub, ioc_cmd, ioc_msg) TRUE
#endif /* DHD_IOVAR_LOG_FILTER_DUMP */

#define DHD_IOVAR_LOG(dhd_pub, ioc_cmd, ioc_msg, fmt) \
	do { \
		if (DHD_IOVAR_LOG_CHECK(dhd_pub, ioc_cmd, ioc_msg)) { \
			DHD_IOVAR_MEM(fmt); \
		} else { \
			DHD_TRACE(fmt); \
		} \
	} while (0)

/* DHD_PRINT - informational non-error messages which need to be always printed */
#define DHD_PRINT	DHD_ERROR

/* Defines msg bits */
#include <dhdioctl.h>

#endif /* _dhd_dbg_ */
