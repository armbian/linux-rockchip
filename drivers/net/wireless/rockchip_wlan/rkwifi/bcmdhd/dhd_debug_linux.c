/*
 * DHD debugability Linux os layer
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

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_dbg.h>
#include <dhd_debug.h>

#include <net/cfg80211.h>
#include <wl_cfgvendor.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
#include <linux/sched/clock.h>
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0) */
#include <dhd_config.h>

typedef void (*dbg_ring_send_sub_t)(void *ctx, const int ring_id, const void *data,
	const uint32 len, const dhd_dbg_ring_status_t ring_status);
typedef void (*dbg_urgent_noti_sub_t)(void *ctx, const void *data,
	const uint32 len, const uint32 fw_len);

static dbg_ring_send_sub_t ring_send_sub_cb[DEBUG_RING_ID_MAX];
static dbg_urgent_noti_sub_t urgent_noti_sub_cb;
typedef struct dhd_dbg_os_ring_info {
	dhd_pub_t *dhdp;
	int ring_id;
	int log_level;
	unsigned long interval;
	struct delayed_work work;
	uint64 tsoffset;
} linux_dbgring_info_t;

struct log_level_table dhd_event_map[] = {
	{1, WIFI_EVENT_DRIVER_EAPOL_FRAME_TRANSMIT_REQUESTED, "DRIVER EAPOL TX REQ"},
	{1, WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED, "DRIVER EAPOL RX"},
	{2, WIFI_EVENT_DRIVER_SCAN_REQUESTED, "SCAN_REQUESTED"},
	{2, WIFI_EVENT_DRIVER_SCAN_COMPLETE, "SCAN COMPELETE"},
	{3, WIFI_EVENT_DRIVER_SCAN_RESULT_FOUND, "SCAN RESULT FOUND"},
	{2, WIFI_EVENT_DRIVER_PNO_ADD, "PNO ADD"},
	{2, WIFI_EVENT_DRIVER_PNO_REMOVE, "PNO REMOVE"},
	{2, WIFI_EVENT_DRIVER_PNO_NETWORK_FOUND, "PNO NETWORK FOUND"},
	{2, WIFI_EVENT_DRIVER_PNO_SCAN_REQUESTED, "PNO SCAN_REQUESTED"},
	{1, WIFI_EVENT_DRIVER_PNO_SCAN_RESULT_FOUND, "PNO SCAN RESULT FOUND"},
	{1, WIFI_EVENT_DRIVER_PNO_SCAN_COMPLETE, "PNO SCAN COMPELETE"}
};

static void
debug_data_send(dhd_pub_t *dhdp, int ring_id, const void *data, const uint32 len,
	const dhd_dbg_ring_status_t ring_status)
{
	struct net_device *ndev;
	dbg_ring_send_sub_t ring_sub_send;
	ndev = dhd_linux_get_primary_netdev(dhdp);

	if (!ndev)
		return;
	if (!VALID_RING(ring_id))
		return;
	if (ring_send_sub_cb[ring_id]) {
		ring_sub_send = ring_send_sub_cb[ring_id];
		ring_sub_send(ndev, ring_id, data, len, ring_status);
	}
}

static void
dhd_os_dbg_urgent_notifier(dhd_pub_t *dhdp, const void *data, const uint32 len)
{
	struct net_device *ndev;
	ndev = dhd_linux_get_primary_netdev(dhdp);
	if (!ndev)
		return;
	if (urgent_noti_sub_cb) {
		urgent_noti_sub_cb(ndev, data, len, dhdp->soc_ram_length);
	}
}

static void
dbg_ring_poll_worker(struct work_struct *work)
{
	struct delayed_work *d_work = to_delayed_work(work);
	bool sched = TRUE;
	dhd_dbg_ring_t *ring;
	linux_dbgring_info_t *ring_info;
	dhd_pub_t *dhdp;
	int ringid;
	dhd_dbg_ring_status_t ring_status;
	void *buf, *buf_entries;
	dhd_dbg_ring_entry_pack_t *pack_hdr;
	dhd_dbg_ring_entry_t *hdr;
	int32 buflen, rlen, remain_buflen;
	int32 alloc_len;
	unsigned long flags;

	BCM_REFERENCE(hdr);

	if (!CAN_SLEEP()) {
		DHD_CONS_ONLY(("this context should be sleepable\n"));
		sched = FALSE;
		goto exit;
	}

	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	ring_info = container_of(d_work, linux_dbgring_info_t, work);
	GCC_DIAGNOSTIC_POP();

	dhdp = ring_info->dhdp;
	ringid = ring_info->ring_id;

	ring = &dhdp->dbg->dbg_rings[ringid];
	DHD_DBG_RING_LOCK(ring->lock, flags);
	dhd_dbg_get_ring_status(dhdp, ringid, &ring_status);
	DHD_DBG_RING_UNLOCK(ring->lock, flags);

#ifdef DHD_PKT_LOGGING_DBGRING
	if (ringid == PACKET_LOG_RING_ID) {
		struct net_device *ndev;
		ndev = dhd_linux_get_primary_netdev(dhdp);
		buflen = DBG_RING_ENTRY_SIZE;
		buflen += dhd_os_get_pktlog_dump_size(ndev);
		DHD_DBGIF(("%s: buflen: %d\n", __FUNCTION__, buflen));
		alloc_len = buflen + DBG_RING_ENTRY_PACK_SIZE;
	} else
#endif /* DHD_PKT_LOGGING_DBGRING */
	{
		DHD_DBG_RING_LOCK(ring->lock, flags);
		if (ring->wp > ring->rp) {
			buflen = ring->wp - ring->rp;
		} else if (ring->wp < ring->rp) {
			buflen = ring->ring_size - ring->rp + ring->wp;
		} else {
			DHD_DBG_RING_UNLOCK(ring->lock, flags);
			goto exit;
		}

		if (buflen > ring->ring_size) {
			DHD_DBG_RING_UNLOCK(ring->lock, flags);
			goto exit;
		}
		DHD_DBG_RING_UNLOCK(ring->lock, flags);

		alloc_len = NLMSG_DEFAULT_SIZE;
	}
	buf = VMALLOCZ(dhdp->osh, alloc_len);

	if (!buf) {
		DHD_CONS_ONLY(("%s failed to allocate read buf\n", __FUNCTION__));
		sched = FALSE;
		goto exit;
	}

	pack_hdr = (dhd_dbg_ring_entry_pack_t *)buf;
	pack_hdr->magic = DBG_RING_PACK_MAGIC;

	buf_entries = (char *)buf + sizeof(dhd_dbg_ring_entry_pack_t);

#ifdef DHD_PKT_LOGGING_DBGRING
	if (ringid == PACKET_LOG_RING_ID) {
		pack_hdr->num_entries = 1u;
		rlen = dhd_dbg_pull_from_pktlog(dhdp, ringid, buf_entries, buflen);
		DHD_DBGIF(("%s: rlen: %d\n", __FUNCTION__, rlen));
		hdr = (dhd_dbg_ring_entry_t *)buf_entries;
		while (rlen > 0) {
			DHD_DBG_RING_LOCK(ring->lock, flags);
			ring_status.read_bytes += (rlen - DBG_RING_ENTRY_SIZE);
			ring->stat.read_bytes += (rlen - DBG_RING_ENTRY_SIZE);
			if (ring->stat.read_bytes > ring->stat.written_bytes) {
				DHD_DBGIF(("%s READ/WRITE counter mismatched!\n", __FUNCTION__));
				ring->stat.read_bytes = ring->stat.written_bytes;
			}
			DHD_DBGIF(("%s RING%d[%s]read_bytes %d, written_bytes %d, "
				"writen_records %d\n", __FUNCTION__, ring->id, ring->name,
				ring->stat.read_bytes, ring->stat.written_bytes,
				ring->stat.written_records));
			DHD_DBG_RING_UNLOCK(ring->lock, flags);
			/* offset fw ts to host ts */
			hdr->timestamp += ring_info->tsoffset;
			debug_data_send(dhdp, ringid, pack_hdr,
					ENTRY_LENGTH(hdr) + DBG_RING_ENTRY_PACK_SIZE, ring_status);
			rlen -= ENTRY_LENGTH(hdr);
			hdr = (dhd_dbg_ring_entry_t *)((char *)hdr + ENTRY_LENGTH(hdr));
		}
	} else
#endif /* DHD_PKT_LOGGING_DBGRING */
	{
		remain_buflen = buflen;
		while (remain_buflen > 0) {
			pack_hdr->num_entries = 0;
			memset_s(buf_entries, NLMSG_DEFAULT_SIZE - DBG_RING_ENTRY_PACK_SIZE,
				0, NLMSG_DEFAULT_SIZE - DBG_RING_ENTRY_PACK_SIZE);
			/* Returns as much as possible with the size of the passed buffer
			 * rlen means the total length of multiple entries including entry hdr
			 */
			rlen = dhd_dbg_pull_from_ring(dhdp, ringid, buf_entries,
				NLMSG_DEFAULT_SIZE - DBG_RING_ENTRY_PACK_SIZE,
				&pack_hdr->num_entries);
			if (rlen <= 0) {
				break;
			}

			DHD_DBG_RING_LOCK(ring->lock, flags);
			ring_status.read_bytes += rlen;
			DHD_DBG_RING_UNLOCK(ring->lock, flags);

			/* payload length includes pack_hdr size */
			debug_data_send(dhdp, ringid, pack_hdr,
					rlen + DBG_RING_ENTRY_PACK_SIZE, ring_status);
			remain_buflen -= rlen;
		}
	}

	VMFREE(dhdp->osh, buf, alloc_len);

	DHD_DBG_RING_LOCK(ring->lock, flags);
	if (!ring->sched_pull) {
		ring->sched_pull = TRUE;
	}
	DHD_DBG_RING_UNLOCK(ring->lock, flags);
exit:
	if (sched) {
		/* retrigger the work at same interval */
		if ((ring_info->interval)) {
			if ((ring_status.written_bytes == ring_status.read_bytes) ||
#ifdef DHD_PKT_LOGGING_DBGRING
				(ringid == PACKET_LOG_RING_ID)) {
#else
				FALSE) {
#endif /* DHD_PKT_LOGGING_DBGRING */
				schedule_delayed_work(d_work, ring_info->interval);
			}
		}
	}

	return;
}

int
dhd_os_dbg_register_callback(int ring_id, dbg_ring_send_sub_t callback)
{
	if (!VALID_RING(ring_id))
		return BCME_RANGE;

	ring_send_sub_cb[ring_id] = callback;
	return BCME_OK;
}

int
dhd_os_dbg_register_urgent_notifier(dhd_pub_t *dhdp, dbg_urgent_noti_sub_t urgent_noti_sub)
{
	if (!dhdp || !urgent_noti_sub)
		return BCME_BADARG;
	urgent_noti_sub_cb = urgent_noti_sub;

	return BCME_OK;
}

int
dhd_os_start_logging(dhd_pub_t *dhdp, char *ring_name, int log_level,
	int flags, int time_intval, int threshold)
{
	int ret = BCME_OK;
	int ring_id;
	linux_dbgring_info_t *os_priv, *ring_info;

	ring_id = dhd_dbg_find_ring_id(dhdp, ring_name);
	if (!VALID_RING(ring_id))
		return BCME_UNSUPPORTED;

	DHD_ERROR(("%s , ring_id : %d log_level : %d, "
			"time_intval : %d, threshod %d Bytes\n",
			__FUNCTION__, ring_id, log_level, time_intval, threshold));

	/* change the configuration */
	ret = dhd_dbg_set_configuration(dhdp, ring_id, log_level, flags, threshold);
	if (ret) {
		DHD_ERROR(("dhd_set_configuration is failed : %d\n", ret));
		return ret;
	}

	os_priv = dhd_dbg_get_priv(dhdp);
	if (!os_priv)
		return BCME_ERROR;
	ring_info = &os_priv[ring_id];
	ring_info->log_level = log_level;
	if (time_intval == 0 || log_level == 0) {
		ring_info->interval = 0;
		cancel_delayed_work_sync(&ring_info->work);
	} else {
		ring_info->interval = msecs_to_jiffies(time_intval * MSEC_PER_SEC);
		cancel_delayed_work_sync(&ring_info->work);
		schedule_delayed_work(&ring_info->work, ring_info->interval);
	}

	return ret;
}

int
dhd_os_reset_logging(dhd_pub_t *dhdp)
{
	int ret = BCME_OK;
	int ring_id;
	linux_dbgring_info_t *os_priv, *ring_info;

	os_priv = dhd_dbg_get_priv(dhdp);
	if (!os_priv)
		return BCME_ERROR;

	/* Stop all rings */
	for (ring_id = DEBUG_RING_ID_INVALID + 1; ring_id < DEBUG_RING_ID_MAX; ring_id++) {
		DHD_INFO(("%s: Stop ring buffer %d\n", __FUNCTION__, ring_id));

		ring_info = &os_priv[ring_id];
		/* log level zero makes stop logging on that ring */
		ring_info->log_level = 0;
		ring_info->interval = 0;
		/* change the configuration */
		ret = dhd_dbg_set_configuration(dhdp, ring_id, 0, 0, 0);
		if (ret) {
			DHD_ERROR(("dhd_set_configuration is failed : %d\n", ret));
			return ret;
		}
		/* cancel any pending work */
		cancel_delayed_work_sync(&ring_info->work);
	}
	return ret;
}

#define SUPPRESS_LOG_LEVEL 1
int
dhd_os_suppress_logging(dhd_pub_t *dhdp, bool suppress)
{
	int ret = BCME_OK;
	int max_log_level;
	int enable = (suppress) ? 0 : 1;
	linux_dbgring_info_t *os_priv;

	os_priv = dhd_dbg_get_priv(dhdp);
	if (!os_priv)
		return BCME_ERROR;

	max_log_level = os_priv[FW_VERBOSE_RING_ID].log_level;

	if (max_log_level == SUPPRESS_LOG_LEVEL) {
		/* suppress the logging in FW not to wake up host while device in suspend mode */
		ret = dhd_iovar(dhdp, 0, "logtrace", (char *)&enable, sizeof(enable), NULL, 0,
				TRUE);
		if (ret < 0 && (ret != BCME_UNSUPPORTED)) {
			DHD_ERROR(("logtrace is failed : %d\n", ret));
		}
	}

	return ret;
}

int
dhd_os_get_ring_status(dhd_pub_t *dhdp, int ring_id, dhd_dbg_ring_status_t *dbg_ring_status)
{
	return dhd_dbg_get_ring_status(dhdp, ring_id, dbg_ring_status);
}

int
dhd_os_trigger_get_ring_data(dhd_pub_t *dhdp, char *ring_name)
{
	int ret = BCME_OK;
	int ring_id;
	linux_dbgring_info_t *os_priv, *ring_info;
	ring_id = dhd_dbg_find_ring_id(dhdp, ring_name);
	if (!VALID_RING(ring_id))
		return BCME_UNSUPPORTED;
	os_priv = dhd_dbg_get_priv(dhdp);
	if (os_priv) {
		ring_info = &os_priv[ring_id];
		if (ring_info->interval) {
			cancel_delayed_work_sync(&ring_info->work);
		}
		schedule_delayed_work(&ring_info->work, 0);
	} else {
		DHD_ERROR(("%s : os_priv is NULL\n", __FUNCTION__));
		ret = BCME_ERROR;
	}
	return ret;
}

int
dhd_os_push_push_ring_data(dhd_pub_t *dhdp, int ring_id, void *data, int32 data_len)
{
	int ret = BCME_OK, i;
	dhd_dbg_ring_entry_t msg_hdr;
	log_conn_event_t *event_data = (log_conn_event_t *)data;
	linux_dbgring_info_t *os_priv, *ring_info = NULL;

	if (!VALID_RING(ring_id))
		return BCME_UNSUPPORTED;
	os_priv = dhd_dbg_get_priv(dhdp);

	if (os_priv) {
		ring_info = &os_priv[ring_id];
	} else
		return BCME_NORESOURCE;

	memset(&msg_hdr, 0, sizeof(dhd_dbg_ring_entry_t));

	if (ring_id == DHD_EVENT_RING_ID) {
		msg_hdr.type = DBG_RING_ENTRY_EVENT_TYPE;
		msg_hdr.flags |= DBG_RING_ENTRY_FLAGS_HAS_TIMESTAMP;
		msg_hdr.flags |= DBG_RING_ENTRY_FLAGS_HAS_BINARY;
		msg_hdr.timestamp = osl_localtime_ns();
		/* convert to ms */
		msg_hdr.timestamp = DIV_U64_BY_U32(msg_hdr.timestamp, NSEC_PER_MSEC);
		msg_hdr.len = data_len;
		/* filter the event for higher log level with current log level */
		for (i = 0; i < ARRAYSIZE(dhd_event_map); i++) {
			if ((dhd_event_map[i].tag == event_data->event) &&
				dhd_event_map[i].log_level > ring_info->log_level) {
				return ret;
			}
		}
	}
#ifdef DHD_DEBUGABILITY_LOG_DUMP_RING
	else if (ring_id == FW_VERBOSE_RING_ID || ring_id == DRIVER_LOG_RING_ID ||
			ring_id == ROAM_STATS_RING_ID) {
		msg_hdr.type = DBG_RING_ENTRY_DATA_TYPE;
		msg_hdr.flags |= DBG_RING_ENTRY_FLAGS_HAS_TIMESTAMP;
		msg_hdr.timestamp = osl_localtime_ns();
		msg_hdr.timestamp = DIV_U64_BY_U32(msg_hdr.timestamp, NSEC_PER_MSEC);
		msg_hdr.len = strlen(data);
	}
#endif /* DHD_DEBUGABILITY_LOG_DUMP_RING */
	ret = dhd_dbg_push_to_ring(dhdp, ring_id, &msg_hdr, event_data);
	if (ret && ret != BCME_BUSY) {
		DHD_ERROR(("%s : failed to push data into the ring (%d) with ret(%d)\n",
			__FUNCTION__, ring_id, ret));
	}

	return ret;
}

#ifdef DBG_PKT_MON
int
dhd_os_dbg_attach_pkt_monitor(dhd_pub_t *dhdp)
{
	return dhd_dbg_attach_pkt_monitor(dhdp, dhd_os_dbg_monitor_tx_pkts,
		dhd_os_dbg_monitor_tx_status, dhd_os_dbg_monitor_rx_pkts);
}

int
dhd_os_dbg_start_pkt_monitor(dhd_pub_t *dhdp)
{
	return dhd_dbg_start_pkt_monitor(dhdp);
}

int
dhd_os_dbg_monitor_tx_pkts(dhd_pub_t *dhdp, void *pkt, uint32 pktid,
	frame_type type, uint8 mgmt_acked)
{
	return dhd_dbg_monitor_tx_pkts(dhdp, pkt, pktid, type, mgmt_acked);
}

int
dhd_os_dbg_monitor_tx_status(dhd_pub_t *dhdp, void *pkt, uint32 pktid,
	uint16 status)
{
	return dhd_dbg_monitor_tx_status(dhdp, pkt, pktid, status);
}

int
dhd_os_dbg_monitor_rx_pkts(dhd_pub_t *dhdp, void *pkt, frame_type type)
{
	return dhd_dbg_monitor_rx_pkts(dhdp, pkt, type);
}

int
dhd_os_dbg_stop_pkt_monitor(dhd_pub_t *dhdp)
{
	return dhd_dbg_stop_pkt_monitor(dhdp);
}

int
dhd_os_dbg_monitor_get_tx_pkts(dhd_pub_t *dhdp, void __user *user_buf,
	uint16 req_count, uint16 *resp_count)
{
	return dhd_dbg_monitor_get_tx_pkts(dhdp, user_buf, req_count, resp_count);
}

int
dhd_os_dbg_monitor_get_rx_pkts(dhd_pub_t *dhdp, void __user *user_buf,
	uint16 req_count, uint16 *resp_count)
{
	return dhd_dbg_monitor_get_rx_pkts(dhdp, user_buf, req_count, resp_count);
}

int
dhd_os_dbg_detach_pkt_monitor(dhd_pub_t *dhdp)
{
	return dhd_dbg_detach_pkt_monitor(dhdp);
}
#endif /* DBG_PKT_MON */

int
dhd_os_dbg_get_feature(dhd_pub_t *dhdp, int32 *features)
{
	int ret = BCME_OK;
#ifdef DEBUGABILITY
#ifndef DEBUGABILITY_DISABLE_MEMDUMP
	struct dhd_conf *conf = dhdp->conf;
#endif /* !DEBUGABILITY_DISABLE_MEMDUMP */
#endif

	/* XXX : we need to find a way to get the features for dbg */
	*features = 0;
#ifdef DEBUGABILITY
#ifndef DEBUGABILITY_DISABLE_MEMDUMP
	// fix for RequestFirmwareDebugDump issue of VTS
	if ((conf->chip != BCM4359_CHIP_ID) && (conf->chip != BCM43751_CHIP_ID) &&
			(conf->chip != BCM43752_CHIP_ID) && (conf->chip != BCM4375_CHIP_ID))
		*features |= DBG_MEMORY_DUMP_SUPPORTED;
#endif /* !DEBUGABILITY_DISABLE_MEMDUMP */
	if (FW_SUPPORTED(dhdp, logtrace)) {
		*features |= DBG_CONNECT_EVENT_SUPPORTED;
		*features |= DBG_VERBOSE_LOG_SUPPORTED;
	}
	if (FW_SUPPORTED(dhdp, hchk)) {
		*features |= DBG_HEALTH_CHECK_SUPPORTED;
	}
#ifdef DBG_PKT_MON
	if (FW_SUPPORTED(dhdp, d11status)) {
		*features |= DBG_PACKET_FATE_SUPPORTED;
	}
#endif /* DBG_PKT_MON */
#endif /* DEBUGABILITY */
#ifdef DHD_PKT_LOGGING_DBGRING
	*features |= DBG_PACKET_LOG_SUPPORTED;
#endif /* DHD_PKT_LOGGING_DBGRING */
	return ret;
}

static void
dhd_os_dbg_pullreq(void *os_priv, int ring_id)
{
	linux_dbgring_info_t *ring_info;

	ring_info = &((linux_dbgring_info_t *)os_priv)[ring_id];
	cancel_delayed_work(&ring_info->work);
	schedule_delayed_work(&ring_info->work, 0);
}

#ifdef DHD_PKT_LOGGING_DBGRING
void
dhd_os_dbg_urgent_pullreq(void *os_priv, int ring_id)
{
	dhd_os_dbg_pullreq(os_priv, ring_id);
}
#endif /* DHD_PKT_LOGGING_DBGRING */

int
dhd_os_dbg_attach(dhd_pub_t *dhdp)
{
	int ret = BCME_OK;
	linux_dbgring_info_t *os_priv, *ring_info;
	int ring_id;

	/* os_dbg data */
	os_priv = VMALLOCZ(dhdp->osh, sizeof(*os_priv) * DEBUG_RING_ID_MAX);
	if (!os_priv) {
		DHD_ERROR(("%s:%d: VMALLOC failed for os_priv, size %d\n", __FUNCTION__,
			__LINE__, (uint32)sizeof(*os_priv) * DEBUG_RING_ID_MAX));
		return BCME_NOMEM;
	}

	for (ring_id = DEBUG_RING_ID_INVALID + 1; ring_id < DEBUG_RING_ID_MAX;
	     ring_id++) {
		ring_info = &os_priv[ring_id];
		INIT_DELAYED_WORK(&ring_info->work, dbg_ring_poll_worker);
		ring_info->dhdp = dhdp;
		ring_info->ring_id = ring_id;
	}

	ret = dhd_dbg_attach(dhdp, dhd_os_dbg_pullreq, dhd_os_dbg_urgent_notifier, os_priv);
	if (ret) {
		VMFREE(dhdp->osh, os_priv, sizeof(*os_priv) * DEBUG_RING_ID_MAX);
	}

	return ret;
}

void
dhd_os_dbg_detach(dhd_pub_t *dhdp)
{
	linux_dbgring_info_t *os_priv, *ring_info;
	int ring_id;
	/* free os_dbg data */
	os_priv = dhd_dbg_get_priv(dhdp);
	if (!os_priv)
		return;
	/* abort pending any job */
	for (ring_id = DEBUG_RING_ID_INVALID + 1; ring_id < DEBUG_RING_ID_MAX; ring_id++) {
		ring_info = &os_priv[ring_id];
		if (ring_info->interval) {
			ring_info->interval = 0;
			cancel_delayed_work_sync(&ring_info->work);
		}
	}
	VMFREE(dhdp->osh, os_priv, sizeof(*os_priv) * DEBUG_RING_ID_MAX);

	return dhd_dbg_detach(dhdp);
}
