#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#ifndef _RAID5_H
#define _RAID5_H

#include <linux/raid/xor.h>
#include <linux/dmaengine.h>

/*
 *
 * Each stripe contains one buffer per device.  Each buffer can be in
 * one of a number of states stored in "flags".  Changes between
 * these states happen *almost* exclusively under the protection of the
 * STRIPE_ACTIVE flag.  Some very specific changes can happen in bi_end_io, and
 * these are not protected by STRIPE_ACTIVE.
 *
 * The flag bits that are used to represent these states are:
 *   R5_UPTODATE and R5_LOCKED
 *
 * State Empty == !UPTODATE, !LOCK
 *        We have no data, and there is no active request
 * State Want == !UPTODATE, LOCK
 *        A read request is being submitted for this block
 * State Dirty == UPTODATE, LOCK
 *        Some new data is in this buffer, and it is being written out
 * State Clean == UPTODATE, !LOCK
 *        We have valid data which is the same as on disc
 *
 * The possible state transitions are:
 *
 *  Empty -> Want   - on read or write to get old data for  parity calc
 *  Empty -> Dirty  - on compute_parity to satisfy write/sync request.
 *  Empty -> Clean  - on compute_block when computing a block for failed drive
 *  Want  -> Empty  - on failed read
 *  Want  -> Clean  - on successful completion of read request
 *  Dirty -> Clean  - on successful completion of write request
 *  Dirty -> Clean  - on failed write
 *  Clean -> Dirty  - on compute_parity to satisfy write/sync (RECONSTRUCT or RMW)
 *
 * The Want->Empty, Want->Clean, Dirty->Clean, transitions
 * all happen in b_end_io at interrupt time.
 * Each sets the Uptodate bit before releasing the Lock bit.
 * This leaves one multi-stage transition:
 *    Want->Dirty->Clean
 * This is safe because thinking that a Clean buffer is actually dirty
 * will at worst delay some action, and the stripe will be scheduled
 * for attention after the transition is complete.
 *
 * There is one possibility that is not covered by these states.  That
 * is if one drive has failed and there is a spare being rebuilt.  We
 * can't distinguish between a clean block that has been generated
 * from parity calculations, and a clean block that has been
 * successfully written to the spare ( or to parity when resyncing).
 * To distinguish these states we have a stripe bit STRIPE_INSYNC that
 * is set whenever a write is scheduled to the spare, or to the parity
 * disc if there is no spare.  A sync request clears this bit, and
 * when we find it set with no buffers locked, we know the sync is
 * complete.
 *
 * Buffers for the md device that arrive via make_request are attached
 * to the appropriate stripe in one of two lists linked on b_reqnext.
 * One list (bh_read) for read requests, one (bh_write) for write.
 * There should never be more than one buffer on the two lists
 * together, but we are not guaranteed of that so we allow for more.
 *
 * If a buffer is on the read list when the associated cache buffer is
 * Uptodate, the data is copied into the read buffer and it's b_end_io
 * routine is called.  This may happen in the end_request routine only
 * if the buffer has just successfully been read.  end_request should
 * remove the buffers from the list and then set the Uptodate bit on
 * the buffer.  Other threads may do this only if they first check
 * that the Uptodate bit is set.  Once they have checked that they may
 * take buffers off the read queue.
 *
 * When a buffer on the write list is committed for write it is copied
 * into the cache buffer, which is then marked dirty, and moved onto a
 * third list, the written list (bh_written).  Once both the parity
 * block and the cached buffer are successfully written, any buffer on
 * a written list can be returned with b_end_io.
 *
 * The write list and read list both act as fifos.  The read list,
 * write list and written list are protected by the device_lock.
 * The device_lock is only for list manipulations and will only be
 * held for a very short time.  It can be claimed from interrupts.
 *
 *
 * Stripes in the stripe cache can be on one of two lists (or on
 * neither).  The "inactive_list" contains stripes which are not
 * currently being used for any request.  They can freely be reused
 * for another stripe.  The "handle_list" contains stripes that need
 * to be handled in some way.  Both of these are fifo queues.  Each
 * stripe is also (potentially) linked to a hash bucket in the hash
 * table so that it can be found by sector number.  Stripes that are
 * not hashed must be on the inactive_list, and will normally be at
 * the front.  All stripes start life this way.
 *
 * The inactive_list, handle_list and hash bucket lists are all protected by the
 * device_lock.
 *  - stripes have a reference counter. If count==0, they are on a list.
 *  - If a stripe might need handling, STRIPE_HANDLE is set.
 *  - When refcount reaches zero, then if STRIPE_HANDLE it is put on
 *    handle_list else inactive_list
 *
 * This, combined with the fact that STRIPE_HANDLE is only ever
 * cleared while a stripe has a non-zero count means that if the
 * refcount is 0 and STRIPE_HANDLE is set, then it is on the
 * handle_list and if recount is 0 and STRIPE_HANDLE is not set, then
 * the stripe is on inactive_list.
 *
 * The possible transitions are:
 *  activate an unhashed/inactive stripe (get_active_stripe())
 *     lockdev check-hash unlink-stripe cnt++ clean-stripe hash-stripe unlockdev
 *  activate a hashed, possibly active stripe (get_active_stripe())
 *     lockdev check-hash if(!cnt++)unlink-stripe unlockdev
 *  attach a request to an active stripe (add_stripe_bh())
 *     lockdev attach-buffer unlockdev
 *  handle a stripe (handle_stripe())
 *     setSTRIPE_ACTIVE,  clrSTRIPE_HANDLE ...
 *		(lockdev check-buffers unlockdev) ..
 *		change-state ..
 *		record io/ops needed clearSTRIPE_ACTIVE schedule io/ops
 *  release an active stripe (release_stripe())
 *     lockdev if (!--cnt) { if  STRIPE_HANDLE, add to handle_list else add to inactive-list } unlockdev
 *
 * The refcount counts each thread that have activated the stripe,
 * plus raid5d if it is handling it, plus one for each active request
 * on a cached buffer, and plus one if the stripe is undergoing stripe
 * operations.
 *
 * The stripe operations are:
 * -copying data between the stripe cache and user application buffers
 * -computing blocks to save a disk access, or to recover a missing block
 * -updating the parity on a write operation (reconstruct write and
 *  read-modify-write)
 * -checking parity correctness
 * -running i/o to disk
 * These operations are carried out by raid5_run_ops which uses the async_tx
 * api to (optionally) offload operations to dedicated hardware engines.
 * When requesting an operation handle_stripe sets the pending bit for the
 * operation and increments the count.  raid5_run_ops is then run whenever
 * the count is non-zero.
 * There are some critical dependencies between the operations that prevent some
 * from being requested while another is in flight.
 * 1/ Parity check operations destroy the in cache version of the parity block,
 *    so we prevent parity dependent operations like writes and compute_blocks
 *    from starting while a check is in progress.  Some dma engines can perform
 *    the check without damaging the parity block, in these cases the parity
 *    block is re-marked up to date (assuming the check was successful) and is
 *    not re-read from disk.
 * 2/ When a write operation is requested we immediately lock the affected
 *    blocks, and mark them as not up to date.  This causes new read requests
 *    to be held off, as well as parity checks and compute block operations.
 * 3/ Once a compute block operation has been requested handle_stripe treats
 *    that block as if it is up to date.  raid5_run_ops guaruntees that any
 *    operation that is dependent on the compute block result is initiated after
 *    the compute block completes.
 */

/*
 * Operations state - intermediate states that are visible outside of
 *   STRIPE_ACTIVE.
 * In general _idle indicates nothing is running, _run indicates a data
 * processing operation is active, and _result means the data processing result
 * is stable and can be acted upon.  For simple operations like biofill and
 * compute that only have an _idle and _run state they are indicated with
 * sh->state flags (STRIPE_BIOFILL_RUN and STRIPE_COMPUTE_RUN)
 */
/**
 * enum check_states - handles syncing / repairing a stripe
 * @check_state_idle - check operations are quiesced
 * @check_state_run - check operation is running
 * @check_state_result - set outside lock when check result is valid
 * @check_state_compute_run - check failed and we are repairing
 * @check_state_compute_result - set outside lock when compute result is valid
 */
enum check_states {
	check_state_idle = 0,
	check_state_run, /* xor parity check */
	check_state_run_q, /* q-parity check */
	check_state_run_pq, /* pq dual parity check */
	check_state_check_result,
	check_state_compute_run, /* parity repair */
	check_state_compute_result,
};

/**
 * enum reconstruct_states - handles writing or expanding a stripe
 */
enum reconstruct_states {
	reconstruct_state_idle = 0,
	reconstruct_state_prexor_drain_run,	/* prexor-write */
	reconstruct_state_drain_run,		/* write */
	reconstruct_state_run,			/* expand */
	reconstruct_state_prexor_drain_result,
	reconstruct_state_drain_result,
	reconstruct_state_result,
};

struct stripe_head {
	struct hlist_node	hash;
	struct list_head	lru;	      /* inactive_list or handle_list */
	struct llist_node	release_list;
	struct r5conf		*raid_conf;
	short			generation;	/* increments with every
						 * reshape */
	sector_t		sector;		/* sector of this row */
	short			pd_idx;		/* parity disk index */
	short			qd_idx;		/* 'Q' disk index for raid6 */
	short			ddf_layout;/* use DDF ordering to calculate Q */
	short			hash_lock_index;
	unsigned long		state;		/* state flags */
	atomic_t		count;	      /* nr of active thread/requests */
#ifdef MY_ABC_HERE
	atomic_t delayed_cnt;
#endif /* MY_ABC_HERE */
	int			bm_seq;	/* sequence number for bitmap flushes */
	int			disks;		/* disks in stripe */
	int			overwrite_disks; /* total overwrite disks in stripe,
						  * this is only checked when stripe
						  * has STRIPE_BATCH_READY
						  */
	enum check_states	check_state;
	enum reconstruct_states reconstruct_state;
	spinlock_t		stripe_lock;
	int			cpu;
	struct r5worker_group	*group;

	struct stripe_head	*batch_head; /* protected by stripe lock */
	spinlock_t		batch_lock; /* only header's lock is useful */
	struct list_head	batch_list; /* protected by head's batch lock*/

	struct r5l_io_unit	*log_io;
	struct list_head	log_list;
#ifdef MY_ABC_HERE
	unsigned long syno_stat_sh_start;
	unsigned long syno_stat_delay_start;
	unsigned long syno_stat_io_start;
	u64 syno_stat_delay_overhead;
	u64 syno_stat_io_overhead;

	u64 syno_stat_handle_stripe_overhead;
	u64 syno_stat_raid_run_ops_overhead;
	u64 syno_stat_bio_fill_drain_overhead;
	short syno_stat_batch_length;
	short syno_stat_is_rcw;
	short syno_stat_is_full_write;
	short syno_stat_have_been_handled;
#endif /* MY_ABC_HERE */
#ifdef MY_DEF_HERE
	short bitmap_bmc;
#endif /* MY_DEF_HERE */
#ifdef MY_ABC_HERE
	unsigned long syno_full_stripe_merge_state;
#endif /* MY_ABC_HERE */
	/**
	 * struct stripe_operations
	 * @target - STRIPE_OP_COMPUTE_BLK target
	 * @target2 - 2nd compute target in the raid6 case
	 * @zero_sum_result - P and Q verification flags
	 * @request - async service request flags for raid_run_ops
	 */
	struct stripe_operations {
		int 		     target, target2;
		enum sum_check_flags zero_sum_result;
	} ops;
	struct r5dev {
		/* rreq and rvec are used for the replacement device when
		 * writing data to both devices.
		 */
		struct bio	req, rreq;
		struct bio_vec	vec, rvec;
		struct page	*page, *orig_page;
		struct bio	*toread, *read, *towrite, *written;
		sector_t	sector;			/* sector of this page */
		unsigned long	flags;
		u32		log_checksum;
	} dev[1]; /* allocated with extra space depending of RAID geometry */
};

/* stripe_head_state - collects and tracks the dynamic state of a stripe_head
 *     for handle_stripe.
 */
struct stripe_head_state {
	/* 'syncing' means that we need to read all devices, either
	 * to check/correct parity, or to reconstruct a missing device.
	 * 'replacing' means we are replacing one or more drives and
	 * the source is valid at this point so we don't need to
	 * read all devices, just the replacement targets.
	 */
	int syncing, expanding, expanded, replacing;
	int locked, uptodate, to_read, to_write, failed, written;
	int to_fill, compute, req_compute, non_overwrite;
	int failed_num[2];
	int p_failed, q_failed;
	int dec_preread_active;
	unsigned long ops_request;

	struct bio_list return_bi;
	struct md_rdev *blocked_rdev;
	int handle_bad_blocks;
	int log_failed;
#ifdef MY_ABC_HERE
	int syno_full_stripe_merging;
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	bool syno_force_stripe_rcw;
#endif /* MY_ABC_HERE */
};

/* Flags for struct r5dev.flags */
enum r5dev_flags {
	R5_UPTODATE,	/* page contains current data */
	R5_LOCKED,	/* IO has been submitted on "req" */
	R5_DOUBLE_LOCKED,/* Cannot clear R5_LOCKED until 2 writes complete */
	R5_OVERWRITE,	/* towrite covers whole page */
/* and some that are internal to handle_stripe */
	R5_Insync,	/* rdev && rdev->in_sync at start */
	R5_Wantread,	/* want to schedule a read */
	R5_Wantwrite,
	R5_Overlap,	/* There is a pending overlapping request
			 * on this block */
	R5_ReadNoMerge, /* prevent bio from merging in block-layer */
	R5_ReadError,	/* seen a read error here recently */
	R5_ReWrite,	/* have tried to over-write the readerror */

	R5_Expanded,	/* This block now has post-expand data */
	R5_Wantcompute,	/* compute_block in progress treat as
			 * uptodate
			 */
	R5_Wantfill,	/* dev->toread contains a bio that needs
			 * filling
			 */
	R5_Wantdrain,	/* dev->towrite needs to be drained */
	R5_WantFUA,	/* Write should be FUA */
	R5_SyncIO,	/* The IO is sync */
	R5_WriteError,	/* got a write error - need to record it */
	R5_MadeGood,	/* A bad block has been fixed by writing to it */
	R5_ReadRepl,	/* Will/did read from replacement rather than orig */
	R5_MadeGoodRepl,/* A bad block on the replacement device has been
			 * fixed by writing to it */
	R5_NeedReplace,	/* This device has a replacement which is not
			 * up-to-date at this stripe. */
	R5_WantReplace, /* We need to update the replacement, we have read
			 * data in, and now is a good time to write it out.
			 */
	R5_Discard,	/* Discard the stripe */
	R5_SkipCopy,	/* Don't copy data from bio to stripe cache */
};

/*
 * Stripe state
 */
enum {
	STRIPE_ACTIVE,
	STRIPE_HANDLE,
	STRIPE_SYNC_REQUESTED,
	STRIPE_SYNCING,
	STRIPE_INSYNC,
	STRIPE_REPLACED,
	STRIPE_PREREAD_ACTIVE,
	STRIPE_DELAYED,
	STRIPE_DEGRADED,
	STRIPE_BIT_DELAY,
	STRIPE_EXPANDING,
	STRIPE_EXPAND_SOURCE,
	STRIPE_EXPAND_READY,
	STRIPE_IO_STARTED,	/* do not count towards 'bypass_count' */
	STRIPE_FULL_WRITE,	/* all blocks are set to be overwritten */
	STRIPE_BIOFILL_RUN,
	STRIPE_COMPUTE_RUN,
	STRIPE_OPS_REQ_PENDING,
	STRIPE_ON_UNPLUG_LIST,
	STRIPE_DISCARD,
	STRIPE_ON_RELEASE_LIST,
	STRIPE_BATCH_READY,
	STRIPE_BATCH_ERR,
	STRIPE_BITMAP_PENDING,	/* Being added to bitmap, don't add
				 * to batch yet.
				 */
	STRIPE_LOG_TRAPPED, /* trapped into log */
#ifdef MY_ABC_HERE
	STRIPE_NORETRY,
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	STRIPE_ACTIVATE_STABLE,
	STRIPE_CHECK_STABLE_LIST,
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	STRIPE_RECORDED,
#endif /* MY_ABC_HERE */
};

#define STRIPE_EXPAND_SYNC_FLAGS \
	((1 << STRIPE_EXPAND_SOURCE) |\
	(1 << STRIPE_EXPAND_READY) |\
	(1 << STRIPE_EXPANDING) |\
	(1 << STRIPE_SYNC_REQUESTED))
/*
 * Operation request flags
 */
enum {
	STRIPE_OP_BIOFILL,
	STRIPE_OP_COMPUTE_BLK,
	STRIPE_OP_PREXOR,
	STRIPE_OP_BIODRAIN,
	STRIPE_OP_RECONSTRUCT,
	STRIPE_OP_CHECK,
};

/*
 * RAID parity calculation preferences
 */
enum {
	PARITY_DISABLE_RMW = 0,
	PARITY_ENABLE_RMW,
	PARITY_PREFER_RMW,
};

/*
 * Pages requested from set_syndrome_sources()
 */
enum {
	SYNDROME_SRC_ALL,
	SYNDROME_SRC_WANT_DRAIN,
	SYNDROME_SRC_WRITTEN,
};
/*
 * Plugging:
 *
 * To improve write throughput, we need to delay the handling of some
 * stripes until there has been a chance that several write requests
 * for the one stripe have all been collected.
 * In particular, any write request that would require pre-reading
 * is put on a "delayed" queue until there are no stripes currently
 * in a pre-read phase.  Further, if the "delayed" queue is empty when
 * a stripe is put on it then we "plug" the queue and do not process it
 * until an unplug call is made. (the unplug_io_fn() is called).
 *
 * When preread is initiated on a stripe, we set PREREAD_ACTIVE and add
 * it to the count of prereading stripes.
 * When write is initiated, or the stripe refcnt == 0 (just in case) we
 * clear the PREREAD_ACTIVE flag and decrement the count
 * Whenever the 'handle' queue is empty and the device is not plugged, we
 * move any strips from delayed to handle and clear the DELAYED flag and set
 * PREREAD_ACTIVE.
 * In stripe_handle, if we find pre-reading is necessary, we do it if
 * PREREAD_ACTIVE is set, else we set DELAYED which will send it to the delayed queue.
 * HANDLE gets cleared if stripe_handle leaves nothing locked.
 */

struct disk_info {
	struct md_rdev	*rdev, *replacement;
};

/* NOTE NR_STRIPE_HASH_LOCKS must remain below 64.
 * This is because we sometimes take all the spinlocks
 * and creating that much locking depth can cause
 * problems.
 */
#define NR_STRIPE_HASH_LOCKS 8
#define STRIPE_HASH_LOCKS_MASK (NR_STRIPE_HASH_LOCKS - 1)

struct r5worker {
	struct work_struct work;
	struct r5worker_group *group;
	struct list_head temp_inactive_list[NR_STRIPE_HASH_LOCKS];
	bool working;
};

struct r5worker_group {
	struct list_head handle_list;
	struct r5conf *conf;
	struct r5worker *workers;
	int stripes_cnt;
};

#ifdef MY_ABC_HERE
struct syno_defer_worker_t {
	struct work_struct work;
	struct syno_defer_worker_group_t *group;
};

struct syno_defer_worker_group_t {
	struct r5conf *conf;
	struct syno_defer_worker_t *workers;
};
#endif /* MY_ABC_HERE */

#ifdef MY_ABC_HERE
enum syno_raid5_heal_stripe_head_stat {
	HEAL_STRIPE_READ_BLOCK = 0,
	HEAL_STRIPE_WANT_COMPUTE,
	HEAL_STRIPE_COMPUTING,
	HEAL_STRIPE_COMPUTE_DONE,
};

struct syno_self_heal_stripe_head {
	struct list_head           sh_list;
	unsigned long              state;
	spinlock_t                 sh_lock;
	sector_t                   sh_sector;
	int                        pd_idx;
	int                        qd_idx;
	int                        ddf_layout;
	atomic_t                   nr_pending;
	atomic_t                   nr_bio_chain;
	struct bio                 *bio_chain;
	struct r5conf              *raid_conf;
	struct syno_r5dev {
		int uptodate;
		struct page	*page;
	} dev[1]; /* allocated with extra space depending of RAID geometry */
};

struct syno_r5bio {
	struct r5conf *conf;
	struct bio *bio;
	struct syno_self_heal_stripe_head *sh;
	int disk_idx;
	sector_t sh_sector;
};
#endif /* MY_ABC_HERE */

#ifdef MY_ABC_HERE
#define SYNO_MAX_SORT_ENT_CNT 512
#define SYNO_DEFAULT_FLUSH_THRESHOLD 2048
#define SYNO_NONROT_FLUSH_THRESHOLD 64
#define SYNO_DEFAULT_FLUSH_BATCH 512
#define DEFER_GROUP_CNT_MAX 6
#define DEFER_GROUP_DISK_CNT_MAX 4

enum r5defer_flags {
	SYNO_DEFER_FLUSH_ALL,	/* flush all bio when all stripe have
				 * been handled
				 */
};

struct syno_r5pending_data {
	struct list_head sibling;
	struct bio_list bios;
	sector_t sector;
	int count;
};

struct syno_r5defer {
	struct list_head free_list;
	struct list_head pending_list;
	spinlock_t pending_bios_lock;
	unsigned long	state;
	int pending_data_cnt;
	struct bio_list pending_bios;
	struct syno_r5pending_data *pending_data;
	struct md_thread *defer_thread;
};
#endif /* MY_ABC_HERE */

struct r5conf {
	struct hlist_head	*stripe_hashtbl;
	/* only protect corresponding hash list and inactive_list */
	spinlock_t		hash_locks[NR_STRIPE_HASH_LOCKS];
	struct mddev		*mddev;
	int			chunk_sectors;
	int			level, algorithm, rmw_level;
	int			max_degraded;
	int			raid_disks;
	int			max_nr_stripes;
#ifdef MY_ABC_HERE
#else /* MY_ABC_HERE */
	int			min_nr_stripes;
#endif /* MY_ABC_HERE */

	/* reshape_progress is the leading edge of a 'reshape'
	 * It has value MaxSector when no reshape is happening
	 * If delta_disks < 0, it is the last sector we started work on,
	 * else is it the next sector to work on.
	 */
	sector_t		reshape_progress;
	/* reshape_safe is the trailing edge of a reshape.  We know that
	 * before (or after) this address, all reshape has completed.
	 */
	sector_t		reshape_safe;
	int			previous_raid_disks;
	int			prev_chunk_sectors;
	int			prev_algo;
	short			generation; /* increments with every reshape */
	seqcount_t		gen_lock;	/* lock against generation changes */
	unsigned long		reshape_checkpoint; /* Time we last updated
						     * metadata */
	long long		min_offset_diff; /* minimum difference between
						  * data_offset and
						  * new_data_offset across all
						  * devices.  May be negative,
						  * but is closest to zero.
						  */

	struct list_head	handle_list; /* stripes needing handling */
	struct list_head	hold_list; /* preread ready stripes */
	struct list_head	delayed_list; /* stripes that have plugged requests */
	struct list_head	bitmap_list; /* stripes delaying awaiting bitmap update */
#ifdef MY_ABC_HERE
	/* stripes that need stable in order to keey consistent,
	 * so we need to delay some writes but can soon be handled again
	 */
	struct list_head    stable_list;
#endif /* MY_ABC_HERE */
	struct bio		*retry_read_aligned; /* currently retrying aligned bios   */
	struct bio		*retry_read_aligned_list; /* aligned bios retry list  */
	atomic_t		preread_active_stripes; /* stripes with scheduled io */
	atomic_t		active_aligned_reads;
	atomic_t		pending_full_writes; /* full write backlog */
	int			bypass_count; /* bypassed prereads */
	int			bypass_threshold; /* preread nice */
	int			skip_copy; /* Don't copy data from bio to stripe cache */
#ifdef MY_ABC_HERE
	int         stripe_cache_memory_usage;
#endif /* MY_ABC_HERE */
	struct list_head	*last_hold; /* detect hold_list promotions */

	/* bios to have bi_end_io called after metadata is synced */
	struct bio_list		return_bi;

	atomic_t		reshape_stripes; /* stripes with pending writes for reshape */
	/* unfortunately we need two cache names as we temporarily have
	 * two caches.
	 */
	int			active_name;
	char			cache_name[2][32];
	struct kmem_cache	*slab_cache; /* for allocating stripes */
	struct mutex		cache_size_mutex; /* Protect changes to cache size */
#ifdef MY_ABC_HERE
	int               syno_self_heal_sh_size;
	wait_queue_head_t syno_self_heal_wait_for_sh;
	spinlock_t        syno_self_heal_sh_handle_list_lock;
	spinlock_t        syno_self_heal_sh_free_list_lock;
	spinlock_t        syno_self_heal_master_bio_lock;
	spinlock_t        syno_self_heal_master_bio_list_lock;
	struct list_head  syno_self_heal_sh_handle_list; /* in processing sh */
	struct list_head  syno_self_heal_sh_free_list; /* free sh */
	struct bio        *syno_self_heal_master_bio_list;
	struct kmem_cache *syno_self_heal_slab_sh_cache;
#endif /* MY_ABC_HERE */

	int			seq_flush, seq_write;
	int			quiesce;

	int			fullsync;  /* set to 1 if a full sync is needed,
					    * (fresh device added).
					    * Cleared when a sync completes.
					    */
	int			recovery_disabled;
	/* per cpu variables */
	struct raid5_percpu {
		struct page	*spare_page; /* Used when checking P/Q in raid6 */
		struct flex_array *scribble;   /* space for constructing buffer
					      * lists and performing address
					      * conversions
					      */
	} __percpu *percpu;
	int scribble_disks;
	int scribble_sectors;
#ifdef CONFIG_HOTPLUG_CPU
	struct notifier_block	cpu_notify;
#endif
#ifdef MY_ABC_HERE
	atomic_t            proxy_enable;
	struct md_thread   *proxy_thread;
#endif /* MY_ABC_HERE */
	/*
	 * Free stripes pool
	 */
	atomic_t		active_stripes;
	struct list_head	inactive_list[NR_STRIPE_HASH_LOCKS];
	atomic_t		empty_inactive_list_nr;
	struct llist_head	released_stripes;
	wait_queue_head_t	wait_for_quiescent;
	wait_queue_head_t	wait_for_stripe;
	wait_queue_head_t	wait_for_overlap;
	unsigned long		cache_state;
#define R5_INACTIVE_BLOCKED	1	/* release of inactive stripes blocked,
					 * waiting for 25% to be free
					 */
#ifdef MY_ABC_HERE
#else /* MY_ABC_HERE */
#define R5_ALLOC_MORE		2	/* It might help to allocate another
					 * stripe.
					 */
#define R5_DID_ALLOC		4	/* A stripe was allocated, don't allocate
					 * more until at least one has been
					 * released.  This avoids flooding
					 * the cache.
					 */
	struct shrinker		shrinker;
#endif /* MY_ABC_HERE */
	int			pool_size; /* number of disks in stripeheads in pool */
	spinlock_t		device_lock;
	struct disk_info	*disks;
	struct bio_set		*bio_split;

	/* When taking over an array from a different personality, we store
	 * the new thread here until we fully activate the array.
	 */
	struct md_thread	*thread;
	struct list_head	temp_inactive_list[NR_STRIPE_HASH_LOCKS];
	struct r5worker_group	*worker_groups;
	int			group_cnt;
	int			worker_cnt_per_group;

#ifdef MY_ABC_HERE
	int syno_defer_flush_threshold;
	int syno_defer_mode;
	int syno_defer_group_cnt;
	struct syno_r5defer *syno_defer_groups;

	struct syno_defer_worker_group_t *syno_defer_worker_groups;
	int syno_defer_worker_cnt_per_group;
	int syno_defer_group_disk_cnt_max;
	int syno_defer_flush_batch_size;
	atomic_t syno_active_stripe_workers;
	bool syno_defer_skip_sort;
#endif /* MY_ABC_HERE */

#ifdef MY_ABC_HERE
	u64 syno_stat_sh_overhead;	/* ticks */
	u64 syno_stat_delay_overhead;	/* ticks */
	u64 syno_stat_io_overhead;	/* ticks */
	u64 syno_stat_sh_max_overhead;	/* ticks */
	u64 syno_stat_delay_max_overhead;
	u64 syno_stat_io_max_overhead;
	/*
	 * record overheads of main functions in raid5
	 */
	u64 syno_stat_handle_stripe_overhead;	/* nanosecond */
	u64 syno_stat_raid_run_ops_overhead;	/* nanosecond */
	u64 syno_stat_bio_fill_drain_overhead;	/* nanosceond */
	unsigned long long syno_stat_recorded_stripe_cnt;
	u64 syno_stat_handle_stripe_max_overhead;
	u64 syno_stat_raid_run_ops_max_overhead;
	u64 syno_stat_bio_fill_drain_max_overhead;
	u64 syno_stat_other_raid_ops_max_overhead;

	int syno_stat_enable_record_time;

	unsigned long long syno_stat_total_stripe_cnt;
	unsigned long long syno_stat_handle_stripe_cnt;	/* batched stripes will be consider as one stripe */
	unsigned long long syno_stat_full_write_stripe_cnt;
	unsigned long long syno_stat_rmw_cnt, syno_stat_rcw_cnt;
	unsigned long long syno_stat_raid5d_handle_cnt, syno_stat_raid5d_proxy_handle_cnt, syno_stat_r5worker_handle_cnt;
#endif /* MY_ABC_HERE */

#ifdef MY_ABC_HERE
	int syno_flush_plug_stripe_cnt;
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	int syno_active_stripe_threshold;
#endif /* MY_ABC_HERE */
#ifdef MY_DEF_HERE
	int syno_handle_stripes_cpu;
#endif /* MY_DEF_HERE */

	struct r5l_log		*log;
#ifdef MY_ABC_HERE
	int syno_dummy_read;
	struct bio *dummy_bio;
	struct page *dummy_page;
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	bool syno_full_stripe_merge;
#endif /* MY_ABC_HERE */

};


/*
 * Our supported algorithms
 */
#define ALGORITHM_LEFT_ASYMMETRIC	0 /* Rotating Parity N with Data Restart */
#define ALGORITHM_RIGHT_ASYMMETRIC	1 /* Rotating Parity 0 with Data Restart */
#define ALGORITHM_LEFT_SYMMETRIC	2 /* Rotating Parity N with Data Continuation */
#define ALGORITHM_RIGHT_SYMMETRIC	3 /* Rotating Parity 0 with Data Continuation */

/* Define non-rotating (raid4) algorithms.  These allow
 * conversion of raid4 to raid5.
 */
#define ALGORITHM_PARITY_0		4 /* P or P,Q are initial devices */
#define ALGORITHM_PARITY_N		5 /* P or P,Q are final devices. */

/* DDF RAID6 layouts differ from md/raid6 layouts in two ways.
 * Firstly, the exact positioning of the parity block is slightly
 * different between the 'LEFT_*' modes of md and the "_N_*" modes
 * of DDF.
 * Secondly, or order of datablocks over which the Q syndrome is computed
 * is different.
 * Consequently we have different layouts for DDF/raid6 than md/raid6.
 * These layouts are from the DDFv1.2 spec.
 * Interestingly DDFv1.2-Errata-A does not specify N_CONTINUE but
 * leaves RLQ=3 as 'Vendor Specific'
 */

#define ALGORITHM_ROTATING_ZERO_RESTART	8 /* DDF PRL=6 RLQ=1 */
#define ALGORITHM_ROTATING_N_RESTART	9 /* DDF PRL=6 RLQ=2 */
#define ALGORITHM_ROTATING_N_CONTINUE	10 /*DDF PRL=6 RLQ=3 */

/* For every RAID5 algorithm we define a RAID6 algorithm
 * with exactly the same layout for data and parity, and
 * with the Q block always on the last device (N-1).
 * This allows trivial conversion from RAID5 to RAID6
 */
#define ALGORITHM_LEFT_ASYMMETRIC_6	16
#define ALGORITHM_RIGHT_ASYMMETRIC_6	17
#define ALGORITHM_LEFT_SYMMETRIC_6	18
#define ALGORITHM_RIGHT_SYMMETRIC_6	19
#define ALGORITHM_PARITY_0_6		20
#define ALGORITHM_PARITY_N_6		ALGORITHM_PARITY_N

#ifdef MY_ABC_HERE
/* For Synology RAID F1, define new layout as follow */
#define ALGORITHM_RAID_F1_0		ALGORITHM_LEFT_SYMMETRIC
#define ALGORITHM_RAID_F1_1		32
#define ALGORITHM_RAID_F1_2		33
#define ALGORITHM_RAID_F1_3		34
#define ALGORITHM_RAID_F1_4		35

#define ALGORITHM_RAID_F1			ALGORITHM_RAID_F1_1

static inline int algorithm_valid_raid_f1(int layout)
{
	return layout == ALGORITHM_RAID_F1_0 ||
		((layout >= ALGORITHM_RAID_F1_1) &&
		(layout <= ALGORITHM_RAID_F1_4));
}
#endif /* MY_ABC_HERE */
static inline int algorithm_valid_raid5(int layout)
{
	return (layout >= 0) &&
		(layout <= 5);
}
static inline int algorithm_valid_raid6(int layout)
{
	return (layout >= 0 && layout <= 5)
		||
		(layout >= 8 && layout <= 10)
		||
		(layout >= 16 && layout <= 20);
}

static inline int algorithm_is_DDF(int layout)
{
	return layout >= 8 && layout <= 10;
}

extern void md_raid5_kick_device(struct r5conf *conf);
extern int raid5_set_cache_size(struct mddev *mddev, int size);
extern sector_t raid5_compute_blocknr(struct stripe_head *sh, int i, int previous);
extern void raid5_release_stripe(struct stripe_head *sh);
extern sector_t raid5_compute_sector(struct r5conf *conf, sector_t r_sector,
				     int previous, int *dd_idx,
				     struct stripe_head *sh);
extern struct stripe_head *
raid5_get_active_stripe(struct r5conf *conf, sector_t sector,
			int previous, int noblock, int noquiesce);
extern int r5l_init_log(struct r5conf *conf, struct md_rdev *rdev);
extern void r5l_exit_log(struct r5l_log *log);
extern int r5l_write_stripe(struct r5l_log *log, struct stripe_head *head_sh);
extern void r5l_write_stripe_run(struct r5l_log *log);
extern void r5l_flush_stripe_to_raid(struct r5l_log *log);
extern void r5l_stripe_write_finished(struct stripe_head *sh);
extern int r5l_handle_flush_request(struct r5l_log *log, struct bio *bio);
extern void r5l_quiesce(struct r5l_log *log, int state);
extern bool r5l_log_disk_error(struct r5conf *conf);

#ifdef MY_ABC_HERE
#define sector_mod(a,b) sector_div(a,b)
#endif /* MY_ABC_HERE */

#ifdef MY_ABC_HERE
static inline int md_raid_diff_uneven_count(int algorithm)
{
	return (algorithm == ALGORITHM_RAID_F1_0? 0: algorithm - ALGORITHM_RAID_F1_1 + 1);
}

#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
#define SYNO_FULL_STRIPE_MERGE_DENOMINATOR 16
/*
 * Full stripe merge state
 */
enum {
	SYNO_FULL_STRIPE_MERGE,
	SYNO_FULL_STRIPE_MERGING,
	SYNO_FULL_STRIPE_MERGE_DO_WRITE,
};
#endif /* MY_ABC_HERE */
#endif
