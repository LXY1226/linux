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

#ifndef __BTRFS_ORDERED_DATA__
#define __BTRFS_ORDERED_DATA__

/* one of these per inode */
struct btrfs_ordered_inode_tree {
	spinlock_t lock;
	struct rb_root tree;
	struct rb_node *last;
};

struct btrfs_ordered_sum {
	/* bytenr is the start of this extent on disk */
	u64 bytenr;

	/*
	 * this is the length in bytes covered by the sums array below.
	 */
	int len;
	struct list_head list;
	/* last field is a variable length array of csums */
	u32 sums[];
};

/*
 * bits for the flags field:
 *
 * BTRFS_ORDERED_IO_DONE is set when all of the blocks are written.
 * It is used to make sure metadata is inserted into the tree only once
 * per extent.
 *
 * BTRFS_ORDERED_COMPLETE is set when the extent is removed from the
 * rbtree, just before waking any waiters.  It is used to indicate the
 * IO is done and any metadata is inserted into the tree.
 */
#define BTRFS_ORDERED_IO_DONE 0 /* set when all the pages are written */

#define BTRFS_ORDERED_COMPLETE 1 /* set when removed from the tree */

#define BTRFS_ORDERED_NOCOW 2 /* set when we want to write in place */

#define BTRFS_ORDERED_COMPRESSED 3 /* writing a zlib compressed extent */

#define BTRFS_ORDERED_PREALLOC 4 /* set when writing to preallocated extent */

#define BTRFS_ORDERED_DIRECT 5 /* set when we're doing DIO with this extent */

#define BTRFS_ORDERED_IOERR 6 /* We had an io error when writing this out */

#define BTRFS_ORDERED_UPDATED_ISIZE 7 /* indicates whether this ordered extent
				       * has done its due diligence in updating
				       * the isize. */
#define BTRFS_ORDERED_TRUNCATED 8 /* Set when we have to truncate an extent */

#ifdef MY_DEF_HERE
#define BTRFS_ORDERED_SYNC 12
#endif /* MY_DEF_HERE */

#ifdef MY_DEF_HERE
#define BTRFS_ORDERED_WORK_INITIALIZED 13
#define BTRFS_ORDERED_HIGH_PRIORITY 14
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
#define BTRFS_ORDERED_INLINE_DEDUPE 15
#endif /* MY_DEF_HERE */

struct btrfs_ordered_extent {
	/* logical offset in the file */
	u64 file_offset;

	/* disk byte number */
	u64 start;

	/* ram length of the extent in bytes */
	u64 len;

	/* extent length on disk */
	u64 disk_len;

	/* number of bytes that still need writing */
	u64 bytes_left;

	/*
	 * the end of the ordered extent which is behind it but
	 * didn't update disk_i_size. Please see the comment of
	 * btrfs_ordered_update_i_size();
	 */
	u64 outstanding_isize;

	/*
	 * If we get truncated we need to adjust the file extent we enter for
	 * this ordered extent so that we do not expose stale data.
	 */
	u64 truncated_len;

	/* flags (described above) */
	unsigned long flags;

	/* compression algorithm */
	int compress_type;

	/* Qgroup reserved space */
	int qgroup_rsv;

	/* reference count */
	atomic_t refs;

	/* the inode we belong to */
	struct inode *inode;

	/* list of checksums for insertion when the extent io is done */
	struct list_head list;

	/* If we need to wait on this to be done */
	struct list_head log_list;

	/* If the transaction needs to wait on this ordered extent */
	struct list_head trans_list;

	/* used to wait for the BTRFS_ORDERED_COMPLETE bit */
	wait_queue_head_t wait;

	/* our friendly rbtree entry */
	struct rb_node rb_node;

	/* a per root list of all the pending ordered extents */
	struct list_head root_extent_list;

	struct btrfs_work work;

	struct completion completion;
	struct btrfs_work flush_work;
	struct list_head work_list;
#ifdef MY_DEF_HERE
	int high_priority;
#endif /* MY_DEF_HERE */
};

/*
 * calculates the total size you need to allocate for an ordered sum
 * structure spanning 'bytes' in the file
 */
static inline int btrfs_ordered_sum_size(struct btrfs_root *root,
					 unsigned long bytes)
{
	int num_sectors = (int)DIV_ROUND_UP(bytes, root->sectorsize);
	return sizeof(struct btrfs_ordered_sum) + num_sectors * sizeof(u32);
}

static inline void
btrfs_ordered_inode_tree_init(struct btrfs_ordered_inode_tree *t)
{
	spin_lock_init(&t->lock);
	t->tree = RB_ROOT;
	t->last = NULL;
}

void btrfs_put_ordered_extent(struct btrfs_ordered_extent *entry);
void btrfs_remove_ordered_extent(struct inode *inode,
				struct btrfs_ordered_extent *entry);
int btrfs_dec_test_ordered_pending(struct inode *inode,
				   struct btrfs_ordered_extent **cached,
				   u64 file_offset, u64 io_size, int uptodate);
int btrfs_dec_test_first_ordered_pending(struct inode *inode,
				   struct btrfs_ordered_extent **cached,
				   u64 *file_offset, u64 io_size,
				   int uptodate);
#ifdef MY_DEF_HERE
int btrfs_add_ordered_extent(struct inode *inode, u64 file_offset,
			     u64 start, u64 len, u64 disk_len, int type, int write_sync);
#else
int btrfs_add_ordered_extent(struct inode *inode, u64 file_offset,
			     u64 start, u64 len, u64 disk_len, int type);
#endif /* MY_DEF_HERE */
int btrfs_add_ordered_extent_dio(struct inode *inode, u64 file_offset,
				 u64 start, u64 len, u64 disk_len, int type);
int btrfs_add_ordered_extent_compress(struct inode *inode, u64 file_offset,
				      u64 start, u64 len, u64 disk_len,
				      int type, int compress_type);
void btrfs_add_ordered_sum(struct inode *inode,
			   struct btrfs_ordered_extent *entry,
			   struct btrfs_ordered_sum *sum);
struct btrfs_ordered_extent *btrfs_lookup_ordered_extent(struct inode *inode,
							 u64 file_offset);
void btrfs_start_ordered_extent(struct inode *inode,
				struct btrfs_ordered_extent *entry, int wait);
int btrfs_wait_ordered_range(struct inode *inode, u64 start, u64 len);
struct btrfs_ordered_extent *
btrfs_lookup_first_ordered_extent(struct inode * inode, u64 file_offset);
struct btrfs_ordered_extent *btrfs_lookup_ordered_range(struct inode *inode,
							u64 file_offset,
							u64 len);
int btrfs_ordered_update_i_size(struct inode *inode, u64 offset,
				struct btrfs_ordered_extent *ordered);
int btrfs_find_ordered_sum(struct inode *inode, u64 offset, u64 disk_bytenr,
			   u32 *sum, int len);
int btrfs_wait_ordered_extents(struct btrfs_root *root, int nr,
			       const u64 range_start, const u64 range_len);
void btrfs_wait_ordered_roots(struct btrfs_fs_info *fs_info, int nr,
			      const u64 range_start, const u64 range_len);
int __init ordered_data_init(void);
void ordered_data_exit(void);
#endif
