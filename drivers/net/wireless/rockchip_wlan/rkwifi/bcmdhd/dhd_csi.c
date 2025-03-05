/*
 * Broadcom Dongle Host Driver (DHD), Channel State information Module
 *
 * Copyright (C) 2023, Broadcom.
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

#ifdef CSI_SUPPORT
#include <osl.h>

#include <bcmutils.h>

#include <bcmendian.h>
#include <linuxver.h>
#include <linux/list.h>
#include <linux/sort.h>
#include <dngl_stats.h>
#include <wlioctl.h>

#include <bcmevent.h>
#include <dhd.h>
#include <dhd_dbg.h>
#include <dhd_csi.h>
#include <wldev_common.h>
#include <dhd_linux.h>

#define SYNA_ALIGN_BYTES      (sizeof(uint32))
#define SYNA_ALIGN_PADDING    (SYNA_ALIGN_BYTES -1LL)
#define SYNA_ALIGN_MASK       (~SYNA_ALIGN_PADDING)
#define SYNA_ALIGN(value) \
(\
	(SYNA_ALIGN_PADDING + ((long long int)(value))) \
	& SYNA_ALIGN_MASK \
)

static const uint8  _gpMAC_zero[6] = {0, 0, 0, 0, 0, 0};

static int dhd_csi_data_append(uint32 remain_length, uint32 data_length,
	const uint8 *pData, struct csi_cfr_node *ptr)
{
	struct syna_csi_header  *pEntry = NULL;
	int  error_status = 0;
	int  ret = BCME_OK;
	int  append_len = 0;

	if ((!pData) || (!ptr) || (ptr->pNode != ptr)) {
		DHD_ERROR(("%s-%d: *Error, invalid parameter, "
		           "pData=0x%px, ptr=0x%px, pNode=0x%px\n",
		           __func__, __LINE__,
		           pData, ptr, ptr?(ptr->pNode):(NULL)));
		ret = BCME_BADARG;
		goto done;
	} else {
		pEntry = (struct syna_csi_header *)&(ptr->entry);
		error_status = SYNA_CSI_FLAG_ERROR_MASK & pEntry->flags;
	}
	/* find the append length */
	if (remain_length) {
		append_len =   data_length
		             - pEntry->data_length
		             - remain_length;
	} else if (pEntry->remain_length) {
		append_len = pEntry->remain_length;
	} else if (SYNA_CSI_CFR_NODE_FREE_LEN(ptr) >= data_length) {
		append_len = data_length;
	}

	DHD_TRACE(("%s-%d: data_length=%d, remain=%d, append=%d    "
	           "pNode: pEntry=0x%px, global_id=%d, flags=0x%x, "
	           "data_len=%d, remain_len=%d, copied_len=%d\n",
	           __func__, __LINE__,
	           data_length, remain_length, append_len,
	           pEntry, pEntry->global_id, pEntry->flags,
	           pEntry->data_length,
	           pEntry->remain_length, pEntry->copied_length));

	/* copy and update */
	if (append_len) {
		memcpy((pEntry->data_length + ((uint8 *)pEntry->data)),
		       pData, append_len);
		pEntry->data_length += append_len;

		if ((!error_status)
		    && (!remain_length)
		    && (pEntry->checksum)) {
			uint16  checksum = 0;
			uint    i;
			pData = (uint8 *)pEntry->data;
			for (i = 0; i < data_length; i++) {
				checksum += pData[i];
			}
			if (checksum != pEntry->checksum) {
				DHD_ERROR(("%s-%d: *Error, global_id=%d, "
				           "checksum=0x%X mismatch to FW=0x%X, "
				           "data_length=%d, remain_length=%d, "
				           "pData=0x%px\n",
				           __func__, __LINE__,
				           pEntry->global_id,
				           checksum, pEntry->checksum,
				           data_length, remain_length, pData));
				pEntry->flags |= SYNA_CSI_FLAG_ERROR_CHECKSUM;
			}
		}
	} else if (!error_status) {
		DHD_ERROR(("%s-%d: *Error, no data append, "
		           "total_size=%d, free_len=%d, "
		           "in_data_length=%d, in_remain_length=%d, "
		           "    Node: global_id=%d, flags=0x%X, "
		           "data_length=%d, remain_length=%d\n",
		           __func__, __LINE__,
		           ptr->total_size,
		           (int)SYNA_CSI_CFR_NODE_FREE_LEN(ptr),
		           data_length, remain_length,
		           pEntry->global_id, pEntry->flags,
		           pEntry->data_length, pEntry->remain_length));
	} else {
		DHD_ERROR(("%s-%d: *Warning, error report frame, "
		           "size=%d, free=%d, "
		           "in_data_length=%d, in_remain_length=%d, "
		           " Node: global_id=%d, flags=0x%X%s, "
		           "data_length=%d, remain_length=%d\n",
		           __func__, __LINE__,
		           ptr->total_size,
		           (int)SYNA_CSI_CFR_NODE_FREE_LEN(ptr),
		           data_length, remain_length,
		           pEntry->global_id, pEntry->flags,
		             (SYNA_CSI_FLAG_ERROR_CHECKSUM & pEntry->flags)
		           ? "(error_checksum)"
		           : (SYNA_CSI_FLAG_ERROR_NO_ACK & pEntry->flags)
		           ? "(error_no_ack)"
		           : (SYNA_CSI_FLAG_ERROR_READ & pEntry->flags)
		           ? "(error_read)"
		           : (SYNA_CSI_FLAG_ERROR_PS & pEntry->flags)
		           ? "(error_ps)"
		           : (SYNA_CSI_FLAG_ERROR_GENERIC & pEntry->flags)
		           ? "(error_generic)"
		           : "",
		           pEntry->data_length, pEntry->remain_length));
	}

	/* make this as final step to avoid packet early retrieved */
	pEntry->copied_length = 0;
	pEntry->remain_length = remain_length;

	ret = append_len;

done:
	return ret;
}

static int dhd_csi_data_new_v0(dhd_pub_t *dhdp, const wl_event_msg_t *event,
	struct dhd_cfr_header_v0 *pEvent)
{
	struct csi_cfr_node    *ptr = NULL;
	struct syna_csi_header *pEntry = NULL;
	struct timespec64 ts;
	dhd_if_t *ifp = NULL;
	int       ret = BCME_ERROR;
	uint      total_size = 0;
	u32       chanspec = 0;
	uint32    remain_length = 0;
	uint32    data_length = 0;
	uint8    *pData = NULL;

	ktime_get_real_ts64(&ts);
	total_size = SYNA_CSI_CFR_NODE_LEN + ltoh32_ua(&(pEvent->cfr_dump_length));

	ptr = (struct csi_cfr_node *)MALLOCZ(dhdp->osh, total_size);
	if (!ptr) {
		DHD_ERROR(("%s-%d: *Error, malloc %d for cfr dump list error\n",
		           __func__, __LINE__, total_size));
		return BCME_NOMEM;
	}
	ptr->pNode = ptr; /* Not useful */
	ptr->total_size = total_size;
	pEntry = &(ptr->entry);

	DHD_TRACE(("%s-%d: ptr=0x%p, ptr=0x%px, pEntry=0x%px, "
	           "delta=%d, header_size=%d\n",
	           __func__, __LINE__, ptr, ptr, pEntry,
	           (int)((uintptr)pEntry - (uintptr)ptr),
	           (int)SYNA_CSI_CFR_NODE_LEN));

	/* process one by one for alignment consideration */
	pEntry->magic_flag = CONST_SYNA_CSI_MAGIC_FLAG;
	pEntry->version = CONST_SYNA_CSI_COMMON_HEADER_VERSION;
	pEntry->format_type = SYNA_CSI_FORMAT_Q8;
	pEntry->fc = 0;
	if (pEvent->status) {
		pEntry->flags |= SYNA_CSI_FLAG_ERROR_GENERIC;
	}
	memcpy(pEntry->client_ea, pEvent->peer_macaddr, ETHER_ADDR_LEN);

	ifp = dhd_get_ifp(dhdp, event->ifidx);
	if (!ifp) {
		DHD_ERROR(("%s-%d: *Error, get ifp error\n",
		           __func__, __LINE__));
	} else {
		memcpy(pEntry->bsscfg_ea, ifp->mac_addr, ETHER_ADDR_LEN);
	}

	if ((ret = dhd_iovar(dhdp, event->ifidx, "chanspec",
	                     NULL, 0, (char*)&chanspec,
	                     sizeof(chanspec), FALSE) != BCME_OK)) {
		pEntry->band = SYNA_CSI_BAND_UNKNOWN;
		pEntry->bandwidth = SYNA_CSI_BW_UNKNOWN;
		pEntry->channel = 0;
	} else {
		switch (CHSPEC_BAND(chanspec)) {
		case WL_CHANSPEC_BAND_2G:
			pEntry->band = SYNA_CSI_BAND_2G;
			break;
		case WL_CHANSPEC_BAND_5G:
			pEntry->band = SYNA_CSI_BAND_5G;
			break;
		case WL_CHANSPEC_BAND_6G:
			pEntry->band = SYNA_CSI_BAND_6G;
			break;
		default:
			pEntry->band = SYNA_CSI_BAND_UNKNOWN;
			break;
		}

		switch (CHSPEC_BW(chanspec)) {
		case WL_CHANSPEC_BW_20:
			pEntry->bandwidth = SYNA_CSI_BW_20MHz;
			break;
		case WL_CHANSPEC_BW_40:
			pEntry->bandwidth = SYNA_CSI_BW_40MHz;
			break;
		case WL_CHANSPEC_BW_80:
			pEntry->bandwidth = SYNA_CSI_BW_80MHz;
			break;
		case WL_CHANSPEC_BW_160:
			pEntry->bandwidth = SYNA_CSI_BW_160MHz;
			break;
		case WL_CHANSPEC_BW_320:
			pEntry->bandwidth = SYNA_CSI_BW_320MHz;
			break;
		default:
			pEntry->bandwidth = SYNA_CSI_BAND_UNKNOWN;
			break;
		}

		pEntry->channel = CHSPEC_CHANNEL(chanspec);
	}

	pEntry->num_txstream = pEvent->sts;
	pEntry->num_rxchain = pEvent->num_rx;
	pEntry->num_subcarrier = ltoh16_ua(&(pEvent->num_carrier));
	pEntry->report_tsf = (uint64)TIMESPEC64_TO_US(ts);
	pEntry->rssi = pEvent->rssi;
	pEntry->data_length = 0;
	pEntry->remain_length = 0;
	pEntry->copied_length = 0;

	DHD_TRACE(("%s-%d: Entry data_size=%d, remain_size=%d, @%llu\n",
	           __func__, __LINE__,
	           ltoh32_ua(&(pEvent->cfr_dump_length)),
	           ltoh32_ua(&(pEvent->remain_length)),
	           ltoh64_ua(&(pEntry->report_tsf))));

	remain_length = ltoh32_ua(&(pEvent->remain_length));
	data_length = ltoh32_ua(&(pEvent->cfr_dump_length));
	pData = (uint8 *)pEvent->data;
	ret = dhd_csi_data_append(remain_length, data_length, pData, ptr);
	if (0 <= ret) {
		INIT_LIST_HEAD(&(ptr->list));
		mutex_lock(&dhdp->csi_lock);
		list_add_tail(&(ptr->list), &dhdp->csi_list);
		dhdp->csi_count++;
		mutex_unlock(&dhdp->csi_lock);
	} else {
		MFREE(dhdp->osh, ptr, total_size);
	}

	return ret;
}

static int dhd_csi_data_new_v1(dhd_pub_t *dhdp, const wl_event_msg_t *event,
	struct dhd_cfr_header_v1 *pEvent)
{
	struct csi_cfr_node    *ptr = NULL;
	struct syna_csi_header *pEntry = NULL;
	dhd_if_t *ifp = NULL;
	int       ret = BCME_ERROR;
	uint      total_size = 0;
	u32       chanspec = 0;
	uint32    remain_length = 0;
	uint32    data_length = 0;
	uint8    *pData = NULL;

	total_size = SYNA_CSI_CFR_NODE_LEN + ltoh32_ua(&(pEvent->cfr_dump_length));

	ptr = (struct csi_cfr_node *)MALLOCZ(dhdp->osh, total_size);
	if (!ptr) {
		DHD_ERROR(("%s-%d: *Error, malloc %d for cfr dump list error\n",
		           __func__, __LINE__, total_size));
		return BCME_NOMEM;
	}
	ptr->pNode = ptr; /* Not useful */
	ptr->total_size = total_size;
	pEntry = &(ptr->entry);

	DHD_TRACE(("%s-%d: ptr=0x%p, ptr=0x%px, pEntry=0x%px, "
	           "delta=%d, header_size=%d\n",
	           __func__, __LINE__, ptr, ptr, pEntry,
	           (int)((uintptr)pEntry - (uintptr)ptr),
	           (int)SYNA_CSI_CFR_NODE_LEN));

	/* process one by one for alignment consideration */
	pEntry->magic_flag = CONST_SYNA_CSI_MAGIC_FLAG;
	pEntry->version = CONST_SYNA_CSI_COMMON_HEADER_VERSION;
	pEntry->format_type = SYNA_CSI_FORMAT_Q8;
	pEntry->fc = pEvent->fc;
	if (pEvent->status) {
		pEntry->flags |= SYNA_CSI_FLAG_ERROR_GENERIC;
	}

	memcpy(pEntry->client_ea, pEvent->peer_macaddr, ETHER_ADDR_LEN);

	ifp = dhd_get_ifp(dhdp, event->ifidx);

	if (!ifp) {
		DHD_ERROR(("%s-%d: *Error, get ifp error\n",
		           __func__, __LINE__));
	} else {
		memcpy(pEntry->bsscfg_ea, ifp->mac_addr, ETHER_ADDR_LEN);
	}

	if ((ret = dhd_iovar(dhdp, event->ifidx, "chanspec",
		NULL, 0, (char*)&chanspec, sizeof(chanspec), FALSE) != BCME_OK)) {
		pEntry->band = SYNA_CSI_BAND_UNKNOWN;
		pEntry->bandwidth = SYNA_CSI_BW_UNKNOWN;
		pEntry->channel = 0;
	} else {
		switch (CHSPEC_BAND(chanspec)) {
		case WL_CHANSPEC_BAND_2G:
			pEntry->band = SYNA_CSI_BAND_2G;
			break;
		case WL_CHANSPEC_BAND_5G:
			pEntry->band = SYNA_CSI_BAND_5G;
			break;
		case WL_CHANSPEC_BAND_6G:
			pEntry->band = SYNA_CSI_BAND_6G;
			break;
		default:
			pEntry->band = SYNA_CSI_BAND_UNKNOWN;
			break;
		}

		switch (CHSPEC_BW(chanspec)) {
		case WL_CHANSPEC_BW_20:
			pEntry->bandwidth = SYNA_CSI_BW_20MHz;
			break;
		case WL_CHANSPEC_BW_40:
			pEntry->bandwidth = SYNA_CSI_BW_40MHz;
			break;
		case WL_CHANSPEC_BW_80:
			pEntry->bandwidth = SYNA_CSI_BW_80MHz;
			break;
		case WL_CHANSPEC_BW_160:
			pEntry->bandwidth = SYNA_CSI_BW_160MHz;
			break;
		case WL_CHANSPEC_BW_320:
			pEntry->bandwidth = SYNA_CSI_BW_320MHz;
			break;
		default:
			pEntry->bandwidth = SYNA_CSI_BAND_UNKNOWN;
			break;
		}

		pEntry->channel = CHSPEC_CHANNEL(chanspec);
	}

	pEntry->num_txstream = pEvent->sts;
	pEntry->num_rxchain = pEvent->num_rx;
	pEntry->num_subcarrier = ltoh16_ua(&(pEvent->num_carrier));
	pEntry->report_tsf = ltoh64_ua(&(pEvent->cfr_timestamp));
	pEntry->rssi = pEvent->rssi;
	pEntry->data_length = 0;
	pEntry->remain_length = 0;
	pEntry->copied_length = 0;

	DHD_TRACE(("%s-%d: Entry data_size=%d, remain_size=%d, @%llu\n",
	           __func__, __LINE__,
	           ltoh32_ua(&(pEvent->cfr_dump_length)),
	           ltoh32_ua(&(pEvent->remain_length)),
	           ltoh64_ua(&(pEntry->report_tsf))));

	remain_length = ltoh32_ua(&(pEvent->remain_length));
	data_length = ltoh32_ua(&(pEvent->cfr_dump_length));
	pData = (uint8 *)pEvent->data;
	ret = dhd_csi_data_append(remain_length, data_length, pData, ptr);
	if (0 <= ret) {
		INIT_LIST_HEAD(&(ptr->list));
		mutex_lock(&dhdp->csi_lock);
		list_add_tail(&(ptr->list), &dhdp->csi_list);
		dhdp->csi_count++;
		mutex_unlock(&dhdp->csi_lock);
	} else {
		MFREE(dhdp->osh, ptr, total_size);
	}

	return ret;
}

static int dhd_csi_data_new_v2(dhd_pub_t *dhdp, const wl_event_msg_t *event,
	struct dhd_cfr_header_v2 *pEvent)
{
	struct csi_cfr_node    *ptr = NULL;
	struct syna_csi_header *pEntry = NULL;
	int     ret = BCME_ERROR;
	uint    total_size = 0;
	uint32  remain_length = 0;
	uint32  data_length = 0;
	uint8  *pData = NULL;

	if (SYNA_ALIGN_PADDING & (uintptr)pEvent) {
		DHD_ERROR(("%s-%d: *Error, pEvent=0x%px is not aligned!",
		           __func__, __LINE__, pEvent));
	}

	total_size = SYNA_CSI_CFR_NODE_LEN + pEvent->data_length;

	ptr = (struct csi_cfr_node *)MALLOCZ(dhdp->osh, total_size);
	if (!ptr) {
		DHD_ERROR(("%s-%d: *Error, malloc %d for cfr dump list error\n",
		           __func__, __LINE__, total_size));
		return BCME_NOMEM;
	}
	ptr->pNode = ptr; /* Not useful */
	ptr->total_size = total_size;
	pEntry = &(ptr->entry);

	DHD_TRACE(("%s-%d: ptr=0x%p, ptr=0x%px, pEntry=0x%px, "
	           "delta=%d, header_size=%d\n",
	           __func__, __LINE__, ptr, ptr, pEntry,
	           (int)((uintptr)pEntry - (uintptr)ptr),
	           (int)SYNA_CSI_CFR_NODE_LEN));

	/* process one by one for alignment consideration */
	memcpy(pEntry, pEvent, sizeof(struct syna_csi_header));
	pEntry->magic_flag = CONST_SYNA_CSI_MAGIC_FLAG;
	pEntry->version = CONST_SYNA_CSI_COMMON_HEADER_VERSION;

	if ((pEvent->status_compat)
	    && (!(SYNA_CSI_FLAG_ERROR_MASK & pEntry->flags))) {
		pEntry->flags |= SYNA_CSI_FLAG_ERROR_GENERIC;
		DHD_ERROR(("%s-%d: *Warning, status=0x%X, "
		           " global_id=%d!\n",
		           __func__, __LINE__,
		           pEvent->status_compat,
		           pEvent->global_id));
	}
	if (!pEntry->format_type) {
		pEntry->format_type = SYNA_CSI_FORMAT_Q8;
	}
	if (!memcmp(_gpMAC_zero, pEntry->bsscfg_ea, ETHER_ADDR_LEN)) {
		dhd_if_t *ifp = dhd_get_ifp(dhdp, event->ifidx);
		if (!ifp) {
			DHD_ERROR(("%s-%d: *Error, get ifp error\n",
			           __func__, __LINE__));
		} else {
			memcpy(pEntry->bsscfg_ea, ifp->mac_addr, ETHER_ADDR_LEN);
		}
	}

	if (!pEntry->report_tsf) {
		struct timespec64  ts;
		ktime_get_real_ts64(&ts);
		pEntry->report_tsf = (uint64)TIMESPEC64_TO_US(ts);
	}

	pEntry->data_length = 0;
	pEntry->remain_length = 0;
	pEntry->copied_length = 0;
	pEntry->checksum = pEvent->checksum;

	DHD_TRACE(("%s-%d: Entry data_size=%d, remain_size=%d, @%llu\n",
	           __func__, __LINE__,
	           pEvent->data_length,
	           pEvent->remain_length,
	           pEntry->report_tsf));

	remain_length = pEvent->remain_length;
	data_length = pEvent->data_length;
	pData = (uint8 *)pEvent->data;
	ret = dhd_csi_data_append(remain_length, data_length, pData, ptr);
	if (0 <= ret) {
		INIT_LIST_HEAD(&(ptr->list));
		mutex_lock(&dhdp->csi_lock);
		list_add_tail(&(ptr->list), &dhdp->csi_list);
		dhdp->csi_count++;
		mutex_unlock(&dhdp->csi_lock);
	} else {
		MFREE(dhdp->osh, ptr, total_size);
	}

	return ret;
}

int dhd_csi_event_handler(dhd_pub_t *dhdp, const wl_event_msg_t *event, void *event_data)
{
	union dhd_cfr_header  *pEvent = NULL;
	struct csi_cfr_node   *ptr = NULL, *next = NULL, *save_ptr = NULL;
	const uint8  *pMAC = NULL, *pData = NULL;
	uint8  version = 0xff;
	uint32 data_length = 0, remain_length = 0, header_length = 0;
	int    ret = BCME_OK;

	NULL_CHECK(dhdp, "dhdp is NULL", ret);

	DHD_TRACE(("%s-%d: Enter\n", __func__, __LINE__));

	if (!event_data) {
		DHD_ERROR(("%s-%d: event_data is NULL\n",
		           __func__, __LINE__));
		ret = BCME_BADARG;
		goto done;
	}

	pEvent = (union dhd_cfr_header *)event_data;
	if (pEvent->header_v0.status) {
		DHD_ERROR(("%s-%d: *Warning, status=0x%X "
		           " may indicate error!\n",
		           __func__, __LINE__,
		           pEvent->header_v0.status));
		/* TODO: do need to abandon this scenario? */
	}

	version = pEvent->header_v0.version;
	switch (version) {
	case CSI_VERSION_V0:
		pMAC = pEvent->header_v0.peer_macaddr;
		header_length = sizeof(struct dhd_cfr_header_v0);
		remain_length = ltoh32_ua(&(pEvent->header_v0.remain_length));
		data_length = ltoh32_ua(&(pEvent->header_v0.cfr_dump_length));
		pData = (uint8 *)pEvent->header_v0.data;
		break;
	case CSI_VERSION_V1:
		pMAC = pEvent->header_v1.peer_macaddr;
		header_length = sizeof(struct dhd_cfr_header_v1);
		remain_length = ltoh32_ua(&(pEvent->header_v1.remain_length));
		data_length = ltoh32_ua(&(pEvent->header_v1.cfr_dump_length));
		pData = (uint8 *)pEvent->header_v1.data;
		break;
	case CSI_VERSION_V2:
		pMAC = pEvent->header_v2.client_ea;
		header_length = sizeof(struct dhd_cfr_header_v2);
		remain_length = pEvent->header_v2.remain_length;
		data_length = pEvent->header_v2.data_length;
		pData = (uint8 *)pEvent->header_v2.data;
		break;
	default:
		DHD_ERROR(("%s-%d: CSI version=%d error\n",
		           __func__, __LINE__, version));
		ret = BCME_BADOPTION;
		goto done;
		break;
	}

	if (header_length <= remain_length) {
		remain_length -= header_length;
	}
	if (CONST_CSI_MAXIMUM_DATA_BYTES < remain_length) {
		DHD_ERROR(("%s-%d: *Error, drop too big length, "
		           "global_id=%d, version=%d, status=%d, "
		           "data_length=%d(0x%X), remain_length=%d(0x%X) > %d\n",
		           __func__, __LINE__,
		           pEvent->header_v2.global_id,
		           version, pEvent->header_v2.status_compat,
		           data_length, data_length,
		           remain_length, remain_length,
		           (int)CONST_CSI_MAXIMUM_DATA_BYTES));
		prhex("TOO_BIG_LENGTH", (uchar *)pEvent, 128);
		ret = BCME_BADLEN;
		goto done;
	}

	mutex_lock(&dhdp->csi_lock);
	/* check if this addr exist */
	if (!list_empty(&dhdp->csi_list)) {
		list_for_each_entry_safe(ptr, next, &dhdp->csi_list, list) {
			struct syna_csi_header *pEntry = &(ptr->entry);
			/* check if is new */
			if (bcmp(&pEntry->client_ea, pMAC, ETHER_ADDR_LEN) == 0) {
				if (pEntry->remain_length > 0) {
					save_ptr = ptr;
					break;
				}
			}
		}
	}
	/* check if need to drop head packet */
	if ((!save_ptr) && (dhdp->csi_count >= MAX_CSI_BUF_NUM)) {
		DHD_ERROR(("%s-%d: CSI data is full, drop pkt \n",
		           __func__, __LINE__));
		ptr = list_first_entry(&dhdp->csi_list, struct csi_cfr_node, list);
		list_del_init(&ptr->list);
		MFREE(dhdp->osh, ptr, ptr->total_size);
		dhdp->csi_count--;
	}
	mutex_unlock(&dhdp->csi_lock);

	/* try to append new node if needs */
	DHD_TRACE(("%s-%d: save_ptr=0x%px, version=%d\n",
	           __func__, __LINE__, save_ptr, version));
	if (!save_ptr) {
		switch (version) {
		case CSI_VERSION_V0:
			ret = dhd_csi_data_new_v0(dhdp, event, &(pEvent->header_v0));
			break;
		case CSI_VERSION_V1:
			ret = dhd_csi_data_new_v1(dhdp, event, &(pEvent->header_v1));
			break;
		case CSI_VERSION_V2:
			ret = dhd_csi_data_new_v2(dhdp, event, &(pEvent->header_v2));
			break;
		default:
			DHD_ERROR(("%s-%d: CSI version=%d error\n",
			           __func__, __LINE__, version));
			ret = BCME_BADOPTION;
			goto done;
			break;
		}
	} else {
		ret = dhd_csi_data_append(remain_length, data_length, pData, save_ptr);
	}

done:
	return ret;
}

static void
dhd_csi_clean_list(dhd_pub_t *dhdp)
{
	struct csi_cfr_node *ptr, *next;
	int num = 0;

	if (!dhdp) {
		DHD_ERROR(("%s-%d: *Error, NULL POINTER\n",
		           __func__, __LINE__));
		return;
	}

	mutex_lock(&dhdp->csi_lock);
	if (!list_empty(&dhdp->csi_list)) {
		list_for_each_entry_safe(ptr, next, &dhdp->csi_list, list) {
			list_del(&ptr->list);
			num++;
			MFREE(dhdp->osh, ptr, ptr->total_size);
		}
	}
	dhdp->csi_count = 0;
	mutex_unlock(&dhdp->csi_lock);

	DHD_TRACE(("%s-%d: Clean up %d record\n",
	           __func__, __LINE__, num));
}

int dhd_csi_config(dhd_pub_t *dhdp, char *pBuf, uint length, bool is_set)
{
	int  ret = BCME_OK;

	if ((!dhdp) || (!pBuf) || (sizeof(uint32) > length)) {
		DHD_ERROR(("%s-%d: *Error, Bad argument\n",
		           __func__, __LINE__));
		ret = BCME_BADARG;
		goto done;
	}

	if (is_set) {
		uint  value = *((uint *)pBuf);
		if (SYNA_CSI_DATA_MODE_LAST <= value) {
			ret = BCME_BADOPTION;
			goto done;
		} else if (SYNA_CSI_DATA_MODE_NONE != value) {
			dhdp->csi_data_send_manner = value;
			dhd_csi_clean_list(dhdp);
		}
	} else {
		memcpy(pBuf, &dhdp->csi_data_send_manner, sizeof(uint32));
	}

done:
	return ret;
}

int dhd_csi_init(dhd_pub_t *dhdp)
{
	int err = BCME_OK;

	mutex_init(&dhdp->csi_lock);
	mutex_lock(&dhdp->csi_lock);
	NULL_CHECK(dhdp, "dhdp is NULL", err);
	INIT_LIST_HEAD(&dhdp->csi_list);
	dhdp->csi_count = 0;
	mutex_unlock(&dhdp->csi_lock);

	return err;
}

int
dhd_csi_deinit(dhd_pub_t *dhdp)
{
	int err = BCME_OK;

	NULL_CHECK(dhdp, "dhdp is NULL", err);

	dhd_csi_clean_list(dhdp);

	mutex_destroy(&dhdp->csi_lock);

	return err;
}

int dhd_csi_retrieve_queue_data(dhd_pub_t *dhdp, char *buf, uint count)
{
	struct csi_cfr_node *ptr = NULL, *next = NULL;
	int   ret = BCME_OK;
	char *pbuf = buf;
	int   num = 0;
	int   copy_len = 0;
	int   left_data = 0;
	int   total_copy_len = 0;

	NULL_CHECK(dhdp, "dhdp is NULL", ret);

	mutex_lock(&dhdp->csi_lock);
	if (!list_empty(&dhdp->csi_list)) {
		list_for_each_entry_safe(ptr, next, &dhdp->csi_list, list) {
			struct syna_csi_header  *pEntry = &(ptr->entry);
			if (pEntry->remain_length) {
				DHD_ERROR(("%s-%d: *Warning, data not ready "
				           "for %02X:%02X:%02X:%02X:%02X:%02X,"
				           "global_id=%d, data_length=%d, "
				           "remain_length=%d\n",
				           __func__, __LINE__,
				           pEntry->client_ea[0],
				           pEntry->client_ea[1],
				           pEntry->client_ea[2],
				           pEntry->client_ea[3],
				           pEntry->client_ea[4],
				           pEntry->client_ea[5],
				           pEntry->global_id,
				           pEntry->data_length,
				           pEntry->remain_length));
				continue;
			}

			left_data =   SYNA_CSI_PKT_TOTAL_LEN(pEntry)
			            - pEntry->copied_length;
			copy_len = count;

			if (0 > left_data) {
				DHD_ERROR(("%s-%d: *Error, invalid case, "
				           "total_len=%d, data_len=%d, "
				           "remain_len=%d, copied_len=%d\n",
				           __func__, __LINE__,
				           (int)SYNA_CSI_PKT_TOTAL_LEN(pEntry),
				           pEntry->data_length,
				           pEntry->remain_length,
				           pEntry->copied_length));
				copy_len = 0;
			} else if (copy_len > left_data) {
				copy_len = left_data;
			}

			DHD_TRACE(("%s-%d: Packet[%d]: "
			           "left_room=%d, copy_len=%d, "
			           "pNode_data_len=%d, pNode_reamin_len=%d, "
			           "pNode_copied_len=%d, csi_count=%d\n",
			           __func__, __LINE__,
			           num, count, copy_len,
			           pEntry->data_length,
			           pEntry->remain_length,
			           pEntry->copied_length,
			           dhdp->csi_count));

			if (0 < copy_len) {
				memcpy(pbuf,
				       pEntry->copied_length + (uint8 *)pEntry,
				       copy_len);
				pEntry->copied_length += copy_len;
				count -= copy_len;
				pbuf  += copy_len;
				num++;
			}

			if (pEntry->copied_length >= SYNA_CSI_PKT_TOTAL_LEN(pEntry)) {
				list_del(&ptr->list);
				MFREE(dhdp->osh, ptr, ptr->total_size);
				dhdp->csi_count--;
			}

			if (0 >= copy_len) {
				break;
			}
		}
	}
	mutex_unlock(&dhdp->csi_lock);
	total_copy_len = pbuf - buf;
	DHD_TRACE(("%s-%d: dump %d record %d bytes\n",
	           __func__, __LINE__, num, total_copy_len));

	return total_copy_len;
}
#endif /* CSI_SUPPORT */
