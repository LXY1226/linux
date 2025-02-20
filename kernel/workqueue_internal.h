#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * kernel/workqueue_internal.h
 *
 * Workqueue internal header file.  Only to be included by workqueue and
 * core kernel subsystems.
 */
#ifndef _KERNEL_WORKQUEUE_INTERNAL_H
#define _KERNEL_WORKQUEUE_INTERNAL_H

#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/preempt.h>

struct worker_pool;

/*
 * The poor guys doing the actual heavy lifting.  All on-duty workers are
 * either serving the manager role, on idle list or on busy hash.  For
 * details on the locking annotation (L, I, X...), refer to workqueue.c.
 *
 * Only to be used in workqueue and async.
 */
struct worker {
	/* on idle list while idle, on busy hash table while busy */
	union {
		struct list_head	entry;	/* L: while idle */
		struct hlist_node	hentry;	/* L: while busy */
	};

	struct work_struct	*current_work;	/* L: work being processed */
	work_func_t		current_func;	/* L: current_work's fn */
	struct pool_workqueue	*current_pwq; /* L: current_work's pwq */
	bool			desc_valid;	/* ->desc is valid */
	struct list_head	scheduled;	/* L: scheduled works */

	/* 64 bytes boundary on 64bit, 32 on 32bit */

	struct task_struct	*task;		/* I: worker task */
	struct worker_pool	*pool;		/* I: the associated pool */
						/* L: for rescuers */
	struct list_head	node;		/* A: anchored at pool->workers */
						/* A: runs through worker->node */

	unsigned long		last_active;	/* L: last active timestamp */
	unsigned int		flags;		/* X: flags */
	int			id;		/* I: worker id */

	/*
	 * Opaque string set with work_set_desc().  Printed out with task
	 * dump for debugging - WARN, BUG, panic or sysrq.
	 */
	char			desc[WORKER_DESC_LEN];

	/* used only by rescuers to point to the target workqueue */
	struct workqueue_struct	*rescue_wq;	/* I: the workqueue to rescue */
};

/**
 * current_wq_worker - return struct worker if %current is a workqueue worker
 */
static inline struct worker *current_wq_worker(void)
{
	if (in_task() && (current->flags & PF_WQ_WORKER))
		return kthread_data(current);
	return NULL;
}

/*
 * Scheduler hooks for concurrency managed workqueue.  Only to be used from
 * sched/core.c and workqueue.c.
 */
void wq_worker_waking_up(struct task_struct *task, int cpu);
struct task_struct *wq_worker_sleeping(struct task_struct *task, int cpu);

#ifdef MY_ABC_HERE
/* For in-thread I/O accumulation. We don't need atomic ops */
struct work_io_acct {
	unsigned long last_update_jiffies;

	/**
	 * Please refer to task_io_accounting.h for the following
	 * I/O statistics
	 */
	u64 read_bytes;
	u64 write_bytes;
	u64 cancelled_write_bytes;
};

struct work_acct {
	struct workqueue_struct *wq;
	work_func_t func;
	struct work_io_acct io_acct;
};

void worker_run_work(struct worker *worker, struct work_struct *work);
void account_work_time(struct work_acct *acct, u64 us, gfp_t gfp);
struct workqueue_struct* get_pwq_wq(struct pool_workqueue *pwq);
#endif /* MY_ABC_HERE */

#endif /* _KERNEL_WORKQUEUE_INTERNAL_H */
