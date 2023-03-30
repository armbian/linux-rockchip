/*
 * Linux cfg80211 driver scan related code
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
 * <<Broadcom-WL-IPTag/Dual:>>
 */
/* */
#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <linux/kernel.h>

#include <bcmutils.h>
#include <bcmstdlib_s.h>
#include <bcmwifi_channels.h>
#include <bcmendian.h>
#include <ethernet.h>
#include <802.11.h>
#include <bcmiov.h>
#include <linux/if_arp.h>
#include <asm/uaccess.h>

#include <ethernet.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <linux/ieee80211.h>
#include <linux/wait.h>
#if defined(CONFIG_TIZEN)
#include <linux/net_stat_tizen.h>
#endif /* CONFIG_TIZEN */
#include <net/cfg80211.h>
#include <net/rtnetlink.h>

#include <wlioctl.h>
#include <bcmevent.h>
#include <wldev_common.h>
#include <wl_cfg80211.h>
#include <wl_cfgscan.h>
#include <wl_cfgp2p.h>
#include <wl_cfgvif.h>
#include <bcmdevs.h>

#include <wl_android.h>

#if defined(BCMDONGLEHOST)
#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_linux.h>
#include <dhd_debug.h>
#include <dhdioctl.h>
#include <wlioctl.h>
#include <dhd_cfg80211.h>
#include <dhd_bus.h>
#include <wl_cfgvendor.h>
#endif /* defined(BCMDONGLEHOST) */
#ifdef BCMPCIE
#include <dhd_flowring.h>
#endif
#ifdef PNO_SUPPORT
#include <dhd_pno.h>
#endif /* PNO_SUPPORT */
#ifdef RTT_SUPPORT
#include "dhd_rtt.h"
#endif /* RTT_SUPPORT */
#ifdef WL_CELLULAR_CHAN_AVOID
#include <wl_cfg_cellavoid.h>
#endif /* WL_CELLULAR_CHAN_AVOID */
#include <dhd_config.h>

#define ACTIVE_SCAN 1
#define PASSIVE_SCAN 0

#define MIN_P2P_IE_LEN	8	/* p2p_ie->OUI(3) + p2p_ie->oui_type(1) +
				 * Attribute ID(1) + Length(2) + 1(Mininum length:1)
				 */
#define MAX_P2P_IE_LEN	251	/* Up To 251 */

#define WPS_ATTR_REQ_TYPE 0x103a
#define WPS_REQ_TYPE_ENROLLEE 0x01
#define SCAN_WAKE_LOCK_MARGIN_MS 500

#if defined(WL_CFG80211_P2P_DEV_IF)
#define CFG80211_READY_ON_CHANNEL(cfgdev, cookie, channel, channel_type, duration, flags) \
	cfg80211_ready_on_channel(cfgdev, cookie, channel, duration, GFP_KERNEL);
#else
#define CFG80211_READY_ON_CHANNEL(cfgdev, cookie, channel, channel_type, duration, flags) \
	cfg80211_ready_on_channel(cfgdev, cookie, channel, channel_type, duration, GFP_KERNEL);
#endif /* WL_CFG80211_P2P_DEV_IF */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
#define CFG80211_SCHED_SCAN_STOPPED(wiphy, schedscan_req) \
	cfg80211_sched_scan_stopped(wiphy, schedscan_req->reqid);
#else
#define CFG80211_SCHED_SCAN_STOPPED(wiphy, schedscan_req) \
	cfg80211_sched_scan_stopped(wiphy);
#endif /* KERNEL > 4.11.0 */

#define IS_DFS(chaninfo) ((chaninfo & WL_CHAN_RADAR) || \
	 (chaninfo & WL_CHAN_PASSIVE))

#if defined(USE_INITIAL_2G_SCAN) || defined(USE_INITIAL_SHORT_DWELL_TIME)
#define FIRST_SCAN_ACTIVE_DWELL_TIME_MS 40
bool g_first_broadcast_scan = TRUE;
#endif /* USE_INITIAL_2G_SCAN || USE_INITIAL_SHORT_DWELL_TIME */
#ifdef P2P_LISTEN_OFFLOADING
void wl_cfg80211_cancel_p2plo(struct bcm_cfg80211 *cfg);
#endif /* P2P_LISTEN_OFFLOADING */
static void _wl_notify_scan_done(struct bcm_cfg80211 *cfg, bool aborted);
static s32 wl_notify_escan_complete(struct bcm_cfg80211 *cfg,
	struct net_device *ndev, bool aborted);
void wl_cfgscan_scan_abort(struct bcm_cfg80211 *cfg);
static void _wl_cfgscan_cancel_scan(struct bcm_cfg80211 *cfg);

#ifdef PNO_SUPPORT
void wl_cfg80211_stop_pno(struct bcm_cfg80211 *cfg, struct net_device *dev);
#endif /* WL_SCHED_SCAN */

#ifdef ROAM_CHANNEL_CACHE
void print_roam_cache(struct bcm_cfg80211 *cfg);
#endif /* ROAM_CHANNEL_CACHE */

extern int passive_channel_skip;

#ifdef DUAL_ESCAN_RESULT_BUFFER
static wl_scan_results_v109_t *
wl_escan_get_buf(struct bcm_cfg80211 *cfg, bool aborted)
{
	u8 index;
	if (aborted) {
		if (cfg->escan_info.escan_type[0] == cfg->escan_info.escan_type[1]) {
			index = (cfg->escan_info.cur_sync_id + 1)%SCAN_BUF_CNT;
		} else {
			index = (cfg->escan_info.cur_sync_id)%SCAN_BUF_CNT;
		}
	} else {
		index = (cfg->escan_info.cur_sync_id)%SCAN_BUF_CNT;
	}

	return (wl_scan_results_v109_t *)cfg->escan_info.escan_buf[index];
}

static int
wl_escan_check_sync_id(struct bcm_cfg80211 *cfg, s32 status, u16 result_id, u16 wl_id)
{
	if (result_id != wl_id) {
		WL_ERR(("ESCAN sync id mismatch :status :%d "
			"cur_sync_id:%d coming sync_id:%d\n",
			status, wl_id, result_id));
#ifdef DHD_SEND_HANG_ESCAN_SYNCID_MISMATCH
		if (cfg->escan_info.prev_escan_aborted == FALSE) {
			wl_cfg80211_handle_hang_event(bcmcfg_to_prmry_ndev(cfg),
				HANG_REASON_ESCAN_SYNCID_MISMATCH, DUMP_TYPE_ESCAN_SYNCID_MISMATCH);
		}
#endif /* DHD_SEND_HANG_ESCAN_SYNCID_MISMATCH */
		return -1;
	} else {
		return 0;
	}
}

#ifdef SYNCID_MISMATCH_DEBUG
#define wl_escan_increment_sync_id(a, b) \
	((u8)((a)->escan_info.cur_sync_id + b) == 0 ? \
	((a)->escan_info.cur_sync_id = 1) : ((a)->escan_info.cur_sync_id += b))
#define wl_escan_init_sync_id(a) ((a)->escan_info.cur_sync_id = 1)
#else
#define wl_escan_increment_sync_id(a, b) ((a)->escan_info.cur_sync_id += b)
#define wl_escan_init_sync_id(a) ((a)->escan_info.cur_sync_id = 0)
#endif /* SYNCID_MISMATCH_DEBUG */
#else
#define wl_escan_get_buf(a, b) ((wl_scan_results_v109_t *) (a)->escan_info.escan_buf)
#define wl_escan_check_sync_id(a, b, c, d) 0
#define wl_escan_increment_sync_id(a, b)
#define wl_escan_init_sync_id(a)
#endif /* DUAL_ESCAN_RESULT_BUFFER */

/*
 * information element utilities
 */
static void wl_rst_ie(struct bcm_cfg80211 *cfg)
{
	struct wl_ie *ie = wl_to_ie(cfg);

	ie->offset = 0;
	bzero(ie->buf, sizeof(ie->buf));
}

static void wl_update_hidden_ap_ie(wl_bss_info_v109_t *bi, const u8 *ie_stream, u32 *ie_size,
	bool update_ssid, u32 *ssid_len_from_ie)
{
	u8 *ssidie;
	int32 ssid_len = MIN(bi->SSID_len, DOT11_MAX_SSID_LEN);
	int32 remaining_ie_buf_len, available_buffer_len, unused_buf_len;
	/* cfg80211_find_ie defined in kernel returning const u8 */
	int ret = 0;

	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	ssidie = (u8 *)cfg80211_find_ie(WLAN_EID_SSID, ie_stream, *ie_size);
	GCC_DIAGNOSTIC_POP();

	/* ERROR out if
	 * 1. No ssid IE is FOUND or
	 * 2. New ssid length is > what was allocated for existing ssid (as
	 * we do not want to overwrite the rest of the IEs) or
	 * 3. If in case of erroneous buffer input where ssid length doesnt match the space
	 * allocated to it.
	 */
	if (!ssidie) {
		return;
	}
	*ssid_len_from_ie = (u32)ssidie[1];

	available_buffer_len = ((int)(*ie_size)) - (ssidie + 2 - ie_stream);
	remaining_ie_buf_len = available_buffer_len - (int)ssidie[1];
	unused_buf_len = WL_EXTRA_BUF_MAX - (4 + bi->length + *ie_size);
	if (ssidie[1] > available_buffer_len) {
		WL_ERR_MEM(("wl_update_hidden_ap_ie: skip wl_update_hidden_ap_ie : overflow\n"));
		return;
	}

	/* ssidie[1] can be different with bi->SSID_len only if roaming status
	 * On scanning the values will be same each other.
	 */

	if (ssidie[1] != ssid_len) {
		if (ssidie[1]) {
			WL_ERR_RLMT(("wl_update_hidden_ap_ie: Wrong SSID len: %d != %d\n",
				ssidie[1], bi->SSID_len));
		}
		/* ssidie[1] is 1 in beacon on CISCO hidden networks. */
		/*
		 * The bss info in firmware gets updated from beacon and probe resp.
		 * In case of hidden network, the bss_info that got updated by beacon,
		 * will not carry SSID and this can result in cfg80211_get_bss not finding a match.
		 * so include the SSID element.
		 */
		if ((update_ssid && (ssid_len > ssidie[1])) && (unused_buf_len > ssid_len)) {
			WL_INFORM_MEM(("Changing the SSID Info.\n"));
			ret = memmove_s(ssidie + ssid_len + 2, available_buffer_len,
				(ssidie + 2) + ssidie[1], remaining_ie_buf_len);
			if (ret) {
				WL_ERR(("SSID Info memmove failed:%d, destsz:%d, n:%d\n",
					ret, available_buffer_len, remaining_ie_buf_len));
				return;
			}
			ret = memcpy_s(ssidie + 2, DOT11_MAX_SSID_LEN, bi->SSID, ssid_len);
			if (ret) {
				WL_ERR(("SSID Info memcpy failed:%d, destsz:%d, n:%d\n",
					ret, DOT11_MAX_SSID_LEN, ssid_len));
				return;
			}
			*ie_size = *ie_size + ssid_len - ssidie[1];
			ssidie[1] = ssid_len;
		} else if (ssid_len < ssidie[1]) {
			WL_ERR_MEM(("wl_update_hidden_ap_ie: Invalid SSID len: %d < %d\n",
				bi->SSID_len, ssidie[1]));
		}
		return;
	}
	if (*(ssidie + 2) == '\0') {
		 ret = memcpy_s(ssidie + 2, DOT11_MAX_SSID_LEN, bi->SSID, ssid_len);
		if (ret) {
			WL_ERR(("memcopy failed:%d, destsz:%d, n:%d\n",
				ret, DOT11_MAX_SSID_LEN, ssid_len));
			return;
		}
	}
	return;
}

static s32 wl_mrg_ie(struct bcm_cfg80211 *cfg, u8 *ie_stream, u16 ie_size)
{
	struct wl_ie *ie = wl_to_ie(cfg);
	s32 err = 0;
	int ret = 0;

	if (unlikely(ie->offset + ie_size > WL_TLV_INFO_MAX)) {
		WL_ERR(("ei_stream crosses buffer boundary\n"));
		return -ENOSPC;
	}
	ret = memcpy_s(&ie->buf[ie->offset], (sizeof(ie->buf) - ie->offset),
		ie_stream, ie_size);
	if (ret) {
		WL_ERR(("memcpy failed:%d, destsz: %zu, n: %d\n",
			ret, (sizeof(ie->buf) - ie->offset), ie_size));
		return BCME_ERROR;
	}
	ie->offset += ie_size;

	return err;
}

static s32 wl_cp_ie(struct bcm_cfg80211 *cfg, u8 *dst, u16 dst_size)
{
	struct wl_ie *ie = wl_to_ie(cfg);
	s32 err = 0;

	if (unlikely(ie->offset > dst_size)) {
		WL_ERR(("dst_size is not enough\n"));
		return -ENOSPC;
	}
	memcpy(dst, &ie->buf[0], ie->offset);

	return err;
}

static u32 wl_get_ielen(struct bcm_cfg80211 *cfg)
{
	struct wl_ie *ie = wl_to_ie(cfg);

	return ie->offset;
}

s32 wl_inform_single_bss(struct bcm_cfg80211 *cfg, wl_bss_info_v109_t *bi, bool update_ssid)
{
	struct wiphy *wiphy = bcmcfg_to_wiphy(cfg);
	struct ieee80211_mgmt *mgmt;
	struct ieee80211_channel *channel;
	struct wl_cfg80211_bss_info *notif_bss_info;
	struct wl_scan_req *sr = wl_to_sr(cfg);
	struct beacon_proberesp *beacon_proberesp;
	struct cfg80211_bss *cbss = NULL;
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);
	log_conn_event_t *event_data = NULL;
	tlv_log *tlv_data = NULL;
	u32 alloc_len;
	u32 payload_len;
	s32 mgmt_type;
	s32 signal;
	u32 freq;
	s32 err = 0;
	gfp_t aflags;
	u8 tmp_buf[IEEE80211_MAX_SSID_LEN + 1];
	u32 ssid_len_from_ie = 0;
	chanspec_t chanspec;

	if (unlikely(dtoh32(bi->length) > WL_BSS_INFO_MAX)) {
		WL_DBG(("Beacon is larger than buffer. Discarding\n"));
		return err;
	}

	if (bi->length < (bi->ie_offset + bi->ie_length)) {
		WL_ERR(("IE length is not Valid. IE offse:%d, len:%d\n",
			bi->ie_offset, bi->ie_length));
		return -EINVAL;
	}

	if (bi->SSID_len > IEEE80211_MAX_SSID_LEN) {
		WL_ERR(("wrong SSID len:%d\n", bi->SSID_len));
		return -EINVAL;
	}

	aflags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	notif_bss_info = (struct wl_cfg80211_bss_info *)MALLOCZ(cfg->osh,
		sizeof(*notif_bss_info) + sizeof(*mgmt) - sizeof(u8) + WL_BSS_INFO_MAX);
	if (unlikely(!notif_bss_info)) {
		WL_ERR(("notif_bss_info alloc failed\n"));
		return -ENOMEM;
	}
	/* Check for all currently supported bands */
	if (!(
#ifdef WL_6G_BAND
		CHSPEC_IS6G(bi->chanspec) ||
#endif /* WL_6G_BAND */
		CHSPEC_IS5G(bi->chanspec) || CHSPEC_IS2G(bi->chanspec))) {
		WL_ERR(("No valid band"));
		MFREE(cfg->osh, notif_bss_info, sizeof(*notif_bss_info)
			+ sizeof(*mgmt) - sizeof(u8) + WL_BSS_INFO_MAX);
		return -EINVAL;
	}

	mgmt = (struct ieee80211_mgmt *)notif_bss_info->frame_buf;
	chanspec = wl_chspec_driver_to_host(bi->chanspec);
	notif_bss_info->channel = wf_chspec_ctlchan(chanspec);
	notif_bss_info->band = CHSPEC_BAND(bi->chanspec);
	notif_bss_info->rssi = dtoh16(bi->RSSI);
#if defined(RSSIAVG)
	notif_bss_info->rssi = wl_get_avg_rssi(&cfg->g_rssi_cache_ctrl, &bi->BSSID);
	if (notif_bss_info->rssi == RSSI_MINVAL)
		notif_bss_info->rssi = MIN(dtoh16(bi->RSSI), RSSI_MAXVAL);
#endif
#if defined(RSSIOFFSET)
	notif_bss_info->rssi = wl_update_rssi_offset(bcmcfg_to_prmry_ndev(cfg), notif_bss_info->rssi);
#endif
#if !defined(RSSIAVG) && !defined(RSSIOFFSET)
	// terence 20150419: limit the max. rssi to -2 or the bss will be filtered out in android OS
	notif_bss_info->rssi = MIN(notif_bss_info->rssi, RSSI_MAXVAL);
#endif
	memcpy(mgmt->bssid, &bi->BSSID, ETHER_ADDR_LEN);
	mgmt_type = cfg->active_scan ?
		IEEE80211_STYPE_PROBE_RESP : IEEE80211_STYPE_BEACON;
	if (!memcmp(bi->SSID, sr->ssid.SSID, bi->SSID_len)) {
	    mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | mgmt_type);
	}
	beacon_proberesp = cfg->active_scan ?
		(struct beacon_proberesp *)&mgmt->u.probe_resp :
		(struct beacon_proberesp *)&mgmt->u.beacon;
	beacon_proberesp->timestamp = 0;
	beacon_proberesp->beacon_int = cpu_to_le16(bi->beacon_period);
	beacon_proberesp->capab_info = cpu_to_le16(bi->capability);
	wl_rst_ie(cfg);
	wl_update_hidden_ap_ie(bi, ((u8 *) bi) + bi->ie_offset, &bi->ie_length,
		update_ssid, &ssid_len_from_ie);
	if (ssid_len_from_ie > IEEE80211_MAX_SSID_LEN) {
		WL_ERR(("wrong SSID len from ie: %d\n", ssid_len_from_ie));
		err = -EINVAL;
		goto out_err;
	}
	wl_mrg_ie(cfg, ((u8 *) bi) + bi->ie_offset, bi->ie_length);
	wl_cp_ie(cfg, beacon_proberesp->variable, WL_BSS_INFO_MAX -
		offsetof(struct wl_cfg80211_bss_info, frame_buf));
	notif_bss_info->frame_len = offsetof(struct ieee80211_mgmt,
		u.beacon.variable) + wl_get_ielen(cfg);
	freq = wl_channel_to_frequency(notif_bss_info->channel, notif_bss_info->band);
	if (freq == 0) {
		WL_ERR(("Invalid channel, failed to change channel to freq\n"));
		MFREE(cfg->osh, notif_bss_info, sizeof(*notif_bss_info)
			+ sizeof(*mgmt) - sizeof(u8) + WL_BSS_INFO_MAX);
		return -EINVAL;
	}
	channel = ieee80211_get_channel(wiphy, freq);
	memcpy(tmp_buf, bi->SSID, bi->SSID_len);
	tmp_buf[bi->SSID_len] = '\0';
	WL_SCAN(("BSSID %pM, channel %s-%-3d(%3d %3sMHz), rssi %3d, capa 0x%-4x, mgmt_type %d, "
		"frame_len %3d, SSID \"%s\"\n",
		&bi->BSSID, CHSPEC2BANDSTR(chanspec), notif_bss_info->channel, CHSPEC_CHANNEL(chanspec),
		CHSPEC_IS20(chanspec)?"20":
		CHSPEC_IS40(chanspec)?"40":
		CHSPEC_IS80(chanspec)?"80":
		CHSPEC_IS160(chanspec)?"160":"??",
		notif_bss_info->rssi, mgmt->u.beacon.capab_info, mgmt_type,
		notif_bss_info->frame_len, tmp_buf));
	if (unlikely(!channel)) {
		WL_ERR(("ieee80211_get_channel error\n"));
		MFREE(cfg->osh, notif_bss_info, sizeof(*notif_bss_info)
			+ sizeof(*mgmt) - sizeof(u8) + WL_BSS_INFO_MAX);
		return -EINVAL;
	}

	signal = notif_bss_info->rssi * 100;
	if (!mgmt->u.probe_resp.timestamp) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
		struct osl_timespec ts;
		osl_get_monotonic_boottime(&ts);
		mgmt->u.probe_resp.timestamp = ((u64)ts.tv_sec*USEC_PER_SEC)
				+ (ts.tv_nsec / NSEC_PER_USEC);
#else
		struct osl_timespec tv;
		osl_do_gettimeofday(&tv);
		mgmt->u.probe_resp.timestamp = ((u64)tv.tv_sec*1000000)
				+ tv.tv_usec;
#endif
	}

	cbss = cfg80211_inform_bss_frame(wiphy, channel, mgmt,
		le16_to_cpu(notif_bss_info->frame_len), signal, aflags);
	if (unlikely(!cbss)) {
		WL_ERR(("cfg80211_inform_bss_frame error bssid " MACDBG " channel %d \n",
			MAC2STRDBG((u8*)(&bi->BSSID)), notif_bss_info->channel));
		err = -EINVAL;
		goto out_err;
	}

	CFG80211_PUT_BSS(wiphy, cbss);

	if (DBG_RING_ACTIVE(dhdp, DHD_EVENT_RING_ID) &&
			(cfg->sched_scan_req && !cfg->scan_request)) {
		alloc_len = sizeof(log_conn_event_t) + (3 * sizeof(tlv_log)) +
			IEEE80211_MAX_SSID_LEN + sizeof(uint16) +
			sizeof(int16);
		event_data = (log_conn_event_t *)MALLOCZ(dhdp->osh, alloc_len);
		if (!event_data) {
			WL_ERR(("%s: failed to allocate the log_conn_event_t with "
				"length(%d)\n", __func__, alloc_len));
			goto out_err;
		}

		payload_len = sizeof(log_conn_event_t);
		event_data->event = WIFI_EVENT_DRIVER_PNO_SCAN_RESULT_FOUND;
		tlv_data = event_data->tlvs;

		/* ssid */
		tlv_data->tag = WIFI_TAG_SSID;
		tlv_data->len = bi->SSID_len;
		memcpy(tlv_data->value, bi->SSID, bi->SSID_len);
		payload_len += TLV_LOG_SIZE(tlv_data);
		tlv_data = TLV_LOG_NEXT(tlv_data);

		/* channel */
		tlv_data->tag = WIFI_TAG_CHANNEL;
		tlv_data->len = sizeof(uint16);
		memcpy(tlv_data->value, &notif_bss_info->channel, sizeof(uint16));
		payload_len += TLV_LOG_SIZE(tlv_data);
		tlv_data = TLV_LOG_NEXT(tlv_data);

		/* rssi */
		tlv_data->tag = WIFI_TAG_RSSI;
		tlv_data->len = sizeof(int16);
		memcpy(tlv_data->value, &notif_bss_info->rssi, sizeof(int16));
		payload_len += TLV_LOG_SIZE(tlv_data);
		tlv_data = TLV_LOG_NEXT(tlv_data);

		dhd_os_push_push_ring_data(dhdp, DHD_EVENT_RING_ID,
			event_data, payload_len);
		MFREE(dhdp->osh, event_data, alloc_len);
	}

out_err:
	MFREE(cfg->osh, notif_bss_info, sizeof(*notif_bss_info)
			+ sizeof(*mgmt) - sizeof(u8) + WL_BSS_INFO_MAX);
	return err;
}

struct wireless_dev * wl_get_scan_wdev(struct bcm_cfg80211 *cfg);
struct net_device *
wl_get_scan_ndev(struct bcm_cfg80211 *cfg)
{
	struct wireless_dev *wdev = NULL;
	struct net_device *ndev = NULL;

	wdev = wl_get_scan_wdev(cfg);
	if (!wdev) {
		WL_ERR(("No wdev present\n"));
		return NULL;
	}

	ndev = wdev_to_wlc_ndev(wdev, cfg);
	if (!ndev) {
		WL_ERR(("No ndev present\n"));
	}

	return ndev;
}

#if defined(BSSCACHE) || defined(RSSIAVG)
void wl_cfg80211_update_bss_cache(struct bcm_cfg80211 *cfg)
{
#if defined(RSSIAVG)
	struct net_device *ndev = wl_get_scan_ndev(cfg);
	int rssi;
#endif
	wl_scan_results_v109_t *bss_list = cfg->bss_list;

	/* Free cache in p2p scanning*/
	if (p2p_is_on(cfg) && p2p_scan(cfg)) {
#if defined(RSSIAVG)
		wl_free_rssi_cache(&cfg->g_rssi_cache_ctrl);
#endif
#if defined(BSSCACHE)
		wl_free_bss_cache(&cfg->g_bss_cache_ctrl);
#endif
	}

	/* Update cache */
#if defined(RSSIAVG)
	wl_update_rssi_cache(&cfg->g_rssi_cache_ctrl, bss_list);
	if (!in_atomic() && ndev) {
		wl_update_connected_rssi_cache(ndev, &cfg->g_rssi_cache_ctrl, &rssi);
	}
#endif
#if defined(BSSCACHE)
	wl_update_bss_cache(&cfg->g_bss_cache_ctrl,
#if defined(RSSIAVG)
		&cfg->g_rssi_cache_ctrl,
#endif
		bss_list);
#endif

	/* delete dirty cache */
#if defined(RSSIAVG)
	wl_delete_dirty_rssi_cache(&cfg->g_rssi_cache_ctrl);
	wl_reset_rssi_cache(&cfg->g_rssi_cache_ctrl);
#endif
#if defined(BSSCACHE)
	wl_delete_dirty_bss_cache(&cfg->g_bss_cache_ctrl);
	wl_reset_bss_cache(&cfg->g_bss_cache_ctrl);
#endif

}
#endif

#if defined(BSSCACHE)
s32 wl_inform_bss_cache(struct bcm_cfg80211 *cfg)
{
	wl_scan_results_v109_t *bss_list = cfg->bss_list;
	wl_bss_info_v109_t *bi = NULL;	/* must be initialized */
	s32 err = 0;
	s32 i, cnt;
	wl_bss_cache_t *node;

	WL_SCAN(("scanned AP count (%d)\n", bss_list->count));
	bss_list = cfg->bss_list;
	preempt_disable();
	bi = next_bss(bss_list, bi);
	for_each_bss(bss_list, bi, i) {
		err = wl_inform_single_bss(cfg, bi, false);
		if (unlikely(err)) {
			WL_ERR(("bss inform failed\n"));
		}
	}

	cnt = i;
	node = cfg->g_bss_cache_ctrl.m_cache_head;
	WL_SCAN(("cached AP count (%d)\n", wl_bss_cache_size(&cfg->g_bss_cache_ctrl)));
	for (i=cnt; node && i<WL_AP_MAX; i++) {
		if (node->dirty > 1) {
			bi = node->results.bss_info;
			err = wl_inform_single_bss(cfg, bi, false);
		}
		node = node->next;
	}
	preempt_enable();

	return err;
}
#endif

static s32
wl_inform_bss(struct bcm_cfg80211 *cfg)
{
#if !defined(BSSCACHE)
	wl_scan_results_v109_t *bss_list;
	wl_bss_info_v109_t *bi = NULL;	/* must be initialized */
	s32 i;
#endif
	struct net_device *ndev = wl_get_scan_ndev(cfg);
	s32 err = 0;

#ifdef WL_EXT_IAPSTA
	if (ndev)
		wl_ext_in4way_sync(ndev, 0, WL_EXT_STATUS_SCAN_COMPLETE, NULL);
#endif

#if defined(BSSCACHE) || defined(RSSIAVG)
	wl_cfg80211_update_bss_cache(cfg);
#endif

#if defined(BSSCACHE)
	err = wl_inform_bss_cache(cfg);
#else
	bss_list = cfg->bss_list;
	WL_SCAN(("scanned AP count (%d)\n", bss_list->count));
#ifdef ESCAN_CHANNEL_CACHE
	reset_roam_cache(cfg);
#endif /* ESCAN_CHANNEL_CACHE */
	preempt_disable();
	bi = next_bss(bss_list, bi);
	for_each_bss(bss_list, bi, i) {
#ifdef ESCAN_CHANNEL_CACHE
		add_roam_cache(cfg, bi);
#endif /* ESCAN_CHANNEL_CACHE */
		err = wl_inform_single_bss(cfg, bi, false);
		if (unlikely(err)) {
			WL_ERR(("bss inform failed\n"));
		}
	}
	preempt_enable();
#endif

	if (cfg->autochannel && ndev) {
#if defined(BSSCACHE)
		wl_ext_get_best_channel(ndev, &cfg->g_bss_cache_ctrl,
			&cfg->best_2g_ch, &cfg->best_5g_ch, &cfg->best_6g_ch);
#else
		wl_ext_get_best_channel(ndev, bss_list,
			&cfg->best_2g_ch, &cfg->best_5g_ch, &cfg->best_6g_ch);
#endif
	}

	WL_MEM(("cfg80211 scan cache updated\n"));
#ifdef ROAM_CHANNEL_CACHE
	/* print_roam_cache(); */
	update_roam_cache(cfg, ioctl_version);
#endif /* ROAM_CHANNEL_CACHE */
	return err;
}

#ifdef WL11U
static bcm_tlv_t *
wl_cfg80211_find_interworking_ie(const u8 *parse, u32 len)
{
	bcm_tlv_t *ie;

/* unfortunately it's too much work to dispose the const cast - bcm_parse_tlvs
 * is used everywhere and changing its prototype to take const qualifier needs
 * a massive change to all its callers...
 */

	if ((ie = bcm_parse_tlvs(parse, len, DOT11_MNG_INTERWORKING_ID))) {
		return ie;
	}
	return NULL;
}

static s32
wl_cfg80211_clear_iw_ie(struct bcm_cfg80211 *cfg, struct net_device *ndev, s32 bssidx)
{
	ie_setbuf_t ie_setbuf;

	WL_DBG(("clear interworking IE\n"));

	bzero(&ie_setbuf, sizeof(ie_setbuf_t));

	ie_setbuf.ie_buffer.iecount = htod32(1);
	ie_setbuf.ie_buffer.ie_list[0].ie_data.id = DOT11_MNG_INTERWORKING_ID;
	ie_setbuf.ie_buffer.ie_list[0].ie_data.len = 0;

	return wldev_iovar_setbuf_bsscfg(ndev, "ie", &ie_setbuf, sizeof(ie_setbuf),
		cfg->ioctl_buf, WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);
}

static s32
wl_cfg80211_add_iw_ie(struct bcm_cfg80211 *cfg, struct net_device *ndev, s32 bssidx, s32 pktflag,
                      uint8 ie_id, uint8 *data, uint8 data_len)
{
	s32 err = BCME_OK;
	s32 buf_len;
	ie_setbuf_t *ie_setbuf;
	ie_getbuf_t ie_getbufp = {0, 0};
	char getbuf[WLC_IOCTL_SMLEN];

	if (ie_id != DOT11_MNG_INTERWORKING_ID) {
		WL_ERR(("unsupported (id=%d)\n", ie_id));
		return BCME_UNSUPPORTED;
	}

	/* access network options (1 octet)  is the mandatory field */
	if (!data || data_len == 0 || data_len > IW_IES_MAX_BUF_LEN) {
		WL_ERR(("wrong interworking IE (len=%d)\n", data_len));
		return BCME_BADARG;
	}

	/* Validate the pktflag parameter */
	if ((pktflag & ~(VNDR_IE_BEACON_FLAG | VNDR_IE_PRBRSP_FLAG |
			VNDR_IE_ASSOCRSP_FLAG | VNDR_IE_AUTHRSP_FLAG |
			VNDR_IE_PRBREQ_FLAG | VNDR_IE_ASSOCREQ_FLAG|
			VNDR_IE_CUSTOM_FLAG))) {
		WL_ERR(("invalid packet flag 0x%x\n", pktflag));
		return BCME_BADARG;
	}

	buf_len = sizeof(ie_setbuf_t) + data_len - 1;

	ie_getbufp.id = DOT11_MNG_INTERWORKING_ID;
	if (wldev_iovar_getbuf_bsscfg(ndev, "ie", (void *)&ie_getbufp,
			sizeof(ie_getbufp), getbuf, WLC_IOCTL_SMLEN, bssidx, &cfg->ioctl_buf_sync)
			== BCME_OK) {
		if (!memcmp(&getbuf[TLV_HDR_LEN], data, data_len)) {
			WL_DBG(("skip to set interworking IE\n"));
			return BCME_OK;
		}
	}

	/* if already set with previous values, delete it first */
	if (cfg->wl11u) {
		if ((err = wl_cfg80211_clear_iw_ie(cfg, ndev, bssidx)) != BCME_OK) {
			return err;
		}
	}

	ie_setbuf = (ie_setbuf_t *)MALLOCZ(cfg->osh, buf_len);
	if (!ie_setbuf) {
		WL_ERR(("Error allocating buffer for IE\n"));
		return -ENOMEM;
	}
	strlcpy(ie_setbuf->cmd, "add", sizeof(ie_setbuf->cmd));

	/* Buffer contains only 1 IE */
	ie_setbuf->ie_buffer.iecount = htod32(1);
	/* use VNDR_IE_CUSTOM_FLAG flags for none vendor IE . currently fixed value */
	ie_setbuf->ie_buffer.ie_list[0].pktflag = htod32(pktflag);

	/* Now, add the IE to the buffer */
	ie_setbuf->ie_buffer.ie_list[0].ie_data.id = DOT11_MNG_INTERWORKING_ID;
	ie_setbuf->ie_buffer.ie_list[0].ie_data.len = data_len;
	/* Returning void here as max data_len can be 8 */
	(void)memcpy_s((uchar *)&ie_setbuf->ie_buffer.ie_list[0].ie_data.data[0], sizeof(uint8),
		data, data_len);

	if ((err = wldev_iovar_setbuf_bsscfg(ndev, "ie", ie_setbuf, buf_len,
			cfg->ioctl_buf, WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync))
			== BCME_OK) {
		WL_DBG(("set interworking IE\n"));
		cfg->wl11u = TRUE;
		err = wldev_iovar_setint_bsscfg(ndev, "grat_arp", 1, bssidx);
	}

	MFREE(cfg->osh, ie_setbuf, buf_len);
	return err;
}
#endif /* WL11U */

#ifdef WL_BCNRECV
/* Beacon recv results handler sending to upper layer */
static s32
wl_bcnrecv_result_handler(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
		wl_bss_info_v109_2_t *bi, uint32 scan_status)
{
	s32 err = BCME_OK;
	struct wiphy *wiphy = NULL;
	wl_bcnrecv_result_t *bcn_recv = NULL;
	struct osl_timespec ts;
	int ret = 0;
	if (!bi) {
		WL_ERR(("%s: bi is NULL\n", __func__));
		err = BCME_NORESOURCE;
		goto exit;
	}
	if ((bi->length - bi->ie_length) < sizeof(wl_bss_info_v109_2_t)) {
		WL_ERR(("bi info version doesn't support bcn_recv attributes\n"));
		goto exit;
	}

	if (scan_status == WLC_E_STATUS_RXBCN) {
		wiphy = cfg->wdev->wiphy;
		if (!wiphy) {
			 WL_ERR(("wiphy is NULL\n"));
			 err = BCME_NORESOURCE;
			 goto exit;
		}
		bcn_recv = (wl_bcnrecv_result_t *)MALLOCZ(cfg->osh, sizeof(*bcn_recv));
		if (unlikely(!bcn_recv)) {
			WL_ERR(("Failed to allocate memory\n"));
			return -ENOMEM;
		}
		ret = memcpy_s((char *)bcn_recv->SSID, sizeof(bcn_recv->SSID),
			(char *)bi->SSID, bi->SSID_len);
		if (ret) {
			WL_ERR(("memcpy failed:%d, destsz:%lu, n:%d\n",
				ret, sizeof(bcn_recv->SSID), bi->SSID_len));
			err = BCME_ERROR;
			goto exit;
		}
		eacopy(&bi->BSSID, &bcn_recv->BSSID);
		bcn_recv->channel = wf_chspec_ctlchan(
			wl_chspec_driver_to_host(bi->chanspec));
		bcn_recv->beacon_interval = bi->beacon_period;

		/* kernal timestamp */
		osl_get_monotonic_boottime(&ts);
		bcn_recv->system_time = ((u64)ts.tv_sec*USEC_PER_SEC)
				+ ts.tv_nsec / NSEC_PER_USEC;
		bcn_recv->timestamp[0] = bi->timestamp[0];
		bcn_recv->timestamp[1] = bi->timestamp[1];
		if ((err = wl_android_bcnrecv_event(cfgdev_to_wlc_ndev(cfgdev, cfg),
				BCNRECV_ATTR_BCNINFO, 0, 0,
				(uint8 *)bcn_recv, sizeof(*bcn_recv)))
				!= BCME_OK) {
			WL_ERR(("failed to send bcnrecv event, error:%d\n", err));
		}
	} else {
		WL_DBG(("Ignoring Escan Event:%d \n", scan_status));
	}
exit:
	if (bcn_recv) {
		MFREE(cfg->osh, bcn_recv, sizeof(*bcn_recv));
	}
	return err;
}
#endif /* WL_BCNRECV */

#ifdef ESCAN_BUF_OVERFLOW_MGMT
#ifndef WL_DRV_AVOID_SCANCACHE
#ifdef WL_6G_BAND
#define WL_6G_CMP	30	/* (dBm) 6G Band RSSI compensation */
#define BSSRSSI(b)	\
	(CHSPEC_IS6G((b)->chanspec) ? (((b)->RSSI) + (WL_6G_CMP)) : ((b)->RSSI))
#else
#define BSSRSSI(b)	((b)->RSSI)
#endif /* WL_6G_BAND */

static void
wl_cfg80211_find_removal_candidate(wl_bss_info_v109_t *bss, removal_element_t *candidate)
{
	int idx;
	for (idx = 0; idx < BUF_OVERFLOW_MGMT_COUNT; idx++) {
		int len = BUF_OVERFLOW_MGMT_COUNT - idx - 1;
		if (BSSRSSI(bss) < candidate[idx].RSSI) {
			if (len) {
				/* In the below memcpy operation the candidate array always has the
				* buffer space available to max 'len' calculated in the for loop.
				*/
				(void)memcpy_s(&candidate[idx + 1],
					(sizeof(removal_element_t) * len),
					&candidate[idx], sizeof(removal_element_t) * len);
			}
			candidate[idx].RSSI = BSSRSSI(bss);
			candidate[idx].length = bss->length;
			(void)memcpy_s(&candidate[idx].BSSID, ETHER_ADDR_LEN,
				&bss->BSSID, ETHER_ADDR_LEN);
			return;
		}
	}
}

static void
wl_cfg80211_remove_lowRSSI_info(wl_scan_results_v109_t *list, removal_element_t *candidate,
	wl_bss_info_v109_t *bi)
{
	int idx1, idx2;
	int total_delete_len = 0;
	for (idx1 = 0; idx1 < BUF_OVERFLOW_MGMT_COUNT; idx1++) {
		int cur_len = WL_SCAN_RESULTS_V109_FIXED_SIZE;
		wl_bss_info_v109_t *bss = NULL;
		if (candidate[idx1].RSSI >= BSSRSSI(bi))
			continue;
		for (idx2 = 0; idx2 < list->count; idx2++) {
			bss = bss ? (wl_bss_info_v109_t *)((uintptr)bss + dtoh32(bss->length)) :
				list->bss_info;
			if (!bss) {
				continue;
			}
			if (!bcmp(&candidate[idx1].BSSID, &bss->BSSID, ETHER_ADDR_LEN) &&
				candidate[idx1].RSSI == BSSRSSI(bss) &&
				candidate[idx1].length == dtoh32(bss->length)) {
				u32 delete_len = dtoh32(bss->length);
				WL_DBG(("delete scan info of " MACDBG " to add new AP\n",
					MAC2STRDBG(bss->BSSID.octet)));
				if (idx2 < list->count -1) {
					memmove((u8 *)bss, (u8 *)bss + delete_len,
						list->buflen - cur_len - delete_len);
				}
				list->buflen -= delete_len;
				list->count--;
				total_delete_len += delete_len;
				/* if delete_len is greater than or equal to result length */
				if (total_delete_len >= bi->length) {
					return;
				}
				break;
			}
			cur_len += dtoh32(bss->length);
		}
	}
}
#endif /* WL_DRV_AVOID_SCANCACHE */
#endif /* ESCAN_BUF_OVERFLOW_MGMT */

s32
wl_escan_handler(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	s32 err = BCME_OK;
	s32 status = ntoh32(e->status);
	wl_escan_result_v109_t *escan_result;
	struct net_device *ndev = NULL;
#ifndef WL_DRV_AVOID_SCANCACHE
	wl_bss_info_v109_t *bi;
	u32 bi_length;
	const wifi_p2p_ie_t * p2p_ie;
	const u8 *p2p_dev_addr = NULL;
	wl_scan_results_v109_t *list;
	wl_bss_info_v109_t *bss = NULL;
	u32 i;
#endif /* WL_DRV_AVOID_SCANCACHE */
	u16 channel;
	struct ieee80211_supported_band *band;

	WL_DBG((" enter event type : %d, status : %d \n",
		ntoh32(e->event_type), ntoh32(e->status)));

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	mutex_lock(&cfg->scan_sync);

	if (cfg->loc.in_progress) {
		/* Listen in progress */
		if ((status == WLC_E_STATUS_SUCCESS) || (status == WLC_E_STATUS_ABORT)) {
			if (delayed_work_pending(&cfg->loc.work)) {
				cancel_delayed_work_sync(&cfg->loc.work);
			}
			err = wl_cfgscan_notify_listen_complete(cfg);
			goto exit;
		} else {
			WL_DBG(("Listen in progress. Unknown status. %d\n", status));
		}
	}

	/* P2P SCAN is coming from primary interface */
	if (wl_get_p2p_status(cfg, SCANNING)) {
		if (wl_get_drv_status_all(cfg, SENDING_ACT_FRM))
			ndev = cfg->afx_hdl->dev;
		else
			ndev = cfg->escan_info.ndev;
	}
	escan_result = (wl_escan_result_v109_t *)data;
	if (!escan_result) {
		WL_ERR(("Invalid escan result (NULL data)\n"));
		goto exit;
	}
#ifdef WL_BCNRECV
	if (status == WLC_E_STATUS_RXBCN) {
		if (cfg->bcnrecv_info.bcnrecv_state == BEACON_RECV_STARTED) {
			/* handle beacon recv scan results */
			wl_bss_info_v109_2_t *bi_info;
			bi_info = (wl_bss_info_v109_2_t *)escan_result->bss_info;
			err = wl_bcnrecv_result_handler(cfg, cfgdev, bi_info, status);
		} else {
			WL_ERR(("ignore bcnrx event in disabled state(%d)\n",
				cfg->bcnrecv_info.bcnrecv_state));
		}
		goto exit;
	}
#endif /* WL_BCNRECV */
	if (!ndev || (!wl_get_drv_status(cfg, SCANNING, ndev) && !cfg->sched_scan_running)) {
		WL_ERR_RLMT(("escan is not ready. drv_scan_status 0x%x"
			" e_type %d e_status %d\n",
			wl_get_drv_status(cfg, SCANNING, ndev),
			ntoh32(e->event_type), ntoh32(e->status)));
		goto exit;
	}

#ifndef WL_DRV_AVOID_SCANCACHE
	if (wl_escan_check_sync_id(cfg, status, escan_result->sync_id,
			cfg->escan_info.cur_sync_id) < 0) {
			goto exit;
	}

	if (status == WLC_E_STATUS_PARTIAL) {
		WL_DBG(("WLC_E_STATUS_PARTIAL \n"));
		DBG_EVENT_LOG((dhd_pub_t *)cfg->pub, WIFI_EVENT_DRIVER_SCAN_RESULT_FOUND);
		if ((dtoh32(escan_result->buflen) > (int)ESCAN_BUF_SIZE) ||
		    (dtoh32(escan_result->buflen) < sizeof(wl_escan_result_v109_t))) {
			WL_ERR(("Invalid escan buffer len:%d\n", dtoh32(escan_result->buflen)));
			goto exit;
		}
		if (dtoh16(escan_result->bss_count) != 1) {
			WL_ERR(("Invalid bss_count %d: ignoring\n", escan_result->bss_count));
			goto exit;
		}
		bi = escan_result->bss_info;
		if (!bi) {
			WL_ERR(("Invalid escan bss info (NULL pointer)\n"));
			goto exit;
		}
		bi_length = dtoh32(bi->length);
		if (bi_length != (dtoh32(escan_result->buflen)
		    - WL_ESCAN_RESULTS_V109_FIXED_SIZE)) {
			WL_ERR(("Invalid bss_info length %d: ignoring\n", bi_length));
			goto exit;
		}

		/* +++++ terence 20130524: skip invalid bss */
		channel =
			bi->ctl_ch ? bi->ctl_ch : CHSPEC_CHANNEL(wl_chspec_driver_to_host(bi->chanspec));
		if (channel <= CH_MAX_2G_CHANNEL)
			band = bcmcfg_to_wiphy(cfg)->bands[IEEE80211_BAND_2GHZ];
		else
			band = bcmcfg_to_wiphy(cfg)->bands[IEEE80211_BAND_5GHZ];
		if (!band) {
			WL_ERR(("No valid band\n"));
			goto exit;
		}
		if (!dhd_conf_match_channel(cfg->pub, channel))
			goto exit;
		/* ----- terence 20130524: skip invalid bss */

		if (!(bcmcfg_to_wiphy(cfg)->interface_modes & BIT(NL80211_IFTYPE_ADHOC))) {
			if (dtoh16(bi->capability) & DOT11_CAP_IBSS) {
				WL_DBG(("Ignoring IBSS result\n"));
				goto exit;
			}
		}

		if (wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL)) {
			p2p_dev_addr = wl_cfgp2p_retreive_p2p_dev_addr(bi, bi_length);
			if (p2p_dev_addr && !memcmp(p2p_dev_addr,
				cfg->afx_hdl->tx_dst_addr.octet, ETHER_ADDR_LEN)) {
				s32 channel = wf_chspec_ctlchan(
					wl_chspec_driver_to_host(bi->chanspec));

				if ((channel > MAXCHANNEL) || (channel <= 0))
					channel = WL_INVALID;
				else
					WL_ERR(("ACTION FRAME SCAN : Peer " MACDBG " found,"
						" channel : %d\n",
						MAC2STRDBG(cfg->afx_hdl->tx_dst_addr.octet),
						channel));

				wl_clr_p2p_status(cfg, SCANNING);
				cfg->afx_hdl->peer_chan = channel;
				complete(&cfg->act_frm_scan);
				goto exit;
			}

		} else {
			int cur_len = WL_SCAN_RESULTS_V109_FIXED_SIZE;
#ifdef ESCAN_BUF_OVERFLOW_MGMT
			removal_element_t candidate[BUF_OVERFLOW_MGMT_COUNT];
			int remove_lower_rssi = FALSE;

			bzero(candidate, sizeof(removal_element_t)*BUF_OVERFLOW_MGMT_COUNT);
#endif /* ESCAN_BUF_OVERFLOW_MGMT */

			list = wl_escan_get_buf(cfg, FALSE);
			if (scan_req_match(cfg)) {
#ifdef WL_HOST_BAND_MGMT
				s32 channel_band = 0;
				chanspec_t chspec;
#endif /* WL_HOST_BAND_MGMT */
				/* p2p scan && allow only probe response */
				if ((cfg->p2p->search_state != WL_P2P_DISC_ST_SCAN) &&
					(bi->flags & WL_BSS_FLAGS_FROM_BEACON))
					goto exit;
				if ((p2p_ie = wl_cfgp2p_find_p2pie(((u8 *) bi) + bi->ie_offset,
					bi->ie_length)) == NULL) {
						WL_ERR(("Couldn't find P2PIE in probe"
							" response/beacon\n"));
						goto exit;
				}
#ifdef WL_HOST_BAND_MGMT
				chspec = wl_chspec_driver_to_host(bi->chanspec);
				channel_band = CHSPEC2WLC_BAND(chspec);

				if ((
#ifdef WL_6G_BAND
					(cfg->curr_band == WLC_BAND_6G) ||
#endif /* WL_6G_BAND */
					(cfg->curr_band == WLC_BAND_5G)) &&
					(channel_band == WLC_BAND_2G)) {
					/* Avoid sending the GO results in band conflict */
					if (wl_cfgp2p_retreive_p2pattrib(p2p_ie,
						P2P_SEID_GROUP_ID) != NULL)
						goto exit;
				}
#endif /* WL_HOST_BAND_MGMT */
			}
#ifdef ESCAN_BUF_OVERFLOW_MGMT
			if (bi_length > ESCAN_BUF_SIZE - list->buflen)
				remove_lower_rssi = TRUE;
#endif /* ESCAN_BUF_OVERFLOW_MGMT */

			for (i = 0; i < list->count; i++) {
				bss = bss ? (wl_bss_info_v109_t *)
				((uintptr)bss + dtoh32(bss->length)): list->bss_info;
				if (!bss) {
					WL_ERR(("bss is NULL\n"));
					goto exit;
				}
#ifdef ESCAN_BUF_OVERFLOW_MGMT
				WL_DBG(("%s("MACDBG"), i=%d bss: RSSI %d list->count %d\n",
					bss->SSID, MAC2STRDBG(bss->BSSID.octet),
					i, bss->RSSI, list->count));

				if (remove_lower_rssi)
					wl_cfg80211_find_removal_candidate(bss, candidate);
#endif /* ESCAN_BUF_OVERFLOW_MGMT */

				if (!bcmp(&bi->BSSID, &bss->BSSID, ETHER_ADDR_LEN) &&
					(CHSPEC_BAND(wl_chspec_driver_to_host(bi->chanspec))
					== CHSPEC_BAND(wl_chspec_driver_to_host(bss->chanspec))) &&
					bi->SSID_len == bss->SSID_len &&
					!bcmp(bi->SSID, bss->SSID, bi->SSID_len)) {

					/* do not allow beacon data to update
					*the data recd from a probe response
					*/
					if (!(bss->flags & WL_BSS_FLAGS_FROM_BEACON) &&
						(bi->flags & WL_BSS_FLAGS_FROM_BEACON))
						goto exit;

					WL_DBG(("%s("MACDBG"), i=%d prev: RSSI %d"
						" flags 0x%x, new: RSSI %d flags 0x%x\n",
						bss->SSID, MAC2STRDBG(bi->BSSID.octet), i,
						bss->RSSI, bss->flags, bi->RSSI, bi->flags));

					if ((bss->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) ==
						(bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL)) {
						/* preserve max RSSI if the measurements are
						* both on-channel or both off-channel
						*/
						WL_DBG(("%s("MACDBG"), same onchan"
						", RSSI: prev %d new %d\n",
						bss->SSID, MAC2STRDBG(bi->BSSID.octet),
						bss->RSSI, bi->RSSI));
						bi->RSSI = MAX(bss->RSSI, bi->RSSI);
					} else if ((bss->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) &&
						(bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) == 0) {
						/* preserve the on-channel rssi measurement
						* if the new measurement is off channel
						*/
						WL_DBG(("%s("MACDBG"), prev onchan"
						", RSSI: prev %d new %d\n",
						bss->SSID, MAC2STRDBG(bi->BSSID.octet),
						bss->RSSI, bi->RSSI));
						bi->RSSI = bss->RSSI;
						bi->flags |= WL_BSS_FLAGS_RSSI_ONCHANNEL;
					}
					if (dtoh32(bss->length) != bi_length) {
						u32 prev_len = dtoh32(bss->length);

						WL_DBG(("bss info replacement"
							" is occured(bcast:%d->probresp%d)\n",
							bss->ie_length, bi->ie_length));
						WL_DBG(("%s("MACDBG"), replacement!(%d -> %d)\n",
						bss->SSID, MAC2STRDBG(bi->BSSID.octet),
						prev_len, bi_length));

						if ((list->buflen - prev_len) + bi_length
							> ESCAN_BUF_SIZE) {
							WL_ERR(("Buffer is too small: keep the"
								" previous result of this AP\n"));
							/* Only update RSSI */
							bss->RSSI = bi->RSSI;
							bss->flags |= (bi->flags
								& WL_BSS_FLAGS_RSSI_ONCHANNEL);
							goto exit;
						}

						if (i < list->count - 1) {
							/* memory copy required by this case only */
							memmove((u8 *)bss + bi_length,
								(u8 *)bss + prev_len,
								list->buflen - cur_len - prev_len);
						}
						list->buflen -= prev_len;
						list->buflen += bi_length;
					}
					list->version = dtoh32(bi->version);
					/* In the above code under check
					*  '(dtoh32(bss->length) != bi_length)'
					* buffer overflow is avoided. bi_length
					* is already accounted in list->buflen
					*/
					if ((err = memcpy_s((u8 *)bss,
						(ESCAN_BUF_SIZE - (list->buflen - bi_length)),
						(u8 *)bi, bi_length)) != BCME_OK) {
						WL_ERR(("Failed to copy the recent bss_info."
							"err:%d recv_len:%d bi_len:%d\n", err,
							ESCAN_BUF_SIZE - (list->buflen - bi_length),
							bi_length));
						/* This scenario should never happen. If it happens,
						 * set list->count to zero for recovery
						 */
						list->count = 0;
						list->buflen = 0;
						ASSERT(0);
					}
					goto exit;
				}
				cur_len += dtoh32(bss->length);
			}
			if (bi_length > ESCAN_BUF_SIZE - list->buflen) {
#ifdef ESCAN_BUF_OVERFLOW_MGMT
				wl_cfg80211_remove_lowRSSI_info(list, candidate, bi);
				if (bi_length > ESCAN_BUF_SIZE - list->buflen) {
					WL_DBG(("RSSI(" MACDBG ") is too low(%d) to add Buffer\n",
						MAC2STRDBG(bi->BSSID.octet), bi->RSSI));
					goto exit;
				}
#else
				WL_ERR(("Buffer is too small: ignoring\n"));
				goto exit;
#endif /* ESCAN_BUF_OVERFLOW_MGMT */
			}
			/* In the previous step check is added to ensure the bi_legth does not
			* exceed the ESCAN_BUF_SIZE
			*/
			(void)memcpy_s(&(((char *)list)[list->buflen]),
				(ESCAN_BUF_SIZE - list->buflen), bi, bi_length);
			list->version = dtoh32(bi->version);
			list->buflen += bi_length;
			list->count++;

			/*
			 * !Broadcast && number of ssid = 1 && number of channels =1
			 * means specific scan to association
			 */
			if (wl_cfgp2p_is_p2p_specific_scan(cfg->scan_request)) {
				WL_ERR(("P2P assoc scan fast aborted.\n"));
				wl_cfgscan_scan_abort(cfg);
				wl_notify_escan_complete(cfg, cfg->escan_info.ndev, false);
				goto exit;
			}
		}
	}
	else if (status == WLC_E_STATUS_SUCCESS) {
		dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);
		if ((dhdp->dhd_induce_error == DHD_INDUCE_SCAN_TIMEOUT) ||
			(dhdp->dhd_induce_error == DHD_INDUCE_SCAN_TIMEOUT_SCHED_ERROR)) {
			WL_ERR(("%s: Skip escan complete to induce scantimeout\n", __FUNCTION__));
			err = BCME_ERROR;
			goto exit;
		}
		cfg->escan_info.escan_state = WL_ESCAN_STATE_IDLE;
#ifdef DHD_SEND_HANG_ESCAN_SYNCID_MISMATCH
		cfg->escan_info.prev_escan_aborted = FALSE;
#endif /* DHD_SEND_HANG_ESCAN_SYNCID_MISMATCH */
		if (wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL)) {
			WL_DBG(("ACTION FRAME SCAN DONE\n"));
			wl_clr_p2p_status(cfg, SCANNING);
			wl_clr_drv_status(cfg, SCANNING, cfg->afx_hdl->dev);
			if (cfg->afx_hdl->peer_chan == WL_INVALID)
				complete(&cfg->act_frm_scan);
		} else if ((likely(cfg->scan_request)) || (cfg->sched_scan_running)) {
			WL_INFORM_MEM(("ESCAN COMPLETED\n"));
			DBG_EVENT_LOG((dhd_pub_t *)cfg->pub, WIFI_EVENT_DRIVER_SCAN_COMPLETE);
			cfg->bss_list = wl_escan_get_buf(cfg, FALSE);
			if (!scan_req_match(cfg)) {
				WL_DBG(("SCAN COMPLETED: scanned AP count=%d\n",
					cfg->bss_list->count));
			}
			wl_inform_bss(cfg);
			wl_notify_escan_complete(cfg, ndev, false);
		}
		wl_escan_increment_sync_id(cfg, SCAN_BUF_NEXT);
	} else if ((status == WLC_E_STATUS_ABORT) || (status == WLC_E_STATUS_NEWSCAN) ||
		(status == WLC_E_STATUS_11HQUIET) || (status == WLC_E_STATUS_CS_ABORT) ||
		(status == WLC_E_STATUS_NEWASSOC)) {
		/* Dump FW preserve buffer content */
		if (status == WLC_E_STATUS_ABORT) {
			wl_flush_fw_log_buffer(ndev, FW_LOGSET_MASK_ALL);
		}
		/* Handle all cases of scan abort */
		cfg->escan_info.escan_state = WL_ESCAN_STATE_IDLE;
		WL_DBG(("ESCAN ABORT reason: %d\n", status));
		if (wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL)) {
			WL_DBG(("ACTION FRAME SCAN DONE\n"));
			wl_clr_drv_status(cfg, SCANNING, cfg->afx_hdl->dev);
			wl_clr_p2p_status(cfg, SCANNING);
			if (cfg->afx_hdl->peer_chan == WL_INVALID)
				complete(&cfg->act_frm_scan);
		} else if ((likely(cfg->scan_request)) || (cfg->sched_scan_running)) {
			WL_INFORM_MEM(("ESCAN ABORTED - reason:%d\n", status));

			if (cfg->escan_info.ndev != ndev) {
				/* Ignore events coming for older scan reqs */
				WL_INFORM_MEM(("abort event doesn't match on going scan req\n"));
				err = BCME_ERROR;
				goto exit;
			}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0))
			if (p2p_scan(cfg) && cfg->scan_request &&
				(cfg->scan_request->flags & NL80211_SCAN_FLAG_FLUSH)) {
				WL_ERR(("scan list is changed"));
				cfg->bss_list = wl_escan_get_buf(cfg, FALSE);
			} else
#endif
				cfg->bss_list = wl_escan_get_buf(cfg, TRUE);

			if (!scan_req_match(cfg)) {
				WL_TRACE_HW4(("SCAN ABORTED: scanned AP count=%d\n",
					cfg->bss_list->count));
			}
#ifdef DUAL_ESCAN_RESULT_BUFFER
			if (escan_result->sync_id != cfg->escan_info.cur_sync_id) {
				/* If sync_id is not matching, then the abort might have
				 * come for the old scan req or for the in-driver initiated
				 * scan. So do abort for scan_req for which sync_id is
				 * matching.
				 */
				WL_INFORM_MEM(("sync_id mismatch (%d != %d). "
					"Ignore the scan abort event.\n",
					escan_result->sync_id, cfg->escan_info.cur_sync_id));
				goto exit;
			} else {
				/* sync id is matching, abort the scan */
				WL_INFORM_MEM(("scan aborted for sync_id: %d \n",
					cfg->escan_info.cur_sync_id));
				wl_inform_bss(cfg);
				wl_notify_escan_complete(cfg, ndev, true);
			}
#else
			wl_inform_bss(cfg);
			wl_notify_escan_complete(cfg, ndev, true);
#endif /* DUAL_ESCAN_RESULT_BUFFER */
		} else {
			/* If there is no pending host initiated scan, do nothing */
			WL_DBG(("ESCAN ABORT: No pending scans. Ignoring event.\n"));
		}
		/* scan aborted, need to set previous success result */
		wl_escan_increment_sync_id(cfg, SCAN_BUF_CNT);
	} else if (status == WLC_E_STATUS_TIMEOUT) {
		WL_ERR(("WLC_E_STATUS_TIMEOUT : scan_request[%p]\n", cfg->scan_request));
		WL_ERR(("reason[0x%x]\n", e->reason));
		if (e->reason == 0xFFFFFFFF) {
			wl_scan_results_v109_t *bss_list;
			bss_list = wl_escan_get_buf(cfg, FALSE);
			if (!bss_list) {
				WL_ERR(("bss_list is null. Didn't receive any partial scan results\n"));
			} else {
				WL_ERR(("Dump scan buffer: scanned AP count (%d)\n", bss_list->count));
				bi = NULL;
				bi = next_bss(bss_list, bi);
				for_each_bss(bss_list, bi, i) {
					channel = wf_chspec_ctlchan(wl_chspec_driver_to_host(bi->chanspec));
					WL_ERR(("SSID :%s  Channel :%d\n", bi->SSID, channel));
				}
			}
			_wl_cfgscan_cancel_scan(cfg);
		}
	} else {
		WL_ERR(("unexpected Escan Event %d : abort\n", status));
		cfg->escan_info.escan_state = WL_ESCAN_STATE_IDLE;
		if (wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL)) {
			WL_DBG(("ACTION FRAME SCAN DONE\n"));
			wl_clr_p2p_status(cfg, SCANNING);
			wl_clr_drv_status(cfg, SCANNING, cfg->afx_hdl->dev);
			if (cfg->afx_hdl->peer_chan == WL_INVALID)
				complete(&cfg->act_frm_scan);
		} else if ((likely(cfg->scan_request)) || (cfg->sched_scan_running)) {
			cfg->bss_list = wl_escan_get_buf(cfg, TRUE);
			if (!scan_req_match(cfg)) {
				WL_TRACE_HW4(("SCAN ABORTED(UNEXPECTED): "
					"scanned AP count=%d\n",
					cfg->bss_list->count));
			}
			wl_inform_bss(cfg);
			wl_notify_escan_complete(cfg, ndev, true);
		}
		/* scan aborted, need to set previous success result */
		wl_escan_increment_sync_id(cfg, 2);
	}
#else /* WL_DRV_AVOID_SCANCACHE */
	err = wl_escan_without_scan_cache(cfg, escan_result, ndev, e, status);
#endif /* WL_DRV_AVOID_SCANCACHE */
exit:
	mutex_unlock(&cfg->scan_sync);
	return err;
}

#ifdef WL_SCHED_SCAN
s32 wl_cfgscan_pfn_handler(struct bcm_cfg80211 *cfg, wl_pfn_scanresult_v3_1_t *pfn_scanresult)
{
	s32 err = BCME_OK;
	wl_bss_info_v109_t *bi = NULL;

	bi = (wl_bss_info_v109_t *)pfn_scanresult->bss_info;
	if (!bi) {
		WL_ERR(("Invalid pfn bss info (NULL pointer)"
			"or invalid bss_info length\n"));
		goto exit;
	}
	preempt_disable();
#ifdef ESCAN_CHANNEL_CACHE
	add_roam_cache(cfg, bi);
#endif /* ESCAN_CHANNEL_CACHE */
	err = wl_inform_single_bss(cfg, bi, false);
	if (unlikely(err)) {
		WL_ERR(("bss inform failed\n"));
	}
	preempt_enable();
	WL_MEM(("cfg80211 scan cache updated\n"));
exit:
	return err;
}

s32
wl_cfgscan_pfn_scanresult_handler(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	s32 err = BCME_OK;
	wl_pfn_scanresult_v3_1_t *pfn_scanresult;

	WL_DBG_MEM(("event type : %d, status : %d \n",
		ntoh32(e->event_type), ntoh32(e->status)));

	mutex_lock(&cfg->scan_sync);

	pfn_scanresult = (wl_pfn_scanresult_v3_1_t *)data;
	if (!pfn_scanresult) {
		WL_ERR(("Invalid pfn scan result (NULL data)\n"));
		goto exit;
	}

	if (cfg->sched_scan_req) {
		err = wl_cfgscan_pfn_handler(cfg, pfn_scanresult);
	}
exit:
	mutex_unlock(&cfg->scan_sync);
	return err;
}
#endif /* WL_SCHED_SCAN */

#if defined(GSCAN_SUPPORT) || defined(WL_SCHED_SCAN)
/* sched scan done and stop pno upon receiving the pfn scan complete event */
s32
wl_cfgscan_notify_pfn_complete(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
        const wl_event_msg_t *e, void *data)
{
	s32 err = BCME_OK;
	struct net_device *dev;
	s32 status = ntoh32(e->status);
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);
	struct wiphy *wiphy = NULL;

	dev = cfgdev_to_wlc_ndev(cfgdev, cfg);
	WL_DBG_MEM(("Enter \n"));
	BCM_REFERENCE(dhdp);

#ifdef GSCAN_SUPPORT
	err = wl_notify_gscan_event(cfg, cfgdev, e, data);
	return err;
#endif
	mutex_lock(&cfg->scan_sync);

	if (!cfg->sched_scan_req) {
		WL_INFORM_MEM(("No sched scan req is in progress.\n"));
		goto exit;
	}

	wiphy = cfg->sched_scan_req->wiphy;
	/* If new sched scan triggered, wiphy set as NULL.
	* In this case, drop this event to avoid kernel state schew up
	*/
	if (!wiphy) {
		WL_INFORM_MEM(("wiphy of sched_scan_req is NULL.\n"));
		goto exit;
	}

	if (status == WLC_E_STATUS_SUCCESS) {
		WL_INFORM_MEM(("[%s] Report sched scan done.\n", dev->name));
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
		cfg80211_sched_scan_results(wiphy,
				cfg->sched_scan_req->reqid);
#else
		cfg80211_sched_scan_results(wiphy);
#endif /* LINUX_VER > 4.11 */
	} else if (status != WLC_E_STATUS_SUCCESS) {
		WL_INFORM_MEM(("bss list empty. report sched_scan_stop\n"));
		wl_cfg80211_stop_pno(cfg,  bcmcfg_to_prmry_ndev(cfg));
#ifdef WL_SCHED_SCAN
		/* schedule the work to indicate sched scan stop to cfg layer */
		schedule_delayed_work(&cfg->sched_scan_stop_work, 0);
#endif /* WL_SCHED_SCAN */
	}
exit:
	mutex_unlock(&cfg->scan_sync);
	return err;
}
#endif /* GSCAN_SUPPORT || WL_SCHED_SCAN */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) && \
	defined(SUPPORT_RANDOM_MAC_SCAN)
static const u8 *
wl_retrieve_wps_attribute(const u8 *buf, u16 element_id)
{
	const wl_wps_ie_t *ie = NULL;
	u16 len = 0;
	const u8 *attrib;

	if (!buf) {
		WL_ERR(("WPS IE not present"));
		return 0;
	}

	ie = (const wl_wps_ie_t*) buf;
	len = ie->len;

	/* Point subel to the P2P IE's subelt field.
	 * Subtract the preceding fields (id, len, OUI, oui_type) from the length.
	 */
	attrib = ie->attrib;
	len -= 4;	/* exclude OUI + OUI_TYPE */

	/* Search for attrib */
	return wl_find_attribute(attrib, len, element_id);
}

bool
wl_is_wps_enrollee_active(struct net_device *ndev, const u8 *ie_ptr, u16 len)
{
	const u8 *ie;
	const u8 *attrib;

	if ((ie = (const u8 *)wl_cfgp2p_find_wpsie(ie_ptr, len)) == NULL) {
		WL_DBG(("WPS IE not present. Do nothing.\n"));
		return false;
	}

	if ((attrib = wl_retrieve_wps_attribute(ie, WPS_ATTR_REQ_TYPE)) == NULL) {
		WL_DBG(("WPS_ATTR_REQ_TYPE not found!\n"));
		return false;
	}

	if (*attrib == WPS_REQ_TYPE_ENROLLEE) {
		WL_INFORM_MEM(("WPS Enrolle Active\n"));
		return true;
	} else {
		WL_DBG(("WPS_REQ_TYPE:%d\n", *attrib));
	}

	return false;
}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0) && defined(SUPPORT_RANDOM_MAC_SCAN) */

/* Find listen channel */
static s32 wl_find_listen_channel(struct bcm_cfg80211 *cfg,
	const u8 *ie, u32 ie_len)
{
	const wifi_p2p_ie_t *p2p_ie;
	const u8 *end, *pos;
	s32 listen_channel;

	pos = (const u8 *)ie;

	p2p_ie = wl_cfgp2p_find_p2pie(pos, ie_len);

	if (p2p_ie == NULL) {
		return 0;
	}

	if (p2p_ie->len < MIN_P2P_IE_LEN || p2p_ie->len > MAX_P2P_IE_LEN) {
		CFGP2P_ERR(("p2p_ie->len out of range - %d\n", p2p_ie->len));
		return 0;
	}
	pos = p2p_ie->subelts;
	end = p2p_ie->subelts + (p2p_ie->len - 4);

	CFGP2P_DBG((" found p2p ie ! lenth %d \n",
		p2p_ie->len));

	while (pos < end) {
		uint16 attr_len;
		if (pos + 2 >= end) {
			CFGP2P_DBG((" -- Invalid P2P attribute"));
			return 0;
		}
		attr_len = ((uint16) (((pos + 1)[1] << 8) | (pos + 1)[0]));

		if (pos + 3 + attr_len > end) {
			CFGP2P_DBG(("P2P: Attribute underflow "
				   "(len=%u left=%d)",
				   attr_len, (int) (end - pos - 3)));
			return 0;
		}

		/* if Listen Channel att id is 6 and the vailue is valid,
		 * return the listen channel
		 */
		if (pos[0] == 6) {
			/* listen channel subel length format
			 * 1(id) + 2(len) + 3(country) + 1(op. class) + 1(chan num)
			 */
			listen_channel = pos[1 + 2 + 3 + 1];

			if (listen_channel == SOCIAL_CHAN_1 ||
				listen_channel == SOCIAL_CHAN_2 ||
				listen_channel == SOCIAL_CHAN_3) {
				CFGP2P_DBG((" Found my Listen Channel %d \n", listen_channel));
				return listen_channel;
			}
		}
		pos += 3 + attr_len;
	}
	return 0;
}

#ifdef WL_SCAN_TYPE
static u32
wl_cfgscan_map_nl80211_scan_type(struct bcm_cfg80211 *cfg, struct cfg80211_scan_request *request)
{
	u32 scan_flags = 0;

	if (!request) {
		return scan_flags;
	}

	if (cfg->latency_mode &&
		wl_is_sta_connected(cfg)) {
		WL_DBG_MEM(("latency mode on. force LP scan\n"));
		scan_flags |= WL_SCANFLAGS_LOW_POWER_SCAN;
		goto exit;
	}

	if (request->flags & NL80211_SCAN_FLAG_LOW_SPAN) {
		scan_flags |= WL_SCANFLAGS_LOW_SPAN;
	}
	if (request->flags & NL80211_SCAN_FLAG_HIGH_ACCURACY) {
		scan_flags |= WL_SCANFLAGS_HIGH_ACCURACY;
	}
	if (request->flags & NL80211_SCAN_FLAG_LOW_POWER) {
		scan_flags |= WL_SCANFLAGS_LOW_POWER_SCAN;
	}
	if (request->flags & NL80211_SCAN_FLAG_LOW_PRIORITY) {
		scan_flags |= WL_SCANFLAGS_LOW_PRIO;
	}

exit:
	WL_INFORM(("scan flags. wl:%x cfg80211:%x\n", scan_flags, request->flags));
	return scan_flags;
}
#endif /* WL_SCAN_TYPE */

chanspec_t wl_freq_to_chanspec(int freq)
{
	chanspec_t chanspec = 0;
	u16 bw;

	/* see 802.11 17.3.8.3.2 and Annex J */
	if (freq == 2484) {
		chanspec = 14;
		chanspec |= WL_CHANSPEC_BAND_2G;
		bw = WL_CHANSPEC_BW_20;
	} else if (freq >= 2412 && freq < 2484) {
		chanspec = (freq - 2407) / 5;
		chanspec |= WL_CHANSPEC_BAND_2G;
		bw = WL_CHANSPEC_BW_20;
	} else if (freq >= 4005 && freq <= 4980) {
		chanspec = (freq - 4000) / 5;
		chanspec |= WL_CHANSPEC_BAND_5G;
		bw = WL_CHANSPEC_BW_20;
	} else if (freq >= 5005 && freq < 5895) {
		chanspec = (freq - 5000) / 5;
		chanspec |= WL_CHANSPEC_BAND_5G;
		bw = WL_CHANSPEC_BW_20;
#ifdef WL_6G_BAND
	} else if (freq >= 5945 && freq <= 7200) {
		/* see 802.11ax D4.1 27.3.22.2 */
		chanspec = (freq - 5950) / 5;
		bw = WL_CHANSPEC_BW_20;
		if ((chanspec % 8) == 3) {
			bw = WL_CHANSPEC_BW_40;
		} else if ((chanspec % 16) == 7) {
			bw = WL_CHANSPEC_BW_80;
		} else if ((chanspec % 32) == 15) {
			bw = WL_CHANSPEC_BW_160;
		}
		chanspec |= WL_CHANSPEC_BAND_6G;
	} else if (freq == 5935) {
		chanspec = 2;
		bw = WL_CHANSPEC_BW_20;
		chanspec |= WL_CHANSPEC_BAND_6G;
#endif /* WL_6G_BAND */
	} else {
		WL_ERR(("Invalid frequency %d\n", freq));
		return INVCHANSPEC;
	}

	/* Get the min_bw set for the interface */
	chanspec |= bw;
	chanspec |= WL_CHANSPEC_CTL_SB_NONE;

	return chanspec;
}

#ifdef SCAN_SUPPRESS
static void
wl_cfgscan_populate_scan_channel(struct bcm_cfg80211 *cfg,
	struct ieee80211_channel **channels, u32 n_channels,
	u16 *channel_list, struct wl_chan_info *chan_info)
{
	u32 i, chanspec = 0;

	for (i=0; i<n_channels; i++) {
		chanspec = wl_freq_to_chanspec(channels[i]->center_freq);
		if (chanspec == INVCHANSPEC) {
			WL_ERR(("Invalid chanspec! Skipping channel\n"));
			continue;
		}
		if (chan_info->band == CHSPEC2WLC_BAND(chanspec) &&
				chan_info->chan == wf_chspec_ctlchan(chanspec)) {
			channel_list[0] = chanspec;
			break;
		}
	}
	WL_SCAN(("chan: %s-%d, chanspec: %x\n",
		WLCBAND2STR(chan_info->band), chan_info->chan, chanspec));
}
#endif

static void
wl_cfgscan_populate_scan_channels(struct bcm_cfg80211 *cfg,
	struct ieee80211_channel **channels, u32 n_channels,
	u16 *channel_list, u32 *num_channels, bool use_chanspecs, bool skip_dfs)
{
	u32 i = 0, j = 0;
	u32 chanspec = 0;
	bool is_p2p_scan = false;
#ifdef P2P_SKIP_DFS
	int is_printed = false;
#endif /* P2P_SKIP_DFS */
	u32 channel;

	if (!channels || !n_channels) {
		/* Do full channel scan */
		return;
	}

	/* Check if request is for p2p scans */
	is_p2p_scan = p2p_is_on(cfg) && p2p_scan(cfg);

	for (i = 0; i < n_channels; i++) {
		channel = ieee80211_frequency_to_channel(channels[i]->center_freq);
		if (skip_dfs && (IS_RADAR_CHAN(channels[i]->flags))) {
			WL_DBG(("Skipping radar channel. freq:%d\n",
				(channels[i]->center_freq)));
			continue;
		}
		if (!dhd_conf_match_channel(cfg->pub, channel))
			continue;

		chanspec = wl_freq_to_chanspec(channels[i]->center_freq);
		if (chanspec == INVCHANSPEC) {
			WL_ERR(("Invalid chanspec! Skipping channel\n"));
			continue;
		}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
		if (channels[i]->band == IEEE80211_BAND_60GHZ) {
			/* Not supported */
			continue;
		}
#endif /* LINUX_VER >= 3.6 */
#ifdef WL_HOST_BAND_MGMT
		if (channels[i]->band == IEEE80211_BAND_2GHZ) {
			if ((cfg->curr_band == WLC_BAND_5G) ||
				(cfg->curr_band == WLC_BAND_6G)) {
				if !(is_p2p_scan &&
					IS_P2P_SOCIAL_CHANNEL(CHSPEC_CHANNEL(chanspec))) {
					WL_DBG(("In 5G only mode, omit 2G channel:%d\n", channel));
					continue;
				}
			}
		} else {
			if (cfg->curr_band == WLC_BAND_2G) {
				WL_DBG(("In 2G only mode, omit 5G channel:%d\n", channel));
				continue;
			}
		}
#endif /* WL_HOST_BAND_MGMT */

		if (is_p2p_scan) {
#ifdef WL_P2P_6G
			if (!(cfg->p2p_6g_enabled)) {
#endif /* WL_P2P_6G */
				if (CHSPEC_IS6G(chanspec)) {
					continue;
				}
#ifdef WL_P2P_6G
			}
#endif /* WL_P2P_6G */

#ifdef P2P_SKIP_DFS
			if (CHSPEC_IS5G(chanspec) &&
				(CHSPEC_CHANNEL(chanspec) >= 52 &&
				CHSPEC_CHANNEL(chanspec) <= 144)) {
				if (is_printed == false) {
					WL_ERR(("SKIP DFS CHANs(52~144)\n"));
					is_printed = true;
				}
				continue;
			}
#endif /* P2P_SKIP_DFS */
#ifdef WL_UNII4_CHAN
			/* Skip UNII-4 frequencies */
			if (CHSPEC_IS5G(chanspec) &&
				IS_UNII4_CHANNEL(wf_chspec_primary20_chan(chanspec))) {
				WL_DBG(("Skip UNII-4 chanspec 0x%x\n", chanspec));
				continue;
			}
#endif /* WL_UNII4_CHAN */
		}

		if (use_chanspecs) {
			channel_list[j] = chanspec;
		} else {
			channel_list[j] = CHSPEC_CHANNEL(chanspec);
		}
		WL_SCAN(("chan: %s-%d, chanspec: %x\n",
			CHSPEC2BANDSTR(chanspec), wf_chspec_ctlchan(chanspec), channel_list[j]));
		j++;
		if (j == WL_NUMCHANSPECS) {
			/* max limit */
			break;
		}
	}
	*num_channels = j;
}

static void
wl_cfgscan_populate_scan_ssids(struct bcm_cfg80211 *cfg, u8 *buf_ptr, u32 buf_len,
	struct cfg80211_scan_request *request, u32 *ssid_num)
{
	u32 n_ssids;
	wlc_ssid_t ssid;
	int i, j = 0;

	if (!request || !buf_ptr) {
		/* Do full channel scan */
		return;
	}

	n_ssids = request->n_ssids;
	if (n_ssids > 0) {

		if (buf_len < (n_ssids * sizeof(wlc_ssid_t))) {
			WL_ERR(("buf len not sufficient for scan ssids\n"));
			return;
		}

		for (i = 0; i < n_ssids; i++) {
			bzero(&ssid, sizeof(wlc_ssid_t));
			ssid.SSID_len = MIN(request->ssids[i].ssid_len, DOT11_MAX_SSID_LEN);
			/* Returning void here, as per previous line copy length does not exceed
			* DOT11_MAX_SSID_LEN
			*/
			(void)memcpy_s(ssid.SSID, DOT11_MAX_SSID_LEN, request->ssids[i].ssid,
				ssid.SSID_len);
			if (!ssid.SSID_len) {
				WL_SCAN(("%d: Broadcast scan\n", i));
			} else {
				WL_SCAN(("%d: scan  for  %s size =%d\n", i,
				ssid.SSID, ssid.SSID_len));
			}
			/* For multiple ssid case copy the each SSID info the ptr below corresponds
			* to that so dest is of type wlc_ssid_t
			*/
			(void)memcpy_s(buf_ptr, sizeof(wlc_ssid_t), &ssid, sizeof(wlc_ssid_t));
			buf_ptr += sizeof(wlc_ssid_t);
			j++;
		}
	} else {
		WL_SCAN(("Broadcast scan\n"));
	}
	*ssid_num = j;
}

static s32
wl_scan_prep(struct bcm_cfg80211 *cfg, struct net_device *ndev, void *scan_params, u32 len,
	struct cfg80211_scan_request *request)
{
#ifdef SCAN_SUPPRESS
	struct wl_chan_info chan_info;
	u32 channel;
#endif
	wl_scan_params_v1_t *params = NULL;
	wl_scan_params_v3_t *params_v3 = NULL;
	u32 scan_type = 0;
	u32 scan_param_size = 0;
	u32 n_channels = 0;
	u32 n_ssids = 0;
	uint16 *chan_list = NULL;
	u32 channel_offset = 0;
	u32 cur_offset;

	if (!scan_params) {
		return BCME_ERROR;
	}

	if (cfg->active_scan == PASSIVE_SCAN) {
		WL_INFORM_MEM(("Enforcing passive scan\n"));
		scan_type = WL_SCANFLAGS_PASSIVE;
	}

	WL_DBG(("Preparing Scan request\n"));
	if (IS_SCAN_PARAMS_V3_V2(cfg)) {
		params_v3 = (wl_scan_params_v3_t *)scan_params;
		scan_param_size = sizeof(wl_scan_params_v3_t);
		channel_offset = offsetof(wl_scan_params_v3_t, channel_list);
	} else {
		params = (wl_scan_params_v1_t *)scan_params;
		scan_param_size = sizeof(wl_scan_params_v1_t);
		channel_offset = offsetof(wl_scan_params_v1_t, channel_list);
	}

	if (params_v3) {
		/* scan params ver3 */
#if defined(WL_SCAN_TYPE)
		scan_type  += wl_cfgscan_map_nl80211_scan_type(cfg, request);
#endif /* WL_SCAN_TYPE */
		(void)memcpy_s(&params_v3->bssid, ETHER_ADDR_LEN, &ether_bcast, ETHER_ADDR_LEN);
		params_v3->version = htod16(cfg->scan_params_ver);
		params_v3->length = htod16(sizeof(wl_scan_params_v3_t));
		params_v3->bss_type = DOT11_BSSTYPE_ANY;
		params_v3->scan_type = htod32(scan_type);
		params_v3->nprobes = htod32(-1);
		params_v3->active_time = htod32(-1);
		params_v3->passive_time = htod32(-1);
		params_v3->home_time = htod32(-1);
		params_v3->channel_num = 0;
		bzero(&params_v3->ssid, sizeof(wlc_ssid_t));
		chan_list = params_v3->channel_list;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)) && defined(DHD_SCAN_INC_RNR)
		/* scan for colocated APs reported by 2.4/5 GHz APs */
		if (request->flags & NL80211_SCAN_FLAG_COLOCATED_6GHZ)
#endif
		{
			params_v3->ssid_type |= WL_SCAN_INC_RNR;
		}
	} else {
		/* scan params ver 1 */
		if (!params) {
			ASSERT(0);
			return BCME_ERROR;
		}
		(void)memcpy_s(&params->bssid, ETHER_ADDR_LEN, &ether_bcast, ETHER_ADDR_LEN);
		params->bss_type = DOT11_BSSTYPE_ANY;
		params->scan_type = 0;
		params->nprobes = htod32(-1);
		params->active_time = htod32(-1);
		params->passive_time = htod32(-1);
		params->home_time = htod32(-1);
		params->channel_num = 0;
		bzero(&params->ssid, sizeof(wlc_ssid_t));
		chan_list = params->channel_list;
	}

	if (!request) {
		/* scan_request null, do scan based on base config */
		WL_DBG(("scan_request is null\n"));
		return BCME_OK;
	}

	WL_INFORM(("n_channels:%d n_ssids:%d ver:%d\n",
		request->n_channels, request->n_ssids, cfg->scan_params_ver));

	cur_offset = channel_offset;
	/* Copy channel array if applicable */
#ifdef SCAN_SUPPRESS
	channel = wl_ext_scan_suppress(ndev, scan_params, cfg->scan_params_ver, &chan_info);
	if (channel) {
		n_channels = 1;
		if ((n_channels > 0) && chan_list) {
			if (len >= (scan_param_size + (n_channels * sizeof(u16)))) {
				wl_cfgscan_populate_scan_channel(cfg,
					request->channels, request->n_channels,
					chan_list, &chan_info);
				cur_offset += (n_channels * (sizeof(u16)));
			}
		}
	} else
#endif
	if ((request->n_channels > 0) && chan_list) {
		if (len >= (scan_param_size + (request->n_channels * sizeof(u16)))) {
			wl_cfgscan_populate_scan_channels(cfg,
					request->channels, request->n_channels,
					chan_list, &n_channels, true, false);
			cur_offset += (uint32)(n_channels * (sizeof(u16)));
		}
	}

	/* Copy ssid array if applicable */
	if (request->n_ssids > 0) {
		cur_offset = (u32) roundup(cur_offset, sizeof(u32));
		if (len > (cur_offset + (request->n_ssids * sizeof(wlc_ssid_t)))) {
			u32 rem_len = len - cur_offset;
			wl_cfgscan_populate_scan_ssids(cfg,
				((u8 *)scan_params + cur_offset), rem_len, request, &n_ssids);
		}
	}

	if (n_ssids || n_channels) {
		u32 channel_num =
				htod32((n_ssids << WL_SCAN_PARAMS_NSSID_SHIFT) |
				(n_channels & WL_SCAN_PARAMS_COUNT_MASK));
		if (params_v3) {
			params_v3->channel_num = channel_num;
			if (n_channels == 1) {
				params_v3->active_time = htod32(WL_SCAN_CONNECT_DWELL_TIME_MS);
				params_v3->nprobes = htod32(
					params_v3->active_time / WL_SCAN_JOIN_PROBE_INTERVAL_MS);
			}
		} else {
			params->channel_num = channel_num;
			if (n_channels == 1) {
				params->active_time = htod32(WL_SCAN_CONNECT_DWELL_TIME_MS);
				params->nprobes = htod32(
					params->active_time / WL_SCAN_JOIN_PROBE_INTERVAL_MS);
			}
		}
	}

	WL_DBG_MEM(("scan_prep done. n_channels:%d n_ssids:%d\n", n_channels, n_ssids));
	return BCME_OK;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) && \
	defined(SUPPORT_RANDOM_MAC_SCAN)
static s32
wl_config_scan_macaddr(struct bcm_cfg80211 *cfg,
		struct net_device *ndev, bool randmac_enable, u8 *mac_addr, u8 *mac_addr_mask)
{
	s32 err = BCME_OK;

	if (randmac_enable) {
		if (!cfg->scanmac_enabled) {
			err = wl_cfg80211_scan_mac_enable(ndev);
			if (unlikely(err)) {
				goto exit;
			}
			WL_DBG(("randmac enabled\n"));
		}

#ifdef WL_HOST_RANDMAC_CONFIG
		/* If mask provided, apply user space configuration */
		if (!mac_addr_mask && !mac_addr && !ETHER_ISNULLADDR(mac_addr_mask)) {
			err = wl_cfg80211_scan_mac_config(ndev,
				mac_addr, mac_addr_mask);
			if (unlikely(err)) {
				WL_ERR(("scan mac config failed\n"));
				goto exit;
			}
		}
#endif /* WL_HOST_RANDMAC_CONFIG */
		if (cfg->scanmac_config) {
			/* Use default scanmac configuration */
			WL_DBG(("Use host provided scanmac config\n"));
		} else {
			WL_DBG(("Use fw default scanmac config\n"));
		}
	} else if (!randmac_enable && cfg->scanmac_enabled) {
		WL_DBG(("randmac disabled\n"));
		err = wl_cfg80211_scan_mac_disable(ndev);
	} else {
		WL_DBG(("no change in randmac configuration\n"));
	}

exit:
	if (err < 0) {
		if (err == BCME_UNSUPPORTED) {
			/* Ignore if chip doesnt support the feature */
			err = BCME_OK;
		} else {
			/* For errors other than unsupported fail the scan */
			WL_ERR(("%s : failed to configure random mac for host scan, %d\n",
				__FUNCTION__, err));
			err = -EAGAIN;
		}
	}

	return err;
}
#endif /* LINUX VER > 3.19 && SUPPORT_RANDOM_MAC_SCAN */

static s32
wl_run_escan(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	struct cfg80211_scan_request *request, uint16 action)
{
	s32 err = BCME_OK;
	u32 num_chans = 0;
	u32 n_channels = 0;
	u32 n_ssids;
	s32 params_size;
	wl_escan_params_v1_t *eparams = NULL;
	wl_escan_params_v3_t *eparams_v3 = NULL;
	u8 *scan_params = NULL;
	u8 *params = NULL;
	s32 search_state = WL_P2P_DISC_ST_SCAN;
	u16 *default_chan_list = NULL;
	s32 bssidx = -1;
	struct net_device *dev = NULL;
#if defined(USE_INITIAL_2G_SCAN) || defined(USE_INITIAL_SHORT_DWELL_TIME)
	bool is_first_init_2g_scan = false;
#endif /* USE_INITIAL_2G_SCAN || USE_INITIAL_SHORT_DWELL_TIME */
	p2p_scan_purpose_t	p2p_scan_purpose = P2P_SCAN_PURPOSE_MIN;
	u32 chan_mem = 0;
	u32 sync_id = 0;

	WL_DBG(("Enter \n"));

	if (!cfg || !request) {
		err = -EINVAL;
		WL_ERR(("invalid escan parameter\n"));
		goto exit;
	}

	if (IS_SCAN_PARAMS_V3_V2(cfg)) {
		params_size = (WL_SCAN_PARAMS_V3_FIXED_SIZE +
			OFFSETOF(wl_escan_params_v3_t, params));
	} else {
		params_size = (WL_SCAN_PARAMS_V1_FIXED_SIZE
			+ OFFSETOF(wl_escan_params_v1_t, params));
	}

	if (!cfg->p2p_supported || !p2p_scan(cfg)) {
		/* LEGACY SCAN TRIGGER */
		WL_SCAN((" LEGACY E-SCAN START\n"));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) && \
	defined(SUPPORT_RANDOM_MAC_SCAN)
	if (request) {
		bool randmac_enable = (request->flags & NL80211_SCAN_FLAG_RANDOM_ADDR);
		if (wl_is_wps_enrollee_active(ndev, request->ie, request->ie_len)) {
			randmac_enable = false;
		}
		if ((err = wl_config_scan_macaddr(cfg, ndev, randmac_enable,
			request->mac_addr, request->mac_addr_mask)) != BCME_OK) {
				WL_ERR(("scanmac addr config failed\n"));
			goto exit;
		}
	}
#endif /* KERNEL_VER >= 3.19 && SUPPORT_RANDOM_MAC_SCAN */

#if defined(USE_INITIAL_2G_SCAN) || defined(USE_INITIAL_SHORT_DWELL_TIME)
		if (ndev == bcmcfg_to_prmry_ndev(cfg) && g_first_broadcast_scan == true) {
#ifdef USE_INITIAL_2G_SCAN
			struct ieee80211_channel tmp_channel_list[CH_MAX_2G_CHANNEL];
			/* allow one 5G channel to add previous connected channel in 5G */
			bool allow_one_5g_channel = TRUE;
			int i, j;
			j = 0;
			for (i = 0; i < request->n_channels; i++) {
				int tmp_chan = ieee80211_frequency_to_channel
					(request->channels[i]->center_freq);
				if (tmp_chan > CH_MAX_2G_CHANNEL) {
					if (allow_one_5g_channel)
						allow_one_5g_channel = FALSE;
					else
						continue;
				}
				if (j > CH_MAX_2G_CHANNEL) {
					WL_ERR(("Index %d exceeds max 2.4GHz channels %d"
						" and previous 5G connected channel\n",
						j, CH_MAX_2G_CHANNEL));
					break;
				}
				bcopy(request->channels[i], &tmp_channel_list[j],
					sizeof(struct ieee80211_channel));
				WL_SCAN(("channel of request->channels[%d]=%d\n", i, tmp_chan));
				j++;
			}
			if ((j > 0) && (j <= CH_MAX_2G_CHANNEL)) {
				for (i = 0; i < j; i++)
					bcopy(&tmp_channel_list[i], request->channels[i],
						sizeof(struct ieee80211_channel));

				request->n_channels = j;
				is_first_init_2g_scan = true;
			}
			else
				WL_ERR(("Invalid number of 2.4GHz channels %d\n", j));

			WL_SCAN(("request->n_channels=%d\n", request->n_channels));
#else /* USE_INITIAL_SHORT_DWELL_TIME */
			is_first_init_2g_scan = true;
#endif /* USE_INITIAL_2G_SCAN */
			g_first_broadcast_scan = false;
		}
#endif /* USE_INITIAL_2G_SCAN || USE_INITIAL_SHORT_DWELL_TIME */

		n_channels = request->n_channels;
		n_ssids = request->n_ssids;
		if (n_channels % 2)
			/* If n_channels is odd, add a padd of u16 */
			params_size += sizeof(u16) * (n_channels + 1);
		else
			params_size += sizeof(u16) * n_channels;

		/* Allocate space for populating ssids in wl_escan_params_v1_t struct */
		params_size += sizeof(struct wlc_ssid) * n_ssids;
		params = MALLOCZ(cfg->osh, params_size);
		if (params == NULL) {
			err = -ENOMEM;
			WL_ERR(("malloc failure for escan params\n"));
			goto exit;
		}

		wl_escan_set_sync_id(sync_id, cfg);
		if (IS_SCAN_PARAMS_V3_V2(cfg)) {
			eparams_v3 = (wl_escan_params_v3_t *)params;
			scan_params = (u8 *)&eparams_v3->params;
			eparams_v3->version = htod32(cfg->scan_params_ver);
			eparams_v3->action =  htod16(action);
			eparams_v3->sync_id = sync_id;
		} else {
			eparams = (wl_escan_params_v1_t *)params;
			scan_params = (u8 *)&eparams->params;
			eparams->version = htod32(ESCAN_REQ_VERSION_V1);
			eparams->action =  htod16(action);
			eparams->sync_id = sync_id;
		}

		if (wl_scan_prep(cfg, ndev, scan_params, params_size, request) < 0) {
			WL_ERR(("scan_prep failed\n"));
			err = -EINVAL;
			goto exit;
		}

#if defined(USE_INITIAL_2G_SCAN) || defined(USE_INITIAL_SHORT_DWELL_TIME)
		/* Override active_time to reduce scan time if it's first bradcast scan. */
		if (is_first_init_2g_scan) {
			if (eparams_v3) {
				eparams_v3->params.active_time = FIRST_SCAN_ACTIVE_DWELL_TIME_MS;
			} else {
				eparams->params.active_time = FIRST_SCAN_ACTIVE_DWELL_TIME_MS;
			}
		}
#endif /* USE_INITIAL_2G_SCAN || USE_INITIAL_SHORT_DWELL_TIME */

		wl_escan_set_type(cfg, WL_SCANTYPE_LEGACY);
		if (params_size + sizeof("escan") >= WLC_IOCTL_MEDLEN) {
			WL_ERR(("ioctl buffer length not sufficient\n"));
			MFREE(cfg->osh, params, params_size);
			err = -ENOMEM;
			goto exit;
		}

		bssidx = wl_get_bssidx_by_wdev(cfg, ndev->ieee80211_ptr);
	//	WL_MSG(ndev->name, "LEGACY_SCAN sync ID: %d, bssidx: %d\n", sync_id, bssidx);
		err = wldev_iovar_setbuf(ndev, "escan", params, params_size,
			cfg->escan_ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
		if (unlikely(err)) {
			if (err == BCME_EPERM)
				/* Scan Not permitted at this point of time */
				WL_DBG((" Escan not permitted at this time (%d)\n", err));
			else
				WL_ERR((" Escan set error (%d)\n", err));
		} else {
			DBG_EVENT_LOG((dhd_pub_t *)cfg->pub, WIFI_EVENT_DRIVER_SCAN_REQUESTED);
		}
		MFREE(cfg->osh, params, params_size);
	}
	else if (p2p_is_on(cfg) && p2p_scan(cfg)) {
		/* P2P SCAN TRIGGER */
		if (request->n_channels) {
			num_chans = request->n_channels;
			WL_SCAN((" chan number : %d\n", num_chans));
			chan_mem = (u32)(num_chans * sizeof(*default_chan_list));
			default_chan_list = MALLOCZ(cfg->osh, chan_mem);
			if (default_chan_list == NULL) {
				WL_ERR(("channel list allocation failed \n"));
				err = -ENOMEM;
				goto exit;
			}
			/* Populate channels for p2p scanning */
			wl_cfgscan_populate_scan_channels(cfg,
				request->channels, request->n_channels,
				default_chan_list, &num_chans, true, true);

			if (num_chans == SOCIAL_CHAN_CNT && (
						(CHSPEC_CHANNEL(default_chan_list[0]) ==
						SOCIAL_CHAN_1) &&
						(CHSPEC_CHANNEL(default_chan_list[1]) ==
						SOCIAL_CHAN_2) &&
						(CHSPEC_CHANNEL(default_chan_list[2]) ==
						SOCIAL_CHAN_3))) {
				/* SOCIAL CHANNELS 1, 6, 11 */
				search_state = WL_P2P_DISC_ST_SEARCH;
				p2p_scan_purpose = P2P_SCAN_SOCIAL_CHANNEL;
				WL_DBG(("P2P SEARCH PHASE START \n"));
			} else if (((dev = wl_to_p2p_bss_ndev(cfg, P2PAPI_BSSCFG_CONNECTION1)) &&
				(wl_get_mode_by_netdev(cfg, dev) == WL_MODE_AP)) ||
				((dev = wl_to_p2p_bss_ndev(cfg, P2PAPI_BSSCFG_CONNECTION2)) &&
				(wl_get_mode_by_netdev(cfg, dev) == WL_MODE_AP))) {
				/* If you are already a GO, then do SEARCH only */
				WL_DBG(("Already a GO. Do SEARCH Only"));
				search_state = WL_P2P_DISC_ST_SEARCH;
				p2p_scan_purpose = P2P_SCAN_NORMAL;

			} else if (num_chans == 1) {
				p2p_scan_purpose = P2P_SCAN_CONNECT_TRY;
			} else if (num_chans == SOCIAL_CHAN_CNT + 1) {
			/* SOCIAL_CHAN_CNT + 1 takes care of the Progressive scan supported by
			 * the supplicant
			 */
				p2p_scan_purpose = P2P_SCAN_SOCIAL_CHANNEL;
			} else {
				WL_DBG(("P2P SCAN STATE START \n"));
				p2p_scan_purpose = P2P_SCAN_NORMAL;
			}
		} else {
			err = -EINVAL;
			goto exit;
		}
		WL_INFORM_MEM(("p2p_scan  num_channels:%d\n", num_chans));
		err = wl_cfgp2p_escan(cfg, ndev, ACTIVE_SCAN, num_chans, default_chan_list,
			search_state, action,
			wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE), NULL,
			p2p_scan_purpose);

		if (!err)
			cfg->p2p->search_state = search_state;

		MFREE(cfg->osh, default_chan_list, chan_mem);
	} else {
		WL_ERR(("escan not triggered - p2p_supported=%d, p2p_scan=%d, p2p_is_on=%d\n",
			cfg->p2p_supported, p2p_scan(cfg), p2p_is_on(cfg)));
		err = -EINVAL;
		goto exit;
	}
exit:
	if (unlikely(err)) {
		/* Don't print Error incase of Scan suppress */
		if ((err == BCME_EPERM) && cfg->scan_suppressed)
			WL_DBG(("Escan failed: Scan Suppressed \n"));
		else
			WL_ERR(("scan error (%d)\n", err));
	}
	return err;
}

s32
wl_do_escan(struct bcm_cfg80211 *cfg, struct wiphy *wiphy, struct net_device *ndev,
	struct cfg80211_scan_request *request)
{
	s32 err = BCME_OK;
	s32 passive_scan;
	s32 passive_scan_time;
	s32 passive_scan_time_org;
	wl_scan_results_v109_t *results;
	WL_SCAN(("Enter \n"));

	results = wl_escan_get_buf(cfg, FALSE);
	results->version = 0;
	results->count = 0;
	results->buflen = WL_SCAN_RESULTS_V109_FIXED_SIZE;

	cfg->escan_info.ndev = ndev;
	cfg->escan_info.wiphy = wiphy;
	cfg->escan_info.escan_state = WL_ESCAN_STATE_SCANING;
	passive_scan = cfg->active_scan ? 0 : 1;
	err = wldev_ioctl_set(ndev, WLC_SET_PASSIVE_SCAN,
	                      &passive_scan, sizeof(passive_scan));
	if (unlikely(err)) {
		WL_ERR(("error (%d)\n", err));
		goto exit;
	}

	if (passive_channel_skip) {

		err = wldev_ioctl_get(ndev, WLC_GET_SCAN_PASSIVE_TIME,
			&passive_scan_time_org, sizeof(passive_scan_time_org));
		if (unlikely(err)) {
			WL_ERR(("== error (%d)\n", err));
			goto exit;
		}

		WL_SCAN(("PASSIVE SCAN time : %d \n", passive_scan_time_org));

		passive_scan_time = 0;
		err = wldev_ioctl_set(ndev, WLC_SET_SCAN_PASSIVE_TIME,
			&passive_scan_time, sizeof(passive_scan_time));
		if (unlikely(err)) {
			WL_ERR(("== error (%d)\n", err));
			goto exit;
		}

		WL_SCAN(("PASSIVE SCAN SKIPED!! (passive_channel_skip:%d) \n",
			passive_channel_skip));
	}

	err = wl_run_escan(cfg, ndev, request, WL_SCAN_ACTION_START);
	if (unlikely(err)) {
		WL_ERR(("escan failed (%d)\n", err));
		goto exit;
	}

	if (passive_channel_skip) {
		err = wldev_ioctl_set(ndev, WLC_SET_SCAN_PASSIVE_TIME,
			&passive_scan_time_org, sizeof(passive_scan_time_org));
		if (unlikely(err)) {
			WL_ERR(("== error (%d)\n", err));
			goto exit;
		}

		WL_SCAN(("PASSIVE SCAN RECOVERED!! (passive_scan_time_org:%d) \n",
			passive_scan_time_org));
	}

exit:
	return err;
}

static s32
wl_get_scan_timeout_val(struct bcm_cfg80211 *cfg)
{
	u32 scan_timer_interval_ms = WL_SCAN_TIMER_INTERVAL_MS;

#ifdef WES_SUPPORT
#ifdef CUSTOMER_SCAN_TIMEOUT_SETTING
	if ((cfg->custom_scan_channel_time > DHD_SCAN_ASSOC_ACTIVE_TIME) |
		(cfg->custom_scan_unassoc_time > DHD_SCAN_UNASSOC_ACTIVE_TIME) |
		(cfg->custom_scan_passive_time > DHD_SCAN_PASSIVE_TIME) |
		(cfg->custom_scan_home_time > DHD_SCAN_HOME_TIME) |
		(cfg->custom_scan_home_away_time > DHD_SCAN_HOME_AWAY_TIME)) {
		scan_timer_interval_ms = CUSTOMER_WL_SCAN_TIMER_INTERVAL_MS;
	}
#endif /* CUSTOMER_SCAN_TIMEOUT_SETTING */
#endif /* WES_SUPPORT */

	/* If NAN is enabled adding +10 sec to the existing timeout value */
#ifdef WL_NAN
	if (wl_cfgnan_is_enabled(cfg)) {
		scan_timer_interval_ms += WL_SCAN_TIMER_INTERVAL_MS_NAN;
	}
#endif /* WL_NAN */
	/* Additional time to scan 6GHz band channels */
#ifdef WL_6G_BAND
	if (cfg->band_6g_supported) {
		scan_timer_interval_ms += WL_SCAN_TIMER_INTERVAL_MS_6G;
	}
#endif /* WL_6G_BAND */
	WL_MEM(("scan_timer_interval_ms %d\n", scan_timer_interval_ms));
	return scan_timer_interval_ms;
}

#define SCAN_EBUSY_RETRY_LIMIT 20
static s32
wl_cfgscan_handle_scanbusy(struct bcm_cfg80211 *cfg, struct net_device *ndev, s32 err)
{
	s32	scanbusy_err = 0;
	static u32 busy_count = 0;

	if (!err) {
		busy_count = 0;
		return scanbusy_err;
	}

	if (!p2p_scan(cfg) && wl_get_drv_status_all(cfg, REMAINING_ON_CHANNEL)) {
		WL_ERR(("Scan err = (%d) due to p2p scan, nothing to do\n", err));
		busy_count = 0;
	}

	if (err == BCME_BUSY || err == BCME_NOTREADY) {
		WL_ERR(("Scan err = (%d), busy?%d\n", err, -EBUSY));
		scanbusy_err = -EBUSY;
	} else if ((err == BCME_EPERM) && cfg->scan_suppressed) {
		WL_ERR(("Scan not permitted due to scan suppress\n"));
		scanbusy_err = -EPERM;
	} else {
		/* For all other fw errors, use a generic error code as return
		 * value to cfg80211 stack
		 */
		scanbusy_err = -EAGAIN;
	}

	/* if continuous busy state, clear assoc type in FW by disassoc cmd */
	if (scanbusy_err == -EBUSY) {
		/* Flush FW preserve buffer logs for checking failure */
		if (busy_count++ > (SCAN_EBUSY_RETRY_LIMIT/5)) {
			wl_flush_fw_log_buffer(ndev, FW_LOGSET_MASK_ALL);
		}
		if (busy_count > SCAN_EBUSY_RETRY_LIMIT) {
			struct ether_addr bssid;
			s32 ret = 0;
#ifdef BCMDONGLEHOST
			dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);
			if (dhd_query_bus_erros(dhdp)) {
				return BCME_NOTREADY;
			}
			dhdp->scan_busy_occurred = TRUE;
#endif /* BCMDONGLEHOST */
			busy_count = 0;
			WL_ERR(("Unusual continuous EBUSY error, %d %d %d %d %d %d %d %d %d\n",
				wl_get_drv_status(cfg, SCANNING, ndev),
				wl_get_drv_status(cfg, SCAN_ABORTING, ndev),
				wl_get_drv_status(cfg, CONNECTING, ndev),
				wl_get_drv_status(cfg, CONNECTED, ndev),
				wl_get_drv_status(cfg, DISCONNECTING, ndev),
				wl_get_drv_status(cfg, AP_CREATING, ndev),
				wl_get_drv_status(cfg, AP_CREATED, ndev),
				wl_get_drv_status(cfg, SENDING_ACT_FRM, ndev),
				wl_get_drv_status(cfg, SENDING_ACT_FRM, ndev)));

#ifdef BCMDONGLEHOST
#if defined(DHD_DEBUG) && defined(DHD_FW_COREDUMP)
			if (dhdp->memdump_enabled) {
				dhdp->memdump_type = DUMP_TYPE_SCAN_BUSY;
				dhd_bus_mem_dump(dhdp);
			}
#endif /* DHD_DEBUG && DHD_FW_COREDUMP */
			dhdp->hang_reason = HANG_REASON_SCAN_BUSY;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
			dhd_os_send_hang_message(dhdp);
#endif

#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
			WL_ERR(("%s: HANG event is unsupported\n", __FUNCTION__));
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) && OEM_ANDROID */
#endif /* BCMDONGLEHOST */

			bzero(&bssid, sizeof(bssid));
			if ((ret = wldev_ioctl_get(ndev, WLC_GET_BSSID,
				&bssid, ETHER_ADDR_LEN)) == 0) {
				WL_ERR(("FW is connected with " MACDBG "\n",
					MAC2STRDBG(bssid.octet)));
			} else {
				WL_ERR(("GET BSSID failed with %d\n", ret));
			}

			/* To support GO, wl_cfgscan_cancel_scan()
			 * is needed instead of wl_cfg80211_disconnect()
			 */
			wl_cfgscan_cancel_scan(cfg);

		} else {
			/* Hold the context for 400msec, so that 10 subsequent scans
			* can give a buffer of 4sec which is enough to
			* cover any on-going scan in the firmware
			*/
			WL_DBG(("Enforcing delay for EBUSY case \n"));
			msleep(400);
		}
	} else {
		busy_count = 0;
	}

	return scanbusy_err;
}

static bool
is_p2p_ssid_present(struct cfg80211_scan_request *request)
{
	struct cfg80211_ssid *ssids;
	int i;

	ssids = request->ssids;
	for (i = 0; i < request->n_ssids; i++) {
			if (ssids[i].ssid_len &&
				IS_P2P_SSID(ssids[i].ssid, ssids[i].ssid_len)) {
				/* P2P Scan */
				return TRUE;
			}
	}
	return FALSE;
}

static void
wl_set_p2p_scan_states(struct bcm_cfg80211 *cfg, struct net_device *ndev)
{
	if (cfg->p2p_supported) {
		/* p2p scan trigger */
		if (p2p_on(cfg) == false) {
			/* p2p on at the first time */
			p2p_on(cfg) = true;
#if defined(P2P_IE_MISSING_FIX)
			cfg->p2p_prb_noti = false;
#endif
		}
		wl_clr_p2p_status(cfg, GO_NEG_PHASE);
		WL_DBG(("P2P: GO_NEG_PHASE status cleared \n"));
		p2p_scan(cfg) = true;
	}
}

static void
wl_set_legacy_scan_states(struct bcm_cfg80211 *cfg,
		struct cfg80211_scan_request *request, struct net_device *ndev, s32 bssidx)
{
	s32 err = BCME_OK;
#ifdef WL11U
	bcm_tlv_t *interworking_ie;
#endif

	/* legacy scan trigger
	 * So, we have to disable p2p discovery if p2p discovery is on
	 */
	if (cfg->p2p_supported) {
		p2p_scan(cfg) = false;
		if (wl_get_p2p_status(cfg, DISCOVERY_ON)) {
			err = wl_cfgp2p_discover_enable_search(cfg, false);
			if (unlikely(err)) {
				WL_ERR(("disable discovery failed\n"));
				/* non-fatal error. fall through */
			}
		}
	}

#ifdef WL11U
	if (request && (interworking_ie = wl_cfg80211_find_interworking_ie(request->ie,
			request->ie_len)) != NULL) {
		if ((err = wl_cfg80211_add_iw_ie(cfg, ndev, bssidx,
			VNDR_IE_CUSTOM_FLAG, interworking_ie->id, interworking_ie->data,
			interworking_ie->len)) != BCME_OK) {
			WL_ERR(("Failed to add interworking IE"));
		}
	} else if (cfg->wl11u) {
		/* we have to clear IW IE and disable gratuitous APR */
		wl_cfg80211_clear_iw_ie(cfg, ndev, bssidx);
		err = wldev_iovar_setint_bsscfg(ndev, "grat_arp", 0, bssidx);
		/* we don't care about error here
		 * because the only failure case is unsupported,
		 * which is fine
		 */
		if (unlikely(err)) {
			WL_ERR(("Set grat_arp failed:(%d) Ignore!\n", err));
		}
		cfg->wl11u = FALSE;
	}
#endif /* WL11U */
}

static bool
is_scan_allowed(struct bcm_cfg80211 *cfg, struct net_device *ndev)
{
#ifdef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
	struct net_device *remain_on_channel_ndev = NULL;
#endif

#ifdef WL_SCHED_SCAN
	if (cfg->sched_scan_running) {
		WL_ERR(("PNO SCAN in progress\n"));
		return FALSE;
	}
#endif /* WL_SCHED_SCAN */

	if (wl_get_drv_status_all(cfg, SCANNING)) {
		if (cfg->scan_request == NULL) {
			/* SCANNING bit is set, but scan_request NULL!! */
			wl_clr_drv_status_all(cfg, SCANNING);
			WL_DBG(("<<<<<<<<<<<Force Clear Scanning Status>>>>>>>>>>>\n"));
		} else {
			WL_ERR(("Scanning already\n"));
			return FALSE;
		}
	}

	if (wl_get_drv_status(cfg, CONNECTING, ndev)) {
		WL_ERR(("Association is in progress, skip new scan\n"));
		return FALSE;
	}

	if (wl_get_drv_status(cfg, SCAN_ABORTING, ndev)) {
		WL_ERR(("Scanning being aborted. skip new scan\n"));
		return FALSE;
	}

	if (cfg->loc.in_progress) {
		WL_ERR(("loc in progress. skip new scan\n"));
		/* Listen in progress, avoid new scan trigger */
		return FALSE;
	}

	if (WL_DRV_STATUS_SENDING_AF_FRM_EXT(cfg)) {
		WL_ERR(("Sending Action Frames. Try it again.\n"));
		return FALSE;
	}

#ifdef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
	remain_on_channel_ndev = wl_cfg80211_get_remain_on_channel_ndev(cfg);
	if (remain_on_channel_ndev) {
		/* scan listen and proceed */
		WL_DBG(("Remain_on_channel bit is set, somehow it didn't get cleared\n"));
		_wl_cfgscan_cancel_scan(cfg);
	}
#endif /* WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST */

	return TRUE;
}

s32
__wl_cfg80211_scan(struct wiphy *wiphy, struct net_device *ndev,
	struct cfg80211_scan_request *request,
	struct cfg80211_ssid *this_ssid)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	bool p2p_ssid = FALSE;
	s32 err = 0;
	bool escan_req_failed = false;
	s32 scanbusy_err = 0;
	unsigned long flags;
	s32 bssidx = 0;

	/*
	 * Hostapd triggers scan before starting automatic channel selection
	 * to collect channel characteristics. However firmware scan engine
	 * doesn't support any channel characteristics collection along with
	 * scan. Hence return scan success.
	 */
	if (request && (scan_req_iftype(request) == NL80211_IFTYPE_AP)) {
		WL_DBG(("Scan Command on SoftAP Interface. Ignoring...\n"));
// terence 20161023: let it scan in SoftAP mode
//		return -EOPNOTSUPP;
	}

	if (request && request->n_ssids > WL_SCAN_PARAMS_SSID_MAX) {
		WL_ERR(("request null or n_ssids > WL_SCAN_PARAMS_SSID_MAX\n"));
		return -EOPNOTSUPP;
	}

	ndev = ndev_to_wlc_ndev(ndev, cfg);

	WL_DBG(("[%s] Enter\n", ndev->name));

#ifdef WL_BCNRECV
	/* check fakeapscan in progress then abort */
	wl_android_bcnrecv_stop(ndev, WL_BCNRECV_SCANBUSY);
#endif /* WL_BCNRECV */

#ifdef P2P_LISTEN_OFFLOADING
	wl_cfg80211_cancel_p2plo(cfg);
#endif /* P2P_LISTEN_OFFLOADING */

#ifdef WL_SDO
	if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS)) {
		wl_cfg80211_pause_sdo(ndev, cfg);
	}
#endif

	if (request) {
		/* scan bss */
		p2p_ssid = is_p2p_ssid_present(request);
		if (p2p_ssid) {
#if defined(WL_CFG80211_P2P_DEV_IF)
			if (!(IS_P2P_IFACE(request->wdev)))
#else
			if (!(IS_P2P_IFACE(ndev->ieee80211_ptr)))
#endif
			{
				/* P2P SSID in legacy scan */
				WL_DBG(("p2p_search on non p2p iface %d\n",
#if defined(WL_CFG80211_P2P_DEV_IF)
					request->wdev->iftype
#else
					ndev->ieee80211_ptr->iftype
#endif
				));
				p2p_ssid = FALSE;
			}
		}
	}

	if ((bssidx = wl_get_bssidx_by_wdev(cfg, ndev->ieee80211_ptr)) < 0) {
		WL_ERR(("Find bssidx from wdev failed!\n"));
		err = -EINVAL;
		goto scan_out;
	}

	WL_TRACE_HW4(("START SCAN\n"));

#if defined(BCMDONGLEHOST)
	DHD_OS_SCAN_WAKE_LOCK_TIMEOUT((dhd_pub_t *)(cfg->pub),
		wl_get_scan_timeout_val(cfg) + SCAN_WAKE_LOCK_MARGIN_MS);
	DHD_DISABLE_RUNTIME_PM((dhd_pub_t *)(cfg->pub));
#endif

	if (cfg->p2p_supported && p2p_ssid) {
#ifdef WL_SDO
		if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS)) {
			/* We shouldn't be getting p2p_find while discovery
			 * offload is in progress
			 */
			WL_SD(("P2P_FIND: Discovery offload is in progress."
				" Do nothing\n"));
			err = -EINVAL;
			goto scan_out;
		}
#endif
		/* find my listen channel */
		cfg->afx_hdl->my_listen_chan =
			wl_find_listen_channel(cfg, request->ie, request->ie_len);
		err = wl_cfgp2p_enable_discovery(cfg, ndev, request->ie, request->ie_len);
		if (unlikely(err)) {
			goto scan_out;
		}
	}

	mutex_lock(&cfg->scan_sync);

	if (is_scan_allowed(cfg, ndev) == FALSE) {
		err = -EAGAIN;
		WL_ERR(("scan not permitted!\n"));
		escan_req_failed = true;
		goto scan_out;
	}

	if (request) {
		/* set scan related states that need mutex protection */
		if (p2p_ssid) {
			wl_set_p2p_scan_states(cfg, ndev);
		} else {
			wl_set_legacy_scan_states(cfg, request, ndev, bssidx);
		}

		/* configure upper app provided IEs to the firmware */
		err = wl_cfg80211_set_mgmt_vndr_ies(cfg, ndev_to_cfgdev(ndev), bssidx,
				VNDR_IE_PRBREQ_FLAG, request->ie, request->ie_len);
		if (unlikely(err)) {
			WL_ERR(("vndr_ie set for probereq failed for bssidx:%d!\n", bssidx));
		}
	}

#ifdef WL_EXT_IAPSTA
	mutex_unlock(&cfg->scan_sync);
	err = wl_ext_in4way_sync(ndev, STA_FAKE_SCAN_IN_CONNECT, WL_EXT_STATUS_SCANNING, NULL);
	if (err) {
		wl_event_msg_t msg;
		mutex_lock(&cfg->scan_sync);
		bzero(&msg, sizeof(wl_event_msg_t));
		msg.event_type = hton32(WLC_E_ESCAN_RESULT);
		msg.status = hton32(WLC_E_STATUS_SUCCESS);
		wl_cfg80211_event(ndev, &msg, NULL);
		goto scan_success;
	}
#endif
	mutex_lock(&cfg->scan_sync);
	err = wl_do_escan(cfg, wiphy, ndev, request);
	if (likely(!err)) {
		goto scan_success;
	} else {
		escan_req_failed = true;
		goto scan_out;
	}

scan_success:
	wl_cfgscan_handle_scanbusy(cfg, ndev, BCME_OK);
	cfg->scan_request = request;
	LOG_TS(cfg, scan_start);
	wl_set_drv_status(cfg, SCANNING, ndev);
	/* Arm the timer */
	mod_timer(&cfg->scan_timeout,
		jiffies + msecs_to_jiffies(wl_get_scan_timeout_val(cfg)));
	mutex_unlock(&cfg->scan_sync);
	return 0;

scan_out:
	if (escan_req_failed) {
		WL_CFG_DRV_LOCK(&cfg->cfgdrv_lock, flags);
		cfg->scan_request = NULL;
		WL_CFG_DRV_UNLOCK(&cfg->cfgdrv_lock, flags);
		mutex_unlock(&cfg->scan_sync);
		/* Handling for scan busy errors */
		scanbusy_err = wl_cfgscan_handle_scanbusy(cfg, ndev, err);
		if (scanbusy_err == BCME_NOTREADY) {
			/* In case of bus failures avoid ioctl calls */

#if defined(BCMDONGLEHOST)
			DHD_OS_SCAN_WAKE_UNLOCK((dhd_pub_t *)(cfg->pub));
			DHD_ENABLE_RUNTIME_PM((dhd_pub_t *)(cfg->pub));
#endif

			return -ENODEV;
		}
		err = scanbusy_err;
	}

#if defined(BCMDONGLEHOST)
	DHD_OS_SCAN_WAKE_UNLOCK((dhd_pub_t *)(cfg->pub));
	DHD_ENABLE_RUNTIME_PM((dhd_pub_t *)(cfg->pub));
#endif

#ifdef WL_SDO
	if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS)) {
		wl_cfg80211_resume_sdo(ndev, cfg);
	}
#endif
	return err;
}

s32
#if defined(WL_CFG80211_P2P_DEV_IF)
wl_cfg80211_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request)
#else
wl_cfg80211_scan(struct wiphy *wiphy, struct net_device *ndev,
	struct cfg80211_scan_request *request)
#endif /* WL_CFG80211_P2P_DEV_IF */
{
	s32 err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
#if defined(WL_CFG80211_P2P_DEV_IF)
	struct net_device *ndev = wdev_to_wlc_ndev(request->wdev, cfg);
#endif /* WL_CFG80211_P2P_DEV_IF */
#ifdef WL_CFGVENDOR_SEND_ALERT_EVENT
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);
#endif /* WL_CFGVENDOR_SEND_ALERT_EVENT */

	WL_DBG(("Enter\n"));
	RETURN_EIO_IF_NOT_UP(cfg);

#ifdef DHD_IFDEBUG
#ifdef WL_CFG80211_P2P_DEV_IF
	PRINT_WDEV_INFO(request->wdev);
#else
	PRINT_WDEV_INFO(ndev);
#endif /* WL_CFG80211_P2P_DEV_IF */
#endif /* DHD_IFDEBUG */

	if (ndev == bcmcfg_to_prmry_ndev(cfg)) {
		if (wl_cfg_multip2p_operational(cfg)) {
			WL_ERR(("wlan0 scan failed, p2p devices are operational"));
			 return -ENODEV;
		}
	}
#ifdef WL_EXT_IAPSTA
	err = wl_ext_in4way_sync(ndev_to_wlc_ndev(ndev, cfg), STA_NO_SCAN_IN4WAY,
		WL_EXT_STATUS_SCAN, NULL);
	if (err) {
		WL_SCAN(("scan suppressed %d\n", err));
		return err;
	}
#endif

	err = __wl_cfg80211_scan(wiphy, ndev, request, NULL);
	if (unlikely(err)) {
		WL_ERR(("scan error (%d)\n", err));
#ifdef WL_CFGVENDOR_SEND_ALERT_EVENT
		if (err == -EBUSY) {
			dhdp->alert_reason = ALERT_SCAN_BUSY;
		} else {
			dhdp->alert_reason = ALERT_SCAN_ERR;
		}
		dhd_os_send_alert_message(dhdp);
#endif /* WL_CFGVENDOR_SEND_ALERT_EVENT */
	}
#ifdef WL_DRV_AVOID_SCANCACHE
	/* Reset roam cache after successful scan request */
#ifdef ROAM_CHANNEL_CACHE
	if (!err) {
		reset_roam_cache(cfg);
	}
#endif /* ROAM_CHANNEL_CACHE */
#endif /* WL_DRV_AVOID_SCANCACHE */
	return err;
}

/* Note: This API should be invoked with scan_sync mutex
 * held so that scan_request data structures doesn't
 * get modified in between.
 */
struct wireless_dev *
wl_get_scan_wdev(struct bcm_cfg80211 *cfg)
{
	struct wireless_dev *wdev = NULL;

	if (!cfg) {
		WL_ERR(("cfg ptr null\n"));
		return NULL;
	}

	if (!cfg->scan_request && !cfg->sched_scan_req) {
		/* No scans in progress */
		WL_MEM(("no scan in progress \n"));
		return NULL;
	}

	if (cfg->scan_request) {
		wdev = GET_SCAN_WDEV(cfg->scan_request);
#ifdef WL_SCHED_SCAN
	} else if (cfg->sched_scan_req) {
		wdev = GET_SCHED_SCAN_WDEV(cfg->sched_scan_req);
#endif /* WL_SCHED_SCAN */
	} else {
		WL_MEM(("no scan in progress \n"));
	}

	return wdev;
}

static void _wl_cfgscan_cancel_scan(struct bcm_cfg80211 *cfg)
{
	struct wireless_dev *wdev = NULL;
	struct net_device *ndev = NULL;

	ndev = wl_cfg80211_get_remain_on_channel_ndev(cfg);
	if (wl_get_drv_status(cfg, REMAINING_ON_CHANNEL, ndev)) {
		/* Cancel P2P listen */
		if (cfg->p2p_supported && cfg->p2p) {
			wl_cfgp2p_set_p2p_mode(cfg, WL_P2P_DISC_ST_SCAN, 0, 0,
					wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE));
		}
	}

	if (!cfg->scan_request && !cfg->sched_scan_req) {
		/* No scans in progress */
		WL_INFORM_MEM(("No scan in progress\n"));
		return;
	}

	wdev = wl_get_scan_wdev(cfg);
	if (!wdev) {
		WL_ERR(("No wdev present\n"));
		return;
	}

	ndev = wdev_to_wlc_ndev(wdev, cfg);

	/* Check if any scan in progress only then abort */
	if (wl_get_drv_status_all(cfg, SCANNING)) {
		WL_MEM(("Called by %pS\n", CALL_SITE));
		wl_cfgscan_scan_abort(cfg);

		/* Indicate escan completion to upper layer */
		wl_notify_escan_complete(cfg, ndev, true);
	}
	WL_INFORM_MEM(("Scan aborted! \n"));
}

/* Wrapper function for cancel_scan with scan_sync mutex */
void wl_cfgscan_cancel_scan(struct bcm_cfg80211 *cfg)
{
	WL_MEM(("Called by %pS\n", CALL_SITE));
	mutex_lock(&cfg->scan_sync);
	_wl_cfgscan_cancel_scan(cfg);
	mutex_unlock(&cfg->scan_sync);
}

/* Use wl_cfgscan_cancel_scan function for scan abort, as this would do a FW abort
* followed by indication to upper layer, the current function wl_cfgscan_scan_abort, does
* only FW abort.
*/
void wl_cfgscan_scan_abort(struct bcm_cfg80211 *cfg)
{
	void *params = NULL;
	s32 params_size = 0;
	s32 err = BCME_OK;
	struct net_device *dev = bcmcfg_to_prmry_ndev(cfg);
	u32 channel, channel_num;

	/* Abort scan params only need space for 1 channel and 0 ssids */
	if (IS_SCAN_PARAMS_V3_V2(cfg)) {
		params_size = WL_SCAN_PARAMS_V3_FIXED_SIZE + (1 * sizeof(uint16));
	} else {
		params_size = WL_SCAN_PARAMS_V1_FIXED_SIZE + (1 * sizeof(uint16));
	}

	params = MALLOCZ(cfg->osh, params_size);
	if (params == NULL) {
		WL_ERR(("mem alloc failed (%d bytes)\n", params_size));
		return;
	}

	/* Use magic value of channel=-1 to abort scan */
	channel = htodchanspec(-1);
	channel_num = htod32((0 << WL_SCAN_PARAMS_NSSID_SHIFT) |
			(1 & WL_SCAN_PARAMS_COUNT_MASK));
	if (IS_SCAN_PARAMS_V3_V2(cfg)) {
		wl_scan_params_v3_t *params_v3 = (wl_scan_params_v3_t *)params;
		params_v3->channel_list[0] = channel;
		params_v3->channel_num = channel_num;
		params_v3->length = htod16(sizeof(wl_scan_params_v3_t));
	} else {
		wl_scan_params_v1_t *params_v1 = (wl_scan_params_v1_t *)params;
		params_v1->channel_list[0] = channel;
		params_v1->channel_num = channel_num;
	}
#ifdef DHD_SEND_HANG_ESCAN_SYNCID_MISMATCH
	cfg->escan_info.prev_escan_aborted = TRUE;
#endif /* DHD_SEND_HANG_ESCAN_SYNCID_MISMATCH */
	/* Do a scan abort to stop the driver's scan engine */
	err = wldev_ioctl_set(dev, WLC_SCAN, params, params_size);
	if (err < 0) {
		/* scan abort can fail if there is no outstanding scan */
		WL_ERR(("scan engine not aborted ret(%d)\n", err));
	}
	MFREE(cfg->osh, params, params_size);
#ifdef WLTDLS
	if (cfg->tdls_mgmt_frame) {
		MFREE(cfg->osh, cfg->tdls_mgmt_frame, cfg->tdls_mgmt_frame_len);
		cfg->tdls_mgmt_frame = NULL;
		cfg->tdls_mgmt_frame_len = 0;
	}
#endif /* WLTDLS */
}

static s32
wl_notify_escan_complete(struct bcm_cfg80211 *cfg,
	struct net_device *ndev, bool aborted)
{
	s32 err = BCME_OK;
	unsigned long flags;
	struct net_device *dev;
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);

	WL_DBG(("Enter \n"));
	BCM_REFERENCE(dhdp);

	if (!ndev) {
		WL_ERR(("ndev is null\n"));
		err = BCME_ERROR;
		goto out;
	}

	if (cfg->escan_info.ndev != ndev) {
		WL_ERR(("Outstanding scan req ndev not matching (%p:%p)\n",
			cfg->escan_info.ndev, ndev));
		err = BCME_ERROR;
		goto out;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) && \
	defined(SUPPORT_RANDOM_MAC_SCAN) && !defined(WL_USE_RANDOMIZED_SCAN)
	/* Disable scanmac if enabled */
	if (cfg->scanmac_enabled) {
		wl_cfg80211_scan_mac_disable(ndev);
	}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0) && defined(SUPPORT_RANDOM_MAC_SCAN) */
	if (cfg->scan_request) {
		dev = bcmcfg_to_prmry_ndev(cfg);
#if defined(WL_ENABLE_P2P_IF)
		if (cfg->scan_request->dev != cfg->p2p_net)
			dev = cfg->scan_request->dev;
#elif defined(WL_CFG80211_P2P_DEV_IF)
		if (cfg->scan_request->wdev->iftype != NL80211_IFTYPE_P2P_DEVICE)
			dev = cfg->scan_request->wdev->netdev;
#endif
	}
	else {
		WL_DBG(("cfg->scan_request is NULL. Internal scan scenario."
			"doing scan_abort for ndev %p primary %p",
			ndev, bcmcfg_to_prmry_ndev(cfg)));
		dev = ndev;
	}

	del_timer_sync(&cfg->scan_timeout);
	/* clear scan enq time on complete */
	CLR_TS(cfg, scan_enq);
	CLR_TS(cfg, scan_start);
#if defined(ESCAN_RESULT_PATCH)
	if (likely(cfg->scan_request)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0))
		if (aborted && cfg->p2p && p2p_scan(cfg) &&
			(cfg->scan_request->flags & NL80211_SCAN_FLAG_FLUSH)) {
			WL_ERR(("scan list is changed"));
			cfg->bss_list = wl_escan_get_buf(cfg, !aborted);
		} else
#endif
			cfg->bss_list = wl_escan_get_buf(cfg, aborted);

		wl_inform_bss(cfg);
	}
#endif /* ESCAN_RESULT_PATCH */

	WL_CFG_DRV_LOCK(&cfg->cfgdrv_lock, flags);
	if (likely(cfg->scan_request)) {
		WL_INFORM_MEM(("[%s] Report scan done.\n", dev->name));
		/* scan_sync mutex is already held */
		_wl_notify_scan_done(cfg, aborted);
		cfg->scan_request = NULL;
	}
	if (p2p_is_on(cfg))
		wl_clr_p2p_status(cfg, SCANNING);
	wl_clr_drv_status(cfg, SCANNING, dev);
	CLR_TS(cfg, scan_start);
	WL_CFG_DRV_UNLOCK(&cfg->cfgdrv_lock, flags);

#ifdef WL_SCHED_SCAN
	if (cfg->sched_scan_running && cfg->sched_scan_req) {
		struct wiphy *wiphy = cfg->sched_scan_req->wiphy;
		if (!aborted) {
			WL_INFORM_MEM(("[%s] Report sched scan done.\n", dev->name));
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
			cfg80211_sched_scan_results(wiphy,
					cfg->sched_scan_req->reqid);
#else
			cfg80211_sched_scan_results(wiphy);
#endif /* LINUX_VER > 4.11 */
		}

		DBG_EVENT_LOG(dhdp, WIFI_EVENT_DRIVER_PNO_SCAN_COMPLETE);
		/* Mark target scan as done */
		cfg->sched_scan_running = FALSE;

		if (cfg->bss_list && (cfg->bss_list->count == 0)) {
			WL_INFORM_MEM(("bss list empty. report sched_scan_stop\n"));
			wl_cfg80211_stop_pno(cfg,  bcmcfg_to_prmry_ndev(cfg));
			/* schedule the work to indicate sched scan stop to cfg layer */
			schedule_delayed_work(&cfg->sched_scan_stop_work, 0);
		}
	}
#endif /* WL_SCHED_SCAN */
	wake_up_interruptible(&dhdp->conf->event_complete);

#if defined(BCMDONGLEHOST)
	DHD_OS_SCAN_WAKE_UNLOCK((dhd_pub_t *)(cfg->pub));
	DHD_ENABLE_RUNTIME_PM((dhd_pub_t *)(cfg->pub));
#endif

#ifdef WL_SDO
	if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS) && !in_atomic()) {
		/* If it is in atomic, we probably have to wait till the
		 * next event or find someother way of invoking this.
		 */
		wl_cfg80211_resume_sdo(ndev, cfg);
	}
#endif

out:
	return err;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
void
wl_cfg80211_abort_scan(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	struct bcm_cfg80211 *cfg;

	WL_DBG(("Enter wl_cfg80211_abort_scan\n"));
	cfg = wiphy_priv(wdev->wiphy);

	/* Check if any scan in progress only then abort */
	if (wl_get_drv_status_all(cfg, SCANNING)) {
		wl_cfgscan_scan_abort(cfg);
		/* Only scan abort is issued here. As per the expectation of abort_scan
		* the status of abort is needed to be communicated using cfg80211_scan_done call.
		* Here we just issue abort request and let the scan complete path to indicate
		* abort to cfg80211 layer.
		*/
		WL_DBG(("wl_cfg80211_abort_scan: Scan abort issued to FW\n"));
	}
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) */

#if defined(DHCP_SCAN_SUPPRESS)
static void wl_cfg80211_scan_supp_timerfunc(ulong data)
{
	struct bcm_cfg80211 *cfg = (struct bcm_cfg80211 *)data;

	WL_DBG(("Enter \n"));
	schedule_work(&cfg->wlan_work);
}

int wl_cfg80211_scan_suppress(struct net_device *dev, int suppress)
{
	int ret = 0;
	struct wireless_dev *wdev;
	struct bcm_cfg80211 *cfg;
	if (!dev || ((suppress != 0) && (suppress != 1))) {
		ret = -EINVAL;
		goto exit;
	}
	wdev = ndev_to_wdev(dev);
	if (!wdev) {
		ret = -EINVAL;
		goto exit;
	}
	cfg = (struct bcm_cfg80211 *)wiphy_priv(wdev->wiphy);
	if (!cfg) {
		ret = -EINVAL;
		goto exit;
	}

	if (suppress == cfg->scan_suppressed) {
		WL_DBG(("No change in scan_suppress state. Ignoring cmd..\n"));
		return 0;
	}

	del_timer_sync(&cfg->scan_supp_timer);

	if ((ret = wldev_ioctl_set(dev, WLC_SET_SCANSUPPRESS,
		&suppress, sizeof(int))) < 0) {
		WL_ERR(("Scan suppress setting failed ret:%d \n", ret));
	} else {
		WL_DBG(("Scan suppress %s \n", suppress ? "Enabled" : "Disabled"));
		cfg->scan_suppressed = suppress;
	}

	/* If scan_suppress is set, Start a timer to monitor it (just incase) */
	if (cfg->scan_suppressed) {
		if (ret) {
			WL_ERR(("Retry scan_suppress reset at a later time \n"));
			mod_timer(&cfg->scan_supp_timer,
				jiffies + msecs_to_jiffies(WL_SCAN_SUPPRESS_RETRY));
		} else {
			WL_DBG(("Start wlan_timer to clear of scan_suppress \n"));
			mod_timer(&cfg->scan_supp_timer,
				jiffies + msecs_to_jiffies(WL_SCAN_SUPPRESS_TIMEOUT));
		}
	}
exit:
	return ret;
}
#endif /* DHCP_SCAN_SUPPRESS */

int wl_cfg80211_scan_stop(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev)
{
	int ret = 0;

	WL_TRACE(("Enter\n"));

	if (!cfg || !cfgdev) {
		return -EINVAL;
	}

	/* cancel scan and notify scan status */
	wl_cfgscan_cancel_scan(cfg);

	return ret;
}

/* This API is just meant as a wrapper for cfg80211_scan_done
 * API. This doesn't do state mgmt. For cancelling scan,
 * please use wl_cfgscan_cancel_scan API.
 */
static void
_wl_notify_scan_done(struct bcm_cfg80211 *cfg, bool aborted)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0))
	struct cfg80211_scan_info info;
#endif

	if (!cfg->scan_request) {
		return;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0))
	memset_s(&info, sizeof(struct cfg80211_scan_info), 0, sizeof(struct cfg80211_scan_info));
	info.aborted = aborted;
	cfg80211_scan_done(cfg->scan_request, &info);
#else
	cfg80211_scan_done(cfg->scan_request, aborted);
#endif
	cfg->scan_request = NULL;
}

#ifdef WL_DRV_AVOID_SCANCACHE
static u32 wl_p2p_find_peer_channel(struct bcm_cfg80211 *cfg, s32 status, wl_bss_info_v109_t *bi,
		u32 bi_length)
{
	u32 ret;
	u8 *p2p_dev_addr = NULL;

	ret = wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL);
	if (!ret) {
		return ret;
	}
	if (status == WLC_E_STATUS_PARTIAL) {
		p2p_dev_addr = wl_cfgp2p_retreive_p2p_dev_addr(bi, bi_length);
		if (p2p_dev_addr && !memcmp(p2p_dev_addr,
			cfg->afx_hdl->tx_dst_addr.octet, ETHER_ADDR_LEN)) {
			s32 channel = wf_chspec_ctlchan(
				wl_chspec_driver_to_host(bi->chanspec));

			if ((channel > MAXCHANNEL) || (channel <= 0)) {
				channel = WL_INVALID;
			} else {
				WL_ERR(("ACTION FRAME SCAN : Peer " MACDBG " found,"
					" channel : %d\n",
					MAC2STRDBG(cfg->afx_hdl->tx_dst_addr.octet),
					channel));
			}
			wl_clr_p2p_status(cfg, SCANNING);
			cfg->afx_hdl->peer_chan = channel;
			complete(&cfg->act_frm_scan);
		}
	} else {
		WL_INFORM_MEM(("ACTION FRAME SCAN DONE\n"));
		wl_clr_p2p_status(cfg, SCANNING);
		wl_clr_drv_status(cfg, SCANNING, cfg->afx_hdl->dev);
		if (cfg->afx_hdl->peer_chan == WL_INVALID)
			complete(&cfg->act_frm_scan);
	}

	return ret;
}

static s32 wl_escan_without_scan_cache(struct bcm_cfg80211 *cfg,
	wl_escan_result_v109_t *escan_result,
	struct net_device *ndev,
	const wl_event_msg_t *e, s32 status)
{
	s32 err = BCME_OK;
	wl_bss_info_v109_t *bi;
	u32 bi_length;
	bool aborted = false;
	bool fw_abort = false;
	bool notify_escan_complete = false;

	if (wl_escan_check_sync_id(cfg, status, escan_result->sync_id,
		cfg->escan_info.cur_sync_id) < 0) {
		goto exit;
	}

	if (!(status == WLC_E_STATUS_TIMEOUT) || !(status == WLC_E_STATUS_PARTIAL)) {
		cfg->escan_info.escan_state = WL_ESCAN_STATE_IDLE;
	}

	if ((likely(cfg->scan_request)) || (cfg->sched_scan_running)) {
		notify_escan_complete = true;
	}

	if (status == WLC_E_STATUS_PARTIAL) {
		WL_DBG(("WLC_E_STATUS_PARTIAL \n"));
		DBG_EVENT_LOG((dhd_pub_t *)cfg->pub, WIFI_EVENT_DRIVER_SCAN_RESULT_FOUND);
		if ((!escan_result) || (dtoh16(escan_result->bss_count) != 1)) {
			WL_ERR(("Invalid escan result (NULL pointer) or invalid bss_count\n"));
			goto exit;
		}

		bi = escan_result->bss_info;
		bi_length = dtoh32(bi->length);
		if ((!bi) ||
		(bi_length != (dtoh32(escan_result->buflen) - WL_ESCAN_RESULTS_V109_FIXED_SIZE))) {
			WL_ERR(("Invalid escan bss info (NULL pointer)"
				"or invalid bss_info length\n"));
			goto exit;
		}

		if (!(bcmcfg_to_wiphy(cfg)->interface_modes & BIT(NL80211_IFTYPE_ADHOC))) {
			if (dtoh16(bi->capability) & DOT11_CAP_IBSS) {
				WL_DBG(("Ignoring IBSS result\n"));
				goto exit;
			}
		}

		if (wl_p2p_find_peer_channel(cfg, status, bi, bi_length)) {
			goto exit;
		} else {
			if (scan_req_match(cfg)) {
				/* p2p scan && allow only probe response */
				if ((cfg->p2p->search_state != WL_P2P_DISC_ST_SCAN) &&
					(bi->flags & WL_BSS_FLAGS_FROM_BEACON))
					goto exit;
			}
#ifdef ROAM_CHANNEL_CACHE
			add_roam_cache(cfg, bi);
#endif /* ROAM_CHANNEL_CACHE */
			err = wl_inform_single_bss(cfg, bi, false);
#ifdef ROAM_CHANNEL_CACHE
			/* print_roam_cache(); */
			update_roam_cache(cfg, ioctl_version);
#endif /* ROAM_CHANNEL_CACHE */

			/*
			 * !Broadcast && number of ssid = 1 && number of channels =1
			 * means specific scan to association
			 */
			if (wl_cfgp2p_is_p2p_specific_scan(cfg->scan_request)) {
				WL_ERR(("P2P assoc scan fast aborted.\n"));
				aborted = false;
				fw_abort = true;
			}
			/* Directly exit from function here and
			* avoid sending notify completion to cfg80211
			*/
			goto exit;
		}
	} else if (status == WLC_E_STATUS_SUCCESS) {
		if (wl_p2p_find_peer_channel(cfg, status, NULL, 0)) {
			goto exit;
		}
		WL_INFORM_MEM(("ESCAN COMPLETED\n"));
		DBG_EVENT_LOG((dhd_pub_t *)cfg->pub, WIFI_EVENT_DRIVER_SCAN_COMPLETE);

		/* Update escan complete status */
		aborted = false;
		fw_abort = false;

	} else if ((status == WLC_E_STATUS_ABORT) || (status == WLC_E_STATUS_NEWSCAN) ||
		(status == WLC_E_STATUS_11HQUIET) || (status == WLC_E_STATUS_CS_ABORT) ||
		(status == WLC_E_STATUS_NEWASSOC)) {
		/* Handle all cases of scan abort */

		WL_INFORM_MEM(("ESCAN ABORT reason: %d\n", status));
		if (wl_p2p_find_peer_channel(cfg, status, NULL, 0)) {
			goto exit;
		}

		/* Update escan complete status */
		aborted = true;
		fw_abort = false;

	} else if (status == WLC_E_STATUS_TIMEOUT) {
		WL_ERR(("WLC_E_STATUS_TIMEOUT : scan_request[%p]\n", cfg->scan_request));
		WL_ERR(("reason[0x%x]\n", e->reason));
		if (e->reason == 0xFFFFFFFF) {
			/* Update escan complete status */
			aborted = true;
			fw_abort = true;
		}
	} else {
		WL_ERR(("unexpected Escan Event %d : abort\n", status));

		if (wl_p2p_find_peer_channel(cfg, status, NULL, 0)) {
			goto exit;
		}
		/* Update escan complete status */
		aborted = true;
		fw_abort = false;
	}

	/* Notify escan complete status */
	if (notify_escan_complete) {
		if (fw_abort == true) {
			wl_cfgscan_cancel_scan(cfg);
		} else {
			wl_notify_escan_complete(cfg, ndev, aborted);
		}
	}

exit:
	return err;

}
#endif /* WL_DRV_AVOID_SCANCACHE */

s32
wl_notify_scan_status(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	struct channel_info channel_inform;
	wl_scan_results_v109_t *bss_list;
	struct net_device *ndev = NULL;
	u32 len = WL_SCAN_BUF_MAX;
	s32 err = 0;
	unsigned long flags;

	WL_DBG(("Enter \n"));
	if (!wl_get_drv_status(cfg, SCANNING, ndev)) {
		WL_DBG(("scan is not ready \n"));
		return err;
	}
	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	mutex_lock(&cfg->scan_sync);
	wl_clr_drv_status(cfg, SCANNING, ndev);
	bzero(&channel_inform, sizeof(channel_inform));
	err = wldev_ioctl_get(ndev, WLC_GET_CHANNEL, &channel_inform,
		sizeof(channel_inform));
	if (unlikely(err)) {
		WL_ERR(("scan busy (%d)\n", err));
		goto scan_done_out;
	}
	channel_inform.scan_channel = dtoh32(channel_inform.scan_channel);
	if (unlikely(channel_inform.scan_channel)) {

		WL_DBG(("channel_inform.scan_channel (%d)\n",
			channel_inform.scan_channel));
	}
	cfg->bss_list = cfg->scan_results;
	bss_list = cfg->bss_list;
	bzero(bss_list, len);
	bss_list->buflen = htod32(len);
	err = wldev_ioctl_get(ndev, WLC_SCAN_RESULTS, bss_list, len);
	if (unlikely(err) && unlikely(!cfg->scan_suppressed)) {
		WL_ERR(("%s Scan_results error (%d)\n", ndev->name, err));
		err = -EINVAL;
		goto scan_done_out;
	}
	bss_list->buflen = dtoh32(bss_list->buflen);
	bss_list->version = dtoh32(bss_list->version);
	bss_list->count = dtoh32(bss_list->count);

	err = wl_inform_bss(cfg);

scan_done_out:
	del_timer_sync(&cfg->scan_timeout);
	WL_CFG_DRV_LOCK(&cfg->cfgdrv_lock, flags);
	if (cfg->scan_request) {
		_wl_notify_scan_done(cfg, false);
		cfg->scan_request = NULL;
	}
	WL_CFG_DRV_UNLOCK(&cfg->cfgdrv_lock, flags);
	WL_DBG(("cfg80211_scan_done\n"));
	mutex_unlock(&cfg->scan_sync);
	return err;
}

void wl_notify_scan_done(struct bcm_cfg80211 *cfg, bool aborted)
{
#if defined(CONFIG_TIZEN)
	struct net_device *ndev = NULL;
#endif /* CONFIG_TIZEN */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0))
	struct cfg80211_scan_info info;

	bzero(&info, sizeof(struct cfg80211_scan_info));
	info.aborted = aborted;
	cfg80211_scan_done(cfg->scan_request, &info);
#else
	cfg80211_scan_done(cfg->scan_request, aborted);
#endif

#if defined(CONFIG_TIZEN)
	ndev = bcmcfg_to_prmry_ndev(cfg);
	if (aborted)
		net_stat_tizen_update_wifi(ndev, WIFISTAT_SCAN_ABORT);
	else
		net_stat_tizen_update_wifi(ndev, WIFISTAT_SCAN_DONE);
#endif /* CONFIG_TIZEN */
}

#if defined(SUPPORT_RANDOM_MAC_SCAN)
int
wl_cfg80211_set_random_mac(struct net_device *dev, bool enable)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	int ret;

	if (cfg->random_mac_enabled == enable) {
		WL_ERR(("Random MAC already %s\n", enable ? "Enabled" : "Disabled"));
		return BCME_OK;
	}

	if (enable) {
		ret = wl_cfg80211_random_mac_enable(dev);
	} else {
		ret = wl_cfg80211_random_mac_disable(dev);
	}

	if (!ret) {
		cfg->random_mac_enabled = enable;
	}

	return ret;
}

int
wl_cfg80211_random_mac_enable(struct net_device *dev)
{
	u8 random_mac[ETH_ALEN] = {0, };
	u8 rand_bytes[3] = {0, };
	s32 err = BCME_ERROR;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
#if !defined(LEGACY_RANDOM_MAC)
	uint8 buffer[WLC_IOCTL_SMLEN] = {0, };
	wl_scanmac_t *sm = NULL;
	int len = 0;
	wl_scanmac_enable_t *sm_enable = NULL;
	wl_scanmac_config_t *sm_config = NULL;
#endif /* !LEGACY_RANDOM_MAC */

	if (wl_get_drv_status_all(cfg, CONNECTED) || wl_get_drv_status_all(cfg, CONNECTING) ||
	    wl_get_drv_status_all(cfg, AP_CREATED) || wl_get_drv_status_all(cfg, AP_CREATING)) {
		WL_ERR(("fail to Set random mac, current state is wrong\n"));
		return err;
	}

	(void)memcpy_s(random_mac, ETH_ALEN, bcmcfg_to_prmry_ndev(cfg)->dev_addr, ETH_ALEN);
	get_random_bytes(&rand_bytes, sizeof(rand_bytes));

	if (rand_bytes[2] == 0x0 || rand_bytes[2] == 0xff) {
		rand_bytes[2] = 0xf0;
	}

#if defined(LEGACY_RANDOM_MAC)
	/* of the six bytes of random_mac the bytes 3, 4, 5 are copied with contents of rand_bytes
	* So while copying 3 bytes of content no overflow would be seen. Hence returning void.
	*/
	(void)memcpy_s(&random_mac[3], (sizeof(u8) * 3), rand_bytes, sizeof(rand_bytes));

	err = wldev_iovar_setbuf_bsscfg(bcmcfg_to_prmry_ndev(cfg), "cur_etheraddr",
		random_mac, ETH_ALEN, cfg->ioctl_buf, WLC_IOCTL_SMLEN, 0, &cfg->ioctl_buf_sync);

	if (err != BCME_OK) {
		WL_ERR(("failed to set random generate MAC address\n"));
	} else {
		WL_ERR(("set mac " MACDBG " to " MACDBG "\n",
			MAC2STRDBG((const u8 *)bcmcfg_to_prmry_ndev(cfg)->dev_addr),
			MAC2STRDBG((const u8 *)&random_mac)));
		WL_ERR(("random MAC enable done"));
	}
#else
	/* Enable scan mac */
	sm = (wl_scanmac_t *)buffer;
	sm_enable = (wl_scanmac_enable_t *)sm->data;
	sm->len = sizeof(*sm_enable);
	sm_enable->enable = 1;
	len = OFFSETOF(wl_scanmac_t, data) + sm->len;
	sm->subcmd_id = WL_SCANMAC_SUBCMD_ENABLE;

	err = wldev_iovar_setbuf_bsscfg(dev, "scanmac",
		sm, len, cfg->ioctl_buf, WLC_IOCTL_SMLEN, 0, &cfg->ioctl_buf_sync);

	/* For older chip which which does not have scanmac support can still use
	 * cur_etheraddr to set the randmac. rand_mask and rand_mac comes from upper
	 * cfg80211 layer. If rand_mask and rand_mac is not passed then fallback
	 * to default cur_etheraddr and default mask.
	 */
	if (err == BCME_UNSUPPORTED) {
		/* In case of host based legacy randomization, random address is
		 * generated by mixing 3 bytes of cur_etheraddr and 3 bytes of
		 * random bytes generated.In that case rand_mask is nothing but
		 * random bytes.
		 */
		(void)memcpy_s(&random_mac[3], (sizeof(u8) * 3), rand_bytes, sizeof(rand_bytes));
		err = wldev_iovar_setbuf_bsscfg(bcmcfg_to_prmry_ndev(cfg), "cur_etheraddr",
				random_mac, ETH_ALEN, cfg->ioctl_buf,
				WLC_IOCTL_SMLEN, 0, &cfg->ioctl_buf_sync);
		if (err != BCME_OK) {
			WL_ERR(("failed to set random generate MAC address\n"));
		} else {
			WL_ERR(("set mac " MACDBG " to " MACDBG "\n",
				MAC2STRDBG((const u8 *)bcmcfg_to_prmry_ndev(cfg)->dev_addr),
				MAC2STRDBG((const u8 *)&random_mac)));
			WL_ERR(("random MAC enable done using legacy randmac"));
		}
	} else if (err == BCME_OK) {
		/* Configure scanmac */
		(void)memset_s(buffer, sizeof(buffer), 0x0, sizeof(buffer));
		sm_config = (wl_scanmac_config_t *)sm->data;
		sm->len = sizeof(*sm_config);
		sm->subcmd_id = WL_SCANMAC_SUBCMD_CONFIG;
		sm_config->scan_bitmap = WL_SCANMAC_SCAN_UNASSOC;

		/* Set randomize mac address recv from upper layer */
		(void)memcpy_s(&sm_config->mac.octet, ETH_ALEN, random_mac, ETH_ALEN);

		/* Set randomize mask recv from upper layer */

		/* Currently in samsung case, upper layer does not provide
		 * variable randmask and its using fixed 3 byte randomization
		 */
		(void)memset_s(&sm_config->random_mask.octet, ETH_ALEN, 0x0, ETH_ALEN);
		/* Memsetting the remaining octets 3, 4, 5. So remaining dest length is 3 */
		(void)memset_s(&sm_config->random_mask.octet[3], 3, 0xFF, 3);

		WL_DBG(("recv random mac addr " MACDBG  " recv rand mask" MACDBG "\n",
			MAC2STRDBG((const u8 *)&sm_config->mac.octet),
			MAC2STRDBG((const u8 *)&sm_config->random_mask)));

		len = OFFSETOF(wl_scanmac_t, data) + sm->len;
		err = wldev_iovar_setbuf_bsscfg(dev, "scanmac",
			sm, len, cfg->ioctl_buf, WLC_IOCTL_SMLEN, 0, &cfg->ioctl_buf_sync);

		if (err != BCME_OK) {
			WL_ERR(("failed scanmac configuration\n"));

			/* Disable scan mac for clean-up */
			wl_cfg80211_random_mac_disable(dev);
			return err;
		}
		WL_DBG(("random MAC enable done using scanmac"));
	} else  {
		WL_ERR(("failed to enable scanmac, err=%d\n", err));
	}
#endif /* LEGACY_RANDOM_MAC */

	return err;
}

int
wl_cfg80211_random_mac_disable(struct net_device *dev)
{
	s32 err = BCME_ERROR;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
#if !defined(LEGACY_RANDOM_MAC)
	uint8 buffer[WLC_IOCTL_SMLEN] = {0, };
	wl_scanmac_t *sm = NULL;
	int len = 0;
	wl_scanmac_enable_t *sm_enable = NULL;
#endif /* !LEGACY_RANDOM_MAC */

#if defined(LEGACY_RANDOM_MAC)
	WL_ERR(("set original mac " MACDBG "\n",
		MAC2STRDBG((const u8 *)bcmcfg_to_prmry_ndev(cfg)->dev_addr)));

	err = wldev_iovar_setbuf_bsscfg(bcmcfg_to_prmry_ndev(cfg), "cur_etheraddr",
		bcmcfg_to_prmry_ndev(cfg)->dev_addr, ETH_ALEN,
		cfg->ioctl_buf, WLC_IOCTL_SMLEN, 0, &cfg->ioctl_buf_sync);

	if (err != BCME_OK) {
		WL_ERR(("failed to set original MAC address\n"));
	} else {
		WL_ERR(("legacy random MAC disable done \n"));
	}
#else
	sm = (wl_scanmac_t *)buffer;
	sm_enable = (wl_scanmac_enable_t *)sm->data;
	sm->len = sizeof(*sm_enable);
	/* Disable scanmac */
	sm_enable->enable = 0;
	len = OFFSETOF(wl_scanmac_t, data) + sm->len;

	sm->subcmd_id = WL_SCANMAC_SUBCMD_ENABLE;

	err = wldev_iovar_setbuf_bsscfg(dev, "scanmac",
		sm, len, cfg->ioctl_buf, WLC_IOCTL_SMLEN, 0, &cfg->ioctl_buf_sync);

	if (err != BCME_OK) {
		WL_ERR(("failed to disable scanmac, err=%d\n", err));
		return err;
	}
	/* Clear scanmac enabled status */
	cfg->scanmac_enabled = 0;
	WL_DBG(("random MAC disable done\n"));
#endif /* LEGACY_RANDOM_MAC */

	return err;
}

int wl_cfg80211_scan_mac_enable(struct net_device *dev)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	s32 err = BCME_ERROR;
	uint8 buffer[WLC_IOCTL_SMLEN] = {0};
	wl_scanmac_t *sm = NULL;
	int len = 0;
	wl_scanmac_enable_t *sm_enable = NULL;

	/* Enable scan mac */
	sm = (wl_scanmac_t *)buffer;
	sm_enable = (wl_scanmac_enable_t *)sm->data;
	sm->len = sizeof(*sm_enable);
	sm_enable->enable = 1;
	len = OFFSETOF(wl_scanmac_t, data) + sm->len;
	sm->subcmd_id = WL_SCANMAC_SUBCMD_ENABLE;

	err = wldev_iovar_setbuf_bsscfg(dev, "scanmac",
		sm, len, cfg->ioctl_buf, WLC_IOCTL_SMLEN, 0, &cfg->ioctl_buf_sync);
	if (unlikely(err)) {
		WL_ERR(("scanmac enable failed\n"));
	} else {
		/* Mark scanmac configured */
		cfg->scanmac_enabled = 1;
	}

	return err;
}

/*
 * This is new interface for mac randomization. It takes randmac and randmask
 * as arg and it uses scanmac iovar to offload the mac randomization to firmware.
 */
int wl_cfg80211_scan_mac_config(struct net_device *dev, uint8 *rand_mac, uint8 *rand_mask)
{
	int byte_index = 0;
	s32 err = BCME_ERROR;
	uint8 buffer[WLC_IOCTL_SMLEN] = {0};
	wl_scanmac_t *sm = NULL;
	int len = 0;
	wl_scanmac_config_t *sm_config = NULL;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	uint8 random_mask_46_bits[ETHER_ADDR_LEN] = {0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	if (rand_mac == NULL) {
		err = BCME_BADARG;
		WL_ERR(("fail to Set random mac, bad argument\n"));
		/* Disable the current scanmac config */
		return err;
	}

	if (ETHER_ISNULLADDR(rand_mac)) {
		WL_DBG(("fail to Set random mac, Invalid rand mac\n"));
		/* Disable the current scanmac config */
		return err;
	}

	/* Configure scanmac */
	(void)memset_s(buffer, sizeof(buffer), 0x0, sizeof(buffer));
	sm = (wl_scanmac_t *)buffer;
	sm_config = (wl_scanmac_config_t *)sm->data;
	sm->len = sizeof(*sm_config);
	sm->subcmd_id = WL_SCANMAC_SUBCMD_CONFIG;
	sm_config->scan_bitmap = WL_SCANMAC_SCAN_UNASSOC;
#ifdef WL_USE_RANDOMIZED_SCAN
	sm_config->scan_bitmap |= WL_SCANMAC_SCAN_ASSOC_HOST;
#endif /* WL_USE_RANDOMIZED_SCAN */
	/* Set randomize mac address recv from upper layer */
	(void)memcpy_s(&sm_config->mac.octet, ETH_ALEN, rand_mac, ETH_ALEN);

	/* Set randomize mask recv from upper layer */

	/* There is a difference in how to interpret rand_mask between
	 * upperlayer and firmware. If the byte is set as FF then for
	 * upper layer it  means keep that byte and do not randomize whereas
	 * for firmware it means randomize those bytes and vice versa. Hence
	 * conversion is needed before setting the iovar
	 */
	(void)memset_s(&sm_config->random_mask.octet, ETH_ALEN, 0x0, ETH_ALEN);
	/* Only byte randomization is supported currently. If mask recv is 0x0F
	 * for a particular byte then it will be treated as no randomization
	 * for that byte.
	 */
	if (!rand_mask) {
		/* If rand_mask not provided, use 46_bits_mask */
		(void)memcpy_s(&sm_config->random_mask.octet, ETH_ALEN,
			random_mask_46_bits, ETH_ALEN);
	} else {
		while (byte_index < ETH_ALEN) {
			if (rand_mask[byte_index] == 0xFF) {
				sm_config->random_mask.octet[byte_index] = 0x00;
			} else if (rand_mask[byte_index] == 0x00) {
				sm_config->random_mask.octet[byte_index] = 0xFF;
			}
			byte_index++;
		}
	}

	WL_DBG(("recv random mac addr " MACDBG  "recv rand mask" MACDBG "\n",
		MAC2STRDBG((const u8 *)&sm_config->mac.octet),
		MAC2STRDBG((const u8 *)&sm_config->random_mask)));

	len = OFFSETOF(wl_scanmac_t, data) + sm->len;
	err = wldev_iovar_setbuf_bsscfg(dev, "scanmac",
		sm, len, cfg->ioctl_buf, WLC_IOCTL_SMLEN, 0, &cfg->ioctl_buf_sync);

	if (err != BCME_OK) {
		WL_ERR(("failed scanmac configuration\n"));

		/* Disable scan mac for clean-up */
		return err;
	}
	WL_INFORM_MEM(("scanmac configured"));
	cfg->scanmac_config = true;

	return err;
}

int
wl_cfg80211_scan_mac_disable(struct net_device *dev)
{
	s32 err = BCME_ERROR;

	err = wl_cfg80211_random_mac_disable(dev);

	return err;
}
#endif /* SUPPORT_RANDOM_MAC_SCAN */

#ifdef WL_SCHED_SCAN
#define PNO_TIME                    30u
#define PNO_REPEAT_MAX              100u
#define PNO_FREQ_EXPO_MAX           2u
#define PNO_ADAPTIVE_SCAN_LIMIT     80u
#define ADP_PNO_REPEAT_DEFAULT      2u
static bool
is_ssid_in_list(struct cfg80211_ssid *ssid, struct cfg80211_ssid *ssid_list, int count)
{
	int i;

	if (!ssid || !ssid_list)
		return FALSE;

	for (i = 0; i < count; i++) {
		if (ssid->ssid_len == ssid_list[i].ssid_len) {
			if (strncmp(ssid->ssid, ssid_list[i].ssid, ssid->ssid_len) == 0)
				return TRUE;
		}
	}
	return FALSE;
}

int
wl_cfg80211_sched_scan_start(struct wiphy *wiphy,
                             struct net_device *dev,
                             struct cfg80211_sched_scan_request *request)
{
	u16 chan_list[WL_NUMCHANNELS] = {0};
	u32 num_channels = 0;
	ushort pno_time = 0;
	int pno_repeat = 0;
	int pno_freq_expo_max = 0;
	wlc_ssid_ext_t *ssids_local = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);
	struct cfg80211_ssid *ssid = NULL;
	struct cfg80211_ssid *hidden_ssid_list = NULL;
	log_conn_event_t *event_data = NULL;
	tlv_log *tlv_data = NULL;
	u32 alloc_len = 0;
	u32 payload_len;
	int ssid_cnt = 0;
	int i;
	int ret = 0;
	unsigned long flags;
	bool adaptive_pno = false;

#ifdef WL_DUAL_STA
	if (!IS_INET_LINK_NDEV(cfg, dev)) {
		WL_ERR(("Sched scan not supported on non-INET link\n"));
		return -EOPNOTSUPP;
	}
	if ((wl_cfgvif_get_iftype_count(cfg, WL_IF_TYPE_STA) >= 2) || cfg->latency_mode) {
		WL_ERR(("Sched scan not supported in multi sta connected state"
			" or latency mode %d\n", cfg->latency_mode));
		return -EOPNOTSUPP;
	}
#endif /* WL_DUAL_STA */
	if (!request) {
		WL_ERR(("Sched scan request was NULL\n"));
		return -EINVAL;
	}

	if (!(request->n_scan_plans == 1) || !request->scan_plans) {
		WL_ERR(("wrong input. plans:%d\n", request->n_scan_plans));
		return -EINVAL;
	}

#ifndef DISABLE_ADAPTIVE_PNO
	if (request->scan_plans->interval <= PNO_ADAPTIVE_SCAN_LIMIT) {
		/* If the host gives a high value for scan interval, then
		 * doing adaptive scan doesn't make sense. For smaller
		 * values attempt adaptive scan.
		 */
		adaptive_pno = true;
	}
#endif /* DISABLE_ADAPTIVE_PNO */

	/* Use host provided iterations */
	pno_repeat = request->scan_plans->iterations;
	if (!pno_repeat || pno_repeat > PNO_REPEAT_MAX) {
		/* (scan_plan->iterations == zero) means infinite */
		pno_repeat = PNO_REPEAT_MAX;
	}

	if (adaptive_pno) {
		/* Run adaptive PNO */
		pno_time = request->scan_plans->interval;
		pno_freq_expo_max = PNO_FREQ_EXPO_MAX;
		pno_repeat = ADP_PNO_REPEAT_DEFAULT;
	} else {
		/* use host provided values */
		pno_time = request->scan_plans->interval;
	}

	if (!request->n_ssids || !request->n_match_sets) {
		WL_ERR(("Invalid sched scan req!! n_ssids:%d, request->n_match_sets:%d \n",
				request->n_ssids, request->n_match_sets));
		return -EINVAL;
	}

	mutex_lock(&cfg->scan_sync);

	WL_INFORM_MEM(("[%s] Enter. ssids:%d match_sets:%d pno_time:%d pno_repeat:%d "
		"channels:%d adaptive:%d\n", dev->name, request->n_ssids, request->n_match_sets,
		pno_time, pno_repeat, request->n_channels, adaptive_pno));

	ssids_local = (wlc_ssid_ext_t *)MALLOCZ(cfg->osh,
		sizeof(wlc_ssid_ext_t) * MAX_PFN_LIST_COUNT);
	if (!ssids_local) {
		WL_ERR(("No memory"));
		ret = -ENOMEM;
		goto exit;
	}

	if (request->n_ssids > 0) {
		hidden_ssid_list = request->ssids;
	}

	if (request->n_channels && request->n_channels < WL_NUMCHANNELS) {
		/* get channel list. Note PNO uses channels and not chanspecs */
		wl_cfgscan_populate_scan_channels(cfg,
				request->channels, request->n_channels,
				chan_list, &num_channels, true, false);
	}

	if (DBG_RING_ACTIVE(dhdp, DHD_EVENT_RING_ID)) {
		alloc_len = sizeof(log_conn_event_t) + sizeof(tlv_log) + DOT11_MAX_SSID_LEN;
		event_data = (log_conn_event_t *)MALLOCZ(cfg->osh, alloc_len);
		if (!event_data) {
			WL_ERR(("%s: failed to allocate log_conn_event_t with "
						"length(%d)\n", __func__, alloc_len));
			ret = -ENOMEM;
			goto exit;
		}
	}
	for (i = 0; i < request->n_match_sets && ssid_cnt < MAX_PFN_LIST_COUNT; i++) {
		ssid = &request->match_sets[i].ssid;
		/* No need to include null ssid */
		if (ssid->ssid_len) {
			ssids_local[ssid_cnt].SSID_len = MIN(ssid->ssid_len,
				(uint32)DOT11_MAX_SSID_LEN);
			/* In previous step max SSID_len is limited to DOT11_MAX_SSID_LEN,
			* returning void
			*/
			(void)memcpy_s(ssids_local[ssid_cnt].SSID, DOT11_MAX_SSID_LEN, ssid->ssid,
				ssids_local[ssid_cnt].SSID_len);
			if (is_ssid_in_list(ssid, hidden_ssid_list, request->n_ssids)) {
				ssids_local[ssid_cnt].hidden = TRUE;
				WL_PNO((">>> PNO hidden SSID (%s) \n", ssid->ssid));
			} else {
				ssids_local[ssid_cnt].hidden = FALSE;
				WL_PNO((">>> PNO non-hidden SSID (%s) \n", ssid->ssid));
			}
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
			if (request->match_sets[i].rssi_thold != NL80211_SCAN_RSSI_THOLD_OFF) {
				ssids_local[ssid_cnt].rssi_thresh =
				      (int8)request->match_sets[i].rssi_thold;
			}
#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0)) */
			ssid_cnt++;
		}
	}

	if (ssid_cnt) {
#if defined(BCMDONGLEHOST)
		if ((ret = dhd_dev_pno_set_for_ssid(dev, ssids_local, ssid_cnt,
			pno_time, pno_repeat, pno_freq_expo_max,
			(num_channels ? chan_list : NULL), num_channels)) < 0) {
			WL_ERR(("PNO setup failed!! ret=%d \n", ret));
			ret = -EINVAL;
			goto exit;
		}
#endif /* BCMDONGLEHOST */

		if (DBG_RING_ACTIVE(dhdp, DHD_EVENT_RING_ID)) {
			/*
			 * purposefully logging here to make sure that
			 * firmware configuration was successful
			 */
			for (i = 0; i < ssid_cnt; i++) {
				payload_len = sizeof(log_conn_event_t);
				event_data->event = WIFI_EVENT_DRIVER_PNO_ADD;
				tlv_data = event_data->tlvs;
				/* ssid */
				tlv_data->tag = WIFI_TAG_SSID;
				tlv_data->len = ssids_local[i].SSID_len;
				(void)memcpy_s(tlv_data->value, DOT11_MAX_SSID_LEN,
					ssids_local[i].SSID, ssids_local[i].SSID_len);
				payload_len += TLV_LOG_SIZE(tlv_data);

				dhd_os_push_push_ring_data(dhdp, DHD_EVENT_RING_ID,
					event_data, payload_len);
			}
		}

		WL_CFG_DRV_LOCK(&cfg->cfgdrv_lock, flags);
		cfg->sched_scan_req = request;
		WL_CFG_DRV_UNLOCK(&cfg->cfgdrv_lock, flags);
	} else {
		ret = -EINVAL;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) && \
	defined(SUPPORT_RANDOM_MAC_SCAN)
	if ((ret = wl_config_scan_macaddr(cfg, dev,
		(request->flags & NL80211_SCAN_FLAG_RANDOM_ADDR),
		request->mac_addr, request->mac_addr_mask)) != BCME_OK) {
		WL_ERR(("scanmac addr config failed\n"));
		/* Cleanup the states and stop the pno */
		if (dhd_dev_pno_stop_for_ssid(dev) < 0) {
			WL_ERR(("PNO Stop for SSID failed"));
		}
		WL_CFG_DRV_LOCK(&cfg->cfgdrv_lock, flags);
		cfg->sched_scan_req = NULL;
		cfg->sched_scan_running = FALSE;
		WL_CFG_DRV_UNLOCK(&cfg->cfgdrv_lock, flags);
		goto exit;
	}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) && (defined(SUPPORT_RANDOM_MAC_SCAN)) */

exit:
	if (ssids_local) {
		MFREE(cfg->osh, ssids_local,
			sizeof(wlc_ssid_ext_t) * MAX_PFN_LIST_COUNT);
	}
	if (event_data) {
		MFREE(cfg->osh, event_data, alloc_len);
	}
	mutex_unlock(&cfg->scan_sync);
	return ret;
}

void
wl_cfg80211_stop_pno(struct bcm_cfg80211 *cfg, struct net_device *dev)
{
#if defined(BCMDONGLEHOST)
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);

	if (dhd_dev_pno_stop_for_ssid(dev) < 0) {
		WL_ERR(("PNO Stop for SSID failed"));
	} else {
		/*
		 * purposefully logging here to make sure that
		 * firmware configuration was successful
		 */
		UNUSED_PARAMETER(dhdp);
		DBG_EVENT_LOG(dhdp, WIFI_EVENT_DRIVER_PNO_REMOVE);
	}
#endif /* BCMDONGLEHOST */
}

int
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 0))
wl_cfg80211_sched_scan_stop(struct wiphy *wiphy, struct net_device *dev, u64 reqid)
#else
wl_cfg80211_sched_scan_stop(struct wiphy *wiphy, struct net_device *dev)
#endif /* LINUX_VER > 4.11 */
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *pri_ndev;

	WL_INFORM_MEM(("[%s] Enter\n", dev->name));
	pri_ndev = bcmcfg_to_prmry_ndev(cfg);
	wl_cfg80211_stop_pno(cfg, dev);

	cancel_delayed_work(&cfg->sched_scan_stop_work);

	mutex_lock(&cfg->scan_sync);
	if (cfg->sched_scan_req) {
		if (cfg->sched_scan_running && wl_get_drv_status(cfg, SCANNING, pri_ndev)) {
			/* If targetted escan for PNO is running, abort it */
			WL_INFORM_MEM(("abort targetted escan\n"));
			_wl_cfgscan_cancel_scan(cfg);
			wl_clr_drv_status(cfg, SCANNING, pri_ndev);
		} else {
			WL_INFORM_MEM(("pno escan state:%d\n",
				cfg->sched_scan_running));
		}
	}
	cfg->sched_scan_req = NULL;
	cfg->sched_scan_running = FALSE;
	mutex_unlock(&cfg->scan_sync);

	return 0;
}

void
wl_cfgscan_sched_scan_stop_work(struct work_struct *work)
{
	struct bcm_cfg80211 *cfg = NULL;
	struct wiphy *wiphy = NULL;
	struct delayed_work *dw = to_delayed_work(work);

	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	cfg = container_of(dw, struct bcm_cfg80211, sched_scan_stop_work);
	GCC_DIAGNOSTIC_POP();

	if (cfg->sched_scan_req) {
	/* Hold rtnl_lock -> scan_sync lock to be in sync with cfg80211_ops path */
		wiphy = cfg->sched_scan_req->wiphy;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0))
		wiphy_lock(wiphy);
#else
		rtnl_lock();
#endif /* KERNEL > 5.12.0 */
		mutex_lock(&cfg->scan_sync);

		/* Indicate sched scan stopped so that user space
		 * can do a full scan incase found match is empty.
		 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0))
		cfg80211_sched_scan_stopped_locked(wiphy, cfg->sched_scan_req->reqid);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
		cfg80211_sched_scan_stopped_rtnl(wiphy, cfg->sched_scan_req->reqid);
#else
		cfg80211_sched_scan_stopped_rtnl(wiphy);
#endif /* KERNEL > 5.12.0 */
		cfg->sched_scan_req = NULL;
		mutex_unlock(&cfg->scan_sync);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0))
		wiphy_unlock(wiphy);
#else
		rtnl_unlock();
#endif /* KERNEL > 5.12.0 */
	}
}
#endif /* WL_SCHED_SCAN */

#ifdef WES_SUPPORT
#ifdef CUSTOMER_SCAN_TIMEOUT_SETTING
s32 wl_cfg80211_custom_scan_time(struct net_device *dev,
		enum wl_custom_scan_time_type type, int time)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	if (cfg == NULL) {
		return FALSE;
	}

	switch (type) {
		case WL_CUSTOM_SCAN_CHANNEL_TIME :
			WL_ERR(("Scan Channel Time %d\n", time));
			cfg->custom_scan_channel_time = time;
			break;
		case WL_CUSTOM_SCAN_UNASSOC_TIME :
			WL_ERR(("Scan Unassoc Time %d\n", time));
			cfg->custom_scan_unassoc_time = time;
			break;
		case WL_CUSTOM_SCAN_PASSIVE_TIME :
			WL_ERR(("Scan Passive Time %d\n", time));
			cfg->custom_scan_passive_time = time;
			break;
		case WL_CUSTOM_SCAN_HOME_TIME :
			WL_ERR(("Scan Home Time %d\n", time));
			cfg->custom_scan_home_time = time;
			break;
		case WL_CUSTOM_SCAN_HOME_AWAY_TIME :
			WL_ERR(("Scan Home Away Time %d\n", time));
			cfg->custom_scan_home_away_time = time;
			break;
		default:
			return FALSE;
	}
	return TRUE;
}
#endif /* CUSTOMER_SCAN_TIMEOUT_SETTING */
#endif /* WES_SUPPORT */

static void wl_scan_timeout(unsigned long data)
{
	wl_event_msg_t msg;
	struct bcm_cfg80211 *cfg = (struct bcm_cfg80211 *)data;
	struct wireless_dev *wdev = NULL;
	struct net_device *ndev = NULL;
#if 0
	wl_scan_results_v109_t *bss_list;
	wl_bss_info_v109_t *bi = NULL;
	s32 i;
	u32 channel;
#endif
	u64 cur_time = OSL_LOCALTIME_NS();
#ifdef BCMDONGLEHOST
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);
#endif /* BCMDONGLEHOST */
	unsigned long flags;
#ifdef RTT_SUPPORT
	rtt_status_info_t *rtt_status = NULL;
	UNUSED_PARAMETER(rtt_status);
#endif /* RTT_SUPPORT */

	UNUSED_PARAMETER(cur_time);
	WL_CFG_DRV_LOCK(&cfg->cfgdrv_lock, flags);
	if (!(cfg->scan_request)) {
		WL_ERR(("timer expired but no scan request\n"));
		WL_CFG_DRV_UNLOCK(&cfg->cfgdrv_lock, flags);
		return;
	}

	wdev = GET_SCAN_WDEV(cfg->scan_request);
	WL_CFG_DRV_UNLOCK(&cfg->cfgdrv_lock, flags);

	if (!wdev) {
		WL_ERR(("No wireless_dev present\n"));
		return;
	}

#ifdef BCMDONGLEHOST
	if (dhd_query_bus_erros(dhdp)) {
		return;
	}

#if defined(DHD_KERNEL_SCHED_DEBUG) && defined(DHD_FW_COREDUMP)
	if (dhdp->memdump_enabled) {
		dhdp->hang_reason = HANG_REASON_SCAN_TIMEOUT;
	}
	/* DHD triggers Kernel panic if the SCAN timeout occurrs
	 * due to tasklet or workqueue scheduling problems in the Linux Kernel.
	 * Customer informs that it is hard to find any clue from the
	 * host memory dump since the important tasklet or workqueue information
	 * is already disappered due the latency while printing out the timestamp
	 * logs for debugging scan timeout issue.
	 * For this reason, customer requestes us to trigger Kernel Panic rather than
	 * taking a SOCRAM dump.
	 */
	if ((cfg->tsinfo.scan_deq < cfg->tsinfo.scan_enq) ||
		dhd_bus_query_dpc_sched_errors(dhdp) ||
		(dhdp->dhd_induce_error == DHD_INDUCE_SCAN_TIMEOUT_SCHED_ERROR)) {
		WL_ERR(("****SCAN event timeout due to scheduling problem\n"));
#ifdef RTT_SUPPORT
		rtt_status = GET_RTTSTATE(dhdp);
#endif /* RTT_SUPPORT */
		WL_ERR(("***SCAN event timeout. WQ state:0x%x scan_enq_time:"SEC_USEC_FMT
			" evt_hdlr_entry_time:"SEC_USEC_FMT" evt_deq_time:"SEC_USEC_FMT
			"\nscan_deq_time:"SEC_USEC_FMT" scan_hdlr_cmplt_time:"SEC_USEC_FMT
			" scan_cmplt_time:"SEC_USEC_FMT" evt_hdlr_exit_time:"SEC_USEC_FMT
			"\ncurrent_time:"SEC_USEC_FMT"\n", work_busy(&cfg->event_work),
			GET_SEC_USEC(cfg->tsinfo.scan_enq),
			GET_SEC_USEC(cfg->tsinfo.wl_evt_hdlr_entry),
			GET_SEC_USEC(cfg->tsinfo.wl_evt_deq),
			GET_SEC_USEC(cfg->tsinfo.scan_deq),
			GET_SEC_USEC(cfg->tsinfo.scan_hdlr_cmplt),
			GET_SEC_USEC(cfg->tsinfo.scan_cmplt),
			GET_SEC_USEC(cfg->tsinfo.wl_evt_hdlr_exit), GET_SEC_USEC(cur_time)));
		if (cfg->tsinfo.scan_enq) {
			WL_ERR(("Elapsed time(ns): %llu\n", (cur_time - cfg->tsinfo.scan_enq)));
		}
		WL_ERR(("lock_states:[%d:%d:%d:%d:%d:%d]\n",
			mutex_is_locked(&cfg->if_sync),
			mutex_is_locked(&cfg->usr_sync),
			mutex_is_locked(&cfg->pm_sync),
			mutex_is_locked(&cfg->scan_sync),
			spin_is_locked(&cfg->cfgdrv_lock),
			spin_is_locked(&cfg->eq_lock)));
#ifdef RTT_SUPPORT
		WL_ERR(("RTT lock_state:[%d]\n",
			mutex_is_locked(&rtt_status->rtt_mutex)));
#ifdef WL_NAN
		WL_ERR(("RTT and Geofence lock_states:[%d:%d]\n",
			mutex_is_locked(&cfg->nancfg->nan_sync),
			mutex_is_locked(&(rtt_status)->geofence_mutex)));
#endif /* WL_NAN */
#endif /* RTT_SUPPORT */
		if (dhdp->memdump_enabled == DUMP_MEMFILE_BUGON) {
			/* change g_assert_type to trigger Kernel panic */
			g_assert_type = 2;
			/* use ASSERT() to trigger panic */
			ASSERT(0);
		}
		if (dhdp->memdump_enabled) {
			dhdp->hang_reason = HANG_REASON_SCAN_TIMEOUT_SCHED_ERROR;
		}
	}
#endif /* DHD_KERNEL_SCHED_DEBUG && DHD_FW_COREDUMP */
	dhd_bus_intr_count_dump(dhdp);
#endif /* BCMDONGLEHOST */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)) && !defined(CONFIG_MODULES)
	/* Print WQ states. Enable only for in-built drivers as the symbol is not exported  */
	show_workqueue_state();
#endif /* LINUX_VER >= 4.1 && !CONFIG_MODULES */

#if 0
	bss_list = wl_escan_get_buf(cfg, FALSE);
	if (!bss_list) {
		WL_ERR(("bss_list is null. Didn't receive any partial scan results\n"));
	} else {
		WL_ERR(("Dump scan buffer:\n"
			"scanned AP count (%d)\n", bss_list->count));

		bi = next_bss(bss_list, bi);
		for_each_bss(bss_list, bi, i) {
			if (wf_chspec_valid(bi->chanspec)) {
				channel = wf_chspec_ctlchan(wl_chspec_driver_to_host(bi->chanspec));
				WL_ERR(("SSID :%s  Channel :%d\n", bi->SSID, channel));
			} else {
				WL_ERR(("SSID :%s Invalid chanspec :0x%x\n",
					bi->SSID, bi->chanspec));
			}
		}
	}
#endif

	ndev = wdev_to_wlc_ndev(wdev, cfg);
	bzero(&msg, sizeof(wl_event_msg_t));
	WL_ERR(("timer expired\n"));
#ifdef BCMDONGLEHOST
	dhdp->scan_timeout_occurred = TRUE;
#ifdef BCMPCIE
	if (!dhd_pcie_dump_int_regs(dhdp)) {
		WL_ERR(("%s : PCIe link might be down\n", __FUNCTION__));
		dhd_bus_set_linkdown(dhdp, TRUE);
		dhdp->hang_reason = HANG_REASON_PCIE_LINK_DOWN_EP_DETECT;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
		dhd_os_send_hang_message(dhdp);
#else
		WL_ERR(("%s: HANG event is unsupported\n", __FUNCTION__));
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) && OEM_ANDROID */
	}

	dhd_pcie_dump_rc_conf_space_cap(dhdp);
#endif /* BCMPCIE */
#if 0
	if (!dhd_bus_get_linkdown(dhdp) && dhdp->memdump_enabled) {
		dhdp->memdump_type = DUMP_TYPE_SCAN_TIMEOUT;
#ifdef BCMPCIE
		dhd_bus_mem_dump(dhdp);
#else
		if (dhd_schedule_socram_dump(dhdp)) {
			WL_ERR(("%s: socram dump failed\n", __FUNCTION__));
		}
#endif /* BCMPCIE */
	}
	/*
	 * For the memdump sanity, blocking bus transactions for a while
	 * Keeping it TRUE causes the sequential private cmd error
	 */
	if (!dhdp->memdump_enabled) {
		dhdp->scan_timeout_occurred = FALSE;
	}
#endif /* DHD_FW_COREDUMP */
#endif /* BCMDONGLEHOST */

	msg.event_type = hton32(WLC_E_ESCAN_RESULT);
	msg.status = hton32(WLC_E_STATUS_TIMEOUT);
	msg.reason = 0xFFFFFFFF;
	wl_cfg80211_event(ndev, &msg, NULL);

#ifdef WL_CFGVENDOR_SEND_ALERT_EVENT
	dhdp->alert_reason = ALERT_SCAN_ERR;
	dhd_os_send_alert_message(dhdp);
#endif /* WL_CFGVENDOR_SEND_ALERT_EVENT */

#if defined(BCMDONGLEHOST)
	DHD_ENABLE_RUNTIME_PM(dhdp);
#endif /* BCMDONGLEHOST && OEM_ANDROID */

#if defined(BCMDONGLEHOST) && defined(DHD_FW_COREDUMP)
	if (dhdp->memdump_enabled) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
		dhd_os_send_hang_message(dhdp);
#else
		WL_ERR(("%s: HANG event is unsupported\n", __FUNCTION__));
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) && OEM_ANDROID */
	}
#endif /* BCMDONGLEHOST && DHD_FW_COREDUMP */

}

s32 wl_init_scan(struct bcm_cfg80211 *cfg)
{
	int err = 0;

	cfg->evt_handler[WLC_E_ESCAN_RESULT] = wl_escan_handler;
	cfg->escan_info.escan_state = WL_ESCAN_STATE_IDLE;
	wl_escan_init_sync_id(cfg);

	/* Init scan_timeout timer */
	init_timer_compat(&cfg->scan_timeout, wl_scan_timeout, cfg);

	wl_cfg80211_set_bcmcfg(cfg);

	return err;
}

#ifdef WL_SCHED_SCAN
static s32
wl_cfgscan_init_pno_escan(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	struct cfg80211_scan_request *request)
{
	struct wiphy *wiphy = bcmcfg_to_wiphy(cfg);
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);
	int err = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) && \
	defined(SUPPORT_RANDOM_MAC_SCAN)
	uint8 random_addr[ETHER_ADDR_LEN] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8 random_mask_46_bits[ETHER_ADDR_LEN] = {0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#endif /* KERNEL_VER >= 3.19 && SUPPORT_RANDOM_MAC_SCAN */
	mutex_lock(&cfg->scan_sync);

	/* No scans in progress */
	if (!cfg->sched_scan_req) {
		err = BCME_ERROR;
		WL_ERR(("No sched scan is in progress, err:%d\n", err));
		goto exit;
	}

	if (wl_get_drv_status_all(cfg, SCANNING)) {
		WL_INFORM_MEM(("scan in progress. cancel and trigger PNO targetted scan\n"));
		_wl_cfgscan_cancel_scan(cfg);
	}

	wl_set_drv_status(cfg, SCANNING, ndev);
	WL_PNO((">>> Doing targeted ESCAN on PNO event\n"));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) && \
	defined(SUPPORT_RANDOM_MAC_SCAN)
	request->flags |= NL80211_SCAN_FLAG_RANDOM_ADDR;
	memcpy_s(&request->mac_addr, ETH_ALEN, random_addr, ETH_ALEN);
	memcpy_s(&request->mac_addr_mask, ETH_ALEN, random_mask_46_bits, ETH_ALEN);
#endif /*  KERNEL_VER >= 3.19 && SUPPORT_RANDOM_MAC_SCAN */
	LOG_TS(cfg, scan_start);
	err = wl_do_escan(cfg, wiphy, ndev, request);
	if (err) {
		wl_clr_drv_status(cfg, SCANNING, ndev);
		WL_ERR(("targeted escan failed. err:%d\n", err));
		CLR_TS(cfg, scan_start);
		goto exit;
	}

	UNUSED_PARAMETER(dhdp);
	DBG_EVENT_LOG(dhdp, WIFI_EVENT_DRIVER_PNO_SCAN_REQUESTED);

	cfg->sched_scan_running = TRUE;

exit:
	mutex_unlock(&cfg->scan_sync);
	return err;
}

static s32
wl_cfgscan_update_v3_schedscan_results(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	wl_pfn_scanresults_v3_t *pfn_result, uint32 event_type)
{
	int err = 0;
	wl_pfn_net_info_v3_t *netinfo, *pnetinfo;
	struct cfg80211_scan_request *request = NULL;
	struct cfg80211_ssid *ssid = NULL;
	struct ieee80211_channel *channel = NULL;
	struct wiphy *wiphy	= bcmcfg_to_wiphy(cfg);
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);
	log_conn_event_t *event_data = NULL;
	tlv_log *tlv_data = NULL;
	u32 alloc_len = 0;
	int channel_req = 0;
	u32 payload_len;

	if (event_type == WLC_E_PFN_NET_LOST) {
		WL_PNO(("Do Nothing %d\n", event_type));
		return 0;
	}

	WL_INFORM_MEM(("PFN NET FOUND event. count:%d \n", pfn_result->count));

	pnetinfo = (wl_pfn_net_info_v3_t *)pfn_result->netinfo;
	if (pfn_result->count > 0) {
		int i;

		if (pfn_result->count > MAX_PFN_LIST_COUNT) {
			pfn_result->count = MAX_PFN_LIST_COUNT;
		}

		ssid = (struct cfg80211_ssid *)MALLOCZ(cfg->osh,
			sizeof(struct cfg80211_ssid) * MAX_PFN_LIST_COUNT);
		request = (struct cfg80211_scan_request *)MALLOCZ(cfg->osh,
			sizeof(*request) + sizeof(*request->channels) * pfn_result->count);
		channel = (struct ieee80211_channel *)MALLOCZ(cfg->osh,
			(sizeof(struct ieee80211_channel) * pfn_result->count));
		if (!request || !channel || !ssid) {
			WL_ERR(("No memory"));
			err = -ENOMEM;
			goto out_err;
		}

		request->wiphy = wiphy;

		if (DBG_RING_ACTIVE(dhdp, DHD_EVENT_RING_ID)) {
			alloc_len = sizeof(log_conn_event_t) + (3 * sizeof(tlv_log)) +
				DOT11_MAX_SSID_LEN + sizeof(uint16) + sizeof(int16);
			event_data = (log_conn_event_t *)MALLOCZ(cfg->osh, alloc_len);
			if (!event_data) {
				WL_ERR(("%s: failed to allocate the log_conn_event_t with "
					"length(%d)\n", __func__, alloc_len));
				err = -ENOMEM;
				goto out_err;
			}
		}

		for (i = 0; i < pfn_result->count; i++) {
			u16 ssid_len;
			u8 ssid_buf[DOT11_MAX_SSID_LEN + 1] = {0};
			netinfo = &pnetinfo[i];

			/* PFN result doesn't have all the info which are required by the
			 * supplicant. (For e.g IEs) Do a target Escan so that sched scan
			 * results are reported via wl_inform_single_bss in the required
			 * format. Escan does require the scan request in the form of
			 * cfg80211_scan_request. For timebeing, create
			 * cfg80211_scan_request one out of the received PNO event.
			 */
			ssid[i].ssid_len = ssid_len = MIN(DOT11_MAX_SSID_LEN,
				netinfo->pfnsubnet.SSID_len);
			/* max ssid_len as in previous step DOT11_MAX_SSID_LEN is same
			* as DOT11_MAX_SSID_LEN = 32
			*/
			(void)memcpy_s(ssid[i].ssid, IEEE80211_MAX_SSID_LEN,
				netinfo->pfnsubnet.u.SSID, ssid_len);
			request->n_ssids++;

			channel_req = netinfo->pfnsubnet.chanspec;
			channel[i].center_freq = wl_channel_to_frequency(
				wf_chspec_ctlchan(netinfo->pfnsubnet.chanspec),
				CHSPEC_BAND(netinfo->pfnsubnet.chanspec));
			channel[i].band =
				wl_get_nl80211_band(CHSPEC_BAND(netinfo->pfnsubnet.chanspec));
			channel[i].flags |= IEEE80211_CHAN_NO_HT40;
			request->channels[i] = &channel[i];
			request->n_channels++;

			(void)memcpy_s(ssid_buf, IEEE80211_MAX_SSID_LEN,
				ssid[i].ssid, ssid_len);
			ssid_buf[ssid_len] = '\0';
			WL_INFORM_MEM(("[PNO] SSID:%s SSID length:%d"
				" chanspec:0x%x freq:%d band:%d\n",
				SSID_DBG(ssid_buf), ssid_len, netinfo->pfnsubnet.chanspec,
				channel[i].center_freq, channel[i].band));
			if (DBG_RING_ACTIVE(dhdp, DHD_EVENT_RING_ID)) {
				payload_len = sizeof(log_conn_event_t);
				event_data->event = WIFI_EVENT_DRIVER_PNO_NETWORK_FOUND;
				tlv_data = event_data->tlvs;

				/* ssid */
				tlv_data->tag = WIFI_TAG_SSID;
				tlv_data->len = netinfo->pfnsubnet.SSID_len;
				(void)memcpy_s(tlv_data->value, DOT11_MAX_SSID_LEN,
					ssid[i].ssid, ssid[i].ssid_len);
				payload_len += TLV_LOG_SIZE(tlv_data);
				tlv_data = TLV_LOG_NEXT(tlv_data);

				/* channel */
				tlv_data->tag = WIFI_TAG_CHANNEL;
				tlv_data->len = sizeof(uint16);
				(void)memcpy_s(tlv_data->value, sizeof(uint16),
					&channel_req, sizeof(uint16));
				payload_len += TLV_LOG_SIZE(tlv_data);
				tlv_data = TLV_LOG_NEXT(tlv_data);

				/* rssi */
				tlv_data->tag = WIFI_TAG_RSSI;
				tlv_data->len = sizeof(int16);
				(void)memcpy_s(tlv_data->value, sizeof(uint16),
					&netinfo->RSSI, sizeof(int16));
				payload_len += TLV_LOG_SIZE(tlv_data);
				tlv_data = TLV_LOG_NEXT(tlv_data);

				dhd_os_push_push_ring_data(dhdp, DHD_EVENT_RING_ID,
					&event_data->event, payload_len);
			}
		}

		/* assign parsed ssid array */
		if (request->n_ssids)
			request->ssids = &ssid[0];

		if (wl_get_p2p_status(cfg, DISCOVERY_ON)) {
			WL_PNO((">>> P2P discovery was ON. Disabling it\n"));
			err = wl_cfgp2p_discover_enable_search(cfg, false);
			if (unlikely(err)) {
				wl_clr_drv_status(cfg, SCANNING, ndev);
				goto out_err;
			}
			p2p_scan(cfg) = false;
		}

		err = wl_cfgscan_init_pno_escan(cfg, ndev, request);
		if (err) {
			goto out_err;
		}
	}
	else {
		WL_ERR(("FALSE PNO Event. (pfn_count == 0) \n"));
	}

out_err:
	if (ssid) {
		MFREE(cfg->osh, ssid,
			sizeof(struct cfg80211_ssid) * MAX_PFN_LIST_COUNT);
	}
	if (request) {
		MFREE(cfg->osh, request,
			sizeof(*request) + sizeof(*request->channels) * pfn_result->count);
	}
	if (channel) {
		MFREE(cfg->osh, channel,
			(sizeof(struct ieee80211_channel) * pfn_result->count));
	}

	if (event_data) {
		MFREE(cfg->osh, event_data, alloc_len);
	}

	return err;
}

/* If target scan is not reliable, set the below define to "1" to do a
 * full escan
 */
static s32
wl_notify_sched_scan_results(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data)
{
	wl_pfn_net_info_v1_t *netinfo, *pnetinfo;
	wl_pfn_net_info_v2_t *netinfo_v2, *pnetinfo_v2;
	struct wiphy *wiphy	= bcmcfg_to_wiphy(cfg);
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);
	int err = 0;
	struct cfg80211_scan_request *request = NULL;
	struct cfg80211_ssid *ssid = NULL;
	struct ieee80211_channel *channel = NULL;
	int channel_req = 0;
	int band = 0;
	wl_pfn_scanresults_v1_t *pfn_result_v1 = (wl_pfn_scanresults_v1_t *)data;
	wl_pfn_scanresults_v2_t *pfn_result_v2 = (wl_pfn_scanresults_v2_t *)data;
	wl_pfn_scanresults_v3_t *pfn_result_v3 = (wl_pfn_scanresults_v3_t *)data;
	int n_pfn_results = 0;
	log_conn_event_t *event_data = NULL;
	tlv_log *tlv_data = NULL;
	u32 alloc_len = 0;
	u32 payload_len;
	u8 tmp_buf[DOT11_MAX_SSID_LEN + 1];

	WL_DBG(("Enter\n"));

	if (cfg->sched_scan_running) {
		WL_INFORM_MEM(("sched_scan is already running. return\n"));
		return BCME_OK;
	}

	/* These static asserts guarantee v1/v2 net_info and subnet_info are compatible
	 * in size and SSID offset, allowing v1 to be used below except for the results
	 * fields themselves (status, count, offset to netinfo).
	 */
	STATIC_ASSERT(sizeof(wl_pfn_net_info_v1_t) == sizeof(wl_pfn_net_info_v2_t));
	STATIC_ASSERT(sizeof(wl_pfn_lnet_info_v1_t) == sizeof(wl_pfn_lnet_info_v2_t));
	STATIC_ASSERT(sizeof(wl_pfn_subnet_info_v1_t) == sizeof(wl_pfn_subnet_info_v2_t));

	/* Extract the version-specific items */
	if (pfn_result_v1->version == PFN_SCANRESULT_VERSION_V1) {
		n_pfn_results = pfn_result_v1->count;
		pnetinfo = pfn_result_v1->netinfo;
		WL_INFORM_MEM(("PFN NET FOUND event. count:%d \n", n_pfn_results));

		if (n_pfn_results > 0) {
			int i;

			if (n_pfn_results > MAX_PFN_LIST_COUNT)
				n_pfn_results = MAX_PFN_LIST_COUNT;

			ssid = (struct cfg80211_ssid *)MALLOCZ(cfg->osh,
				sizeof(struct cfg80211_ssid) * MAX_PFN_LIST_COUNT);
			request = (struct cfg80211_scan_request *)MALLOCZ(cfg->osh,
				sizeof(*request) + sizeof(*request->channels) * n_pfn_results);
			channel = (struct ieee80211_channel *)MALLOCZ(cfg->osh,
				(sizeof(struct ieee80211_channel) * n_pfn_results));
			if (!request || !channel || !ssid) {
				WL_ERR(("No memory"));
				err = -ENOMEM;
				goto out_err;
			}

			request->wiphy = wiphy;

			if (DBG_RING_ACTIVE(dhdp, DHD_EVENT_RING_ID)) {
				alloc_len = sizeof(log_conn_event_t) + (3 * sizeof(tlv_log)) +
					DOT11_MAX_SSID_LEN + sizeof(uint16) + sizeof(int16);
				event_data = (log_conn_event_t *)MALLOCZ(cfg->osh, alloc_len);
				if (!event_data) {
					WL_ERR(("%s: failed to allocate the log_conn_event_t with "
						"length(%d)\n", __func__, alloc_len));
					goto out_err;
				}
			}

			for (i = 0; i < n_pfn_results; i++) {
				netinfo = &pnetinfo[i];
				/* This looks useless, shouldn't Coverity complain? */
				if (!netinfo) {
					WL_ERR(("Invalid netinfo ptr. index:%d", i));
					err = -EINVAL;
					goto out_err;
				}
				if (netinfo->pfnsubnet.SSID_len > DOT11_MAX_SSID_LEN) {
					WL_ERR(("Wrong SSID length:%d\n",
						netinfo->pfnsubnet.SSID_len));
					err = -EINVAL;
					goto out_err;
				}
				/* In previous step max SSID_len limited to DOT11_MAX_SSID_LEN
				* and tmp_buf size is DOT11_MAX_SSID_LEN+1
				*/
				(void)memcpy_s(tmp_buf, DOT11_MAX_SSID_LEN,
					netinfo->pfnsubnet.SSID, netinfo->pfnsubnet.SSID_len);
				tmp_buf[netinfo->pfnsubnet.SSID_len] = '\0';
				WL_PNO((">>> SSID:%s Channel:%d \n",
					tmp_buf, netinfo->pfnsubnet.channel));
				/* PFN result doesn't have all the info which are required by
				 * the supplicant. (For e.g IEs) Do a target Escan so that
				 * sched scan results are reported via wl_inform_single_bss in
				 * the required format. Escan does require the scan request in
				 * the form of cfg80211_scan_request. For timebeing, create
				 * cfg80211_scan_request one out of the received PNO event.
				 */

				ssid[i].ssid_len = netinfo->pfnsubnet.SSID_len;
				/* Returning void as ssid[i].ssid_len is limited to max of
				* DOT11_MAX_SSID_LEN
				*/
				(void)memcpy_s(ssid[i].ssid, IEEE80211_MAX_SSID_LEN,
					netinfo->pfnsubnet.SSID, ssid[i].ssid_len);
				request->n_ssids++;

				channel_req = netinfo->pfnsubnet.channel;
				band = (channel_req <= CH_MAX_2G_CHANNEL) ? NL80211_BAND_2GHZ
					: NL80211_BAND_5GHZ;
				channel[i].center_freq =
					ieee80211_channel_to_frequency(channel_req, band);
				channel[i].band = band;
				channel[i].flags |= IEEE80211_CHAN_NO_HT40;
				request->channels[i] = &channel[i];
				request->n_channels++;

				if (DBG_RING_ACTIVE(dhdp, DHD_EVENT_RING_ID)) {
					payload_len = sizeof(log_conn_event_t);
					event_data->event = WIFI_EVENT_DRIVER_PNO_NETWORK_FOUND;
					tlv_data = event_data->tlvs;

					/* ssid */
					tlv_data->tag = WIFI_TAG_SSID;
					tlv_data->len = ssid[i].ssid_len;
					(void)memcpy_s(tlv_data->value, DOT11_MAX_SSID_LEN,
						ssid[i].ssid, ssid[i].ssid_len);
					payload_len += TLV_LOG_SIZE(tlv_data);
					tlv_data = TLV_LOG_NEXT(tlv_data);

					/* channel */
					tlv_data->tag = WIFI_TAG_CHANNEL;
					tlv_data->len = sizeof(uint16);
					(void)memcpy_s(tlv_data->value, sizeof(uint16),
						&channel_req, sizeof(uint16));
					payload_len += TLV_LOG_SIZE(tlv_data);
					tlv_data = TLV_LOG_NEXT(tlv_data);

					/* rssi */
					tlv_data->tag = WIFI_TAG_RSSI;
					tlv_data->len = sizeof(int16);
					(void)memcpy_s(tlv_data->value, sizeof(int16),
						&netinfo->RSSI, sizeof(int16));
					payload_len += TLV_LOG_SIZE(tlv_data);
					tlv_data = TLV_LOG_NEXT(tlv_data);

					dhd_os_push_push_ring_data(dhdp, DHD_EVENT_RING_ID,
						&event_data->event, payload_len);
				}
			}

			/* assign parsed ssid array */
			if (request->n_ssids)
				request->ssids = &ssid[0];

			if (wl_get_p2p_status(cfg, DISCOVERY_ON)) {
				WL_PNO((">>> P2P discovery was ON. Disabling it\n"));
				err = wl_cfgp2p_discover_enable_search(cfg, false);
				if (unlikely(err)) {
					wl_clr_drv_status(cfg, SCANNING, ndev);
					goto out_err;
				}
				p2p_scan(cfg) = false;
			}
			err = wl_cfgscan_init_pno_escan(cfg, ndev, request);
			if (err) {
				goto out_err;
			}
		}
		else {
			WL_ERR(("FALSE PNO Event. (pfn_count == 0) \n"));
		}

	} else if (pfn_result_v2->version == PFN_SCANRESULT_VERSION_V2) {
		n_pfn_results = pfn_result_v2->count;
		pnetinfo_v2 = (wl_pfn_net_info_v2_t *)pfn_result_v2->netinfo;

		if (e->event_type == WLC_E_PFN_NET_LOST) {
			WL_PNO(("Do Nothing %d\n", e->event_type));
			return 0;
		}

		WL_INFORM_MEM(("PFN NET FOUND event. count:%d \n", n_pfn_results));

		if (n_pfn_results > 0) {
			int i;

			if (n_pfn_results > MAX_PFN_LIST_COUNT)
				n_pfn_results = MAX_PFN_LIST_COUNT;

			ssid = (struct cfg80211_ssid *)MALLOCZ(cfg->osh,
				sizeof(struct cfg80211_ssid) * MAX_PFN_LIST_COUNT);
			request = (struct cfg80211_scan_request *)MALLOCZ(cfg->osh,
				sizeof(*request) + sizeof(*request->channels) * n_pfn_results);
			channel = (struct ieee80211_channel *)MALLOCZ(cfg->osh,
				(sizeof(struct ieee80211_channel) * n_pfn_results));
			if (!request || !channel || !ssid) {
				WL_ERR(("No memory"));
				err = -ENOMEM;
				goto out_err;
			}

			request->wiphy = wiphy;

			if (DBG_RING_ACTIVE(dhdp, DHD_EVENT_RING_ID)) {
				alloc_len = sizeof(log_conn_event_t) + (3 * sizeof(tlv_log)) +
					DOT11_MAX_SSID_LEN + sizeof(uint16) + sizeof(int16);
				event_data = (log_conn_event_t *)MALLOC(cfg->osh, alloc_len);
				if (!event_data) {
					WL_ERR(("%s: failed to allocate the log_conn_event_t with "
						"length(%d)\n", __func__, alloc_len));
					goto out_err;
				}
			}

			for (i = 0; i < n_pfn_results; i++) {
				netinfo_v2 = &pnetinfo_v2[i];
				/* This looks useless, shouldn't Coverity complain? */
				if (!netinfo_v2) {
					WL_ERR(("Invalid netinfo ptr. index:%d", i));
					err = -EINVAL;
					goto out_err;
				}
				WL_PNO((">>> SSID:%s Channel:%d \n",
					netinfo_v2->pfnsubnet.u.SSID,
					netinfo_v2->pfnsubnet.channel));
				/* PFN result doesn't have all the info which are required by the
				 * supplicant. (For e.g IEs) Do a target Escan so that sched scan
				 * results are reported via wl_inform_single_bss in the required
				 * format. Escan does require the scan request in the form of
				 * cfg80211_scan_request. For timebeing, create
				 * cfg80211_scan_request one out of the received PNO event.
				 */
				ssid[i].ssid_len = MIN(DOT11_MAX_SSID_LEN,
					netinfo_v2->pfnsubnet.SSID_len);
				/* max ssid_len as in previous step DOT11_MAX_SSID_LEN is same
				* as DOT11_MAX_SSID_LEN = 32
				*/
				(void)memcpy_s(ssid[i].ssid, IEEE80211_MAX_SSID_LEN,
					netinfo_v2->pfnsubnet.u.SSID, ssid[i].ssid_len);
				request->n_ssids++;

				channel_req = netinfo_v2->pfnsubnet.channel;
				band = (channel_req <= CH_MAX_2G_CHANNEL) ? NL80211_BAND_2GHZ
					: NL80211_BAND_5GHZ;
				channel[i].center_freq =
					ieee80211_channel_to_frequency(channel_req, band);
				channel[i].band = band;
				channel[i].flags |= IEEE80211_CHAN_NO_HT40;
				request->channels[i] = &channel[i];
				request->n_channels++;

				if (DBG_RING_ACTIVE(dhdp, DHD_EVENT_RING_ID)) {
					payload_len = sizeof(log_conn_event_t);
					event_data->event = WIFI_EVENT_DRIVER_PNO_NETWORK_FOUND;
					tlv_data = event_data->tlvs;

					/* ssid */
					tlv_data->tag = WIFI_TAG_SSID;
					tlv_data->len = netinfo_v2->pfnsubnet.SSID_len;
					(void)memcpy_s(tlv_data->value, DOT11_MAX_SSID_LEN,
						ssid[i].ssid, ssid[i].ssid_len);
					payload_len += TLV_LOG_SIZE(tlv_data);
					tlv_data = TLV_LOG_NEXT(tlv_data);

					/* channel */
					tlv_data->tag = WIFI_TAG_CHANNEL;
					tlv_data->len = sizeof(uint16);
					(void)memcpy_s(tlv_data->value, sizeof(uint16),
						&channel_req, sizeof(uint16));
					payload_len += TLV_LOG_SIZE(tlv_data);
					tlv_data = TLV_LOG_NEXT(tlv_data);

					/* rssi */
					tlv_data->tag = WIFI_TAG_RSSI;
					tlv_data->len = sizeof(int16);
					(void)memcpy_s(tlv_data->value, sizeof(uint16),
						&netinfo_v2->RSSI, sizeof(int16));
					payload_len += TLV_LOG_SIZE(tlv_data);
					tlv_data = TLV_LOG_NEXT(tlv_data);

					dhd_os_push_push_ring_data(dhdp, DHD_EVENT_RING_ID,
						&event_data->event, payload_len);
				}
			}

			/* assign parsed ssid array */
			if (request->n_ssids)
				request->ssids = &ssid[0];

			if (wl_get_p2p_status(cfg, DISCOVERY_ON)) {
				WL_PNO((">>> P2P discovery was ON. Disabling it\n"));
				err = wl_cfgp2p_discover_enable_search(cfg, false);
				if (unlikely(err)) {
					wl_clr_drv_status(cfg, SCANNING, ndev);
					goto out_err;
				}
				p2p_scan(cfg) = false;
			}

			err = wl_cfgscan_init_pno_escan(cfg, ndev, request);
			if (err) {
				goto out_err;
			}
		}
		else {
			WL_ERR(("FALSE PNO Event. (pfn_count == 0) \n"));
		}
	} else if (pfn_result_v3->version == PFN_SCANRESULT_VERSION_V3) {
		err = wl_cfgscan_update_v3_schedscan_results(cfg, ndev,
			pfn_result_v3, e->event_type);
		if (err) {
			goto out_err;
		}
	} else {
		WL_ERR(("Unsupported version %d, expected %d or %d\n", pfn_result_v1->version,
			PFN_SCANRESULT_VERSION_V1, PFN_SCANRESULT_VERSION_V2));
		err = -EINVAL;
	}

out_err:

	mutex_lock(&cfg->scan_sync);
	if (err) {
		/* Notify upper layer that sched scan has stopped so that
		 * upper layer can attempt fresh scan.
		 */
		if (cfg->sched_scan_req) {
			WL_ERR(("sched_scan stopped\n"));
			wl_cfg80211_stop_pno(cfg,  bcmcfg_to_prmry_ndev(cfg));
			/* schedule the work to indicate sched scan stop to cfg layer */
			schedule_delayed_work(&cfg->sched_scan_stop_work, 0);
		} else {
			WL_ERR(("sched scan req null!\n"));
		}
		cfg->sched_scan_running = FALSE;
		CLR_TS(cfg, scan_start);
	}
	mutex_unlock(&cfg->scan_sync);

	if (ssid) {
		MFREE(cfg->osh, ssid,
			sizeof(struct cfg80211_ssid) * MAX_PFN_LIST_COUNT);
	}
	if (request) {
		MFREE(cfg->osh, request,
			sizeof(*request) + sizeof(*request->channels) * n_pfn_results);
	}
	if (channel) {
		MFREE(cfg->osh, channel,
			(sizeof(struct ieee80211_channel) * n_pfn_results));
	}

	if (event_data) {
		MFREE(cfg->osh, event_data, alloc_len);
	}
	return err;
}
#endif /* WL_SCHED_SCAN */

#ifdef PNO_SUPPORT
s32
wl_notify_pfn_status(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	struct net_device *ndev = NULL;
#ifdef GSCAN_SUPPORT
	void *ptr;
	int send_evt_bytes = 0;
	u32 event = be32_to_cpu(e->event_type);
	struct wiphy *wiphy = bcmcfg_to_wiphy(cfg);
#endif /* GSCAN_SUPPORT */

	WL_INFORM_MEM((">>> PNO Event\n"));

	if (!data) {
		WL_ERR(("Data received is NULL!\n"));
		return 0;
	}

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);
#ifdef GSCAN_SUPPORT
	ptr = dhd_dev_process_epno_result(ndev, data, event, &send_evt_bytes);
	if (ptr) {
		wl_cfgvendor_send_async_event(wiphy, ndev,
			GOOGLE_SCAN_EPNO_EVENT, ptr, send_evt_bytes);
		MFREE(cfg->osh, ptr, send_evt_bytes);
	}
	if (!dhd_dev_is_legacy_pno_enabled(ndev))
		return 0;
#endif /* GSCAN_SUPPORT */

#ifndef WL_SCHED_SCAN
	/* CUSTOMER_HW4 has other PNO wakelock time by RB:5911 */
	mutex_lock(&cfg->usr_sync);
	/* TODO: Use cfg80211_sched_scan_results(wiphy); */
	/* GregG : WAR as to supplicant busy and not allowed Kernel to suspend */
	CFG80211_DISCONNECTED(ndev, 0, NULL, 0, false, GFP_KERNEL);
	mutex_unlock(&cfg->usr_sync);
#else
	/* If cfg80211 scheduled scan is supported, report the pno results via sched
	 * scan results
	 */
	if (cfg->sched_scan_req) {
		wl_notify_sched_scan_results(cfg, ndev, e, data);
	} else {
		WL_ERR(("No sched scan running. ignore.\n"));
	}
#endif /* WL_SCHED_SCAN */
	return 0;
}
#endif /* PNO_SUPPORT */

#ifdef GSCAN_SUPPORT
s32
wl_notify_gscan_event(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	s32 err = 0;
	u32 event = be32_to_cpu(e->event_type);
	void *ptr = NULL;
	int send_evt_bytes = 0;
	int event_type;
	struct net_device *ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);
	struct wiphy *wiphy = bcmcfg_to_wiphy(cfg);
	u32 len = ntoh32(e->datalen);
	u32 buf_len = 0;

	switch (event) {
		case WLC_E_PFN_BEST_BATCHING:
			err = dhd_dev_retrieve_batch_scan(ndev);
			if (err < 0) {
				WL_ERR(("Batch retrieval already in progress %d\n", err));
			} else {
				event_type = WIFI_SCAN_THRESHOLD_NUM_SCANS;
				if (data && len) {
					event_type = *((int *)data);
				}
				wl_cfgvendor_send_async_event(wiphy, ndev,
				    GOOGLE_GSCAN_BATCH_SCAN_EVENT,
				     &event_type, sizeof(int));
			}
			break;
		case WLC_E_PFN_SCAN_COMPLETE:
			event_type = WIFI_SCAN_COMPLETE;
			wl_cfgvendor_send_async_event(wiphy, ndev,
				GOOGLE_SCAN_COMPLETE_EVENT,
				&event_type, sizeof(int));
			break;
		case WLC_E_PFN_BSSID_NET_FOUND:
			ptr = dhd_dev_hotlist_scan_event(ndev, data, &send_evt_bytes,
			      HOTLIST_FOUND, &buf_len);
			if (ptr) {
				wl_cfgvendor_send_hotlist_event(wiphy, ndev,
				 ptr, send_evt_bytes, GOOGLE_GSCAN_GEOFENCE_FOUND_EVENT);
				dhd_dev_gscan_hotlist_cache_cleanup(ndev, HOTLIST_FOUND);
				MFREE(cfg->osh, ptr, send_evt_bytes);
			} else {
				err = -ENOMEM;
			}
			break;
		case WLC_E_PFN_BSSID_NET_LOST:
			/* WLC_E_PFN_BSSID_NET_LOST is conflict shared with WLC_E_PFN_SCAN_ALLGONE
			 * We currently do not use WLC_E_PFN_SCAN_ALLGONE, so if we get it, ignore
			 */
			if (len) {
				ptr = dhd_dev_hotlist_scan_event(ndev, data, &send_evt_bytes,
				                                 HOTLIST_LOST, &buf_len);
				if (ptr) {
					wl_cfgvendor_send_hotlist_event(wiphy, ndev,
					 ptr, send_evt_bytes, GOOGLE_GSCAN_GEOFENCE_LOST_EVENT);
					dhd_dev_gscan_hotlist_cache_cleanup(ndev, HOTLIST_LOST);
					MFREE(cfg->osh, ptr, send_evt_bytes);
				} else {
					err = -ENOMEM;
				}
			} else {
				err = -EINVAL;
			}
			break;
		case WLC_E_PFN_GSCAN_FULL_RESULT:
			ptr = dhd_dev_process_full_gscan_result(ndev, data, len, &send_evt_bytes);
			if (ptr) {
				wl_cfgvendor_send_async_event(wiphy, ndev,
				    GOOGLE_SCAN_FULL_RESULTS_EVENT, ptr, send_evt_bytes);
				MFREE(cfg->osh, ptr, send_evt_bytes);
			} else {
				err = -ENOMEM;
			}
			break;
		case WLC_E_PFN_SSID_EXT:
			ptr = dhd_dev_process_epno_result(ndev, data, event, &send_evt_bytes);
			if (ptr) {
				wl_cfgvendor_send_async_event(wiphy, ndev,
				    GOOGLE_SCAN_EPNO_EVENT, ptr, send_evt_bytes);
				MFREE(cfg->osh, ptr, send_evt_bytes);
			} else {
				err = -ENOMEM;
			}
			break;
		default:
			WL_ERR(("Unknown event %d\n", event));
			break;
	}
	return err;
}
#endif /* GSCAN_SUPPORT */

void wl_cfg80211_set_passive_scan(struct net_device *dev, char *command)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	if (strcmp(command, "SCAN-ACTIVE") == 0) {
		cfg->active_scan = 1;
	} else if (strcmp(command, "SCAN-PASSIVE") == 0) {
		cfg->active_scan = 0;
	} else
		WL_ERR(("Unknown command \n"));
	return;
}

void
wl_cfgscan_listen_complete_work(struct work_struct *work)
{
	struct bcm_cfg80211 *cfg = NULL;
	BCM_SET_CONTAINER_OF(cfg, work, struct bcm_cfg80211, loc.work.work);

	WL_ERR(("listen timeout\n"));
	/* listen not completed. Do recovery */
	if (!cfg->loc.in_progress) {
		WL_ERR(("No listen in progress!\n"));
		return;
	}
	wl_cfgscan_notify_listen_complete(cfg);
}

s32
wl_cfgscan_notify_listen_complete(struct bcm_cfg80211 *cfg)
{
	WL_DBG(("listen on channel complete! cookie:%llu\n", cfg->last_roc_id));
	if (cfg->loc.wdev && cfg->loc.in_progress) {
#if defined(WL_CFG80211_P2P_DEV_IF)
		cfg80211_remain_on_channel_expired(cfg->loc.wdev, cfg->last_roc_id,
			&cfg->remain_on_chan, GFP_KERNEL);
#else
		cfg80211_remain_on_channel_expired(cfg->loc.wdev->netdev, cfg->last_roc_id,
			&cfg->remain_on_chan, cfg->remain_on_chan_type, GFP_KERNEL);
#endif
		cfg->loc.in_progress = false;
		cfg->loc.wdev = NULL;
	}
	return BCME_OK;
}

static void
wl_init_scan_params(struct bcm_cfg80211 *cfg, u8 *params, u16 params_size,
	u32 scan_type, u32 action, u32 passive_time)
{
	u32 sync_id = 0;
	wl_escan_params_v1_t *eparams = NULL;
	wl_escan_params_v3_t *eparams_v3 = NULL;
	wl_scan_params_v1_t *scanparams = NULL;
	wl_scan_params_v3_t *scanparams_v3 = NULL;

	wl_escan_set_sync_id(sync_id, cfg);
	if (IS_SCAN_PARAMS_V3_V2(cfg)) {
		eparams_v3 = (wl_escan_params_v3_t *)params;
		eparams_v3->version = htod32(cfg->scan_params_ver);
		eparams_v3->action =  htod16(action);
		eparams_v3->sync_id = sync_id;
		scanparams_v3 = (wl_scan_params_v3_t *)&eparams_v3->params;
		(void)memcpy_s(&scanparams_v3->bssid, ETHER_ADDR_LEN, &ether_bcast, ETHER_ADDR_LEN);
		scanparams_v3->version = htod16(WL_SCAN_PARAMS_VERSION_V3);
		scanparams_v3->length = htod16(sizeof(wl_scan_params_v3_t));
		scanparams_v3->bss_type = DOT11_BSSTYPE_ANY;
		scanparams_v3->scan_type = htod32(scan_type);
		scanparams_v3->nprobes = htod32(-1);
		scanparams_v3->active_time = htod32(-1);
		scanparams_v3->passive_time = htod32(passive_time);
		scanparams_v3->home_time = htod32(-1);
		bzero(&scanparams_v3->ssid, sizeof(wlc_ssid_t));
	} else {
		eparams = (wl_escan_params_v1_t *)params;
		eparams->version = htod32(ESCAN_REQ_VERSION_V1);
		eparams->action =  htod16(action);
		eparams->sync_id = sync_id;
		scanparams = (wl_scan_params_v1_t *)&eparams->params;
		(void)memcpy_s(&scanparams->bssid, ETHER_ADDR_LEN, &ether_bcast, ETHER_ADDR_LEN);
		scanparams->bss_type = DOT11_BSSTYPE_ANY;
		scanparams->scan_type = 0;
		scanparams->nprobes = htod32(-1);
		scanparams->active_time = htod32(-1);
		scanparams->passive_time = htod32(passive_time);
		scanparams->home_time = htod32(-1);
		bzero(&scanparams->ssid, sizeof(wlc_ssid_t));
	}
}

/* timeout for recoverying upper layer statemachine */
#define WL_LISTEN_TIMEOUT    3000u

s32
wl_cfgscan_cancel_listen_on_channel(struct bcm_cfg80211 *cfg, bool notify_user)
{
	WL_DBG(("Enter\n"));

	mutex_lock(&cfg->scan_sync);
	if (!cfg->loc.in_progress) {
		WL_ERR(("listen not in progress. do nothing\n"));
		goto exit;
	}

	if (delayed_work_pending(&cfg->loc.work)) {
		cancel_delayed_work_sync(&cfg->loc.work);
	}

	/* abort scan listen */
	_wl_cfgscan_cancel_scan(cfg);

	if (notify_user) {
		wl_cfgscan_notify_listen_complete(cfg);
	}
	cfg->loc.in_progress = false;
	cfg->loc.wdev = NULL;
exit:
	mutex_unlock(&cfg->scan_sync);
	return 0;
}

s32
wl_cfgscan_listen_on_channel(struct bcm_cfg80211 *cfg, struct wireless_dev *wdev,
	struct ieee80211_channel *channel, unsigned int duration)
{
	u32 dwell = duration;
	u32 chanspec, params_size;
	u16 chanspec_num = 0;
	s32 bssidx = -1;
	s32 err = 0;
	struct net_device *ndev = NULL;
	u8 *params = NULL;
	wl_escan_params_v1_t *eparams = NULL;
	wl_escan_params_v3_t *eparams_v3 = NULL;
	wl_scan_params_v1_t *scanparams = NULL;
	wl_scan_params_v3_t *scanparams_v3 = NULL;
	u16 *chanspec_list = NULL;
	u32 channel_num = 0, scan_type = 0;

	WL_DBG(("Enter \n"));
	if (!wdev) {
	  WL_ERR(("wdev null!\n"));
	  return -EINVAL;
	}

	mutex_lock(&cfg->scan_sync);
	if (wl_get_drv_status_all(cfg, SCANNING)) {
		WL_ERR(("Scanning in progress avoid listen on channel\n"));
		err = -EBUSY;
		goto exit;
	}
	if (cfg->loc.in_progress == true) {
		WL_ERR(("Listen in progress\n"));
		err = -EAGAIN;
		goto exit;
	}
	bssidx = wl_get_bssidx_by_wdev(cfg, wdev);
	if (bssidx < 0) {
		WL_ERR(("invalid bssidx!\n"));
		err = -EINVAL;
		goto exit;
	}

	/* Use primary ndev for netless dev. BSSIDX will point to right I/F */
	ndev = wdev->netdev ? wdev->netdev : bcmcfg_to_prmry_ndev(cfg);

	if (IS_SCAN_PARAMS_V3_V2(cfg)) {
		params_size = (WL_SCAN_PARAMS_V3_FIXED_SIZE +
			OFFSETOF(wl_escan_params_v3_t, params));
	} else {
		params_size = (WL_SCAN_PARAMS_V1_FIXED_SIZE
		+ OFFSETOF(wl_escan_params_v1_t, params));
	}

	/* Single channel for listen case. Add a padd of u16 for alignment */
	chanspec_num = 1;
	params_size += (chanspec_num + 1);

	/* Allocate space for populating single ssid in wl_escan_params_v1_t struct */
	params_size += ((u32) sizeof(struct wlc_ssid));

	params = MALLOCZ(cfg->osh, params_size);
	if (params == NULL) {
		err = -ENOMEM;
		WL_ERR(("listen fail. no mem.\n"));
		goto exit;
	}

	scan_type = WL_SCANFLAGS_PASSIVE | WL_SCANFLAGS_LISTEN;

	wl_init_scan_params(cfg, params, params_size,
		scan_type, WL_SCAN_ACTION_START, dwell);

	channel_num = (chanspec_num & WL_SCAN_PARAMS_COUNT_MASK);
	if (IS_SCAN_PARAMS_V3_V2(cfg)) {
		eparams_v3 = (wl_escan_params_v3_t *)params;
		scanparams_v3 = (wl_scan_params_v3_t *)&eparams_v3->params;
		chanspec_list = scanparams_v3->channel_list;
		scanparams_v3->channel_num = channel_num;
	} else {
		eparams = (wl_escan_params_v1_t *)params;
		scanparams = (wl_scan_params_v1_t *)&eparams->params;
		chanspec_list = scanparams->channel_list;
		scanparams->channel_num = channel_num;
	}

	/* Copy the single listen channel */
	chanspec = wl_freq_to_chanspec(channel->center_freq);
	chanspec_list[0] = chanspec;

	err = wldev_iovar_setbuf_bsscfg(ndev, "escan", params, params_size,
		cfg->escan_ioctl_buf, WLC_IOCTL_MEDLEN, bssidx, &cfg->ioctl_buf_sync);
	if (unlikely(err)) {
		if (err == BCME_EPERM) {
			/* Scan Not permitted at this point of time */
			WL_DBG((" listen not permitted at this time (%d)\n", err));
		} else {
			WL_ERR((" listen set error (%d)\n", err));
		}
		goto exit;
	} else {
		unsigned long listen_timeout = dwell + WL_LISTEN_TIMEOUT;
		WL_DBG(("listen started. chanspec:%x\n", chanspec));
		cfg->loc.in_progress = true;
		cfg->loc.wdev = wdev;

		if (schedule_delayed_work(&cfg->loc.work,
				msecs_to_jiffies(listen_timeout))) {

#if defined(BCMDONGLEHOST)
			DHD_PM_WAKE_LOCK_TIMEOUT(cfg->pub, listen_timeout);
#endif /* BCMDONGLEHOST && OEM_ANDROID */

		} else {
			WL_ERR(("Can't schedule listen work handler\n"));
		}
	}

exit:
	if (params) {
		MFREE(cfg->osh, params, params_size);
	}
	mutex_unlock(&cfg->scan_sync);
	return err;
}

#define LONG_LISTEN_TIME 2000
#ifdef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
static void
wl_priortize_scan_over_listen(struct bcm_cfg80211 *cfg,
	struct net_device *ndev, unsigned int duration)
{
	WL_DBG(("scan is running. go to fake listen state\n"));
	wl_set_drv_status(cfg, FAKE_REMAINING_ON_CHANNEL, ndev);

	WL_DBG(("cancel current listen timer \n"));
	del_timer_sync(&cfg->p2p->listen_timer);

	wl_clr_p2p_status(cfg, LISTEN_EXPIRED);

	mod_timer(&cfg->p2p->listen_timer, jiffies + msecs_to_jiffies(duration));
}
#endif /* WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST */

/* Few vendors use hard coded static ndev p2p0 for p2p disc */
#define IS_P2P_DISC_NDEV(wdev) \
	(wdev->netdev ? (strncmp(wdev->netdev->name, "p2p0", strlen("p2p0")) == 0) : false)

s32
#if defined(WL_CFG80211_P2P_DEV_IF)
wl_cfgscan_remain_on_channel(struct wiphy *wiphy, bcm_struct_cfgdev *cfgdev,
	struct ieee80211_channel *channel, unsigned int duration, u64 *cookie)
#else
wl_cfgscan_remain_on_channel(struct wiphy *wiphy, bcm_struct_cfgdev *cfgdev,
	struct ieee80211_channel *channel,
	enum nl80211_channel_type channel_type,
	unsigned int duration, u64 *cookie)
#endif /* WL_CFG80211_P2P_DEV_IF */
{
	s32 target_channel;
	u32 id;
	s32 err = BCME_OK;
	struct net_device *ndev = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct wireless_dev *wdev;

	RETURN_EIO_IF_NOT_UP(cfg);

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);
#if defined(WL_CFG80211_P2P_DEV_IF)
	wdev = cfgdev;
#else
	wdev = ndev_to_wdev(ndev);
#endif
	if (!wdev) {
		WL_ERR(("wdev null\n"));
		err = -EINVAL;
		goto exit;
	}

	if (wl_get_drv_status(cfg, AP_CREATING, ndev)) {
		err = BCME_BADARG;
		goto exit;
	}

	target_channel = ieee80211_frequency_to_channel(channel->center_freq);

	WL_DBG(("Enter, channel: %d, duration ms (%d) scan_state:%d\n",
		target_channel, duration,
		(wl_get_drv_status(cfg, SCANNING, ndev)) ? TRUE : FALSE));

#ifdef WL_BCNRECV
	/* check fakeapscan in progress then abort */
	wl_android_bcnrecv_stop(ndev, WL_BCNRECV_LISTENBUSY);
#endif /* WL_BCNRECV */

#if defined(WL_CFG80211_P2P_DEV_IF)
	if ((wdev->iftype == NL80211_IFTYPE_P2P_DEVICE) || IS_P2P_DISC_NDEV(wdev))
#else
	if (cfg->p2p)
#endif
	{
		/* p2p discovery */
		if (!cfg->p2p) {
			WL_ERR(("cfg->p2p is not initialized\n"));
			err = BCME_ERROR;
			goto exit;
		}

#ifdef P2P_LISTEN_OFFLOADING
		if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS)) {
			WL_ERR(("P2P_FIND: Discovery offload is in progress\n"));
			err = -EAGAIN;
			goto exit;
		}
#endif /* P2P_LISTEN_OFFLOADING */

		if (wl_get_drv_status_all(cfg, SCANNING)) {
#ifdef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
			if (duration > LONG_LISTEN_TIME) {
				wl_cfgscan_cancel_scan(cfg);
			} else {
				wl_priortize_scan_over_listen(cfg, ndev, duration);
				err = BCME_OK;
				goto exit;
			}
#else
			wl_cfgscan_cancel_scan(cfg);
#endif /* WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST */
		}

#ifdef WL_CFG80211_SYNC_GON
		if (wl_get_drv_status_all(cfg, WAITING_NEXT_ACT_FRM_LISTEN)) {
			/* Do not enter listen mode again if we are in listen mode already
			* for next af. Remain on channel completion will be returned by
			* af completion.
			*/
#ifdef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
			wl_set_drv_status(cfg, FAKE_REMAINING_ON_CHANNEL, ndev);
#else
			wl_set_drv_status(cfg, REMAINING_ON_CHANNEL, ndev);
#endif
			goto exit;
		}
#endif /* WL_CFG80211_SYNC_GON */

		if (!cfg->p2p->on) {
		/* In case of p2p_listen command, supplicant may send
		* remain_on_channel without turning on P2P
		*/
			p2p_on(cfg) = true;
		}

		err = wl_cfgp2p_enable_discovery(cfg, ndev, NULL, 0);
		if (unlikely(err)) {
			goto exit;
		}

		mutex_lock(&cfg->usr_sync);
		err = wl_cfgp2p_discover_listen(cfg, target_channel, duration);
		if (err == BCME_OK) {
			wl_set_drv_status(cfg, REMAINING_ON_CHANNEL, ndev);
		} else {
#ifdef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
			if (err == BCME_BUSY) {
				/* if failed, firmware may be internal scanning state.
				* so other scan request shall not abort it
				*/
				wl_set_drv_status(cfg, FAKE_REMAINING_ON_CHANNEL, ndev);
				/* WAR: set err = ok to prevent cookie mismatch in wpa_supplicant
				* and expire timer will send a completion to the upper layer
				*/
				err = BCME_OK;
			}
#endif /* WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST */
		}
		mutex_unlock(&cfg->usr_sync);
	} else if (wdev->iftype == NL80211_IFTYPE_STATION ||
		wdev->iftype == NL80211_IFTYPE_AP) {
		WL_DBG(("LISTEN ON CHANNEL\n"));
		err = wl_cfgscan_listen_on_channel(cfg, wdev, channel, duration);
	}

exit:
	if (err == BCME_OK) {
		WL_DBG(("Success\n"));
		(void)memcpy_s(&cfg->remain_on_chan, sizeof(struct ieee80211_channel),
			channel, sizeof(struct ieee80211_channel));
#if defined(WL_ENABLE_P2P_IF)
		cfg->remain_on_chan_type = channel_type;
#endif /* WL_ENABLE_P2P_IF */
		id = ++cfg->last_roc_id;
		if (id == 0) {
			id = ++cfg->last_roc_id;
		}
		*cookie = id;

		/* Notify userspace that listen has started */
		CFG80211_READY_ON_CHANNEL(cfgdev, *cookie, channel, channel_type, duration, flags);
		WL_INFORM_MEM(("listen started on channel:%d duration (ms):%d cookie:%llu\n",
				target_channel, duration, *cookie));
	} else {
		WL_ERR(("Fail to Set (err=%d cookie:%llu)\n", err, *cookie));
		wl_flush_fw_log_buffer(ndev, FW_LOGSET_MASK_ALL);
	}
	return err;
}

s32
wl_cfgscan_cancel_remain_on_channel(struct wiphy *wiphy,
	bcm_struct_cfgdev *cfgdev, u64 cookie)
{
	s32 err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
#ifdef P2PLISTEN_AP_SAMECHN
	struct net_device *dev;
#endif /* P2PLISTEN_AP_SAMECHN */

	RETURN_EIO_IF_NOT_UP(cfg);

#ifdef DHD_IFDEBUG
	PRINT_WDEV_INFO(cfgdev);
#endif /* DHD_IFDEBUG */

	mutex_lock(&cfg->usr_sync);
#if defined(WL_CFG80211_P2P_DEV_IF)
	WL_DBG(("cancel listen for iftype:%d\n", cfgdev->iftype));
	if ((cfgdev->iftype != NL80211_IFTYPE_P2P_DEVICE) &&
		!IS_P2P_DISC_NDEV(cfgdev)) {
		/* Handle non-p2p cases here */
		err = wl_cfgscan_cancel_listen_on_channel(cfg, false);
		goto exit;
	}
#else
	WL_DBG(("cancel listen for netdev_ifidx: %d \n", cfgdev->ifindex));
#endif /* WL_CFG80211_P2P_DEV_IF */

#ifdef P2PLISTEN_AP_SAMECHN
	if (cfg && cfg->p2p_resp_apchn_status) {
		dev = bcmcfg_to_prmry_ndev(cfg);
		wl_cfg80211_set_p2p_resp_ap_chn(dev, 0);
		cfg->p2p_resp_apchn_status = false;
		WL_DBG(("p2p_resp_apchn_status Turn OFF \n"));
	}
#endif /* P2PLISTEN_AP_SAMECHN */

	if (cfg->last_roc_id == cookie) {
		WL_DBG(("cancel p2p listen. cookie:%llu\n", cookie));
		wl_cfgp2p_set_p2p_mode(cfg, WL_P2P_DISC_ST_SCAN, 0, 0,
			wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE));
	} else {
		WL_ERR(("wl_cfg80211_cancel_remain_on_channel: ignore, request cookie(%llu)"
			" is not matched. (cur : %llu)\n",
			cookie, cfg->last_roc_id));
	}

#if defined(WL_CFG80211_P2P_DEV_IF)
exit:
#endif
	mutex_unlock(&cfg->usr_sync);
	return err;
}

#ifdef WL_GET_RCC
int
wl_android_get_roam_scan_chanlist(struct bcm_cfg80211 *cfg)
{
	s32 err = BCME_OK;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	struct sk_buff *skb;
	gfp_t kflags;
	struct net_device *ndev;
	struct wiphy *wiphy;
	wlc_ssid_t *ssid = NULL;
	wl_roam_channel_list_t channel_list;
	uint16 channels[MAX_ROAM_CHANNEL] = {0};
	char chanspec_buf[CHANSPEC_STR_LEN];
	int i = 0, nchan = 0;

	ndev = bcmcfg_to_prmry_ndev(cfg);
	wiphy = bcmcfg_to_wiphy(cfg);

	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	skb = CFG80211_VENDOR_EVENT_ALLOC(wiphy, ndev_to_wdev(ndev),
		BRCM_VENDOR_GET_RCC_EVENT_BUF_LEN, BRCM_VENDOR_EVENT_RCC_INFO, kflags);

	if (!skb) {
		WL_ERR(("skb alloc failed"));
		return BCME_NOMEM;
	}

	/* Get Current SSID */
	ssid = (struct wlc_ssid *)wl_read_prof(cfg, ndev, WL_PROF_SSID);
	if (!ssid) {
		WL_ERR(("No SSID found in the saved profile\n"));
		err = BCME_ERROR;
		goto fail;
	}

	/* Get Current RCC List */
	err = wldev_iovar_getbuf(ndev, "roamscan_channels", 0, 0,
		(void *)&channel_list, sizeof(channel_list), NULL);
	if (err) {
		WL_ERR(("Failed to get roamscan channels, err = %d\n", err));
		goto fail;
	}
	if (channel_list.n > MAX_ROAM_CHANNEL) {
		WL_ERR(("Invalid roamscan channels count(%d)\n", channel_list.n));
		goto fail;
	}

	WL_DBG(("SSID %s(%d), RCC(%d)\n", ssid->SSID, ssid->SSID_len, channel_list.n));
	for (i = 0; i < channel_list.n; i++) {
		WL_DBG(("Chanspec[%d] CH: %s (0x%04x)\n",
			i, wf_chspec_ntoa_ex(channel_list.channels[i], chanspec_buf),
			channel_list.channels[i]));
		if (CHSPEC_IS6G(channel_list.channels[i])) {
			WL_DBG(("Skip 6G channel\n"));
			continue;
		}
		channels[nchan] = CHSPEC_CHANNEL(channel_list.channels[i]);
		nchan++;
	}

	err = nla_put_string(skb, RCC_ATTRIBUTE_SSID, ssid->SSID);
	if (unlikely(err)) {
		WL_ERR(("nla_put_string RCC_ATTRIBUTE_SSID failed\n"));
		goto fail;
	}

	err = nla_put_u32(skb, RCC_ATTRIBUTE_SSID_LEN, ssid->SSID_len);
	if (unlikely(err)) {
		WL_ERR(("nla_put_u32 RCC_ATTRIBUTE_SSID_LEN failed\n"));
		goto fail;
	}

	err = nla_put_u32(skb, RCC_ATTRIBUTE_NUM_CHANNELS, nchan);
	if (unlikely(err)) {
		WL_ERR(("nla_put_u32 RCC_ATTRIBUTE_NUM_CHANNELS failed\n"));
		goto fail;
	}

	err = nla_put(skb, RCC_ATTRIBUTE_CHANNEL_LIST,
		sizeof(uint16) * MAX_ROAM_CHANNEL, channels);
	if (unlikely(err)) {
		WL_ERR(("nla_put RCC_ATTRIBUTE_CHANNEL_LIST failed\n"));
		goto fail;
	}

	cfg80211_vendor_event(skb, kflags);

	return err;

fail:
	if (skb) {
		nlmsg_free(skb);
	}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0) */
	return err;
}

int
wl_android_get_roam_scan_freqlist(struct bcm_cfg80211 *cfg)
{
	s32 err = BCME_OK;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	struct sk_buff *skb;
	gfp_t kflags;
	struct net_device *ndev;
	struct wiphy *wiphy;
	wlc_ssid_t *ssid = NULL;
	wl_roam_channel_list_t channel_list;
	uint16 frequencies[MAX_ROAM_CHANNEL] = {0};
	int i = 0;

	ndev = bcmcfg_to_prmry_ndev(cfg);
	wiphy = bcmcfg_to_wiphy(cfg);

	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	skb = CFG80211_VENDOR_EVENT_ALLOC(wiphy, ndev_to_wdev(ndev),
		BRCM_VENDOR_GET_RCC_EVENT_BUF_LEN, BRCM_VENDOR_EVENT_RCC_FREQ_INFO, kflags);

	if (!skb) {
		WL_ERR(("skb alloc failed"));
		return BCME_NOMEM;
	}

	/* Get Current SSID */
	ssid = (struct wlc_ssid *)wl_read_prof(cfg, ndev, WL_PROF_SSID);
	if (!ssid) {
		WL_ERR(("No SSID found in the saved profile\n"));
		err = BCME_ERROR;
		goto fail;
	}

	/* Get Current RCC List */
	err = wldev_iovar_getbuf(ndev, "roamscan_channels", 0, 0,
		(void *)&channel_list, sizeof(channel_list), NULL);
	if (err) {
		WL_ERR(("Failed to get roamscan channels, err = %d\n", err));
		goto fail;
	}
	if (channel_list.n > MAX_ROAM_CHANNEL) {
		WL_ERR(("Invalid roamscan channels count(%d)\n", channel_list.n));
		goto fail;
	}

	WL_DBG(("SSID %s(%d), RCC(%d)\n", ssid->SSID, ssid->SSID_len, channel_list.n));
	for (i = 0; i < channel_list.n; i++) {
		chanspec_t chsp = channel_list.channels[i];
		u32 freq = wl_channel_to_frequency(wf_chspec_ctlchan(chsp), CHSPEC_BAND(chsp));
		WL_DBG(("  [%d] Freq: %d (0x%04x)\n", i, freq, channel_list.channels[i]));
		frequencies[i] = (uint16)freq;
	}

	err = nla_put_string(skb, RCC_ATTRIBUTE_SSID, ssid->SSID);
	if (unlikely(err)) {
		WL_ERR(("nla_put_string RCC_ATTRIBUTE_SSID failed\n"));
		goto fail;
	}

	err = nla_put_u32(skb, RCC_ATTRIBUTE_SSID_LEN, ssid->SSID_len);
	if (unlikely(err)) {
		WL_ERR(("nla_put_u32 RCC_ATTRIBUTE_SSID_LEN failed\n"));
		goto fail;
	}

	err = nla_put_u32(skb, RCC_ATTRIBUTE_NUM_CHANNELS, channel_list.n);
	if (unlikely(err)) {
		WL_ERR(("nla_put_u32 RCC_ATTRIBUTE_NUM_CHANNELS failed\n"));
		goto fail;
	}

	err = nla_put(skb, RCC_ATTRIBUTE_CHANNEL_LIST,
		sizeof(uint16) * MAX_ROAM_CHANNEL, frequencies);
	if (unlikely(err)) {
		WL_ERR(("nla_put RCC_ATTRIBUTE_CHANNEL_LIST failed\n"));
		goto fail;
	}

	cfg80211_vendor_event(skb, kflags);

	return err;

fail:
	if (skb) {
		nlmsg_free(skb);
	}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0) */
	return err;
}

#endif /* WL_GET_RCC */

/*
 * This function prepares assoc channel/s
 */
s32
wl_get_assoc_channels(struct bcm_cfg80211 *cfg,
	struct net_device *dev, wlcfg_assoc_info_t *info)
{
#ifdef ESCAN_CHANNEL_CACHE
	s32 err;
	u32 max_channels = MAX_ROAM_CHANNEL;
	u16 rcc_chan_cnt = 0;

	/*
	 * If bcast join 'OR' no channel information is provided by user space,
	 * then use channels from ESCAN_CHANNEL_CACHE. For other cases where target
	 * channel is available, update RCC via iovar.
	 *
	 * For a given SSID there might multiple APs on different channels and FW
	 * would scan all those channels before deciding up on the AP.
	 */
	if (cfg->rcc_enabled) {
		wlc_ssid_t ssid;
		int band;
		chanspec_t chanspecs[MAX_ROAM_CHANNEL] = {0};
		chanspec_t target_chspec;

		err = wldev_get_band(dev, &band);
		if (!err) {
			set_roam_band(band);
		}

		if (memcpy_s(ssid.SSID, sizeof(ssid.SSID), info->ssid, info->ssid_len) != BCME_OK) {
			WL_ERR(("ssid copy failed\n"));
			return -EINVAL;
		}
		ssid.SSID_len = (uint32)info->ssid_len;

		/* Utilize channel hint if availble */
		if ((info->chan_cnt == 1) && info->chanspecs[0]) {
			target_chspec = info->chanspecs[0];
			WL_DBG(("channel_hint chanspec:%x\n", info->chanspecs[0]));
		} else {
			target_chspec = INVCHANSPEC;
		}
		rcc_chan_cnt = get_roam_channel_list(cfg, target_chspec, chanspecs,
				max_channels, &ssid, ioctl_version);
		if ((!info->targeted_join) || (info->bssid_hint) ||
				(info->chan_cnt == 0)) {
#if !defined(DISABLE_FW_NW_SEL_FOR_6G) && defined(WL_6G_BAND)
			int i;
			/* If 6G AP is present, override bssid_hint with our fw nw
			 * selection. Supplicant bssid_hint logic doesn't have support for
			 * 6G, HE, OCE load IE support
			 */
			for (i = 0; i < rcc_chan_cnt; i++) {
				if (CHSPEC_IS6G(chanspecs[i])) {
					WL_INFORM_MEM(("6G channel in rcc. use fw nw sel\n"));
					/* skip bssid hint inclusion and provide bcast bssid */
					info->bssid_hint = false;
					info->targeted_join = false;
					(void)memcpy_s(&info->bssid,
							ETH_ALEN, &ether_bcast, ETH_ALEN);
					break;
				}
			}
#endif /* !DISABLE_FW_NW_SEL_FOR_6G && WL_6G_BAND */
			/* Use RCC channels as part of join params */
			info->chan_cnt = rcc_chan_cnt;
			if (memcpy_s(info->chanspecs, sizeof(info->chanspecs), chanspecs,
					(sizeof(chanspec_t) * rcc_chan_cnt)) != BCME_OK) {
				WL_ERR(("chanspec copy failed!\n"));
				return -EINVAL;
			}
		}
	}
#endif /* ESCAN_CHANNEL_CACHE */

	WL_SCAN(("channel cnt:%d\n", info->chan_cnt));
	return BCME_OK;
}

bool
wl_cfgscan_is_dfs_set(wifi_band band)
{
	switch (band) {
		case WIFI_BAND_A_DFS:
		case WIFI_BAND_A_WITH_DFS:
		case WIFI_BAND_ABG_WITH_DFS:
		case WIFI_BAND_24GHZ_5GHZ_WITH_DFS_6GHZ:
			return true;
		default:
			return false;
	}
	return false;
}

s32
wl_cfgscan_get_band_freq_list(struct bcm_cfg80211 *cfg, struct wireless_dev *wdev, int band,
	uint32 *list, uint32 *num_channels)
{
	s32 err = BCME_OK;
	uint32 i, freq, list_count, count = 0;
	struct net_device *dev = bcmcfg_to_prmry_ndev(cfg);
	uint32 chspec, chaninfo;
	bool dfs_set = false;
	bool ap_iface;

	if (!wdev) {
		WL_ERR(("wdev null\n"));
		return -EINVAL;
	}

	ap_iface = IS_AP_IFACE(wdev);
#ifndef WL_DUAL_APSTA
	if (!ap_iface && (wdev->netdev != bcmcfg_to_prmry_ndev(cfg))) {
		/* The GETCHANNEL API could come before role conversion. so
		 * for now consider the requests coming on non primary I/F
		 * too as request for AP.
		 */
		ap_iface = TRUE;
	}
#endif /* WL_DUAL_APSTA */

	dfs_set = wl_cfgscan_is_dfs_set(band);
	err = wldev_iovar_getbuf_bsscfg(dev, "chan_info_list", NULL,
			0, list, CHANINFO_LIST_BUF_SIZE, 0, &cfg->ioctl_buf_sync);
	if (err == BCME_UNSUPPORTED) {
		WL_INFORM(("get chan_info_list, UNSUPPORTED\n"));
		return err;
	} else if (err != BCME_OK) {
		WL_ERR(("get chan_info_list err(%d)\n", err));
		return err;
	}

	list_count = ((wl_chanspec_list_v1_t *)list)->count;
	for (i = 0; i < list_count; i++) {
		chspec = dtoh32(((wl_chanspec_list_v1_t *)list)->chspecs[i].chanspec);
		chaninfo = dtoh32(((wl_chanspec_list_v1_t *)list)->chspecs[i].chaninfo);
		freq = wl_channel_to_frequency(wf_chspec_ctlchan(chspec),
			CHSPEC_BAND(chspec));

		if ((band & WIFI_BAND_BG) && CHSPEC_IS2G(chspec)) {
			/* add 2g channels */
			list[count++] = freq;
		}

		if ((band & WIFI_BAND_6GHZ) && CHSPEC_IS6G(chspec) && CHSPEC_IS20(chspec)) {
			/* For AP interface and 6G band, use only VLP, PSC channels */
			if (ap_iface && (!(chaninfo & WL_CHAN_BAND_6G_PSC) ||
					!(chaninfo & WL_CHAN_BAND_6G_VLP))) {
				WL_DBG(("skipping chspec:%x freq:%d for ap_iface\n", chspec, freq));
				continue;
			}
			/* add 6g channels */
			list[count++] = freq;
		}

		/* handle 5g separately */
		if (CHSPEC_IS5G(chspec) && CHSPEC_IS20(chspec)) {
			if (!((band == WIFI_BAND_A_DFS) && IS_DFS(chaninfo)) &&
				!(band & WIFI_BAND_A)) {
				/* Not DFS only case nor 5G case */
				continue;
			}

			if ((band & WIFI_BAND_A) && !dfs_set && IS_DFS(chaninfo)) {
				continue;
			}
			list[count++] = freq;
		}
	}

	WL_INFORM_MEM(("get_freqlist_cnt:%d band:%d\n", count, band));
	*num_channels = count;
	return err;
}

#if defined(WL_SOFTAP_ACS)
#define SEC_FREQ_HT40_OFFSET 20
static acs_delay_work_t delay_work_acs = { .init_flag = 0 };

static int wl_cfgscan_acs_parse_result(acs_selected_channels_t *pResult,
        chanspec_t ch_chosen, drv_acs_params_t *pParameter)
{
	unsigned int chspec_band, chspec_ctl_freq, chspec_center_ch, chspec_bw, chspec_sb;

	if ((!pResult) || (!pParameter)) {
		WL_ERR(("%s: parameter invalid\n", __FUNCTION__));
		return BCME_BADARG;
	} else if (!wf_chspec_valid(ch_chosen)) {
		WL_ERR(("%s: ch_chosen=0x%X invalid\n",
		        __FUNCTION__, ch_chosen));
		return BCME_BADARG;
	}

	chspec_ctl_freq = wl_channel_to_frequency(wf_chspec_ctlchan((chanspec_t)ch_chosen),
			CHSPEC_BAND((chanspec_t)ch_chosen));
	chspec_center_ch = wf_chspec_ctlchan((chanspec_t)ch_chosen);
	chspec_band = CHSPEC_BAND((chanspec_t)ch_chosen);
	chspec_bw = CHSPEC_BW(ch_chosen);
	chspec_sb = CHSPEC_CTL_SB(ch_chosen);
	WL_TRACE(("%s: ctl_freq=%d, center_ch=%d, band=0x%X, bw=0x%X, sb=0x%X\n",
	          __FUNCTION__,
	          chspec_ctl_freq,  chspec_center_ch, chspec_band, chspec_bw, chspec_sb));

	(void)memset_s(pResult, sizeof(acs_selected_channels_t),
		0, sizeof(acs_selected_channels_t));

	/* hw_mode */
	switch (chspec_band) {
		case WL_CHANSPEC_BAND_2G:
			pResult->hw_mode = HOSTAPD_MODE_IEEE80211G;
			break;
		case WL_CHANSPEC_BAND_5G:
		case WL_CHANSPEC_BAND_6G:
		default:
			pResult->hw_mode = HOSTAPD_MODE_IEEE80211A;
			break;
	}
	WL_TRACE(("%s: hw_mode=%d\n", __FUNCTION__, pResult->hw_mode));

	/* ch_width and others */
	switch (chspec_bw) {
	case WL_CHANSPEC_BW_40:
		if (pParameter->ht40_enabled) {
			pResult->ch_width = 40;
			switch (chspec_sb) {
			case WL_CHANSPEC_CTL_SB_U:
				pResult->pri_freq = chspec_ctl_freq;
				pResult->sec_freq = chspec_ctl_freq - SEC_FREQ_HT40_OFFSET;
				break;
			case WL_CHANSPEC_CTL_SB_L:
			default:
				pResult->pri_freq = chspec_ctl_freq;
				pResult->sec_freq = chspec_ctl_freq + SEC_FREQ_HT40_OFFSET;
				break;
			}
			WL_TRACE(("%s: HT40 ok\n", __FUNCTION__));
		} else {
			pResult->ch_width = 20;
			pResult->pri_freq = chspec_ctl_freq;
			pResult->sec_freq = 0;
			WL_TRACE(("%s: HT40 to HT20\n", __FUNCTION__));
		}
		break;
	case WL_CHANSPEC_BW_80:
		if ((pParameter->vht_enabled) || (pParameter->he_enabled)) {
			pResult->ch_width = 80;
			chspec_center_ch = wf_chspec_primary80_channel((chanspec_t)ch_chosen);
			pResult->vht_seg0_center_ch = chspec_center_ch;
			pResult->vht_seg1_center_ch = 0;
			switch (chspec_sb) {
			case WL_CHANSPEC_CTL_SB_LL:
			case WL_CHANSPEC_CTL_SB_LU:
				pResult->pri_freq = chspec_ctl_freq;
				pResult->sec_freq = chspec_ctl_freq + SEC_FREQ_HT40_OFFSET;
				break;
			case WL_CHANSPEC_CTL_SB_UL:
			case WL_CHANSPEC_CTL_SB_UU:
			default:
				pResult->pri_freq = chspec_ctl_freq;
				pResult->sec_freq = chspec_ctl_freq - SEC_FREQ_HT40_OFFSET;
				break;
			}
			WL_TRACE(("%s: HT80 ok\n", __FUNCTION__));
		} else if (pParameter->ht40_enabled) {
			pResult->ch_width = 40;
			switch (chspec_sb) {
			case WL_CHANSPEC_CTL_SB_LL:
			case WL_CHANSPEC_CTL_SB_UL:
				pResult->pri_freq = chspec_ctl_freq;
				pResult->sec_freq = chspec_ctl_freq + SEC_FREQ_HT40_OFFSET;
				break;
			case WL_CHANSPEC_CTL_SB_LU:
			case WL_CHANSPEC_CTL_SB_UU:
			default:
				pResult->pri_freq = chspec_ctl_freq;
				pResult->sec_freq = chspec_ctl_freq - SEC_FREQ_HT40_OFFSET;
				break;
			}
			WL_TRACE(("%s: HT80 to HT40\n", __FUNCTION__));
		} else {
			pResult->ch_width = 20;
			pResult->pri_freq = chspec_ctl_freq;
			pResult->sec_freq = 0;
			WL_TRACE(("%s: HT80 to HT20\n", __FUNCTION__));
		}
		break;
	case WL_CHANSPEC_BW_160:
	case WL_CHANSPEC_BW_8080:
		if ((pParameter->vht_enabled) || (pParameter->he_enabled)) {
			pResult->ch_width = 160;
			switch (chspec_sb) {
			case WL_CHANSPEC_CTL_SB_LLL:
			case WL_CHANSPEC_CTL_SB_LLU:
			case WL_CHANSPEC_CTL_SB_LUL:
			case WL_CHANSPEC_CTL_SB_LUU:
				pResult->pri_freq = chspec_ctl_freq;
				pResult->sec_freq = chspec_ctl_freq + SEC_FREQ_HT40_OFFSET;
				pResult->vht_seg0_center_ch = chspec_center_ch;
				pResult->vht_seg1_center_ch = chspec_center_ch + CH_80MHZ_APART;
				break;
			case WL_CHANSPEC_CTL_SB_ULL:
			case WL_CHANSPEC_CTL_SB_ULU:
			case WL_CHANSPEC_CTL_SB_UUL:
			case WL_CHANSPEC_CTL_SB_UUU:
			default:
				pResult->pri_freq = chspec_ctl_freq;
				pResult->sec_freq = chspec_ctl_freq - SEC_FREQ_HT40_OFFSET;
				pResult->vht_seg0_center_ch = chspec_center_ch;
				pResult->vht_seg1_center_ch = chspec_center_ch - CH_80MHZ_APART;
				break;
			}
			WL_TRACE(("%s: HT160 ok\n", __FUNCTION__));
		} else if (pParameter->ht40_enabled) {
			pResult->ch_width = 40;
			switch (chspec_sb) {
			case WL_CHANSPEC_CTL_SB_LLL:
			case WL_CHANSPEC_CTL_SB_LUL:
			case WL_CHANSPEC_CTL_SB_ULL:
			case WL_CHANSPEC_CTL_SB_UUL:
				pResult->pri_freq = chspec_ctl_freq;
				pResult->sec_freq = chspec_ctl_freq + SEC_FREQ_HT40_OFFSET;
				break;
			case WL_CHANSPEC_CTL_SB_LLU:
			case WL_CHANSPEC_CTL_SB_LUU:
			case WL_CHANSPEC_CTL_SB_ULU:
			case WL_CHANSPEC_CTL_SB_UUU:
			default:
				pResult->pri_freq = chspec_ctl_freq;
				pResult->sec_freq = chspec_ctl_freq - SEC_FREQ_HT40_OFFSET;
				break;
			}
			WL_TRACE(("%s: HT160 to HT40\n", __FUNCTION__));
		} else {
			pResult->ch_width = 20;
			pResult->pri_freq = chspec_ctl_freq;
			pResult->sec_freq = 0;
			WL_TRACE(("%s: HT160 to HT20\n", __FUNCTION__));
		}
		break;
	case WL_CHANSPEC_BW_20:
	default:
		if ((pParameter->ht_enabled) || (TRUE)) {
			pResult->ch_width = 20;
			pResult->pri_freq = chspec_ctl_freq;
		}
		WL_TRACE(("%s: HT20 ok\n", __FUNCTION__));
		break;
	}

	WL_TRACE(("%s: result: pri_freq=%d, sec_freq=%d, vht_seg0=%d, vht_seg1=%d,"
	          " ch_width=%d, hw_mode=%d\n", __FUNCTION__,
	          pResult->pri_freq, pResult->sec_freq,
	          pResult->vht_seg0_center_ch, pResult->vht_seg1_center_ch,
	          pResult->ch_width, pResult->hw_mode));

	return 0;
}

static int wl_cfgscan_acs_parse_parameter_save(int *pLen, uint32 *pList, chanspec_t chspec)
{
	int ret = 0;
	int qty = 0;
	int i;

	do {
		if ((!pLen) || (!pList)) {
			WL_ERR(("%s: parameter invalid\n", __FUNCTION__));
			ret = BCME_BADARG;
			break;
		} else {
			qty = *pLen;
		}

		if (!wf_chspec_valid(chspec)) {
			WL_TRACE(("%s: chanspec=0x%X invalid\n", __FUNCTION__, chspec));
			ret = BCME_BADARG;
			break;
		}

		for (i = 0; i < qty; i++) {
			if (pList[i] == chspec) {
				break;
			}
		}

		if (i == qty) {
			pList[qty++] = chspec;
			*pLen = qty;
			WL_TRACE(("%s: fill list[%d] = 0x%X\n", __FUNCTION__, qty, chspec));
		} else {
			WL_TRACE(("%s: duplicate with [idx]=[%d]=0x%X\n", __FUNCTION__, i, chspec));
			break;
		}
	} while (0);

	return ret;
}

static int wl_cfgscan_acs_parse_parameter(struct bcm_cfg80211 *cfg,
		int *pLen, uint32 *pList, unsigned int chanspec,
		drv_acs_params_t *pParameter)
{
	unsigned int chspec = 0, chspec_ctl_ch = 0, chspec_band = 0, chspec_bw = 0, chspec_sb = 0;
	uint32 qty = 0, channel = 0, bw = 0;
	s32 ret = 0;

	do {
		if ((!pLen) || (!pList) || (!pParameter)) {
			WL_ERR(("%s: parameter invalid\n", __FUNCTION__));
			ret = BCME_BADARG;
			break;
		}

		chspec_band = CHSPEC_BAND((chanspec_t)chanspec);
		channel = wf_chspec_ctlchan((chanspec_t)chanspec);
		if (channel != 165) {
			bw = pParameter->ch_width;
		} else {
			bw = 20;	/* ch165 supports bw20 only */
		}
		qty = *pLen;

		/* Handle 20MHz case for 2G and 6G (PSC) */
		if (chspec_band == WL_CHANSPEC_BAND_2G ||
				chspec_band == WL_CHANSPEC_BAND_6G) {
			/* Firmware expects 20Mhz PSC channels. */
			chspec_bw = WL_CHANSPEC_BW_20;
			chspec_ctl_ch = channel;
			chspec_sb = WL_CHANSPEC_CTL_SB_NONE;
			chspec = (chanspec_t)(chspec_ctl_ch | chspec_band |
					chspec_bw | chspec_sb);
			wl_cfgscan_acs_parse_parameter_save(&qty, pList, chspec);
			*pLen = qty;
			return 0;
		}

		/* Handle 5G band (from bw20 to bw80) */
		/* bw80 */
		if ((bw == 80) &&
				(pParameter->vht_enabled || pParameter->he_enabled)) {
			chspec = wf_create_chspec_from_primary(channel,
				WL_CHANSPEC_BW_80, chspec_band);
#ifdef WL_CELLULAR_CHAN_AVOID
			if (!wl_cellavoid_is_safe_overlap(cfg->cellavoid_info, chspec)) {
				chspec = INVCHANSPEC;
			}
#endif /* WL_CELLULAR_CHAN_AVOID */
			if (chspec != INVCHANSPEC) {
				WL_INFORM_MEM(("set %d/80 (0x%x)\n", channel, chspec));
				wl_cfgscan_acs_parse_parameter_save(&qty, pList, chspec);
			} else {
				bw = 40;	/* downgrade if not found proper chanspec */
			}
		}

		/* bw40 */
		if ((bw == 40) &&
				((pParameter->ht40_enabled) || (pParameter->vht_enabled) ||
				(pParameter->he_enabled))) {
			chspec = wf_create_chspec_from_primary(channel,
				WL_CHANSPEC_BW_40, chspec_band);
#ifdef WL_CELLULAR_CHAN_AVOID
			if (!wl_cellavoid_is_safe_overlap(cfg->cellavoid_info, chspec)) {
				chspec = INVCHANSPEC;
			}
#endif /* WL_CELLULAR_CHAN_AVOID */
			if (chspec != INVCHANSPEC) {
				WL_INFORM_MEM(("set %d/40 (0x%x)\n", channel, chspec));
				wl_cfgscan_acs_parse_parameter_save(&qty, pList, chspec);
			} else {
				bw = 20;	/* downgrade if not found proper chanspec */
			}
		}

		/* bw20 */
		if ((bw == 20) &&
				((pParameter->ht_enabled) || (pParameter->ht40_enabled) ||
				(pParameter->vht_enabled) || (pParameter->he_enabled))) {
			chspec = wf_create_chspec_from_primary(channel,
				WL_CHANSPEC_BW_20, chspec_band);
#ifdef WL_CELLULAR_CHAN_AVOID
			if (!wl_cellavoid_is_safe_overlap(cfg->cellavoid_info, chspec)) {
				chspec = INVCHANSPEC;
			}
#endif /* WL_CELLULAR_CHAN_AVOID */
			if (chspec != INVCHANSPEC) {
				WL_INFORM_MEM(("set %d/20 (0x%x)\n", channel, chspec));
				wl_cfgscan_acs_parse_parameter_save(&qty, pList, chspec);
			}
		}

		*pLen = qty;
		WL_TRACE(("%s: current quantity=%d\n", __FUNCTION__, qty));
	} while (0);

	return ret;
}

static void wl_cfgscan_acs_result_event(struct work_struct *work)
{
	acs_delay_work_t *delay_work = (acs_delay_work_t *)work;
	struct net_device *ndev = NULL;
	struct wiphy *wiphy = NULL;
	chanspec_t ch_chosen;
	drv_acs_params_t *pParameter;
	gfp_t kflags;
	struct sk_buff *skb = NULL;
	acs_selected_channels_t result;
	int len = 0;
	int ret = 0;

	do {
		if (!delay_work) {
			WL_ERR(("%s: work parameter invalid\n", __FUNCTION__));
			ret = BCME_BADARG;
			break;
		} else {
			ndev = delay_work->ndev;
			ch_chosen = delay_work->ch_chosen;
			pParameter = &delay_work->parameter;
		}

		if ((!ndev) || (!(ndev->ieee80211_ptr)) || (!(ndev->ieee80211_ptr->wiphy))) {
			WL_ERR(("%s: parameter invalid\n", __FUNCTION__));
			ret = BCME_BADARG;
			break;
		}
		wiphy = ndev->ieee80211_ptr->wiphy;

		/* construct result */
		if (wl_cfgscan_acs_parse_result(&result, ch_chosen, pParameter) < 0) {
			WL_ERR(("%s: fail to convert the result\n", __FUNCTION__));
			ret = BCME_BADARG;
			break;
		}

		len = 200;
		kflags = in_atomic()? GFP_ATOMIC : GFP_KERNEL;
		WL_TRACE(("%s: idx=%d, wiphy->n_vendor_events=%d\n",
		          __FUNCTION__, BRCM_VENDOR_EVENT_ACS, wiphy->n_vendor_events));
		/* Alloc the SKB for vendor_event */
#if (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0) || defined(USE_BACKPORT_4)
		skb = cfg80211_vendor_event_alloc(wiphy, ndev_to_wdev(ndev), len,
				BRCM_VENDOR_EVENT_ACS, kflags);
#else
		skb = cfg80211_vendor_event_alloc(wiphy, len, BRCM_VENDOR_EVENT_ACS, kflags);
#endif /* CONFIG_ARCH_MSM SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC */
		if (!skb) {
			WL_ERR(("%s: Error, no memory for event\n", __FUNCTION__));
			ret = BCME_NOMEM;
			break;
		}

		if ((nla_put_u32(skb, BRCM_VENDOR_ATTR_ACS_PRIMARY_FREQ, result.pri_freq) < 0) ||
		    (nla_put_u32(skb, BRCM_VENDOR_ATTR_ACS_SECONDARY_FREQ, result.sec_freq) < 0) ||
		    (nla_put_u8(skb, BRCM_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL,
				result.vht_seg0_center_ch) < 0) ||
		    (nla_put_u8(skb, BRCM_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL,
				result.vht_seg1_center_ch) < 0) ||
		    (nla_put_u16(skb, BRCM_VENDOR_ATTR_ACS_CHWIDTH, result.ch_width) < 0) ||
		    (nla_put_u8(skb, BRCM_VENDOR_ATTR_ACS_HW_MODE, result.hw_mode) < 0)) {
			WL_ERR(("%s: Error, fail to fill the result\n", __FUNCTION__));
			ret = BCME_BADARG;
			break;
		}

		WL_INFORM_MEM(("%s: ACS event notified. freq:%d:%d\n",
			__FUNCTION__, result.pri_freq, result.sec_freq));
		cfg80211_vendor_event(skb, kflags);
	} while (0);

	if (ret < 0) {
		if (skb) {
			WL_ERR(("%s: free the event since fail with ret=%d\n", __FUNCTION__, ret));
			dev_kfree_skb_any(skb);
		}
	}
}

static chanspec_t
wl_cfgscan_acs_find_default_chanspec(struct bcm_cfg80211 *cfg, u32 band,
	uint32 *pList, int qty)
{
	chanspec_t chosen;
#ifndef WL_CELLULAR_CHAN_AVOID
	uint16 channel;
	BCM_REFERENCE(cfg);
#endif

	if (pList != NULL && qty != 0) {
		chosen = pList[qty - 1];
	} else {
#ifdef WL_CELLULAR_CHAN_AVOID
		wl_cellavoid_sync_lock(cfg);
		chosen = wl_cellavoid_find_chspec_fromband(cfg->cellavoid_info, band);
		wl_cellavoid_sync_unlock(cfg);
#else
		switch (band) {
#ifdef WL_6G_BAND
			case WLC_BAND_6G:
				channel = APCS_DEFAULT_6G_CH;
				break;
#endif /* WL_6G_BAND */
			case WLC_BAND_5G:
				channel = APCS_DEFAULT_5G_CH;
				break;
			default:
				channel = APCS_DEFAULT_2G_CH;
				break;
		}
		chosen = CH20MHZ_CHSPEC(channel);
#endif /* WL_CELLULAR_CHAN_AVOID */
	}

	return chosen;
}

static int wl_cfgscan_acs_do_apcs(struct net_device *dev,
	drv_acs_params_t *parameter, chanspec_t *pCH,
	int len, unsigned char *pBuffer, int qty, uint32 *pList)
{
	int channel = 0;
	int chosen = 0;
	int retry = 0;
	int ret = 0;
	u32 band = parameter->band;
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);

	if (parameter->scc_chspec) {
		chosen = parameter->scc_chspec;
		WL_INFORM_MEM(("sta connected case. chosen:0x%x\n", chosen));
		goto done2;
	}

	if ((pBuffer) && (0 < len)) {
		/* The buffer has count followed by chanspecs */
		u32 buf_len = (qty + 1) * sizeof(uint32);
		if (wl_dbg_level & WL_DBG_DBG) {
			prhex("acs iovar_buf:", pBuffer, buf_len);
		}

		/* Skip ACS for single channel case */
		if ((qty == 1) && pList) {
			/* For single chanspec, ACS not required */
			chosen = pList[0];
			WL_INFORM_MEM(("single channel case. Skip ACS. chosen:0x%x\n", chosen));
			goto done2;
		}

		/* Make sure any scan is not running before ACS trigged. */
		wl_cfgscan_cancel_scan(cfg);

		chosen = wl_ext_autochannel(dev, ACS_DRV_BIT, band);
		if (chosen) {
			goto done2;
		} else
			goto done;

		ret = wldev_ioctl_set(dev, WLC_START_CHANNEL_SEL, (void *)pBuffer, buf_len);
		if (ret) {
			WL_ERR(("autochannel trigger failed. ret=%d\n", ret));
		} else {
			WL_INFORM_MEM(("acs triggered for %d chanspecs. ret=%d\n", qty, ret));
		}
	} else {
		ret = BCME_BADARG;
		WL_ERR(("%s: Error, no parameter to go, ret=%d\n", __FUNCTION__, ret));
	}

	if (ret < 0) {
		channel = 0;
		ret = BCME_BADARG;
		goto done;
	}

	/* Wait for auto channel selection, max 3000 ms */
	if ((band == WLC_BAND_2G) || (band == WLC_BAND_5G) || (band == WLC_BAND_6G)) {
		OSL_SLEEP(500);
	} else {
		/* Full channel scan at the minimum takes 1.2secs
		 * even with parallel scan. max wait time: 3500ms
		 */
		OSL_SLEEP(1000);
	}

	retry = APCS_MAX_RETRY;
	while (retry--) {
		ret = wldev_ioctl_get(dev, WLC_GET_CHANNEL_SEL, &chosen, sizeof(chosen));
		if (ret < 0) {
			chosen = 0;
		} else {
			chosen = dtoh32(chosen);
		}
		WL_DBG(("%s: round=%d, ret=%d, chosen=0x%X\n",
		          __FUNCTION__, APCS_MAX_RETRY-retry, ret, chosen));

		if (chosen) {
			int freq;

			channel = wf_chspec_ctlchan((chanspec_t)chosen);
			freq = wl_channel_to_frequency(
				wf_chspec_ctlchan((chanspec_t)chosen),
				CHSPEC_BAND((chanspec_t)chosen));

			WL_INFORM_MEM(("%s: * good, selected chosen=0x%X, freq:%d channel = %d\n",
				__FUNCTION__, chosen, freq, channel));
			break;
		}
		OSL_SLEEP(300);
	}

done:

	if (!chosen) {
		WL_ERR(("%s: retry=%d, ret=%d, chosen=0x%X. Attempt default chan\n",
			__FUNCTION__, retry, ret, chosen));

		/* On failure, fallback to a default channel */
		chosen = wl_cfgscan_acs_find_default_chanspec(cfg, band, pList, qty);

		/* Cellavoid might return INVCHANSPEC */
		if (ret < 0 && chosen != INVCHANSPEC) {
			/* set it 0 when use default channel */
			ret = 0;
		}
		WL_ERR(("ACS failed. Fall back to default chanspec (0x%x)\n", chosen));
	}

done2:
	*pCH = chosen;

	return ret;
}

#ifdef DHD_ACS_CHECK_SCC_2G_ACTIVE_CH
bool wl_check_active_2g_chan(struct bcm_cfg80211 *cfg, drv_acs_params_t *parameter,
	chanspec_t sta_chanspec)
{
	struct net_device *dev = bcmcfg_to_prmry_ndev(cfg);
	bool scc = FALSE;
	s32 ret = BCME_OK;
	uint bitmap = 0;
	u8 ioctl_buf[WLC_IOCTL_SMLEN];

	bzero(ioctl_buf, WLC_IOCTL_SMLEN);
	ret = wldev_iovar_getbuf(dev, "per_chan_info",
			(void *)&sta_chanspec, sizeof(sta_chanspec),
			ioctl_buf, WLC_IOCTL_SMLEN, NULL);
	if (ret != BCME_OK) {
		WL_ERR(("Failed to get per_chan_info chspec:0x%x, error:%d\n",
				sta_chanspec, ret));
		goto exit;
	}

	bitmap = dtoh32(*(uint *)ioctl_buf);
	if (bitmap & (WL_CHAN_PASSIVE | WL_CHAN_RESTRICTED | WL_CHAN_CLM_RESTRICTED)) {
		WL_INFORM_MEM(("chspec is not active chanspec:0x%x bitmap:%d\n",
				sta_chanspec, bitmap));
		goto exit;
	}

#ifdef WL_CELLULAR_CHAN_AVOID
	if (wl_cellavoid_mandatory_isset(cfg->cellavoid_info, NL80211_IFTYPE_AP) &&
		!wl_cellavoid_is_safe(cfg->cellavoid_info, sta_chanspec)) {
		WL_INFORM_MEM(("Not allow unsafe channel and mandatory chspec:0x%x\n",
			sta_chanspec));
		goto exit;
	}
#endif /* WL_CELLULAR_CHAN_AVOID */

	scc = TRUE;

exit:
	WL_INFORM_MEM(("STA chanspec:0x%x per_chan_info:0x%x scc:%d\n", sta_chanspec, bitmap, scc));
	return scc;
}
#endif /* DHD_ACS_CHECK_SCC_2G_ACTIVE_CH */

#define MAX_ACS_FREQS	256u
static int
wl_convert_freqlist_to_chspeclist(struct bcm_cfg80211 *cfg,
		u32 *pElem_freq, u32 freq_list_len, u32 *req_len,
		u32 *pList, drv_acs_params_t *parameter, int qty)
{
	int i, j;
	u32 list_size;
	s32 ret = -EINVAL;
	u32 *chspeclist = NULL;
	u32 *p_chspec_list = NULL;
	char chanspec_str[CHANSPEC_STR_LEN];
#ifdef WL_CELLULAR_CHAN_AVOID
	int safe_chspec_cnt = 0;
	u32 *safe_chspeclist = NULL;
	drv_acs_params_t safe_param = { 0 };
	bool safe_success = FALSE;
#endif /* WL_CELLULAR_CHAN_AVOID */

	if (freq_list_len > MAX_ACS_FREQS) {
		WL_ERR(("invalid len:%d\n", freq_list_len));
		return -EINVAL;
	}

	list_size = sizeof(u32) * freq_list_len;
	chspeclist = MALLOCZ(cfg->osh, list_size);
	if (!chspeclist) {
		WL_ERR(("chspec list alloc failed\n"));
		return -ENOMEM;
	}

#ifdef WL_CELLULAR_CHAN_AVOID
	safe_chspeclist = MALLOCZ(cfg->osh, list_size);
	if (!safe_chspeclist) {
		WL_ERR(("safe chspec list alloc failed\n"));
		MFREE(cfg->osh, chspeclist, list_size);
		return -ENOMEM;
	}

	wl_cellavoid_sync_lock(cfg);
#endif /* WL_CELLULAR_CHAN_AVOID */

	for (i = 0, j = 0; i < freq_list_len; i++) {
		chspeclist[j] = wl_freq_to_chanspec(pElem_freq[i]);
		if (CHSPEC_IS6G(chspeclist[j]) && !CHSPEC_IS_6G_PSC(chspeclist[j])) {
			/* Skip non PSC channels */
			WL_DBG(("Skipping 6G non PSC channel\n"));
			continue;
		}

#ifdef WL_UNII4_CHAN
		/* Skip UNII-4 frequencies */
		if (CHSPEC_IS5G(chspeclist[j]) &&
			IS_UNII4_CHANNEL(CHSPEC_CHANNEL(chspeclist[j]))) {
			WL_DBG(("Skipped UNII-4 chanspec 0x%x\n", chspeclist[j]));
			continue;
		}
#endif /* WL_UNII4_CHAN */

#ifdef WL_CELLULAR_CHAN_AVOID
		if (wl_cellavoid_is_safe(cfg->cellavoid_info, chspeclist[j])) {
			safe_chspeclist[safe_chspec_cnt++] = chspeclist[j];
			safe_param.freq_bands |= CHSPEC_TO_WLC_BAND(CHSPEC_BAND(chspeclist[j]));
			wf_chspec_ntoa(chspeclist[j], chanspec_str);
			WL_INFORM_MEM(("Adding %s (0x%x) to the safe list\n",
				chanspec_str, chspeclist[j]));
		}
#endif /* WL_CELLULAR_CHAN_AVOID */

		/* mark all the bands found */
		parameter->freq_bands |= CHSPEC_TO_WLC_BAND(CHSPEC_BAND(chspeclist[j]));
		WL_DBG(("%s: list[%d]=%d => chspec=0x%x\n", __FUNCTION__, i,
				pElem_freq[i], chspeclist[j]));
		j++;
	}

#ifdef WL_CELLULAR_CHAN_AVOID
	wl_cellavoid_sync_unlock(cfg);
#endif /* WL_CELLULAR_CHAN_AVOID */

	/* Overried freq list len with the new value */
	freq_list_len = j;

	WL_DBG(("** freq_bands=0x%x\n", parameter->freq_bands));
#ifdef WL_5G_SOFTAP_ONLY_ON_DEF_CHAN
	if ((parameter->freq_bands & WLC_BAND_5G) &&
			!(parameter->freq_bands & WLC_BAND_6G)) {

		/* Use default 5G channel for cases where 6G is not provided */
		for (i = 0; i < freq_list_len; i++) {
			if (CHSPEC_CHANNEL(chspeclist[i]) == APCS_DEFAULT_5G_CH) {
				WL_INFORM_MEM(("Def ACS chanspec:0x%x\n", chspeclist[i]));
				wl_cfgscan_acs_parse_parameter(cfg, req_len, pList,
					chspeclist[i], parameter);
				goto exit;
			}
		}

		if (i == freq_list_len) {
			WL_ERR(("Default 5g channel not found in the list\n"));
			ret = -EINVAL;
			goto exit;
		}
	}
#endif /* WL_5G_SOFTAP_ONLY_ON_DEF_CHAN */

#ifdef WL_CELLULAR_CHAN_AVOID
	if (safe_chspec_cnt) {
		WL_INFORM_MEM(("Try with safe channel, freq_band %x, chanspec cnt %d\n",
			safe_param.freq_bands, safe_chspec_cnt));
		ret = wl_handle_acs_concurrency_cases(cfg, &safe_param,
			safe_chspec_cnt, safe_chspeclist);
		if (ret == BCME_OK) {
			WL_INFORM_MEM(("Succeded to handle the acs concurrency case "
				"with safe channels!\n"));
			safe_success = TRUE;
			goto success;
		}
	}

	if (wl_cellavoid_mandatory_isset(cfg->cellavoid_info, NL80211_IFTYPE_AP)) {
		WL_INFORM_MEM(("Mandatory flag for AP is set, skip the ACS, safe_chspec_cnt %d\n",
			safe_chspec_cnt));
		goto exit;
	}

	if (safe_chspec_cnt != freq_list_len) {
		WL_INFORM_MEM(("Try with all channels, freq_band %x, chanspec cnt %d\n",
			parameter->freq_bands, freq_list_len));
		ret = wl_handle_acs_concurrency_cases(cfg, parameter, freq_list_len, chspeclist);
		if (ret) {
			WL_ERR(("Failed to handle the acs concurrency cases!\n"));
			goto exit;
		}
	}
#else
	ret = wl_handle_acs_concurrency_cases(cfg, parameter, freq_list_len, chspeclist);
#endif /* WL_CELLULAR_CHAN_AVOID */

	if (ret) {
		WL_ERR(("Failed to handle the acs concurrency cases!\n"));
		goto exit;
	}

#ifdef WL_CELLULAR_CHAN_AVOID
success:
	if (safe_success) {
		p_chspec_list = safe_chspeclist;
		parameter->freq_bands = safe_param.freq_bands;
		parameter->scc_chspec = safe_param.scc_chspec;
		freq_list_len = safe_chspec_cnt;
	} else
#endif /* WL_CELLULAR_CHAN_AVOID */
	{
		p_chspec_list = chspeclist;
	}

	for (i = 0; i < freq_list_len; i++) {
		if ((parameter->freq_bands & CHSPEC_TO_WLC_BAND(p_chspec_list[i])) == 0) {
			WL_DBG(("Skipping no matched band channel(0x%x).\n", p_chspec_list[i]));
			continue;
		}

		wf_chspec_ntoa(p_chspec_list[i], chanspec_str);
		WL_INFORM_MEM(("ACS : %s (0x%x)\n", chanspec_str, p_chspec_list[i]));
		wl_cfgscan_acs_parse_parameter(cfg, req_len, pList,
			p_chspec_list[i], parameter);
	}

exit:
	MFREE(cfg->osh, chspeclist, list_size);
#ifdef WL_CELLULAR_CHAN_AVOID
	MFREE(cfg->osh, safe_chspeclist, list_size);
#endif /* WL_CELLULAR_CHAN_AVOID */
	return ret;
}

#define ACS_WORK_DELAY_US   (100 * 1000)
#define ACS_WORK_CNT        5
int
wl_cfgscan_acs(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	int  ret = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *net = wdev_to_ndev(wdev);

	int qty = 0, total = 0;
	unsigned int *pElem_freq = NULL;
	/* original HOSTAPD parameters */
	struct nlattr *tb[BRCM_VENDOR_ATTR_ACS_LAST + 1];
	drv_acs_params_t *parameter = NULL;
#ifdef WL_ACS_CHANLIST
	int i = 0;
	unsigned char *pElem_chan = NULL;
	const struct nlattr *pChanList = NULL;
#endif /* WL_ACS_CHANLIST */
	const struct nlattr *pFreqList = NULL;
	uint32 chan_list_len = 0, freq_list_len = 0;
	/* converted list */
	wl_uint32_list_t *pReq = NULL;
	uint32 *pList = NULL;
	int req_len = 0;
	chanspec_t ch_chosen = 0x0;

	if (!delay_work_acs.init_flag) {
		delay_work_acs.ndev = net;
		delay_work_acs.ch_chosen = 0;
		INIT_DELAYED_WORK(&delay_work_acs.acs_delay_work, wl_cfgscan_acs_result_event);
		delay_work_acs.init_flag = 1;
	}

	do {
		/* get orignal HOSTAPD paramters */
		if (
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
			nla_parse(tb, BRCM_VENDOR_ATTR_ACS_LAST, (struct nlattr *)data,
				len, NULL, NULL) ||
#else
			nla_parse(tb, BRCM_VENDOR_ATTR_ACS_LAST, (struct nlattr *)data,
				len, NULL) ||
#endif
				(!tb[BRCM_VENDOR_ATTR_ACS_HW_MODE])) {
			WL_ERR(("%s: ***Error, parse fail\n", __FUNCTION__));
			ret = BCME_BADARG;
			break;
		}

		parameter = &delay_work_acs.parameter;
		(void)memset_s(parameter, sizeof(drv_acs_params_t), 0, sizeof(drv_acs_params_t));
		if (tb[BRCM_VENDOR_ATTR_ACS_HW_MODE]) {
			parameter->hw_mode = nla_get_u8(tb[BRCM_VENDOR_ATTR_ACS_HW_MODE]);
			WL_TRACE(("%s: hw_mode=%d\n", __FUNCTION__, parameter->hw_mode));
		}
		if (tb[BRCM_VENDOR_ATTR_ACS_HT_ENABLED]) {
			parameter->ht_enabled = nla_get_u8(tb[BRCM_VENDOR_ATTR_ACS_HT_ENABLED]);
			WL_TRACE(("%s: ht_enabled=%d\n", __FUNCTION__, parameter->ht_enabled));
		}
		if (tb[BRCM_VENDOR_ATTR_ACS_HT40_ENABLED]) {
			parameter->ht40_enabled = nla_get_u8(tb[BRCM_VENDOR_ATTR_ACS_HT40_ENABLED]);
			WL_TRACE(("%s: ht40_enabled=%d\n", __FUNCTION__, parameter->ht40_enabled));
		}
		if (tb[BRCM_VENDOR_ATTR_ACS_VHT_ENABLED]) {
			parameter->vht_enabled = nla_get_u8(tb[BRCM_VENDOR_ATTR_ACS_VHT_ENABLED]);
			WL_TRACE(("%s: vht_enabled=%d\n", __FUNCTION__, parameter->vht_enabled));
		}
		parameter->he_enabled = parameter->vht_enabled;
		if (tb[BRCM_VENDOR_ATTR_ACS_CHWIDTH]) {
			parameter->ch_width = nla_get_u8(tb[BRCM_VENDOR_ATTR_ACS_CHWIDTH]);
			WL_TRACE(("%s: ch_width=%d\n", __FUNCTION__, parameter->ch_width));
		}

		if (tb[BRCM_VENDOR_ATTR_ACS_CH_LIST]) {
#ifdef WL_ACS_CHANLIST
			pChanList = tb[BRCM_VENDOR_ATTR_ACS_CH_LIST];
			chan_list_len = nla_len(tb[BRCM_VENDOR_ATTR_ACS_CH_LIST]);
			WL_TRACE(("%s: chan_list_len=%d\n", __FUNCTION__, chan_list_len));
#else
			WL_ERR(("%s: chan_list attribute not supported\n", __FUNCTION__));
#endif /* WL_ACS_CHANLIST */
		}
		if (tb[BRCM_VENDOR_ATTR_ACS_FREQ_LIST]) {
			pFreqList = tb[BRCM_VENDOR_ATTR_ACS_FREQ_LIST];
			freq_list_len = nla_len(tb[BRCM_VENDOR_ATTR_ACS_FREQ_LIST]) / sizeof(int);
			WL_TRACE(("%s: freq_list_len=%d\n", __FUNCTION__, freq_list_len));
		}

		switch (parameter->hw_mode) {
			case HOSTAPD_MODE_IEEE80211B:
				parameter->band = WLC_BAND_2G;
				break;
			case HOSTAPD_MODE_IEEE80211G:
				parameter->band = WLC_BAND_2G;
				break;
			case HOSTAPD_MODE_IEEE80211A:
				parameter->band = WLC_BAND_5G;
				break;
			case HOSTAPD_MODE_IEEE80211ANY:
				parameter->band = WLC_BAND_AUTO;
				break;
			case HOSTAPD_MODE_IEEE80211AD:
				/* 802.11ad 60G is 'dead' and not supported */
			default:
				parameter->band = WLC_BAND_INVALID;
				break;
		}
		WL_INFORM_MEM(("%s: hw_mode=%d, band=%d ht_enabled:%d vht_enabled:%d ch_width:%d "
			"parameter->he_enabled = %d\n",
			__FUNCTION__, parameter->hw_mode, parameter->band, parameter->ht_enabled,
			parameter->vht_enabled, parameter->ch_width, parameter->he_enabled));
		if (WLC_BAND_INVALID == parameter->band) {
			ret = BCME_BADARG;
			WL_ERR(("%s: *Error, hw_mode=%d based band invalid\n",
			        __FUNCTION__, parameter->hw_mode));
			break;
		}

		/* count memory requirement */
		qty = chan_list_len + freq_list_len;
		total = sizeof(uint32) * qty *
		        (/* extra structure 'count' item */
		           1 +
		         /* maximum expand quantity of each channel: 20MHZ * 1, 40MHz * 2,
			  * 80MHz * 4, 160MHz * 8
			  */
		         (1 + 2 + 4 + 8));
		WL_TRACE(("%s: qty=%d+%d=%d, total=%d\n",
		          __FUNCTION__, chan_list_len, freq_list_len, qty, total));

		if (total <= 0) {
			ret = BCME_BADARG;
			WL_ERR(("%s: *Error, total number (%d) is invalid\n",
					__FUNCTION__, total));
			break;
		}
		pReq = MALLOCZ(cfg->osh, total);
		if (!pReq) {
			WL_ERR(("%s: *Error, no memory for %d bytes\n", __FUNCTION__, total));
			ret = BCME_NOMEM;
			break;
		} else {
			memset_s(pReq, total, 0, total);
			pReq->count = req_len = 0;
			pList = pReq->element;
		}

#ifdef WL_ACS_CHANLIST
		/* process 'ch_list' for select list */
		pElem_chan = (unsigned char *)nla_data(pChanList);
		if (pElem_chan) {
			/* Applicable only for 2g or 5g band */
			if ((parameter->band == WLC_BAND_AUTO) ||
					(parameter->band == WLC_BAND_INVALID)) {
				WL_ERR(("chanlist not applicable for band:%d\n",
					parameter->band));
				break;
			}
			for (i = 0; i < chan_list_len; i++) {
				/* TODO chanspec needs to be created */
				chanspec = pElem_chan[i];
				wl_cfgscan_acs_parse_parameter(cfg, &req_len, pList,
					chanspec, parameter);
			}
		}
		WL_TRACE(("%s: list_len=%d after ch_list\n", __FUNCTION__, req_len));
#endif /* WL_ACS_CHANLIST */

		/* process 'freq_list' */
		pElem_freq = (unsigned int *)nla_data(pFreqList);
		if (pElem_freq) {
#ifdef WL_CELLULAR_CHAN_AVOID
			ret = wl_cellavoid_set_requested_freq_bands(net, cfg->cellavoid_info,
				pElem_freq, freq_list_len);
			if (ret) {
				WL_ERR(("Setting requested freq band failed, ret %d\n", ret));
				break;
			}
#endif /* WL_CELLULAR_CHAN_AVOID */
			ret = wl_convert_freqlist_to_chspeclist(cfg, pElem_freq, freq_list_len,
					&req_len, pList, parameter, qty);
			if (ret) {
				WL_ERR(("Freq conversion failed!\n"));
				break;
			}
		}
		WL_TRACE(("%s: list_len=%d after freq_list\n", __FUNCTION__, req_len));

		pReq->count = req_len;
		req_len = pReq->count * (sizeof(pReq->element[0]));

		WL_DBG(("%s: set pReq->count=0x%X, with req_len=%d\n",
		          __FUNCTION__, pReq->count, req_len));
		ret = wl_cfgscan_acs_do_apcs(net, parameter, &ch_chosen, total,
				(unsigned char *)pReq, pReq->count, pList);
		WL_DBG(("%s: do acs ret=%d, ch_chosen=(0x%X)\n",
		          __FUNCTION__, ret, ch_chosen));
		if (ret >= 0) {
			u8 acs_work_cnt = ACS_WORK_CNT;
			while (delayed_work_pending(&delay_work_acs.acs_delay_work)) {
				if (acs_work_cnt) {
					WL_INFORM(("acs_delay_work: wait_cnt:%d\n", acs_work_cnt));
					acs_work_cnt--;
					OSL_DELAY(ACS_WORK_DELAY_US);
				} else {
					WL_INFORM_MEM(("ACS work time out. Force cancel.\n"));
					break;
				}
			}
			cancel_delayed_work_sync(&delay_work_acs.acs_delay_work);
			delay_work_acs.ndev = net;
			delay_work_acs.ch_chosen = ch_chosen;
			schedule_delayed_work(&delay_work_acs.acs_delay_work,
			                      msecs_to_jiffies((const unsigned int)50));
			WL_INFORM_MEM(("%s: scheduled work for acs event (chspec:0x%X)\n",
					__FUNCTION__, ch_chosen));
			ret = 0;
		}

	} while (0);

	/* free and clean up */
	if (NULL != pReq) {
		WL_TRACE(("%s: free the pReq=0x%p with total=%d\n",
			__FUNCTION__, pReq, total));
		MFREE(cfg->osh, pReq, total);
	}

#ifdef WL_CELLULAR_CHAN_AVOID
	/* This needs to be cleared in case of ACS failure */
	if (ret < 0) {
		wl_cellavoid_clear_requested_freq_bands(net, cfg->cellavoid_info);
	}
#endif /* WL_CELLULAR_CHAN_AVOID */

	return ret;
}
#endif /* WL_SOFTAP_ACS */

void
wl_get_ap_chanspecs(struct bcm_cfg80211 *cfg, wl_ap_oper_data_t *ap_data)
{
	struct net_info *iter, *next;
	u32 ch;

	bzero(ap_data, sizeof(wl_ap_oper_data_t));

	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	for_each_ndev(cfg, iter, next) {
		GCC_DIAGNOSTIC_POP();
		if ((iter->ndev) &&
			(iter->ndev->ieee80211_ptr->iftype == NL80211_IFTYPE_AP) &&
			wl_get_drv_status(cfg, CONNECTED, iter->ndev)) {
				if (wldev_iovar_getint(iter->ndev, "chanspec", (&ch)) == BCME_OK) {
					ap_data->iface[ap_data->count].ndev = iter->ndev;
					ap_data->iface[ap_data->count].chspec = (chanspec_t)ch;
					ap_data->count++;
				}
		}
	}
}

bool wl_is_chanspec_restricted(struct bcm_cfg80211 *cfg, chanspec_t sta_chanspec)
{
	struct net_device *dev = bcmcfg_to_prmry_ndev(cfg);
	s32 ret = BCME_OK;
	uint bitmap = 0;
	u8 ioctl_buf[WLC_IOCTL_SMLEN];

	bzero(ioctl_buf, WLC_IOCTL_SMLEN);
	ret = wldev_iovar_getbuf(dev, "per_chan_info",
			(void *)&sta_chanspec, sizeof(sta_chanspec),
			ioctl_buf, WLC_IOCTL_SMLEN, NULL);
	if (ret != BCME_OK) {
		WL_ERR(("Failed to get per_chan_info chspec:0x%x, error:%d\n",
				sta_chanspec, ret));
		return FALSE;
	}

	bitmap = dtoh32(*(uint *)ioctl_buf);
	if (bitmap & (WL_CHAN_PASSIVE | WL_CHAN_RADAR |
		WL_CHAN_RESTRICTED | WL_CHAN_CLM_RESTRICTED)) {
		WL_INFORM_MEM(("chanspec:0x%x is restricted by per_chan_info:0x%x\n",
			sta_chanspec, bitmap));
		return TRUE;
	}

	WL_INFORM_MEM(("STA chanspec:0x%x per_chan_info:0x%x\n", sta_chanspec, bitmap));
	return FALSE;
}

static bool wl_find_matching_chanspec(chanspec_t sta_chanspec,
	int qty, uint32 *pList)
{
	uint32 i = qty;

	if (!qty) {
		WL_ERR(("Invalid qty\n"));
		return false;
	}

	while (i--) {
		if (pList[i]) {
			if (wf_chspec_ctlchspec((pList[i])) == wf_chspec_ctlchspec(sta_chanspec)) {
				WL_INFORM_MEM(("Found sta chanspec in the list:0x%x\n",
					sta_chanspec));
				return true;
			}
			WL_INFORM_MEM(("skipped chanspec:0x%x\n", pList[i]));
		}
	}

	return false;
}

static bool
wl_acs_check_scc(struct bcm_cfg80211 *cfg, drv_acs_params_t *parameter,
	chanspec_t sta_chanspec, int qty, uint32 *pList)
{
	bool scc = FALSE;

	if (!(parameter->freq_bands & CHSPEC_TO_WLC_BAND(sta_chanspec))) {
		return scc;
	}

	if (wl_find_matching_chanspec(sta_chanspec, qty, pList)) {
		scc = TRUE;
	}

#ifdef DHD_ACS_CHECK_SCC_2G_ACTIVE_CH
	/*
	 * For the corner case when STA is running in Ch12 or Ch13
	 * and Framework may give the Ch [1-11] to ACS algorithm.
	 * In this case, SoftAP will be failed to run.
	 * To allow SoftAP to run in that channel as SCC mode,
	 * get active channels and check it
	 */
	if (scc == FALSE && CHSPEC_IS2G(sta_chanspec)) {
#ifdef WL_CELLULAR_CHAN_AVOID
		scc = wl_cellavoid_operation_allowed(cfg->cellavoid_info,
			sta_chanspec, NL80211_IFTYPE_AP);
		if (scc == FALSE) {
			WL_INFORM_MEM(("Not allow unsafe channel and mandatory chspec:0x%x\n",
			sta_chanspec));
		}
#endif /* WL_CELLULAR_CHAN_AVOID */
	}
#endif /* DHD_ACS_CHECK_SCC_2G_ACTIVE_CH */

	if (scc == TRUE) {
		parameter->scc_chspec = sta_chanspec;
		parameter->freq_bands = CHSPEC_TO_WLC_BAND(sta_chanspec);
		WL_INFORM_MEM(("SCC case, ACS pick up STA chanspec:0x%x\n", sta_chanspec));
	}
	return scc;
}

int
wl_handle_acs_concurrency_cases(struct bcm_cfg80211 *cfg, drv_acs_params_t *parameter,
	int qty, uint32 *pList)
{
	chanspec_t chspec = 0;
	wl_ap_oper_data_t ap_oper_data = {0};

	/* If STA is connected, figure out the STA connected band and applly
	 * following rules:
	 * If STA is in DFS channel or there is an AP already in that band, check
	 * whether the AP can be started in the other band
	 * If STA band and incoming Band matches, attempt SCC
	 */

	/* Check whether AP is already operational */
	wl_get_ap_chanspecs(cfg, &ap_oper_data);

	if (ap_oper_data.count >= MAX_AP_IFACES) {
		WL_ERR(("ACS request in multi AP case!! count:%d\n",
			ap_oper_data.count));
		return -EINVAL;
	}

	if (ap_oper_data.count == 1) {
		chanspec_t ch = ap_oper_data.iface[0].chspec;
		u16 ap_band;

		/* Single AP case. Bring up the AP in the other band */
		ap_band = CHSPEC_TO_WLC_BAND(ch);
		if ((ap_band == WLC_BAND_5G) || (ap_band == WLC_BAND_6G)) {
			WL_INFORM_MEM(("AP operational in band:%d\n", ap_band));
			if (!(parameter->freq_bands & WLC_BAND_2G)) {
				WL_ERR(("2G band not present in ACS list. fail ACS\n"));
				return -EINVAL;
			} else {
				/* Force set 2g and clear other bands for ACS */
				parameter->freq_bands = WLC_BAND_2G;
			}
		} else if (ap_band == WLC_BAND_2G) {
			WL_INFORM_MEM(("AP operational in 2G band\n"));
			if (!(parameter->freq_bands & WLC_BAND_5G) &&
					!(parameter->freq_bands & WLC_BAND_6G)) {
				WL_ERR(("5G/6G freqs not available in the ACS list. FAIL ACS\n"));
				return -EINVAL;
			} else {
				/* 6g/5g freqlist available. Clear 2g */
				parameter->freq_bands &= ~WLC_BAND_2G;
			}
		}
		WL_INFORM_MEM(("AP band:0x%x Trimmed ACS band:0x%x\n",
			ap_band, parameter->freq_bands));
	}

	/* Check STA concurrency cases */
	if (wl_cfgvif_get_iftype_count(cfg, WL_IF_TYPE_STA) >= 2) {
		/* Dual sta operational. Invalid use case */
		WL_ERR(("Dual sta operational. ACS request not expected.\n"));
		return -EINVAL;
	}

	chspec = wl_cfg80211_get_sta_chanspec(cfg);

	if (chspec) {
		bool scc_case = false;
		u32 sta_band = CHSPEC_TO_WLC_BAND(chspec);
		if (sta_band == WLC_BAND_2G) {
			if (wl_is_chanspec_restricted(cfg, chspec) ||
				(parameter->freq_bands & (WLC_BAND_5G | WLC_BAND_6G))) {
				/* Remove the 2g band from incoming ACS bands */
				parameter->freq_bands &= ~WLC_BAND_2G;
			} else if (wl_acs_check_scc(cfg, parameter, chspec, qty, pList)) {
				scc_case = TRUE;
			} else {
				WL_ERR(("STA connected in 2G,"
					" but no 2G channel available. Fail ACS\n"));
				return -EINVAL;
			}
		} else if (sta_band == WLC_BAND_5G) {
			if (wl_is_chanspec_restricted(cfg, chspec) ||
#ifdef WL_UNII4_CHAN
				IS_UNII4_CHANNEL(wf_chspec_primary20_chan(chspec)) ||
#endif /* WL_UNII4_CHAN */
				FALSE) {
				/*
				 * If STA is in DFS/Restricted/UNII4 channel,
				 * check for 2G availability in ACS list
				 */
				if (!(parameter->freq_bands & WLC_BAND_2G)) {
					WL_ERR(("STA connected in 5G DFS."
						" but no 2G channel available. Fail ACS\n"));
					return -EINVAL;
				}
				/* Remove the 5g/6g band from incoming ACS bands */
				parameter->freq_bands &= ~(WLC_BAND_5G | WLC_BAND_6G);
			} else if (wl_acs_check_scc(cfg, parameter, chspec, qty, pList)) {
				scc_case = TRUE;
			} else if (parameter->freq_bands & WLC_BAND_2G) {
				parameter->freq_bands = WLC_BAND_2G;
			} else {
				WL_ERR(("STA connected in 5G %x, but no channel available "
					"for ACS %x\n", chspec, parameter->freq_bands));
				return -EINVAL;
			}
		} else if (sta_band == WLC_BAND_6G) {
			if (wl_acs_check_scc(cfg, parameter, chspec, qty, pList)) {
				scc_case = TRUE;
			} else if (parameter->freq_bands & WLC_BAND_2G) {
				parameter->freq_bands = WLC_BAND_2G;
			} else {
				 WL_ERR(("STA connected in 6G %x, but no channel available "
				"for ACS %x\n", chspec, parameter->freq_bands));
				 return -EINVAL;
			}
		} else {
			WL_ERR(("Invalid sta band. Fail ACS\n"));
			return -EINVAL;
		}

		if (!scc_case) {
			WL_INFORM_MEM(("sta_band:%d chanspec:0x%x."
					" Attempt rsdb ACS for band/s:0x%x\n",
					sta_band, chspec, parameter->freq_bands));
		}
	}
	return BCME_OK;
}
