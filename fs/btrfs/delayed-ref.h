#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Copyright (C) 2008 Oracle.  All rights reserved.
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
#ifndef __DELAYED_REF__
#define __DELAYED_REF__

#ifdef MY_DEF_HERE
#include "btrfs_inode.h"
#endif /* MY_DEF_HERE */

/* these are the possible values of struct btrfs_delayed_ref_node->action */
#define BTRFS_ADD_DELAYED_REF    1 /* add one backref to the tree */
#define BTRFS_DROP_DELAYED_REF   2 /* delete one backref from the tree */
#define BTRFS_ADD_DELAYED_EXTENT 3 /* record a full extent allocation */
#define BTRFS_UPDATE_DELAYED_HEAD 4 /* not changing ref count on head ref */

/*
 * XXX: Qu: I really hate the design that ref_head and tree/data ref shares the
 * same ref_node structure.
 * Ref_head is in a higher logic level than tree/data ref, and duplicated
 * bytenr/num_bytes in ref_node is really a waste or memory, they should be
 * referred from ref_head.
 * This gets more disgusting after we use list to store tree/data ref in
 * ref_head. Must clean this mess up later.
 */
struct btrfs_delayed_ref_node {
	/*
	 * ref_head use rb tree, stored in ref_root->href.
	 * indexed by bytenr
	 */
	struct rb_node rb_node;

	/*data/tree ref use list, stored in ref_head->ref_list. */
	struct list_head list;
	/*
	 * If action is BTRFS_ADD_DELAYED_REF, also link this node to
	 * ref_head->ref_add_list, then we do not need to iterate the
	 * whole ref_head->ref_list to find BTRFS_ADD_DELAYED_REF nodes.
	 */
	struct list_head add_list;

#ifdef MY_DEF_HERE
	struct list_head syno_list;
#endif /* MY_DEF_HERE */

	/* the starting bytenr of the extent */
	u64 bytenr;

	/* the size of the extent */
	u64 num_bytes;

	/* seq number to keep track of insertion order */
	u64 seq;

	/* ref count on this data structure */
	atomic_t refs;

	/*
	 * how many refs is this entry adding or deleting.  For
	 * head refs, this may be a negative number because it is keeping
	 * track of the total mods done to the reference count.
	 * For individual refs, this will always be a positive number
	 *
	 * It may be more than one, since it is possible for a single
	 * parent to have more than one ref on an extent
	 */
	int ref_mod;

	unsigned int action:8;
	unsigned int type:8;
#ifdef MY_DEF_HERE
	unsigned int no_quota:1;
#endif /* MY_DEF_HERE */
	/* is this node still in the rbtree? */
	unsigned int is_head:1;
	unsigned int in_tree:1;
};

struct btrfs_delayed_extent_op {
	struct btrfs_disk_key key;
	u8 level;
	bool update_key;
	bool update_flags;
	bool is_data;
	u64 flags_to_set;
};

/*
 * the head refs are used to hold a lock on a given extent, which allows us
 * to make sure that only one process is running the delayed refs
 * at a time for a single extent.  They also store the sum of all the
 * reference count modifications we've queued up.
 */
struct btrfs_delayed_ref_head {
	struct btrfs_delayed_ref_node node;

	/*
	 * the mutex is held while running the refs, and it is also
	 * held when checking the sum of reference modifications.
	 */
	struct mutex mutex;

	spinlock_t lock;
	struct list_head ref_list;
	/* accumulate add BTRFS_ADD_DELAYED_REF nodes to this ref_add_list. */
	struct list_head ref_add_list;

#ifdef MY_DEF_HERE
	struct list_head ref_syno_list;
#endif /* MY_DEF_HERE */

	struct rb_node href_node;

	struct btrfs_delayed_extent_op *extent_op;

	/*
	 * This is used to track the final ref_mod from all the refs associated
	 * with this head ref, this is not adjusted as delayed refs are run,
	 * this is meant to track if we need to do the csum accounting or not.
	 */
	int total_ref_mod;

	/*
	 * For qgroup reserved space freeing.
	 *
	 * ref_root and reserved will be recorded after
	 * BTRFS_ADD_DELAYED_EXTENT is called.
	 * And will be used to free reserved qgroup space at
	 * run_delayed_refs() time.
	 */
	u64 qgroup_ref_root;
	u64 qgroup_reserved;

	/*
	 * when a new extent is allocated, it is just reserved in memory
	 * The actual extent isn't inserted into the extent allocation tree
	 * until the delayed ref is processed.  must_insert_reserved is
	 * used to flag a delayed ref so the accounting can be updated
	 * when a full insert is done.
	 *
	 * It is possible the extent will be freed before it is ever
	 * inserted into the extent allocation tree.  In this case
	 * we need to update the in ram accounting to properly reflect
	 * the free has happened.
	 */
	unsigned int must_insert_reserved:1;
	unsigned int is_data:1;
	unsigned int processing:1;
#ifdef MY_DEF_HERE
	unsigned int syno_usage;
#endif /* MY_DEF_HERE */
};

struct btrfs_delayed_tree_ref {
	struct btrfs_delayed_ref_node node;
	u64 root;
	u64 parent;
	int level;
};

struct btrfs_delayed_data_ref {
	struct btrfs_delayed_ref_node node;
	u64 root;
	u64 parent;
	u64 objectid;
	u64 offset;
#ifdef MY_DEF_HERE
	u64 ram_bytes;
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
	uid_t uid;
	struct inode *inode;
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
	unsigned int syno_usage;
#endif /* MY_DEF_HERE */
};

struct btrfs_delayed_ref_root {
	/* head ref rbtree */
	struct rb_root href_root;

	/* dirty extent records */
#ifdef MY_DEF_HERE
#else
	struct rb_root dirty_extent_root;
#endif /* MY_DEF_HERE */

	/* this spin lock protects the rbtree and the entries inside */
	spinlock_t lock;

	/* how many delayed ref updates we've queued, used by the
	 * throttling code
	 */
	atomic_t num_entries;

#ifdef MY_DEF_HERE
	atomic_t num_syno_usage_entries;
#endif /* MY_DEF_HERE */

	/* total number of head nodes in tree */
	unsigned long num_heads;

	/* total number of head nodes ready for processing */
	unsigned long num_heads_ready;

#ifdef MY_DEF_HERE
	unsigned long num_syno_usage_heads_ready;
#endif /* MY_DEF_HERE */

	u64 pending_csums;

#ifdef MY_DEF_HERE
	u64 num_pending_csums_leafs;
#endif /* MY_DEF_HERE */

	/*
	 * set when the tree is flushing before a transaction commit,
	 * used by the throttling code to decide if new updates need
	 * to be run right away
	 */
	int flushing;

	u64 run_delayed_start;

	/*
	 * To make qgroup to skip given root.
	 * This is for snapshot, as btrfs_qgroup_inherit() will manually
	 * modify counters for snapshot and its source, so we should skip
	 * the snapshot in new_root/old_roots or it will get calculated twice
	 */
	u64 qgroup_to_skip;
};

extern struct kmem_cache *btrfs_delayed_ref_head_cachep;
extern struct kmem_cache *btrfs_delayed_tree_ref_cachep;
extern struct kmem_cache *btrfs_delayed_data_ref_cachep;
extern struct kmem_cache *btrfs_delayed_extent_op_cachep;

int __init btrfs_delayed_ref_init(void);
void btrfs_delayed_ref_exit(void);

static inline struct btrfs_delayed_extent_op *
btrfs_alloc_delayed_extent_op(void)
{
	return kmem_cache_alloc(btrfs_delayed_extent_op_cachep, GFP_NOFS);
}

static inline void
btrfs_free_delayed_extent_op(struct btrfs_delayed_extent_op *op)
{
	if (op)
		kmem_cache_free(btrfs_delayed_extent_op_cachep, op);
}

#ifdef MY_DEF_HERE
static inline struct btrfs_delayed_data_ref *
btrfs_delayed_node_to_data_ref(struct btrfs_delayed_ref_node *node);
#endif /* MY_DEF_HERE */

static inline void btrfs_put_delayed_ref(struct btrfs_delayed_ref_node *ref)
{
#ifdef MY_DEF_HERE
	struct btrfs_delayed_data_ref *data_ref = NULL;
#endif /* MY_DEF_HERE */

	WARN_ON(atomic_read(&ref->refs) == 0);
	if (atomic_dec_and_test(&ref->refs)) {
		WARN_ON(ref->in_tree);
		switch (ref->type) {
		case BTRFS_TREE_BLOCK_REF_KEY:
		case BTRFS_SHARED_BLOCK_REF_KEY:
			kmem_cache_free(btrfs_delayed_tree_ref_cachep, ref);
			break;
		case BTRFS_EXTENT_DATA_REF_KEY:
		case BTRFS_SHARED_DATA_REF_KEY:
#ifdef MY_DEF_HERE
			data_ref = btrfs_delayed_node_to_data_ref(ref);
			syno_usrquota_inode_put(data_ref->inode);
#endif /* MY_DEF_HERE */
			kmem_cache_free(btrfs_delayed_data_ref_cachep, ref);
			break;
		case 0:
			kmem_cache_free(btrfs_delayed_ref_head_cachep, ref);
			break;
		default:
			BUG();
		}
	}
}

int btrfs_add_delayed_tree_ref(struct btrfs_fs_info *fs_info,
			       struct btrfs_trans_handle *trans,
			       u64 bytenr, u64 num_bytes, u64 parent,
			       u64 ref_root, int level, int action,
			       struct btrfs_delayed_extent_op *extent_op);
int btrfs_add_delayed_data_ref(struct btrfs_fs_info *fs_info,
			       struct btrfs_trans_handle *trans,
			       u64 bytenr, u64 num_bytes,
			       u64 parent, u64 ref_root,
			       u64 owner, u64 offset, u64 reserved,
#ifdef MY_DEF_HERE
			       int no_quota,
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
			       struct inode *inode, uid_t uid,
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
			       int syno_usage,
#endif /* MY_DEF_HERE */
			       int action,
			       struct btrfs_delayed_extent_op *extent_op);
int btrfs_add_delayed_qgroup_reserve(struct btrfs_fs_info *fs_info,
				     struct btrfs_trans_handle *trans,
				     u64 ref_root, u64 bytenr, u64 num_bytes);
int btrfs_add_delayed_extent_op(struct btrfs_fs_info *fs_info,
				struct btrfs_trans_handle *trans,
				u64 bytenr, u64 num_bytes,
				struct btrfs_delayed_extent_op *extent_op);
void btrfs_merge_delayed_refs(struct btrfs_trans_handle *trans,
			      struct btrfs_fs_info *fs_info,
			      struct btrfs_delayed_ref_root *delayed_refs,
			      struct btrfs_delayed_ref_head *head);

struct btrfs_delayed_ref_head *
btrfs_find_delayed_ref_head(struct btrfs_delayed_ref_root *delayed_refs,
			    u64 bytenr);
int btrfs_delayed_ref_lock(struct btrfs_trans_handle *trans,
			   struct btrfs_delayed_ref_head *head);
static inline void btrfs_delayed_ref_unlock(struct btrfs_delayed_ref_head *head)
{
	mutex_unlock(&head->mutex);
}


struct btrfs_delayed_ref_head *
btrfs_select_ref_head(struct btrfs_trans_handle *trans);
#ifdef MY_DEF_HERE
struct btrfs_delayed_ref_head *
btrfs_select_data_ref_head(struct btrfs_trans_handle *trans);
#endif /* MY_DEF_HERE */

int btrfs_check_delayed_seq(struct btrfs_fs_info *fs_info,
			    struct btrfs_delayed_ref_root *delayed_refs,
			    u64 seq);

/*
 * a node might live in a head or a regular ref, this lets you
 * test for the proper type to use.
 */
static int btrfs_delayed_ref_is_head(struct btrfs_delayed_ref_node *node)
{
	return node->is_head;
}

/*
 * helper functions to cast a node into its container
 */
static inline struct btrfs_delayed_tree_ref *
btrfs_delayed_node_to_tree_ref(struct btrfs_delayed_ref_node *node)
{
	WARN_ON(btrfs_delayed_ref_is_head(node));
	return container_of(node, struct btrfs_delayed_tree_ref, node);
}

static inline struct btrfs_delayed_data_ref *
btrfs_delayed_node_to_data_ref(struct btrfs_delayed_ref_node *node)
{
	WARN_ON(btrfs_delayed_ref_is_head(node));
	return container_of(node, struct btrfs_delayed_data_ref, node);
}

static inline struct btrfs_delayed_ref_head *
btrfs_delayed_node_to_head(struct btrfs_delayed_ref_node *node)
{
	WARN_ON(!btrfs_delayed_ref_is_head(node));
	return container_of(node, struct btrfs_delayed_ref_head, node);
}
#endif
