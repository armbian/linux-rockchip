// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2022-2024 ARM Limited. All rights reserved.
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

#include <mali_kbase.h>
#include "backend/gpu/mali_kbase_pm_internal.h"
#include "mali_kbase_csf.h"
#include "mali_kbase_csf_tiler_heap.h"
#include "mali_kbase_csf_tiler_heap_reclaim.h"

/* Tiler heap shrinker seek value, needs to be higher than jit and memory pools */
#define HEAP_SHRINKER_SEEKS (DEFAULT_SEEKS + 2)

/* Tiler heap shrinker batch value */
#define HEAP_SHRINKER_BATCH (512 / (GPU_PAGES_PER_CPU_PAGE))

/* Tiler heap reclaim scan (free) method size for limiting a scan run length */
#define HEAP_RECLAIM_SCAN_BATCH_SIZE (HEAP_SHRINKER_BATCH << 7)

/*
 * Default setting for reclaiming offslot CSG's heap.
 * Disabling reclaim with pages being 0.
 */
#define HEAP_RECLAIM_OFFSLOT_TIMEOUT_MS (30000)
#define HEAP_RECLAIM_OFFSLOT_PAGES (0)

static u8 get_kctx_highest_csg_priority(struct kbase_context *kctx)
{
	u8 prio;

	for (prio = KBASE_QUEUE_GROUP_PRIORITY_REALTIME; prio < KBASE_QUEUE_GROUP_PRIORITY_LOW;
	     prio++)
		if (!list_empty(&kctx->csf.sched.runnable_groups[prio]))
			break;

	if (prio != KBASE_QUEUE_GROUP_PRIORITY_REALTIME && kctx->csf.sched.num_idle_wait_grps) {
		struct kbase_queue_group *group;

		list_for_each_entry(group, &kctx->csf.sched.idle_wait_groups, link) {
			if (group->priority < prio)
				prio = group->priority;
		}
	}

	return prio;
}

static void detach_ctx_from_heap_reclaim_mgr(struct kbase_context *kctx)
{
	struct kbase_csf_scheduler *const scheduler = &kctx->kbdev->csf.scheduler;
	struct kbase_csf_ctx_heap_reclaim_info *info = &kctx->csf.sched.heap_info;

	lockdep_assert_held(&scheduler->lock);

	if (!list_empty(&info->mgr_link)) {
		u32 remaining = (info->nr_est_unused_pages > info->nr_freed_pages) ?
					      info->nr_est_unused_pages - info->nr_freed_pages :
					      0;

		list_del_init(&info->mgr_link);
		if (remaining)
			WARN_ON(atomic_sub_return((int)remaining,
						  &scheduler->reclaim_mgr.unused_pages) < 0);

		dev_dbg(kctx->kbdev->dev,
			"Reclaim_mgr_detach: ctx_%d_%d, est_pages=0%u, freed_pages=%u", kctx->tgid,
			kctx->id, info->nr_est_unused_pages, info->nr_freed_pages);
	}

	/* 0 indicates that the kctx may have CSG on slot */
	kctx->offslot_ts = 0;
}

static void attach_ctx_to_heap_reclaim_mgr(struct kbase_context *kctx)
{
	struct kbase_csf_ctx_heap_reclaim_info *const info = &kctx->csf.sched.heap_info;
	struct kbase_csf_scheduler *const scheduler = &kctx->kbdev->csf.scheduler;
	u8 const prio = get_kctx_highest_csg_priority(kctx);

	lockdep_assert_held(&scheduler->lock);

	if (WARN_ON(!list_empty(&info->mgr_link)))
		list_del_init(&info->mgr_link);

	/* Count the pages that could be freed */
	info->nr_est_unused_pages = kbase_csf_tiler_heap_count_kctx_unused_pages(kctx);
	/* Initialize the scan operation tracking pages */
	info->nr_freed_pages = 0;

	list_add_tail(&info->mgr_link, &scheduler->reclaim_mgr.ctx_lists[prio]);
	/* Accumulate the estimated pages to the manager total field */
	atomic_add((int)info->nr_est_unused_pages, &scheduler->reclaim_mgr.unused_pages);

	kctx->offslot_ts = ktime_get_raw_ns();
	dev_dbg(kctx->kbdev->dev, "Reclaim_mgr_attach [%llu]: ctx_%d_%d, est_count_pages=%u",
		kctx->offslot_ts, kctx->tgid, kctx->id, info->nr_est_unused_pages);
}

void kbase_csf_tiler_heap_reclaim_sched_notify_grp_active(struct kbase_queue_group *group)
{
	struct kbase_context *kctx = group->kctx;
	struct kbase_csf_ctx_heap_reclaim_info *info = &kctx->csf.sched.heap_info;

	lockdep_assert_held(&kctx->kbdev->csf.scheduler.lock);

	info->on_slot_grps++;
	/* If the kctx has an on-slot change from 0 => 1, detach it from reclaim_mgr */
	if (info->on_slot_grps == 1) {
		dev_dbg(kctx->kbdev->dev, "CSG_%d_%d_%d on-slot, remove kctx from reclaim manager",
			group->kctx->tgid, group->kctx->id, group->handle);

		detach_ctx_from_heap_reclaim_mgr(kctx);
	}
}

void kbase_csf_tiler_heap_reclaim_sched_notify_grp_evict(struct kbase_queue_group *group)
{
	struct kbase_context *kctx = group->kctx;
	struct kbase_csf_ctx_heap_reclaim_info *const info = &kctx->csf.sched.heap_info;
	struct kbase_csf_scheduler *const scheduler = &kctx->kbdev->csf.scheduler;
	const u32 num_groups = kctx->kbdev->csf.global_iface.group_num;
	u32 on_slot_grps = 0;
	u32 i;

	lockdep_assert_held(&scheduler->lock);

	/* Group eviction from the scheduler is a bit more complex, but fairly less
	 * frequent in operations. Taking the opportunity to actually count the
	 * on-slot CSGs from the given kctx, for robustness and clearer code logic.
	 */
	for_each_set_bit(i, scheduler->csg_inuse_bitmap, num_groups) {
		struct kbase_csf_csg_slot *csg_slot = &scheduler->csg_slots[i];
		struct kbase_queue_group *grp = csg_slot->resident_group;

		if (unlikely(!grp))
			continue;

		if (grp->kctx == kctx)
			on_slot_grps++;
	}

	info->on_slot_grps = on_slot_grps;

	/* If the kctx has no other CSGs on-slot, handle the heap reclaim related actions */
	if (!info->on_slot_grps) {
		if (kctx->csf.sched.num_runnable_grps || kctx->csf.sched.num_idle_wait_grps) {
			/* The kctx has other operational CSGs, attach it if not yet done */
			if (list_empty(&info->mgr_link)) {
				dev_dbg(kctx->kbdev->dev,
					"CSG_%d_%d_%d evict, add kctx to reclaim manager",
					group->kctx->tgid, group->kctx->id, group->handle);

				attach_ctx_to_heap_reclaim_mgr(kctx);
			}
		} else {
			/* The kctx is a zombie after the group eviction, drop it out */
			dev_dbg(kctx->kbdev->dev,
				"CSG_%d_%d_%d evict leading to zombie kctx, dettach from reclaim manager",
				group->kctx->tgid, group->kctx->id, group->handle);

			detach_ctx_from_heap_reclaim_mgr(kctx);
		}
	}
}

void kbase_csf_tiler_heap_reclaim_sched_notify_grp_suspend(struct kbase_queue_group *group)
{
	struct kbase_context *kctx = group->kctx;
	struct kbase_csf_ctx_heap_reclaim_info *info = &kctx->csf.sched.heap_info;

	lockdep_assert_held(&kctx->kbdev->csf.scheduler.lock);

	if (!WARN_ON(info->on_slot_grps == 0))
		info->on_slot_grps--;
	/* If the kctx has no CSGs on-slot, attach it to scheduler's reclaim manager */
	if (info->on_slot_grps == 0) {
		dev_dbg(kctx->kbdev->dev, "CSG_%d_%d_%d off-slot, add kctx to reclaim manager",
			group->kctx->tgid, group->kctx->id, group->handle);

		attach_ctx_to_heap_reclaim_mgr(kctx);
	}
}

unsigned long kbase_csf_tiler_heap_reclaim_unused_pages(struct kbase_device *kbdev,
							enum heap_reclaim_scenario scenario)
{
	struct kbase_csf_scheduler *const scheduler = &kbdev->csf.scheduler;
	struct kbase_csf_sched_heap_reclaim_mgr *const mgr = &scheduler->reclaim_mgr;
	unsigned long total_freed_pages = 0, max_pages;
	int prio, min_prio;
	u64 now = 0, offslot_ts = 0;

	lockdep_assert_held(&scheduler->lock);

	if (scenario == HEAP_RECLAIM_SCENARIO_SHRINKER) {
		/* triggered by shrinker */
		max_pages = HEAP_RECLAIM_SCAN_BATCH_SIZE;
		min_prio = KBASE_QUEUE_GROUP_PRIORITY_REALTIME;
	} else if (scenario == HEAP_RECLAIM_SCENARIO_SCHEDULER) {
		/* triggered by schedule_on_tick
		 * reclaim heap from CSGs with below conditions:
		 * 1.being offslot for a period
		 * 2.limit page numbers for each reclaim
		 * 3.skip RT kctx
		 */
		if (mgr->offslot_setting.pages == 0) {
			dev_dbg(kbdev->dev, "HEAP_RECLAIM_SCENARIO_SCHEDULER is disabled");
			return 0;
		}
		max_pages = mgr->offslot_setting.pages;
		min_prio = KBASE_QUEUE_GROUP_PRIORITY_HIGH;
		now = ktime_get_raw_ns();
		offslot_ts = now > mgr->offslot_setting.timeout_ms * 1000000ULL ?
					   now - mgr->offslot_setting.timeout_ms * 1000000ULL :
					   0;
	} else {
		dev_err(kbdev->dev, "Unknown heap reclaim scenario %d\n", scenario);
		return 0;
	}

	if (scheduler->state != SCHED_SUSPENDED) {
		/* Clean and invalidate the L2 cache before reading from the heap contexts,
		 * headers of the individual chunks and buffer descriptors.
		 */
		kbase_gpu_start_cache_clean(kbdev, GPU_COMMAND_CACHE_CLN_INV_L2);
		if (kbase_gpu_wait_cache_clean_timeout(
			    kbdev, kbase_get_timeout_ms(kbdev, MMU_AS_INACTIVE_WAIT_TIMEOUT)))
			dev_warn(
				kbdev->dev,
				"[%llu] Timeout waiting for CACHE_CLN_INV_L2 to complete before Tiler heap reclaim",
				kbase_backend_get_cycle_cnt(kbdev));

	} else {
		/* Make sure power down transitions have completed, i.e. L2 has been
		 * powered off as that would ensure its contents are flushed to memory.
		 * This is needed as Scheduler doesn't wait for the power down to finish.
		 */
		if (kbase_pm_wait_for_desired_state(kbdev))
			dev_warn(kbdev->dev,
				 "Wait for power down transition failed before Tiler heap reclaim");
	}

	for (prio = KBASE_QUEUE_GROUP_PRIORITY_LOW;
	     total_freed_pages < max_pages && prio >= min_prio; prio--) {
		struct kbase_csf_ctx_heap_reclaim_info *info, *tmp;
		u32 cnt_ctxs = 0;

		list_for_each_entry_safe(info, tmp, &scheduler->reclaim_mgr.ctx_lists[prio],
					 mgr_link) {
			struct kbase_context *kctx =
				container_of(info, struct kbase_context, csf.sched.heap_info);
			u32 freed_pages;

			if (scenario == HEAP_RECLAIM_SCENARIO_SCHEDULER) {
				WARN_ON(kctx->offslot_ts == 0);
				if (kctx->offslot_ts > offslot_ts) {
					dev_dbg(kbdev->dev,
						"Reclaim aborts from ctx %d_%d, prio %d, current time %llu - offslot time %llu = %llu",
						kctx->tgid, kctx->id, prio, now, kctx->offslot_ts,
						now - kctx->offslot_ts);
					/* Skip following ctx as they are attached later */
					break;
				}
			}

			freed_pages = kbase_csf_tiler_heap_scan_kctx_unused_pages(
				kctx, info->nr_est_unused_pages);

			dev_dbg(kbdev->dev, "Reclaim free heap pages for ctx %d_%d freed pages %u",
				kctx->tgid, kctx->id, freed_pages);

			if (freed_pages) {
				/* Remove the freed pages from the manager retained estimate. The
				 * accumulated removals from the kctx should not exceed the kctx
				 * initially notified contribution amount:
				 *   info->nr_est_unused_pages.
				 */
				u32 rm_cnt = MIN(info->nr_est_unused_pages - info->nr_freed_pages,
						 freed_pages);

				WARN_ON(atomic_sub_return((int)rm_cnt, &mgr->unused_pages) < 0);

				/* tracking the freed pages, before a potential detach call */
				info->nr_freed_pages += freed_pages;
				total_freed_pages += freed_pages;

				schedule_work(&kctx->jit_work);
			}

			/* If the kctx can't offer anymore, drop it from the reclaim manger,
			 * otherwise leave it remaining in. If the kctx changes its state (i.e.
			 * some CSGs becoming on-slot), the scheduler will pull it out.
			 */
			if (info->nr_freed_pages >= info->nr_est_unused_pages || freed_pages == 0)
				detach_ctx_from_heap_reclaim_mgr(kctx);

			cnt_ctxs++;

			/* Enough has been freed, break to avoid holding the lock too long */
			if (total_freed_pages >= max_pages)
				break;
		}

		dev_dbg(kbdev->dev, "Reclaim free heap pages: %lu (cnt_ctxs: %u, prio: %d)",
			total_freed_pages, cnt_ctxs, prio);
	}

	dev_dbg(kbdev->dev, "Reclaim free total heap pages: %lu (across all CSG priority)",
		total_freed_pages);

	return total_freed_pages;
}

static unsigned long kbase_csf_tiler_heap_reclaim_count_free_pages(struct kbase_device *kbdev,
								   struct shrink_control *sc)
{
	struct kbase_csf_sched_heap_reclaim_mgr *mgr = &kbdev->csf.scheduler.reclaim_mgr;
	unsigned long page_cnt = (unsigned long)atomic_read(&mgr->unused_pages);

	CSTD_UNUSED(sc);

	dev_dbg(kbdev->dev, "Reclaim count unused pages (estimate): %lu", page_cnt);

	return page_cnt;
}

static unsigned long kbase_csf_tiler_heap_reclaim_scan_free_pages(struct kbase_device *kbdev,
								  struct shrink_control *sc)
{
	struct kbase_csf_sched_heap_reclaim_mgr *mgr = &kbdev->csf.scheduler.reclaim_mgr;
	unsigned long freed = 0;
	unsigned long avail = 0;

	/* If Scheduler is busy in action, return 0 */
	if (!mutex_trylock(&kbdev->csf.scheduler.lock)) {
		struct kbase_csf_scheduler *const scheduler = &kbdev->csf.scheduler;
		long remaining = (long)msecs_to_jiffies(2);

		/* Wait for roughly 2-ms */
		remaining = kbase_csf_fw_io_wait_event_timeout(&kbdev->csf.fw_io,
							       kbdev->csf.event_wait,
							       (scheduler->state != SCHED_BUSY),
							       remaining);

		if (!mutex_trylock(&kbdev->csf.scheduler.lock)) {
			dev_dbg(kbdev->dev, "Tiler heap reclaim scan see device busy (freed: 0)");
			return 0;
		}
	}

	avail = (unsigned long)atomic_read(&mgr->unused_pages);
	if (avail)
		freed = kbase_csf_tiler_heap_reclaim_unused_pages(kbdev,
								  HEAP_RECLAIM_SCENARIO_SHRINKER);

	mutex_unlock(&kbdev->csf.scheduler.lock);

#if (KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE)
	if (freed > sc->nr_to_scan)
		sc->nr_scanned = freed;
#endif /* (KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE) */

	dev_info(kbdev->dev, "Tiler heap reclaim scan freed pages: %lu (unused: %lu)", freed,
		 avail);

	/* On estimate suggesting available, yet actual free failed, return STOP */
	if (avail && !freed)
		return SHRINK_STOP;
	else
		return freed;
}

static unsigned long kbase_csf_tiler_heap_reclaim_count_objects(struct shrinker *s,
								struct shrink_control *sc)
{
	struct kbase_device *kbdev = KBASE_GET_KBASE_DATA_FROM_SHRINKER(
		s, struct kbase_device, csf.scheduler.reclaim_mgr.heap_reclaim);

	return kbase_csf_tiler_heap_reclaim_count_free_pages(kbdev, sc);
}

static unsigned long kbase_csf_tiler_heap_reclaim_scan_objects(struct shrinker *s,
							       struct shrink_control *sc)
{
	struct kbase_device *kbdev = KBASE_GET_KBASE_DATA_FROM_SHRINKER(
		s, struct kbase_device, csf.scheduler.reclaim_mgr.heap_reclaim);

	return kbase_csf_tiler_heap_reclaim_scan_free_pages(kbdev, sc);
}

void kbase_csf_tiler_heap_reclaim_ctx_init(struct kbase_context *kctx)
{
	/* Per-kctx heap_info object initialization */
	INIT_LIST_HEAD(&kctx->csf.sched.heap_info.mgr_link);
}

int kbase_csf_tiler_heap_reclaim_mgr_init(struct kbase_device *kbdev)
{
	struct kbase_csf_scheduler *scheduler = &kbdev->csf.scheduler;
	u8 prio;
	struct shrinker *reclaim;

	reclaim =
		KBASE_INIT_RECLAIM(&(scheduler->reclaim_mgr), heap_reclaim, "mali-csf-tiler-heap");
	if (!reclaim)
		return -ENOMEM;
	KBASE_SET_RECLAIM(&(scheduler->reclaim_mgr), heap_reclaim, reclaim);

	for (prio = KBASE_QUEUE_GROUP_PRIORITY_REALTIME; prio < KBASE_QUEUE_GROUP_PRIORITY_COUNT;
	     prio++)
		INIT_LIST_HEAD(&scheduler->reclaim_mgr.ctx_lists[prio]);

	scheduler->reclaim_mgr.offslot_setting.timeout_ms = HEAP_RECLAIM_OFFSLOT_TIMEOUT_MS;
	scheduler->reclaim_mgr.offslot_setting.pages = HEAP_RECLAIM_OFFSLOT_PAGES;

	reclaim->count_objects = kbase_csf_tiler_heap_reclaim_count_objects;
	reclaim->scan_objects = kbase_csf_tiler_heap_reclaim_scan_objects;
	reclaim->seeks = HEAP_SHRINKER_SEEKS;
	reclaim->batch = HEAP_SHRINKER_BATCH;

	if (!IS_ENABLED(CONFIG_MALI_VECTOR_DUMP))
		KBASE_REGISTER_SHRINKER(reclaim, "mali-csf-tiler-heap", kbdev);

	return 0;
}

void kbase_csf_tiler_heap_reclaim_mgr_term(struct kbase_device *kbdev)
{
	struct kbase_csf_scheduler *scheduler = &kbdev->csf.scheduler;
	u8 prio;

	if (!IS_ENABLED(CONFIG_MALI_VECTOR_DUMP))
		KBASE_UNREGISTER_SHRINKER(scheduler->reclaim_mgr.heap_reclaim);

	for (prio = KBASE_QUEUE_GROUP_PRIORITY_REALTIME; prio < KBASE_QUEUE_GROUP_PRIORITY_COUNT;
	     prio++)
		WARN_ON(!list_empty(&scheduler->reclaim_mgr.ctx_lists[prio]));

	WARN_ON(atomic_read(&scheduler->reclaim_mgr.unused_pages));
}
