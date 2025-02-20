#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
 * Copyright (C) 2014 Fujitsu.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/freezer.h>
#include "async-thread.h"
#include "ctree.h"

#define WORK_DONE_BIT 0
#define WORK_ORDER_DONE_BIT 1
#define WORK_HIGH_PRIO_BIT 2

#define NO_THRESHOLD (-1)
#define DFT_THRESHOLD (32)

struct __btrfs_workqueue {
	struct workqueue_struct *normal_wq;

	/* File system this workqueue services */
	struct btrfs_fs_info *fs_info;

	/* List head pointing to ordered work list */
	struct list_head ordered_list;

	/* Spinlock for ordered_list */
	spinlock_t list_lock;

	/* Thresholding related variants */
	atomic_t pending;

	/* Up limit of concurrency workers */
	int limit_active;

	/* Current number of concurrency workers */
	int current_active;

	/* Threshold to change current_active */
	int thresh;
	unsigned int count;
	spinlock_t thres_lock;
};

struct btrfs_workqueue {
	struct __btrfs_workqueue *normal;
	struct __btrfs_workqueue *high;
};

static void normal_work_helper(struct btrfs_work *work);

#define BTRFS_WORK_HELPER(name)					\
void btrfs_##name(struct work_struct *arg)				\
{									\
	struct btrfs_work *work = container_of(arg, struct btrfs_work,	\
					       normal_work);		\
	normal_work_helper(work);					\
}

struct btrfs_fs_info *
btrfs_workqueue_owner(struct __btrfs_workqueue *wq)
{
	return wq->fs_info;
}

struct btrfs_fs_info *
btrfs_work_owner(struct btrfs_work *work)
{
	return work->wq->fs_info;
}

bool btrfs_workqueue_normal_congested(struct btrfs_workqueue *wq)
{
	/*
	 * We could compare wq->normal->pending with num_online_cpus()
	 * to support "thresh == NO_THRESHOLD" case, but it requires
	 * moving up atomic_inc/dec in thresh_queue/exec_hook. Let's
	 * postpone it until someone needs the support of that case.
	 */
	if (wq->normal->thresh == NO_THRESHOLD)
		return false;

	return atomic_read(&wq->normal->pending) > wq->normal->thresh * 2;
}

BTRFS_WORK_HELPER(worker_helper);
BTRFS_WORK_HELPER(delalloc_helper);
BTRFS_WORK_HELPER(flush_delalloc_helper);
BTRFS_WORK_HELPER(cache_helper);
BTRFS_WORK_HELPER(submit_helper);
BTRFS_WORK_HELPER(fixup_helper);
BTRFS_WORK_HELPER(endio_helper);
BTRFS_WORK_HELPER(endio_meta_helper);
#ifdef MY_DEF_HERE
BTRFS_WORK_HELPER(endio_meta_fix_helper);
#endif /* MY_DEF_HERE */
BTRFS_WORK_HELPER(endio_meta_write_helper);
BTRFS_WORK_HELPER(endio_raid56_helper);
BTRFS_WORK_HELPER(endio_repair_helper);
BTRFS_WORK_HELPER(rmw_helper);
BTRFS_WORK_HELPER(endio_write_helper);
#ifdef MY_DEF_HERE
BTRFS_WORK_HELPER(endio_write_sync_helper);
#endif /* MY_DEF_HERE */
BTRFS_WORK_HELPER(freespace_write_helper);
BTRFS_WORK_HELPER(delayed_meta_helper);
BTRFS_WORK_HELPER(readahead_helper);
#ifdef MY_DEF_HERE
BTRFS_WORK_HELPER(reada_path_start_helper);
#endif /* MY_DEF_HERE */
BTRFS_WORK_HELPER(qgroup_rescan_helper);
#ifdef MY_DEF_HERE
BTRFS_WORK_HELPER(usrquota_rescan_helper);
#endif /* MY_DEF_HERE */
BTRFS_WORK_HELPER(extent_refs_helper);
BTRFS_WORK_HELPER(scrub_helper);
BTRFS_WORK_HELPER(scrubwrc_helper);
BTRFS_WORK_HELPER(scrubnc_helper);
BTRFS_WORK_HELPER(scrubparity_helper);
#ifdef MY_DEF_HERE
BTRFS_WORK_HELPER(syno_cow_endio_helper);
BTRFS_WORK_HELPER(syno_nocow_endio_helper);
BTRFS_WORK_HELPER(syno_high_priority_endio_helper);
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
BTRFS_WORK_HELPER(syno_bg_cache_helper);
#endif /* MY_DEF_HERE */

static struct __btrfs_workqueue *
__btrfs_alloc_workqueue(struct btrfs_fs_info *fs_info, const char *name,
			unsigned int flags, int limit_active, int thresh)
{
	struct __btrfs_workqueue *ret = kzalloc(sizeof(*ret), GFP_KERNEL);

	if (!ret)
		return NULL;

	ret->fs_info = fs_info;
	ret->limit_active = limit_active;
	atomic_set(&ret->pending, 0);
	if (thresh == 0)
		thresh = DFT_THRESHOLD;
	/* For low threshold, disabling threshold is a better choice */
	if (thresh < DFT_THRESHOLD) {
		ret->current_active = limit_active;
		ret->thresh = NO_THRESHOLD;
	} else {
		/*
		 * For threshold-able wq, let its concurrency grow on demand.
		 * Use minimal max_active at alloc time to reduce resource
		 * usage.
		 */
		ret->current_active = 1;
		ret->thresh = thresh;
	}

	if (flags & WQ_HIGHPRI)
		ret->normal_wq = alloc_workqueue("%s-%s-high", flags,
						 ret->current_active, "btrfs",
						 name);
	else
		ret->normal_wq = alloc_workqueue("%s-%s", flags,
						 ret->current_active, "btrfs",
						 name);
	if (!ret->normal_wq) {
		kfree(ret);
		return NULL;
	}

	INIT_LIST_HEAD(&ret->ordered_list);
	spin_lock_init(&ret->list_lock);
	spin_lock_init(&ret->thres_lock);
	trace_btrfs_workqueue_alloc(ret, name, flags & WQ_HIGHPRI);
	return ret;
}

static inline void
__btrfs_destroy_workqueue(struct __btrfs_workqueue *wq);

struct btrfs_workqueue *btrfs_alloc_workqueue(struct btrfs_fs_info *fs_info,
					      const char *name,
					      unsigned int flags,
					      int limit_active,
					      int thresh)
{
	struct btrfs_workqueue *ret = kzalloc(sizeof(*ret), GFP_KERNEL);

	if (!ret)
		return NULL;

	ret->normal = __btrfs_alloc_workqueue(fs_info, name,
					      flags & ~WQ_HIGHPRI,
					      limit_active, thresh);
	if (!ret->normal) {
		kfree(ret);
		return NULL;
	}

	if (flags & WQ_HIGHPRI) {
		ret->high = __btrfs_alloc_workqueue(fs_info, name, flags,
						    limit_active, thresh);
		if (!ret->high) {
			__btrfs_destroy_workqueue(ret->normal);
			kfree(ret);
			return NULL;
		}
	}
	return ret;
}

#ifdef MY_ABC_HERE
struct btrfs_workqueue *btrfs_alloc_workqueue_with_sysfs(
	struct btrfs_fs_info *fs_info,
	const char *name,
	unsigned int flags,
	int limit_active,
	int thresh)
{
	/* 32 for sid, 12 for prefix and postfix in __btrfs_alloc_workqueue */
	char trimmed_name[21] = {0};
	char name_sid[WQ_NAME_LEN] = {0};
	BUILD_BUG_ON((WQ_NAME_LEN - 44) < 20);

	snprintf(trimmed_name, sizeof(trimmed_name), "%s", name);
	snprintf(name_sid, sizeof(name_sid), "%s-%s", trimmed_name, fs_info->sb->s_id);
	return btrfs_alloc_workqueue(fs_info, name_sid, flags | WQ_SYSFS, limit_active, thresh);
}
#endif /* MY_ABC_HERE */

/*
 * Hook for threshold which will be called in btrfs_queue_work.
 * This hook WILL be called in IRQ handler context,
 * so workqueue_set_max_active MUST NOT be called in this hook
 */
static inline void thresh_queue_hook(struct __btrfs_workqueue *wq)
{
	if (wq->thresh == NO_THRESHOLD)
		return;
	atomic_inc(&wq->pending);
}

/*
 * Hook for threshold which will be called before executing the work,
 * This hook is called in kthread content.
 * So workqueue_set_max_active is called here.
 */
static inline void thresh_exec_hook(struct __btrfs_workqueue *wq)
{
	int new_current_active;
	long pending;
	int need_change = 0;

	if (wq->thresh == NO_THRESHOLD)
		return;

	atomic_dec(&wq->pending);
	spin_lock(&wq->thres_lock);
	/*
	 * Use wq->count to limit the calling frequency of
	 * workqueue_set_max_active.
	 */
	wq->count++;
	wq->count %= (wq->thresh / 4);
	if (!wq->count)
		goto  out;
	new_current_active = wq->current_active;

	/*
	 * pending may be changed later, but it's OK since we really
	 * don't need it so accurate to calculate new_max_active.
	 */
	pending = atomic_read(&wq->pending);
	if (pending > wq->thresh)
		new_current_active++;
	if (pending < wq->thresh / 2)
		new_current_active--;
	new_current_active = clamp_val(new_current_active, 1, wq->limit_active);
	if (new_current_active != wq->current_active)  {
		need_change = 1;
		wq->current_active = new_current_active;
	}
out:
	spin_unlock(&wq->thres_lock);

	if (need_change) {
		workqueue_set_max_active(wq->normal_wq, wq->current_active);
	}
}

static void run_ordered_work(struct __btrfs_workqueue *wq,
			     struct btrfs_work *self)
{
	struct list_head *list = &wq->ordered_list;
	struct btrfs_work *work;
	spinlock_t *lock = &wq->list_lock;
	unsigned long flags;
	bool free_self = false;

	while (1) {
		spin_lock_irqsave(lock, flags);
		if (list_empty(list))
			break;
		work = list_entry(list->next, struct btrfs_work,
				  ordered_list);
		if (!test_bit(WORK_DONE_BIT, &work->flags))
			break;
		/*
		 * Orders all subsequent loads after reading WORK_DONE_BIT,
		 * paired with the smp_mb__before_atomic in btrfs_work_helper
		 * this guarantees that the ordered function will see all
		 * updates from ordinary work function.
		 */
		smp_rmb();

		/*
		 * we are going to call the ordered done function, but
		 * we leave the work item on the list as a barrier so
		 * that later work items that are done don't have their
		 * functions called before this one returns
		 */
		if (test_and_set_bit(WORK_ORDER_DONE_BIT, &work->flags))
			break;
		trace_btrfs_ordered_sched(work);
		spin_unlock_irqrestore(lock, flags);
		work->ordered_func(work);

		/* now take the lock again and drop our item from the list */
		spin_lock_irqsave(lock, flags);
		list_del(&work->ordered_list);
		spin_unlock_irqrestore(lock, flags);

		if (work == self) {
			/*
			 * This is the work item that the worker is currently
			 * executing.
			 *
			 * The kernel workqueue code guarantees non-reentrancy
			 * of work items. I.e., if a work item with the same
			 * address and work function is queued twice, the second
			 * execution is blocked until the first one finishes. A
			 * work item may be freed and recycled with the same
			 * work function; the workqueue code assumes that the
			 * original work item cannot depend on the recycled work
			 * item in that case (see find_worker_executing_work()).
			 *
			 * Note that the work of one Btrfs filesystem may depend
			 * on the work of another Btrfs filesystem via, e.g., a
			 * loop device. Therefore, we must not allow the current
			 * work item to be recycled until we are really done,
			 * otherwise we break the above assumption and can
			 * deadlock.
			 */
			free_self = true;
		} else {
			/*
			 * we don't want to call the ordered free functions
			 * with the lock held though
			 */
			work->ordered_free(work);
			trace_btrfs_all_work_done(work);
		}
	}
	spin_unlock_irqrestore(lock, flags);

	if (free_self) {
		self->ordered_free(self);
		trace_btrfs_all_work_done(self);
	}
}

static void normal_work_helper(struct btrfs_work *work)
{
	struct __btrfs_workqueue *wq;
	int need_order = 0;

	/*
	 * We should not touch things inside work in the following cases:
	 * 1) after work->func() if it has no ordered_free
	 *    Since the struct is freed in work->func().
	 * 2) after setting WORK_DONE_BIT
	 *    The work may be freed in other threads almost instantly.
	 * So we save the needed things here.
	 */
	if (work->ordered_func)
		need_order = 1;
	wq = work->wq;

	trace_btrfs_work_sched(work);
	thresh_exec_hook(wq);
	work->func(work);
	if (need_order) {
		/*
		 * Ensures all memory accesses done in the work function are
		 * ordered before setting the WORK_DONE_BIT. Ensuring the thread
		 * which is going to executed the ordered work sees them.
		 * Pairs with the smp_rmb in run_ordered_work.
		 */
		smp_mb__before_atomic();
		set_bit(WORK_DONE_BIT, &work->flags);
		run_ordered_work(wq, work);
	}
	if (!need_order)
		trace_btrfs_all_work_done(work);
}

void btrfs_init_work(struct btrfs_work *work, btrfs_work_func_t uniq_func,
		     btrfs_func_t func,
		     btrfs_func_t ordered_func,
		     btrfs_func_t ordered_free)
{
	work->func = func;
	work->ordered_func = ordered_func;
	work->ordered_free = ordered_free;
	INIT_WORK(&work->normal_work, uniq_func);
	INIT_LIST_HEAD(&work->ordered_list);
	work->flags = 0;
}

static inline void __btrfs_queue_work(struct __btrfs_workqueue *wq,
				      struct btrfs_work *work)
{
	unsigned long flags;

	work->wq = wq;
	thresh_queue_hook(wq);
	if (work->ordered_func) {
		spin_lock_irqsave(&wq->list_lock, flags);
		list_add_tail(&work->ordered_list, &wq->ordered_list);
		spin_unlock_irqrestore(&wq->list_lock, flags);
	}
	trace_btrfs_work_queued(work);
	queue_work(wq->normal_wq, &work->normal_work);
}

void btrfs_queue_work(struct btrfs_workqueue *wq,
		      struct btrfs_work *work)
{
	struct __btrfs_workqueue *dest_wq;

	if (test_bit(WORK_HIGH_PRIO_BIT, &work->flags) && wq->high)
		dest_wq = wq->high;
	else
		dest_wq = wq->normal;
	__btrfs_queue_work(dest_wq, work);
}

static inline void
__btrfs_destroy_workqueue(struct __btrfs_workqueue *wq)
{
	destroy_workqueue(wq->normal_wq);
	trace_btrfs_workqueue_destroy(wq);
	kfree(wq);
}

void btrfs_destroy_workqueue(struct btrfs_workqueue *wq)
{
	if (!wq)
		return;
	if (wq->high)
		__btrfs_destroy_workqueue(wq->high);
	__btrfs_destroy_workqueue(wq->normal);
	kfree(wq);
}

void btrfs_workqueue_set_max(struct btrfs_workqueue *wq, int limit_active)
{
	if (!wq)
		return;
	wq->normal->limit_active = limit_active;
	if (wq->high)
		wq->high->limit_active = limit_active;
}

void btrfs_set_work_high_priority(struct btrfs_work *work)
{
	set_bit(WORK_HIGH_PRIO_BIT, &work->flags);
}

void btrfs_flush_workqueue(struct btrfs_workqueue *wq)
{
	if (wq->high)
		flush_workqueue(wq->high->normal_wq);

	flush_workqueue(wq->normal->normal_wq);
}
