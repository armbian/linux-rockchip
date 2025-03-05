/*
 * DHD debug ring API and structures - implementation
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
#include <dhd_dbg_ring.h>
#include <dhd_debug.h>

dhd_dbg_ring_t *
dhd_dbg_ring_alloc_init(dhd_pub_t *dhd, uint16 ring_id,
	char *ring_name, uint32 ring_sz, void *allocd_buf,
	bool pull_inactive)
{
	dhd_dbg_ring_t *ring = NULL;
	int ret = 0;
	unsigned long flags = 0;

	ring = MALLOCZ(dhd->osh, sizeof(dhd_dbg_ring_t));
	if (!ring)
		goto fail;

	ret = dhd_dbg_ring_init(dhd, ring, ring_id,
			(uint8 *)ring_name, ring_sz,
			allocd_buf, pull_inactive);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s: unable to init ring %s!\n",
				__FUNCTION__, ring_name));
		goto fail;
	}
	DHD_DBG_RING_LOCK(ring->lock, flags);
	ring->state = RING_ACTIVE;
	ring->threshold = 0;
	DHD_DBG_RING_UNLOCK(ring->lock, flags);

	return ring;

fail:
	if (ring) {
		dhd_dbg_ring_deinit(dhd, ring);
		ring->ring_buf = NULL;
		ring->ring_size = 0;
		MFREE(dhd->osh, ring, sizeof(dhd_dbg_ring_t));
	}
	return NULL;
}

void
dhd_dbg_ring_dealloc_deinit(void **ring_ptr, dhd_pub_t *dhd)
{
	dhd_dbg_ring_t *ring = NULL;
	dhd_dbg_ring_t **dbgring = (dhd_dbg_ring_t **)ring_ptr;

	if (!dbgring)
		return;

	ring = *dbgring;

	if (ring) {
		dhd_dbg_ring_deinit(dhd, ring);
		ring->ring_buf = NULL;
		ring->ring_size = 0;
		MFREE(dhd->osh, ring, sizeof(dhd_dbg_ring_t));
		*dbgring = NULL;
	}
}

int
dhd_dbg_ring_init(dhd_pub_t *dhdp, dhd_dbg_ring_t *ring, uint16 id, uint8 *name,
		uint32 ring_sz, void *allocd_buf, bool pull_inactive)
{
	void *buf;
	unsigned long flags = 0;

	if (allocd_buf == NULL) {
		/* for DEBUG_DUMP and MEM_DUMP, buffer can be NULL
		 * since act as delayed allocation or fake rings
		 */
		if (id != DEBUG_DUMP_RING1_ID && id != DEBUG_DUMP_RING2_ID &&
				id != MEM_DUMP_RING_ID) {
			return BCME_NOMEM;
		}
		buf = NULL;
	} else {
		buf = allocd_buf;
	}

	ring->lock = DHD_DBG_RING_LOCK_INIT(dhdp->osh);

	DHD_DBG_RING_LOCK(ring->lock, flags);
	ring->id = id;
	strlcpy((char *)ring->name, (char *)name, sizeof(ring->name));
	ring->ring_size = ring_sz;
	ring->wp = ring->rp = 0;
	ring->ring_buf = buf;
	ring->threshold = DBGRING_FLUSH_THRESHOLD(ring);
	ring->state = RING_SUSPEND;
	ring->rem_len = 0;
	ring->sched_pull = TRUE;
	ring->pull_inactive = pull_inactive;
	DHD_DBG_RING_UNLOCK(ring->lock, flags);

	return BCME_OK;
}

int
dhd_dbg_ring_set_buf(dhd_pub_t *dhdp, dhd_dbg_ring_t *ring, void *buf)
{
	unsigned long flags = 0;

	DHD_DBG_RING_LOCK(ring->lock, flags);
	ring->ring_buf = buf;
	DHD_DBG_RING_UNLOCK(ring->lock, flags);

	return BCME_OK;
}
void
dhd_dbg_ring_deinit(dhd_pub_t *dhdp, dhd_dbg_ring_t *ring)
{
	unsigned long flags = 0;

	DHD_DBG_RING_LOCK(ring->lock, flags);
	ring->id = 0;
	ring->name[0] = 0;
	ring->wp = ring->rp = 0;
	memset(&ring->stat, 0, sizeof(ring->stat));
	ring->threshold = 0;
	ring->state = RING_STOP;
	DHD_DBG_RING_UNLOCK(ring->lock, flags);

	DHD_DBG_RING_LOCK_DEINIT(dhdp->osh, ring->lock);
}

void
dhd_dbg_ring_sched_pull(dhd_dbg_ring_t *ring, uint32 pending_len,
		os_pullreq_t pull_fn, void *os_pvt, const int id)
{
	unsigned long flags = 0;

	DHD_DBG_RING_LOCK(ring->lock, flags);
	/* if the current pending size is bigger than threshold and
	 * threshold is set
	 */
	if (ring->threshold > 0 &&
	   (pending_len >= ring->threshold) && ring->sched_pull) {
		/*
		 * Update the state and release the lock before calling
		 * the pull_fn. Do not transfer control to other layers
		 * with locks held. If the call back again calls into
		 * the same layer fro this context, can lead to deadlock.
		 */
		ring->sched_pull = FALSE;
		DHD_DBG_RING_UNLOCK(ring->lock, flags);
		pull_fn(os_pvt, id);
	} else {
		DHD_DBG_RING_UNLOCK(ring->lock, flags);
	}
}

uint32
dhd_dbg_ring_get_pending_len(dhd_dbg_ring_t *ring)
{
	uint32 pending_len = 0;
	unsigned long flags = 0;

	DHD_DBG_RING_LOCK(ring->lock, flags);
	if (ring->stat.written_bytes > ring->stat.read_bytes) {
		pending_len = ring->stat.written_bytes - ring->stat.read_bytes;
	} else if (ring->stat.written_bytes < ring->stat.read_bytes) {
		pending_len = PENDING_LEN_MAX - ring->stat.read_bytes + ring->stat.written_bytes;
	} else {
		pending_len = 0;
	}
	DHD_DBG_RING_UNLOCK(ring->lock, flags);

	return pending_len;
}

#ifdef DHD_PKT_LOGGING_DBGRING
int
dhd_dbg_ring_update(void *dbg_ring, uint32 w_len)
{
	unsigned long flags;
	dhd_dbg_ring_t *ring = (dhd_dbg_ring_t *)dbg_ring;

	if (ring->id != PACKET_LOG_RING_ID) {
		return BCME_UNSUPPORTED;
	}

	DHD_DBG_RING_LOCK(ring->lock, flags);

	if (ring->state != RING_ACTIVE) {
		DHD_DBG_RING_UNLOCK(ring->lock, flags);
		return BCME_OK;
	}

	/* update statistics */
	ring->stat.written_records++;
	ring->stat.written_bytes += w_len;
	DHD_DBGIF(("%s : RING%d[%s] written_records %d, written_bytes %d, read_bytes=%d,"
		" ring->threshold=%d, wp=%d, rp=%d\n", __FUNCTION__, ring->id, ring->name,
		ring->stat.written_records, ring->stat.written_bytes, ring->stat.read_bytes,
		ring->threshold, ring->wp, ring->rp));

	DHD_DBG_RING_UNLOCK(ring->lock, flags);

	return BCME_OK;
}
#endif /* DHD_PKT_LOGGING_DBGRING */

int
dhd_dbg_ring_push(dhd_dbg_ring_t *ring, dhd_dbg_ring_entry_t *hdr, void *data)
{
	unsigned long flags;
	uint32 w_len;
	uint32 avail_size;
	dhd_dbg_ring_entry_t *w_entry, *r_entry;
	int ret;

	if (!ring || !hdr || !data) {
		return BCME_BADARG;
	}

#if defined(__linux__)
	/* Prevents the case of accessing the ring buffer in the HardIRQ context.
	 * If an interrupt arise after holding ring lock, It could try the same lock.
	 * This is to use the ring lock as spin_lock_bh instead of spin_lock_irqsave.
	 */
	if (in_irq()) {
		return BCME_BUSY;
	}
#endif /* defined(__linux__) */

	DHD_DBG_RING_LOCK(ring->lock, flags);

	if (ring->state != RING_ACTIVE) {
		DHD_DBG_RING_UNLOCK(ring->lock, flags);
		return BCME_OK;
	}

	w_len = ENTRY_LENGTH(hdr);

	DHD_DBGIF(("%s: RING%d[%s] hdr->len=%u, w_len=%u, wp=%d, rp=%d, ring_start=0x%p;"
		" ring_size=%u\n",
		__FUNCTION__, ring->id, ring->name, hdr->len, w_len, ring->wp, ring->rp,
		ring->ring_buf, ring->ring_size));

	if (w_len > ring->ring_size) {
		DHD_DBG_RING_UNLOCK(ring->lock, flags);
		DHD_ERROR(("%s: RING%d[%s] w_len=%u, ring_size=%u,"
			" write size exceeds ring size !\n",
			__FUNCTION__, ring->id, ring->name, w_len, ring->ring_size));
		return BCME_ERROR;
	}
	/* Claim the space */
	do {
		avail_size = DBG_RING_CHECK_WRITE_SPACE(ring->rp, ring->wp, ring->ring_size);
		if (avail_size <= w_len) {
			/* Prepare the space */
			if (ring->rp <= ring->wp) {
				ring->tail_padded = TRUE;
				ring->rem_len = ring->ring_size - ring->wp;
				DHD_DBGIF(("%s: RING%d[%s] Insuffient tail space,"
					" rp=%d, wp=%d, rem_len=%d, ring_size=%d,"
					" avail_size=%d, w_len=%d\n", __FUNCTION__,
					ring->id, ring->name, ring->rp, ring->wp,
					ring->rem_len, ring->ring_size, avail_size,
					w_len));

				/* 0 pad insufficient tail space */
				memset((uint8 *)ring->ring_buf + ring->wp, 0, ring->rem_len);
				/* If read pointer is still at the beginning, make some room */
				if (ring->rp == 0) {
					r_entry = (dhd_dbg_ring_entry_t *)((uint8 *)ring->ring_buf +
						ring->rp);
					ring->rp += ENTRY_LENGTH(r_entry);
					ring->stat.read_bytes += ENTRY_LENGTH(r_entry);
					DHD_DBGIF(("%s: rp at 0, move by one entry length"
						" (%u bytes)\n",
						__FUNCTION__, (uint32)ENTRY_LENGTH(r_entry)));
				}
				if (ring->rp == ring->wp) {
					ring->rp = 0;
				}
				ring->wp = 0;
				DHD_DBGIF(("%s: new rp=%u, wp=%u\n",
					__FUNCTION__, ring->rp, ring->wp));
			} else {
				/* Not enough space for new entry, free some up */
				r_entry = (dhd_dbg_ring_entry_t *)((uint8 *)ring->ring_buf +
					ring->rp);
				/* check bounds before incrementing read ptr */
				if (ring->rp + ENTRY_LENGTH(r_entry) >= ring->ring_size) {
					DHD_DBGIF(("%s: RING%d[%s] rp points out of boundary,"
						"ring->wp=%u, ring->rp=%u, ring->ring_size=%d\n",
						__FUNCTION__, ring->id, ring->name, ring->wp,
						ring->rp, ring->ring_size));
					ASSERT(0);
					DHD_DBG_RING_UNLOCK(ring->lock, flags);
					return BCME_BUFTOOSHORT;
				}
				ring->rp += ENTRY_LENGTH(r_entry);
				/* skip padding if there is one */
				if (ring->tail_padded &&
					((ring->rp + ring->rem_len) == ring->ring_size)) {
					DHD_DBGIF(("%s: RING%d[%s] Found padding,"
						" avail_size=%d, w_len=%d, set rp = 0\n",
						__FUNCTION__,
						ring->id, ring->name, avail_size, w_len));
					ring->rp = 0;
					ring->tail_padded = FALSE;
					ring->rem_len = 0;
				}
				ring->stat.read_bytes += ENTRY_LENGTH(r_entry);
				DHD_DBGIF(("%s: RING%d[%s] read_bytes=%d, wp=%d, rp=%d\n",
					__FUNCTION__, ring->id, ring->name, ring->stat.read_bytes,
					ring->wp, ring->rp));
			}
		} else {
			break;
		}
	} while (TRUE);

	/* check before writing to the ring */
	if (ring->wp + w_len >= ring->ring_size) {
		DHD_DBGIF(("%s: RING%d[%s] wp pointed out of ring boundary, "
			"wp=%d, ring_size=%d, w_len=%u\n", __FUNCTION__, ring->id,
			ring->name, ring->wp, ring->ring_size, w_len));
		ASSERT(0);
		DHD_DBG_RING_UNLOCK(ring->lock, flags);
		return BCME_BUFTOOLONG;
	}

	w_entry = (dhd_dbg_ring_entry_t *)((uint8 *)ring->ring_buf + ring->wp);
	/* header */
	ret = memcpy_s(w_entry, avail_size, hdr, DBG_RING_ENTRY_SIZE);
	if (ret) {
		DHD_ERROR((" memcpy_s() error : %d, destsz: %d, n: %d\n",
			ret, avail_size, (int)DBG_RING_ENTRY_SIZE));
		return BCME_ERROR;
	}
	w_entry->len = hdr->len;
	/* payload */
	avail_size -= DBG_RING_ENTRY_SIZE;
	ret = memcpy_s((char *)w_entry + DBG_RING_ENTRY_SIZE,
		avail_size, data, w_entry->len);
	if (ret) {
		DHD_ERROR((" memcpy_s() error : %d, destsz: %d, n: %d\n",
			ret, avail_size, w_entry->len));
		return BCME_ERROR;
	}
	/* update write pointer */
	ring->wp += w_len;

	/* update statistics */
	ring->stat.written_records++;
	ring->stat.written_bytes += w_len;
	DHD_DBGIF(("%s : RING%d[%s] written_records %d, written_bytes %d, read_bytes=%d,"
		" ring->threshold=%d, wp=%d, rp=%d\n", __FUNCTION__, ring->id, ring->name,
		ring->stat.written_records, ring->stat.written_bytes, ring->stat.read_bytes,
		ring->threshold, ring->wp, ring->rp));

	DHD_DBG_RING_UNLOCK(ring->lock, flags);
	return BCME_OK;
}

/*
 * This function folds ring->lock, so callers of this function
 * should not hold ring->lock.
 */
int
dhd_dbg_ring_pull_single(dhd_dbg_ring_t *ring, void *data, uint32 buf_len, bool strip_header)
{
	dhd_dbg_ring_entry_t *r_entry = NULL;
	uint32 rlen = 0;
	char *buf = NULL;
	unsigned long flags;

	if (!ring || !data || buf_len <= 0) {
		return 0;
	}

	DHD_DBG_RING_LOCK(ring->lock, flags);
	/* pull from ring is allowed for inactive (suspended) ring
	 * in case of ecounters only, this is because, for ecounters
	 * when a trap occurs the ring is suspended and data is then
	 * pulled to dump it to a file. For other rings if ring is
	 * not in active state return without processing (as before)
	 */
	if (!ring->pull_inactive && (ring->state != RING_ACTIVE)) {
		goto exit;
	}

	if (ring->rp == ring->wp) {
		goto exit;
	}

	DHD_DBGIF(("%s: RING%d[%s] buf_len=%u, wp=%d, rp=%d, ring_start=0x%p; ring_size=%u\n",
		__FUNCTION__, ring->id, ring->name, buf_len, ring->wp, ring->rp,
		ring->ring_buf, ring->ring_size));

	r_entry = (dhd_dbg_ring_entry_t *)((uint8 *)ring->ring_buf + ring->rp);

	/* Boundary Check */
	rlen = ENTRY_LENGTH(r_entry);
	if ((ring->rp + rlen) > ring->ring_size) {
		DHD_DBGIF(("%s: entry len %d is out of boundary of ring size %d,"
			" current ring %d[%s] - rp=%d\n", __FUNCTION__, rlen,
			ring->ring_size, ring->id, ring->name, ring->rp));
		rlen = 0;
		goto exit;
	}

	if (strip_header) {
		rlen = r_entry->len;
		buf = (char *)r_entry + DBG_RING_ENTRY_SIZE;
	} else {
		rlen = ENTRY_LENGTH(r_entry);
		buf = (char *)r_entry;
	}
	if (rlen > buf_len) {
		DHD_DBGIF(("%s: buf len %d is too small for entry len %d\n",
			__FUNCTION__, buf_len, rlen));
		DHD_DBGIF(("%s: ring %d[%s] - ring size=%d, wp=%d, rp=%d\n",
			__FUNCTION__, ring->id, ring->name, ring->ring_size,
			ring->wp, ring->rp));
		/* The state of ringbuffer is different between calculating buf_len
		 * and current. ring->rp have chance to be update by pushing data
		 * to ring buffer when unlocking after calculating buf_len.
		 * But, It doesn't need to ASSERT because we only send up the
		 * entries stored so far.
		 */
		rlen = 0;
		goto exit;
	}

	memcpy(data, buf, rlen);
	/* update ring context */
	ring->rp += ENTRY_LENGTH(r_entry);
	/* don't pass wp but skip padding if there is one */
	if (ring->rp != ring->wp &&
	    ring->tail_padded && ((ring->rp + ring->rem_len) >= ring->ring_size)) {
		DHD_DBGIF(("%s: RING%d[%s] Found padding, rp=%d, wp=%d\n",
			__FUNCTION__, ring->id, ring->name, ring->rp, ring->wp));
		ring->rp = 0;
		ring->tail_padded = FALSE;
		ring->rem_len = 0;
	}
	if (ring->rp >= ring->ring_size) {
		DHD_DBGIF(("%s: RING%d[%s] rp pointed out of ring boundary,"
			" rp=%d, ring_size=%d\n", __FUNCTION__, ring->id,
			ring->name, ring->rp, ring->ring_size));
		ASSERT(0);
		rlen = 0;
		goto exit;
	}
	ring->stat.read_bytes += ENTRY_LENGTH(r_entry);
	DHD_DBGIF(("%s RING%d[%s]read_bytes %d, wp=%d, rp=%d\n", __FUNCTION__,
		ring->id, ring->name, ring->stat.read_bytes, ring->wp, ring->rp));

exit:
	DHD_DBG_RING_UNLOCK(ring->lock, flags);

	return rlen;
}

int
dhd_dbg_ring_pull(dhd_dbg_ring_t *ring, void *data, uint32 buf_len,
		bool strip_hdr, int *num_entries)
{
	int32 r_len, total_r_len = 0;
	unsigned long flags;

	if (!ring || !data)
		return 0;

	DHD_DBG_RING_LOCK(ring->lock, flags);
	if (!ring->pull_inactive && (ring->state != RING_ACTIVE)) {
		DHD_DBG_RING_UNLOCK(ring->lock, flags);
		return 0;
	}
	DHD_DBG_RING_UNLOCK(ring->lock, flags);

	while (buf_len > 0) {
		r_len = dhd_dbg_ring_pull_single(ring, data, buf_len, strip_hdr);
		if (r_len == 0)
			break;
		data = (uint8 *)data + r_len;
		buf_len -= r_len;
		total_r_len += r_len;
		(*num_entries)++;
	}

	return total_r_len;
}

int
dhd_dbg_ring_config(dhd_dbg_ring_t *ring, int log_level, uint32 threshold)
{
	unsigned long flags = 0;

	if (!ring)
		return BCME_BADADDR;

	if (ring->state == RING_STOP)
		return BCME_UNSUPPORTED;

	DHD_DBG_RING_LOCK(ring->lock, flags);

	if (log_level == 0)
		ring->state = RING_SUSPEND;
	else
		ring->state = RING_ACTIVE;

	ring->log_level = log_level;
	ring->threshold = MIN(threshold, DBGRING_FLUSH_THRESHOLD(ring));

	DHD_DBG_RING_UNLOCK(ring->lock, flags);

	return BCME_OK;
}

void
dhd_dbg_ring_start(dhd_dbg_ring_t *ring)
{
	if (!ring)
		return;

	/* Initialize the information for the ring */
	ring->state = RING_SUSPEND;
	ring->log_level = 0;
	ring->rp = ring->wp = 0;
	ring->threshold = 0;
	memset(&ring->stat, 0, sizeof(struct ring_statistics));
	memset(ring->ring_buf, 0, ring->ring_size);
}
