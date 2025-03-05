/*
 * IE/TLV fragmentation/defragmentation support for
 * Broadcom 802.11bang Networking Device Driver
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

#include <bcmutils.h>
#include <frag.h>
#include <802.11.h>
#include <bcmstdlib_s.h>

/* defrag a fragmented dot11 ie/tlv. if space does not permit, return the needed
 * ie length to contain all the fragments with status BCME_BUFTOOSHORT.
 * out_len is in/out parameter, max length on input, used/required length on output
 * data_len is out parameter, it has the length of de-fragmented data.
 */
int
bcm_tlv_dot11_defrag(const void *buf, uint buf_len, uint8 id, bool id_ext,
	uint8 *out, uint *out_len, uint *data_len)
{
	int err = BCME_OK;
	const bcm_tlv_t *ie;
	uint tot_len = 0;
	uint tot_data_len = 0;
	uint out_left;

	/* find the ie; includes validation */
	ie = bcm_parse_tlvs_dot11(buf, buf_len, id, id_ext);
	if (!ie) {
		err = BCME_IE_NOTFOUND;
		goto done;
	}

	out_left =  (out && out_len) ? *out_len : 0;

	/* first fragment */
	tot_len = id_ext ? ie->len - 1u : ie->len;
	tot_data_len = tot_len;

	/* copy out if output space permits */
	if ((out == NULL) || (out_left < tot_len)) {
		err = BCME_BUFTOOSHORT;
		out_left = 0; /* prevent further copy */
	} else {
		if ((err = memcpy_s(out, out_left, &ie->data[id_ext ? 1u : 0], tot_len))
			!= BCME_OK) {
			goto fail;
		}
		out += tot_len;
		out_left -= tot_len;
	}

	/* if not fragmened or not fragmentable per 802.11 table  9-77 11md0.1 bail
	 * we can introduce the latter check later
	 */
	if (ie->len != BCM_TLV_MAX_DATA_SIZE) {
		goto done;
	}

	/* adjust buf_len to length after ie including it */
	buf_len -= (uint)(((const uint8 *)ie - (const uint8 *)buf));

	/* update length from fragments, okay if no next ie */
	while ((ie = bcm_next_tlv(ie, &buf_len)) &&
			(ie->id == DOT11_MNG_FRAGMENT_ID)) {
		/* note: buf_len starts at next ie and last frag may be partial */
		if ((out == NULL) || (out_left < ie->len)) {
			err = BCME_BUFTOOSHORT;
			out_left = 0;
		} else {
			if ((err = memcpy_s(out, out_left, &ie->data[0], ie->len)) != BCME_OK) {
				goto fail;
			}
			out += ie->len;
			out_left -= ie->len;
		}
		tot_data_len += ie->len;
		tot_len += ie->len + BCM_TLV_HDR_SIZE;

		/* all but last should be of max size */
		if (ie->len < BCM_TLV_MAX_DATA_SIZE) {
			break;
		}
	}

done:
	if (out_len) {
		*out_len = tot_len;
	}
	if (data_len) {
		*data_len = tot_data_len;
	}
fail:
	return err;
}

int
bcm_tlv_dot11_frag_tot_len(const void *buf, uint buf_len,
	uint8 id, bool id_ext, uint *ie_len)
{
	return  bcm_tlv_dot11_defrag(buf, buf_len, id, id_ext, NULL, ie_len, NULL);
}

int
bcm_tlv_dot11_frag_tot_data_len(const void *buf, uint buf_len,
	uint8 id, bool id_ext, uint *ie_len, uint *data_len)
{
	return  bcm_tlv_dot11_defrag(buf, buf_len, id, id_ext, NULL, ie_len, data_len);
}

/* To support IE fragmentation.
 * data will be fit into a series of elements consisting of one leading element
 * followed by Fragment elements if needed.
 * data_len is in/out parameter, it has the length of all the elements, including
 * the leading element header size.
 * Caller can get elements length by passing dst as NULL.
 */
int
bcm_tlv_dot11_frag(uint8 id, bool id_ext, const void *data, uint data_len,
	uint8 *buf, uint *buf_len)
{
	uint8 id_ext_len = id_ext ? 1u : 0;
	bool is_write = (buf != NULL);
	uint8 *ie_data = NULL;
	uint ie_len = 0;
	uint8 num_frags = 0, last_frag_size = 0;
	int err = BCME_OK;

	if ((data_len + id_ext_len) >
		BCM_TLV_MAX_DATA_SIZE) {
		last_frag_size = (data_len + id_ext_len) % BCM_TLV_MAX_DATA_SIZE;
		num_frags = CEIL(data_len + id_ext_len, BCM_TLV_MAX_DATA_SIZE);
	} else {
		last_frag_size = data_len + id_ext_len;
		num_frags = 1u;
	}
	ie_len = data_len + id_ext_len + num_frags * BCM_TLV_HDR_SIZE;

	if (is_write) {
		uint len_to_copy = (num_frags > 1u) ? (uint)(BCM_TLV_MAX_DATA_SIZE - id_ext_len) :
			data_len;
		uint copied_len = 0;
		if (ie_len > *buf_len) {
			err = BCME_BUFTOOSHORT;
			goto done;
		}
		if (id_ext) {
			ie_data = bcm_write_tlv_ext(DOT11_MNG_ID_EXT_ID, id,
				data, len_to_copy, buf);
		} else {
			ie_data = bcm_write_tlv(id, data, len_to_copy, buf);
		}
		while (num_frags > 1u) {
			copied_len += len_to_copy;
			len_to_copy = (num_frags > 2u) ? BCM_TLV_MAX_DATA_SIZE : last_frag_size;
			ie_data = bcm_write_tlv(DOT11_MNG_FRAGMENT_ID,
				(const uint8 *)data + copied_len,
				len_to_copy, ie_data);
			num_frags --;
		};
	}
done:
	*buf_len = ie_len;
	return err;
}
