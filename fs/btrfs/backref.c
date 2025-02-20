#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Copyright (C) 2011 STRATO.  All rights reserved.
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

#include <linux/vmalloc.h>
#include <linux/rbtree.h>
#include "ctree.h"
#include "disk-io.h"
#include "backref.h"
#include "ulist.h"
#include "transaction.h"
#include "delayed-ref.h"
#include "locking.h"

/* Just an arbitrary number so we can be sure this happened */
#define BACKREF_FOUND_SHARED 6
#ifdef MY_DEF_HERE
#define BACKREF_NEXT_ITEM 253
#define BACKREF_FOUND_SHARED_ROOT 254
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
#define BACKREF_FOUND_ROOT_INO 255
#endif /* MY_DEF_HERE */

#ifdef MY_DEF_HERE
enum btrfs_backref_mode {
	/*
	 * The original backref mode
	 */
	BTRFS_BACKREF_NORMAL,
#ifdef MY_DEF_HERE
	/*
	 * This mode will check whether EXTENT_ITEM is referenced prior to
	 * an offset in an inode of a desiganted subvolume.
	 * If offset is provided with (u64)-1, all the file is checked.
	 * This mode is currently used by quota accounting for
	 * 1. clone range
	 * 2. remove extents
	 * 3. usrquota chown.
	 * The offset of file should be passed to check_root_inode_ref for usrquota
	 * chown or (u64)-1 for the other two cases.
	 */
	BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET,
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
	/*
	 * This mode will find if specific EXTENT_ITEM/METADATA_ITEM is pointed by
	 * any subvolume that is not in the list.
	 */
	BTRFS_BACKREF_FIND_SHARED_ROOT,
#endif /* MY_DEF_HERE */
};
#endif /* MY_DEF_HERE */

struct extent_inode_elem {
	u64 inum;
	u64 offset;
#ifdef MY_DEF_HERE
	int extent_type;
#endif /* MY_DEF_HERE */
	struct extent_inode_elem *next;
};

/*
 * ref_root is used as the root of the ref tree that hold a collection
 * of unique references.
 */
struct ref_root {
	struct rb_root rb_root;

	/*
	 * The unique_refs represents the number of ref_nodes with a positive
	 * count stored in the tree. Even if a ref_node (the count is greater
	 * than one) is added, the unique_refs will only increase by one.
	 */
	unsigned int unique_refs;
};

/* ref_node is used to store a unique reference to the ref tree. */
struct ref_node {
	struct rb_node rb_node;

	/* For NORMAL_REF, otherwise all these fields should be set to 0 */
	u64 root_id;
	u64 object_id;
	u64 offset;

	/* For SHARED_REF, otherwise parent field should be set to 0 */
	u64 parent;

	/* Ref to the ref_mod of btrfs_delayed_ref_node */
	int ref_mod;
};

/* Dynamically allocate and initialize a ref_root */
static struct ref_root *ref_root_alloc(void)
{
	struct ref_root *ref_tree;

	ref_tree = kmalloc(sizeof(*ref_tree), GFP_NOFS);
	if (!ref_tree)
		return NULL;

	ref_tree->rb_root = RB_ROOT;
	ref_tree->unique_refs = 0;

	return ref_tree;
}

/* Free all nodes in the ref tree, and reinit ref_root */
static void ref_root_fini(struct ref_root *ref_tree)
{
	struct ref_node *node;
	struct rb_node *next;

	while ((next = rb_first(&ref_tree->rb_root)) != NULL) {
		node = rb_entry(next, struct ref_node, rb_node);
		rb_erase(next, &ref_tree->rb_root);
		kfree(node);
	}

	ref_tree->rb_root = RB_ROOT;
	ref_tree->unique_refs = 0;
}

static void ref_root_free(struct ref_root *ref_tree)
{
	if (!ref_tree)
		return;

	ref_root_fini(ref_tree);
	kfree(ref_tree);
}

/*
 * Compare ref_node with (root_id, object_id, offset, parent)
 *
 * The function compares two ref_node a and b. It returns an integer less
 * than, equal to, or greater than zero , respectively, to be less than, to
 * equal, or be greater than b.
 */
static int ref_node_cmp(struct ref_node *a, struct ref_node *b)
{
	if (a->root_id < b->root_id)
		return -1;
	else if (a->root_id > b->root_id)
		return 1;

	if (a->object_id < b->object_id)
		return -1;
	else if (a->object_id > b->object_id)
		return 1;

	if (a->offset < b->offset)
		return -1;
	else if (a->offset > b->offset)
		return 1;

	if (a->parent < b->parent)
		return -1;
	else if (a->parent > b->parent)
		return 1;

	return 0;
}

/*
 * Search ref_node with (root_id, object_id, offset, parent) in the tree
 *
 * if found, the pointer of the ref_node will be returned;
 * if not found, NULL will be returned and pos will point to the rb_node for
 * insert, pos_parent will point to pos'parent for insert;
*/
static struct ref_node *__ref_tree_search(struct ref_root *ref_tree,
					  struct rb_node ***pos,
					  struct rb_node **pos_parent,
					  u64 root_id, u64 object_id,
					  u64 offset, u64 parent)
{
	struct ref_node *cur = NULL;
	struct ref_node entry;
	int ret;

	entry.root_id = root_id;
	entry.object_id = object_id;
	entry.offset = offset;
	entry.parent = parent;

	*pos = &ref_tree->rb_root.rb_node;

	while (**pos) {
		*pos_parent = **pos;
		cur = rb_entry(*pos_parent, struct ref_node, rb_node);

		ret = ref_node_cmp(cur, &entry);
		if (ret > 0)
			*pos = &(**pos)->rb_left;
		else if (ret < 0)
			*pos = &(**pos)->rb_right;
		else
			return cur;
	}

	return NULL;
}

/*
 * Insert a ref_node to the ref tree
 * @pos used for specifiy the position to insert
 * @pos_parent for specifiy pos's parent
 *
 * success, return 0;
 * ref_node already exists, return -EEXIST;
*/
static int ref_tree_insert(struct ref_root *ref_tree, struct rb_node **pos,
			   struct rb_node *pos_parent, struct ref_node *ins)
{
	struct rb_node **p = NULL;
	struct rb_node *parent = NULL;
	struct ref_node *cur = NULL;

	if (!pos) {
		cur = __ref_tree_search(ref_tree, &p, &parent, ins->root_id,
					ins->object_id, ins->offset,
					ins->parent);
		if (cur)
			return -EEXIST;
	} else {
		p = pos;
		parent = pos_parent;
	}

	rb_link_node(&ins->rb_node, parent, p);
	rb_insert_color(&ins->rb_node, &ref_tree->rb_root);

	return 0;
}

/* Erase and free ref_node, caller should update ref_root->unique_refs */
static void ref_tree_remove(struct ref_root *ref_tree, struct ref_node *node)
{
	rb_erase(&node->rb_node, &ref_tree->rb_root);
	kfree(node);
}

/*
 * Update ref_root->unique_refs
 *
 * Call __ref_tree_search
 *	1. if ref_node doesn't exist, ref_tree_insert this node, and update
 *	ref_root->unique_refs:
 *		if ref_node->ref_mod > 0, ref_root->unique_refs++;
 *		if ref_node->ref_mod < 0, do noting;
 *
 *	2. if ref_node is found, then get origin ref_node->ref_mod, and update
 *	ref_node->ref_mod.
 *		if ref_node->ref_mod is equal to 0,then call ref_tree_remove
 *
 *		according to origin_mod and new_mod, update ref_root->items
 *		+----------------+--------------+-------------+
 *		|		 |new_count <= 0|new_count > 0|
 *		+----------------+--------------+-------------+
 *		|origin_count < 0|       0      |      1      |
 *		+----------------+--------------+-------------+
 *		|origin_count > 0|      -1      |      0      |
 *		+----------------+--------------+-------------+
 *
 * In case of allocation failure, -ENOMEM is returned and the ref_tree stays
 * unaltered.
 * Success, return 0
 */
static int ref_tree_add(struct ref_root *ref_tree, u64 root_id, u64 object_id,
			u64 offset, u64 parent, int count)
{
	struct ref_node *node = NULL;
	struct rb_node **pos = NULL;
	struct rb_node *pos_parent = NULL;
	int origin_count;
	int ret;

	if (!count)
		return 0;

	node = __ref_tree_search(ref_tree, &pos, &pos_parent, root_id,
				 object_id, offset, parent);
	if (node == NULL) {
		node = kmalloc(sizeof(*node), GFP_NOFS);
		if (!node)
			return -ENOMEM;

		node->root_id = root_id;
		node->object_id = object_id;
		node->offset = offset;
		node->parent = parent;
		node->ref_mod = count;

		ret = ref_tree_insert(ref_tree, pos, pos_parent, node);
		ASSERT(!ret);
		if (ret) {
			kfree(node);
			return ret;
		}

		ref_tree->unique_refs += node->ref_mod > 0 ? 1 : 0;

		return 0;
	}

	origin_count = node->ref_mod;
	node->ref_mod += count;

	if (node->ref_mod > 0)
		ref_tree->unique_refs += origin_count > 0 ? 0 : 1;
	else if (node->ref_mod <= 0)
		ref_tree->unique_refs += origin_count > 0 ? -1 : 0;

	if (!node->ref_mod)
		ref_tree_remove(ref_tree, node);

	return 0;
}

static int check_extent_in_eb(struct btrfs_key *key, struct extent_buffer *eb,
				struct btrfs_file_extent_item *fi,
				u64 extent_item_pos,
				struct extent_inode_elem **eie)
{
	u64 offset = 0;
	struct extent_inode_elem *e;

	if (!btrfs_file_extent_compression(eb, fi) &&
	    !btrfs_file_extent_encryption(eb, fi) &&
	    !btrfs_file_extent_other_encoding(eb, fi)) {
		u64 data_offset;
		u64 data_len;

		data_offset = btrfs_file_extent_offset(eb, fi);
		data_len = btrfs_file_extent_num_bytes(eb, fi);

		if (extent_item_pos < data_offset ||
		    extent_item_pos >= data_offset + data_len)
			return 1;
		offset = extent_item_pos - data_offset;
	}

	e = kmalloc(sizeof(*e), GFP_NOFS);
	if (!e)
		return -ENOMEM;

	e->next = *eie;
	e->inum = key->objectid;
	e->offset = key->offset + offset;
#ifdef MY_DEF_HERE
	e->extent_type = btrfs_file_extent_type(eb, fi);
#endif /* MY_DEF_HERE */
	*eie = e;

	return 0;
}

#ifdef MY_DEF_HERE
static int find_ino_extent_in_eb(struct extent_buffer *eb,
				u64 wanted_disk_byte, u64 ino, u64 offset)
{
	u64 disk_byte;
	struct btrfs_key key;
	struct btrfs_file_extent_item *fi;
	int slot;
	int nritems;
	int extent_type;

	/*
	 * from the shared data ref, we only have the leaf but we need
	 * the key. thus, we must look into all items and see that we
	 * find one (some) with a reference to our extent item.
	 */
	nritems = btrfs_header_nritems(eb);
	for (slot = 0; slot < nritems; ++slot) {
		btrfs_item_key_to_cpu(eb, &key, slot);
		if (key.objectid > ino)
			break;
		if (key.type != BTRFS_EXTENT_DATA_KEY)
			continue;
		fi = btrfs_item_ptr(eb, slot, struct btrfs_file_extent_item);
		extent_type = btrfs_file_extent_type(eb, fi);
		if (extent_type == BTRFS_FILE_EXTENT_INLINE)
			continue;
		/* don't skip BTRFS_FILE_EXTENT_PREALLOC, we can handle that */
		disk_byte = btrfs_file_extent_disk_bytenr(eb, fi);
		if (disk_byte != wanted_disk_byte)
			continue;

		if (key.objectid == ino) {
			if (key.offset >= offset)
				return 0;
			/*
			 * For offset != (u64)-1, ulist could avoid calling check for
			 * same extent multiple times.
			 */
			return 1;
		}
	}

	return 0;
}
#endif /* MY_DEF_HERE */

static void free_inode_elem_list(struct extent_inode_elem *eie)
{
	struct extent_inode_elem *eie_next;

	for (; eie; eie = eie_next) {
		eie_next = eie->next;
		kfree(eie);
	}
}

static int find_extent_in_eb(struct extent_buffer *eb, u64 wanted_disk_byte,
				u64 extent_item_pos,
				struct extent_inode_elem **eie)
{
	u64 disk_byte;
	struct btrfs_key key;
	struct btrfs_file_extent_item *fi;
	int slot;
	int nritems;
	int extent_type;
	int ret;

	/*
	 * from the shared data ref, we only have the leaf but we need
	 * the key. thus, we must look into all items and see that we
	 * find one (some) with a reference to our extent item.
	 */
	nritems = btrfs_header_nritems(eb);
	for (slot = 0; slot < nritems; ++slot) {
		btrfs_item_key_to_cpu(eb, &key, slot);
		if (key.type != BTRFS_EXTENT_DATA_KEY)
			continue;
		fi = btrfs_item_ptr(eb, slot, struct btrfs_file_extent_item);
		extent_type = btrfs_file_extent_type(eb, fi);
		if (extent_type == BTRFS_FILE_EXTENT_INLINE)
			continue;
		/* don't skip BTRFS_FILE_EXTENT_PREALLOC, we can handle that */
		disk_byte = btrfs_file_extent_disk_bytenr(eb, fi);
		if (disk_byte != wanted_disk_byte)
			continue;

		ret = check_extent_in_eb(&key, eb, fi, extent_item_pos, eie);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * this structure records all encountered refs on the way up to the root
 */
struct __prelim_ref {
	struct list_head list;
	u64 root_id;
	struct btrfs_key key_for_search;
	int level;
	int count;
	struct extent_inode_elem *inode_list;
	u64 parent;
	u64 wanted_disk_byte;
};

static struct kmem_cache *btrfs_prelim_ref_cache;

int __init btrfs_prelim_ref_init(void)
{
	btrfs_prelim_ref_cache = kmem_cache_create("btrfs_prelim_ref",
					sizeof(struct __prelim_ref),
					0,
					SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD,
					NULL);
	if (!btrfs_prelim_ref_cache)
		return -ENOMEM;
	return 0;
}

void btrfs_prelim_ref_exit(void)
{
	kmem_cache_destroy(btrfs_prelim_ref_cache);
}

/*
 * the rules for all callers of this function are:
 * - obtaining the parent is the goal
 * - if you add a key, you must know that it is a correct key
 * - if you cannot add the parent or a correct key, then we will look into the
 *   block later to set a correct key
 *
 * delayed refs
 * ============
 *        backref type | shared | indirect | shared | indirect
 * information         |   tree |     tree |   data |     data
 * --------------------+--------+----------+--------+----------
 *      parent logical |    y   |     -    |    -   |     -
 *      key to resolve |    -   |     y    |    y   |     y
 *  tree block logical |    -   |     -    |    -   |     -
 *  root for resolving |    y   |     y    |    y   |     y
 *
 * - column 1:       we've the parent -> done
 * - column 2, 3, 4: we use the key to find the parent
 *
 * on disk refs (inline or keyed)
 * ==============================
 *        backref type | shared | indirect | shared | indirect
 * information         |   tree |     tree |   data |     data
 * --------------------+--------+----------+--------+----------
 *      parent logical |    y   |     -    |    y   |     -
 *      key to resolve |    -   |     -    |    -   |     y
 *  tree block logical |    y   |     y    |    y   |     y
 *  root for resolving |    -   |     y    |    y   |     y
 *
 * - column 1, 3: we've the parent -> done
 * - column 2:    we take the first key from the block to find the parent
 *                (see __add_missing_keys)
 * - column 4:    we use the key to find the parent
 *
 * additional information that's available but not required to find the parent
 * block might help in merging entries to gain some speed.
 */

static int __add_prelim_ref(struct list_head *head, u64 root_id,
			    struct btrfs_key *key, int level,
			    u64 parent, u64 wanted_disk_byte, int count,
#ifdef MY_DEF_HERE
			    enum btrfs_backref_mode mode,
#endif /* MY_DEF_HERE */
			    gfp_t gfp_mask)
{
	struct __prelim_ref *ref;

	if (root_id == BTRFS_DATA_RELOC_TREE_OBJECTID)
		return 0;

	ref = kmem_cache_alloc(btrfs_prelim_ref_cache, gfp_mask);
	if (!ref)
		return -ENOMEM;

	ref->root_id = root_id;
	if (key) {
		ref->key_for_search = *key;
		/*
		 * We can often find data backrefs with an offset that is too
		 * large (>= LLONG_MAX, maximum allowed file offset) due to
		 * underflows when subtracting a file's offset with the data
		 * offset of its corresponding extent data item. This can
		 * happen for example in the clone ioctl.
		 * So if we detect such case we set the search key's offset to
		 * zero to make sure we will find the matching file extent item
		 * at add_all_parents(), otherwise we will miss it because the
		 * offset taken form the backref is much larger then the offset
		 * of the file extent item. This can make us scan a very large
		 * number of file extent items, but at least it will not make
		 * us miss any.
		 * This is an ugly workaround for a behaviour that should have
		 * never existed, but it does and a fix for the clone ioctl
		 * would touch a lot of places, cause backwards incompatibility
		 * and would not fix the problem for extents cloned with older
		 * kernels.
		 */
#ifdef MY_DEF_HERE
		/*
		 * We want to speed up our backref walk for case in finding
		 * whether there's a reference from particular subvolume's
		 * inode to this extent item. In order for that to walk correctly
		 * we need the exact offset which backref holds. Therefore,
		 * we apply this workaround when we use the key to search, so
		 * we sould keep this information for later use.
		 */
		if (mode == BTRFS_BACKREF_NORMAL)
#endif /* MY_DEF_HERE */
		if (ref->key_for_search.type == BTRFS_EXTENT_DATA_KEY &&
		    ref->key_for_search.offset >= LLONG_MAX)
			ref->key_for_search.offset = 0;
	} else {
		memset(&ref->key_for_search, 0, sizeof(ref->key_for_search));
	}

	ref->inode_list = NULL;
	ref->level = level;
	ref->count = count;
	ref->parent = parent;
	ref->wanted_disk_byte = wanted_disk_byte;
	list_add_tail(&ref->list, head);

	return 0;
}

static int add_all_parents(struct btrfs_root *root, struct btrfs_path *path,
			   struct ulist *parents, struct __prelim_ref *ref,
			   int level, u64 time_seq, const u64 *extent_item_pos,
#ifdef MY_DEF_HERE
			   enum btrfs_backref_mode mode,
			   u64 file_offset, int check_first_ref,
#endif /* MY_DEF_HERE */
			   u64 total_refs)
{
	int ret = 0;
	int slot;
	struct extent_buffer *eb;
	struct btrfs_key key;
	struct btrfs_key *key_for_search = &ref->key_for_search;
	struct btrfs_file_extent_item *fi;
	struct extent_inode_elem *eie = NULL, *old = NULL;
	u64 disk_byte;
	u64 wanted_disk_byte = ref->wanted_disk_byte;
	u64 count = 0;
#ifdef MY_DEF_HERE
	u64 total_count;
	u64 datao;
	u64 ram_bytes = 0x10000000; //256MB

	/*
	 * We want to optimize the EXTENT_DATA search process. Since the
	 * resolving is based on items in backref, we know that EXTENT_DATA
	 * keys that belong to this backref won't span across num bytes of
	 * that EXTENT_ITEM. We might have case where, 2 EXTENT_DATA belongs
	 * to the same backref with type BTRFS_EXTENT_DATA_REF_KEY, but
	 * there exists one EXTENT_DATA whose offset is between the previous
	 * 2 EXTENT_DATA. If the key type for this middle reference is
	 * BTRFS_EXTENT_DATA_REF_KEY, and it doesn't belong to the same
	 * backref (i.e. It has difference offset), we can handle it by
	 * checking offset. If it's BTRFS_SHARED_DATA_REF_KEY, in the previous
	 * backref collection (add_inline_ref, add_keyed_refs, add_delayed_refs)
	 * we have recorded how many BTRFS_SHARED_DATA_REF_KEY this
	 * EXTENT_ITEM has. Treat it as our upper bound to search, so we won't
	 * miss our keys that fall behind this offset.
	 */
	if (mode == BTRFS_BACKREF_NORMAL || key_for_search->type != BTRFS_EXTENT_DATA_KEY)
		total_count = total_refs;
	else
		total_count = ref->count + total_refs;
#endif /* MY_DEF_HERE */

	if (level != 0) {
		eb = path->nodes[level];
		ret = ulist_add(parents, eb->start, 0, GFP_NOFS);
		if (ret < 0)
			return ret;
		return 0;
	}

	/*
	 * We normally enter this function with the path already pointing to
	 * the first item to check. But sometimes, we may enter it with
	 * slot==nritems. In that case, go to the next leaf before we continue.
	 */
	if (path->slots[0] >= btrfs_header_nritems(path->nodes[0])) {
		if (time_seq == (u64)-1)
			ret = btrfs_next_leaf(root, path);
		else
			ret = btrfs_next_old_leaf(root, path, time_seq);
	}

#ifdef MY_DEF_HERE
	while (!ret && count < total_count) {
#else
	while (!ret && count < total_refs) {
#endif /* MY_DEF_HERE */
		eb = path->nodes[0];
		slot = path->slots[0];

		btrfs_item_key_to_cpu(eb, &key, slot);

		if (key.objectid != key_for_search->objectid ||
		    key.type != BTRFS_EXTENT_DATA_KEY)
			break;

		fi = btrfs_item_ptr(eb, slot, struct btrfs_file_extent_item);
		disk_byte = btrfs_file_extent_disk_bytenr(eb, fi);

#ifdef MY_DEF_HERE
		if (key_for_search->type == BTRFS_EXTENT_DATA_KEY &&
		    key.offset >= key_for_search->offset + ram_bytes)
			break;
#endif /* MY_DEF_HERE */
		if (disk_byte == wanted_disk_byte) {
			eie = NULL;
			old = NULL;
#ifdef MY_DEF_HERE
			ram_bytes = btrfs_file_extent_ram_bytes(eb, fi);
			if (mode != BTRFS_BACKREF_NORMAL) {
				datao = key.offset - btrfs_file_extent_offset(eb, fi);
				if (datao != key_for_search->offset)
					goto next;
#ifdef MY_DEF_HERE
				if (mode == BTRFS_BACKREF_FIND_SHARED_ROOT && check_first_ref &&
					key.offset < file_offset) {
					/*
					 * goto out the return value will be set to 0, so return
					 * it directly, the caller can handle it.
					 */
					return BACKREF_NEXT_ITEM;
				}
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
				if (mode == BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET &&
				    key.offset >= file_offset) {
					break;
				}
#endif /* MY_DEF_HERE */
			}
#endif /* MY_DEF_HERE */
			count++;
			if (extent_item_pos) {
				ret = check_extent_in_eb(&key, eb, fi,
						*extent_item_pos,
						&eie);
				if (ret < 0)
					break;
			}
			if (ret > 0)
				goto next;
			ret = ulist_add_merge_ptr(parents, eb->start,
						  eie, (void **)&old, GFP_NOFS);
			if (ret < 0)
				break;
			if (!ret && extent_item_pos) {
				while (old->next)
					old = old->next;
				old->next = eie;
			}
			eie = NULL;
		}
next:
		if (time_seq == (u64)-1)
			ret = btrfs_next_item(root, path);
		else
			ret = btrfs_next_old_item(root, path, time_seq);
	}

	if (ret > 0)
		ret = 0;
	else if (ret < 0)
		free_inode_elem_list(eie);
	return ret;
}

/*
 * resolve an indirect backref in the form (root_id, key, level)
 * to a logical address
 */
static int __resolve_indirect_ref(struct btrfs_fs_info *fs_info,
				  struct btrfs_path *path, u64 time_seq,
				  struct __prelim_ref *ref,
				  struct ulist *parents,
#ifdef MY_DEF_HERE
				  const u64 *extent_item_pos, enum btrfs_backref_mode mode,
				  int check_first_ref, u64 file_offset,
				  u64 total_refs)
#else
				  const u64 *extent_item_pos, u64 total_refs)
#endif /* MY_DEF_HERE */
{
	struct btrfs_root *root;
	struct btrfs_key root_key;
	struct extent_buffer *eb;
	int ret = 0;
	int root_level;
	int level = ref->level;
	int index;
#ifdef MY_DEF_HERE
	u64 origin_offset = ref->key_for_search.offset;

	/*
	 * We apply workaround here, see __add_prelim_ref for more detail.
	 */
	if (ref->key_for_search.type == BTRFS_EXTENT_DATA_KEY &&
	    ref->key_for_search.offset >= LLONG_MAX)
		ref->key_for_search.offset = 0;
#endif /* MY_DEF_HERE */

	root_key.objectid = ref->root_id;
	root_key.type = BTRFS_ROOT_ITEM_KEY;
	root_key.offset = (u64)-1;

	index = srcu_read_lock(&fs_info->subvol_srcu);

	root = btrfs_get_fs_root(fs_info, &root_key, false);
	if (IS_ERR(root)) {
		srcu_read_unlock(&fs_info->subvol_srcu, index);
		ret = PTR_ERR(root);
		goto out;
	}

	if (btrfs_test_is_dummy_root(root)) {
		srcu_read_unlock(&fs_info->subvol_srcu, index);
		ret = -ENOENT;
		goto out;
	}

	if (path->search_commit_root)
		root_level = btrfs_header_level(root->commit_root);
	else if (time_seq == (u64)-1)
		root_level = btrfs_header_level(root->node);
	else
		root_level = btrfs_old_root_level(root, time_seq);

	if (root_level + 1 == level) {
		srcu_read_unlock(&fs_info->subvol_srcu, index);
		goto out;
	}

	path->lowest_level = level;
	if (time_seq == (u64)-1)
		ret = btrfs_search_slot(NULL, root, &ref->key_for_search, path,
					0, 0);
	else
		ret = btrfs_search_old_slot(root, &ref->key_for_search, path,
					    time_seq);

	/* root node has been locked, we can release @subvol_srcu safely here */
	srcu_read_unlock(&fs_info->subvol_srcu, index);

	pr_debug("search slot in root %llu (level %d, ref count %d) returned "
		 "%d for key (%llu %u %llu)\n",
		 ref->root_id, level, ref->count, ret,
		 ref->key_for_search.objectid, ref->key_for_search.type,
		 ref->key_for_search.offset);
	if (ret < 0)
		goto out;

	eb = path->nodes[level];
	while (!eb) {
		if (WARN_ON(!level)) {
			ret = 1;
			goto out;
		}
		level--;
		eb = path->nodes[level];
	}

#ifdef MY_DEF_HERE
	/*
	 * Reset offset to original value since we need this value to help
	 * us identify if we the EXTENT_DATA key we find is correspondent to
	 * the extent item backref we are processing.
	 */
	ref->key_for_search.offset = origin_offset;
	ret = add_all_parents(root, path, parents, ref, level, time_seq,
			      extent_item_pos, mode, file_offset,
			      check_first_ref, total_refs);
#else
	ret = add_all_parents(root, path, parents, ref, level, time_seq,
			      extent_item_pos, total_refs);
#endif /* MY_DEF_HERE */
out:
	path->lowest_level = 0;
	btrfs_release_path(path);
	return ret;
}

/*
 * resolve all indirect backrefs from the list
 */
static int __resolve_indirect_refs(struct btrfs_fs_info *fs_info,
				   struct btrfs_path *path, u64 time_seq,
				   struct list_head *head,
				   const u64 *extent_item_pos, u64 total_refs,
#ifdef MY_DEF_HERE
				   u64 root_objectid, u64 inum, u64 file_offset, u64 datao,
				   enum btrfs_backref_mode mode)
#else
				   u64 root_objectid)
#endif /* MY_DEF_HERE */
{
	int err;
	int ret = 0;
	struct __prelim_ref *ref;
	struct __prelim_ref *ref_safe;
	struct __prelim_ref *new_ref;
	struct ulist *parents;
	struct ulist_node *node;
	struct ulist_iterator uiter;

	parents = ulist_alloc(GFP_NOFS);
	if (!parents)
		return -ENOMEM;

	/*
	 * _safe allows us to insert directly after the current item without
	 * iterating over the newly inserted items.
	 * we're also allowed to re-assign ref during iteration.
	 */
	list_for_each_entry_safe(ref, ref_safe, head, list) {
#ifdef MY_DEF_HERE
		int check_first_ref = 0;
#endif /* MY_DEF_HERE */
		if (ref->parent)	/* already direct */
			continue;
		if (ref->count == 0)
			continue;
#ifdef MY_DEF_HERE
		if (mode == BTRFS_BACKREF_NORMAL)
#endif /* MY_DEF_HERE */
		if (root_objectid && ref->root_id != root_objectid) {
			ret = BACKREF_FOUND_SHARED;
			goto out;
		}
#ifdef MY_DEF_HERE
		if (mode == BTRFS_BACKREF_FIND_SHARED_ROOT) {
			if (ref->level == 0 && ref->root_id == root_objectid &&
				ref->key_for_search.objectid == inum &&
				ref->key_for_search.offset == file_offset - datao) {
				check_first_ref = 1;
			}
		}
#endif /* MY_DEF_HERE */
		err = __resolve_indirect_ref(fs_info, path, time_seq, ref,
					     parents, extent_item_pos,
#ifdef MY_DEF_HERE
					     mode,
					     check_first_ref, file_offset,
#endif /* MY_DEF_HERE */
					     total_refs);
		/*
		 * we can only tolerate ENOENT,otherwise,we should catch error
		 * and return directly.
		 */
		if (err == -ENOENT) {
			continue;
		} else if (err) {
			ret = err;
			goto out;
		}

		/* we put the first parent into the ref at hand */
		ULIST_ITER_INIT(&uiter);
		node = ulist_next(parents, &uiter);
		ref->parent = node ? node->val : 0;
		ref->inode_list = node ?
			(struct extent_inode_elem *)(uintptr_t)node->aux : NULL;

		/* additional parents require new refs being added here */
		while ((node = ulist_next(parents, &uiter))) {
			new_ref = kmem_cache_alloc(btrfs_prelim_ref_cache,
						   GFP_NOFS);
			if (!new_ref) {
				ret = -ENOMEM;
				goto out;
			}
			memcpy(new_ref, ref, sizeof(*ref));
			new_ref->parent = node->val;
			new_ref->inode_list = (struct extent_inode_elem *)
							(uintptr_t)node->aux;
			list_add(&new_ref->list, &ref->list);
		}
		ulist_reinit(parents);
	}
out:
	ulist_free(parents);
	return ret;
}

static inline int ref_for_same_block(struct __prelim_ref *ref1,
				     struct __prelim_ref *ref2)
{
	if (ref1->level != ref2->level)
		return 0;
	if (ref1->root_id != ref2->root_id)
		return 0;
	if (ref1->key_for_search.type != ref2->key_for_search.type)
		return 0;
	if (ref1->key_for_search.objectid != ref2->key_for_search.objectid)
		return 0;
	if (ref1->key_for_search.offset != ref2->key_for_search.offset)
		return 0;
	if (ref1->parent != ref2->parent)
		return 0;

	return 1;
}

/*
 * read tree blocks and add keys where required.
 */
static int __add_missing_keys(struct btrfs_fs_info *fs_info,
			      struct list_head *head)
{
	struct __prelim_ref *ref;
	struct extent_buffer *eb;

	list_for_each_entry(ref, head, list) {
		if (ref->parent)
			continue;
		if (ref->key_for_search.type)
			continue;
		BUG_ON(!ref->wanted_disk_byte);
		eb = read_tree_block(fs_info->tree_root, ref->wanted_disk_byte,
				     0, ref->level - 1, NULL);
		if (IS_ERR(eb)) {
			return PTR_ERR(eb);
		} else if (!extent_buffer_uptodate(eb)) {
			free_extent_buffer(eb);
			return -EIO;
		}
		btrfs_tree_read_lock(eb);
		if (btrfs_header_level(eb) == 0)
			btrfs_item_key_to_cpu(eb, &ref->key_for_search, 0);
		else
			btrfs_node_key_to_cpu(eb, &ref->key_for_search, 0);
		btrfs_tree_read_unlock(eb);
		free_extent_buffer(eb);
	}
	return 0;
}

/*
 * merge backrefs and adjust counts accordingly
 *
 * mode = 1: merge identical keys, if key is set
 *    FIXME: if we add more keys in __add_prelim_ref, we can merge more here.
 *           additionally, we could even add a key range for the blocks we
 *           looked into to merge even more (-> replace unresolved refs by those
 *           having a parent).
 * mode = 2: merge identical parents
 */
static void __merge_refs(struct list_head *head, int mode)
{
	struct __prelim_ref *pos1;

	list_for_each_entry(pos1, head, list) {
		struct __prelim_ref *pos2 = pos1, *tmp;

		list_for_each_entry_safe_continue(pos2, tmp, head, list) {
			struct __prelim_ref *ref1 = pos1, *ref2 = pos2;
			struct extent_inode_elem *eie;

			if (!ref_for_same_block(ref1, ref2))
				continue;
			if (mode == 1) {
				if (!ref1->parent && ref2->parent)
					swap(ref1, ref2);
			} else {
				if (ref1->parent != ref2->parent)
					continue;
			}

			eie = ref1->inode_list;
			while (eie && eie->next)
				eie = eie->next;
			if (eie)
				eie->next = ref2->inode_list;
			else
				ref1->inode_list = ref2->inode_list;
			ref1->count += ref2->count;

			list_del(&ref2->list);
			kmem_cache_free(btrfs_prelim_ref_cache, ref2);
			cond_resched();
		}

	}
}

/*
 * add all currently queued delayed refs from this head whose seq nr is
 * smaller or equal that seq to the list
 */
static int __add_delayed_refs(struct btrfs_delayed_ref_head *head, u64 seq,
			      struct list_head *prefs, u64 *total_refs,
#ifdef MY_DEF_HERE
			      u64 root_objectid, u64 inum, u64 file_offset,
			      enum btrfs_backref_mode mode)
#else
			      u64 inum)
#endif /* MY_DEF_HERE */
{
	struct btrfs_delayed_ref_node *node;
	struct btrfs_delayed_extent_op *extent_op = head->extent_op;
	struct btrfs_key key;
	struct btrfs_key op_key = {0};
	int sgn;
	int ret = 0;

	if (extent_op && extent_op->update_key)
		btrfs_disk_key_to_cpu(&op_key, &extent_op->key);

	spin_lock(&head->lock);
	list_for_each_entry(node, &head->ref_list, list) {
		if (node->seq > seq)
			continue;

		switch (node->action) {
		case BTRFS_ADD_DELAYED_EXTENT:
		case BTRFS_UPDATE_DELAYED_HEAD:
			WARN_ON(1);
			continue;
		case BTRFS_ADD_DELAYED_REF:
			sgn = 1;
			break;
		case BTRFS_DROP_DELAYED_REF:
			sgn = -1;
			break;
		default:
			BUG_ON(1);
		}
#ifdef MY_DEF_HERE
		if (mode == BTRFS_BACKREF_NORMAL || node->type != BTRFS_EXTENT_DATA_REF_KEY)
#endif /* MY_DEF_HERE */
		*total_refs += (node->ref_mod * sgn);
		switch (node->type) {
		case BTRFS_TREE_BLOCK_REF_KEY: {
			struct btrfs_delayed_tree_ref *ref;

			ref = btrfs_delayed_node_to_tree_ref(node);
			ret = __add_prelim_ref(prefs, ref->root, &op_key,
					       ref->level + 1, 0, node->bytenr,
#ifdef MY_DEF_HERE
					       node->ref_mod * sgn, 0, GFP_ATOMIC);
#else
					       node->ref_mod * sgn, GFP_ATOMIC);
#endif /* MY_DEF_HERE */
			break;
		}
		case BTRFS_SHARED_BLOCK_REF_KEY: {
			struct btrfs_delayed_tree_ref *ref;

			ref = btrfs_delayed_node_to_tree_ref(node);
			ret = __add_prelim_ref(prefs, 0, NULL,
					       ref->level + 1, ref->parent,
					       node->bytenr,
#ifdef MY_DEF_HERE
					       node->ref_mod * sgn, 0, GFP_ATOMIC);
#else
					       node->ref_mod * sgn, GFP_ATOMIC);
#endif /* MY_DEF_HERE */
			break;
		}
		case BTRFS_EXTENT_DATA_REF_KEY: {
			struct btrfs_delayed_data_ref *ref;
			ref = btrfs_delayed_node_to_data_ref(node);

			key.objectid = ref->objectid;
			key.type = BTRFS_EXTENT_DATA_KEY;
			key.offset = ref->offset;

#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET) {
				WARN_ON(!inum || !root_objectid);
				if (key.objectid != inum)
					break;
				if (key.offset < LLONG_MAX && key.offset >= file_offset)
					break;
				/*
				 * Only when we check if an inode has reference to extent_item,
				 * could we break now. Otherwise, we need to run into the
				 * leaf block.
				 */
				if (file_offset == (u64)-1 && ref->root == root_objectid) {
					ret = BACKREF_FOUND_ROOT_INO;
					break;
				}
			} else
#endif /* MY_DEF_HERE */
			/*
			 * Found a inum that doesn't match our known inum, we
			 * know it's shared.
			 */
			if (inum && ref->objectid != inum) {
				ret = BACKREF_FOUND_SHARED;
				break;
			}

			ret = __add_prelim_ref(prefs, ref->root, &key, 0, 0,
					       node->bytenr,
#ifdef MY_DEF_HERE
					       node->ref_mod * sgn, mode, GFP_ATOMIC);
#else
					       node->ref_mod * sgn, GFP_ATOMIC);
#endif /* MY_DEF_HERE */
			break;
		}
		case BTRFS_SHARED_DATA_REF_KEY: {
			struct btrfs_delayed_data_ref *ref;

			ref = btrfs_delayed_node_to_data_ref(node);
#ifdef MY_DEF_HERE
			if (mode != BTRFS_BACKREF_NORMAL)
				*total_refs += (node->ref_mod * sgn);
#endif /* MY_DEF_HERE */
			ret = __add_prelim_ref(prefs, 0, NULL, 0,
					       ref->parent, node->bytenr,
#ifdef MY_DEF_HERE
					       node->ref_mod * sgn, 0, GFP_ATOMIC);
#else
					       node->ref_mod * sgn, GFP_ATOMIC);
#endif /* MY_DEF_HERE */
			break;
		}
		default:
			WARN_ON(1);
		}
		if (ret)
			break;
	}
	spin_unlock(&head->lock);
	return ret;
}

/*
 * add all inline backrefs for bytenr to the list
 */
static int __add_inline_refs(struct btrfs_fs_info *fs_info,
			     struct btrfs_path *path, u64 bytenr,
			     int *info_level, struct list_head *prefs,
#ifdef MY_DEF_HERE
			     struct ulist *roots, u64 *lowest_full_backref,
			     u64 *highest_rootid, u64 *lowest_inum, u64 *lowest_offset,
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
			     struct ref_root *ref_tree,
			     u64 *total_refs, u64 root_objectid,
			     u64 inum, u64 file_offset,
			     enum btrfs_backref_mode mode)
#else
			     struct ref_root *ref_tree,
			     u64 *total_refs, u64 inum)
#endif /* MY_DEF_HERE */
{
	int ret = 0;
	int slot;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	struct btrfs_key found_key;
	unsigned long ptr;
	unsigned long end;
	struct btrfs_extent_item *ei;
	u64 flags;
	u64 item_size;

	/*
	 * enumerate all inline refs
	 */
	leaf = path->nodes[0];
	slot = path->slots[0];

	item_size = btrfs_item_size_nr(leaf, slot);
	BUG_ON(item_size < sizeof(*ei));

	ei = btrfs_item_ptr(leaf, slot, struct btrfs_extent_item);
	flags = btrfs_extent_flags(leaf, ei);
#ifdef MY_DEF_HERE
	if (mode == BTRFS_BACKREF_NORMAL || !(flags & BTRFS_EXTENT_FLAG_DATA))
#endif /* MY_DEF_HERE */
	*total_refs += btrfs_extent_refs(leaf, ei);
	btrfs_item_key_to_cpu(leaf, &found_key, slot);

	ptr = (unsigned long)(ei + 1);
	end = (unsigned long)ei + item_size;

	if (found_key.type == BTRFS_EXTENT_ITEM_KEY &&
	    flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
		struct btrfs_tree_block_info *info;

		info = (struct btrfs_tree_block_info *)ptr;
		*info_level = btrfs_tree_block_level(leaf, info);
		ptr += sizeof(struct btrfs_tree_block_info);
		BUG_ON(ptr > end);
	} else if (found_key.type == BTRFS_METADATA_ITEM_KEY) {
		*info_level = found_key.offset;
	} else {
		BUG_ON(!(flags & BTRFS_EXTENT_FLAG_DATA));
	}

	while (ptr < end) {
		struct btrfs_extent_inline_ref *iref;
		u64 offset;
		int type;

		iref = (struct btrfs_extent_inline_ref *)ptr;
		type = btrfs_extent_inline_ref_type(leaf, iref);
		offset = btrfs_extent_inline_ref_offset(leaf, iref);

		switch (type) {
		case BTRFS_SHARED_BLOCK_REF_KEY:
			ret = __add_prelim_ref(prefs, 0, NULL,
						*info_level + 1, offset,
#ifdef MY_DEF_HERE
						bytenr, 1, 0, GFP_NOFS);
#else
						bytenr, 1, GFP_NOFS);
#endif /* MY_DEF_HERE */
			break;
		case BTRFS_SHARED_DATA_REF_KEY: {
			struct btrfs_shared_data_ref *sdref;
			int count;

#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_FIND_SHARED_ROOT &&
			    *lowest_full_backref > offset)
				*lowest_full_backref = offset;
#endif /* MY_DEF_HERE */
			sdref = (struct btrfs_shared_data_ref *)(iref + 1);
			count = btrfs_shared_data_ref_count(leaf, sdref);
#ifdef MY_DEF_HERE
			if (mode != BTRFS_BACKREF_NORMAL)
				*total_refs += count;
			ret = __add_prelim_ref(prefs, 0, NULL, 0, offset,
					       bytenr, count, 0, GFP_NOFS);
#else
			ret = __add_prelim_ref(prefs, 0, NULL, 0, offset,
					       bytenr, count, GFP_NOFS);
#endif /* MY_DEF_HERE */
			if (ref_tree) {
				if (!ret)
					ret = ref_tree_add(ref_tree, 0, 0, 0,
							   bytenr, count);
				if (!ret && ref_tree->unique_refs > 1)
					ret = BACKREF_FOUND_SHARED;
			}
			break;
		}
		case BTRFS_TREE_BLOCK_REF_KEY:
#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_FIND_SHARED_ROOT &&
				!ulist_search(roots, offset)) {
				ret = BACKREF_FOUND_SHARED_ROOT;
				break;
			}
#endif /* MY_DEF_HERE */
			ret = __add_prelim_ref(prefs, offset, NULL,
					       *info_level + 1, 0,
#ifdef MY_DEF_HERE
					       bytenr, 1, 0, GFP_NOFS);
#else
					       bytenr, 1, GFP_NOFS);
#endif /* MY_DEF_HERE */
			break;
		case BTRFS_EXTENT_DATA_REF_KEY: {
			struct btrfs_extent_data_ref *dref;
			int count;
			u64 root;

			dref = (struct btrfs_extent_data_ref *)(&iref->offset);
			count = btrfs_extent_data_ref_count(leaf, dref);
			key.objectid = btrfs_extent_data_ref_objectid(leaf,
								      dref);
			key.type = BTRFS_EXTENT_DATA_KEY;
			key.offset = btrfs_extent_data_ref_offset(leaf, dref);

#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_NORMAL)
#endif /* MY_DEF_HERE */
			if (inum && key.objectid != inum) {
				ret = BACKREF_FOUND_SHARED;
				break;
			}

			root = btrfs_extent_data_ref_root(leaf, dref);
#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_FIND_SHARED_ROOT) {
				WARN_ON(!root_objectid || !inum);
				if (!ulist_search(roots, root)) {
					ret = BACKREF_FOUND_SHARED_ROOT;
					break;
				}
				if (*highest_rootid < root ||
					(*highest_rootid == root && *lowest_inum > key.objectid) ||
					(*highest_rootid == root && *lowest_inum == key.objectid &&
					 *lowest_offset > key.offset)) {
					*highest_rootid = root;
					*lowest_inum = key.objectid;
					*lowest_offset = key.offset;
				}
			}
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET) {
				WARN_ON(!inum || !root_objectid);
				if (key.objectid != inum)
					break;
				if (key.offset < LLONG_MAX && key.offset >= file_offset)
					break;
				/*
				 * Only when we check if an inode has reference to extent_item,
				 * could we break now. Otherwise, we need to run into the
				 * leaf block.
				 */
				if (file_offset == (u64)-1 && root == root_objectid) {
					ret = BACKREF_FOUND_ROOT_INO;
					break;
				}
			}
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
			ret = __add_prelim_ref(prefs, root, &key, 0, 0,
					       bytenr, count, mode, GFP_NOFS);
#else
			ret = __add_prelim_ref(prefs, root, &key, 0, 0,
					       bytenr, count, GFP_NOFS);
#endif /* MY_DEF_HERE */
			if (ref_tree) {
				if (!ret)
					ret = ref_tree_add(ref_tree, root,
							   key.objectid,
							   key.offset, 0,
							   count);
				if (!ret && ref_tree->unique_refs > 1)
					ret = BACKREF_FOUND_SHARED;
			}
			break;
		}
		default:
			WARN_ON(1);
		}
		if (ret)
			return ret;
		ptr += btrfs_extent_inline_ref_size(type);
	}

	return 0;
}

/*
 * add all non-inline backrefs for bytenr to the list
 */
static int __add_keyed_refs(struct btrfs_fs_info *fs_info,
			    struct btrfs_path *path, u64 bytenr,
#ifdef MY_DEF_HERE
			    struct ulist *roots, u64 *lowest_full_backref,
			    u64 *highest_rootid, u64 *lowest_inum, u64 *lowest_offset,
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
			    int info_level, struct list_head *prefs,
			    u64 *total_refs, u64 root_objectid,
			    struct ref_root *ref_tree, u64 inum, u64 file_offset,
                enum btrfs_backref_mode mode)
#else
			    int info_level, struct list_head *prefs,
			    struct ref_root *ref_tree, u64 inum)
#endif /* MY_DEF_HERE */
{
	struct btrfs_root *extent_root = fs_info->extent_root;
	int ret;
	int slot;
	struct extent_buffer *leaf;
	struct btrfs_key key;

	while (1) {
		ret = btrfs_next_item(extent_root, path);
		if (ret < 0)
			break;
		if (ret) {
			ret = 0;
			break;
		}

		slot = path->slots[0];
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &key, slot);

		if (key.objectid != bytenr)
			break;
		if (key.type < BTRFS_TREE_BLOCK_REF_KEY)
			continue;
		if (key.type > BTRFS_SHARED_DATA_REF_KEY)
			break;

		switch (key.type) {
		case BTRFS_SHARED_BLOCK_REF_KEY:
			ret = __add_prelim_ref(prefs, 0, NULL,
						info_level + 1, key.offset,
#ifdef MY_DEF_HERE
						bytenr, 1, 0, GFP_NOFS);
#else
						bytenr, 1, GFP_NOFS);
#endif /* MY_DEF_HERE */
			break;
		case BTRFS_SHARED_DATA_REF_KEY: {
			struct btrfs_shared_data_ref *sdref;
			int count;

#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_FIND_SHARED_ROOT &&
			    *lowest_full_backref > key.offset)
				*lowest_full_backref = key.offset;
#endif /* MY_DEF_HERE */
			sdref = btrfs_item_ptr(leaf, slot,
					      struct btrfs_shared_data_ref);
			count = btrfs_shared_data_ref_count(leaf, sdref);
#ifdef MY_DEF_HERE
			if (mode != BTRFS_BACKREF_NORMAL)
				*total_refs += count;
			ret = __add_prelim_ref(prefs, 0, NULL, 0, key.offset,
						bytenr, count, 0, GFP_NOFS);
#else
			ret = __add_prelim_ref(prefs, 0, NULL, 0, key.offset,
						bytenr, count, GFP_NOFS);
#endif /* MY_DEF_HERE */
			if (ref_tree) {
				if (!ret)
					ret = ref_tree_add(ref_tree, 0, 0, 0,
							   bytenr, count);
				if (!ret && ref_tree->unique_refs > 1)
					ret = BACKREF_FOUND_SHARED;
			}
			break;
		}
		case BTRFS_TREE_BLOCK_REF_KEY:
#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_FIND_SHARED_ROOT &&
				!ulist_search(roots, key.offset)) {
				ret = BACKREF_FOUND_SHARED_ROOT;
				break;
			}
#endif /* MY_DEF_HERE */
			ret = __add_prelim_ref(prefs, key.offset, NULL,
					       info_level + 1, 0,
#ifdef MY_DEF_HERE
					       bytenr, 1, 0, GFP_NOFS);
#else
					       bytenr, 1, GFP_NOFS);
#endif /* MY_DEF_HERE */
			break;
		case BTRFS_EXTENT_DATA_REF_KEY: {
			struct btrfs_extent_data_ref *dref;
			int count;
			u64 root;

			dref = btrfs_item_ptr(leaf, slot,
					      struct btrfs_extent_data_ref);
			count = btrfs_extent_data_ref_count(leaf, dref);
			key.objectid = btrfs_extent_data_ref_objectid(leaf,
								      dref);
			key.type = BTRFS_EXTENT_DATA_KEY;
			key.offset = btrfs_extent_data_ref_offset(leaf, dref);

#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_NORMAL)
#endif /* MY_DEF_HERE */
			if (inum && key.objectid != inum) {
				ret = BACKREF_FOUND_SHARED;
				break;
			}

			root = btrfs_extent_data_ref_root(leaf, dref);
#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_FIND_SHARED_ROOT) {
				WARN_ON(!root_objectid || !inum);
				if (!ulist_search(roots, root)) {
					ret = BACKREF_FOUND_SHARED_ROOT;
					break;
				}
				if (*highest_rootid < root ||
					(*highest_rootid == root && *lowest_inum > key.objectid) ||
					(*highest_rootid == root && *lowest_inum == key.objectid &&
					 *lowest_offset > key.offset)) {
					*highest_rootid = root;
					*lowest_inum = key.objectid;
					*lowest_offset = key.offset;
				}
			}
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET) {
				WARN_ON(!inum || !root_objectid);
				if (key.objectid != inum)
					break;
				if (key.offset < LLONG_MAX && key.offset >= file_offset)
					break;
				/*
				 * Only when we check if an inode has reference to extent_item,
				 * could we break now. Otherwise, we need to run into the
				 * leaf block.
				 */
				if (file_offset == (u64)-1 && root == root_objectid) {
					ret = BACKREF_FOUND_ROOT_INO;
					break;
				}
			}
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
			ret = __add_prelim_ref(prefs, root, &key, 0, 0,
					       bytenr, count, mode, GFP_NOFS);
#else
			ret = __add_prelim_ref(prefs, root, &key, 0, 0,
					       bytenr, count, GFP_NOFS);
#endif /* MY_DEF_HERE */
			if (ref_tree) {
				if (!ret)
					ret = ref_tree_add(ref_tree, root,
							   key.objectid,
							   key.offset, 0,
							   count);
				if (!ret && ref_tree->unique_refs > 1)
					ret = BACKREF_FOUND_SHARED;
			}
			break;
		}
		default:
			WARN_ON(1);
		}
		if (ret)
			return ret;

	}

	return ret;
}

#ifdef MY_DEF_HERE
static int check_first_ref(struct extent_buffer *eb, u64 bytenr,
		  u64 inum, u64 file_offset)
{
	u64 disk_byte;
	struct btrfs_key key;
	struct btrfs_file_extent_item *fi;
	int slot;
	int nritems;
	int extent_type;

	/*
	 * from the shared data ref, we only have the leaf but we need
	 * the key. thus, we must look into all items and see that we
	 * find one (some) with a reference to our extent item.
	 */
	nritems = btrfs_header_nritems(eb);
	for (slot = 0; slot < nritems; ++slot) {
		btrfs_item_key_to_cpu(eb, &key, slot);
		if (key.type != BTRFS_EXTENT_DATA_KEY)
			continue;
		fi = btrfs_item_ptr(eb, slot, struct btrfs_file_extent_item);
		extent_type = btrfs_file_extent_type(eb, fi);
		if (extent_type == BTRFS_FILE_EXTENT_INLINE)
			continue;
		/* don't skip BTRFS_FILE_EXTENT_PREALLOC, we can handle that */
		disk_byte = btrfs_file_extent_disk_bytenr(eb, fi);
		if (disk_byte != bytenr)
			continue;

		if (key.objectid < inum ||
		    (key.objectid == inum && key.offset < file_offset))
			return 0;
	}

	return 1;
}

/*
 * Copy from find_parent_nodes
 */
static int find_parent_nodes_shared_root(struct btrfs_fs_info *fs_info,
			     u64 bytenr, u64 parent_bytenr, u64 datao,
			     struct ulist *refs, struct ulist *roots,
			     u64 root_objectid, u64 inum, u64 offset,
			     u64 *counted_root)
{
	struct btrfs_key key;
	struct btrfs_path *path;
	int info_level = 0;
	int ret;
	struct list_head prefs;
	struct __prelim_ref *ref;
	u64 total_refs = 0;
	/*
	 * We record the following record:
	 * 1. smallest full backref bytenr
	 * 2. smallest offset of smallest inode number in largest root
	 *
	 * If both records are available, we use smallest full backref bytenr.
	 * This is used to make sure we only account EXTENT once.
	 * We only take this extent into account if this path contains the record.
	 *
	 * The reason why we use largest rootid but smallest inode number
	 * and offset is we want to use cumulative accounting. Therefore,
	 * larget root id is needed. Using smallest offset is
	 * to make resolve indirect reference work faster. In resolving
	 * indirect ref, we always start from smaller offset of inode number.
	 * If the behavior of root id changes(e.g. from largest to smallest),
	 * make sure to change the function snap_entry_insert in ctree.c
	 */
	u64 lowest_full_backref = (u64)-1;
	u64 highest_rootid = 0;
	u64 lowest_inum = (u64)-1;
	u64 lowest_offset = (u64)-1;
	enum btrfs_backref_mode mode = BTRFS_BACKREF_FIND_SHARED_ROOT;

	INIT_LIST_HEAD(&prefs);

	key.objectid = bytenr;
	key.offset = (u64)-1;
	if (btrfs_fs_incompat(fs_info, SKINNY_METADATA))
		key.type = BTRFS_METADATA_ITEM_KEY;
	else
		key.type = BTRFS_EXTENT_ITEM_KEY;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	path->search_commit_root = 1;
	path->skip_locking = 1;

	ret = btrfs_search_slot(NULL, fs_info->extent_root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	BUG_ON(ret == 0);

	if (path->slots[0]) {
		struct extent_buffer *leaf;
		int slot;

		path->slots[0]--;
		leaf = path->nodes[0];
		slot = path->slots[0];
		btrfs_item_key_to_cpu(leaf, &key, slot);
		if (key.objectid == bytenr &&
		    (key.type == BTRFS_EXTENT_ITEM_KEY ||
		     key.type == BTRFS_METADATA_ITEM_KEY)) {
			ret = __add_inline_refs(fs_info, path, bytenr,
						&info_level, &prefs,
						roots, &lowest_full_backref,
						&highest_rootid, &lowest_inum, &lowest_offset,
						0, &total_refs, root_objectid,
						inum, (u64)-1, mode);
			if (ret)
				goto out;
			ret = __add_keyed_refs(fs_info, path, bytenr,
					       roots, &lowest_full_backref,
					       &highest_rootid, &lowest_inum, &lowest_offset,
					       info_level, &prefs,
					       &total_refs, root_objectid,
					       0, inum, (u64)-1, mode);
			if (ret)
				goto out;
			if (key.type == BTRFS_EXTENT_ITEM_KEY) {
				if (lowest_full_backref != (u64)-1) {
					if (parent_bytenr != lowest_full_backref) {
						ret = BACKREF_NEXT_ITEM;
						goto out;
					}
				} else if (highest_rootid != 0) {
					if (highest_rootid != root_objectid || lowest_inum != inum ||
						lowest_offset != offset - datao) {
						ret = BACKREF_NEXT_ITEM;
						goto out;
					}
				}
			}
		}
	}
	btrfs_release_path(path);

	ret = __add_missing_keys(fs_info, &prefs);
	if (ret)
		goto out;

	__merge_refs(&prefs, 1);

	WARN_ON(!path->search_commit_root);
	/*
	 * if lowest_full_backref is not set, we know that this EXTENT_ITEM for data
	 * only has implicit backref, and we need to check first ref case here.
	 */
	ret = __resolve_indirect_refs(fs_info, path, 0, &prefs,
				      NULL, total_refs,
				      lowest_full_backref == (u64) -1 ? root_objectid : 0,
				      inum, offset, datao, mode);
	if (ret)
		goto out;

	__merge_refs(&prefs, 2);

	while (!list_empty(&prefs)) {
		ref = list_first_entry(&prefs, struct __prelim_ref, list);
		WARN_ON(ref->count < 0);
		if (roots && ref->count && ref->root_id && ref->parent == 0) {
			if (!ulist_search(roots, ref->root_id)) {
				ret = BACKREF_FOUND_SHARED_ROOT;
				goto out;
			}
			if (counted_root && ref->root_id > *counted_root)
				*counted_root = ref->root_id;
		}
		if (ref->count && ref->parent) {
			if (ref->level == 0 &&
			    ref->key_for_search.type == 0 &&
				parent_bytenr == ref->parent) {
				/*
				 * The reason to add parent_bytenr == ref->parent condition here:
				 * If the parent bytenr is the smallest bytenr among all
				 * the full backrefs, and this leaf contains four EXTENT_DATA
				 * pointing to this EXTENT_ITEM.
				 * We will go into this check shared four times, so only check
				 * this EXTENT_ITEM for the first time pointing to the EXTENT_ITEM
				 * in this leaf. Otherwise we'll account four times here.
				 */
				struct extent_buffer *eb;
				eb = read_tree_block(fs_info->extent_root,
							   ref->parent, 0, ref->level, NULL);
				if (IS_ERR(eb)) {
					ret = PTR_ERR(eb);
					goto out;
				} else if (!extent_buffer_uptodate(eb)) {
					free_extent_buffer(eb);
					ret = -EIO;
					goto out;
				}
				btrfs_tree_read_lock(eb);
				btrfs_set_lock_blocking_rw(eb, BTRFS_READ_LOCK);
				ret = check_first_ref(eb, bytenr, inum, offset);
				btrfs_tree_read_unlock_blocking(eb);
				free_extent_buffer(eb);
				if (!ret) {
					ret = BACKREF_NEXT_ITEM;
					goto out;
				}
			}
			/*
			 * When dealing with an EXTENT_ITEM, we only account data that is
			 * referenced from the lowest block bytenr(or smallest offset of
			 * smallest inode in the largest root id if there's no full backref)
			 * to avoid counting one extent more than once. Therefore, we need
			 * to go through check_first_ref for every EXTENT_DATA. After
			 * passing that, we can safely check parent bytenr to skip checking
			 * backref on parent, which we know is not shared.
			 */
			if (parent_bytenr && ref->parent == parent_bytenr)
				goto skip_ref;
			ret = ulist_add(refs, ref->parent, 0, GFP_NOFS);
			if (ret < 0)
				goto out;
		}
skip_ref:
		list_del(&ref->list);
		kmem_cache_free(btrfs_prelim_ref_cache, ref);
	}

out:
	btrfs_free_path(path);
	while (!list_empty(&prefs)) {
		ref = list_first_entry(&prefs, struct __prelim_ref, list);
		list_del(&ref->list);
		kmem_cache_free(btrfs_prelim_ref_cache, ref);
	}
	return ret;
}
#endif /* MY_DEF_HERE */

/*
 * this adds all existing backrefs (inline backrefs, backrefs and delayed
 * refs) for the given bytenr to the refs list, merges duplicates and resolves
 * indirect refs to their parent bytenr.
 * When roots are found, they're added to the roots list
 *
 * NOTE: This can return values > 0
 *
 * If time_seq is set to (u64)-1, it will not search delayed_refs, and behave
 * much like trans == NULL case, the difference only lies in it will not
 * commit root.
 * The special case is for qgroup to search roots in commit_transaction().

 * If check_shared is set to 1, any extent has more than one ref item, will
 * be returned BACKREF_FOUND_SHARED immediately.
 *
 * FIXME some caching might speed things up
 */
static int find_parent_nodes(struct btrfs_trans_handle *trans,
			     struct btrfs_fs_info *fs_info, u64 bytenr,
			     u64 time_seq, struct ulist *refs,
			     struct ulist *roots, const u64 *extent_item_pos,
#ifdef MY_DEF_HERE
			     u64 datao, u64 root_objectid, u64 inum, u64 offset,
			     int check_shared, enum btrfs_backref_mode mode,
			     int in_run_delayed)
#else
			     u64 root_objectid, u64 inum, int check_shared)
#endif /* MY_DEF_HERE */
{
	struct btrfs_key key;
	struct btrfs_path *path;
	struct btrfs_delayed_ref_root *delayed_refs = NULL;
	struct btrfs_delayed_ref_head *head;
	int info_level = 0;
	int ret;
	struct list_head prefs_delayed;
	struct list_head prefs;
	struct __prelim_ref *ref;
	struct extent_inode_elem *eie = NULL;
	struct ref_root *ref_tree = NULL;
	u64 total_refs = 0;

	INIT_LIST_HEAD(&prefs);
	INIT_LIST_HEAD(&prefs_delayed);

	key.objectid = bytenr;
	key.offset = (u64)-1;
	if (btrfs_fs_incompat(fs_info, SKINNY_METADATA))
		key.type = BTRFS_METADATA_ITEM_KEY;
	else
		key.type = BTRFS_EXTENT_ITEM_KEY;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	if (!trans) {
		path->search_commit_root = 1;
		path->skip_locking = 1;
	}

	if (time_seq == (u64)-1)
		path->skip_locking = 1;

	/*
	 * grab both a lock on the path and a lock on the delayed ref head.
	 * We need both to get a consistent picture of how the refs look
	 * at a specified point in time
	 */
again:
	head = NULL;

	if (check_shared) {
		if (!ref_tree) {
			ref_tree = ref_root_alloc();
			if (!ref_tree) {
				ret = -ENOMEM;
				goto out;
			}
		} else {
			ref_root_fini(ref_tree);
		}
	}

	ret = btrfs_search_slot(trans, fs_info->extent_root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	if (ret == 0) {
		/* This shouldn't happen, indicates a bug or fs corruption. */
		ASSERT(ret != 0);
		ret = -EUCLEAN;
		goto out;
	}

#ifdef CONFIG_BTRFS_FS_RUN_SANITY_TESTS
	if (trans && likely(trans->type != __TRANS_DUMMY) &&
	    time_seq != (u64)-1) {
#else
	if (trans && time_seq != (u64)-1) {
#endif
		/*
		 * look if there are updates for this ref queued and lock the
		 * head
		 */
		delayed_refs = &trans->transaction->delayed_refs;
		spin_lock(&delayed_refs->lock);
		head = btrfs_find_delayed_ref_head(delayed_refs, bytenr);
		if (head) {
#ifdef MY_DEF_HERE
			if (in_run_delayed) {
				/*
				 * We are currently running this delayed reference and
				 * hold the lock already, so skip loccking phase.
				 */
			} else
#endif /* MY_DEF_HERE */
			if (!mutex_trylock(&head->mutex)) {
				atomic_inc(&head->node.refs);
				spin_unlock(&delayed_refs->lock);

				btrfs_release_path(path);

				/*
				 * Mutex was contended, block until it's
				 * released and try again
				 */
				mutex_lock(&head->mutex);
				mutex_unlock(&head->mutex);
				btrfs_put_delayed_ref(&head->node);
				goto again;
			}
			spin_unlock(&delayed_refs->lock);
			ret = __add_delayed_refs(head, time_seq,
						 &prefs_delayed, &total_refs,
#ifdef MY_DEF_HERE
						 root_objectid, inum, offset, mode);
			if (!in_run_delayed)
#else
						 inum);
#endif /* MY_DEF_HERE */
			mutex_unlock(&head->mutex);
			if (ret)
				goto out;
		} else {
			spin_unlock(&delayed_refs->lock);
		}

		if (check_shared && !list_empty(&prefs_delayed)) {
			/*
			 * Add all delay_ref to the ref_tree and check if there
			 * are multiple ref items added.
			 */
			list_for_each_entry(ref, &prefs_delayed, list) {
				if (ref->key_for_search.type) {
					ret = ref_tree_add(ref_tree,
						ref->root_id,
						ref->key_for_search.objectid,
						ref->key_for_search.offset,
						0, ref->count);
					if (ret)
						goto out;
				} else {
					ret = ref_tree_add(ref_tree, 0, 0, 0,
						     ref->parent, ref->count);
					if (ret)
						goto out;
				}

			}

			if (ref_tree->unique_refs > 1) {
				ret = BACKREF_FOUND_SHARED;
				goto out;
			}

		}
	}

	if (path->slots[0]) {
		struct extent_buffer *leaf;
		int slot;

		path->slots[0]--;
		leaf = path->nodes[0];
		slot = path->slots[0];
		btrfs_item_key_to_cpu(leaf, &key, slot);
		if (key.objectid == bytenr &&
		    (key.type == BTRFS_EXTENT_ITEM_KEY ||
		     key.type == BTRFS_METADATA_ITEM_KEY)) {
			ret = __add_inline_refs(fs_info, path, bytenr,
						&info_level, &prefs,
#ifdef MY_DEF_HERE
						NULL, NULL, NULL, NULL, NULL,
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
						ref_tree,
						&total_refs, root_objectid,
						inum, offset, mode);
#else
						ref_tree, &total_refs,
						inum);
#endif /* MY_DEF_HERE */
			if (ret)
				goto out;
			ret = __add_keyed_refs(fs_info, path, bytenr,
#ifdef MY_DEF_HERE
					       NULL, NULL, NULL, NULL, NULL,
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
					       info_level, &prefs, &total_refs,
					       root_objectid, ref_tree, inum, offset, mode);
#else
					       info_level, &prefs,
					       ref_tree, inum);
#endif /* MY_DEF_HERE */
			if (ret)
				goto out;
		}
	}
	btrfs_release_path(path);

	list_splice_init(&prefs_delayed, &prefs);

	ret = __add_missing_keys(fs_info, &prefs);
	if (ret)
		goto out;

	__merge_refs(&prefs, 1);

	ret = __resolve_indirect_refs(fs_info, path, time_seq, &prefs,
				      extent_item_pos, total_refs,
#ifdef MY_DEF_HERE
				      root_objectid,
				      inum, offset, datao,
				      mode);
#else
				      root_objectid);
#endif /* MY_DEF_HERE */
	if (ret)
		goto out;

	__merge_refs(&prefs, 2);

	while (!list_empty(&prefs)) {
		ref = list_first_entry(&prefs, struct __prelim_ref, list);
		WARN_ON(ref->count < 0);
		if (roots && ref->count && ref->root_id && ref->parent == 0) {
#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET) {
				WARN_ON(!root_objectid);
				if (ref->root_id == root_objectid) {
					ret = BACKREF_FOUND_ROOT_INO;
					goto out;
				}
			} else {
#endif /* MY_DEF_HERE */
			if (root_objectid && ref->root_id != root_objectid) {
				ret = BACKREF_FOUND_SHARED;
				goto out;
			}

			/* no parent == root of tree */
			ret = ulist_add(roots, ref->root_id, 0, GFP_NOFS);
			if (ret < 0)
				goto out;
#ifdef MY_DEF_HERE
			} // mode != BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET
#endif /* MY_DEF_HERE */
		}
		if (ref->count && ref->parent) {
			if (extent_item_pos && !ref->inode_list &&
			    ref->level == 0) {
				struct extent_buffer *eb;

				eb = read_tree_block(fs_info->extent_root,
							   ref->parent, 0, ref->level, NULL);
				if (IS_ERR(eb)) {
					ret = PTR_ERR(eb);
					goto out;
				} else if (!extent_buffer_uptodate(eb)) {
					free_extent_buffer(eb);
					ret = -EIO;
					goto out;
				}
				btrfs_tree_read_lock(eb);
				btrfs_set_lock_blocking_rw(eb, BTRFS_READ_LOCK);
				ret = find_extent_in_eb(eb, bytenr,
							*extent_item_pos, &eie);
				btrfs_tree_read_unlock_blocking(eb);
				free_extent_buffer(eb);
				if (ret < 0)
					goto out;
				ref->inode_list = eie;
			}
#ifdef MY_DEF_HERE
			if (mode == BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET && ref->level == 0 &&
			    ref->key_for_search.type == 0) {
				struct extent_buffer *eb;
				eb = read_tree_block(fs_info->extent_root,
							    ref->parent, 0, ref->level, NULL);
				if (IS_ERR(eb)) {
					ret = PTR_ERR(eb);
					goto out;
				} else if (!extent_buffer_uptodate(eb)) {
					free_extent_buffer(eb);
					ret = -EIO;
					goto out;
				}
				btrfs_tree_read_lock(eb);
				btrfs_set_lock_blocking_rw(eb, BTRFS_READ_LOCK);
				ret = find_ino_extent_in_eb(eb, bytenr, inum, offset);
				btrfs_tree_read_unlock_blocking(eb);
				free_extent_buffer(eb);
				if (!ret)
					goto next;
			}
#endif /* MY_DEF_HERE */
			ret = ulist_add_merge_ptr(refs, ref->parent,
						  ref->inode_list,
						  (void **)&eie, GFP_NOFS);
			if (ret < 0)
				goto out;
			if (!ret && extent_item_pos) {
				/*
				 * We've recorded that parent, so we must extend
				 * its inode list here.
				 *
				 * However if there was corruption we may not
				 * have found an eie, return an error in this
				 * case.
				 */
				ASSERT(eie);
				if (!eie) {
					ret = -EUCLEAN;
					goto out;
				}
				while (eie->next)
					eie = eie->next;
				eie->next = ref->inode_list;
			}
			eie = NULL;
		}
#ifdef MY_DEF_HERE
next:
#endif /* MY_DEF_HERE */
		list_del(&ref->list);
		kmem_cache_free(btrfs_prelim_ref_cache, ref);
	}

out:
	btrfs_free_path(path);
	ref_root_free(ref_tree);
	while (!list_empty(&prefs)) {
		ref = list_first_entry(&prefs, struct __prelim_ref, list);
		list_del(&ref->list);
		kmem_cache_free(btrfs_prelim_ref_cache, ref);
	}
	while (!list_empty(&prefs_delayed)) {
		ref = list_first_entry(&prefs_delayed, struct __prelim_ref,
				       list);
		list_del(&ref->list);
		kmem_cache_free(btrfs_prelim_ref_cache, ref);
	}
	if (ret < 0)
		free_inode_elem_list(eie);
	return ret;
}

static void free_leaf_list(struct ulist *blocks)
{
	struct ulist_node *node = NULL;
	struct extent_inode_elem *eie;
	struct ulist_iterator uiter;

	ULIST_ITER_INIT(&uiter);
	while ((node = ulist_next(blocks, &uiter))) {
		if (!node->aux)
			continue;
		eie = (struct extent_inode_elem *)(uintptr_t)node->aux;
		free_inode_elem_list(eie);
		node->aux = 0;
	}

	ulist_free(blocks);
}

/*
 * Finds all leafs with a reference to the specified combination of bytenr and
 * offset. key_list_head will point to a list of corresponding keys (caller must
 * free each list element). The leafs will be stored in the leafs ulist, which
 * must be freed with ulist_free.
 *
 * returns 0 on success, <0 on error
 */
static int btrfs_find_all_leafs(struct btrfs_trans_handle *trans,
				struct btrfs_fs_info *fs_info, u64 bytenr,
				u64 time_seq, struct ulist **leafs,
				const u64 *extent_item_pos)
{
	int ret;

	*leafs = ulist_alloc(GFP_NOFS);
	if (!*leafs)
		return -ENOMEM;

	ret = find_parent_nodes(trans, fs_info, bytenr, time_seq,
#ifdef MY_DEF_HERE
				*leafs, NULL, extent_item_pos, 0, 0, 0, (u64)-1, 0,
				BTRFS_BACKREF_NORMAL, 0);
#else
				*leafs, NULL, extent_item_pos, 0, 0, 0);
				time_seq, *leafs, NULL, extent_item_pos, 0, 0);
#endif /* MY_DEF_HERE */
	if (ret < 0 && ret != -ENOENT) {
		free_leaf_list(*leafs);
		return ret;
	}

	return 0;
}

/*
 * walk all backrefs for a given extent to find all roots that reference this
 * extent. Walking a backref means finding all extents that reference this
 * extent and in turn walk the backrefs of those, too. Naturally this is a
 * recursive process, but here it is implemented in an iterative fashion: We
 * find all referencing extents for the extent in question and put them on a
 * list. In turn, we find all referencing extents for those, further appending
 * to the list. The way we iterate the list allows adding more elements after
 * the current while iterating. The process stops when we reach the end of the
 * list. Found roots are added to the roots list.
 *
 * returns 0 on success, < 0 on error.
 */
static int __btrfs_find_all_roots(struct btrfs_trans_handle *trans,
				  struct btrfs_fs_info *fs_info, u64 bytenr,
#ifdef MY_DEF_HERE
				  u64 time_seq, struct ulist **roots,
				  u64 root_objectid, enum btrfs_backref_mode mode)
#else
				  u64 time_seq, struct ulist **roots)
#endif /* MY_DEF_HERE */
{
	struct ulist *tmp;
	struct ulist_node *node = NULL;
	struct ulist_iterator uiter;
	int ret;

	tmp = ulist_alloc(GFP_NOFS);
	if (!tmp)
		return -ENOMEM;
	*roots = ulist_alloc(GFP_NOFS);
	if (!*roots) {
		ulist_free(tmp);
		return -ENOMEM;
	}

	ULIST_ITER_INIT(&uiter);
	while (1) {
		ret = find_parent_nodes(trans, fs_info, bytenr, time_seq,
#ifdef MY_DEF_HERE
					tmp, *roots, NULL, 0,
#ifdef MY_DEF_HERE
					mode == BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET ?
					root_objectid : 0,
					0, (u64)-1, 0, mode, 0);
		if (mode == BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET &&
		    ret == BACKREF_FOUND_ROOT_INO) {
			ulist_free(tmp);
			ulist_free(*roots);
			*roots = NULL;
			return ret;
		}
#else
					0, 0, (u64)-1, 0, mode, 0);
#endif /* MY_DEF_HERE */
#else
					tmp, *roots, NULL, 0, 0, 0);
#endif /* MY_DEF_HERE */
		if (ret < 0 && ret != -ENOENT) {
			ulist_free(tmp);
			ulist_free(*roots);
			*roots = NULL;
			return ret;
		}
		node = ulist_next(tmp, &uiter);
		if (!node)
			break;
		bytenr = node->val;
		cond_resched();
	}

	ulist_free(tmp);
	return 0;
}

int btrfs_find_all_roots(struct btrfs_trans_handle *trans,
			 struct btrfs_fs_info *fs_info, u64 bytenr,
			 u64 time_seq, struct ulist **roots)
{
	int ret;

	if (!trans)
		down_read(&fs_info->commit_root_sem);
#ifdef MY_DEF_HERE
	ret = __btrfs_find_all_roots(trans, fs_info, bytenr, time_seq, roots, 0, 0);
#else
	ret = __btrfs_find_all_roots(trans, fs_info, bytenr, time_seq, roots);
#endif /* MY_DEF_HERE */
	if (!trans)
		up_read(&fs_info->commit_root_sem);
	return ret;
}

#ifdef MY_DEF_HERE
/*
 * Copy from __btrfs_find_all_roots
 */
static int __btrfs_find_all_roots_shared(struct btrfs_fs_info *fs_info,
				  u64 bytenr, u64 parent_bytenr,
				  u64 datao, struct ulist *roots,
				  u64 root_id, u64 inum, u64 file_offset,
				  u64 *counted_root)
{
	struct ulist *tmp;
	struct ulist_node *node = NULL;
	struct ulist_iterator uiter;
	int ret = 0;

	tmp = ulist_alloc(GFP_NOFS);
	if (!tmp)
		return -ENOMEM;

	ULIST_ITER_INIT(&uiter);
	while (1) {
		ret = find_parent_nodes_shared_root(fs_info, bytenr,
					parent_bytenr, datao, tmp, roots,
					root_id, inum, file_offset, counted_root);
		if (ret == BACKREF_NEXT_ITEM || ret == BACKREF_FOUND_SHARED_ROOT) {
			ulist_free(tmp);
			return ret;
		}
		if (ret < 0 && ret != -ENOENT) {
			ulist_free(tmp);
			return ret;
		}

		node = ulist_next(tmp, &uiter);
		if (!node)
			break;
		bytenr = node->val;
		cond_resched();
	}

	ulist_free(tmp);
	return 0;
}

/*
 * Here's how we work, we only calculate the extent data from lowest bytenr node if
 * full backref is presented. Otherwise, extent from lowest file offset of
 * lowest inode number in largest subvolume id is counted.
 */
int btrfs_find_shared_root(struct btrfs_fs_info *fs_info,
			 u64 bytenr, u64 parent_bytenr, u64 datao, u64 *counted_root,
			 struct ulist *root_list, struct btrfs_snapshot_size_entry *entry,
			 struct btrfs_snapshot_size_ctx *ctx)
{
	int ret;

	down_read(&fs_info->commit_root_sem);
	ret = __btrfs_find_all_roots_shared(fs_info, bytenr, parent_bytenr, datao,
						root_list, entry->root_id, entry->key.objectid,
						entry->key.offset, counted_root);
	up_read(&fs_info->commit_root_sem);

	WARN_ON(ret > 0 && ret != BACKREF_NEXT_ITEM && ret != BACKREF_FOUND_SHARED_ROOT);

	return ret;
}

#endif /* MY_DEF_HERE */

#ifdef MY_DEF_HERE
int btrfs_find_root_inode(struct btrfs_trans_handle *trans,
			 struct btrfs_fs_info *fs_info, u64 bytenr,
			 u64 datao, u64 time_seq,
			 u64 root_objectid, u64 ino, u64 offset,
			 int in_run_delayed)
{
	int ret;
	struct ulist *leafs = NULL;
	struct ulist *roots = NULL;
	struct ulist_node *ref_node = NULL;
	struct ulist_iterator ref_uiter;

	leafs = ulist_alloc(GFP_NOFS);
	if (!leafs)
		return -ENOMEM;

	ret = find_parent_nodes(trans, fs_info, bytenr, time_seq,
				leafs, NULL, NULL, datao, root_objectid, ino, offset,
				0, BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET, in_run_delayed);
	if (ret < 0 || ret == BACKREF_FOUND_ROOT_INO ||
	    ret == BACKREF_NEXT_ITEM) {
		goto out;
	}
	ret = 0;

	ULIST_ITER_INIT(&ref_uiter);
	while (!ret && (ref_node = ulist_next(leafs, &ref_uiter))) {
		ret = __btrfs_find_all_roots(trans, fs_info, ref_node->val,
					     time_seq, &roots, root_objectid,
					     BTRFS_BACKREF_FIND_ROOT_INO_PRIOR_OFFSET);
		if (ret >= 0) {
			ulist_free(roots);
			roots = NULL;
		}
	}
out:
	if (ret > 0 && ret != BACKREF_FOUND_ROOT_INO) {
		/*
		 * find_parent_nodes might set ret to 1, it's not what
		 * we want.
		 */
		WARN_ON(ret == BACKREF_FOUND_SHARED);
		ret = 0;
	}
	ulist_free(leafs);
	return ret;
}

int check_root_inode_ref(struct btrfs_trans_handle *trans,
		    struct btrfs_fs_info *fs_info, u64 bytenr,
		    u64 datao, u64 root_objectid, u64 ino, u64 offset,
		    int in_run_delayed)
{
	struct seq_list tree_mod_seq_elem = {};
	int ret;

	btrfs_get_tree_mod_seq(fs_info, &tree_mod_seq_elem);
	ret = btrfs_find_root_inode(trans, fs_info, bytenr,
				datao, tree_mod_seq_elem.seq, root_objectid,
				ino, offset, in_run_delayed);

	btrfs_put_tree_mod_seq(fs_info, &tree_mod_seq_elem);
	if (ret > 0) {
		ret = 1;
	}
	return ret;
}
#endif /* MY_DEF_HERE */

/**
 * btrfs_check_shared - tell us whether an extent is shared
 *
 * @trans: optional trans handle
 *
 * btrfs_check_shared uses the backref walking code but will short
 * circuit as soon as it finds a root or inode that doesn't match the
 * one passed in. This provides a significant performance benefit for
 * callers (such as fiemap) which want to know whether the extent is
 * shared but do not need a ref count.
 *
 * Return: 0 if extent is not shared, 1 if it is shared, < 0 on error.
 */
int btrfs_check_shared(struct btrfs_trans_handle *trans,
		       struct btrfs_fs_info *fs_info, u64 root_objectid,
		       u64 inum, u64 bytenr)
{
	struct ulist *tmp = NULL;
	struct ulist *roots = NULL;
	struct ulist_iterator uiter;
	struct ulist_node *node;
	struct seq_list elem = SEQ_LIST_INIT(elem);
	int ret = 0;

	tmp = ulist_alloc(GFP_NOFS);
	roots = ulist_alloc(GFP_NOFS);
	if (!tmp || !roots) {
		ulist_free(tmp);
		ulist_free(roots);
		return -ENOMEM;
	}

	if (trans)
		btrfs_get_tree_mod_seq(fs_info, &elem);
	else
		down_read(&fs_info->commit_root_sem);
	ULIST_ITER_INIT(&uiter);
	while (1) {
		ret = find_parent_nodes(trans, fs_info, bytenr, elem.seq, tmp,
#ifdef MY_DEF_HERE
					roots, NULL, 0, root_objectid, inum, (u64)-1, 1,
					BTRFS_BACKREF_NORMAL, 0);
#else
					roots, NULL, root_objectid, inum, 1);
#endif /* MY_DEF_HERE */
		if (ret == BACKREF_FOUND_SHARED) {
			/* this is the only condition under which we return 1 */
			ret = 1;
			break;
		}
		if (ret < 0 && ret != -ENOENT)
			break;
		ret = 0;
		node = ulist_next(tmp, &uiter);
		if (!node)
			break;
		bytenr = node->val;
		cond_resched();
	}
	if (trans)
		btrfs_put_tree_mod_seq(fs_info, &elem);
	else
		up_read(&fs_info->commit_root_sem);
	ulist_free(tmp);
	ulist_free(roots);
	return ret;
}

int btrfs_find_one_extref(struct btrfs_root *root, u64 inode_objectid,
			  u64 start_off, struct btrfs_path *path,
			  struct btrfs_inode_extref **ret_extref,
			  u64 *found_off)
{
	int ret, slot;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_inode_extref *extref;
	struct extent_buffer *leaf;
	unsigned long ptr;

	key.objectid = inode_objectid;
	key.type = BTRFS_INODE_EXTREF_KEY;
	key.offset = start_off;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		return ret;

	while (1) {
		leaf = path->nodes[0];
		slot = path->slots[0];
		if (slot >= btrfs_header_nritems(leaf)) {
			/*
			 * If the item at offset is not found,
			 * btrfs_search_slot will point us to the slot
			 * where it should be inserted. In our case
			 * that will be the slot directly before the
			 * next INODE_REF_KEY_V2 item. In the case
			 * that we're pointing to the last slot in a
			 * leaf, we must move one leaf over.
			 */
			ret = btrfs_next_leaf(root, path);
			if (ret) {
				if (ret >= 1)
					ret = -ENOENT;
				break;
			}
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &found_key, slot);

		/*
		 * Check that we're still looking at an extended ref key for
		 * this particular objectid. If we have different
		 * objectid or type then there are no more to be found
		 * in the tree and we can exit.
		 */
		ret = -ENOENT;
		if (found_key.objectid != inode_objectid)
			break;
		if (found_key.type != BTRFS_INODE_EXTREF_KEY)
			break;

		ret = 0;
		ptr = btrfs_item_ptr_offset(leaf, path->slots[0]);
		extref = (struct btrfs_inode_extref *)ptr;
		*ret_extref = extref;
		if (found_off)
			*found_off = found_key.offset;
		break;
	}

	return ret;
}

/*
 * this iterates to turn a name (from iref/extref) into a full filesystem path.
 * Elements of the path are separated by '/' and the path is guaranteed to be
 * 0-terminated. the path is only given within the current file system.
 * Therefore, it never starts with a '/'. the caller is responsible to provide
 * "size" bytes in "dest". the dest buffer will be filled backwards. finally,
 * the start point of the resulting string is returned. this pointer is within
 * dest, normally.
 * in case the path buffer would overflow, the pointer is decremented further
 * as if output was written to the buffer, though no more output is actually
 * generated. that way, the caller can determine how much space would be
 * required for the path to fit into the buffer. in that case, the returned
 * value will be smaller than dest. callers must check this!
 */
char *btrfs_ref_to_path(struct btrfs_root *fs_root, struct btrfs_path *path,
			u32 name_len, unsigned long name_off,
			struct extent_buffer *eb_in, u64 parent,
			char *dest, u32 size)
{
	int slot;
	u64 next_inum;
	int ret;
	s64 bytes_left = ((s64)size) - 1;
	struct extent_buffer *eb = eb_in;
	struct btrfs_key found_key;
	int leave_spinning = path->leave_spinning;
	struct btrfs_inode_ref *iref;

	if (bytes_left >= 0)
		dest[bytes_left] = '\0';

	path->leave_spinning = 1;
	while (1) {
		bytes_left -= name_len;
		if (bytes_left >= 0)
			read_extent_buffer(eb, dest + bytes_left,
					   name_off, name_len);
		if (eb != eb_in) {
			if (!path->skip_locking)
				btrfs_tree_read_unlock_blocking(eb);
			free_extent_buffer(eb);
		}
		ret = btrfs_find_item(fs_root, path, parent, 0,
				BTRFS_INODE_REF_KEY, &found_key);
		if (ret > 0)
			ret = -ENOENT;
		if (ret)
			break;

		next_inum = found_key.offset;

		/* regular exit ahead */
		if (parent == next_inum)
			break;

		slot = path->slots[0];
		eb = path->nodes[0];
		/* make sure we can use eb after releasing the path */
		if (eb != eb_in) {
			if (!path->skip_locking)
				btrfs_set_lock_blocking_rw(eb, BTRFS_READ_LOCK);
			path->nodes[0] = NULL;
			path->locks[0] = 0;
		}
		btrfs_release_path(path);
		iref = btrfs_item_ptr(eb, slot, struct btrfs_inode_ref);

		name_len = btrfs_inode_ref_name_len(eb, iref);
		name_off = (unsigned long)(iref + 1);

		parent = next_inum;
		--bytes_left;
		if (bytes_left >= 0)
			dest[bytes_left] = '/';
	}

	btrfs_release_path(path);
	path->leave_spinning = leave_spinning;

	if (ret)
		return ERR_PTR(ret);

	return dest + bytes_left;
}

/*
 * this makes the path point to (logical EXTENT_ITEM *)
 * returns BTRFS_EXTENT_FLAG_DATA for data, BTRFS_EXTENT_FLAG_TREE_BLOCK for
 * tree blocks and <0 on error.
 */
int extent_from_logical(struct btrfs_fs_info *fs_info, u64 logical,
			struct btrfs_path *path, struct btrfs_key *found_key,
			u64 *flags_ret)
{
	int ret;
	u64 flags;
	u64 size = 0;
	u32 item_size;
	struct extent_buffer *eb;
	struct btrfs_extent_item *ei;
	struct btrfs_key key;

	if (btrfs_fs_incompat(fs_info, SKINNY_METADATA))
		key.type = BTRFS_METADATA_ITEM_KEY;
	else
		key.type = BTRFS_EXTENT_ITEM_KEY;
	key.objectid = logical;
	key.offset = (u64)-1;

	ret = btrfs_search_slot(NULL, fs_info->extent_root, &key, path, 0, 0);
	if (ret < 0)
		return ret;

	ret = btrfs_previous_extent_item(fs_info->extent_root, path, 0);
	if (ret) {
		if (ret > 0)
			ret = -ENOENT;
		return ret;
	}
	btrfs_item_key_to_cpu(path->nodes[0], found_key, path->slots[0]);
	if (found_key->type == BTRFS_METADATA_ITEM_KEY)
		size = fs_info->extent_root->nodesize;
	else if (found_key->type == BTRFS_EXTENT_ITEM_KEY)
		size = found_key->offset;

	if (found_key->objectid > logical ||
	    found_key->objectid + size <= logical) {
		pr_debug("logical %llu is not within any extent\n", logical);
		return -ENOENT;
	}

	eb = path->nodes[0];
	item_size = btrfs_item_size_nr(eb, path->slots[0]);
	BUG_ON(item_size < sizeof(*ei));

	ei = btrfs_item_ptr(eb, path->slots[0], struct btrfs_extent_item);
	flags = btrfs_extent_flags(eb, ei);

	pr_debug("logical %llu is at position %llu within the extent (%llu "
		 "EXTENT_ITEM %llu) flags %#llx size %u\n",
		 logical, logical - found_key->objectid, found_key->objectid,
		 found_key->offset, flags, item_size);

	WARN_ON(!flags_ret);
	if (flags_ret) {
		if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK)
			*flags_ret = BTRFS_EXTENT_FLAG_TREE_BLOCK;
		else if (flags & BTRFS_EXTENT_FLAG_DATA)
			*flags_ret = BTRFS_EXTENT_FLAG_DATA;
		else
			BUG_ON(1);
		return 0;
	}

	return -EIO;
}

/*
 * helper function to iterate extent inline refs. ptr must point to a 0 value
 * for the first call and may be modified. it is used to track state.
 * if more refs exist, 0 is returned and the next call to
 * __get_extent_inline_ref must pass the modified ptr parameter to get the
 * next ref. after the last ref was processed, 1 is returned.
 * returns <0 on error
 */
static int __get_extent_inline_ref(unsigned long *ptr, struct extent_buffer *eb,
				   struct btrfs_key *key,
				   struct btrfs_extent_item *ei, u32 item_size,
				   struct btrfs_extent_inline_ref **out_eiref,
				   int *out_type)
{
	unsigned long end;
	u64 flags;
	struct btrfs_tree_block_info *info;

	if (!*ptr) {
		/* first call */
		flags = btrfs_extent_flags(eb, ei);
		if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
			if (key->type == BTRFS_METADATA_ITEM_KEY) {
				/* a skinny metadata extent */
				*out_eiref =
				     (struct btrfs_extent_inline_ref *)(ei + 1);
			} else {
				WARN_ON(key->type != BTRFS_EXTENT_ITEM_KEY);
				info = (struct btrfs_tree_block_info *)(ei + 1);
				*out_eiref =
				   (struct btrfs_extent_inline_ref *)(info + 1);
			}
		} else {
			*out_eiref = (struct btrfs_extent_inline_ref *)(ei + 1);
		}
		*ptr = (unsigned long)*out_eiref;
		if ((unsigned long)(*ptr) >= (unsigned long)ei + item_size)
			return -ENOENT;
	}

	end = (unsigned long)ei + item_size;
	*out_eiref = (struct btrfs_extent_inline_ref *)(*ptr);
	*out_type = btrfs_extent_inline_ref_type(eb, *out_eiref);

	*ptr += btrfs_extent_inline_ref_size(*out_type);
	WARN_ON(*ptr > end);
	if (*ptr == end)
		return 1; /* last */

	return 0;
}

/*
 * reads the tree block backref for an extent. tree level and root are returned
 * through out_level and out_root. ptr must point to a 0 value for the first
 * call and may be modified (see __get_extent_inline_ref comment).
 * returns 0 if data was provided, 1 if there was no more data to provide or
 * <0 on error.
 */
int tree_backref_for_extent(unsigned long *ptr, struct extent_buffer *eb,
			    struct btrfs_key *key, struct btrfs_extent_item *ei,
			    u32 item_size, u64 *out_root, u8 *out_level)
{
	int ret;
	int type;
	struct btrfs_extent_inline_ref *eiref;

	if (*ptr == (unsigned long)-1)
		return 1;

	while (1) {
		ret = __get_extent_inline_ref(ptr, eb, key, ei, item_size,
					      &eiref, &type);
		if (ret < 0)
			return ret;

		if (type == BTRFS_TREE_BLOCK_REF_KEY ||
		    type == BTRFS_SHARED_BLOCK_REF_KEY)
			break;

		if (ret == 1)
			return 1;
	}

	/* we can treat both ref types equally here */
	*out_root = btrfs_extent_inline_ref_offset(eb, eiref);

	if (key->type == BTRFS_EXTENT_ITEM_KEY) {
		struct btrfs_tree_block_info *info;

		info = (struct btrfs_tree_block_info *)(ei + 1);
		*out_level = btrfs_tree_block_level(eb, info);
	} else {
		ASSERT(key->type == BTRFS_METADATA_ITEM_KEY);
		*out_level = (u8)key->offset;
	}

	if (ret == 1)
		*ptr = (unsigned long)-1;

	return 0;
}

static int iterate_leaf_refs(struct extent_inode_elem *inode_list,
				u64 root, u64 extent_item_objectid,
				iterate_extent_inodes_t *iterate, void *ctx)
{
	struct extent_inode_elem *eie;
	int ret = 0;

	for (eie = inode_list; eie; eie = eie->next) {
		pr_debug("ref for %llu resolved, key (%llu EXTEND_DATA %llu), "
			 "root %llu\n", extent_item_objectid,
			 eie->inum, eie->offset, root);
		ret = iterate(eie->inum, eie->offset, root, ctx
#ifdef MY_DEF_HERE
			      , eie->extent_type
#endif /* MY_DEF_HERE */
			      );
		if (ret) {
			pr_debug("stopping iteration for %llu due to ret=%d\n",
				 extent_item_objectid, ret);
			break;
		}
	}

	return ret;
}

/*
 * calls iterate() for every inode that references the extent identified by
 * the given parameters.
 * when the iterator function returns a non-zero value, iteration stops.
 */
int iterate_extent_inodes(struct btrfs_fs_info *fs_info,
				u64 extent_item_objectid, u64 extent_item_pos,
				int search_commit_root,
				iterate_extent_inodes_t *iterate, void *ctx)
{
	int ret;
	struct btrfs_trans_handle *trans = NULL;
	struct ulist *refs = NULL;
	struct ulist *roots = NULL;
	struct ulist_node *ref_node = NULL;
	struct ulist_node *root_node = NULL;
	struct seq_list tree_mod_seq_elem = SEQ_LIST_INIT(tree_mod_seq_elem);
	struct ulist_iterator ref_uiter;
	struct ulist_iterator root_uiter;

	pr_debug("resolving all inodes for extent %llu\n",
			extent_item_objectid);

	if (!search_commit_root) {
		trans = btrfs_attach_transaction(fs_info->extent_root);
		if (IS_ERR(trans)) {
			if (PTR_ERR(trans) != -ENOENT &&
			    PTR_ERR(trans) != -EROFS)
				return PTR_ERR(trans);
			trans = NULL;
		}
	}

	if (trans)
		btrfs_get_tree_mod_seq(fs_info, &tree_mod_seq_elem);
	else
		down_read(&fs_info->commit_root_sem);

	ret = btrfs_find_all_leafs(trans, fs_info, extent_item_objectid,
				   tree_mod_seq_elem.seq, &refs,
				   &extent_item_pos);
	if (ret)
		goto out;

	ULIST_ITER_INIT(&ref_uiter);
	while (!ret && (ref_node = ulist_next(refs, &ref_uiter))) {
		ret = __btrfs_find_all_roots(trans, fs_info, ref_node->val,
#ifdef MY_DEF_HERE
					     tree_mod_seq_elem.seq, &roots, 0, 0);
#else
					     tree_mod_seq_elem.seq, &roots);
#endif /* MY_DEF_HERE */
		if (ret)
			break;
		ULIST_ITER_INIT(&root_uiter);
		while (!ret && (root_node = ulist_next(roots, &root_uiter))) {
			pr_debug("root %llu references leaf %llu, data list "
				 "%#llx\n", root_node->val, ref_node->val,
				 ref_node->aux);
			ret = iterate_leaf_refs((struct extent_inode_elem *)
						(uintptr_t)ref_node->aux,
						root_node->val,
						extent_item_objectid,
						iterate, ctx);
		}
		ulist_free(roots);
	}

	free_leaf_list(refs);
out:
	if (trans) {
		btrfs_put_tree_mod_seq(fs_info, &tree_mod_seq_elem);
		btrfs_end_transaction(trans, fs_info->extent_root);
	} else {
		up_read(&fs_info->commit_root_sem);
	}

	return ret;
}

int iterate_inodes_from_logical(u64 logical, struct btrfs_fs_info *fs_info,
				struct btrfs_path *path,
				iterate_extent_inodes_t *iterate, void *ctx)
{
	int ret;
	u64 extent_item_pos;
	u64 flags = 0;
	struct btrfs_key found_key;
	int search_commit_root = path->search_commit_root;

	ret = extent_from_logical(fs_info, logical, path, &found_key, &flags);
	btrfs_release_path(path);
	if (ret < 0)
		return ret;
	if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK)
		return -EINVAL;

	extent_item_pos = logical - found_key.objectid;
	ret = iterate_extent_inodes(fs_info, found_key.objectid,
					extent_item_pos, search_commit_root,
					iterate, ctx);

	return ret;
}

typedef int (iterate_irefs_t)(u64 parent, u32 name_len, unsigned long name_off,
			      struct extent_buffer *eb, void *ctx);

static int iterate_inode_refs(u64 inum, struct btrfs_root *fs_root,
			      struct btrfs_path *path,
			      iterate_irefs_t *iterate, void *ctx)
{
	int ret = 0;
	int slot;
	u32 cur;
	u32 len;
	u32 name_len;
	u64 parent = 0;
	int found = 0;
	struct extent_buffer *eb;
	struct btrfs_item *item;
	struct btrfs_inode_ref *iref;
	struct btrfs_key found_key;

	while (!ret) {
		ret = btrfs_find_item(fs_root, path, inum,
				parent ? parent + 1 : 0, BTRFS_INODE_REF_KEY,
				&found_key);

		if (ret < 0)
			break;
		if (ret) {
			ret = found ? 0 : -ENOENT;
			break;
		}
		++found;

		parent = found_key.offset;
		slot = path->slots[0];
		eb = btrfs_clone_extent_buffer(path->nodes[0]);
		if (!eb) {
			ret = -ENOMEM;
			break;
		}
		extent_buffer_get(eb);
		btrfs_tree_read_lock(eb);
		btrfs_set_lock_blocking_rw(eb, BTRFS_READ_LOCK);
		btrfs_release_path(path);

		item = btrfs_item_nr(slot);
		iref = btrfs_item_ptr(eb, slot, struct btrfs_inode_ref);

		for (cur = 0; cur < btrfs_item_size(eb, item); cur += len) {
			name_len = btrfs_inode_ref_name_len(eb, iref);
			/* path must be released before calling iterate()! */
			pr_debug("following ref at offset %u for inode %llu in "
				 "tree %llu\n", cur, found_key.objectid,
				 fs_root->objectid);
			ret = iterate(parent, name_len,
				      (unsigned long)(iref + 1), eb, ctx);
			if (ret)
				break;
			len = sizeof(*iref) + name_len;
			iref = (struct btrfs_inode_ref *)((char *)iref + len);
		}
		btrfs_tree_read_unlock_blocking(eb);
		free_extent_buffer(eb);
	}

	btrfs_release_path(path);

	return ret;
}

static int iterate_inode_extrefs(u64 inum, struct btrfs_root *fs_root,
				 struct btrfs_path *path,
				 iterate_irefs_t *iterate, void *ctx)
{
	int ret;
	int slot;
	u64 offset = 0;
	u64 parent;
	int found = 0;
	struct extent_buffer *eb;
	struct btrfs_inode_extref *extref;
	u32 item_size;
	u32 cur_offset;
	unsigned long ptr;

	while (1) {
		ret = btrfs_find_one_extref(fs_root, inum, offset, path, &extref,
					    &offset);
		if (ret < 0)
			break;
		if (ret) {
			ret = found ? 0 : -ENOENT;
			break;
		}
		++found;

		slot = path->slots[0];
		eb = btrfs_clone_extent_buffer(path->nodes[0]);
		if (!eb) {
			ret = -ENOMEM;
			break;
		}
		extent_buffer_get(eb);

		btrfs_tree_read_lock(eb);
		btrfs_set_lock_blocking_rw(eb, BTRFS_READ_LOCK);
		btrfs_release_path(path);

		item_size = btrfs_item_size_nr(eb, slot);
		ptr = btrfs_item_ptr_offset(eb, slot);
		cur_offset = 0;

		while (cur_offset < item_size) {
			u32 name_len;

			extref = (struct btrfs_inode_extref *)(ptr + cur_offset);
			parent = btrfs_inode_extref_parent(eb, extref);
			name_len = btrfs_inode_extref_name_len(eb, extref);
			ret = iterate(parent, name_len,
				      (unsigned long)&extref->name, eb, ctx);
			if (ret)
				break;

			cur_offset += btrfs_inode_extref_name_len(eb, extref);
			cur_offset += sizeof(*extref);
		}
		btrfs_tree_read_unlock_blocking(eb);
		free_extent_buffer(eb);

		offset++;
	}

	btrfs_release_path(path);

	return ret;
}

static int iterate_irefs(u64 inum, struct btrfs_root *fs_root,
			 struct btrfs_path *path, iterate_irefs_t *iterate,
			 void *ctx)
{
	int ret;
	int found_refs = 0;

	ret = iterate_inode_refs(inum, fs_root, path, iterate, ctx);
	if (!ret)
		++found_refs;
	else if (ret != -ENOENT)
		return ret;

	ret = iterate_inode_extrefs(inum, fs_root, path, iterate, ctx);
	if (ret == -ENOENT && found_refs)
		return 0;

	return ret;
}

/*
 * returns 0 if the path could be dumped (probably truncated)
 * returns <0 in case of an error
 */
static int inode_to_path(u64 inum, u32 name_len, unsigned long name_off,
			 struct extent_buffer *eb, void *ctx)
{
	struct inode_fs_paths *ipath = ctx;
	char *fspath;
	char *fspath_min;
	int i = ipath->fspath->elem_cnt;
	const int s_ptr = sizeof(char *);
	u32 bytes_left;

	bytes_left = ipath->fspath->bytes_left > s_ptr ?
					ipath->fspath->bytes_left - s_ptr : 0;

	fspath_min = (char *)ipath->fspath->val + (i + 1) * s_ptr;
	fspath = btrfs_ref_to_path(ipath->fs_root, ipath->btrfs_path, name_len,
				   name_off, eb, inum, fspath_min, bytes_left);
	if (IS_ERR(fspath))
		return PTR_ERR(fspath);

	if (fspath > fspath_min) {
		ipath->fspath->val[i] = (u64)(unsigned long)fspath;
		++ipath->fspath->elem_cnt;
		ipath->fspath->bytes_left = fspath - fspath_min;
	} else {
		++ipath->fspath->elem_missed;
		ipath->fspath->bytes_missing += fspath_min - fspath;
		ipath->fspath->bytes_left = 0;
	}

	return 0;
}

/*
 * this dumps all file system paths to the inode into the ipath struct, provided
 * is has been created large enough. each path is zero-terminated and accessed
 * from ipath->fspath->val[i].
 * when it returns, there are ipath->fspath->elem_cnt number of paths available
 * in ipath->fspath->val[]. when the allocated space wasn't sufficient, the
 * number of missed paths is recorded in ipath->fspath->elem_missed, otherwise,
 * it's zero. ipath->fspath->bytes_missing holds the number of bytes that would
 * have been needed to return all paths.
 */
int paths_from_inode(u64 inum, struct inode_fs_paths *ipath)
{
	return iterate_irefs(inum, ipath->fs_root, ipath->btrfs_path,
			     inode_to_path, ipath);
}

struct btrfs_data_container *init_data_container(u32 total_bytes)
{
	struct btrfs_data_container *data;
	size_t alloc_bytes;

	alloc_bytes = max_t(size_t, total_bytes, sizeof(*data));
	data = vmalloc(alloc_bytes);
	if (!data)
		return ERR_PTR(-ENOMEM);

	if (total_bytes >= sizeof(*data)) {
		data->bytes_left = total_bytes - sizeof(*data);
		data->bytes_missing = 0;
	} else {
		data->bytes_missing = sizeof(*data) - total_bytes;
		data->bytes_left = 0;
	}

	data->elem_cnt = 0;
	data->elem_missed = 0;

	return data;
}

/*
 * allocates space to return multiple file system paths for an inode.
 * total_bytes to allocate are passed, note that space usable for actual path
 * information will be total_bytes - sizeof(struct inode_fs_paths).
 * the returned pointer must be freed with free_ipath() in the end.
 */
struct inode_fs_paths *init_ipath(s32 total_bytes, struct btrfs_root *fs_root,
					struct btrfs_path *path)
{
	struct inode_fs_paths *ifp;
	struct btrfs_data_container *fspath;

	fspath = init_data_container(total_bytes);
	if (IS_ERR(fspath))
		return (void *)fspath;

	ifp = kmalloc(sizeof(*ifp), GFP_NOFS);
	if (!ifp) {
		vfree(fspath);
		return ERR_PTR(-ENOMEM);
	}

	ifp->btrfs_path = path;
	ifp->fspath = fspath;
	ifp->fs_root = fs_root;

	return ifp;
}

void free_ipath(struct inode_fs_paths *ipath)
{
	if (!ipath)
		return;
	vfree(ipath->fspath);
	kfree(ipath);
}
