#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
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

#ifndef __BTRFS_I__
#define __BTRFS_I__

#include <linux/hash.h>
#include "extent_map.h"
#include "extent_io.h"
#include "ordered-data.h"
#include "delayed-inode.h"

/*
 * ordered_data_close is set by truncate when a file that used
 * to have good data has been truncated to zero.  When it is set
 * the btrfs file release call will add this inode to the
 * ordered operations list so that we make sure to flush out any
 * new data the application may have written before commit.
 */
#define BTRFS_INODE_ORDERED_DATA_CLOSE		0
#define BTRFS_INODE_DUMMY			2
#define BTRFS_INODE_IN_DEFRAG			3
#define BTRFS_INODE_DELALLOC_META_RESERVED	4
#define BTRFS_INODE_HAS_ASYNC_EXTENT		6
#define BTRFS_INODE_NEEDS_FULL_SYNC		7
#define BTRFS_INODE_COPY_EVERYTHING		8
#define BTRFS_INODE_IN_DELALLOC_LIST		9
#define BTRFS_INODE_READDIO_NEED_LOCK		10
#define BTRFS_INODE_HAS_PROPS		        11
/*
 * The following 3 bits are meant only for the btree inode.
 * When any of them is set, it means an error happened while writing an
 * extent buffer belonging to:
 * 1) a non-log btree
 * 2) a log btree and first log sub-transaction
 * 3) a log btree and second log sub-transaction
 */
#define BTRFS_INODE_BTREE_ERR		        12
#define BTRFS_INODE_BTREE_LOG1_ERR		13
#define BTRFS_INODE_BTREE_LOG2_ERR		14
#ifdef MY_DEF_HERE
#define BTRFS_INODE_SNAP_FLUSH			15
#endif /* MY_DEF_HERE */

#ifdef MY_DEF_HERE
/* these two bits are mutually exclusive */
#define BTRFS_INODE_LOCKER_NOLOCK		27
#define BTRFS_INODE_LOCKER_LOCKABLE		28
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
#define BTRFS_INODE_USRQUOTA_META_RESERVED	29
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
#define BTRFS_INODE_SYNO_WRITEBACK_LRU_LIST	30
#define BTRFS_INODE_SYNO_WRITEBACK_RUNNING	31
#endif /* MY_DEF_HERE */

/* in memory btrfs inode */
struct btrfs_inode {
	/* which subvolume this inode belongs to */
	struct btrfs_root *root;

	/* key used to find this inode on disk.  This is used by the code
	 * to read in roots of subvolumes
	 */
	struct btrfs_key location;

	/*
	 * Lock for counters and all fields used to determine if the inode is in
	 * the log or not (last_trans, last_sub_trans, last_log_commit,
	 * logged_trans), to access/update new_delalloc_bytes and to update the
	 * VFS' inode number of bytes used.
	 */
	spinlock_t lock;

	/* the extent_tree has caches of all the extent mappings to disk */
	struct extent_map_tree extent_tree;

	/* the io_tree does range state (DIRTY, LOCKED etc) */
	struct extent_io_tree io_tree;

	/* special utility tree used to record which mirrors have already been
	 * tried when checksums fail for a given block
	 */
	struct extent_io_tree io_failure_tree;

	/* held while logging the inode in tree-log.c */
	struct mutex log_mutex;

	/* held while doing delalloc reservations */
	struct mutex delalloc_mutex;

	/* used to order data wrt metadata */
	struct btrfs_ordered_inode_tree ordered_tree;

	/* list of all the delalloc inodes in the FS.  There are times we need
	 * to write all the delalloc pages to disk, and this list is used
	 * to walk them all.
	 */
	struct list_head delalloc_inodes;

#ifdef MY_DEF_HERE
	/*
	 * likely delalloc_inodes, for async flush
	 * 1. data reclaim
	 * 2. avoid deadlock
	 */
	struct list_head syno_delalloc_inodes;
#endif /* MY_DEF_HERE */

#ifdef MY_DEF_HERE
	struct list_head syno_dirty_lru_inode;
#endif /* MY_DEF_HERE */

	/* node for the red-black tree that links inodes in subvolume root */
	struct rb_node rb_node;

	unsigned long runtime_flags;

	/* Keep track of who's O_SYNC/fsyncing currently */
	atomic_t sync_writers;

	/* full 64 bit generation number, struct vfs_inode doesn't have a big
	 * enough field for this.
	 */
	u64 generation;

	/*
	 * transid of the trans_handle that last modified this inode
	 */
	u64 last_trans;

	/*
	 * transid that last logged this inode
	 */
	u64 logged_trans;

	/*
	 * log transid when this inode was last modified
	 */
	int last_sub_trans;

	/* a local copy of root's last_log_commit */
	int last_log_commit;

	/* total number of bytes pending delalloc, used by stat to calc the
	 * real block usage of the file
	 */
	u64 delalloc_bytes;

	/*
	 * Total number of bytes pending delalloc that fall within a file
	 * range that is either a hole or beyond EOF (and no prealloc extent
	 * exists in the range). This is always <= delalloc_bytes.
	 */
	u64 new_delalloc_bytes;

	/*
	 * total number of bytes pending defrag, used by stat to check whether
	 * it needs COW.
	 */
	u64 defrag_bytes;

	/*
	 * the size of the file stored in the metadata on disk.  data=ordered
	 * means the in-memory i_size might be larger than the size on disk
	 * because not all the blocks are written yet.
	 */
	u64 disk_i_size;

	/*
	 * if this is a directory then index_cnt is the counter for the index
	 * number for new files that are created
	 */
	u64 index_cnt;

	/* Cache the directory index number to speed the dir/file remove */
	u64 dir_index;

	/* the fsync log has some corner cases that mean we have to check
	 * directories to see if any unlinks have been done before
	 * the directory was logged.  See tree-log.c for all the
	 * details
	 */
	u64 last_unlink_trans;

	/*
	 * The id/generation of the last transaction where this inode was
	 * either the source or the destination of a clone/dedupe operation.
	 * Used when logging an inode to know if there are shared extents that
	 * need special care when logging checksum items, to avoid duplicate
	 * checksum items in a log (which can lead to a corruption where we end
	 * up with missing checksum ranges after log replay).
	 * Protected by the vfs inode lock.
	 */
	u64 last_reflink_trans;

	/*
	 * Number of bytes outstanding that are going to need csums.  This is
	 * used in ENOSPC accounting.
	 */
	u64 csum_bytes;

	/* flags field from the on disk inode */
	u32 flags;

	/*
	 * Counters to keep track of the number of extent item's we may use due
	 * to delalloc and such.  outstanding_extents is the number of extent
	 * items we think we'll end up using, and reserved_extents is the number
	 * of extent items we've reserved metadata for.
	 */
	unsigned outstanding_extents;
	unsigned reserved_extents;

	/*
	 * always compress this one file
	 */
	unsigned force_compress;

	struct btrfs_delayed_node *delayed_node;

	/* File creation time. */
	struct timespec i_otime;

	/* Hook into fs_info->delayed_iputs */
	struct list_head delayed_iput;
	long delayed_iput_count;

	/*
	 * To avoid races between lockless (i_mutex not held) direct IO writes
	 * and concurrent fsync requests. Direct IO writes must acquire read
	 * access on this semaphore for creating an extent map and its
	 * corresponding ordered extent. The fast fsync path must acquire write
	 * access on this semaphore before it collects ordered extents and
	 * extent maps.
	 */
	struct rw_semaphore dio_sem;

	struct inode vfs_inode;

#ifdef MY_DEF_HERE
	union {
		enum locker_state __locker_state;
		const enum locker_state locker_state;
	};
	union {
		time64_t __locker_update_time;          // in volume clock
		const time64_t locker_update_time;
	};
	union {
		time64_t __locker_period_begin;         // in volume clock
		const time64_t locker_period_begin;
	};
	union {
		time64_t __locker_period_end;           // in volume clock
		const time64_t locker_period_end;
	};
	bool locker_dirty;
	spinlock_t locker_lock;
#endif /* MY_DEF_HERE */

#ifdef MY_DEF_HERE
	struct list_head free_extent_map_inode;
	atomic_t free_extent_map_counts;
#endif /* MY_DEF_HERE */

#ifdef MY_DEF_HERE
	atomic_t syno_uq_refs;
	u64 syno_uq_rfer_used;
	u64 syno_uq_reserved;
#endif /* MY_DEF_HERE */

#ifdef MY_DEF_HERE
	struct list_head syno_rbd_meta_file;
#endif /* MY_DEF_HERE */

#ifdef MY_DEF_HERE
	// For chown.
	u64 uq_reserved;
#endif /* MY_DEF_HERE */
};

extern unsigned char btrfs_filetype_table[];

static inline struct btrfs_inode *BTRFS_I(struct inode *inode)
{
	return container_of(inode, struct btrfs_inode, vfs_inode);
}

static inline unsigned long btrfs_inode_hash(u64 objectid,
					     const struct btrfs_root *root)
{
	u64 h = objectid ^ (root->objectid * GOLDEN_RATIO_PRIME);

#if BITS_PER_LONG == 32
	h = (h >> 32) ^ (h & 0xffffffff);
#endif

	return (unsigned long)h;
}

static inline void btrfs_insert_inode_hash(struct inode *inode)
{
	unsigned long h = btrfs_inode_hash(inode->i_ino, BTRFS_I(inode)->root);

	__insert_inode_hash(inode, h);
}

static inline u64 btrfs_ino(struct inode *inode)
{
	u64 ino = BTRFS_I(inode)->location.objectid;

	/*
	 * !ino: btree_inode
	 * type == BTRFS_ROOT_ITEM_KEY: subvol dir
	 */
	if (!ino || BTRFS_I(inode)->location.type == BTRFS_ROOT_ITEM_KEY)
		ino = inode->i_ino;
	return ino;
}

static inline void btrfs_i_size_write(struct inode *inode, u64 size)
{
	i_size_write(inode, size);
	BTRFS_I(inode)->disk_i_size = size;
}

static inline bool btrfs_is_free_space_inode(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;

	if (root == root->fs_info->tree_root &&
	    btrfs_ino(inode) != BTRFS_BTREE_INODE_OBJECTID)
		return true;
	if (BTRFS_I(inode)->location.objectid == BTRFS_FREE_INO_OBJECTID)
		return true;
	return false;
}

static inline int btrfs_inode_in_log(struct inode *inode, u64 generation)
{
	int ret = 0;

	spin_lock(&BTRFS_I(inode)->lock);
	if (BTRFS_I(inode)->logged_trans == generation &&
	    BTRFS_I(inode)->last_sub_trans <=
	    BTRFS_I(inode)->last_log_commit &&
	    BTRFS_I(inode)->last_sub_trans <=
	    BTRFS_I(inode)->root->last_log_commit) {
		/*
		 * After a ranged fsync we might have left some extent maps
		 * (that fall outside the fsync's range). So return false
		 * here if the list isn't empty, to make sure btrfs_log_inode()
		 * will be called and process those extent maps.
		 */
		smp_mb();
		if (list_empty(&BTRFS_I(inode)->extent_tree.modified_extents))
			ret = 1;
	}
	spin_unlock(&BTRFS_I(inode)->lock);
	return ret;
}

#define BTRFS_DIO_ORIG_BIO_SUBMITTED	0x1

/*
 * Check if the inode has flags compatible with compression
 */
static inline bool btrfs_inode_can_compress(const struct btrfs_inode *inode)
{
	if (inode->flags & BTRFS_INODE_NODATACOW ||
	    inode->flags & BTRFS_INODE_NODATASUM)
		return false;
	return true;
}

struct btrfs_dio_private {
	struct inode *inode;
	unsigned long flags;
	u64 logical_offset;
	u64 disk_bytenr;
	u64 bytes;
	void *private;

	/* number of bios pending for this dio */
	atomic_t pending_bios;

	/* IO errors */
	int errors;

	/* orig_bio is our btrfs_io_bio */
	struct bio *orig_bio;

	/* dio_bio came from fs/direct-io.c */
	struct bio *dio_bio;

	/*
	 * The original bio may be split to several sub-bios, this is
	 * done during endio of sub-bios
	 */
	int (*subio_endio)(struct inode *, struct btrfs_io_bio *, int);
};

/*
 * Disable DIO read nolock optimization, so new dio readers will be forced
 * to grab i_mutex. It is used to avoid the endless truncate due to
 * nonlocked dio read.
 */
static inline void btrfs_inode_block_unlocked_dio(struct inode *inode)
{
	set_bit(BTRFS_INODE_READDIO_NEED_LOCK, &BTRFS_I(inode)->runtime_flags);
	smp_mb();
}

static inline void btrfs_inode_resume_unlocked_dio(struct inode *inode)
{
	smp_mb__before_atomic();
	clear_bit(BTRFS_INODE_READDIO_NEED_LOCK,
		  &BTRFS_I(inode)->runtime_flags);
}

static inline bool btrfs_page_exists_in_range(struct inode *inode,
						loff_t start, loff_t end)
{
	return filemap_range_has_page(inode->i_mapping, start, end);
}

#ifdef MY_DEF_HERE
static inline bool btrfs_usrquota_fast_chown_enable(struct inode *inode)
{
	if (!inode || !BTRFS_I(inode)->root || !BTRFS_I(inode)->root->fs_info)
		return false;
#ifdef MY_DEF_HERE
	if (btrfs_root_disable_quota(BTRFS_I(inode)->root))
		return false;
#endif /* MY_DEF_HERE */
	if (!BTRFS_I(inode)->root->fs_info->syno_usrquota_v1_enabled)
		return false;
	if (!btrfs_usrquota_compat_inode_quota(BTRFS_I(inode)->root->fs_info))
		return false;
	if (!(BTRFS_I(inode)->flags & BTRFS_INODE_UQ_REF_USED))
		return false;
	return true;
}
static inline struct inode *syno_usrquota_inode_get(struct inode *inode)
{
	if (!btrfs_usrquota_fast_chown_enable(inode))
		return NULL;
	inode = igrab(inode);
	if (inode)
		atomic_inc(&BTRFS_I(inode)->syno_uq_refs);
	return inode;
}
static inline void syno_usrquota_inode_put(struct inode *inode)
{
	u64 to_free = 0;
	if (!btrfs_usrquota_fast_chown_enable(inode))
		return;
	WARN_ON(atomic_read(&BTRFS_I(inode)->syno_uq_refs) == 0);
	if (atomic_dec_and_test(&BTRFS_I(inode)->syno_uq_refs)) {
		spin_lock(&BTRFS_I(inode)->lock);
		if (test_and_clear_bit(BTRFS_INODE_USRQUOTA_META_RESERVED,
			       &BTRFS_I(inode)->runtime_flags)) {
			to_free = btrfs_calc_trans_metadata_size(BTRFS_I(inode)->root, 1);
		}
		spin_unlock(&BTRFS_I(inode)->lock);
		if (to_free)
			btrfs_block_rsv_release(BTRFS_I(inode)->root, &BTRFS_I(inode)->root->fs_info->delalloc_block_rsv, to_free);
	}
	btrfs_add_delayed_iput(inode);
}
#endif /* MY_DEF_HERE */

#endif
