#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#ifndef _UAPI_FALLOC_H_
#define _UAPI_FALLOC_H_

#define FALLOC_FL_KEEP_SIZE	0x01 /* default is extend size */
#define FALLOC_FL_PUNCH_HOLE	0x02 /* de-allocates range */
#define FALLOC_FL_NO_HIDE_STALE	0x04 /* reserved codepoint */

/*
 * FALLOC_FL_COLLAPSE_RANGE is used to remove a range of a file
 * without leaving a hole in the file. The contents of the file beyond
 * the range being removed is appended to the start offset of the range
 * being removed (i.e. the hole that was punched is "collapsed"),
 * resulting in a file layout that looks like the range that was
 * removed never existed. As such collapsing a range of a file changes
 * the size of the file, reducing it by the same length of the range
 * that has been removed by the operation.
 *
 * Different filesystems may implement different limitations on the
 * granularity of the operation. Most will limit operations to
 * filesystem block size boundaries, but this boundary may be larger or
 * smaller depending on the filesystem and/or the configuration of the
 * filesystem or file.
 *
 * Attempting to collapse a range that crosses the end of the file is
 * considered an illegal operation - just use ftruncate(2) if you need
 * to collapse a range that crosses EOF.
 */
#define FALLOC_FL_COLLAPSE_RANGE	0x08

/*
 * FALLOC_FL_ZERO_RANGE is used to convert a range of file to zeros preferably
 * without issuing data IO. Blocks should be preallocated for the regions that
 * span holes in the file, and the entire range is preferable converted to
 * unwritten extents - even though file system may choose to zero out the
 * extent or do whatever which will result in reading zeros from the range
 * while the range remains allocated for the file.
 *
 * This can be also used to preallocate blocks past EOF in the same way as
 * with fallocate. Flag FALLOC_FL_KEEP_SIZE should cause the inode
 * size to remain the same.
 */
#define FALLOC_FL_ZERO_RANGE		0x10

/*
 * FALLOC_FL_INSERT_RANGE is use to insert space within the file size without
 * overwriting any existing data. The contents of the file beyond offset are
 * shifted towards right by len bytes to create a hole.  As such, this
 * operation will increase the size of the file by len bytes.
 *
 * Different filesystems may implement different limitations on the granularity
 * of the operation. Most will limit operations to filesystem block size
 * boundaries, but this boundary may be larger or smaller depending on
 * the filesystem and/or the configuration of the filesystem or file.
 *
 * Attempting to insert space using this flag at OR beyond the end of
 * the file is considered an illegal operation - just use ftruncate(2) or
 * fallocate(2) with mode 0 for such type of operations.
 */
#define FALLOC_FL_INSERT_RANGE		0x20

#ifdef MY_DEF_HERE
/*
 * FALLOC_FL_MARK_WRITTEN is used to mark the newly allocated range of
 * file as written instead of prealloc. Without this flag, each fallocated
 * area will be marked as prealloc, and changed to written status when
 * first write occured within that range. This update requires metadata
 * update. Since we don't initialize the fallocated range and the
 * status is already marked as written, we'll read garbage data until
 * we first write those blocks.
 */
#define FALLOC_FL_MARK_WRITTEN		0x1000
#endif /* MY_DEF_HERE */

#endif /* _UAPI_FALLOC_H_ */
