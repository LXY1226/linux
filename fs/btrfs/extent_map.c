#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hardirq.h>
#include "ctree.h"
#include "extent_map.h"
#ifdef MY_DEF_HERE
#include "btrfs_inode.h"
#endif /* MY_DEF_HERE */
#include "compression.h"


static struct kmem_cache *extent_map_cache;

int __init extent_map_init(void)
{
	extent_map_cache = kmem_cache_create("btrfs_extent_map",
			sizeof(struct extent_map), 0,
			SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD, NULL);
	if (!extent_map_cache)
		return -ENOMEM;
	return 0;
}

void extent_map_exit(void)
{
	kmem_cache_destroy(extent_map_cache);
}

/**
 * extent_map_tree_init - initialize extent map tree
 * @tree:		tree to initialize
 *
 * Initialize the extent tree @tree.  Should be called for each new inode
 * or other user of the extent_map interface.
 */
void extent_map_tree_init(struct extent_map_tree *tree)
{
#ifdef MY_DEF_HERE
	atomic_set(&tree->nr_extent_maps, 0);
	INIT_LIST_HEAD(&tree->not_modified_extents);
	INIT_LIST_HEAD(&tree->syno_modified_extents);
	INIT_LIST_HEAD(&tree->pinned_extents);
#endif /* MY_DEF_HERE */
	tree->map = RB_ROOT;
	INIT_LIST_HEAD(&tree->modified_extents);
	rwlock_init(&tree->lock);
}

/**
 * alloc_extent_map - allocate new extent map structure
 *
 * Allocate a new extent_map structure.  The new structure is
 * returned with a reference count of one and needs to be
 * freed using free_extent_map()
 */
struct extent_map *alloc_extent_map(void)
{
	struct extent_map *em;
	em = kmem_cache_zalloc(extent_map_cache, GFP_NOFS);
	if (!em)
		return NULL;
	RB_CLEAR_NODE(&em->rb_node);
	em->flags = 0;
	em->compress_type = BTRFS_COMPRESS_NONE;
	em->generation = 0;
	atomic_set(&em->refs, 1);
	INIT_LIST_HEAD(&em->list);
#ifdef MY_DEF_HERE
	INIT_LIST_HEAD(&em->free_list);
	em->bl_increase = false;
#endif /* MY_DEF_HERE */
	return em;
}

/**
 * free_extent_map - drop reference count of an extent_map
 * @em:		extent map being released
 *
 * Drops the reference out on @em by one and free the structure
 * if the reference count hits zero.
 */
void free_extent_map(struct extent_map *em)
{
	if (!em)
		return;
	WARN_ON(atomic_read(&em->refs) == 0);
	if (atomic_dec_and_test(&em->refs)) {
		WARN_ON(extent_map_in_tree(em));
		WARN_ON(!list_empty(&em->list));
#ifdef MY_DEF_HERE
		WARN_ON(!list_empty(&em->free_list));
#endif /* MY_DEF_HERE */
		if (test_bit(EXTENT_FLAG_FS_MAPPING, &em->flags))
			kfree(em->map_lookup);
		kmem_cache_free(extent_map_cache, em);
	}
}

/* simple helper to do math around the end of an extent, handling wrap */
static u64 range_end(u64 start, u64 len)
{
	if (start + len < start)
		return (u64)-1;
	return start + len;
}

static int tree_insert(struct rb_root *root, struct extent_map *em)
{
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct extent_map *entry = NULL;
	struct rb_node *orig_parent = NULL;
	u64 end = range_end(em->start, em->len);

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct extent_map, rb_node);

		if (em->start < entry->start)
			p = &(*p)->rb_left;
		else if (em->start >= extent_map_end(entry))
			p = &(*p)->rb_right;
		else
			return -EEXIST;
	}

	orig_parent = parent;
	while (parent && em->start >= extent_map_end(entry)) {
		parent = rb_next(parent);
		entry = rb_entry(parent, struct extent_map, rb_node);
	}
	if (parent)
		if (end > entry->start && em->start < extent_map_end(entry))
			return -EEXIST;

	parent = orig_parent;
	entry = rb_entry(parent, struct extent_map, rb_node);
	while (parent && em->start < entry->start) {
		parent = rb_prev(parent);
		entry = rb_entry(parent, struct extent_map, rb_node);
	}
	if (parent)
		if (end > entry->start && em->start < extent_map_end(entry))
			return -EEXIST;

	rb_link_node(&em->rb_node, orig_parent, p);
	rb_insert_color(&em->rb_node, root);
	return 0;
}

/*
 * search through the tree for an extent_map with a given offset.  If
 * it can't be found, try to find some neighboring extents
 */
static struct rb_node *__tree_search(struct rb_root *root, u64 offset,
				     struct rb_node **prev_ret,
				     struct rb_node **next_ret)
{
	struct rb_node *n = root->rb_node;
	struct rb_node *prev = NULL;
	struct rb_node *orig_prev = NULL;
	struct extent_map *entry;
	struct extent_map *prev_entry = NULL;

	while (n) {
		entry = rb_entry(n, struct extent_map, rb_node);
		prev = n;
		prev_entry = entry;

		if (offset < entry->start)
			n = n->rb_left;
		else if (offset >= extent_map_end(entry))
			n = n->rb_right;
		else
			return n;
	}

	if (prev_ret) {
		orig_prev = prev;
		while (prev && offset >= extent_map_end(prev_entry)) {
			prev = rb_next(prev);
			prev_entry = rb_entry(prev, struct extent_map, rb_node);
		}
		*prev_ret = prev;
		prev = orig_prev;
	}

	if (next_ret) {
		prev_entry = rb_entry(prev, struct extent_map, rb_node);
		while (prev && offset < prev_entry->start) {
			prev = rb_prev(prev);
			prev_entry = rb_entry(prev, struct extent_map, rb_node);
		}
		*next_ret = prev;
	}
	return NULL;
}

/* check to see if two extent_map structs are adjacent and safe to merge */
static int mergable_maps(struct extent_map *prev, struct extent_map *next)
{
	if (test_bit(EXTENT_FLAG_PINNED, &prev->flags))
		return 0;

	/*
	 * don't merge compressed extents, we need to know their
	 * actual size
	 */
	if (test_bit(EXTENT_FLAG_COMPRESSED, &prev->flags))
		return 0;

	if (test_bit(EXTENT_FLAG_LOGGING, &prev->flags) ||
	    test_bit(EXTENT_FLAG_LOGGING, &next->flags))
		return 0;

	/*
	 * We don't want to merge stuff that hasn't been written to the log yet
	 * since it may not reflect exactly what is on disk, and that would be
	 * bad.
	 */
	if (!list_empty(&prev->list) || !list_empty(&next->list))
		return 0;

	if (extent_map_end(prev) == next->start &&
	    prev->flags == next->flags &&
	    prev->bdev == next->bdev &&
	    ((next->block_start == EXTENT_MAP_HOLE &&
	      prev->block_start == EXTENT_MAP_HOLE) ||
	     (next->block_start == EXTENT_MAP_INLINE &&
	      prev->block_start == EXTENT_MAP_INLINE) ||
	     (next->block_start == EXTENT_MAP_DELALLOC &&
	      prev->block_start == EXTENT_MAP_DELALLOC) ||
	     (next->block_start < EXTENT_MAP_LAST_BYTE - 1 &&
	      next->block_start == extent_map_block_end(prev)))) {
		return 1;
	}
	return 0;
}

#ifdef MY_DEF_HERE
static void check_and_insert_extent_map_to_global_extent(struct extent_map_tree *tree, struct extent_map *em, int modified)
{
	u64 rootid = 0;

	atomic_inc(&tree->nr_extent_maps);
	if (tree->inode && tree->inode->root && !btrfs_is_free_space_inode(&tree->inode->vfs_inode)) {
		rootid = tree->inode->root->objectid;
		if (rootid == BTRFS_FS_TREE_OBJECTID ||
			(rootid >= BTRFS_FIRST_FREE_OBJECTID && rootid <= BTRFS_LAST_FREE_OBJECTID)) {
			if (!test_bit(EXTENT_FLAG_PINNED, &em->flags)) {
				if (!em->bl_increase) {
					atomic_inc(&tree->inode->root->fs_info->nr_extent_maps);
					em->bl_increase = true;
				}
			}
			if (!modified) {
				list_move_tail(&em->free_list, &tree->not_modified_extents);
			} else if (test_bit(EXTENT_FLAG_PINNED, &em->flags)) {
				list_move_tail(&em->free_list, &tree->pinned_extents);
			} else {
				list_move_tail(&em->free_list, &tree->syno_modified_extents);
			}
			if (list_empty(&tree->inode->free_extent_map_inode)) {
				spin_lock(&tree->inode->root->fs_info->extent_map_inode_list_lock);
				list_move_tail(&tree->inode->free_extent_map_inode, &tree->inode->root->fs_info->extent_map_inode_list);
				spin_unlock(&tree->inode->root->fs_info->extent_map_inode_list_lock);
			}
		}
	}
}
static void check_and_decrease_global_extent(struct extent_map_tree *tree, struct extent_map *em)
{
	u64 rootid = 0;

	// decreace nr_extent_maps when extent_map dettached from extent_tree
	WARN_ON(atomic_read(&tree->nr_extent_maps) == 0);
	atomic_dec(&tree->nr_extent_maps);
	if (!list_empty(&em->free_list)) {
		list_del_init(&em->free_list);
	}
	if (tree->inode && tree->inode->root && !btrfs_is_free_space_inode(&tree->inode->vfs_inode)) {
		rootid = tree->inode->root->objectid;
		if (rootid == BTRFS_FS_TREE_OBJECTID ||
			(rootid >= BTRFS_FIRST_FREE_OBJECTID && rootid <= BTRFS_LAST_FREE_OBJECTID)) {
			if (em->bl_increase) {
				WARN_ON(atomic_read(&(tree->inode->root->fs_info->nr_extent_maps)) == 0);
				atomic_dec(&tree->inode->root->fs_info->nr_extent_maps);
				em->bl_increase = false;
			}
			if (atomic_read(&tree->nr_extent_maps) == 0 && !list_empty(&tree->inode->free_extent_map_inode)) {
				spin_lock(&tree->inode->root->fs_info->extent_map_inode_list_lock);
				if (atomic_read(&tree->inode->free_extent_map_counts) == 0) {
					list_del_init(&tree->inode->free_extent_map_inode);
				}
				spin_unlock(&tree->inode->root->fs_info->extent_map_inode_list_lock);
			}
		}
	}
}
#endif /* MY_DEF_HERE */

static void try_merge_map(struct extent_map_tree *tree, struct extent_map *em)
{
	struct extent_map *merge = NULL;
	struct rb_node *rb;

	/*
	 * We can't modify an extent map that is in the tree and that is being
	 * used by another task, as it can cause that other task to see it in
	 * inconsistent state during the merging. We always have 1 reference for
	 * the tree and 1 for this task (which is unpinning the extent map or
	 * clearing the logging flag), so anything > 2 means it's being used by
	 * other tasks too.
	 */
	if (atomic_read(&em->refs) > 2)
		return;

	if (em->start != 0) {
		rb = rb_prev(&em->rb_node);
		if (rb)
			merge = rb_entry(rb, struct extent_map, rb_node);
		if (rb && mergable_maps(merge, em)) {
			em->start = merge->start;
			em->orig_start = merge->orig_start;
			em->len += merge->len;
			em->block_len += merge->block_len;
			em->block_start = merge->block_start;
			em->mod_len = (em->mod_len + em->mod_start) - merge->mod_start;
			em->mod_start = merge->mod_start;
			em->generation = max(em->generation, merge->generation);

			rb_erase(&merge->rb_node, &tree->map);
			RB_CLEAR_NODE(&merge->rb_node);
#ifdef MY_DEF_HERE
			check_and_decrease_global_extent(tree, merge);
#endif /* MY_DEF_HERE */
			free_extent_map(merge);
		}
	}

	rb = rb_next(&em->rb_node);
	if (rb)
		merge = rb_entry(rb, struct extent_map, rb_node);
	if (rb && mergable_maps(em, merge)) {
		em->len += merge->len;
		em->block_len += merge->block_len;
		rb_erase(&merge->rb_node, &tree->map);
		RB_CLEAR_NODE(&merge->rb_node);
		em->mod_len = (merge->mod_start + merge->mod_len) - em->mod_start;
		em->generation = max(em->generation, merge->generation);
#ifdef MY_DEF_HERE
		check_and_decrease_global_extent(tree, merge);
#endif /* MY_DEF_HERE */
		free_extent_map(merge);
	}
}

/**
 * unpin_extent_cache - unpin an extent from the cache
 * @tree:	tree to unpin the extent in
 * @start:	logical offset in the file
 * @len:	length of the extent
 * @gen:	generation that this extent has been modified in
 *
 * Called after an extent has been written to disk properly.  Set the generation
 * to the generation that actually added the file item to the inode so we know
 * we need to sync this extent when we call fsync().
 */
int unpin_extent_cache(struct extent_map_tree *tree, u64 start, u64 len,
		       u64 gen)
{
	int ret = 0;
	struct extent_map *em;
	bool prealloc = false;

	write_lock(&tree->lock);
	em = lookup_extent_mapping(tree, start, len);

	WARN_ON(!em || em->start != start);

	if (!em)
		goto out;

	em->generation = gen;
	clear_bit(EXTENT_FLAG_PINNED, &em->flags);
#ifdef MY_DEF_HERE
	list_move_tail(&em->free_list, &tree->syno_modified_extents);
	if (!em->bl_increase) {
		atomic_inc(&tree->inode->root->fs_info->nr_extent_maps);
		em->bl_increase = true;
	}
#endif /* MY_DEF_HERE */
	em->mod_start = em->start;
	em->mod_len = em->len;

	if (test_bit(EXTENT_FLAG_FILLING, &em->flags)) {
		prealloc = true;
		clear_bit(EXTENT_FLAG_FILLING, &em->flags);
	}

	try_merge_map(tree, em);

	if (prealloc) {
		em->mod_start = em->start;
		em->mod_len = em->len;
	}

	free_extent_map(em);
out:
	write_unlock(&tree->lock);
	return ret;

}

void clear_em_logging(struct extent_map_tree *tree, struct extent_map *em)
{
	clear_bit(EXTENT_FLAG_LOGGING, &em->flags);
	if (extent_map_in_tree(em))
		try_merge_map(tree, em);
}

static inline void setup_extent_mapping(struct extent_map_tree *tree,
					struct extent_map *em,
					int modified)
{
	atomic_inc(&em->refs);
	em->mod_start = em->start;
	em->mod_len = em->len;

	if (modified)
		list_move(&em->list, &tree->modified_extents);
	else
		try_merge_map(tree, em);
}

/**
 * add_extent_mapping - add new extent map to the extent tree
 * @tree:	tree to insert new map in
 * @em:		map to insert
 *
 * Insert @em into @tree or perform a simple forward/backward merge with
 * existing mappings.  The extent_map struct passed in will be inserted
 * into the tree directly, with an additional reference taken, or a
 * reference dropped if the merge attempt was successful.
 */
int add_extent_mapping(struct extent_map_tree *tree,
		       struct extent_map *em, int modified)
{
	int ret = 0;

	ret = tree_insert(&tree->map, em);
	if (ret)
		goto out;

#ifdef MY_DEF_HERE
	check_and_insert_extent_map_to_global_extent(tree, em, modified);
#endif /* MY_DEF_HERE */
	setup_extent_mapping(tree, em, modified);
out:
	return ret;
}

static struct extent_map *
__lookup_extent_mapping(struct extent_map_tree *tree,
			u64 start, u64 len, int strict)
{
	struct extent_map *em;
	struct rb_node *rb_node;
	struct rb_node *prev = NULL;
	struct rb_node *next = NULL;
	u64 end = range_end(start, len);

	rb_node = __tree_search(&tree->map, start, &prev, &next);
	if (!rb_node) {
		if (prev)
			rb_node = prev;
		else if (next)
			rb_node = next;
		else
			return NULL;
	}

	em = rb_entry(rb_node, struct extent_map, rb_node);

	if (strict && !(end > em->start && start < extent_map_end(em)))
		return NULL;

	atomic_inc(&em->refs);
	return em;
}

/**
 * lookup_extent_mapping - lookup extent_map
 * @tree:	tree to lookup in
 * @start:	byte offset to start the search
 * @len:	length of the lookup range
 *
 * Find and return the first extent_map struct in @tree that intersects the
 * [start, len] range.  There may be additional objects in the tree that
 * intersect, so check the object returned carefully to make sure that no
 * additional lookups are needed.
 */
struct extent_map *lookup_extent_mapping(struct extent_map_tree *tree,
					 u64 start, u64 len)
{
	return __lookup_extent_mapping(tree, start, len, 1);
}

/**
 * search_extent_mapping - find a nearby extent map
 * @tree:	tree to lookup in
 * @start:	byte offset to start the search
 * @len:	length of the lookup range
 *
 * Find and return the first extent_map struct in @tree that intersects the
 * [start, len] range.
 *
 * If one can't be found, any nearby extent may be returned
 */
struct extent_map *search_extent_mapping(struct extent_map_tree *tree,
					 u64 start, u64 len)
{
	return __lookup_extent_mapping(tree, start, len, 0);
}

/**
 * remove_extent_mapping - removes an extent_map from the extent tree
 * @tree:	extent tree to remove from
 * @em:		extent map being removed
 *
 * Removes @em from @tree.  No reference counts are dropped, and no checks
 * are done to see if the range is in use
 */
int remove_extent_mapping(struct extent_map_tree *tree, struct extent_map *em)
{
	int ret = 0;

	WARN_ON(test_bit(EXTENT_FLAG_PINNED, &em->flags));

	rb_erase(&em->rb_node, &tree->map);
	if (!test_bit(EXTENT_FLAG_LOGGING, &em->flags))
		list_del_init(&em->list);
	RB_CLEAR_NODE(&em->rb_node);

#ifdef MY_DEF_HERE
	check_and_decrease_global_extent(tree, em);
#endif /* MY_DEF_HERE */
	return ret;
}

void replace_extent_mapping(struct extent_map_tree *tree,
			    struct extent_map *cur,
			    struct extent_map *new,
			    int modified)
{
	WARN_ON(test_bit(EXTENT_FLAG_PINNED, &cur->flags));
	ASSERT(extent_map_in_tree(cur));
	if (!test_bit(EXTENT_FLAG_LOGGING, &cur->flags))
		list_del_init(&cur->list);
	rb_replace_node(&cur->rb_node, &new->rb_node, &tree->map);
	RB_CLEAR_NODE(&cur->rb_node);

#ifdef MY_DEF_HERE
	check_and_decrease_global_extent(tree, cur);
	check_and_insert_extent_map_to_global_extent(tree, new, modified);
#endif

	setup_extent_mapping(tree, new, modified);
}

static struct extent_map *next_extent_map(struct extent_map *em)
{
	struct rb_node *next;

	next = rb_next(&em->rb_node);
	if (!next)
		return NULL;
	return container_of(next, struct extent_map, rb_node);
}

static struct extent_map *prev_extent_map(struct extent_map *em)
{
	struct rb_node *prev;

	prev = rb_prev(&em->rb_node);
	if (!prev)
		return NULL;
	return container_of(prev, struct extent_map, rb_node);
}

/* helper for btfs_get_extent.  Given an existing extent in the tree,
 * the existing extent is the nearest extent to map_start,
 * and an extent that you want to insert, deal with overlap and insert
 * the best fitted new extent into the tree.
 */
static noinline int merge_extent_mapping(struct extent_map_tree *em_tree,
					 struct extent_map *existing,
					 struct extent_map *em,
					 u64 map_start)
{
	struct extent_map *prev;
	struct extent_map *next;
	u64 start;
	u64 end;
	u64 start_diff;

	BUG_ON(map_start < em->start || map_start >= extent_map_end(em));

	if (existing->start > map_start) {
		next = existing;
		prev = prev_extent_map(next);
	} else {
		prev = existing;
		next = next_extent_map(prev);
	}

	start = prev ? extent_map_end(prev) : em->start;
	start = max_t(u64, start, em->start);
	end = next ? next->start : extent_map_end(em);
	end = min_t(u64, end, extent_map_end(em));
	start_diff = start - em->start;
	em->start = start;
	em->len = end - start;
	if (em->block_start < EXTENT_MAP_LAST_BYTE &&
	    !test_bit(EXTENT_FLAG_COMPRESSED, &em->flags)) {
		em->block_start += start_diff;
		em->block_len = em->len;
	}
	return add_extent_mapping(em_tree, em, 0);
}

/**
 * btrfs_add_extent_mapping - add extent mapping into em_tree
 * @em_tree - the extent tree into which we want to insert the extent mapping
 * @em_in   - extent we are inserting
 * @start   - start of the logical range btrfs_get_extent() is requesting
 * @len     - length of the logical range btrfs_get_extent() is requesting
 *
 * Note that @em_in's range may be different from [start, start+len),
 * but they must be overlapped.
 *
 * Insert @em_in into @em_tree. In case there is an overlapping range, handle
 * the -EEXIST by either:
 * a) Returning the existing extent in @em_in if @start is within the
 *    existing em.
 * b) Merge the existing extent with @em_in passed in.
 *
 * Return 0 on success, otherwise -EEXIST.
 *
 */
int btrfs_add_extent_mapping(struct extent_map_tree *em_tree,
			     struct extent_map **em_in, u64 start, u64 len)
{
	int ret;
	struct extent_map *em = *em_in;

	ret = add_extent_mapping(em_tree, em, 0);
	/* it is possible that someone inserted the extent into the tree
	 * while we had the lock dropped.  It is also possible that
	 * an overlapping map exists in the tree
	 */
	if (ret == -EEXIST) {
		struct extent_map *existing;

		ret = 0;

		existing = search_extent_mapping(em_tree, start, len);
		/*
		 * existing will always be non-NULL, since there must be
		 * extent causing the -EEXIST.
		 */
		if (start >= existing->start &&
		    start < extent_map_end(existing)) {
			free_extent_map(em);
			*em_in = existing;
			ret = 0;
		} else {
			u64 orig_start = em->start;
			u64 orig_len = em->len;

			/*
			 * The existing extent map is the one nearest to
			 * the [start, start + len) range which overlaps
			 */
			ret = merge_extent_mapping(em_tree, existing,
						   em, start);
			if (ret) {
				free_extent_map(em);
				*em_in = NULL;
				WARN_ONCE(ret,
"unexpected error %d: merge existing(start %llu len %llu) with em(start %llu len %llu)\n",
					  ret, existing->start, existing->len,
					  orig_start, orig_len);
			}
			free_extent_map(existing);
		}
	}

	ASSERT(ret == 0 || ret == -EEXIST);
	return ret;
}
