#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#ifndef __EXTENTIO__
#define __EXTENTIO__

#include <linux/rbtree.h>
#include "ulist.h"

/* bits for the extent state */
#define EXTENT_DIRTY		(1U << 0)
#define EXTENT_WRITEBACK	(1U << 1)
#define EXTENT_UPTODATE		(1U << 2)
#define EXTENT_LOCKED		(1U << 3)
#define EXTENT_NEW		(1U << 4)
#define EXTENT_DELALLOC		(1U << 5)
#define EXTENT_DEFRAG		(1U << 6)
#define EXTENT_BOUNDARY		(1U << 9)
#define EXTENT_NODATASUM	(1U << 10)
#define EXTENT_CLEAR_META_RESV	(1U << 11)
#define EXTENT_FIRST_DELALLOC	(1U << 12)
#define EXTENT_NEED_WAIT	(1U << 13)
#define EXTENT_DAMAGED		(1U << 14)
#define EXTENT_NORESERVE	(1U << 15)
#define EXTENT_QGROUP_RESERVED	(1U << 16)
#define EXTENT_CLEAR_DATA_RESV	(1U << 17)
/*
 * Must be cleared only during ordered extent completion or on error paths if we
 * did not manage to submit bios and create the ordered extents for the range.
 * Should not be cleared during page release and page invalidation (if there is
 * an ordered extent in flight), that is left for the ordered extent completion.
 */
#define EXTENT_DELALLOC_NEW	(1U << 18)
/*
 * When an ordered extent successfully completes for a region marked as a new
 * delalloc range, use this flag when clearing a new delalloc range to indicate
 * that the VFS' inode number of bytes should be incremented and the inode's new
 * delalloc bytes decremented, in an atomic way to prevent races with stat(2).
 */
#define EXTENT_ADD_INODE_BYTES  (1U << 19)
#define EXTENT_IOBITS		(EXTENT_LOCKED | EXTENT_WRITEBACK)
#define EXTENT_DO_ACCOUNTING    (EXTENT_CLEAR_META_RESV | \
				 EXTENT_CLEAR_DATA_RESV)
#define EXTENT_CTLBITS		(EXTENT_DO_ACCOUNTING | EXTENT_FIRST_DELALLOC | EXTENT_ADD_INODE_BYTES)

/*
 * flags for bio submission. The high bits indicate the compression
 * type for this bio
 */
#define EXTENT_BIO_COMPRESSED 1
#define EXTENT_BIO_TREE_LOG 2
#ifdef MY_DEF_HERE
#define EXTENT_BIO_RETRY 8
// EXTENT_BIO_FLAG_SHIFT is not a bit flag, we are safe here.
#define EXTENT_BIO_ABORT 16
#endif /* MY_DEF_HERE */
#define EXTENT_BIO_FLAG_SHIFT 16

/* these are bit numbers for test/set bit */
#define EXTENT_BUFFER_UPTODATE 0
#define EXTENT_BUFFER_DIRTY 2
#define EXTENT_BUFFER_CORRUPT 3
#define EXTENT_BUFFER_READAHEAD 4	/* this got triggered by readahead */
#define EXTENT_BUFFER_TREE_REF 5
#define EXTENT_BUFFER_STALE 6
#define EXTENT_BUFFER_WRITEBACK 7
#define EXTENT_BUFFER_READ_ERR 8        /* read IO error */
#define EXTENT_BUFFER_DUMMY 9
#define EXTENT_BUFFER_IN_TREE 10
#define EXTENT_BUFFER_WRITE_ERR 11    /* write IO error */
#ifdef MY_DEF_HERE
#define EXTENT_BUFFER_SHOULD_REPAIR 31    /* one and only one process can do the repair in repair_eb_io_failure() */
#define EXTENT_BUFFER_RETRY_ERR 32    /* no more redundancies in lower layer */
#endif

/* these are flags for extent_clear_unlock_delalloc */
#define PAGE_UNLOCK		(1 << 0)
#define PAGE_CLEAR_DIRTY	(1 << 1)
#define PAGE_SET_WRITEBACK	(1 << 2)
#define PAGE_END_WRITEBACK	(1 << 3)
#define PAGE_SET_PRIVATE2	(1 << 4)
#define PAGE_SET_ERROR		(1 << 5)

/*
 * page->private values.  Every page that is controlled by the extent
 * map has page->private set to one.
 */
#define EXTENT_PAGE_PRIVATE 1

/*
 * The extent buffer bitmap operations are done with byte granularity instead of
 * word granularity for two reasons:
 * 1. The bitmaps must be little-endian on disk.
 * 2. Bitmap items are not guaranteed to be aligned to a word and therefore a
 *    single word in a bitmap may straddle two pages in the extent buffer.
 */
#define BIT_BYTE(nr) ((nr) / BITS_PER_BYTE)
#define BYTE_MASK ((1 << BITS_PER_BYTE) - 1)
#define BITMAP_FIRST_BYTE_MASK(start) \
	((BYTE_MASK << ((start) & (BITS_PER_BYTE - 1))) & BYTE_MASK)
#define BITMAP_LAST_BYTE_MASK(nbits) \
	(BYTE_MASK >> (-(nbits) & (BITS_PER_BYTE - 1)))

static inline int le_test_bit(int nr, const u8 *addr)
{
	return 1U & (addr[BIT_BYTE(nr)] >> (nr & (BITS_PER_BYTE-1)));
}

extern void le_bitmap_set(u8 *map, unsigned int start, int len);
extern void le_bitmap_clear(u8 *map, unsigned int start, int len);

struct extent_state;
struct btrfs_root;
struct btrfs_io_bio;
struct io_failure_record;

typedef	int (extent_submit_bio_hook_t)(struct inode *inode, int rw,
				       struct bio *bio, int mirror_num,
				       unsigned long bio_flags, u64 bio_offset);
struct extent_io_ops {
#ifdef MY_DEF_HERE
	int (*fill_delalloc)(struct inode *inode, struct page *locked_page,
			     u64 start, u64 end, int *page_started,
			     unsigned long *nr_written, int write_sync);
#else
	int (*fill_delalloc)(struct inode *inode, struct page *locked_page,
			     u64 start, u64 end, int *page_started,
			     unsigned long *nr_written);
#endif /* MY_DEF_HERE */
	int (*writepage_start_hook)(struct page *page, u64 start, u64 end);
	extent_submit_bio_hook_t *submit_bio_hook;
	int (*merge_bio_hook)(int rw, struct page *page, unsigned long offset,
			      size_t size, struct bio *bio,
			      unsigned long bio_flags);
#ifdef MY_DEF_HERE
	int (*readpage_io_failed_hook)(struct page *page, int failed_mirror, int correction_err);
#else
	int (*readpage_io_failed_hook)(struct page *page, int failed_mirror);
#endif /* MY_DEF_HERE */
	int (*readpage_end_io_hook)(struct btrfs_io_bio *io_bio, u64 phy_offset,
				    struct page *page, u64 start, u64 end,
				    int mirror);
	int (*writepage_end_io_hook)(struct page *page, u64 start, u64 end,
				      struct extent_state *state, int uptodate);
	void (*set_bit_hook)(struct inode *inode, struct extent_state *state,
			     unsigned *bits);
	void (*clear_bit_hook)(struct inode *inode, struct extent_state *state,
			       unsigned *bits
#ifdef MY_DEF_HERE
			       , u64 *add_bytes
#endif /* MY_DEF_HERE */
			       );
	void (*merge_extent_hook)(struct inode *inode,
				  struct extent_state *new,
				  struct extent_state *other);
	void (*split_extent_hook)(struct inode *inode,
				  struct extent_state *orig, u64 split);
};

struct extent_io_tree {
	struct rb_root state;
	struct address_space *mapping;
	u64 dirty_bytes;
	int track_uptodate;
	spinlock_t lock;
	const struct extent_io_ops *ops;
};

struct extent_state {
	u64 start;
	u64 end; /* inclusive */
	struct rb_node rb_node;

	/* ADD NEW ELEMENTS AFTER THIS */
	wait_queue_head_t wq;
	atomic_t refs;
	unsigned state;

	struct io_failure_record *failrec;

#ifdef CONFIG_BTRFS_DEBUG
	struct list_head leak_list;
#endif
};

#ifdef MY_DEF_HERE
#define EXTENT_BUFFER_SHOULD_ABORT_RETRY ((u8)-2)
#define EXTENT_BUFFER_RETRY_ABORTED ((u8)-1)
#endif /* MY_DEF_HERE */

#ifdef MY_DEF_HERE
#define INLINE_EXTENT_BUFFER_PAGES 4
#else
#define INLINE_EXTENT_BUFFER_PAGES 16
#endif /* MY_DEF_HERE */
#define MAX_INLINE_EXTENT_BUFFER_SIZE (INLINE_EXTENT_BUFFER_PAGES * PAGE_CACHE_SIZE)
struct extent_buffer {
	u64 start;
	unsigned long len;
	unsigned long bflags;
	struct btrfs_fs_info *fs_info;
	spinlock_t refs_lock;
	atomic_t refs;
	atomic_t io_pages;
	int read_mirror;
	struct rcu_head rcu_head;
	pid_t lock_owner;

	/* count of read lock holders on the extent buffer */
	atomic_t write_locks;
	atomic_t read_locks;
	atomic_t blocking_writers;
	atomic_t blocking_readers;
	atomic_t spinning_readers;
	atomic_t spinning_writers;
	short lock_nested;
	/* >= 0 if eb belongs to a log tree, -1 otherwise */
	short log_index;

	/* protects write locks */
	rwlock_t lock;

	/* readers use lock_wq while they wait for the write
	 * lock holders to unlock
	 */
	wait_queue_head_t write_lock_wq;

	/* writers use read_lock_wq while they wait for readers
	 * to unlock
	 */
	wait_queue_head_t read_lock_wq;
#ifdef MY_DEF_HERE
	u8 nr_retry;
	u8 can_retry;
	u32 prev_bad_csum;
	u64 parent_transid;
	u64 prev_bad_transid;
#endif /* MY_DEF_HERE */
	struct page *pages[INLINE_EXTENT_BUFFER_PAGES];
#ifdef CONFIG_BTRFS_DEBUG
	struct list_head leak_list;
#endif
};

/*
 * Structure to record how many bytes and which ranges are set/cleared
 */
struct extent_changeset {
	/* How many bytes are set/cleared in this operation */
	u64 bytes_changed;

	/* Changed ranges */
	struct ulist *range_changed;
#ifdef MY_DEF_HERE
	struct ulist_node *prealloc_ulist_node;
#endif /* MY_DEF_HERE */
};

static inline void extent_set_compress_type(unsigned long *bio_flags,
					    int compress_type)
{
	*bio_flags |= compress_type << EXTENT_BIO_FLAG_SHIFT;
}

static inline int extent_compress_type(unsigned long bio_flags)
{
	return bio_flags >> EXTENT_BIO_FLAG_SHIFT;
}

struct extent_map_tree;

typedef struct extent_map *(get_extent_t)(struct inode *inode,
					  struct page *page,
					  size_t pg_offset,
					  u64 start, u64 len,
					  int create);

void extent_io_tree_init(struct extent_io_tree *tree,
			 struct address_space *mapping);
void extent_io_tree_release(struct extent_io_tree *tree);
int try_release_extent_mapping(struct extent_map_tree *map,
			       struct extent_io_tree *tree, struct page *page,
			       gfp_t mask);
int try_release_extent_buffer(struct page *page);
int lock_extent_bits(struct extent_io_tree *tree, u64 start, u64 end,
		     struct extent_state **cached);

static inline int lock_extent(struct extent_io_tree *tree, u64 start, u64 end)
{
	return lock_extent_bits(tree, start, end, NULL);
}

int try_lock_extent(struct extent_io_tree *tree, u64 start, u64 end);
int extent_read_full_page(struct extent_io_tree *tree, struct page *page,
			  get_extent_t *get_extent, int mirror_num);
int __init extent_io_init(void);
void extent_io_exit(void);

u64 count_range_bits(struct extent_io_tree *tree,
		     u64 *start, u64 search_end,
		     u64 max_bytes, unsigned bits, int contig);

void free_extent_state(struct extent_state *state);
int test_range_bit(struct extent_io_tree *tree, u64 start, u64 end,
		   unsigned bits, int filled,
		   struct extent_state *cached_state);
int clear_record_extent_bits(struct extent_io_tree *tree, u64 start, u64 end,
		unsigned bits, struct extent_changeset *changeset);
int clear_extent_bit(struct extent_io_tree *tree, u64 start, u64 end,
		     unsigned bits, int wake, int delete,
		     struct extent_state **cached, gfp_t mask);

static inline int unlock_extent(struct extent_io_tree *tree, u64 start, u64 end)
{
	return clear_extent_bit(tree, start, end, EXTENT_LOCKED, 1, 0, NULL,
				GFP_NOFS);
}

static inline int unlock_extent_cached(struct extent_io_tree *tree, u64 start,
		u64 end, struct extent_state **cached, gfp_t mask)
{
	return clear_extent_bit(tree, start, end, EXTENT_LOCKED, 1, 0, cached,
				mask);
}

static inline int clear_extent_bits(struct extent_io_tree *tree, u64 start,
		u64 end, unsigned bits)
{
	int wake = 0;

	if (bits & EXTENT_LOCKED)
		wake = 1;

	return clear_extent_bit(tree, start, end, bits, wake, 0, NULL,
			GFP_NOFS);
}

int set_record_extent_bits(struct extent_io_tree *tree, u64 start, u64 end,
			   unsigned bits, struct extent_changeset *changeset);
int set_extent_bit(struct extent_io_tree *tree, u64 start, u64 end,
		   unsigned bits, u64 *failed_start,
		   struct extent_state **cached_state, gfp_t mask);

static inline int set_extent_bits(struct extent_io_tree *tree, u64 start,
		u64 end, unsigned bits)
{
	return set_extent_bit(tree, start, end, bits, NULL, NULL, GFP_NOFS);
}

static inline int clear_extent_uptodate(struct extent_io_tree *tree, u64 start,
		u64 end, struct extent_state **cached_state, gfp_t mask)
{
	return clear_extent_bit(tree, start, end, EXTENT_UPTODATE, 0, 0,
				cached_state, mask);
}

static inline int set_extent_dirty(struct extent_io_tree *tree, u64 start,
		u64 end, gfp_t mask)
{
	return set_extent_bit(tree, start, end, EXTENT_DIRTY, NULL,
			      NULL, mask);
}

static inline int clear_extent_dirty(struct extent_io_tree *tree, u64 start,
		u64 end)
{
	return clear_extent_bit(tree, start, end,
				EXTENT_DIRTY | EXTENT_DELALLOC |
				EXTENT_DO_ACCOUNTING, 0, 0, NULL, GFP_NOFS);
}

int convert_extent_bit(struct extent_io_tree *tree, u64 start, u64 end,
		       unsigned bits, unsigned clear_bits,
		       struct extent_state **cached_state);

static inline int set_extent_delalloc(struct extent_io_tree *tree, u64 start,
				      u64 end, unsigned int extra_bits,
				      struct extent_state **cached_state)
{
	return set_extent_bit(tree, start, end,
			      EXTENT_DELALLOC | extra_bits,
			      NULL, cached_state, GFP_NOFS);
}

static inline int set_extent_defrag(struct extent_io_tree *tree, u64 start,
		u64 end, struct extent_state **cached_state)
{
	return set_extent_bit(tree, start, end,
			      EXTENT_DELALLOC | EXTENT_DEFRAG,
			      NULL, cached_state, GFP_NOFS);
}

static inline int set_extent_new(struct extent_io_tree *tree, u64 start,
		u64 end)
{
	return set_extent_bit(tree, start, end, EXTENT_NEW, NULL, NULL,
			GFP_NOFS);
}

static inline int set_extent_uptodate(struct extent_io_tree *tree, u64 start,
		u64 end, struct extent_state **cached_state, gfp_t mask)
{
	return set_extent_bit(tree, start, end, EXTENT_UPTODATE, NULL,
			      cached_state, mask);
}

int find_first_extent_bit(struct extent_io_tree *tree, u64 start,
			  u64 *start_ret, u64 *end_ret, unsigned bits,
			  struct extent_state **cached_state);
int extent_invalidatepage(struct extent_io_tree *tree,
			  struct page *page, unsigned long offset);
int extent_write_full_page(struct extent_io_tree *tree, struct page *page,
			  get_extent_t *get_extent,
			  struct writeback_control *wbc);
int extent_write_locked_range(struct extent_io_tree *tree, struct inode *inode,
			      u64 start, u64 end, get_extent_t *get_extent,
			      int mode);
int extent_writepages(struct extent_io_tree *tree,
		      struct address_space *mapping,
		      get_extent_t *get_extent,
		      struct writeback_control *wbc);
#ifdef MY_DEF_HERE
int syno_cache_protection_extent_writepages(struct extent_io_tree *tree,
		      struct address_space *mapping,
		      get_extent_t *get_extent,
		      struct writeback_control *wbc);
#endif /* MY_DEF_HERE */
int btree_write_cache_pages(struct address_space *mapping,
			    struct writeback_control *wbc);
int extent_readpages(struct extent_io_tree *tree,
		     struct address_space *mapping,
		     struct list_head *pages, unsigned nr_pages,
		     get_extent_t get_extent);
int extent_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
		__u64 start, __u64 len, get_extent_t *get_extent);
void set_page_extent_mapped(struct page *page);

#ifdef MY_DEF_HERE
struct extent_buffer *alloc_extent_buffer(struct btrfs_root *root,
					  u64 start);
#else
struct extent_buffer *alloc_extent_buffer(struct btrfs_fs_info *fs_info,
					  u64 start);
#endif /* MY_DEF_HERE */
struct extent_buffer *__alloc_dummy_extent_buffer(struct btrfs_fs_info *fs_info,
						  u64 start, unsigned long len);
struct extent_buffer *alloc_dummy_extent_buffer(struct btrfs_fs_info *fs_info,
						u64 start);
struct extent_buffer *btrfs_clone_extent_buffer(struct extent_buffer *src);
#ifdef MY_DEF_HERE
struct extent_buffer *find_extent_buffer(struct btrfs_root *root,
					 u64 start);
#else
struct extent_buffer *find_extent_buffer(struct btrfs_fs_info *fs_info,
					 u64 start);
#endif /* MY_DEF_HERE */
void free_extent_buffer(struct extent_buffer *eb);
void free_extent_buffer_stale(struct extent_buffer *eb);
#define WAIT_NONE	0
#define WAIT_COMPLETE	1
#define WAIT_PAGE_LOCK	2
#ifdef MY_DEF_HERE
int read_extent_buffer_pages(struct extent_io_tree *tree,
			     struct extent_buffer *eb, int wait,
			     get_extent_t *get_extent, int mirror_num, int can_retry, u64 parent_transid);
#else
int read_extent_buffer_pages(struct extent_io_tree *tree,
			     struct extent_buffer *eb, int wait,
			     get_extent_t *get_extent, int mirror_num);
#endif /* MY_DEF_HERE */
void wait_on_extent_buffer_writeback(struct extent_buffer *eb);

static inline unsigned long num_extent_pages(u64 start, u64 len)
{
	return ((start + len + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT) -
		(start >> PAGE_CACHE_SHIFT);
}

static inline void extent_buffer_get(struct extent_buffer *eb)
{
	atomic_inc(&eb->refs);
}

#ifdef MY_DEF_HERE
int memcmp_caseless_extent_buffer(struct extent_buffer *eb, const void *ptrv,
			  unsigned long len_ptrv,
			  unsigned long start,
			  unsigned long len);
#endif /* MY_DEF_HERE */
int memcmp_extent_buffer(const struct extent_buffer *eb, const void *ptrv,
			 unsigned long start, unsigned long len);
void read_extent_buffer(const struct extent_buffer *eb, void *dst,
			unsigned long start,
			unsigned long len);
int read_extent_buffer_to_user_nofault(const struct extent_buffer *eb,
				       void __user *dst, unsigned long start,
				       unsigned long len);
void write_extent_buffer_fsid(struct extent_buffer *eb, const void *src);
void write_extent_buffer_chunk_tree_uuid(struct extent_buffer *eb,
		const void *src);
void write_extent_buffer(struct extent_buffer *eb, const void *src,
			 unsigned long start, unsigned long len);
void copy_extent_buffer_full(struct extent_buffer *dst,
			     struct extent_buffer *src);
void copy_extent_buffer(struct extent_buffer *dst, struct extent_buffer *src,
			unsigned long dst_offset, unsigned long src_offset,
			unsigned long len);
void memcpy_extent_buffer(struct extent_buffer *dst, unsigned long dst_offset,
			   unsigned long src_offset, unsigned long len);
void memmove_extent_buffer(struct extent_buffer *dst, unsigned long dst_offset,
			   unsigned long src_offset, unsigned long len);
void memzero_extent_buffer(struct extent_buffer *eb, unsigned long start,
			   unsigned long len);
int extent_buffer_test_bit(struct extent_buffer *eb, unsigned long start,
			   unsigned long pos);
void extent_buffer_bitmap_set(struct extent_buffer *eb, unsigned long start,
			      unsigned long pos, unsigned long len);
void extent_buffer_bitmap_clear(struct extent_buffer *eb, unsigned long start,
				unsigned long pos, unsigned long len);
void clear_extent_buffer_dirty(struct extent_buffer *eb);
bool set_extent_buffer_dirty(struct extent_buffer *eb);
void set_extent_buffer_uptodate(struct extent_buffer *eb);
void clear_extent_buffer_uptodate(struct extent_buffer *eb);
int extent_buffer_uptodate(struct extent_buffer *eb);
int extent_buffer_under_io(struct extent_buffer *eb);
int map_private_extent_buffer(const struct extent_buffer *eb,
			      unsigned long offset, unsigned long min_len,
			      char **map, unsigned long *map_start,
			      unsigned long *map_len);
void extent_range_clear_dirty_for_io(struct inode *inode, u64 start, u64 end);
void extent_range_redirty_for_io(struct inode *inode, u64 start, u64 end);
void extent_clear_unlock_delalloc(struct inode *inode, u64 start, u64 end,
				 struct page *locked_page,
				 unsigned bits_to_clear,
				 unsigned long page_ops);
struct bio *
btrfs_bio_alloc(struct block_device *bdev, u64 first_sector, int nr_vecs,
#ifdef MY_DEF_HERE
		gfp_t gfp_flags, int rw);
#else
		gfp_t gfp_flags);
#endif /* MY_DEF_HERE */
struct bio *btrfs_io_bio_alloc(gfp_t gfp_mask, unsigned int nr_iovecs);
struct bio *btrfs_bio_clone(struct bio *bio, gfp_t gfp_mask);

struct btrfs_fs_info;

#ifdef MY_DEF_HERE
int repair_io_failure(struct inode *inode, u64 start, u64 length, u64 logical,
		      struct page *page, unsigned int pg_offset,
		      int mirror_num, int abort_correction);
#else
int repair_io_failure(struct inode *inode, u64 start, u64 length, u64 logical,
		      struct page *page, unsigned int pg_offset,
		      int mirror_num);
#endif /* MY_DEF_HERE */
int clean_io_failure(struct inode *inode, u64 start, struct page *page,
		     unsigned int pg_offset);
void end_extent_writepage(struct page *page, int err, u64 start, u64 end);
int btrfs_repair_eb_io_failure(struct extent_buffer *eb, int mirror_num);

/*
 * When IO fails, either with EIO or csum verification fails, we
 * try other mirrors that might have a good copy of the data.  This
 * io_failure_record is used to record state as we go through all the
 * mirrors.  If another mirror has good data, the page is set up to date
 * and things continue.  If a good mirror can't be found, the original
 * bio end_io callback is called to indicate things have failed.
 */
struct io_failure_record {
	struct page *page;
	u64 start;
	u64 len;
	u64 logical;
	unsigned long bio_flags;
	int this_mirror;
	int failed_mirror;
	int in_validation;
#ifdef MY_DEF_HERE
	bool io_error;
#endif /* MY_DEF_HERE */
};

void btrfs_free_io_failure_record(struct inode *inode, u64 start, u64 end);
#ifdef MY_DEF_HERE
int btrfs_get_io_failure_record(struct inode *inode, u64 start, u64 end,
				struct io_failure_record **failrec_ret,
				struct page *page, bool io_error);
#else
int btrfs_get_io_failure_record(struct inode *inode, u64 start, u64 end,
				struct io_failure_record **failrec_ret);
#endif /* MY_DEF_HERE */
int btrfs_check_repairable(struct inode *inode, struct bio *failed_bio,
			   struct io_failure_record *failrec, int fail_mirror);
struct bio *btrfs_create_repair_bio(struct inode *inode, struct bio *failed_bio,
				    struct io_failure_record *failrec,
				    struct page *page, int pg_offset, int icsum,
				    bio_end_io_t *endio_func, void *data);
int free_io_failure(struct inode *inode, struct io_failure_record *rec);
#ifdef CONFIG_BTRFS_FS_RUN_SANITY_TESTS
noinline u64 find_lock_delalloc_range(struct inode *inode,
				      struct extent_io_tree *tree,
				      struct page *locked_page, u64 *start,
				      u64 *end, u64 max_bytes);
#endif
struct extent_buffer *alloc_test_extent_buffer(struct btrfs_fs_info *fs_info,
					       u64 start);
#ifdef MY_DEF_HERE
void add_cksumfailed_file(u64 rootid, u64 i_ino, struct btrfs_fs_info *fs_info);

struct correction_record {
	struct rb_node node;
	u64 logical;
};

void correction_get_locked_record(struct btrfs_fs_info *fs_info, u64 logical);
void correction_put_locked_record(struct btrfs_fs_info *fs_info, u64 logical);
void correction_destroy_locked_record(struct btrfs_fs_info *fs_info);
#endif /* MY_DEF_HERE */

#endif
