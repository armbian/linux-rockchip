// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * Author: Huang Lee <Putin.li@rock-chips.com>
 */

#include "rga_job.h"
#include "rga_fence.h"
#include "rga_dma_buf.h"
#include "rga_mm.h"
#include "rga_iommu.h"
#include "rga_debugger.h"
#include "rga_common.h"

static void rga_job_free(struct rga_job *job)
{
	if (job->cmd_buf)
		rga_dma_free(job->cmd_buf);

	kfree(job);
}

static void rga_job_kref_release(struct kref *ref)
{
	struct rga_job *job;

	job = container_of(ref, struct rga_job, refcount);

	rga_job_free(job);
}

static int rga_job_put(struct rga_job *job)
{
	return kref_put(&job->refcount, rga_job_kref_release);
}

static void rga_job_get(struct rga_job *job)
{
	kref_get(&job->refcount);
}

static int rga_job_cleanup(struct rga_job *job)
{
	rga_job_put(job);

	return 0;
}

static int rga_job_judgment_support_core(struct rga_job *job)
{
	int ret = 0;
	uint32_t mm_flag;
	struct rga_req *req;
	struct rga_mm *mm;

	req = &job->rga_command_base;
	mm = rga_drvdata->mm;
	if (mm == NULL) {
		rga_job_err(job, "rga mm is null!\n");
		return -EFAULT;
	}

	mutex_lock(&mm->lock);

	if (likely(req->src.yrgb_addr > 0)) {
		ret = rga_mm_lookup_flag(mm, req->src.yrgb_addr);
		if (ret < 0)
			goto out_finish;
		else
			mm_flag = (uint32_t)ret;

		if (~mm_flag & RGA_MEM_UNDER_4G) {
			job->flags |= RGA_JOB_UNSUPPORT_RGA_MMU;
			goto out_finish;
		}
	}

	if (likely(req->dst.yrgb_addr > 0)) {
		ret = rga_mm_lookup_flag(mm, req->dst.yrgb_addr);
		if (ret < 0)
			goto out_finish;
		else
			mm_flag = (uint32_t)ret;

		if (~mm_flag & RGA_MEM_UNDER_4G) {
			job->flags |= RGA_JOB_UNSUPPORT_RGA_MMU;
			goto out_finish;
		}
	}

	if (req->pat.yrgb_addr > 0) {
		ret = rga_mm_lookup_flag(mm, req->pat.yrgb_addr);
		if (ret < 0)
			goto out_finish;
		else
			mm_flag = (uint32_t)ret;

		if (~mm_flag & RGA_MEM_UNDER_4G) {
			job->flags |= RGA_JOB_UNSUPPORT_RGA_MMU;
			goto out_finish;
		}
	}

out_finish:
	mutex_unlock(&mm->lock);

	return ret;
}

static struct rga_job *rga_job_alloc(struct rga_req *rga_command_base)
{
	struct rga_job *job = NULL;

	job = kzalloc(sizeof(*job), GFP_KERNEL);
	if (!job)
		return NULL;

	INIT_LIST_HEAD(&job->head);
	kref_init(&job->refcount);

	job->timestamp.init = ktime_get();
	job->pid = current->pid;

	job->rga_command_base = *rga_command_base;

	if (rga_command_base->priority > 0) {
		if (rga_command_base->priority > RGA_SCHED_PRIORITY_MAX)
			job->priority = RGA_SCHED_PRIORITY_MAX;
		else
			job->priority = rga_command_base->priority;
	}

	if (DEBUGGER_EN(INTERNAL_MODE)) {
		job->flags |= RGA_JOB_DEBUG_FAKE_BUFFER;

		/* skip subsequent flag judgments. */
		return job;
	}

	if (job->rga_command_base.handle_flag & 1) {
		job->flags |= RGA_JOB_USE_HANDLE;

		rga_job_judgment_support_core(job);
	}

	return job;
}

static int rga_job_run(struct rga_job *job, struct rga_scheduler_t *scheduler)
{
	int ret = 0;

	/* enable power */
	ret = rga_power_enable(scheduler);
	if (ret < 0) {
		rga_job_err(job, "power enable failed");
		return ret;
	}

	ret = scheduler->ops->set_reg(job, scheduler);
	if (ret < 0) {
		rga_job_err(job, "set reg failed");
		rga_power_disable(scheduler);
		return ret;
	}

	set_bit(RGA_JOB_STATE_RUNNING, &job->state);

	return ret;
}

void rga_job_next(struct rga_scheduler_t *scheduler)
{
	int ret;
	struct rga_job *job = NULL;
	unsigned long flags;

next_job:
	spin_lock_irqsave(&scheduler->irq_lock, flags);

	if (scheduler->running_job ||
		list_empty(&scheduler->todo_list)) {
		spin_unlock_irqrestore(&scheduler->irq_lock, flags);
		return;
	}

	job = list_first_entry(&scheduler->todo_list, struct rga_job, head);

	list_del_init(&job->head);

	scheduler->job_count--;

	scheduler->running_job = job;
	set_bit(RGA_JOB_STATE_PREPARE, &job->state);
	rga_job_get(job);

	spin_unlock_irqrestore(&scheduler->irq_lock, flags);

	ret = rga_job_run(job, scheduler);
	/* If some error before hw run */
	if (ret < 0) {
		rga_job_err(job, "some error on rga_job_run before hw start, %s(%d)\n",
			__func__, __LINE__);

		spin_lock_irqsave(&scheduler->irq_lock, flags);
		scheduler->running_job = NULL;
		spin_unlock_irqrestore(&scheduler->irq_lock, flags);

		job->ret = ret;
		rga_request_release_signal(scheduler, job);

		rga_job_put(job);

		goto next_job;
	}

	rga_job_put(job);
}

struct rga_job *rga_job_done(struct rga_scheduler_t *scheduler)
{
	struct rga_job *job;
	unsigned long flags;

	spin_lock_irqsave(&scheduler->irq_lock, flags);

	job = scheduler->running_job;
	if (job == NULL) {
		rga_err("%s(%#x) running job has been cleanup.\n",
			rga_get_core_name(scheduler->core), scheduler->core);

		spin_unlock_irqrestore(&scheduler->irq_lock, flags);
		return NULL;
	}

	if (!test_bit(RGA_JOB_STATE_FINISH, &job->state) &&
	    !test_bit(RGA_JOB_STATE_INTR_ERR, &job->state)) {
		rga_err("%s(%#x) running job has not yet been completed.",
			rga_get_core_name(scheduler->core), scheduler->core);

		spin_unlock_irqrestore(&scheduler->irq_lock, flags);
		return NULL;
	}

	scheduler->running_job = NULL;

	scheduler->timer.busy_time +=
		ktime_us_delta(job->timestamp.hw_done, job->timestamp.hw_recode);
	job->session->last_active = job->timestamp.hw_done;
	set_bit(RGA_JOB_STATE_DONE, &job->state);

	spin_unlock_irqrestore(&scheduler->irq_lock, flags);

	if (scheduler->ops->read_back_reg)
		scheduler->ops->read_back_reg(job, scheduler);

	if (DEBUGGER_EN(DUMP_IMAGE))
		rga_dump_job_image(job);

	if (DEBUGGER_EN(TIME))
		rga_job_log(job, "hardware[%s] cost time %lld us, work cycle %d\n",
			rga_get_core_name(scheduler->core),
			ktime_us_delta(job->timestamp.hw_done, job->timestamp.hw_execute),
			job->work_cycle);

	rga_mm_unmap_job_info(job);

	return job;
}

static int rga_job_timeout_query_state(struct rga_job *job, int orig_ret)
{
	int ret = orig_ret;
	struct rga_scheduler_t *scheduler = job->scheduler;

	if (test_bit(RGA_JOB_STATE_DONE, &job->state) &&
	    test_bit(RGA_JOB_STATE_FINISH, &job->state)) {
		return orig_ret;
	} else if (!test_bit(RGA_JOB_STATE_DONE, &job->state) &&
		   test_bit(RGA_JOB_STATE_FINISH, &job->state)) {
		rga_job_err(job, "job hardware has finished, but the software has timeout!\n");

		ret = -EBUSY;
	} else if (!test_bit(RGA_JOB_STATE_DONE, &job->state) &&
		   !test_bit(RGA_JOB_STATE_FINISH, &job->state)) {
		rga_job_err(job, "job hardware has timeout.\n");

		if (scheduler->ops->read_status)
			scheduler->ops->read_status(job, scheduler);

		ret = -EBUSY;
	}

	rga_job_err(job, "timeout core[%d]: INTR[0x%x], HW_STATUS[0x%x], CMD_STATUS[0x%x], WORK_CYCLE[0x%x(%d)]\n",
		    scheduler->core,
		    job->intr_status, job->hw_status, job->cmd_status,
		    job->work_cycle, job->work_cycle);

	return ret;
}

static void rga_job_scheduler_timeout_clean(struct rga_scheduler_t *scheduler)
{
	unsigned long flags;
	struct rga_job *job = NULL;

	spin_lock_irqsave(&scheduler->irq_lock, flags);

	if (scheduler->running_job == NULL || scheduler->running_job->timestamp.hw_execute == 0) {
		spin_unlock_irqrestore(&scheduler->irq_lock, flags);
		return;
	}

	job = scheduler->running_job;
	if (ktime_ms_delta(ktime_get(), job->timestamp.hw_execute) >= RGA_JOB_TIMEOUT_DELAY) {
		job->ret = rga_job_timeout_query_state(job, job->ret);

		scheduler->running_job = NULL;
		scheduler->status = RGA_SCHEDULER_ABORT;
		scheduler->ops->soft_reset(scheduler);

		spin_unlock_irqrestore(&scheduler->irq_lock, flags);

		rga_mm_unmap_job_info(job);
		rga_request_release_signal(scheduler, job);

		rga_power_disable(scheduler);
	} else {
		spin_unlock_irqrestore(&scheduler->irq_lock, flags);
	}
}

static void rga_job_insert_todo_list(struct rga_job *job)
{
	bool first_match = 0;
	unsigned long flags;
	struct rga_job *job_pos;
	struct rga_scheduler_t *scheduler = job->scheduler;

	spin_lock_irqsave(&scheduler->irq_lock, flags);

	/* priority policy set by userspace */
	if (list_empty(&scheduler->todo_list)
		|| (job->priority == RGA_SCHED_PRIORITY_DEFAULT)) {
		list_add_tail(&job->head, &scheduler->todo_list);
	} else {
		list_for_each_entry(job_pos, &scheduler->todo_list, head) {
			if (job->priority > job_pos->priority &&
					(!first_match)) {
				list_add(&job->head, &job_pos->head);
				first_match = true;
			}

			/*
			 * Increase the priority of subsequent tasks
			 * after inserting into the list
			 */
			if (first_match)
				job_pos->priority++;
		}

		if (!first_match)
			list_add_tail(&job->head, &scheduler->todo_list);
	}

	job->timestamp.insert = ktime_get();
	scheduler->job_count++;
	set_bit(RGA_JOB_STATE_PENDING, &job->state);

	spin_unlock_irqrestore(&scheduler->irq_lock, flags);
}

static struct rga_scheduler_t *rga_job_schedule(struct rga_job *job)
{
	int i;
	struct rga_scheduler_t *scheduler = NULL;

	for (i = 0; i < rga_drvdata->num_of_scheduler; i++) {
		scheduler = rga_drvdata->scheduler[i];
		rga_job_scheduler_timeout_clean(scheduler);
	}

	if (rga_drvdata->num_of_scheduler > 1) {
		job->core = rga_job_assign(job);
		if (job->core <= 0) {
			rga_job_err(job, "job assign failed");
			job->ret = -EINVAL;
			return NULL;
		}
	} else {
		job->core = rga_drvdata->scheduler[0]->core;
		job->scheduler = rga_drvdata->scheduler[0];
	}

	scheduler = job->scheduler;
	if (scheduler == NULL) {
		rga_job_err(job, "failed to get scheduler, %s(%d)\n", __func__, __LINE__);
		job->ret = -EFAULT;
		return NULL;
	}

	return scheduler;
}

int rga_job_commit(struct rga_req *rga_command_base, struct rga_request *request)
{
	int ret;
	struct rga_job *job = NULL;
	struct rga_scheduler_t *scheduler = NULL;

	job = rga_job_alloc(rga_command_base);
	if (!job) {
		rga_err("failed to alloc rga job!\n");
		return -ENOMEM;
	}

	job->use_batch_mode = request->use_batch_mode;
	job->request_id = request->id;
	job->session = request->session;
	job->mm = request->current_mm;

	scheduler = rga_job_schedule(job);
	if (scheduler == NULL) {
		goto err_free_job;
	}

	job->cmd_buf = rga_dma_alloc_coherent(scheduler, RGA_CMD_REG_SIZE);
	if (job->cmd_buf == NULL) {
		rga_job_err(job, "failed to alloc command buffer.\n");
		goto err_free_job;
	}

	/* Memory mapping needs to keep pd enabled. */
	if (rga_power_enable(scheduler) < 0) {
		rga_job_err(job, "power enable failed");
		job->ret = -EFAULT;
		goto err_free_cmd_buf;
	}

	ret = rga_mm_map_job_info(job);
	if (ret < 0) {
		rga_job_err(job, "%s: failed to map job info\n", __func__);
		job->ret = ret;
		goto err_power_disable;
	}

	ret = scheduler->ops->init_reg(job);
	if (ret < 0) {
		rga_job_err(job, "%s: init reg failed", __func__);
		job->ret = ret;
		goto err_unmap_job_info;
	}

	rga_job_insert_todo_list(job);

	rga_job_next(scheduler);

	rga_power_disable(scheduler);

	return 0;

err_unmap_job_info:
	rga_mm_unmap_job_info(job);

err_power_disable:
	rga_power_disable(scheduler);

err_free_cmd_buf:
	rga_dma_free(job->cmd_buf);
	job->cmd_buf = NULL;

err_free_job:
	ret = job->ret;
	rga_job_free(job);

	return ret;
}

static bool rga_is_need_current_mm(struct rga_req *req)
{
	int mmu_flag;
	struct rga_img_info_t *src0 = NULL;
	struct rga_img_info_t *src1 = NULL;
	struct rga_img_info_t *dst = NULL;
	struct rga_img_info_t *els = NULL;

	src0 = &req->src;
	dst = &req->dst;
	if (req->render_mode != UPDATE_PALETTE_TABLE_MODE)
		src1 = &req->pat;
	else
		els = &req->pat;

	if (likely(src0 != NULL)) {
		mmu_flag = ((req->mmu_info.mmu_flag >> 8) & 1);
		if (mmu_flag && src0->uv_addr)
			return true;
	}

	if (likely(dst != NULL)) {
		mmu_flag = ((req->mmu_info.mmu_flag >> 10) & 1);
		if (mmu_flag && dst->uv_addr)
			return true;
	}

	if (src1 != NULL) {
		mmu_flag = ((req->mmu_info.mmu_flag >> 9) & 1);
		if (mmu_flag && src1->uv_addr)
			return true;
	}

	if (els != NULL) {
		mmu_flag = ((req->mmu_info.mmu_flag >> 11) & 1);
		if (mmu_flag && els->uv_addr)
			return true;
	}

	return false;
}

static struct mm_struct *rga_request_get_current_mm(struct rga_request *request)
{
	int i;

	for (i = 0; i < request->task_count; i++) {
		if (rga_is_need_current_mm(&(request->task_list[i]))) {
			mmgrab(current->mm);
			mmget(current->mm);

			return current->mm;
		}
	}

	return NULL;
}

static void rga_request_put_current_mm(struct mm_struct *mm)
{
	if (mm == NULL)
		return;

	mmput(mm);
	mmdrop(mm);
}

static int rga_request_add_acquire_fence_callback(int acquire_fence_fd,
						  struct rga_request *request,
						  dma_fence_func_t cb_func)
{
	int ret;
	struct dma_fence *acquire_fence = NULL;
	struct rga_pending_request_manager *request_manager = rga_drvdata->pend_request_manager;

	if (DEBUGGER_EN(MSG))
		rga_req_log(request, "acquire_fence_fd = %d", acquire_fence_fd);

	acquire_fence = rga_get_dma_fence_from_fd(acquire_fence_fd);
	if (IS_ERR_OR_NULL(acquire_fence)) {
		rga_req_err(request, "%s: failed to get acquire dma_fence from[%d]\n",
		       __func__, acquire_fence_fd);
		return -EINVAL;
	}

	if (!request->feature.user_close_fence) {
		/* close acquire fence fd */
#ifdef CONFIG_NO_GKI
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		close_fd(acquire_fence_fd);
#else
		ksys_close(acquire_fence_fd);
#endif
#else
		rga_req_err(request, "Please update the driver to v1.2.28 to prevent acquire_fence_fd leaks.");
		return -EFAULT;
#endif
	}


	ret = rga_dma_fence_get_status(acquire_fence);
	if (ret < 0) {
		rga_req_err(request, "%s: Current acquire fence unexpectedly has error status before signal\n",
		       __func__);
		return ret;
	} else if (ret > 0) {
		/* has been signaled */
		return ret;
	}

	/*
	 * Ensure that the request will not be free early when
	 * the callback is called.
	 */
	mutex_lock(&request_manager->lock);
	rga_request_get(request);
	mutex_unlock(&request_manager->lock);

	ret = rga_dma_fence_add_callback(acquire_fence, cb_func, (void *)request);
	if (ret < 0) {
		if (ret != -ENOENT)
			rga_req_err(request, "%s: failed to add fence callback\n", __func__);

		mutex_lock(&request_manager->lock);
		rga_request_put(request);
		mutex_unlock(&request_manager->lock);
		return ret;
	}

	return 0;
}

int rga_request_check(struct rga_user_request *req)
{
	if (req->id <= 0) {
		rga_err("ID[%d]: request_id is invalid", req->id);
		return -EINVAL;
	}

	if (req->task_num <= 0) {
		rga_err("ID[%d]: invalid user request!\n", req->id);
		return -EINVAL;
	}

	if (req->task_ptr == 0) {
		rga_err("ID[%d]: task_ptr is NULL!\n", req->id);
		return -EINVAL;
	}

	if (req->task_num > RGA_TASK_NUM_MAX) {
		rga_err("ID[%d]: Only supports running %d tasks, now %d\n",
			req->id, RGA_TASK_NUM_MAX, req->task_num);
		return -EFBIG;
	}

	return 0;
}

struct rga_request *rga_request_lookup(struct rga_pending_request_manager *manager, uint32_t id)
{
	struct rga_request *request = NULL;

	WARN_ON(!mutex_is_locked(&manager->lock));

	request = idr_find(&manager->request_idr, id);

	return request;
}

void rga_request_scheduler_shutdown(struct rga_scheduler_t *scheduler)
{
	struct rga_job *job, *job_q;
	unsigned long flags;

	rga_power_enable(scheduler);

	spin_lock_irqsave(&scheduler->irq_lock, flags);

	job = scheduler->running_job;
	if (job) {
		if (test_bit(RGA_JOB_STATE_INTR_ERR, &job->state) ||
		    test_bit(RGA_JOB_STATE_FINISH, &job->state)) {
			spin_unlock_irqrestore(&scheduler->irq_lock, flags);
			goto finish;
		}

		scheduler->running_job = NULL;
		scheduler->status = RGA_SCHEDULER_ABORT;
		scheduler->ops->soft_reset(scheduler);

		spin_unlock_irqrestore(&scheduler->irq_lock, flags);

		rga_mm_unmap_job_info(job);

		job->ret = -EBUSY;
		rga_request_release_signal(scheduler, job);

		/*
		 *  Since the running job was abort, turn off the power here that
		 * should have been turned off after job done (corresponds to
		 * power_enable in rga_job_run()).
		 */
		rga_power_disable(scheduler);
	} else {
		/* Clean up the jobs in the todo list that need to be free. */
		list_for_each_entry_safe(job, job_q, &scheduler->todo_list, head) {
			rga_mm_unmap_job_info(job);

			job->ret = -EBUSY;
			rga_request_release_signal(scheduler, job);
		}

		spin_unlock_irqrestore(&scheduler->irq_lock, flags);
	}

finish:
	rga_power_disable(scheduler);
}

void rga_request_scheduler_abort(struct rga_scheduler_t *scheduler)
{
	struct rga_job *job;
	unsigned long flags;

	rga_power_enable(scheduler);

	spin_lock_irqsave(&scheduler->irq_lock, flags);

	job = scheduler->running_job;
	if (job) {
		scheduler->running_job = NULL;
		scheduler->status = RGA_SCHEDULER_ABORT;
		scheduler->ops->soft_reset(scheduler);

		spin_unlock_irqrestore(&scheduler->irq_lock, flags);

		rga_mm_unmap_job_info(job);

		job->ret = -EBUSY;
		rga_request_release_signal(scheduler, job);

		rga_job_next(scheduler);

		/*
		 *  Since the running job was abort, turn off the power here that
		 * should have been turned off after job done (corresponds to
		 * power_enable in rga_job_run()).
		 */
		rga_power_disable(scheduler);
	} else {
		scheduler->status = RGA_SCHEDULER_ABORT;
		scheduler->ops->soft_reset(scheduler);

		spin_unlock_irqrestore(&scheduler->irq_lock, flags);
	}

	rga_power_disable(scheduler);
}

static int rga_request_scheduler_job_abort(struct rga_request *request)
{
	int i;
	unsigned long flags;
	enum rga_scheduler_status scheduler_status;
	int running_abort_count = 0, todo_abort_count = 0, all_task_count = 0;
	struct rga_scheduler_t *scheduler = NULL;
	struct rga_job *job, *job_q;
	LIST_HEAD(list_to_free);

	for (i = 0; i < rga_drvdata->num_of_scheduler; i++) {
		scheduler = rga_drvdata->scheduler[i];
		spin_lock_irqsave(&scheduler->irq_lock, flags);

		list_for_each_entry_safe(job, job_q, &scheduler->todo_list, head) {
			if (request->id == job->request_id) {
				list_move(&job->head, &list_to_free);
				scheduler->job_count--;

				todo_abort_count++;
			}
		}

		job = NULL;
		if (scheduler->running_job) {
			if (request->id == scheduler->running_job->request_id) {
				job = scheduler->running_job;
				scheduler_status = scheduler->status;
				scheduler->running_job = NULL;
				scheduler->status = RGA_SCHEDULER_ABORT;
				list_add_tail(&job->head, &list_to_free);

				if (job->timestamp.hw_execute != 0) {
					scheduler->timer.busy_time +=
						ktime_us_delta(ktime_get(),
							       job->timestamp.hw_recode);
					scheduler->ops->soft_reset(scheduler);
				}
				job->session->last_active = ktime_get();

				rga_req_err(request, "reset core[%d] by request abort",
					scheduler->core);
				running_abort_count++;
			}
		}

		spin_unlock_irqrestore(&scheduler->irq_lock, flags);

		if (job && scheduler_status == RGA_SCHEDULER_WORKING)
			rga_power_disable(scheduler);
	}

	/* Clean up the jobs in the todo list that need to be free. */
	list_for_each_entry_safe(job, job_q, &list_to_free, head) {
		rga_mm_unmap_job_info(job);

		job->ret = -EBUSY;
		rga_job_cleanup(job);
	}

	all_task_count = request->finished_task_count + request->failed_task_count +
			 running_abort_count + todo_abort_count;

	/* This means it has been cleaned up. */
	if (running_abort_count + todo_abort_count == 0 &&
	    all_task_count == request->task_count)
		return 1;

	rga_err("request[%d] abort! finished %d failed %d running_abort %d todo_abort %d\n",
		request->id, request->finished_task_count, request->failed_task_count,
		running_abort_count, todo_abort_count);

	return 0;
}

static void rga_request_release_abort(struct rga_request *request, int err_code)
{
	unsigned long flags;
	struct mm_struct *current_mm;
	struct rga_pending_request_manager *request_manager = rga_drvdata->pend_request_manager;

	if (rga_request_scheduler_job_abort(request) > 0)
		return;

	spin_lock_irqsave(&request->lock, flags);

	if (request->is_done) {
		spin_unlock_irqrestore(&request->lock, flags);
		return;
	}

	request->is_running = false;
	request->is_done = false;
	current_mm = request->current_mm;
	request->current_mm = NULL;

	spin_unlock_irqrestore(&request->lock, flags);

	rga_request_put_current_mm(current_mm);

	rga_dma_fence_signal(request->release_fence, err_code);

	mutex_lock(&request_manager->lock);
	/* current submit request put */
	rga_request_put(request);
	mutex_unlock(&request_manager->lock);
}

void rga_request_session_destroy_abort(struct rga_session *session)
{
	int request_id;
	struct rga_request *request;
	struct rga_pending_request_manager *request_manager;

	request_manager = rga_drvdata->pend_request_manager;
	if (request_manager == NULL) {
		rga_err("rga_pending_request_manager is null!\n");
		return;
	}

	mutex_lock(&request_manager->lock);

	idr_for_each_entry(&request_manager->request_idr, request, request_id) {
		if (session == request->session) {
			rga_req_err(request, "destroy when the user exits, current refcount = %d\n",
				kref_read(&request->refcount));
			rga_request_put(request);
		}
	}

	mutex_unlock(&request_manager->lock);
}

static int rga_request_timeout_query_state(struct rga_request *request)
{
	int i;
	unsigned long flags;
	struct rga_scheduler_t *scheduler = NULL;
	struct rga_job *job = NULL;

	for (i = 0; i < rga_drvdata->num_of_scheduler; i++) {
		scheduler = rga_drvdata->scheduler[i];

		spin_lock_irqsave(&scheduler->irq_lock, flags);

		if (scheduler->running_job) {
			job = scheduler->running_job;

			if (request->id == job->request_id) {
				request->ret = rga_job_timeout_query_state(job, request->ret);

				spin_unlock_irqrestore(&scheduler->irq_lock, flags);
				break;
			}
		}

		spin_unlock_irqrestore(&scheduler->irq_lock, flags);
	}

	return request->ret;
}

static int rga_request_wait(struct rga_request *request)
{
	int left_time;
	int ret;

	left_time = wait_event_timeout(request->finished_wq, request->is_done,
				       RGA_JOB_TIMEOUT_DELAY * request->task_count);

	switch (left_time) {
	case 0:
		ret = rga_request_timeout_query_state(request);
		break;
	case -ERESTARTSYS:
		ret = -ERESTARTSYS;
		break;
	default:
		ret = request->ret;
		break;
	}

	return ret;
}

int rga_request_commit(struct rga_request *request)
{
	int ret;
	int i = 0;

	if (DEBUGGER_EN(MSG))
		rga_req_log(request, "commit process: %s\n", request->session->pname);

	for (i = 0; i < request->task_count; i++) {
		struct rga_req *req = &(request->task_list[i]);

		if (DEBUGGER_EN(MSG)) {
			rga_req_log(request, "commit task[%d]:\n", i);
			rga_dump_req(request, req);
		}

		ret = rga_job_commit(req, request);
		if (ret < 0) {
			rga_req_err(request, "task[%d] job_commit failed.\n", i);

			return ret;
		}
	}

	if (request->sync_mode == RGA_BLIT_SYNC) {
		ret = rga_request_wait(request);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void rga_request_acquire_fence_work(struct work_struct *work)
{
	int ret;
	unsigned long flags;
	struct mm_struct *current_mm;
	struct rga_request *request = container_of(work, struct rga_request, fence_work);
	struct rga_pending_request_manager *request_manager = rga_drvdata->pend_request_manager;

	ret = rga_request_commit(request);
	if (ret < 0) {
		rga_req_err(request, "acquire_fence callback: request commit failed!\n");

		spin_lock_irqsave(&request->lock, flags);

		request->is_running = false;
		current_mm = request->current_mm;
		request->current_mm = NULL;

		spin_unlock_irqrestore(&request->lock, flags);

		rga_request_put_current_mm(current_mm);

		if (rga_dma_fence_get_status(request->release_fence) == 0)
			rga_dma_fence_signal(request->release_fence, ret);
	}

	mutex_lock(&request_manager->lock);
	rga_request_put(request);
	mutex_unlock(&request_manager->lock);
}

static void rga_request_acquire_fence_signaled_cb(struct dma_fence *fence,
						  struct dma_fence_cb *_waiter)
{
	struct rga_fence_waiter *waiter = (struct rga_fence_waiter *)_waiter;
	struct rga_request *request = (struct rga_request *)waiter->private;

	queue_work(system_highpri_wq, &request->fence_work);
	kfree(waiter);
}

int rga_request_release_signal(struct rga_scheduler_t *scheduler, struct rga_job *job)
{
	struct rga_pending_request_manager *request_manager;
	struct rga_request *request;
	struct mm_struct *current_mm;
	int finished_count, failed_count;
	bool is_finished = false;
	unsigned long flags;

	request_manager = rga_drvdata->pend_request_manager;
	if (request_manager == NULL) {
		rga_job_err(job, "rga_pending_request_manager is null!\n");
		return -EFAULT;
	}

	mutex_lock(&request_manager->lock);

	request = rga_request_lookup(request_manager, job->request_id);
	if (IS_ERR_OR_NULL(request)) {
		rga_job_err(job, "can not find internal request from id[%d]", job->request_id);
		mutex_unlock(&request_manager->lock);
		return -EINVAL;
	}

	rga_request_get(request);
	mutex_unlock(&request_manager->lock);

	spin_lock_irqsave(&request->lock, flags);

	if (job->ret < 0) {
		request->failed_task_count++;
		request->ret = job->ret;
	} else {
		request->finished_task_count++;
	}

	failed_count = request->failed_task_count;
	finished_count = request->finished_task_count;

	spin_unlock_irqrestore(&request->lock, flags);

	if ((failed_count + finished_count) >= request->task_count) {
		spin_lock_irqsave(&request->lock, flags);

		request->is_running = false;
		request->is_done = true;
		current_mm = request->current_mm;
		request->current_mm = NULL;

		spin_unlock_irqrestore(&request->lock, flags);

		rga_request_put_current_mm(current_mm);

		rga_dma_fence_signal(request->release_fence, request->ret);

		is_finished = true;
		job->timestamp.done = ktime_get();

		if (DEBUGGER_EN(MSG))
			rga_job_log(job, "finished %d failed %d\n", finished_count, failed_count);

		/* current submit request put */
		mutex_lock(&request_manager->lock);
		rga_request_put(request);
		mutex_unlock(&request_manager->lock);
	}

	mutex_lock(&request_manager->lock);

	if (is_finished)
		wake_up(&request->finished_wq);

	rga_request_put(request);

	mutex_unlock(&request_manager->lock);

	if (DEBUGGER_EN(TIME)) {
		rga_job_log(job,
			"stats: prepare %lld us, schedule %lld us, hardware %lld us, free %lld us\n",
			ktime_us_delta(job->timestamp.insert, job->timestamp.init),
			ktime_us_delta(job->timestamp.hw_execute, job->timestamp.insert),
			ktime_us_delta(job->timestamp.hw_done, job->timestamp.hw_execute),
			ktime_us_delta(ktime_get(), job->timestamp.hw_done));
		rga_job_log(job, "total: job done cost %lld us, cleanup done cost %lld us\n",
			ktime_us_delta(job->timestamp.done, job->timestamp.init),
			ktime_us_delta(ktime_get(), job->timestamp.init));
	}

	rga_job_cleanup(job);

	return 0;
}

struct rga_request *rga_request_config(struct rga_user_request *user_request)
{
	int ret;
	unsigned long flags;
	struct rga_pending_request_manager *request_manager;
	struct rga_request *request;
	struct rga_req *task_list;

	request_manager = rga_drvdata->pend_request_manager;
	if (request_manager == NULL) {
		rga_err("rga_pending_request_manager is null!\n");
		return ERR_PTR(-EFAULT);
	}

	mutex_lock(&request_manager->lock);

	request = rga_request_lookup(request_manager, user_request->id);
	if (IS_ERR_OR_NULL(request)) {
		rga_err("can not find request from id[%d]", user_request->id);
		mutex_unlock(&request_manager->lock);
		return ERR_PTR(-EINVAL);
	}

	rga_request_get(request);
	mutex_unlock(&request_manager->lock);

	task_list = kmalloc_array(user_request->task_num, sizeof(struct rga_req), GFP_KERNEL);
	if (task_list == NULL) {
		rga_req_err(request, "task_req list alloc error!\n");
		ret = -ENOMEM;
		goto err_put_request;
	}

	if (unlikely(copy_from_user(task_list, u64_to_user_ptr(user_request->task_ptr),
				    sizeof(struct rga_req) * user_request->task_num))) {
		rga_req_err(request, "rga_user_request task list copy_from_user failed\n");
		ret = -EFAULT;
		goto err_free_task_list;
	}

	spin_lock_irqsave(&request->lock, flags);

	request->use_batch_mode = true;
	request->task_list = task_list;
	request->task_count = user_request->task_num;
	request->sync_mode = user_request->sync_mode;
	request->mpi_config_flags = user_request->mpi_config_flags;
	request->acquire_fence_fd = user_request->acquire_fence_fd;
	request->feature = task_list[0].feature;

	spin_unlock_irqrestore(&request->lock, flags);

	return request;

err_free_task_list:
	kfree(task_list);
err_put_request:
	mutex_lock(&request_manager->lock);
	rga_request_put(request);
	mutex_unlock(&request_manager->lock);

	return ERR_PTR(ret);
}

struct rga_request *rga_request_kernel_config(struct rga_user_request *user_request)
{
	int ret = 0;
	unsigned long flags;
	struct rga_pending_request_manager *request_manager;
	struct rga_request *request;
	struct rga_req *task_list;

	request_manager = rga_drvdata->pend_request_manager;
	if (request_manager == NULL) {
		rga_err("rga_pending_request_manager is null!\n");
		return ERR_PTR(-EFAULT);
	}

	mutex_lock(&request_manager->lock);

	request = rga_request_lookup(request_manager, user_request->id);
	if (IS_ERR_OR_NULL(request)) {
		rga_err("can not find request from id[%d]", user_request->id);
		mutex_unlock(&request_manager->lock);
		return ERR_PTR(-EINVAL);
	}

	rga_request_get(request);
	mutex_unlock(&request_manager->lock);

	task_list = kmalloc_array(user_request->task_num, sizeof(struct rga_req), GFP_KERNEL);
	if (task_list == NULL) {
		rga_req_err(request, "task_req list alloc error!\n");
		ret = -ENOMEM;
		goto err_put_request;
	}

	memcpy(task_list, u64_to_user_ptr(user_request->task_ptr),
	       sizeof(struct rga_req) * user_request->task_num);

	spin_lock_irqsave(&request->lock, flags);

	request->use_batch_mode = true;
	request->task_list = task_list;
	request->task_count = user_request->task_num;
	request->sync_mode = user_request->sync_mode;
	request->mpi_config_flags = user_request->mpi_config_flags;
	request->acquire_fence_fd = user_request->acquire_fence_fd;

	spin_unlock_irqrestore(&request->lock, flags);

	return request;

err_put_request:
	mutex_lock(&request_manager->lock);
	rga_request_put(request);
	mutex_unlock(&request_manager->lock);

	return ERR_PTR(ret);
}

int rga_request_submit(struct rga_request *request)
{
	int ret = 0;
	unsigned long flags;
	struct dma_fence *release_fence;

	spin_lock_irqsave(&request->lock, flags);

	if (request->is_running) {
		spin_unlock_irqrestore(&request->lock, flags);

		rga_req_err(request, "can not re-config when request is running\n");
		ret = -EFAULT;
		goto err_abort_request;
	}

	if (request->task_list == NULL) {
		spin_unlock_irqrestore(&request->lock, flags);

		rga_req_err(request, "can not find task list\n");
		ret = -EINVAL;
		goto err_abort_request;
	}

	/* Reset */
	request->is_running = true;
	request->is_done = false;
	request->finished_task_count = 0;
	request->failed_task_count = 0;
	request->ret = 0;

	/* Unlock after ensuring that the current request will not be resubmitted. */
	spin_unlock_irqrestore(&request->lock, flags);

	if (request->sync_mode == RGA_BLIT_ASYNC) {
		release_fence = rga_dma_fence_alloc();
		if (IS_ERR_OR_NULL(release_fence)) {
			rga_req_err(request, "Can not alloc release fence!\n");
			ret = IS_ERR(release_fence) ? PTR_ERR(release_fence) : -EINVAL;
			goto err_abort_request;
		}

		request->current_mm = rga_request_get_current_mm(request);
		request->release_fence = release_fence;

		if (request->acquire_fence_fd > 0) {
			INIT_WORK(&request->fence_work, rga_request_acquire_fence_work);
			ret = rga_request_add_acquire_fence_callback(
				request->acquire_fence_fd, request,
				rga_request_acquire_fence_signaled_cb);
			if (ret == 0) {
				/* acquire fence active */
				goto export_release_fence_fd;
			} else if (ret > 0) {
				/* acquire fence has been signaled */
				goto request_commit;
			} else {
				rga_req_err(request, "Failed to add callback with acquire fence fd[%d]!\n",
				       request->acquire_fence_fd);

				rga_dma_fence_put(request->release_fence);
				request->release_fence = NULL;
				goto err_put_current_mm;
			}
		}
	} else {
		request->current_mm = rga_request_get_current_mm(request);
		request->release_fence = NULL;
	}

request_commit:
	ret = rga_request_commit(request);
	if (ret < 0) {
		rga_req_err(request, "request commit failed!\n");
		goto err_put_current_mm;
	}

export_release_fence_fd:
	if (request->release_fence != NULL) {
		ret = rga_dma_fence_get_fd(request->release_fence);
		if (ret < 0) {
			rga_req_err(request, "Failed to alloc release fence fd!\n");
			goto err_put_current_mm;
		}

		request->release_fence_fd = ret;
	}

	return 0;

err_put_current_mm:
	rga_request_put_current_mm(request->current_mm);
	request->current_mm = NULL;

err_abort_request:
	rga_request_release_abort(request, ret);

	return ret;
}

int rga_request_mpi_submit(struct rga_req *req, struct rga_request *request)
{
	int ret = 0;
	unsigned long flags;
	struct rga_pending_request_manager *request_manager;

	request_manager = rga_drvdata->pend_request_manager;

	if (request->sync_mode == RGA_BLIT_ASYNC) {
		rga_req_err(request, "mpi unsupported async mode!\n");
		ret = -EINVAL;
		goto err_abort_request;
	}

	spin_lock_irqsave(&request->lock, flags);

	if (request->is_running) {
		rga_req_err(request, "can not re-config when request is running");
		spin_unlock_irqrestore(&request->lock, flags);
		ret = -EFAULT;
		goto err_abort_request;
	}

	if (request->task_list == NULL) {
		rga_req_err(request, "can not find task list");
		spin_unlock_irqrestore(&request->lock, flags);
		ret = -EINVAL;
		goto err_abort_request;
	}

	/* Reset */
	request->is_running = true;
	request->is_done = false;
	request->finished_task_count = 0;
	request->failed_task_count = 0;
	request->ret = 0;

	spin_unlock_irqrestore(&request->lock, flags);

	/*
	 * The mpi submit will use the request repeatedly, so an additional
	 * get() is added here.
	 */
	mutex_lock(&request_manager->lock);
	rga_request_get(request);
	mutex_unlock(&request_manager->lock);

	ret = rga_job_commit(req, request);
	if (ret < 0) {
		rga_req_err(request, "failed to commit job!\n");
		goto err_abort_request;
	}

	ret = rga_request_wait(request);
	if (ret < 0)
		goto err_abort_request;

	return 0;

err_abort_request:
	rga_request_release_abort(request, ret);

	return ret;
}

int rga_request_free(struct rga_request *request)
{
	struct rga_pending_request_manager *request_manager;
	struct rga_req *task_list;
	unsigned long flags;

	request_manager = rga_drvdata->pend_request_manager;
	if (request_manager == NULL) {
		rga_err("rga_pending_request_manager is null!\n");
		return -EFAULT;
	}

	WARN_ON(!mutex_is_locked(&request_manager->lock));

	if (IS_ERR_OR_NULL(request)) {
		rga_err("request already freed");
		return -EFAULT;
	}

	request_manager->request_count--;
	idr_remove(&request_manager->request_idr, request->id);

	spin_lock_irqsave(&request->lock, flags);

	task_list = request->task_list;

	spin_unlock_irqrestore(&request->lock, flags);

	if (task_list != NULL)
		kfree(task_list);

	rga_session_put(request->session);

	kfree(request);

	return 0;
}

static void rga_request_kref_release(struct kref *ref)
{
	struct rga_request *request;
	struct mm_struct *current_mm;
	unsigned long flags;

	request = container_of(ref, struct rga_request, refcount);

	if (rga_dma_fence_get_status(request->release_fence) == 0)
		rga_dma_fence_signal(request->release_fence, -EFAULT);

	spin_lock_irqsave(&request->lock, flags);

	rga_dma_fence_put(request->release_fence);
	current_mm = request->current_mm;
	request->current_mm = NULL;

	if (!request->is_running || request->is_done) {
		spin_unlock_irqrestore(&request->lock, flags);

		rga_request_put_current_mm(current_mm);

		goto free_request;
	}

	spin_unlock_irqrestore(&request->lock, flags);

	rga_request_put_current_mm(current_mm);

	rga_request_scheduler_job_abort(request);

free_request:
	rga_request_free(request);
}

/*
 * Called at driver close to release the request's id references.
 */
static int rga_request_free_cb(int id, void *ptr, void *data)
{
	return rga_request_free((struct rga_request *)ptr);
}

int rga_request_alloc(uint32_t flags, struct rga_session *session)
{
	int new_id;
	struct rga_pending_request_manager *request_manager;
	struct rga_request *request;

	request_manager = rga_drvdata->pend_request_manager;
	if (request_manager == NULL) {
		rga_err("rga_pending_request_manager is null!\n");
		return -EFAULT;
	}

	request = kzalloc(sizeof(*request), GFP_KERNEL);
	if (request == NULL) {
		rga_err("can not kzalloc for rga_request\n");
		return -ENOMEM;
	}

	spin_lock_init(&request->lock);
	init_waitqueue_head(&request->finished_wq);

	request->pid = current->pid;
	request->flags = flags;

	rga_session_get(session);
	request->session = session;

	kref_init(&request->refcount);

	/*
	 * Get the user-visible handle using idr. Preload and perform
	 * allocation under our spinlock.
	 */
	mutex_lock(&request_manager->lock);

	idr_preload(GFP_KERNEL);
	new_id = idr_alloc_cyclic(&request_manager->request_idr, request, 1, 0, GFP_NOWAIT);
	idr_preload_end();
	if (new_id < 0) {
		rga_err("request alloc id failed!\n");

		mutex_unlock(&request_manager->lock);
		rga_session_put(session);
		kfree(request);
		return new_id;
	}

	request->id = new_id;
	request_manager->request_count++;

	mutex_unlock(&request_manager->lock);

	return request->id;
}

int rga_request_put(struct rga_request *request)
{
	return kref_put(&request->refcount, rga_request_kref_release);
}

void rga_request_get(struct rga_request *request)
{
	kref_get(&request->refcount);
}

int rga_request_manager_init(struct rga_pending_request_manager **request_manager_session)
{
	struct rga_pending_request_manager *request_manager = NULL;

	*request_manager_session = kzalloc(sizeof(struct rga_pending_request_manager), GFP_KERNEL);
	if (*request_manager_session == NULL) {
		pr_err("can not kzalloc for rga_pending_request_manager\n");
		return -ENOMEM;
	}

	request_manager = *request_manager_session;

	mutex_init(&request_manager->lock);

	idr_init_base(&request_manager->request_idr, 1);

	return 0;
}

int rga_request_manager_remove(struct rga_pending_request_manager **request_manager_session)
{
	struct rga_pending_request_manager *request_manager = *request_manager_session;

	mutex_lock(&request_manager->lock);

	idr_for_each(&request_manager->request_idr, &rga_request_free_cb, request_manager);
	idr_destroy(&request_manager->request_idr);

	mutex_unlock(&request_manager->lock);

	kfree(*request_manager_session);

	*request_manager_session = NULL;

	return 0;
}
