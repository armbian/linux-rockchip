/*
 * Bad AP Manager for ADPS
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
#include <linuxver.h>
#include <bcmiov.h>
#include <linux/list_sort.h>
#include <wl_cfg80211.h>
#include <wlioctl.h>
#include <wldev_common.h>
#include <wl_bam.h>

static int
wl_bad_ap_mngr_add_entry(wl_bad_ap_mngr_t *bad_ap_mngr, wl_bad_ap_info_t *bad_ap_info)
{
	unsigned long flags;
	wl_bad_ap_info_entry_t *entry;

	entry = MALLOCZ(bad_ap_mngr->osh, sizeof(*entry));
	if (entry == NULL) {
		WL_ERR(("%s: allocation for list failed\n", __FUNCTION__));
		return BCME_NOMEM;
	}

	memcpy(&entry->bad_ap, bad_ap_info, sizeof(entry->bad_ap));
	INIT_LIST_HEAD(&entry->list);
	spin_lock_irqsave(&bad_ap_mngr->lock, flags);
	list_add_tail(&entry->list, &bad_ap_mngr->list);
	spin_unlock_irqrestore(&bad_ap_mngr->lock, flags);

	bad_ap_mngr->num++;

	return BCME_OK;
}

extern wl_bad_ap_mngr_t *g_bad_ap_mngr;

wl_bad_ap_info_entry_t*
wl_bad_ap_mngr_find(wl_bad_ap_mngr_t *bad_ap_mngr, const struct ether_addr *bssid)
{
	wl_bad_ap_info_entry_t *entry;
	unsigned long flags;

#if defined(STRICT_GCC_WARNINGS) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
	spin_lock_irqsave(&bad_ap_mngr->lock, flags);
	list_for_each_entry(entry, &bad_ap_mngr->list, list) {
		if (!memcmp(&entry->bad_ap.bssid.octet, bssid->octet, ETHER_ADDR_LEN)) {
			spin_unlock_irqrestore(&bad_ap_mngr->lock, flags);
			return entry;
		}
	}
	spin_unlock_irqrestore(&bad_ap_mngr->lock, flags);
#if defined(STRICT_GCC_WARNINGS) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
	return NULL;
}

int
wl_bad_ap_mngr_add(wl_bad_ap_mngr_t *bad_ap_mngr, wl_bad_ap_info_t *bad_ap_info)
{
	int ret;
	wl_bad_ap_info_entry_t *entry;
	unsigned long flags;

	BCM_REFERENCE(entry);
	BCM_REFERENCE(flags);

	if (bad_ap_mngr->num == WL_BAD_AP_MAX_ENTRY_NUM) {
		/* Remove the oldest entry if entry list is full */
		spin_lock_irqsave(&bad_ap_mngr->lock, flags);
		entry = list_entry(bad_ap_mngr->list.next, wl_bad_ap_info_entry_t, list);
		if (entry) {
			list_del(&entry->list);
			MFREE(bad_ap_mngr->osh, entry, sizeof(*entry));
			bad_ap_mngr->num--;
		}
		spin_unlock_irqrestore(&bad_ap_mngr->lock, flags);
	}

	/* delete duplicated entry to update it at tail to keep the odrer */
	entry = wl_bad_ap_mngr_find(bad_ap_mngr, &bad_ap_info->bssid);
	if (entry != NULL) {
		spin_lock_irqsave(&bad_ap_mngr->lock, flags);
		list_del(&entry->list);
		MFREE(bad_ap_mngr->osh, entry, sizeof(*entry));
		bad_ap_mngr->num--;
		spin_unlock_irqrestore(&bad_ap_mngr->lock, flags);
	}

	ret = wl_bad_ap_mngr_add_entry(bad_ap_mngr, bad_ap_info);
	if (ret < 0) {
		WL_ERR(("%s - fail to add bad ap data(%d)\n", __FUNCTION__, ret));
		return ret;
	}

	return ret;
}

void
wl_bad_ap_mngr_deinit(struct bcm_cfg80211 *cfg)
{
	wl_bad_ap_info_entry_t *entry;
	unsigned long flags;

#if defined(STRICT_GCC_WARNINGS) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
	WL_CFG_BAM_LOCK(&cfg->bad_ap_mngr.lock, flags);
	while (!list_empty(&cfg->bad_ap_mngr.list)) {
		entry = list_entry(cfg->bad_ap_mngr.list.next, wl_bad_ap_info_entry_t, list);
		if (entry) {
			list_del(&entry->list);
			MFREE(cfg->osh, entry, sizeof(*entry));
		}
	}
	WL_CFG_BAM_UNLOCK(&cfg->bad_ap_mngr.lock, flags);
#if defined(STRICT_GCC_WARNINGS) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}

void
wl_bad_ap_mngr_init(struct bcm_cfg80211 *cfg)
{
	cfg->bad_ap_mngr.osh = cfg->osh;
	cfg->bad_ap_mngr.num = 0;

	spin_lock_init(&cfg->bad_ap_mngr.lock);
	INIT_LIST_HEAD(&cfg->bad_ap_mngr.list);

	g_bad_ap_mngr = &cfg->bad_ap_mngr;
}

static int
wl_event_adps_bad_ap_mngr(struct bcm_cfg80211 *cfg, void *data)
{
	int ret = BCME_OK;

	wl_event_adps_t *event_data = (wl_event_adps_t *)data;
	wl_event_adps_bad_ap_t *bad_ap_data;

	wl_bad_ap_info_entry_t *entry;
	wl_bad_ap_info_t temp;

	if (event_data->version != WL_EVENT_ADPS_VER_1) {
		return BCME_VERSION;
	}

	if (event_data->length != (OFFSETOF(wl_event_adps_t, data) + sizeof(*bad_ap_data))) {
		return BCME_ERROR;
	}

	BCM_REFERENCE(ret);
	BCM_REFERENCE(entry);
	bad_ap_data = (wl_event_adps_bad_ap_t *)event_data->data;

	memcpy(temp.bssid.octet, &bad_ap_data->ea.octet, ETHER_ADDR_LEN);
	ret = wl_bad_ap_mngr_add(&cfg->bad_ap_mngr, &temp);

	return ret;
}

static int
wl_adps_get_mode(struct net_device *ndev, uint8 band)
{
	int len;
	int ret;

	uint8 *pdata;
	char buf[WLC_IOCTL_SMLEN];

	bcm_iov_buf_t iov_buf;
	bcm_iov_buf_t *resp;
	wl_adps_params_v1_t *data = NULL;

	memset(&iov_buf, 0, sizeof(iov_buf));
	len = OFFSETOF(bcm_iov_buf_t, data) + sizeof(band);

	iov_buf.version = WL_ADPS_IOV_VER;
	iov_buf.len = sizeof(band);
	iov_buf.id = WL_ADPS_IOV_MODE;
	pdata = (uint8 *)iov_buf.data;
	*pdata = band;

	ret = wldev_iovar_getbuf(ndev, "adps", &iov_buf, len, buf, WLC_IOCTL_SMLEN, NULL);
	if (ret < 0) {
		return ret;
	}
	resp = (bcm_iov_buf_t *)buf;
	data = (wl_adps_params_v1_t *)resp->data;

	return data->mode;
}

/*
 * Return value:
 *  Disabled: 0
 *  Enabled: bitmap of WLC_BAND_2G or WLC_BAND_5G when ADPS is enabled at each BAND
 *
 */
int
wl_adps_enabled(struct bcm_cfg80211 *cfg, struct net_device *ndev)
{
	uint8 i;
	int mode;
	int ret = 0;

	for (i = 1; i <= MAX_BANDS; i++) {
		mode = wl_adps_get_mode(ndev, i);
		if (mode > 0) {
			ret |= (1 << i);
		}
	}

	return ret;
}

int
wl_adps_set_suspend(struct bcm_cfg80211 *cfg, struct net_device *ndev, uint8 suspend)
{
	int ret = BCME_OK;

	int buf_len;
	bcm_iov_buf_t *iov_buf = NULL;
	wl_adps_suspend_v1_t *data = NULL;

	buf_len = OFFSETOF(bcm_iov_buf_t, data) + sizeof(*data);
	iov_buf = MALLOCZ(cfg->osh, buf_len);
	if (iov_buf == NULL) {
		WL_ERR(("%s - failed to alloc %d bytes for iov_buf\n",
			__FUNCTION__, buf_len));
		ret = BCME_NOMEM;
		goto exit;
	}

	iov_buf->version = WL_ADPS_IOV_VER;
	iov_buf->len = sizeof(*data);
	iov_buf->id = WL_ADPS_IOV_SUSPEND;

	data = (wl_adps_suspend_v1_t *)iov_buf->data;
	data->version = ADPS_SUB_IOV_VERSION_1;
	data->length = sizeof(*data);
	data->suspend = suspend;

	ret = wldev_iovar_setbuf(ndev, "adps", (char *)iov_buf, buf_len,
		cfg->ioctl_buf, WLC_IOCTL_SMLEN, NULL);
	if (ret < 0) {
		if (ret == BCME_UNSUPPORTED) {
			WL_ERR(("%s - adps suspend is not supported\n", __FUNCTION__));
			ret = BCME_OK;
		}
		else {
			WL_ERR(("%s - fail to set adps suspend %d (%d)\n",
				__FUNCTION__, suspend, ret));
		}
		goto exit;
	}
	WL_INFORM_MEM(("[%s] Detect BAD AP and Suspend ADPS\n",	ndev->name));
exit:
	if (iov_buf) {
		MFREE(cfg->osh, iov_buf, buf_len);
	}
	return ret;
}

bool
wl_adps_bad_ap_check(struct bcm_cfg80211 *cfg, const struct ether_addr *bssid)
{
	if (wl_bad_ap_mngr_find(&cfg->bad_ap_mngr, bssid) != NULL)
		return TRUE;

	return FALSE;
}

s32
wl_adps_event_handler(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	int ret = BCME_OK;
	wl_event_adps_t *event_data = (wl_event_adps_t *)data;

	switch (event_data->type) {
	case WL_E_TYPE_ADPS_BAD_AP:
		ret = wl_event_adps_bad_ap_mngr(cfg, data);
		break;
	default:
		break;
	}

	return ret;
}
