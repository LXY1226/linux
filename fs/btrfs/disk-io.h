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

#ifndef __DISKIO__
#define __DISKIO__

#define BTRFS_SUPER_INFO_OFFSET SZ_64K
#define BTRFS_SUPER_INFO_SIZE 4096

#define BTRFS_SUPER_MIRROR_MAX	 3
#define BTRFS_SUPER_MIRROR_SHIFT 12

enum btrfs_wq_endio_type {
	BTRFS_WQ_ENDIO_DATA = 0,
	BTRFS_WQ_ENDIO_METADATA = 1,
	BTRFS_WQ_ENDIO_FREE_SPACE = 2,
	BTRFS_WQ_ENDIO_RAID56 = 3,
	BTRFS_WQ_ENDIO_DIO_REPAIR = 4,
};

static inline u64 btrfs_sb_offset(int mirror)
{
	u64 start = SZ_16K;
	if (mirror)
		return start << (BTRFS_SUPER_MIRROR_SHIFT * mirror);
	return BTRFS_SUPER_INFO_OFFSET;
}

#if defined(MY_DEF_HERE) || defined(MY_DEF_HERE)
struct btrfs_new_fs_root_args {
#ifdef MY_DEF_HERE
	/* Preallocated syno delalloc bytes */
	struct percpu_counter *syno_delalloc_bytes;
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
	 struct percpu_counter *eb_hit;
	 struct percpu_counter *eb_miss;
#endif /* MY_DEF_HERE */
};
#endif /* MY_DEF_HERE || MY_DEF_HERE */

struct btrfs_device;
struct btrfs_fs_devices;

int btrfs_verify_level_key(struct btrfs_fs_info *fs_info,
			   struct extent_buffer *eb, int level,
			   struct btrfs_key *first_key, u64 parent_transid);
struct extent_buffer *read_tree_block(struct btrfs_root *root, u64 bytenr,
				      u64 parent_transid, int level,
				      struct btrfs_key *first_key);
void readahead_tree_block(struct btrfs_root *root, u64 bytenr);
int reada_tree_block_flagged(struct btrfs_root *root, u64 bytenr,
			 int mirror_num, struct extent_buffer **eb);
struct extent_buffer *btrfs_find_create_tree_block(struct btrfs_root *root,
						   u64 bytenr);
void clean_tree_block(struct btrfs_trans_handle *trans,
		      struct btrfs_fs_info *fs_info, struct extent_buffer *buf);
int open_ctree(struct super_block *sb,
	       struct btrfs_fs_devices *fs_devices,
	       char *options);
void close_ctree(struct btrfs_root *root);
int write_ctree_super(struct btrfs_trans_handle *trans,
		      struct btrfs_root *root, int max_mirrors);
struct buffer_head *btrfs_read_dev_super(struct block_device *bdev);
int btrfs_read_dev_one_super(struct block_device *bdev, int copy_num,
			struct buffer_head **bh_ret);
int btrfs_commit_super(struct btrfs_root *root);
#ifdef MY_DEF_HERE
struct extent_buffer *btrfs_find_tree_block(struct btrfs_root *root,
					    u64 bytenr);
#else
struct extent_buffer *btrfs_find_tree_block(struct btrfs_fs_info *fs_info,
					    u64 bytenr);
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
struct btrfs_root *btrfs_read_tree_root(struct btrfs_root *tree_root,
					struct btrfs_key *key);
#endif /* MY_DEF_HERE */
struct btrfs_root *btrfs_read_fs_root(struct btrfs_root *tree_root,
				      struct btrfs_key *location);
#if defined(MY_DEF_HERE) || defined(MY_DEF_HERE)
void btrfs_free_new_fs_root_args(struct btrfs_new_fs_root_args *args);
struct btrfs_new_fs_root_args *btrfs_alloc_new_fs_root_args(void);
#endif /* MY_DEF_HERE || MY_DEF_HERE */
int btrfs_init_fs_root(struct btrfs_root *root
#if defined(MY_DEF_HERE)
					   , struct btrfs_new_fs_root_args *new_fs_root_args
#endif /* MY_DEF_HERE */
					   );
struct btrfs_root *btrfs_lookup_fs_root(struct btrfs_fs_info *fs_info,
					u64 root_id);
int btrfs_insert_fs_root(struct btrfs_fs_info *fs_info,
			 struct btrfs_root *root);
void btrfs_free_fs_roots(struct btrfs_fs_info *fs_info);

struct btrfs_root *btrfs_get_fs_root(struct btrfs_fs_info *fs_info,
				     struct btrfs_key *key,
				     bool check_ref);
struct btrfs_root *btrfs_get_new_fs_root(struct btrfs_fs_info *fs_info,
				     struct btrfs_key *key
#if defined(MY_DEF_HERE) || defined(MY_DEF_HERE)
				     , struct btrfs_new_fs_root_args *new_fs_root_args
#endif /* MY_DEF_HERE || MY_DEF_HERE */
				     );
static inline struct btrfs_root *
btrfs_read_fs_root_no_name(struct btrfs_fs_info *fs_info,
			   struct btrfs_key *location)
{
	return btrfs_get_fs_root(fs_info, location, true);
}

int btrfs_cleanup_fs_roots(struct btrfs_fs_info *fs_info);
void btrfs_btree_balance_dirty(struct btrfs_root *root);
void btrfs_btree_balance_dirty_nodelay(struct btrfs_root *root);
void btrfs_drop_and_free_fs_root(struct btrfs_fs_info *fs_info,
				 struct btrfs_root *root);
void btrfs_free_fs_root(struct btrfs_root *root);

#ifdef CONFIG_BTRFS_FS_RUN_SANITY_TESTS
struct btrfs_root *btrfs_alloc_dummy_root(void);
#endif

#ifdef MY_DEF_HERE
void btrfs_add_dead_root(struct btrfs_root *root);

static inline void btrfs_hold_fs_root(struct btrfs_root *root)
{
	atomic_inc(&root->use_refs);
}

static inline void btrfs_release_fs_root(struct btrfs_root *root)
{
	int empty = 0;

	WARN_ON(atomic_read(&root->use_refs) == 0);
	if(!atomic_dec_and_test(&root->use_refs))
		return;

	if (btrfs_root_refs(&root->root_item) != 0)
		return;
	synchronize_srcu(&root->fs_info->subvol_srcu);
	spin_lock(&root->inode_lock);
	empty = RB_EMPTY_ROOT(&root->inode_tree);
	spin_unlock(&root->inode_lock);
	if (empty && atomic_read(&root->use_refs) == 0)
		btrfs_add_dead_root(root);
	return;
}
#endif /* MY_DEF_HERE */

/*
 * This function is used to grab the root, and avoid it is freed when we
 * access it. But it doesn't ensure that the tree is not dropped.
 *
 * If you want to ensure the whole tree is safe, you should use
 * 	fs_info->subvol_srcu
 */
static inline struct btrfs_root *btrfs_grab_fs_root(struct btrfs_root *root)
{
	if (atomic_inc_not_zero(&root->refs))
		return root;
	return NULL;
}

#ifdef MY_DEF_HERE
static inline void btrfs_free_root_eb_monitor(struct btrfs_root *root)
{
	if (root) {
		if (root->eb_hit)
			percpu_counter_destroy(root->eb_hit);
		if (root->eb_miss)
			percpu_counter_destroy(root->eb_miss);
	}
}

void debugfs_remove_root_hook(struct btrfs_root *root);
#endif /* MY_DEF_HERE */

static inline void btrfs_put_fs_root(struct btrfs_root *root)
{
#ifdef MY_DEF_HERE
	if (atomic_dec_and_test(&root->refs)) {
		debugfs_remove_root_hook(root);
		btrfs_free_root_eb_monitor(root);
		kfree(root);
	}
#else
	if (atomic_dec_and_test(&root->refs))
		kfree(root);
#endif /* MY_DEF_HERE */
}

void btrfs_mark_buffer_dirty(struct extent_buffer *buf);
int btrfs_buffer_uptodate(struct extent_buffer *buf, u64 parent_transid,
			  int atomic);
int btrfs_read_buffer(struct extent_buffer *buf, u64 parent_transid, int level,
		      struct btrfs_key *first_key);
u32 btrfs_csum_data(char *data, u32 seed, size_t len);
void btrfs_csum_final(u32 crc, char *result);
int btrfs_bio_wq_end_io(struct btrfs_fs_info *info, struct bio *bio,
			enum btrfs_wq_endio_type metadata);
#ifdef MY_DEF_HERE
int btrfs_wq_submit_bio(struct btrfs_fs_info *fs_info, struct inode *inode,
			int rw, struct bio *bio, int mirror_num,
			unsigned long bio_flags, u64 bio_offset,
			extent_submit_bio_hook_t *submit_bio_start,
			extent_submit_bio_hook_t *submit_bio_done, int throttle);
#else
int btrfs_wq_submit_bio(struct btrfs_fs_info *fs_info, struct inode *inode,
			int rw, struct bio *bio, int mirror_num,
			unsigned long bio_flags, u64 bio_offset,
			extent_submit_bio_hook_t *submit_bio_start,
			extent_submit_bio_hook_t *submit_bio_done);
#endif /* MY_DEF_HERE */
unsigned long btrfs_async_submit_limit(struct btrfs_fs_info *info);
int btrfs_write_tree_block(struct extent_buffer *buf);
int btrfs_wait_tree_block_writeback(struct extent_buffer *buf);
int btrfs_init_log_root_tree(struct btrfs_trans_handle *trans,
			     struct btrfs_fs_info *fs_info);
int btrfs_add_log_tree(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root);
void btrfs_cleanup_one_transaction(struct btrfs_transaction *trans,
				  struct btrfs_root *root);
struct btrfs_root *btrfs_create_tree(struct btrfs_trans_handle *trans,
				     struct btrfs_fs_info *fs_info,
				     u64 objectid);
int btree_lock_page_hook(struct page *page, void *data,
				void (*flush_fn)(void *));
int btrfs_get_num_tolerated_disk_barrier_failures(u64 flags);
int btrfs_calc_num_tolerated_disk_barrier_failures(
	struct btrfs_fs_info *fs_info);
int __init btrfs_end_io_wq_init(void);
void btrfs_end_io_wq_exit(void);

#ifdef CONFIG_DEBUG_LOCK_ALLOC
void btrfs_init_lockdep(void);
void btrfs_set_buffer_lockdep_class(u64 objectid,
			            struct extent_buffer *eb, int level);
#else
static inline void btrfs_init_lockdep(void)
{ }
static inline void btrfs_set_buffer_lockdep_class(u64 objectid,
					struct extent_buffer *eb, int level)
{
}
#endif
#endif
