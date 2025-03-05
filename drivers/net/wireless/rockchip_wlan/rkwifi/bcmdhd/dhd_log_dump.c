/*
 * log_dump - debugability support for dumping logs to file
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
#ifdef DHD_LOG_DUMP

#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmstdlib_s.h>
#include <dngl_stats.h>
#include <dhd_linux_priv.h>
#include <dhd_linux_wq.h>
#include <dhd.h>
#include <dhd_proto.h>
#include <dhd_log_dump.h>
#ifdef DHD_EVENT_LOG_FILTER
#include <dhd_event_log_filter.h>
#endif /* DHD_EVENT_LOG_FILTER */
#ifdef DHD_PKT_LOGGING
#include <dhd_pktlog.h>
#endif /* DHD_PKT_LOGGING */
#if defined(WL_CFG80211)
#include <wl_cfg80211.h>
#endif

extern char dhd_version[];
extern char fw_version[];
struct dhd_log_dump_buf g_dld_buf[DLD_BUFFER_NUM];

/* Only header for log dump buffers is stored in array
 * header for sections like 'dhd dump', 'ext trap'
 * etc, is not in the array, because they are not log
 * ring buffers
 */
dld_hdr_t dld_hdrs[DLD_BUFFER_NUM] = {
		{GENERAL_LOG_HDR, LOG_DUMP_SECTION_GENERAL},
		{PRESERVE_LOG_HDR, LOG_DUMP_SECTION_PRESERVE},
		{SPECIAL_LOG_HDR, LOG_DUMP_SECTION_SPECIAL}
};
static int dld_buf_size[DLD_BUFFER_NUM] = {
		LOG_DUMP_GENERAL_MAX_BUFSIZE,	/* DLD_BUF_TYPE_GENERAL */
		LOG_DUMP_PRESERVE_MAX_BUFSIZE,	/* DLD_BUF_TYPE_PRESERVE */
		LOG_DUMP_SPECIAL_MAX_BUFSIZE,	/* DLD_BUF_TYPE_SPECIAL */
};

int logdump_max_filesize = LOG_DUMP_MAX_FILESIZE;
module_param(logdump_max_filesize, int, 0644);
int logdump_max_bufsize = LOG_DUMP_GENERAL_MAX_BUFSIZE;
module_param(logdump_max_bufsize, int, 0644);
int logdump_periodic_flush = FALSE;
module_param(logdump_periodic_flush, int, 0644);
#ifdef EWP_ECNTRS_LOGGING
int logdump_ecntr_enable = TRUE;
#else
int logdump_ecntr_enable = FALSE;
#endif /* EWP_ECNTRS_LOGGING */
module_param(logdump_ecntr_enable, int, 0644);
#ifdef EWP_RTT_LOGGING
int logdump_rtt_enable = TRUE;
#else
int logdump_rtt_enable = FALSE;
#endif /* EWP_RTT_LOGGING */

int logdump_prsrv_tailsize = DHD_LOG_DUMP_MAX_TAIL_FLUSH_SIZE;

#ifdef DHD_DEBUGABILITY_DEBUG_DUMP
static dhd_debug_dump_ring_entry_t dhd_debug_dump_ring_map[] = {
	{LOG_DUMP_SECTION_TIMESTAMP, DEBUG_DUMP_RING1_ID},
	{LOG_DUMP_SECTION_ECNTRS, DEBUG_DUMP_RING2_ID},
	{LOG_DUMP_SECTION_STATUS, DEBUG_DUMP_RING1_ID},
	{LOG_DUMP_SECTION_RTT, DEBUG_DUMP_RING2_ID},
	{LOG_DUMP_SECTION_PKTID_MAP_LOG, DEBUG_DUMP_RING2_ID},
	{LOG_DUMP_SECTION_PKTID_UNMAP_LOG, DEBUG_DUMP_RING2_ID},
	{LOG_DUMP_SECTION_DHD_DUMP, DEBUG_DUMP_RING1_ID},
	{LOG_DUMP_SECTION_EXT_TRAP, DEBUG_DUMP_RING1_ID},
	{LOG_DUMP_SECTION_HEALTH_CHK, DEBUG_DUMP_RING1_ID},
	{LOG_DUMP_SECTION_COOKIE, DEBUG_DUMP_RING1_ID},
	{LOG_DUMP_SECTION_RING, DEBUG_DUMP_RING1_ID},
};
#endif /* DHD_DEBUGABILITY_DEBUG_DUMP */

int
dhd_log_flush(dhd_pub_t *dhdp, log_dump_type_t *type)
{
	unsigned long flags = 0;
#ifdef EWP_EDL
	int i = 0;
#endif /* EWP_EDL */
	dhd_info_t *dhd_info = NULL;

	BCM_REFERENCE(dhd_info);

	/* if dhdp is null, its extremely unlikely that log dump will be scheduled
	 * so not freeing 'type' here is ok, even if we want to free 'type'
	 * we cannot do so, since 'dhdp->osh' is unavailable
	 * as dhdp is null
	 */
	if (!dhdp || !type) {
		if (dhdp) {
			DHD_GENERAL_LOCK(dhdp, flags);
			DHD_BUS_BUSY_CLEAR_IN_LOGDUMP(dhdp);
			dhd_os_busbusy_wake(dhdp);
			DHD_GENERAL_UNLOCK(dhdp, flags);
		}
		return BCME_ERROR;
	}

#if defined(BCMPCIE)
	if (dhd_bus_get_linkdown(dhdp)) {
		/* As link is down donot collect any data over PCIe.
		 * Also return BCME_OK to caller, so that caller can
		 * dump all the outstanding data to file
		 */
		return BCME_OK;
	}
#endif /* BCMPCIE */

	dhd_info = (dhd_info_t *)dhdp->info;
	/* in case of trap get preserve logs from ETD */
#if defined(BCMPCIE) && defined(EWP_ETD_PRSRV_LOGS)
	if (dhdp->dongle_trap_occured &&
			dhdp->extended_trap_data) {
		dhdpcie_get_etd_preserve_logs(dhdp, (uint8 *)dhdp->extended_trap_data,
				&dhd_info->event_data);
	}
#endif /* BCMPCIE */

	/* flush the event work items to get any fw events/logs
	 * flush_work is a blocking call
	 */
#ifdef SHOW_LOGTRACE
#ifdef EWP_EDL
	if (dhd_info->pub.dongle_edl_support) {
		/* wait till existing edl items are processed */
		dhd_flush_logtrace_process(dhd_info);
		/* dhd_flush_logtrace_process will ensure the work items in the ring
		* (EDL ring) from rd to wr are processed. But if wr had
		* wrapped around, only the work items from rd to ring-end are processed.
		* So to ensure that the work items at the
		* beginning of ring are also processed in the wrap around case, call
		* it twice
		*/
		for (i = 0; i < 2; i++) {
			/* blocks till the edl items are processed */
			dhd_flush_logtrace_process(dhd_info);
		}
	} else {
		dhd_flush_logtrace_process(dhd_info);
	}
#else
	dhd_flush_logtrace_process(dhd_info);
#endif /* EWP_EDL */
#endif /* SHOW_LOGTRACE */

	return BCME_OK;
}

void
dhd_log_dump(void *handle, void *event_info, u8 event)
{
	dhd_info_t *dhd = handle;
	log_dump_type_t *type = (log_dump_type_t *)event_info;

	if (!dhd || !type) {
		DHD_ERROR(("%s: dhd/type is NULL\n", __FUNCTION__));
		return;
	}

#ifdef WL_CFG80211
	/* flush the fw preserve logs */
	wl_flush_fw_log_buffer(dhd_linux_get_primary_netdev(&dhd->pub),
		FW_LOGSET_MASK_ALL);
#endif

	/* there are currently 3 possible contexts from which
	 * log dump can be scheduled -
	 * 1.TRAP 2.supplicant DEBUG_DUMP pvt driver command
	 * 3.HEALTH CHECK event
	 * The concise debug info buffer is a shared resource
	 * and in case a trap is one of the contexts then both the
	 * scheduled work queues need to run because trap data is
	 * essential for debugging. Hence a mutex lock is acquired
	 * before calling do_dhd_log_dump().
	 */
	DHD_ERROR(("%s: calling log dump.. \n", __FUNCTION__));
	dhd_os_logdump_lock(&dhd->pub);
	DHD_OS_WAKE_LOCK(&dhd->pub);
	if (do_dhd_log_dump(&dhd->pub, type) != BCME_OK) {
		DHD_ERROR(("%s: writing debug dump to the file failed\n", __FUNCTION__));
	}
	DHD_OS_WAKE_UNLOCK(&dhd->pub);
	dhd_os_logdump_unlock(&dhd->pub);
}

void dhd_schedule_log_dump(dhd_pub_t *dhdp, void *type)
{
	DHD_ERROR(("%s: scheduling log dump.. \n", __FUNCTION__));

	dhd_deferred_schedule_work(dhdp->info->dhd_deferred_wq,
		type, DHD_WQ_WORK_DHD_LOG_DUMP,
		dhd_log_dump, DHD_WQ_WORK_PRIORITY_HIGH);
}

void
dhd_print_buf_addr(dhd_pub_t *dhdp, char *name, void *buf, unsigned int size)
{
#ifdef DHD_FW_COREDUMP
	if ((dhdp->memdump_enabled == DUMP_MEMONLY) ||
		(dhdp->memdump_enabled == DUMP_MEMFILE_BUGON) ||
		(dhdp->memdump_type == DUMP_TYPE_SMMU_FAULT) ||
#ifdef DHD_DETECT_CONSECUTIVE_MFG_HANG
		(dhdp->op_mode & DHD_FLAG_MFG_MODE &&
			(dhdp->hang_count >= MAX_CONSECUTIVE_MFG_HANG_COUNT-1)) ||
#endif /* DHD_DETECT_CONSECUTIVE_MFG_HANG */
		FALSE)
#else
	if (dhdp->memdump_type == DUMP_TYPE_SMMU_FAULT)
#endif
	{
#if defined(CONFIG_ARM64)
		DHD_ERROR(("-------- %s: buf(va)=%llx, buf(pa)=%llx, bufsize=%d\n",
			name, (uint64)buf, (uint64)__virt_to_phys((ulong)buf), size));
#elif defined(__ARM_ARCH_7A__)
		DHD_ERROR(("-------- %s: buf(va)=%x, buf(pa)=%x, bufsize=%d\n",
			name, (uint32)buf, (uint32)__virt_to_phys((ulong)buf), size));
#endif /* __ARM_ARCH_7A__ */
	}
}

void
dhd_log_dump_buf_addr(dhd_pub_t *dhdp, log_dump_type_t *type)
{
	int i;
	unsigned long wr_size = 0;
	struct dhd_log_dump_buf *dld_buf = &g_dld_buf[0];
	size_t log_size = 0;
	char buf_name[DHD_PRINT_BUF_NAME_LEN];
	dhd_dbg_ring_t *ring = NULL;

	BCM_REFERENCE(ring);

	for (i = 0; i < DLD_BUFFER_NUM; i++) {
		dld_buf = &g_dld_buf[i];
		log_size = (unsigned long)dld_buf->max -
			(unsigned long)dld_buf->buffer;
		if (dld_buf->wraparound) {
			wr_size = log_size;
		} else {
			wr_size = (unsigned long)dld_buf->present -
				(unsigned long)dld_buf->front;
		}
		scnprintf(buf_name, sizeof(buf_name), "dlb_buf[%d]", i);
		dhd_print_buf_addr(dhdp, buf_name, dld_buf, dld_buf_size[i]);
		scnprintf(buf_name, sizeof(buf_name), "dlb_buf[%d] buffer", i);
		dhd_print_buf_addr(dhdp, buf_name, dld_buf->buffer, wr_size);
		scnprintf(buf_name, sizeof(buf_name), "dlb_buf[%d] present", i);
		dhd_print_buf_addr(dhdp, buf_name, dld_buf->present, wr_size);
		scnprintf(buf_name, sizeof(buf_name), "dlb_buf[%d] front", i);
		dhd_print_buf_addr(dhdp, buf_name, dld_buf->front, wr_size);
	}

#ifdef DEBUGABILITY_ECNTRS_LOGGING
	/* periodic flushing of ecounters is NOT supported */
	if (*type == DLD_BUF_TYPE_ALL &&
			logdump_ecntr_enable &&
			dhdp->ecntr_dbg_ring) {

		ring = (dhd_dbg_ring_t *)dhdp->ecntr_dbg_ring;
		dhd_print_buf_addr(dhdp, "ecntr_dbg_ring", ring, LOG_DUMP_ECNTRS_MAX_BUFSIZE);
		dhd_print_buf_addr(dhdp, "ecntr_dbg_ring ring_buf", ring->ring_buf,
				LOG_DUMP_ECNTRS_MAX_BUFSIZE);
	}
#endif /* DEBUGABILITY_ECNTRS_LOGGING */

#if defined(BCMPCIE)
	if (dhdp->dongle_trap_occured && dhdp->extended_trap_data) {
		dhd_print_buf_addr(dhdp, "extended_trap_data", dhdp->extended_trap_data,
				BCMPCIE_EXT_TRAP_DATA_MAXLEN);
	}
#endif /* BCMPCIE */

#if defined(DHD_FW_COREDUMP) && defined(DNGL_EVENT_SUPPORT)
	/* if health check event was received */
	if (dhdp->memdump_type == DUMP_TYPE_DONGLE_HOST_EVENT) {
		dhd_print_buf_addr(dhdp, "health_chk_event_data", dhdp->health_chk_event_data,
				HEALTH_CHK_BUF_SIZE);
	}
#endif /* DHD_FW_COREDUMP && DNGL_EVENT_SUPPORT */

	/* append the concise debug information */
	if (dhdp->concise_dbg_buf) {
		dhd_print_buf_addr(dhdp, "concise_dbg_buf", dhdp->concise_dbg_buf,
				CONCISE_DUMP_BUFLEN);
	}
}

#ifdef DHD_SSSR_DUMP
#ifdef DHD_COREDUMP
extern dhd_coredump_t dhd_coredump_types[];
#endif /* DHD_COREDUMP */
int
dhdpcie_sssr_dump_get_before_after_len(dhd_pub_t *dhd, uint32 *arr_len)
{
	int i = 0;
	uint dig_buf_size = 0;

	DHD_ERROR(("%s\n", __FUNCTION__));

	/* core 0 */
	i = 0;
#ifdef DHD_SSSR_DUMP_BEFORE_SR
	if (dhd->sssr_d11_before[i] && dhd->sssr_d11_outofreset[i] &&
		(dhd->sssr_dump_mode == SSSR_DUMP_MODE_SSSR)) {
#ifdef DHD_COREDUMP
		dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE0_BEFORE].length =
#endif /* DHD_COREDUMP */
			arr_len[SSSR_C0_D11_BEFORE] = dhd_sssr_mac_buf_size(dhd, i);
		DHD_ERROR(("%s: arr_len[SSSR_C0_D11_BEFORE] : %d\n", __FUNCTION__,
			arr_len[SSSR_C0_D11_BEFORE]));
#ifdef DHD_LOG_DUMP
		dhd_print_buf_addr(dhd, "SSSR_C0_D11_BEFORE",
			dhd->sssr_d11_before[i], arr_len[SSSR_C0_D11_BEFORE]);
#endif /* DHD_LOG_DUMP */
#ifdef DHD_COREDUMP
		dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE0_BEFORE].bufptr =
			dhd->sssr_d11_before[i];
#endif /* DHD_COREDUMP */
	}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
	if (dhd->sssr_d11_after[i] && dhd->sssr_d11_outofreset[i]) {
#ifdef DHD_COREDUMP
		dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE0_AFTER].length =
#endif /* DHD_COREDUMP */
			arr_len[SSSR_C0_D11_AFTER] = dhd_sssr_mac_buf_size(dhd, i);
		DHD_ERROR(("%s: arr_len[SSSR_C0_D11_AFTER] : %d\n", __FUNCTION__,
			arr_len[SSSR_C0_D11_AFTER]));
#ifdef DHD_LOG_DUMP
		dhd_print_buf_addr(dhd, "SSSR_C0_D11_AFTER",
			dhd->sssr_d11_after[i], arr_len[SSSR_C0_D11_AFTER]);
#endif /* DHD_LOG_DUMP */
#ifdef DHD_COREDUMP
		dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE0_AFTER].bufptr =
			dhd->sssr_d11_after[i];
#endif /* DHD_COREDUMP */
	}

	/* core 1 */
	i = 1;
#ifdef DHD_SSSR_DUMP_BEFORE_SR
	if (dhd->sssr_d11_before[i] && dhd->sssr_d11_outofreset[i] &&
		(dhd->sssr_dump_mode == SSSR_DUMP_MODE_SSSR)) {
#ifdef DHD_COREDUMP
		dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE1_BEFORE].length =
#endif /* DHD_COREDUMP */
			arr_len[SSSR_C1_D11_BEFORE] = dhd_sssr_mac_buf_size(dhd, i);
		DHD_ERROR(("%s: arr_len[SSSR_C1_D11_BEFORE] : %d\n", __FUNCTION__,
			arr_len[SSSR_C1_D11_BEFORE]));
#ifdef DHD_LOG_DUMP
		dhd_print_buf_addr(dhd, "SSSR_C1_D11_BEFORE",
			dhd->sssr_d11_before[i], arr_len[SSSR_C1_D11_BEFORE]);
#endif /* DHD_LOG_DUMP */
#ifdef DHD_COREDUMP
		dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE1_BEFORE].bufptr =
			dhd->sssr_d11_before[i];
#endif /* DHD_COREDUMP */
	}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
	if (dhd->sssr_d11_after[i] && dhd->sssr_d11_outofreset[i]) {
#ifdef DHD_COREDUMP
		dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE1_AFTER].length =
#endif /* DHD_COREDUMP */
			arr_len[SSSR_C1_D11_AFTER] = dhd_sssr_mac_buf_size(dhd, i);
		DHD_ERROR(("%s: arr_len[SSSR_C1_D11_AFTER] : %d\n", __FUNCTION__,
			arr_len[SSSR_C1_D11_AFTER]));
#ifdef DHD_LOG_DUMP
		dhd_print_buf_addr(dhd, "SSSR_C1_D11_AFTER",
			dhd->sssr_d11_after[i], arr_len[SSSR_C1_D11_AFTER]);
#endif /* DHD_LOG_DUMP */
#ifdef DHD_COREDUMP
		dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE1_AFTER].bufptr =
			dhd->sssr_d11_after[i];
#endif /* DHD_COREDUMP */
	}

	/* core 2 scan core */
	if (dhd->sssr_reg_info->rev2.version >= SSSR_REG_INFO_VER_2) {
		i = 2;
#ifdef DHD_SSSR_DUMP_BEFORE_SR
		if (dhd->sssr_d11_before[i] && dhd->sssr_d11_outofreset[i] &&
			(dhd->sssr_dump_mode == SSSR_DUMP_MODE_SSSR)) {
#ifdef DHD_COREDUMP
			dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE2_BEFORE].length =
#endif /* DHD_COREDUMP */
				arr_len[SSSR_C2_D11_BEFORE] = dhd_sssr_mac_buf_size(dhd, i);
			DHD_ERROR(("%s: arr_len[SSSR_C2_D11_BEFORE] : %d\n", __FUNCTION__,
				arr_len[SSSR_C2_D11_BEFORE]));
#ifdef DHD_LOG_DUMP
			dhd_print_buf_addr(dhd, "SSSR_C2_D11_BEFORE",
				dhd->sssr_d11_before[i], arr_len[SSSR_C2_D11_BEFORE]);
#endif /* DHD_LOG_DUMP */
#ifdef DHD_COREDUMP
		dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE2_BEFORE].bufptr =
			dhd->sssr_d11_before[i];
#endif /* DHD_COREDUMP */
		}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
		if (dhd->sssr_d11_after[i] && dhd->sssr_d11_outofreset[i]) {
#ifdef DHD_COREDUMP
			dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE2_AFTER].length =
#endif /* DHD_COREDUMP */
				arr_len[SSSR_C2_D11_AFTER]  = dhd_sssr_mac_buf_size(dhd, i);
			DHD_ERROR(("%s: arr_len[SSSR_C2_D11_AFTER] : %d\n", __FUNCTION__,
				arr_len[SSSR_C2_D11_AFTER]));
#ifdef DHD_LOG_DUMP
			dhd_print_buf_addr(dhd, "SSSR_C2_D11_AFTER",
				dhd->sssr_d11_after[i], arr_len[SSSR_C2_D11_AFTER]);
#endif /* DHD_LOG_DUMP */
#ifdef DHD_COREDUMP
			dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_CORE2_AFTER].bufptr =
				dhd->sssr_d11_after[i];
#endif /* DHD_COREDUMP */
		}
	}

	/* DIG core or VASIP */
	dig_buf_size = dhd_sssr_dig_buf_size(dhd);
#ifdef DHD_SSSR_DUMP_BEFORE_SR
#ifdef DHD_COREDUMP
	dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_DIG_BEFORE].length =
#endif /* DHD_COREDUMP */
		arr_len[SSSR_DIG_BEFORE] = (dhd->sssr_dig_buf_before) ? dig_buf_size : 0;
	DHD_ERROR(("%s: arr_len[SSSR_DIG_BEFORE] : %d\n", __FUNCTION__,
		arr_len[SSSR_DIG_BEFORE]));
#ifdef DHD_LOG_DUMP
	if (dhd->sssr_dig_buf_before) {
		dhd_print_buf_addr(dhd, "SSSR_DIG_BEFORE",
			dhd->sssr_dig_buf_before, arr_len[SSSR_DIG_BEFORE]);
	}
#endif /* DHD_LOG_DUMP */
#ifdef DHD_COREDUMP
	dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_DIG_BEFORE].bufptr =
		dhd->sssr_dig_buf_before;
#endif /* DHD_COREDUMP */
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

#ifdef DHD_COREDUMP
	dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_DIG_AFTER].length =
#endif /* DHD_COREDUMP */
		arr_len[SSSR_DIG_AFTER] = (dhd->sssr_dig_buf_after) ? dig_buf_size : 0;
	DHD_ERROR(("%s: arr_len[SSSR_DIG_AFTER] : %d\n", __FUNCTION__,
		arr_len[SSSR_DIG_AFTER]));
#ifdef DHD_LOG_DUMP
	if (dhd->sssr_dig_buf_after) {
		dhd_print_buf_addr(dhd, "SSSR_DIG_AFTER",
			dhd->sssr_dig_buf_after, arr_len[SSSR_DIG_AFTER]);
	}
#endif /* DHD_LOG_DUMP */
#ifdef DHD_COREDUMP
	dhd_coredump_types[DHD_COREDUMP_TYPE_SSSRDUMP_DIG_AFTER].bufptr =
		dhd->sssr_dig_buf_after;
#endif /* DHD_COREDUMP */

	return BCME_OK;
}

void
dhd_nla_put_sssr_dump_len(void *ndev, uint32 *arr_len)
{
	dhd_info_t *dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)ndev);
	dhd_pub_t *dhdp = &dhd_info->pub;

	if (dhdp->sssr_dump_collected) {
		dhdpcie_sssr_dump_get_before_after_len(dhdp, arr_len);
	}
}
#endif /* DHD_SSSR_DUMP */

uint32
dhd_get_time_str_len(void)
{
	char *ts = NULL, time_str[128];

	ts = dhd_log_dump_get_timestamp();
	snprintf(time_str, sizeof(time_str),
			"\n\n ========== LOG DUMP TAKEN AT : %s =========\n", ts);
	return strlen(time_str);
}

#if defined(BCMPCIE)
uint32
dhd_get_ext_trap_len(void *ndev, dhd_pub_t *dhdp)
{
	int length = 0;
	log_dump_section_hdr_t sec_hdr;
	dhd_info_t *dhd_info;

	if (ndev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)ndev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return length;

	if (dhdp->extended_trap_data) {
		length = (strlen(EXT_TRAP_LOG_HDR)
					+ sizeof(sec_hdr) + BCMPCIE_EXT_TRAP_DATA_MAXLEN);
	}
	return length;
}
#endif /* BCMPCIE */

#if defined(DHD_FW_COREDUMP) && defined(DNGL_EVENT_SUPPORT)
uint32
dhd_get_health_chk_len(void *ndev, dhd_pub_t *dhdp)
{
	int length = 0;
	log_dump_section_hdr_t sec_hdr;
	dhd_info_t *dhd_info;

	if (ndev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)ndev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return length;

	if (dhdp->memdump_type == DUMP_TYPE_DONGLE_HOST_EVENT) {
		length = (strlen(HEALTH_CHK_LOG_HDR)
			+ sizeof(sec_hdr) + HEALTH_CHK_BUF_SIZE);
	}
	return length;
}
#endif /* DHD_FW_COREDUMP && DNGL_EVENT_SUPPORT */

uint32
dhd_get_dhd_dump_len(void *ndev, dhd_pub_t *dhdp)
{
	uint32 length = 0;
	log_dump_section_hdr_t sec_hdr;
	dhd_info_t *dhd_info;
	int remain_len = 0;

	if (ndev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)ndev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return length;

	if (dhdp->concise_dbg_buf) {
		remain_len = dhd_dump(dhdp, (char *)dhdp->concise_dbg_buf, CONCISE_DUMP_BUFLEN);
		 if (remain_len <= 0 || remain_len >= CONCISE_DUMP_BUFLEN) {
			DHD_ERROR(("%s: error getting concise debug info ! remain_len: %d\n",
				__FUNCTION__, remain_len));
			return length;
		}

		length += (uint32)(CONCISE_DUMP_BUFLEN - remain_len);
	}

	length += (uint32)(strlen(DHD_DUMP_LOG_HDR) + sizeof(sec_hdr));
	return length;
}

#ifdef EWP_RTT_LOGGING
uint32
dhd_get_rtt_len(void *ndev, dhd_pub_t *dhdp)
{
	dhd_info_t *dhd_info;
	log_dump_section_hdr_t sec_hdr;
	int length = 0;
	dhd_dbg_ring_t *ring;

	if (ndev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)ndev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return length;

	if (logdump_rtt_enable && dhdp->rtt_dbg_ring) {
		ring = (dhd_dbg_ring_t *)dhdp->rtt_dbg_ring;
		length = ring->ring_size + strlen(RTT_LOG_HDR) + sizeof(sec_hdr);
	}
	return length;
}
#endif /* EWP_RTT_LOGGING */

uint32
dhd_get_cookie_log_len(void *ndev, dhd_pub_t *dhdp)
{
	int length = 0;
	dhd_info_t *dhd_info;

	if (ndev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)ndev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return length;

	if (dhdp->logdump_cookie && dhd_logdump_cookie_count(dhdp) > 0) {
		length = dhd_log_dump_cookie_len(dhdp);
	}
	return length;

}

#ifdef DHD_DUMP_PCIE_RINGS
uint32
dhd_get_flowring_len(void *ndev, dhd_pub_t *dhdp)
{
	uint32 length = 0;
	log_dump_section_hdr_t sec_hdr;
	dhd_info_t *dhd_info;
	uint16 max_tx_flowrings;

	if (ndev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)ndev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return length;

	length += (uint32) strlen(RING_DUMP_HDR);
	length += (uint32) sizeof(sec_hdr);
	length += (uint32) sizeof(max_tx_flowrings);
	max_tx_flowrings = dhd_get_max_flow_rings(dhdp);
	/* max_item and item_size value which is of 4bytes is dumped at
	 * start of each ring dump, so adding 4bytes to total length.
	 */
	length += (uint32) ((D2HRING_TXCMPLT_ITEMSIZE * d2h_max_txcpl)
				+ (sizeof(uint16) * 2)
				+ (H2DRING_RXPOST_ITEMSIZE * h2d_max_rxpost)
				+ (sizeof(uint16) * 2)
				+ (D2HRING_RXCMPLT_ITEMSIZE * d2h_max_rxcpl)
				+ (sizeof(uint16) * 2)
				+ (H2DRING_CTRL_SUB_ITEMSIZE * h2d_max_ctrlpost)
				+ (sizeof(uint16) * 2)
				+ (D2HRING_CTRL_CMPLT_ITEMSIZE * d2h_max_ctrlcpl)
				+ (sizeof(uint16) * 2)
#ifdef EWP_EDL
				/* EDL ring doesn't have max_item and item_size */
				+ (D2HRING_EDL_HDR_SIZE * D2HRING_EDL_MAX_ITEM));
#else
				+ (H2DRING_INFO_BUFPOST_ITEMSIZE * H2DRING_DYNAMIC_INFO_MAX_ITEM)
				+ (sizeof(uint16) * 2)
				+ (D2HRING_INFO_BUFCMPLT_ITEMSIZE * D2HRING_DYNAMIC_INFO_MAX_ITEM)
				+ (sizeof(uint16) * 2));
#endif /* EWP_EDL */

	if (dhdp->htput_support) {
		/* flowring lengths are different for HTPUT rings, handle accordingly */
		length += ((H2DRING_TXPOST_ITEMSIZE * h2d_htput_max_txpost *
			HTPUT_TOTAL_FLOW_RINGS) +
			(H2DRING_TXPOST_ITEMSIZE * h2d_max_txpost *
			(max_tx_flowrings - HTPUT_TOTAL_FLOW_RINGS)));
	} else {
		length += (H2DRING_TXPOST_ITEMSIZE * h2d_max_txpost *
			max_tx_flowrings);
	}
	length += max_tx_flowrings * (sizeof(uint16) * 2);

	return length;
}
#endif /* DHD_DUMP_PCIE_RINGS */

#ifdef EWP_ECNTRS_LOGGING
uint32
dhd_get_ecntrs_len(void *ndev, dhd_pub_t *dhdp)
{
	dhd_info_t *dhd_info;
	log_dump_section_hdr_t sec_hdr;
	int length = 0;
	dhd_dbg_ring_t *ring;

	if (ndev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)ndev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return length;

	if (logdump_ecntr_enable && dhdp->ecntr_dbg_ring) {
		ring = (dhd_dbg_ring_t *)dhdp->ecntr_dbg_ring;
		length = ring->ring_size + strlen(ECNTRS_LOG_HDR) + sizeof(sec_hdr);
	}
	return length;
}
#endif /* EWP_ECNTRS_LOGGING */

int
dhd_get_dld_log_dump(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, int type, void *pos)
{
	int ret = BCME_OK;
	struct dhd_log_dump_buf *dld_buf;
	log_dump_section_hdr_t sec_hdr;
	dhd_info_t *dhd_info;

	dld_buf = &g_dld_buf[type];

	if (dev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)dev);
		dhdp = &dhd_info->pub;
	} else if (!dhdp) {
		return BCME_ERROR;
	}

	DHD_ERROR(("%s: ENTER \n", __FUNCTION__));

	dhd_init_sec_hdr(&sec_hdr);

	/* write the section header first */
	ret = dhd_export_debug_data(dld_hdrs[type].hdr_str, fp, user_buf,
		strlen(dld_hdrs[type].hdr_str), pos);
	if (ret < 0)
		goto exit;
	len -= (uint32)strlen(dld_hdrs[type].hdr_str);
	len -= (uint32)sizeof(sec_hdr);
	sec_hdr.type = dld_hdrs[type].sec_type;
	sec_hdr.length = len;
	ret = dhd_export_debug_data((char *)&sec_hdr, fp, user_buf, sizeof(sec_hdr), pos);
	if (ret < 0)
		goto exit;
	ret = dhd_export_debug_data(dld_buf->buffer, fp, user_buf, len, pos);
	if (ret < 0)
		goto exit;

exit:
	return ret;
}

int
dhd_get_debug_dump_file_name(void *dev, dhd_pub_t *dhdp, char *dump_path, int size)
{
	int ret;
	int len = 0;
	dhd_info_t *dhd_info;

	if (dev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)dev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return BCME_ERROR;

	memset(dump_path, 0, size);

	ret = snprintf(dump_path, size, "%s",
			DHD_COMMON_DUMP_PATH DHD_DEBUG_DUMP_TYPE);
	len += ret;

	/* Keep the same timestamp across different dump logs */
	if (!dhdp->logdump_periodic_flush) {
		struct rtc_time tm;
#ifdef DHD_LOG_DUMP
		clear_debug_dump_time(dhdp->debug_dump_time_str);
		get_debug_dump_time(dhdp->debug_dump_time_str);
#endif /* DHD_LOG_DUMP */
		sscanf(dhdp->debug_dump_time_str, DHD_LOG_DUMP_TS_FMT_YYMMDDHHMMSS,
			&tm.tm_year, &tm.tm_mon, &tm.tm_mday,
			&tm.tm_hour, &tm.tm_min, &tm.tm_sec);
		ret = snprintf(dump_path + len, size - len, "_" DHD_LOG_DUMP_TS_FMT_YYMMDDHHMMSS,
				tm.tm_year, tm.tm_mon, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);
		len += ret;
	}

	ret = 0;
	switch (dhdp->debug_dump_subcmd) {
	case CMD_UNWANTED:
		ret = snprintf(dump_path + len, size - len, "%s", DHD_DUMP_SUBSTR_UNWANTED);
		break;
	case CMD_DISCONNECTED:
		ret = snprintf(dump_path + len, size - len, "%s", DHD_DUMP_SUBSTR_DISCONNECTED);
		break;
	default:
		break;
	}
	len += ret;

	return BCME_OK;
}

uint32
dhd_get_dld_len(int log_type)
{
	unsigned long wr_size = 0;
	unsigned long buf_size = 0;
	unsigned long flags = 0;
	struct dhd_log_dump_buf *dld_buf;
	log_dump_section_hdr_t sec_hdr;

	/* calculate the length of the log */
	dld_buf = &g_dld_buf[log_type];
	buf_size = (unsigned long)dld_buf->max -
			(unsigned long)dld_buf->buffer;

	if (dld_buf->wraparound) {
		wr_size = buf_size;
	} else {
		/* need to hold the lock before accessing 'present' and 'remain' ptrs */
		DHD_LOG_DUMP_BUF_LOCK(&dld_buf->lock, flags);
		wr_size = (unsigned long)dld_buf->present -
				(unsigned long)dld_buf->front;
		DHD_LOG_DUMP_BUF_UNLOCK(&dld_buf->lock, flags);
	}
	return (wr_size + sizeof(sec_hdr) + strlen(dld_hdrs[log_type].hdr_str));
}

static void
dhd_get_time_str(dhd_pub_t *dhdp, char *time_str, int size)
{
	char *ts = NULL;
	memset(time_str, 0, size);
	ts = dhd_log_dump_get_timestamp();
	snprintf(time_str, size,
			"\n\n ========== LOG DUMP TAKEN AT : %s =========\n", ts);
}

int
dhd_print_time_str(const void *user_buf, void *fp, uint32 len, void *pos)
{
	char *ts = NULL;
	int ret = 0;
	char time_str[128];

	memset_s(time_str, sizeof(time_str), 0, sizeof(time_str));
	ts = dhd_log_dump_get_timestamp();
	snprintf(time_str, sizeof(time_str),
			"\n\n ========== LOG DUMP TAKEN AT : %s =========\n", ts);

	/* write the timestamp hdr to the file first */
	ret = dhd_export_debug_data(time_str, fp, user_buf, strlen(time_str), pos);
	if (ret < 0) {
		DHD_ERROR(("write file error, err = %d\n", ret));
	}
	return ret;
}

#if defined(DHD_FW_COREDUMP) && defined(DNGL_EVENT_SUPPORT)
int
dhd_print_health_chk_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos)
{
	int ret = BCME_OK;
	log_dump_section_hdr_t sec_hdr;
	dhd_info_t *dhd_info;

	if (dev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)dev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return BCME_ERROR;

	dhd_init_sec_hdr(&sec_hdr);

	if (dhdp->memdump_type == DUMP_TYPE_DONGLE_HOST_EVENT) {
		/* write the section header first */
		ret = dhd_export_debug_data(HEALTH_CHK_LOG_HDR, fp, user_buf,
			strlen(HEALTH_CHK_LOG_HDR), pos);
		if (ret < 0)
			goto exit;

		len -= (uint32)strlen(HEALTH_CHK_LOG_HDR);
		sec_hdr.type = LOG_DUMP_SECTION_HEALTH_CHK;
		sec_hdr.length = HEALTH_CHK_BUF_SIZE;
		ret = dhd_export_debug_data((char *)&sec_hdr, fp, user_buf, sizeof(sec_hdr), pos);
		if (ret < 0)
			goto exit;

		len -= (uint32)sizeof(sec_hdr);
		/* write the log */
		ret = dhd_export_debug_data((char *)dhdp->health_chk_event_data, fp,
			user_buf, len, pos);
		if (ret < 0)
			goto exit;
	}
exit:
	return ret;
}
#endif /* DHD_FW_COREDUMP && DNGL_EVENT_SUPPORT */

#if defined(BCMPCIE)
int
dhd_print_ext_trap_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos)
{
	int ret = BCME_OK;
	log_dump_section_hdr_t sec_hdr;
	dhd_info_t *dhd_info;

	if (dev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)dev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return BCME_ERROR;

	dhd_init_sec_hdr(&sec_hdr);

	/* append extended trap data to the file in case of traps */
	if (dhdp->dongle_trap_occured &&
			dhdp->extended_trap_data) {
		/* write the section header first */
		ret = dhd_export_debug_data(EXT_TRAP_LOG_HDR, fp, user_buf,
			strlen(EXT_TRAP_LOG_HDR), pos);
		if (ret < 0)
			goto exit;

		len -= (uint32)strlen(EXT_TRAP_LOG_HDR);
		sec_hdr.type = LOG_DUMP_SECTION_EXT_TRAP;
		sec_hdr.length = BCMPCIE_EXT_TRAP_DATA_MAXLEN;
		ret = dhd_export_debug_data((uint8 *)&sec_hdr, fp, user_buf, sizeof(sec_hdr), pos);
		if (ret < 0)
			goto exit;

		len -= (uint32)sizeof(sec_hdr);
		/* write the log */
		ret = dhd_export_debug_data((uint8 *)dhdp->extended_trap_data, fp,
			user_buf, len, pos);
		if (ret < 0)
			goto exit;
	}
exit:
	return ret;
}
#endif /* BCMPCIE */

int
dhd_print_dump_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos)
{
	int ret = BCME_OK;
	log_dump_section_hdr_t sec_hdr;
	dhd_info_t *dhd_info;

	if (dev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)dev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return BCME_ERROR;

	dhd_init_sec_hdr(&sec_hdr);

	ret = dhd_export_debug_data(DHD_DUMP_LOG_HDR, fp, user_buf, strlen(DHD_DUMP_LOG_HDR), pos);
	if (ret < 0)
		goto exit;

	len -= (uint32)strlen(DHD_DUMP_LOG_HDR);
	sec_hdr.type = LOG_DUMP_SECTION_DHD_DUMP;
	sec_hdr.length = len;
	ret = dhd_export_debug_data((char *)&sec_hdr, fp, user_buf, sizeof(sec_hdr), pos);
	if (ret < 0)
		goto exit;

	len -= (uint32)sizeof(sec_hdr);

	if (dhdp->concise_dbg_buf) {
		dhd_dump(dhdp, (char *)dhdp->concise_dbg_buf, CONCISE_DUMP_BUFLEN);
		ret = dhd_export_debug_data(dhdp->concise_dbg_buf, fp, user_buf, len, pos);
		if (ret < 0)
			goto exit;
	}

exit:
	return ret;
}

int
dhd_print_cookie_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos)
{
	int ret = BCME_OK;
	dhd_info_t *dhd_info;

	if (dev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)dev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return BCME_ERROR;

	if (dhdp->logdump_cookie && dhd_logdump_cookie_count(dhdp) > 0) {
		ret = dhd_log_dump_cookie_to_file(dhdp, fp, user_buf, (unsigned long *)pos);
	}
	return ret;
}

#ifdef DHD_DUMP_PCIE_RINGS
int
dhd_print_flowring_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
		void *fp, uint32 len, void *pos)
{
	log_dump_section_hdr_t sec_hdr;
	int ret = BCME_OK;
	uint16 max_tx_flowrings;
	dhd_info_t *dhd_info;

	if (dev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)dev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return BCME_ERROR;

	dhd_init_sec_hdr(&sec_hdr);

	/* write the section header first */
	ret = dhd_export_debug_data(RING_DUMP_HDR, fp, user_buf,
		strlen(RING_DUMP_HDR), pos);
	if (ret < 0)
		goto exit;
	len -= strlen(RING_DUMP_HDR);

	sec_hdr.type = LOG_DUMP_SECTION_RING;
	sec_hdr.length = len - sizeof(sec_hdr);
	ret = dhd_export_debug_data((char *)&sec_hdr, fp, user_buf, sizeof(sec_hdr), pos);
	if (ret < 0)
		goto exit;

	max_tx_flowrings =  dhd_get_max_flow_rings(dhdp);
	/* write the number of max_tx_flowrings after section header */
	ret = dhd_export_debug_data((char *)&max_tx_flowrings, fp, user_buf,
		sizeof(max_tx_flowrings), pos);
	if (ret < 0)
		goto exit;

	/* write the log */
	ret = dhd_d2h_h2d_ring_dump(dhdp, fp, user_buf, (unsigned long *)pos, TRUE);
	if (ret < 0)
		goto exit;
exit:
	return ret;
}
#endif /* DHD_DUMP_PCIE_RINGS */

#ifdef EWP_ECNTRS_LOGGING
int
dhd_print_ecntrs_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
		void *fp, uint32 len, void *pos)
{
	log_dump_section_hdr_t sec_hdr;
	int ret = BCME_OK;
	dhd_info_t *dhd_info;

	if (dev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)dev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return BCME_ERROR;

	dhd_init_sec_hdr(&sec_hdr);

	if (logdump_ecntr_enable &&
			dhdp->ecntr_dbg_ring) {
		sec_hdr.type = LOG_DUMP_SECTION_ECNTRS;
		ret = dhd_dump_debug_ring(dhdp, dhdp->ecntr_dbg_ring,
				user_buf, &sec_hdr, ECNTRS_LOG_HDR, len, LOG_DUMP_SECTION_ECNTRS);
	}
	return ret;

}
#endif /* EWP_ECNTRS_LOGGING */

#ifdef EWP_RTT_LOGGING
int
dhd_print_rtt_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
		void *fp, uint32 len, void *pos)
{
	log_dump_section_hdr_t sec_hdr;
	int ret = BCME_OK;
	dhd_info_t *dhd_info;

	if (dev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)dev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp)
		return BCME_ERROR;

	dhd_init_sec_hdr(&sec_hdr);

	if (logdump_rtt_enable && dhdp->rtt_dbg_ring) {
		ret = dhd_dump_debug_ring(dhdp, dhdp->rtt_dbg_ring,
				user_buf, &sec_hdr, RTT_LOG_HDR, len, LOG_DUMP_SECTION_RTT);
	}
	return ret;

}
#endif /* EWP_RTT_LOGGING */

#ifdef DHD_STATUS_LOGGING
int
dhd_print_status_log_data(void *dev, dhd_pub_t *dhdp, const void *user_buf,
	void *fp, uint32 len, void *pos)
{
	dhd_info_t *dhd_info;

	if (dev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)dev);
		dhdp = &dhd_info->pub;
	}

	if (!dhdp) {
		return BCME_ERROR;
	}

	return dhd_statlog_write_logdump(dhdp, user_buf, fp, len, pos);
}

uint32
dhd_get_status_log_len(void *ndev, dhd_pub_t *dhdp)
{
	dhd_info_t *dhd_info;
	uint32 length = 0;

	if (ndev) {
		dhd_info = *(dhd_info_t **)netdev_priv((struct net_device *)ndev);
		dhdp = &dhd_info->pub;
	}

	if (dhdp) {
		length = dhd_statlog_get_logbuf_len(dhdp);
	}

	return length;
}
#endif /* DHD_STATUS_LOGGING */

void
dhd_init_sec_hdr(log_dump_section_hdr_t *sec_hdr)
{
	/* prep the section header */
	memset(sec_hdr, 0, sizeof(*sec_hdr));
	sec_hdr->magic = LOG_DUMP_MAGIC;
	sec_hdr->timestamp = local_clock();
}

/* Must hold 'dhd_os_logdump_lock' before calling this function ! */
int
do_dhd_log_dump(dhd_pub_t *dhdp, log_dump_type_t *type)
{
	int ret = 0, i = 0;
	struct file *fp = NULL;
#ifdef get_fs
	mm_segment_t old_fs;
#endif /* get_fs */
	loff_t pos = 0;
	char dump_path[128];
	uint32 file_mode;
	unsigned long flags = 0;
	size_t log_size = 0;
	size_t fspace_remain = 0;
	struct file *filep = NULL;
	int logstrs_size = 0;
	char time_str[128];
	unsigned int len = 0;
	log_dump_section_hdr_t sec_hdr;

	DHD_ERROR(("%s: ENTER \n", __FUNCTION__));

	DHD_GENERAL_LOCK(dhdp, flags);
	if (DHD_BUS_CHECK_DOWN_OR_DOWN_IN_PROGRESS(dhdp)) {
		DHD_GENERAL_UNLOCK(dhdp, flags);
		DHD_ERROR(("%s: bus is down! can't collect log dump. \n", __FUNCTION__));
		goto exit1;
	}
	DHD_BUS_BUSY_SET_IN_LOGDUMP(dhdp);
	DHD_GENERAL_UNLOCK(dhdp, flags);

	if ((ret = dhd_log_flush(dhdp, type)) < 0) {
		goto exit1;
	}
#ifdef get_fs
	/* change to KERNEL_DS address limit */
	old_fs = get_fs();
	set_fs(KERNEL_DS);
#endif /* get_fs */

	dhd_get_debug_dump_file_name(NULL, dhdp, dump_path, sizeof(dump_path));

	DHD_ERROR(("debug_dump_path = %s\n", dump_path));
	DHD_ERROR(("DHD version: %s\n", dhd_version));
	DHD_ERROR(("F/W version: %s\n", fw_version));

	dhd_log_dump_buf_addr(dhdp, type);

	dhd_get_time_str(dhdp, time_str, 128);

	/* if this is the first time after dhd is loaded,
	 * or, if periodic flush is disabled, clear the log file
	 */
	if (!dhdp->logdump_periodic_flush || dhdp->last_file_posn == 0)
		file_mode = O_CREAT | O_WRONLY | O_SYNC | O_TRUNC;
	else
		file_mode = O_CREAT | O_RDWR | O_SYNC;

	fp = dhd_filp_open(dump_path, file_mode, 0664);
	if (IS_ERR(fp) || (fp == NULL)) {
		/* If android installed image, try '/data' directory */
#if defined(CONFIG_X86) && defined(OEM_ANDROID)
		DHD_ERROR(("%s: File open error on Installed android image, trying /data...\n",
			__FUNCTION__));
		snprintf(dump_path, sizeof(dump_path), "/data/" DHD_DEBUG_DUMP_TYPE);
		if (!dhdp->logdump_periodic_flush) {
			snprintf(dump_path + strlen(dump_path),
				sizeof(dump_path) - strlen(dump_path),
				"_%s", dhdp->debug_dump_time_str);
		}
		fp = dhd_filp_open(dump_path, file_mode, 0664);
		if (IS_ERR(fp) || (fp == NULL)) {
			ret = PTR_ERR(fp);
			DHD_ERROR(("open file error, err = %d\n", ret));
			goto exit2;
		}
		DHD_ERROR(("debug_dump_path = %s\n", dump_path));
#else
		ret = PTR_ERR(fp);
		DHD_ERROR(("open file error, err = %d\n", ret));
		goto exit2;
#endif /* CONFIG_X86 && OEM_ANDROID */
	}

	filep = dhd_filp_open(dump_path, O_RDONLY, 0);

	if (IS_ERR(filep) || (filep == NULL)) {
		DHD_ERROR(("file stat error, err = %d\n", ret));
		goto exit2;
	}
	logstrs_size = dhd_vfs_size_read(filep);
	/* if some one else has changed the file */
	if (dhdp->last_file_posn != 0 &&
			logstrs_size < dhdp->last_file_posn) {
		dhdp->last_file_posn = 0;
	}

	/* XXX: periodic flush is disabled by default, if enabled
	 * only periodic flushing of 'GENERAL' log dump buffer
	 * is supported, its not recommended to turn on periodic
	 * flushing, except for developer unit test.
	 */
	if (dhdp->logdump_periodic_flush) {
		log_size = strlen(time_str) + strlen(DHD_DUMP_LOG_HDR) + sizeof(sec_hdr);
		/* calculate the amount of space required to dump all logs */
		for (i = 0; i < DLD_BUFFER_NUM; ++i) {
			if (*type != DLD_BUF_TYPE_ALL && i != *type)
				continue;

			if (g_dld_buf[i].wraparound) {
				log_size += (unsigned long)g_dld_buf[i].max
						- (unsigned long)g_dld_buf[i].buffer;
			} else {
				DHD_LOG_DUMP_BUF_LOCK(&g_dld_buf[i].lock, flags);
				log_size += (unsigned long)g_dld_buf[i].present -
						(unsigned long)g_dld_buf[i].front;
				DHD_LOG_DUMP_BUF_UNLOCK(&g_dld_buf[i].lock, flags);
			}
			log_size += strlen(dld_hdrs[i].hdr_str) + sizeof(sec_hdr);

			if (*type != DLD_BUF_TYPE_ALL && i == *type)
				break;
		}

		ret = generic_file_llseek(fp, dhdp->last_file_posn, SEEK_CUR);
		if (ret < 0) {
			DHD_ERROR(("file seek last posn error ! err = %d \n", ret));
			goto exit2;
		}
		pos = fp->f_pos;

		/* if the max file size is reached, wrap around to beginning of the file
		 * we're treating the file as a large ring buffer
		 */
		fspace_remain = logdump_max_filesize - pos;
		if (log_size > fspace_remain) {
			fp->f_pos -= pos;
			pos = fp->f_pos;
		}
	}

	dhd_print_time_str(0, fp, len, &pos);

	for (i = 0; i < DLD_BUFFER_NUM; ++i) {

		if (*type != DLD_BUF_TYPE_ALL && i != *type)
			continue;

		len = dhd_get_dld_len(i);
		dhd_get_dld_log_dump(NULL, dhdp, 0, fp, len, i, &pos);
		if (*type != DLD_BUF_TYPE_ALL)
			break;
	}

#ifdef EWP_ECNTRS_LOGGING
	if (*type == DLD_BUF_TYPE_ALL &&
			logdump_ecntr_enable &&
			dhdp->ecntr_dbg_ring) {
		dhd_log_dump_ring_to_file(dhdp, dhdp->ecntr_dbg_ring,
				fp, (unsigned long *)&pos,
				&sec_hdr, ECNTRS_LOG_HDR, LOG_DUMP_SECTION_ECNTRS);
	}
#endif /* EWP_ECNTRS_LOGGING */

#ifdef DHD_STATUS_LOGGING
	if (dhdp->statlog) {
		/* write the statlog */
		len = dhd_get_status_log_len(NULL, dhdp);
		if (len) {
			if (dhd_print_status_log_data(NULL, dhdp, 0, fp,
				len, &pos) < 0) {
				goto exit2;
			}
		}
	}
#endif /* DHD_STATUS_LOGGING */

#ifdef DHD_STATUS_LOGGING
	if (dhdp->statlog) {
		dhd_print_buf_addr(dhdp, "statlog_logbuf", dhd_statlog_get_logbuf(dhdp),
			dhd_statlog_get_logbuf_len(dhdp));
	}
#endif /* DHD_STATUS_LOGGING */

#ifdef EWP_RTT_LOGGING
	if (*type == DLD_BUF_TYPE_ALL &&
			logdump_rtt_enable &&
			dhdp->rtt_dbg_ring) {
		dhd_log_dump_ring_to_file(dhdp, dhdp->rtt_dbg_ring,
				fp, (unsigned long *)&pos,
				&sec_hdr, RTT_LOG_HDR, LOG_DUMP_SECTION_RTT);
	}
#endif /* EWP_RTT_LOGGING */

#ifdef EWP_BCM_TRACE
	if (*type == DLD_BUF_TYPE_ALL &&
		dhdp->bcm_trace_dbg_ring) {
		dhd_log_dump_ring_to_file(dhdp, dhdp->bcm_trace_dbg_ring,
				fp, (unsigned long *)&pos,
				&sec_hdr, BCM_TRACE_LOG_HDR, LOG_DUMP_SECTION_BCM_TRACE);
	}
#endif /* EWP_BCM_TRACE */

#ifdef BCMPCIE
	len = dhd_get_ext_trap_len(NULL, dhdp);
	if (len) {
		if (dhd_print_ext_trap_data(NULL, dhdp, 0, fp, len, &pos) < 0)
			goto exit2;
	}
#endif /* BCMPCIE */

#if defined(DHD_FW_COREDUMP) && defined(DNGL_EVENT_SUPPORT)
	len = dhd_get_health_chk_len(NULL, dhdp);
	if (len) {
		if (dhd_print_health_chk_data(NULL, dhdp, 0, fp, len, &pos) < 0)
			goto exit2;
	}
#endif /* DHD_FW_COREDUMP && DNGL_EVENT_SUPPORT */

	len = dhd_get_dhd_dump_len(NULL, dhdp);
	if (len) {
		if (dhd_print_dump_data(NULL, dhdp, 0, fp, len, &pos) < 0)
			goto exit2;
	}

	len = dhd_get_cookie_log_len(NULL, dhdp);
	if (len) {
		if (dhd_print_cookie_data(NULL, dhdp, 0, fp, len, &pos) < 0)
			goto exit2;
	}

#ifdef DHD_DUMP_PCIE_RINGS
	len = dhd_get_flowring_len(NULL, dhdp);
	if (len) {
		if (dhd_print_flowring_data(NULL, dhdp, 0, fp, len, &pos) < 0)
			goto exit2;
	}
#endif

	if (dhdp->logdump_periodic_flush) {
		/* store the last position written to in the file for future use */
		dhdp->last_file_posn = pos;
	}

exit2:
	if (!IS_ERR(fp) && fp != NULL) {
		dhd_filp_close(fp, NULL);
		DHD_ERROR(("%s: Finished writing log dump to file - '%s' \n",
				__FUNCTION__, dump_path));
	}
#ifdef get_fs
	set_fs(old_fs);
#endif /* get_fs */
exit1:
	if (type) {
		MFREE(dhdp->osh, type, sizeof(*type));
	}
	DHD_GENERAL_LOCK(dhdp, flags);
	DHD_BUS_BUSY_CLEAR_IN_LOGDUMP(dhdp);
	dhd_os_busbusy_wake(dhdp);
	DHD_GENERAL_UNLOCK(dhdp, flags);

#ifdef DHD_DUMP_MNGR
	if (ret >= 0) {
		dhd_dump_file_manage_enqueue(dhdp, dump_path, DHD_DEBUG_DUMP_TYPE);
	}
#endif /* DHD_DUMP_MNGR */

	return (ret < 0) ? BCME_ERROR : BCME_OK;
}

bool
dhd_log_dump_ecntr_enabled(void)
{
	return (bool)logdump_ecntr_enable;
}

bool
dhd_log_dump_rtt_enabled(void)
{
	return (bool)logdump_rtt_enable;
}

void
dhd_log_dump_init(dhd_pub_t *dhd)
{
	struct dhd_log_dump_buf *dld_buf, *dld_buf_special;
	int i = 0;
	uint8 *prealloc_buf = NULL, *bufptr = NULL;
#if defined(CONFIG_DHD_USE_STATIC_BUF) && defined(DHD_USE_STATIC_MEMDUMP)
	int prealloc_idx = DHD_PREALLOC_DHD_LOG_DUMP_BUF;
#endif /* CONFIG_DHD_USE_STATIC_BUF && DHD_USE_STATIC_MEMDUMP */
	int ret;
	dhd_dbg_ring_t *ring = NULL;
	unsigned long flags = 0;
	dhd_info_t *dhd_info = dhd->info;
#if defined(EWP_ECNTRS_LOGGING)
	void *cookie_buf = NULL;
#endif

	BCM_REFERENCE(ret);
	BCM_REFERENCE(ring);
	BCM_REFERENCE(flags);

	/* sanity check */
	if (logdump_prsrv_tailsize <= 0 ||
		logdump_prsrv_tailsize > DHD_LOG_DUMP_MAX_TAIL_FLUSH_SIZE) {
		logdump_prsrv_tailsize = DHD_LOG_DUMP_MAX_TAIL_FLUSH_SIZE;
	}
	/* now adjust the preserve log flush size based on the
	* kernel printk log buffer size
	*/
#ifdef CONFIG_LOG_BUF_SHIFT
	DHD_ERROR(("%s: kernel log buf size = %uKB; logdump_prsrv_tailsize = %uKB;"
		" limit prsrv tail size to = %uKB\n",
		__FUNCTION__, (1 << CONFIG_LOG_BUF_SHIFT)/1024,
		logdump_prsrv_tailsize/1024, LOG_DUMP_KERNEL_TAIL_FLUSH_SIZE/1024));

	if (logdump_prsrv_tailsize > LOG_DUMP_KERNEL_TAIL_FLUSH_SIZE) {
		logdump_prsrv_tailsize = LOG_DUMP_KERNEL_TAIL_FLUSH_SIZE;
	}
#else
	DHD_ERROR(("%s: logdump_prsrv_tailsize = %uKB \n",
		__FUNCTION__, logdump_prsrv_tailsize/1024));
#endif /* CONFIG_LOG_BUF_SHIFT */

	mutex_init(&dhd_info->logdump_lock);
	/* initialize log dump buf structures */
	memset(g_dld_buf, 0, sizeof(struct dhd_log_dump_buf) * DLD_BUFFER_NUM);

	/* set the log dump buffer size based on the module_param */
	if (logdump_max_bufsize > LOG_DUMP_GENERAL_MAX_BUFSIZE ||
			logdump_max_bufsize <= 0)
		dld_buf_size[DLD_BUF_TYPE_GENERAL] = LOG_DUMP_GENERAL_MAX_BUFSIZE;
	else
		dld_buf_size[DLD_BUF_TYPE_GENERAL] = logdump_max_bufsize;

	/* pre-alloc the memory for the log buffers & 'special' buffer */
	dld_buf_special = &g_dld_buf[DLD_BUF_TYPE_SPECIAL];
#if defined(CONFIG_DHD_USE_STATIC_BUF) && defined(DHD_USE_STATIC_MEMDUMP)
	prealloc_buf = DHD_OS_PREALLOC(dhd, prealloc_idx++, LOG_DUMP_TOTAL_BUFSIZE);
	dld_buf_special->buffer = DHD_OS_PREALLOC(dhd, prealloc_idx++,
			dld_buf_size[DLD_BUF_TYPE_SPECIAL]);
#else
	prealloc_buf = MALLOCZ(dhd->osh, LOG_DUMP_TOTAL_BUFSIZE);
	dld_buf_special->buffer = MALLOCZ(dhd->osh, dld_buf_size[DLD_BUF_TYPE_SPECIAL]);
#endif /* CONFIG_DHD_USE_STATIC_BUF && DHD_USE_STATIC_MEMDUMP */

	if (!prealloc_buf) {
		DHD_ERROR(("Failed to allocate memory for log buffers\n"));
		goto fail;
	}
	if (!dld_buf_special->buffer) {
		DHD_ERROR(("Failed to allocate memory for special buffer\n"));
		goto fail;
	}

	bufptr = prealloc_buf;
	for (i = 0; i < DLD_BUFFER_NUM; i++) {
		dld_buf = &g_dld_buf[i];
		dld_buf->dhd_pub = dhd;
		spin_lock_init(&dld_buf->lock);
		dld_buf->wraparound = 0;
		if (i != DLD_BUF_TYPE_SPECIAL) {
			dld_buf->buffer = bufptr;
			dld_buf->max = (unsigned long)dld_buf->buffer + dld_buf_size[i];
			bufptr = (uint8 *)dld_buf->max;
		} else {
			dld_buf->max = (unsigned long)dld_buf->buffer + dld_buf_size[i];
		}
		dld_buf->present = dld_buf->front = dld_buf->buffer;
		dld_buf->remain = dld_buf_size[i];
		dld_buf->enable = 1;
	}

	/* now use the rest of the pre-alloc'd memory for other rings */
#ifdef EWP_ECNTRS_LOGGING
	dhd->ecntr_dbg_ring = dhd_dbg_ring_alloc_init(dhd,
			ECNTR_RING_ID, ECNTR_RING_NAME,
			LOG_DUMP_ECNTRS_MAX_BUFSIZE,
			bufptr, TRUE);
	if (!dhd->ecntr_dbg_ring) {
		DHD_ERROR(("%s: unable to init ecounters dbg ring !\n",
				__FUNCTION__));
		goto fail;
	}
	bufptr += LOG_DUMP_ECNTRS_MAX_BUFSIZE;
#endif /* EWP_ECNTRS_LOGGING */

#ifdef EWP_RTT_LOGGING
	dhd->rtt_dbg_ring = dhd_dbg_ring_alloc_init(dhd,
			RTT_RING_ID, RTT_RING_NAME,
			LOG_DUMP_RTT_MAX_BUFSIZE,
			bufptr, TRUE);
	if (!dhd->rtt_dbg_ring) {
		DHD_ERROR(("%s: unable to init rtt dbg ring !\n",
				__FUNCTION__));
		goto fail;
	}
	bufptr += LOG_DUMP_RTT_MAX_BUFSIZE;
#endif /* EWP_RTT_LOGGING */

#ifdef EWP_BCM_TRACE
	dhd->bcm_trace_dbg_ring = dhd_dbg_ring_alloc_init(dhd,
			BCM_TRACE_RING_ID, BCM_TRACE_RING_NAME,
			LOG_DUMP_BCM_TRACE_MAX_BUFSIZE,
			bufptr, TRUE);
	if (!dhd->bcm_trace_dbg_ring) {
		DHD_ERROR(("%s: unable to init bcm trace dbg ring !\n",
				__FUNCTION__));
		goto fail;
	}
	bufptr += LOG_DUMP_BCM_TRACE_MAX_BUFSIZE;
#endif /* EWP_BCM_TRACE */

	/* Concise buffer is used as intermediate buffer for following purposes
	* a) pull ecounters records temporarily before
	*  writing it to file
	* b) to store dhd dump data before putting it to file
	* It should have a size equal to
	* MAX(largest possible ecntr record, 'dhd dump' data size)
	*/
	dhd->concise_dbg_buf = MALLOC(dhd->osh, CONCISE_DUMP_BUFLEN);
	if (!dhd->concise_dbg_buf) {
		DHD_ERROR(("%s: unable to alloc mem for concise debug info !\n",
				__FUNCTION__));
		goto fail;
	}

#if defined(DHD_EVENT_LOG_FILTER)
	/* XXX init filter last, because filter use buffer which alloced by log dump */
	ret = dhd_event_log_filter_init(dhd,
		bufptr,
		LOG_DUMP_FILTER_MAX_BUFSIZE);
	if (ret != BCME_OK) {
		goto fail;
	}
#endif /* DHD_EVENT_LOG_FILTER */

#if defined(EWP_ECNTRS_LOGGING)
	cookie_buf = MALLOC(dhd->osh, LOG_DUMP_COOKIE_BUFSIZE);
	if (!cookie_buf) {
		DHD_ERROR(("%s: unable to alloc mem for logdump cookie buffer\n",
			__FUNCTION__));
		goto fail;
	}

	ret = dhd_logdump_cookie_init(dhd, cookie_buf, LOG_DUMP_COOKIE_BUFSIZE);
	if (ret != BCME_OK) {
		MFREE(dhd->osh, cookie_buf, LOG_DUMP_COOKIE_BUFSIZE);
		goto fail;
	}
#endif /* EWP_ECNTRS_LOGGING */
	return;

fail:

#if defined(DHD_EVENT_LOG_FILTER)
	/* XXX deinit filter first, because filter use buffer which alloced by log dump */
	if (dhd->event_log_filter) {
		dhd_event_log_filter_deinit(dhd);
	}
#endif /* DHD_EVENT_LOG_FILTER */

	if (dhd->concise_dbg_buf) {
		MFREE(dhd->osh, dhd->concise_dbg_buf, CONCISE_DUMP_BUFLEN);
	}

#ifdef EWP_ECNTRS_LOGGING
	if (dhd->logdump_cookie) {
		dhd_logdump_cookie_deinit(dhd);
		MFREE(dhd->osh, dhd->logdump_cookie, LOG_DUMP_COOKIE_BUFSIZE);
		dhd->logdump_cookie = NULL;
	}
#endif /* EWP_ECNTRS_LOGGING */

#if defined(CONFIG_DHD_USE_STATIC_BUF) && defined(DHD_USE_STATIC_MEMDUMP)
	if (prealloc_buf) {
		DHD_OS_PREFREE(dhd, prealloc_buf, LOG_DUMP_TOTAL_BUFSIZE);
	}
	if (dld_buf_special->buffer) {
		DHD_OS_PREFREE(dhd, dld_buf_special->buffer,
				dld_buf_size[DLD_BUF_TYPE_SPECIAL]);
	}
#else
	if (prealloc_buf) {
		MFREE(dhd->osh, prealloc_buf, LOG_DUMP_TOTAL_BUFSIZE);
	}
	if (dld_buf_special->buffer) {
		MFREE(dhd->osh, dld_buf_special->buffer,
				dld_buf_size[DLD_BUF_TYPE_SPECIAL]);
	}
#endif /* CONFIG_DHD_USE_STATIC_BUF */
	for (i = 0; i < DLD_BUFFER_NUM; i++) {
		dld_buf = &g_dld_buf[i];
		dld_buf->enable = 0;
		dld_buf->buffer = NULL;
	}
	mutex_destroy(&dhd_info->logdump_lock);
}

void
dhd_log_dump_deinit(dhd_pub_t *dhd)
{
	struct dhd_log_dump_buf *dld_buf = NULL, *dld_buf_special = NULL;
	int i = 0;
	dhd_info_t *dhd_info = dhd->info;
	dhd_dbg_ring_t *ring = NULL;

	BCM_REFERENCE(ring);

	if (dhd->concise_dbg_buf) {
		MFREE(dhd->osh, dhd->concise_dbg_buf, CONCISE_DUMP_BUFLEN);
		dhd->concise_dbg_buf = NULL;
	}

#ifdef EWP_ECNTRS_LOGGING
	if (dhd->logdump_cookie) {
		dhd_logdump_cookie_deinit(dhd);
		MFREE(dhd->osh, dhd->logdump_cookie, LOG_DUMP_COOKIE_BUFSIZE);
		dhd->logdump_cookie = NULL;
	}

	if (dhd->ecntr_dbg_ring) {
		dhd_dbg_ring_dealloc_deinit(&dhd->ecntr_dbg_ring, dhd);
	}
#endif /* EWP_ECNTRS_LOGGING */

#ifdef EWP_RTT_LOGGING
	if (dhd->rtt_dbg_ring) {
		dhd_dbg_ring_dealloc_deinit(&dhd->rtt_dbg_ring, dhd);
	}
#endif /* EWP_RTT_LOGGING */

#ifdef EWP_BCM_TRACE
	if (dhd->bcm_trace_dbg_ring) {
		dhd_dbg_ring_dealloc_deinit(&dhd->bcm_trace_dbg_ring, dhd);
	}
#endif /* EWP_BCM_TRACE */

	/* 'general' buffer points to start of the pre-alloc'd memory */
	dld_buf = &g_dld_buf[DLD_BUF_TYPE_GENERAL];
	dld_buf_special = &g_dld_buf[DLD_BUF_TYPE_SPECIAL];
#if defined(CONFIG_DHD_USE_STATIC_BUF) && defined(DHD_USE_STATIC_MEMDUMP)
	if (dld_buf->buffer) {
		DHD_OS_PREFREE(dhd, dld_buf->buffer, LOG_DUMP_TOTAL_BUFSIZE);
	}
	if (dld_buf_special->buffer) {
		DHD_OS_PREFREE(dhd, dld_buf_special->buffer,
				dld_buf_size[DLD_BUF_TYPE_SPECIAL]);
	}
#else
	if (dld_buf->buffer) {
		MFREE(dhd->osh, dld_buf->buffer, LOG_DUMP_TOTAL_BUFSIZE);
	}
	if (dld_buf_special->buffer) {
		MFREE(dhd->osh, dld_buf_special->buffer,
				dld_buf_size[DLD_BUF_TYPE_SPECIAL]);
	}
#endif /* CONFIG_DHD_USE_STATIC_BUF */
	for (i = 0; i < DLD_BUFFER_NUM; i++) {
		dld_buf = &g_dld_buf[i];
		dld_buf->enable = 0;
		dld_buf->buffer = NULL;
	}
	mutex_destroy(&dhd_info->logdump_lock);
}

void
dhd_log_dump_write(int type, char *binary_data,
		int binary_len, const char *fmt, ...)
{
	int len = 0;
	char tmp_buf[DHD_LOG_DUMP_MAX_TEMP_BUFFER_SIZE] = {0, };
	va_list args;
	unsigned long flags = 0;
	struct dhd_log_dump_buf *dld_buf = NULL;
	bool flush_log = FALSE;

	if (type < 0 || type >= DLD_BUFFER_NUM) {
		DHD_INFO(("%s: Unsupported DHD_LOG_DUMP_BUF_TYPE(%d).\n",
			__FUNCTION__, type));
		return;
	}

	dld_buf = &g_dld_buf[type];
	if (dld_buf->enable != 1) {
		return;
	}

	va_start(args, fmt);
	len = vsnprintf(tmp_buf, DHD_LOG_DUMP_MAX_TEMP_BUFFER_SIZE, fmt, args);
	/* Non ANSI C99 compliant returns -1,
	 * ANSI compliant return len >= DHD_LOG_DUMP_MAX_TEMP_BUFFER_SIZE
	 */
	va_end(args);
	if (len < 0) {
		return;
	}

	if (len >= DHD_LOG_DUMP_MAX_TEMP_BUFFER_SIZE) {
		len = DHD_LOG_DUMP_MAX_TEMP_BUFFER_SIZE - 1;
		tmp_buf[len] = '\0';
	}

	/* make a critical section to eliminate race conditions */
	DHD_LOG_DUMP_BUF_LOCK(&dld_buf->lock, flags);
	if (dld_buf->remain < len) {
		dld_buf->wraparound = 1;
		dld_buf->present = dld_buf->front;
		dld_buf->remain = dld_buf_size[type];
		/* if wrap around happens, flush the ring buffer to the file */
		flush_log = TRUE;
	}

	memcpy(dld_buf->present, tmp_buf, len);
	dld_buf->remain -= len;
	dld_buf->present += len;
	DHD_LOG_DUMP_BUF_UNLOCK(&dld_buf->lock, flags);

	/* double check invalid memory operation */
	ASSERT((unsigned long)dld_buf->present <= dld_buf->max);

	if (dld_buf->dhd_pub) {
		dhd_pub_t *dhdp = (dhd_pub_t *)dld_buf->dhd_pub;
		dhdp->logdump_periodic_flush =
			logdump_periodic_flush;
		if (logdump_periodic_flush && flush_log) {
			log_dump_type_t *flush_type = MALLOCZ(dhdp->osh,
					sizeof(log_dump_type_t));
			if (flush_type) {
				*flush_type = type;
				dhd_schedule_log_dump(dld_buf->dhd_pub, flush_type);
			}
		}
	}
}

void
dhd_log_dump_vendor_trigger(dhd_pub_t *dhd_pub)
{
	unsigned long flags = 0;
	DHD_GENERAL_LOCK(dhd_pub, flags);
	DHD_BUS_BUSY_SET_IN_DUMP_DONGLE_MEM(dhd_pub);
	DHD_GENERAL_UNLOCK(dhd_pub, flags);

	dhd_log_dump_trigger(dhd_pub, CMD_DEFAULT);

	DHD_GENERAL_LOCK(dhd_pub, flags);
	DHD_BUS_BUSY_CLEAR_IN_DUMP_DONGLE_MEM(dhd_pub);
	dhd_os_busbusy_wake(dhd_pub);
	DHD_GENERAL_UNLOCK(dhd_pub, flags);

	return;
}

void
dhd_log_dump_trigger(dhd_pub_t *dhdp, int subcmd)
{
#if defined(DHD_DUMP_FILE_WRITE_FROM_KERNEL)
	log_dump_type_t *flush_type;
#endif /* DHD_DUMP_FILE_WRITE_FROM_KERNEL */
	uint64 current_time_sec;

	if (!dhdp) {
		DHD_ERROR(("dhdp is NULL !\n"));
		return;
	}

	if (subcmd >= CMD_MAX || subcmd < CMD_DEFAULT) {
		DHD_ERROR(("%s : Invalid subcmd \n", __FUNCTION__));
		return;
	}

	current_time_sec = DIV_U64_BY_U32(OSL_LOCALTIME_NS(), NSEC_PER_SEC);

	DHD_ERROR(("%s: current_time_sec=%lld debug_dump_time_sec=%lld interval=%d\n",
		__FUNCTION__, current_time_sec, dhdp->debug_dump_time_sec,
		DEBUG_DUMP_TRIGGER_INTERVAL_SEC));

	if ((current_time_sec - dhdp->debug_dump_time_sec) < DEBUG_DUMP_TRIGGER_INTERVAL_SEC) {
		DHD_ERROR(("%s : Last debug dump triggered(%lld) within %d seconds, so SKIP\n",
			__FUNCTION__, dhdp->debug_dump_time_sec, DEBUG_DUMP_TRIGGER_INTERVAL_SEC));
		return;
	}

	clear_debug_dump_time(dhdp->debug_dump_time_str);
	/*  */

	dhdp->debug_dump_subcmd = subcmd;

	dhdp->debug_dump_time_sec = DIV_U64_BY_U32(OSL_LOCALTIME_NS(), NSEC_PER_SEC);

#if defined(DHD_DUMP_FILE_WRITE_FROM_KERNEL)
	/* flush_type is freed at do_dhd_log_dump function */
	flush_type = MALLOCZ(dhdp->osh, sizeof(log_dump_type_t));
	if (flush_type) {
		*flush_type = DLD_BUF_TYPE_ALL;
		dhd_schedule_log_dump(dhdp, flush_type);
	} else {
		DHD_ERROR(("%s Fail to malloc flush_type\n", __FUNCTION__));
		return;
	}
#endif /* DHD_DUMP_FILE_WRITE_FROM_KERNEL */

#if defined(DHD_SDTC_ETB_DUMP) && defined(BCMQT_HW)
	/* For QT/zebu memdump collection is disabled,
	 * but set collect_sdtc true here, so that
	 * developers can issue 'log_dump' dhd iovar
	 * to get etb/sdtc dumps
	 */
	DHD_ERROR(("%s : QT/FPGA set collect_sdtc as TRUE\n", __FUNCTION__));
	dhdp->collect_sdtc = TRUE;
#endif /* DHD_SDTC_ETB_DUMP && BCMQT_HW */

	/* Inside dhd_mem_dump, event notification will be sent to HAL and
	 * from other context DHD pushes memdump, debug_dump and pktlog dump
	 * to HAL and HAL will write into file
	 */
#if (defined(BCMPCIE) || defined(BCMSDIO)) && defined(DHD_FW_COREDUMP)
	dhdp->memdump_type = DUMP_TYPE_BY_SYSDUMP;
	dhd_bus_mem_dump(dhdp);
#endif /* BCMPCIE && DHD_FW_COREDUMP */

#if defined(DHD_PKT_LOGGING) && defined(DHD_DUMP_FILE_WRITE_FROM_KERNEL)
	dhd_schedule_pktlog_dump(dhdp);
#endif /* DHD_PKT_LOGGING && DHD_DUMP_FILE_WRITE_FROM_KERNEL */
}

#ifdef DHD_DEBUGABILITY_DEBUG_DUMP
int dhd_debug_dump_get_ring_num(int sec_type)
{
	int idx = 0;
	for (idx = 0; idx < ARRAYSIZE(dhd_debug_dump_ring_map); idx++) {
		if (dhd_debug_dump_ring_map[idx].type == sec_type) {
			idx = dhd_debug_dump_ring_map[idx].debug_dump_ring;
			return idx;
		}
	}
	return idx;
}
#endif /* DHD_DEBUGABILITY_DEBUG_DUMP */

int
dhd_dump_debug_ring(dhd_pub_t *dhdp, void *ring_ptr, const void *user_buf,
		log_dump_section_hdr_t *sec_hdr,
		char *text_hdr, int buflen, uint32 sec_type)
{
	uint32 rlen = 0;
	uint32 data_len = 0;
	void *data = NULL;
	unsigned long flags = 0;
	int ret = 0;
	dhd_dbg_ring_t *ring = (dhd_dbg_ring_t *)ring_ptr;
#ifndef DHD_DEBUGABILITY_DEBUG_DUMP
	int pos = 0;
#endif /* !DHD_DEBUGABILITY_DEBUG_DUMP */
	int fpos_sechdr = 0;
	int tot_len = 0;
	char *tmp_buf = NULL;
	int idx;
	int ring_num = 0;

	BCM_REFERENCE(idx);
	BCM_REFERENCE(tot_len);
	BCM_REFERENCE(fpos_sechdr);
	BCM_REFERENCE(data_len);
	BCM_REFERENCE(tmp_buf);
	BCM_REFERENCE(ring_num);

#ifdef DHD_DEBUGABILITY_DEBUG_DUMP
	if (!dhdp || !ring || !sec_hdr || !text_hdr) {
		return BCME_BADARG;
	}

	tmp_buf = (char *)VMALLOCZ(dhdp->osh, ring->ring_size);
	if (!tmp_buf) {
		DHD_ERROR(("%s: VMALLOC Fail id:%d size:%u\n",
			__func__, ring->id, ring->ring_size));
		return BCME_NOMEM;
	}
#else
	if (!dhdp || !ring || !user_buf || !sec_hdr || !text_hdr) {
		return BCME_BADARG;
	}
#endif /* DHD_DEBUGABILITY_DEBUG_DUMP */
	/* do not allow further writes to the ring
	 * till we flush it
	 */
	DHD_DBG_RING_LOCK(ring->lock, flags);
	ring->state = RING_SUSPEND;
	DHD_DBG_RING_UNLOCK(ring->lock, flags);

#ifdef DHD_DEBUGABILITY_DEBUG_DUMP
	ring_num = dhd_debug_dump_get_ring_num(sec_type);
	dhd_export_debug_data(text_hdr, NULL, NULL, strlen(text_hdr), &ring_num);

	data = tmp_buf;
	do {
		rlen = dhd_dbg_ring_pull_single(ring, data, ring->ring_size, TRUE);
		if (rlen > 0) {
			tot_len += rlen;
			data += rlen;
		}
	} while ((rlen > 0));

	sec_hdr->type = sec_type;
	sec_hdr->length = tot_len;
	DHD_ERROR(("%s: DUMP id:%d type:%u tot_len:%d\n", __func__, ring->id, sec_type, tot_len));

	dhd_export_debug_data((char *)sec_hdr, NULL, NULL, sizeof(*sec_hdr), &ring_num);
	dhd_export_debug_data(tmp_buf, NULL, NULL, tot_len, &ring_num);

	VMFREE(dhdp->osh, tmp_buf, ring->ring_size);
#else
	if (dhdp->concise_dbg_buf) {
		/* re-use concise debug buffer temporarily
		 * to pull ring data, to write
		 * record by record to file
		 */
		data_len = CONCISE_DUMP_BUFLEN;
		data = dhdp->concise_dbg_buf;
		ret = dhd_export_debug_data(text_hdr, NULL, user_buf, strlen(text_hdr), &pos);
		/* write the section header now with zero length,
		 * once the correct length is found out, update
		 * it later
		 */
		fpos_sechdr = pos;
		sec_hdr->type = sec_type;
		sec_hdr->length = 0;
		ret = dhd_export_debug_data((char *)sec_hdr, NULL, user_buf,
			sizeof(*sec_hdr), &pos);
		do {
			rlen = dhd_dbg_ring_pull_single(ring, data, data_len, TRUE);
			if (rlen > 0) {
				/* write the log */
				ret = dhd_export_debug_data(data, NULL, user_buf, rlen, &pos);
			}
			DHD_DBGIF(("%s: rlen : %d\n", __FUNCTION__, rlen));
		} while ((rlen > 0));
		/* now update the section header length in the file */
		/* Complete ring size is dumped by HAL, hence updating length to ring size */
		sec_hdr->length = ring->ring_size;
		ret = dhd_export_debug_data((char *)sec_hdr, NULL, user_buf,
			sizeof(*sec_hdr), &fpos_sechdr);
	} else {
		DHD_ERROR(("%s: No concise buffer available !\n", __FUNCTION__));
	}
#endif /* DHD_DEBUGABILITY_DEBUG_DUMP */

	DHD_DBG_RING_LOCK(ring->lock, flags);
	ring->state = RING_ACTIVE;
	/* Resetting both read and write pointer,
	 * since all items are read.
	 */
	ring->rp = ring->wp = 0;
	DHD_DBG_RING_UNLOCK(ring->lock, flags);

	return ret;
}

int
dhd_log_dump_ring_to_file(dhd_pub_t *dhdp, void *ring_ptr, void *file,
		unsigned long *file_posn, log_dump_section_hdr_t *sec_hdr,
		char *text_hdr, uint32 sec_type)
{
	uint32 rlen = 0;
	uint32 data_len = 0, total_len = 0;
	void *data = NULL;
	unsigned long fpos_sechdr = 0;
	unsigned long flags = 0;
	int ret = 0;
	dhd_dbg_ring_t *ring = (dhd_dbg_ring_t *)ring_ptr;

	if (!dhdp || !ring || !file || !sec_hdr ||
		!file_posn || !text_hdr)
		return BCME_BADARG;

	/* do not allow further writes to the ring
	 * till we flush it
	 */
	DHD_DBG_RING_LOCK(ring->lock, flags);
	ring->state = RING_SUSPEND;
	DHD_DBG_RING_UNLOCK(ring->lock, flags);

	if (dhdp->concise_dbg_buf) {
		/* re-use concise debug buffer temporarily
		 * to pull ring data, to write
		 * record by record to file
		 */
		data_len = CONCISE_DUMP_BUFLEN;
		data = dhdp->concise_dbg_buf;
		dhd_os_write_file_posn(file, file_posn, text_hdr,
				strlen(text_hdr));
		/* write the section header now with zero length,
		 * once the correct length is found out, update
		 * it later
		 */
		dhd_init_sec_hdr(sec_hdr);
		fpos_sechdr = *file_posn;
		sec_hdr->type = sec_type;
		sec_hdr->length = 0;
		dhd_os_write_file_posn(file, file_posn, (char *)sec_hdr,
				sizeof(*sec_hdr));
		do {
			rlen = dhd_dbg_ring_pull_single(ring, data, data_len, TRUE);
			if (rlen > 0) {
				/* write the log */
				ret = dhd_os_write_file_posn(file, file_posn, data, rlen);
				if (ret < 0) {
					DHD_ERROR(("%s: write file error !\n", __FUNCTION__));
					DHD_DBG_RING_LOCK(ring->lock, flags);
					ring->state = RING_ACTIVE;
					DHD_DBG_RING_UNLOCK(ring->lock, flags);
					return BCME_ERROR;
				}
			}
			total_len += rlen;
		} while (rlen > 0);
		/* now update the section header length in the file */
		sec_hdr->length = total_len;
		dhd_os_write_file_posn(file, &fpos_sechdr, (char *)sec_hdr, sizeof(*sec_hdr));
	} else {
		DHD_ERROR(("%s: No concise buffer available !\n", __FUNCTION__));
	}

	DHD_DBG_RING_LOCK(ring->lock, flags);
	ring->state = RING_ACTIVE;
	/* Resetting both read and write pointer,
	 * since all items are read.
	 */
	ring->rp = ring->wp = 0;
	DHD_DBG_RING_UNLOCK(ring->lock, flags);
	return BCME_OK;
}

/* logdump cookie */
#define MAX_LOGUDMP_COOKIE_CNT	10u
#define LOGDUMP_COOKIE_STR_LEN	50u
int
dhd_logdump_cookie_init(dhd_pub_t *dhdp, uint8 *buf, uint32 buf_size)
{
	uint32 ring_size;

	if (!dhdp || !buf) {
		DHD_ERROR(("INVALID PTR: dhdp:%p buf:%p\n", dhdp, buf));
		return BCME_ERROR;
	}

	ring_size = dhd_ring_get_hdr_size() + LOGDUMP_COOKIE_STR_LEN * MAX_LOGUDMP_COOKIE_CNT;
	if (buf_size < ring_size) {
		DHD_ERROR(("BUF SIZE IS TO SHORT: req:%d buf_size:%d\n",
			ring_size, buf_size));
		return BCME_ERROR;
	}

	dhdp->logdump_cookie = dhd_ring_init(dhdp, buf, buf_size,
		LOGDUMP_COOKIE_STR_LEN, MAX_LOGUDMP_COOKIE_CNT,
		DHD_RING_TYPE_FIXED);
	if (!dhdp->logdump_cookie) {
		DHD_ERROR(("FAIL TO INIT COOKIE RING\n"));
		return BCME_ERROR;
	}

	return BCME_OK;
}

void
dhd_logdump_cookie_deinit(dhd_pub_t *dhdp)
{
	if (!dhdp) {
		return;
	}
	if (dhdp->logdump_cookie) {
		dhd_ring_deinit(dhdp, dhdp->logdump_cookie);
	}

	return;
}

void
dhd_logdump_cookie_save(dhd_pub_t *dhdp, char *cookie, char *type)
{
	char *ptr;

	if (!dhdp || !cookie || !type || !dhdp->logdump_cookie) {
		DHD_ERROR(("%s: At least one buffer ptr is NULL dhdp=%p cookie=%p"
			" type = %p, cookie_cfg:%p\n", __FUNCTION__,
			dhdp, cookie, type, dhdp?dhdp->logdump_cookie: NULL));
		return;
	}
	ptr = (char *)dhd_ring_get_empty(dhdp->logdump_cookie);
	if (ptr == NULL) {
		DHD_ERROR(("%s : Skip to save due to locking\n", __FUNCTION__));
		return;
	}
	scnprintf(ptr, LOGDUMP_COOKIE_STR_LEN, "%s: %s\n", type, cookie);
	return;
}

int
dhd_logdump_cookie_get(dhd_pub_t *dhdp, char *ret_cookie, uint32 buf_size)
{
	char *ptr;

	if (!dhdp || !ret_cookie || !dhdp->logdump_cookie) {
		DHD_ERROR(("%s: At least one buffer ptr is NULL dhdp=%p"
			"cookie=%p cookie_cfg:%p\n", __FUNCTION__,
			dhdp, ret_cookie, dhdp?dhdp->logdump_cookie: NULL));
		return BCME_ERROR;
	}
	ptr = (char *)dhd_ring_get_first(dhdp->logdump_cookie);
	if (ptr == NULL) {
		DHD_ERROR(("%s : Skip to save due to locking\n", __FUNCTION__));
		return BCME_ERROR;
	}
	memcpy(ret_cookie, ptr, MIN(buf_size, strlen(ptr)));
	dhd_ring_free_first(dhdp->logdump_cookie);
	return BCME_OK;
}

int
dhd_logdump_cookie_count(dhd_pub_t *dhdp)
{
	if (!dhdp || !dhdp->logdump_cookie) {
		DHD_ERROR(("%s: At least one buffer ptr is NULL dhdp=%p cookie=%p\n",
			__FUNCTION__, dhdp, dhdp?dhdp->logdump_cookie: NULL));
		return 0;
	}
	return dhd_ring_get_cur_size(dhdp->logdump_cookie);
}

static inline int
__dhd_log_dump_cookie_to_file(
	dhd_pub_t *dhdp, void *fp, const void *user_buf, unsigned long *f_pos,
	char *buf, uint32 buf_size)
{

	uint32 remain = buf_size;
	int ret = BCME_ERROR;
	char tmp_buf[LOGDUMP_COOKIE_STR_LEN];
	log_dump_section_hdr_t sec_hdr;
	uint32 read_idx;
	uint32 write_idx;

	read_idx = dhd_ring_get_read_idx(dhdp->logdump_cookie);
	write_idx = dhd_ring_get_write_idx(dhdp->logdump_cookie);
	while (dhd_logdump_cookie_count(dhdp) > 0) {
		memset(tmp_buf, 0, sizeof(tmp_buf));
		ret = dhd_logdump_cookie_get(dhdp, tmp_buf, LOGDUMP_COOKIE_STR_LEN);
		if (ret != BCME_OK) {
			return ret;
		}
		remain -= scnprintf(&buf[buf_size - remain], remain, "%s", tmp_buf);
	}
	dhd_ring_set_read_idx(dhdp->logdump_cookie, read_idx);
	dhd_ring_set_write_idx(dhdp->logdump_cookie, write_idx);

	ret = dhd_export_debug_data(COOKIE_LOG_HDR, fp, user_buf, strlen(COOKIE_LOG_HDR), f_pos);
	if (ret < 0) {
		DHD_ERROR(("%s : Write file Error for cookie hdr\n", __FUNCTION__));
		return ret;
	}
	sec_hdr.magic = LOG_DUMP_MAGIC;
	sec_hdr.timestamp = local_clock();
	sec_hdr.type = LOG_DUMP_SECTION_COOKIE;
	sec_hdr.length = buf_size - remain;

	ret = dhd_export_debug_data((char *)&sec_hdr, fp, user_buf, sizeof(sec_hdr), f_pos);
	if (ret < 0) {
		DHD_ERROR(("%s : Write file Error for section hdr\n", __FUNCTION__));
		return ret;
	}

	ret = dhd_export_debug_data(buf, fp, user_buf, sec_hdr.length, f_pos);
	if (ret < 0) {
		DHD_ERROR(("%s : Write file Error for cookie data\n", __FUNCTION__));
	}

	return ret;
}

uint32
dhd_log_dump_cookie_len(dhd_pub_t *dhdp)
{
	int len = 0;
	char tmp_buf[LOGDUMP_COOKIE_STR_LEN];
	log_dump_section_hdr_t sec_hdr;
	char *buf = NULL;
	int ret = BCME_ERROR;
	uint32 buf_size = MAX_LOGUDMP_COOKIE_CNT * LOGDUMP_COOKIE_STR_LEN;
	uint32 read_idx;
	uint32 write_idx;
	uint32 remain;

	remain = buf_size;

	if (!dhdp || !dhdp->logdump_cookie) {
		DHD_ERROR(("%s At least one ptr is NULL "
			"dhdp = %p cookie %p\n",
			__FUNCTION__, dhdp, dhdp?dhdp->logdump_cookie:NULL));
		goto exit;
	}

	buf = (char *)MALLOCZ(dhdp->osh, buf_size);
	if (!buf) {
		DHD_ERROR(("%s Fail to malloc buffer\n", __FUNCTION__));
		goto exit;
	}

	read_idx = dhd_ring_get_read_idx(dhdp->logdump_cookie);
	write_idx = dhd_ring_get_write_idx(dhdp->logdump_cookie);
	while (dhd_logdump_cookie_count(dhdp) > 0) {
		memset(tmp_buf, 0, sizeof(tmp_buf));
		ret = dhd_logdump_cookie_get(dhdp, tmp_buf, LOGDUMP_COOKIE_STR_LEN);
		if (ret != BCME_OK) {
			goto exit;
		}
		remain -= (uint32)strlen(tmp_buf);
	}
	dhd_ring_set_read_idx(dhdp->logdump_cookie, read_idx);
	dhd_ring_set_write_idx(dhdp->logdump_cookie, write_idx);
	len += strlen(COOKIE_LOG_HDR);
	len += sizeof(sec_hdr);
	len += (buf_size - remain);
exit:
	if (buf)
		MFREE(dhdp->osh, buf, buf_size);
	return len;
}

int
dhd_log_dump_cookie(dhd_pub_t *dhdp, const void *user_buf)
{
	int ret = BCME_ERROR;
	char tmp_buf[LOGDUMP_COOKIE_STR_LEN];
	log_dump_section_hdr_t sec_hdr;
	char *buf = NULL;
	uint32 buf_size = MAX_LOGUDMP_COOKIE_CNT * LOGDUMP_COOKIE_STR_LEN;
	int pos = 0;
	uint32 read_idx;
	uint32 write_idx;
	uint32 remain;

	remain = buf_size;

	if (!dhdp || !dhdp->logdump_cookie) {
		DHD_ERROR(("%s At least one ptr is NULL "
			"dhdp = %p cookie %p\n",
			__FUNCTION__, dhdp, dhdp?dhdp->logdump_cookie:NULL));
		goto exit;
	}

	buf = (char *)MALLOCZ(dhdp->osh, buf_size);
	if (!buf) {
		DHD_ERROR(("%s Fail to malloc buffer\n", __FUNCTION__));
		goto exit;
	}

	read_idx = dhd_ring_get_read_idx(dhdp->logdump_cookie);
	write_idx = dhd_ring_get_write_idx(dhdp->logdump_cookie);
	while (dhd_logdump_cookie_count(dhdp) > 0) {
		memset(tmp_buf, 0, sizeof(tmp_buf));
		ret = dhd_logdump_cookie_get(dhdp, tmp_buf, LOGDUMP_COOKIE_STR_LEN);
		if (ret != BCME_OK) {
			goto exit;
		}
		remain -= scnprintf(&buf[buf_size - remain], remain, "%s", tmp_buf);
	}
	dhd_ring_set_read_idx(dhdp->logdump_cookie, read_idx);
	dhd_ring_set_write_idx(dhdp->logdump_cookie, write_idx);
	ret = dhd_export_debug_data(COOKIE_LOG_HDR, NULL, user_buf, strlen(COOKIE_LOG_HDR), &pos);
	sec_hdr.magic = LOG_DUMP_MAGIC;
	sec_hdr.timestamp = local_clock();
	sec_hdr.type = LOG_DUMP_SECTION_COOKIE;
	sec_hdr.length = buf_size - remain;
	ret = dhd_export_debug_data((char *)&sec_hdr, NULL, user_buf, sizeof(sec_hdr), &pos);
	ret = dhd_export_debug_data(buf, NULL, user_buf, sec_hdr.length, &pos);
exit:
	if (buf)
		MFREE(dhdp->osh, buf, buf_size);
	return ret;
}

int
dhd_log_dump_cookie_to_file(dhd_pub_t *dhdp, void *fp, const void *user_buf, unsigned long *f_pos)
{
	char *buf;
	int ret = BCME_ERROR;
	uint32 buf_size = MAX_LOGUDMP_COOKIE_CNT * LOGDUMP_COOKIE_STR_LEN;

#ifdef DHD_DEBUGABILITY_DEBUG_DUMP
	if (!dhdp || !dhdp->logdump_cookie)
#else
	if (!dhdp || !dhdp->logdump_cookie || (!fp && !user_buf) || !f_pos)
#endif /* DHD_DEBUGABILITY_DEBUG_DUMP */
	{
		DHD_ERROR(("%s At least one ptr is NULL "
			"dhdp = %p cookie %p fp = %p f_pos = %p\n",
			__FUNCTION__, dhdp, dhdp?dhdp->logdump_cookie:NULL, fp, f_pos));
		return ret;
	}

	buf = (char *)MALLOCZ(dhdp->osh, buf_size);
	if (!buf) {
		DHD_ERROR(("%s Fail to malloc buffer\n", __FUNCTION__));
		return ret;
	}
	ret = __dhd_log_dump_cookie_to_file(dhdp, fp, user_buf, f_pos, buf, buf_size);
	MFREE(dhdp->osh, buf, buf_size);

	return ret;
}

void
get_debug_dump_time(char *str)
{
	struct osl_timespec curtime;
	unsigned long long local_time;
	struct rtc_time tm;

	if (!strlen(str)) {
		osl_do_gettimeofday(&curtime);
		local_time = (u64)(curtime.tv_sec -
				(sys_tz.tz_minuteswest * DHD_LOG_DUMP_TS_MULTIPLIER_VALUE));
		rtc_time_to_tm(local_time, &tm);
		snprintf(str, DEBUG_DUMP_TIME_BUF_LEN, DHD_LOG_DUMP_TS_FMT_YYMMDDHHMMSSMSMS,
				tm.tm_year - 100, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
				tm.tm_sec, curtime.tv_nsec/NSEC_PER_MSEC);
	}
}

void
clear_debug_dump_time(char *str)
{
	memset(str, 0, DEBUG_DUMP_TIME_BUF_LEN);
}
#if defined(WL_CFGVENDOR_SEND_HANG_EVENT) || defined(DHD_PKT_LOGGING)
void
copy_debug_dump_time(char *dest, char *src)
{
	memcpy(dest, src, DEBUG_DUMP_TIME_BUF_LEN);
}
#endif

#ifdef DHD_IOVAR_LOG_FILTER_DUMP
typedef struct iovar_log_filter_table {
	char command[64];
	int  enable;
} iovar_log_filter_table_t;

/* WLC_GET_VAR is not being logged by default. Add for logging in debug_dump. */
static const iovar_log_filter_table_t iovar_get_filter_params[] = {
	{"cur_etheraddr", TRUE},
	{"\0", TRUE}
};

/* WLC_SET_VAR is being logged by default. Add for not logging in debug_dump. */
static const iovar_log_filter_table_t iovar_set_filter_params[] = {
	{"pkt_filter_enable", FALSE},
	{"pkt_filter_mode", FALSE},
	{"\0", FALSE}
};

bool
dhd_iovar_log_dump_check(dhd_pub_t *dhd_pub, uint32 cmd, char *msg)
{
	int cnt = 0;
	const iovar_log_filter_table_t *table;
	bool ret_val = TRUE;

	/* Logging all IOVARs with DHD_IOVAR_MEM() in debug_dump file
	 * during during Wifi ON.
	 */
	if (dhd_pub->up == FALSE) {
		return TRUE;
	}

	if (cmd == WLC_GET_VAR) {
		ret_val = FALSE;
		table = iovar_get_filter_params;
	} else if (cmd == WLC_SET_VAR) {
		ret_val = TRUE;
		table = iovar_set_filter_params;
	} else {
		return TRUE;
	}
	while (strlen(table[cnt].command) > 0) {
		if (!strncmp(msg, table[cnt].command,
				strlen(table[cnt].command))) {
			return table[cnt].enable;
		}

		cnt++;
	}

	return ret_val;
}
#endif /* DHD_IOVAR_LOG_FILTER_DUMP */
#endif /* DHD_LOG_DUMP */
