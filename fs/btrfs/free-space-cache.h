#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Copyright (C) 2009 Oracle.  All rights reserved.
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

#ifndef __BTRFS_FREE_SPACE_CACHE
#define __BTRFS_FREE_SPACE_CACHE

struct btrfs_free_space {
	struct rb_node offset_index;
	struct rb_node bytes_index;
#ifdef MY_DEF_HERE
	struct rb_node bytes_index_with_extent;
#endif /* MY_DEF_HERE */
	u64 offset;
	u64 bytes;
	u64 max_extent_size;
	unsigned long *bitmap;
	struct list_head list;
};

struct btrfs_free_space_ctl {
	spinlock_t tree_lock;
	struct rb_root free_space_offset;
	struct rb_root_cached free_space_bytes;
#ifdef MY_DEF_HERE
	struct rb_root_cached free_space_bytes_with_extent;
#endif /* MY_DEF_HERE */
	u64 free_space;
	int extents_thresh;
	int free_extents;
	int total_bitmaps;
	int unit;
	u64 start;
	const struct btrfs_free_space_op *op;
	void *private;
	struct mutex cache_writeout_mutex;
	struct list_head trimming_ranges;
};

struct btrfs_free_space_op {
	void (*recalc_thresholds)(struct btrfs_free_space_ctl *ctl);
	bool (*use_bitmap)(struct btrfs_free_space_ctl *ctl,
			   struct btrfs_free_space *info);
};

struct btrfs_io_ctl;

struct inode *lookup_free_space_inode(struct btrfs_root *root,
				      struct btrfs_block_group_cache
				      *block_group, struct btrfs_path *path);
int create_free_space_inode(struct btrfs_root *root,
			    struct btrfs_trans_handle *trans,
			    struct btrfs_block_group_cache *block_group,
			    struct btrfs_path *path);

int btrfs_check_trunc_cache_free_space(struct btrfs_root *root,
				       struct btrfs_block_rsv *rsv);
int btrfs_truncate_free_space_cache(struct btrfs_root *root,
				    struct btrfs_trans_handle *trans,
				    struct btrfs_block_group_cache *block_group,
				    struct inode *inode);
int load_free_space_cache(struct btrfs_fs_info *fs_info,
			  struct btrfs_block_group_cache *block_group);
int btrfs_wait_cache_io(struct btrfs_root *root,
			struct btrfs_trans_handle *trans,
			struct btrfs_block_group_cache *block_group,
			struct btrfs_io_ctl *io_ctl,
			struct btrfs_path *path, u64 offset);
int btrfs_write_out_cache(struct btrfs_root *root,
			  struct btrfs_trans_handle *trans,
			  struct btrfs_block_group_cache *block_group,
			  struct btrfs_path *path);
struct inode *lookup_free_ino_inode(struct btrfs_root *root,
				    struct btrfs_path *path);
int create_free_ino_inode(struct btrfs_root *root,
			  struct btrfs_trans_handle *trans,
			  struct btrfs_path *path);
int load_free_ino_cache(struct btrfs_fs_info *fs_info,
			struct btrfs_root *root);
int btrfs_write_out_ino_cache(struct btrfs_root *root,
			      struct btrfs_trans_handle *trans,
			      struct btrfs_path *path,
			      struct inode *inode);

void btrfs_init_free_space_ctl(struct btrfs_block_group_cache *block_group);
int __btrfs_add_free_space(struct btrfs_free_space_ctl *ctl,
			   u64 bytenr, u64 size);
#ifdef MY_DEF_HERE
int __btrfs_add_free_space_with_cache_protection(struct btrfs_free_space_ctl *ctl,
			   u64 offset, u64 bytes);
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
void btrfs_syno_allocator_relink_block_group(struct btrfs_block_group_cache *cache);
void btrfs_syno_allocator_remove_block_group(struct btrfs_block_group_cache *cache);
void btrfs_syno_allocator_preload_block_group(struct btrfs_block_group_cache *cache, u64 bytes);
void btrfs_syno_allocator_release_cache_block_group(struct btrfs_block_group_cache *cache);
#endif /* MY_DEF_HERE */
static inline int
btrfs_add_free_space(struct btrfs_block_group_cache *block_group,
		     u64 bytenr, u64 size)
{
	int ret;
	int (*add_free_space)(struct btrfs_free_space_ctl *ctl, u64 bytenr, u64 size) = __btrfs_add_free_space;

#ifdef MY_DEF_HERE
	add_free_space = __btrfs_add_free_space_with_cache_protection;
#endif /* MY_DEF_HERE */
	ret = add_free_space(block_group->free_space_ctl, bytenr, size);
#ifdef MY_DEF_HERE
	if (!ret)
		btrfs_syno_allocator_relink_block_group(block_group);
#endif /* MY_DEF_HERE */
	return ret;
}
int btrfs_remove_free_space(struct btrfs_block_group_cache *block_group,
			    u64 bytenr, u64 size);
void __btrfs_remove_free_space_cache(struct btrfs_free_space_ctl *ctl);
void btrfs_remove_free_space_cache(struct btrfs_block_group_cache
				     *block_group);
u64 btrfs_find_space_for_alloc(struct btrfs_block_group_cache *block_group,
			       u64 offset, u64 bytes, u64 empty_size,
			       u64 *max_extent_size);
u64 btrfs_find_ino_for_alloc(struct btrfs_root *fs_root);
void btrfs_dump_free_space(struct btrfs_block_group_cache *block_group,
			   u64 bytes);
int btrfs_find_space_cluster(struct btrfs_root *root,
			     struct btrfs_block_group_cache *block_group,
			     struct btrfs_free_cluster *cluster,
#ifdef MY_DEF_HERE
			     u64 offset, u64 bytes, u64 empty_size, u64 reserve_bytes);
#else
			     u64 offset, u64 bytes, u64 empty_size);
#endif /* MY_DEF_HERE */
void btrfs_init_free_cluster(struct btrfs_free_cluster *cluster);
u64 btrfs_alloc_from_cluster(struct btrfs_block_group_cache *block_group,
			     struct btrfs_free_cluster *cluster, u64 bytes,
			     u64 min_start, u64 *max_extent_size);
int btrfs_return_cluster_to_free_space(
			       struct btrfs_block_group_cache *block_group,
			       struct btrfs_free_cluster *cluster);
#ifdef MY_DEF_HERE
int btrfs_trim_block_group(struct btrfs_block_group_cache *block_group,
			   u64 *trimmed, u64 start, u64 end, u64 minlen, enum trim_act act);
#else /* MY_DEF_HERE */
int btrfs_trim_block_group(struct btrfs_block_group_cache *block_group,
			   u64 *trimmed, u64 start, u64 end, u64 minlen);
#endif /* MY_DEF_HERE */

/* Support functions for running our sanity tests */
#ifdef CONFIG_BTRFS_FS_RUN_SANITY_TESTS
int test_add_free_space_entry(struct btrfs_block_group_cache *cache,
			      u64 offset, u64 bytes, bool bitmap);
int test_check_exists(struct btrfs_block_group_cache *cache,
		      u64 offset, u64 bytes);
#endif

#endif
