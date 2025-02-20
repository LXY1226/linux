#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#ifndef _UAPI_LINUX_FS_H
#define _UAPI_LINUX_FS_H

/*
 * This file has definitions for some important file table
 * structures etc.
 */

#include <linux/limits.h>
#include <linux/ioctl.h>
#include <linux/types.h>

/*
 * It's silly to have NR_OPEN bigger than NR_FILE, but you can change
 * the file limit at runtime and only root can increase the per-process
 * nr_file rlimit, so it's safe to set up a ridiculously high absolute
 * upper limit on files-per-process.
 *
 * Some programs (notably those using select()) may have to be 
 * recompiled to take full advantage of the new limits..  
 */

/* Fixed constants first: */
#undef NR_OPEN
#define INR_OPEN_CUR 1024	/* Initial setting for nfile rlimits */
#define INR_OPEN_MAX 4096	/* Hard limit for nfile rlimits */

#define BLOCK_SIZE_BITS 10
#define BLOCK_SIZE (1<<BLOCK_SIZE_BITS)

#define SEEK_SET	0	/* seek relative to beginning of file */
#define SEEK_CUR	1	/* seek relative to current file position */
#define SEEK_END	2	/* seek relative to end of file */
#define SEEK_DATA	3	/* seek to the next data */
#define SEEK_HOLE	4	/* seek to the next hole */
#define SEEK_MAX	SEEK_HOLE

#define RENAME_NOREPLACE	(1 << 0)	/* Don't overwrite target */
#define RENAME_EXCHANGE		(1 << 1)	/* Exchange source and dest */
#define RENAME_WHITEOUT		(1 << 2)	/* Whiteout source */

struct file_clone_range {
	__s64 src_fd;
	__u64 src_offset;
	__u64 src_length;
	__u64 dest_offset;
};

struct fstrim_range {
	__u64 start;
	__u64 len;
	__u64 minlen;
};

/* And dynamically-tunable limits and defaults: */
struct files_stat_struct {
	unsigned long nr_files;		/* read only */
	unsigned long nr_free_files;	/* read only */
	unsigned long max_files;		/* tunable */
};

struct inodes_stat_t {
	long nr_inodes;
	long nr_unused;
	long dummy[5];		/* padding for sysctl ABI compatibility */
};

#ifdef MY_DEF_HERE
enum locker_mode {
	LM_NONE         = 0,
	LM_ENTERPRISE   = 1,
	LM_COMPLIANCE   = 2,
	LM_INHOUSE      = 3,
	LM_MAX          = LM_INHOUSE
};

enum locker_state {
	LS_OPEN         = 0,
	LS_IMMUTABLE    = 1,
	LS_APPENDABLE   = 2,
	LS_EXPIRED_I    = 3,
	LS_EXPIRED_A    = 4,
	LS_W_IMMUTABLE  = 5,
	LS_W_APPENDABLE = 6,
	LS_MAX          = LS_W_APPENDABLE
};
#endif /* MY_DEF_HERE */

#ifdef MY_ABC_HERE
enum SYNO_RBD_META_IOCTL_ACT {
	SYNO_RBD_META_ACTIVATE = 1,
	SYNO_RBD_META_DEACTIVATE = 2,
	SYNO_RBD_META_MAPPING = 3,
	SYNO_RBD_META_SET_FIRST_OFFSET = 4,
	SYNO_RBD_META_CLEANUP_ALL = 5,
	SYNO_RBD_META_MAPPING_COUNT = 6,
};

struct syno_rbd_meta_file_mapping {
	__u64 dev_offset;
	__u64 length;
};

struct syno_rbd_meta_ioctl_args {
	unsigned int act; /* enum SYNO_RBD_META_IOCTL_ACT */
	union {
		struct {
			__u64 first_offset;
		};
		struct {
			__u64 start;
			size_t size;
			__u64 cnt;
		};
	};
	struct syno_rbd_meta_file_mapping mappings[0];
};
#endif /* MY_ABC_HERE */

#ifdef MY_ABC_HERE
/* request */
#define SYNO_SPACE_USAGE_REQUEST_DATA_USED (1ULL << 0) /* Want/got data used */
#define SYNO_SPACE_USAGE_REQUEST_DATA_DELAY_ALLOCATED (1ULL << 1) /* Want/got data delay allocated */
#define SYNO_SPACE_USAGE_REQUEST_METADATA_USED (1ULL << 2) /* Want/got metadata used */

/* flags */
#define SYNO_SPACE_USAGE_FLAG_RESCAN (1ULL << 0)

struct syno_space_usage_info {
	/* in */
	__u64 request_mask;
	/* out */
	__u64 result_mask;
	__u64 flags;
	__u64 data_used;
	__u64 data_delay_allocated;
	__u64 metadata_used;
	__u64 reserved[10]; /* pad to 128 bytes */
};
#endif /* MY_ABC_HERE */

#define NR_FILE  8192	/* this can well be larger on a larger system */


/*
 * These are the fs-independent mount-flags: up to 32 flags are supported
 */
#define MS_RDONLY	 1	/* Mount read-only */
#define MS_NOSUID	 2	/* Ignore suid and sgid bits */
#define MS_NODEV	 4	/* Disallow access to device special files */
#define MS_NOEXEC	 8	/* Disallow program execution */
#define MS_SYNCHRONOUS	16	/* Writes are synced at once */
#define MS_REMOUNT	32	/* Alter flags of a mounted FS */
#define MS_MANDLOCK	64	/* Allow mandatory locks on an FS */
#define MS_DIRSYNC	128	/* Directory modifications are synchronous */
#define MS_NOATIME	1024	/* Do not update access times. */
#define MS_NODIRATIME	2048	/* Do not update directory access times */
#define MS_BIND		4096
#define MS_MOVE		8192
#define MS_REC		16384
#define MS_VERBOSE	32768	/* War is peace. Verbosity is silence.
				   MS_VERBOSE is deprecated. */
#define MS_SILENT	32768
#define MS_POSIXACL	(1<<16)	/* VFS does not apply the umask */
#define MS_UNBINDABLE	(1<<17)	/* change to unbindable */
#define MS_PRIVATE	(1<<18)	/* change to private */
#define MS_SLAVE	(1<<19)	/* change to slave */
#define MS_SHARED	(1<<20)	/* change to shared */
#define MS_RELATIME	(1<<21)	/* Update atime relative to mtime/ctime. */
#define MS_KERNMOUNT	(1<<22) /* this is a kern_mount call */
#define MS_I_VERSION	(1<<23) /* Update inode I_version field */
#define MS_STRICTATIME	(1<<24) /* Always perform atime updates */
#define MS_LAZYTIME	(1<<25) /* Update the on-disk [acm]times lazily */
#ifdef MY_ABC_HERE
#define MS_SYNOACL	(1<<26)	/* Synology ACL */
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
#define MS_ROOTPRJQUOTA (1<<27)
#endif /* MY_ABC_HERE */

/* These sb flags are internal to the kernel */
#define MS_NOSEC	(1<<28)
#define MS_BORN		(1<<29)
#define MS_ACTIVE	(1<<30)
#define MS_NOUSER	(1<<31)

/*
 * Superblock flags that can be altered by MS_REMOUNT
 */
#define MS_RMT_MASK	(MS_RDONLY|MS_SYNCHRONOUS|MS_MANDLOCK|MS_I_VERSION|\
			 MS_LAZYTIME)

/*
 * Old magic mount flag and mask
 */
#define MS_MGC_VAL 0xC0ED0000
#define MS_MGC_MSK 0xffff0000

/* the read-only stuff doesn't really belong here, but any other place is
   probably as bad and I don't want to create yet another include file. */

#define BLKROSET   _IO(0x12,93)	/* set device read-only (0 = read-write) */
#define BLKROGET   _IO(0x12,94)	/* get read-only status (0 = read_write) */
#define BLKRRPART  _IO(0x12,95)	/* re-read partition table */
#define BLKGETSIZE _IO(0x12,96)	/* return device size /512 (long *arg) */
#define BLKFLSBUF  _IO(0x12,97)	/* flush buffer cache */
#define BLKRASET   _IO(0x12,98)	/* set read ahead for block device */
#define BLKRAGET   _IO(0x12,99)	/* get current read ahead setting */
#define BLKFRASET  _IO(0x12,100)/* set filesystem (mm/filemap.c) read-ahead */
#define BLKFRAGET  _IO(0x12,101)/* get filesystem (mm/filemap.c) read-ahead */
#define BLKSECTSET _IO(0x12,102)/* set max sectors per request (ll_rw_blk.c) */
#define BLKSECTGET _IO(0x12,103)/* get max sectors per request (ll_rw_blk.c) */
#define BLKSSZGET  _IO(0x12,104)/* get block device sector size */
#if 0
#define BLKPG      _IO(0x12,105)/* See blkpg.h */

/* Some people are morons.  Do not use sizeof! */

#define BLKELVGET  _IOR(0x12,106,size_t)/* elevator get */
#define BLKELVSET  _IOW(0x12,107,size_t)/* elevator set */
/* This was here just to show that the number is taken -
   probably all these _IO(0x12,*) ioctls should be moved to blkpg.h. */
#endif
/* A jump here: 108-111 have been used for various private purposes. */
#define BLKBSZGET  _IOR(0x12,112,size_t)
#define BLKBSZSET  _IOW(0x12,113,size_t)
#define BLKGETSIZE64 _IOR(0x12,114,size_t)	/* return device size in bytes (u64 *arg) */
#define BLKTRACESETUP _IOWR(0x12,115,struct blk_user_trace_setup)
#define BLKTRACESTART _IO(0x12,116)
#define BLKTRACESTOP _IO(0x12,117)
#define BLKTRACETEARDOWN _IO(0x12,118)
#define BLKDISCARD _IO(0x12,119)
#define BLKIOMIN _IO(0x12,120)
#define BLKIOOPT _IO(0x12,121)
#define BLKALIGNOFF _IO(0x12,122)
#define BLKPBSZGET _IO(0x12,123)
#define BLKDISCARDZEROES _IO(0x12,124)
#define BLKSECDISCARD _IO(0x12,125)
#define BLKROTATIONAL _IO(0x12,126)
#define BLKZEROOUT _IO(0x12,127)
#define BLKDAXSET _IO(0x12,128)
#define BLKDAXGET _IO(0x12,129)
#ifdef MY_ABC_HERE
#define BLKHINTUNUSED _IO(0x12,140)
#endif /* MY_ABC_HERE */

#define BMAP_IOCTL 1		/* obsolete - kept for compatibility */
#define FIBMAP	   _IO(0x00,1)	/* bmap access */
#define FIGETBSZ   _IO(0x00,2)	/* get the block size used for bmap */
#define FIFREEZE	_IOWR('X', 119, int)	/* Freeze */
#define FITHAW		_IOWR('X', 120, int)	/* Thaw */
#define FITRIM		_IOWR('X', 121, struct fstrim_range)	/* Trim */
#define FICLONE		_IOW(0x94, 9, int)
#define FICLONERANGE	_IOW(0x94, 13, struct file_clone_range)

#ifdef MY_ABC_HERE
#define FIGETVERSION			_IOWR('x', 122, unsigned int)	/* get syno archive version */
#define FISETVERSION			_IOWR('x', 123, unsigned int)	/* set syno archive version */
#define FIINCVERSION			_IO('x', 124)	/* increase syno archive version by 1 */
#define FISETFILEVERSION		_IOWR('x', 125, unsigned int)	/* set file syno archive version */
#ifdef MY_ABC_HERE
#define FIGETBADVERSION			_IOWR('x', 126, unsigned int)	/* fix bad archive version */
#define FICLEARBADVERSION		_IO('x', 127)	/* fix bad archive version */
#define FISETBADVERSION			_IOWR('x', 128, unsigned int)	/* fix bad archive version */
#endif /* MY_ABC_HERE */
#endif /* MY_ABC_HERE */

#ifdef MY_ABC_HERE
#define FIHINTUNUSED			_IOWR('x', 129, unsigned int)	/* search unused space as hints */
#endif /* MY_ABC_HERE */

#ifdef MY_ABC_HERE
#define FICTRRBDMETA			_IOWR('x', 130, unsigned int)	/* control syno rbd meta */
#endif /* MY_ABC_HERE */

#ifdef MY_ABC_HERE
#define FISPACEUSAGE			_IOWR('x', 131, struct syno_space_usage_info)	/* get space usage */
#endif /* MY_ABC_HERE */

#define	FS_IOC_GETFLAGS			_IOR('f', 1, long)
#define	FS_IOC_SETFLAGS			_IOW('f', 2, long)
#define	FS_IOC_GETVERSION		_IOR('v', 1, long)
#define	FS_IOC_SETVERSION		_IOW('v', 2, long)
#define FS_IOC_FIEMAP			_IOWR('f', 11, struct fiemap)
#define FS_IOC32_GETFLAGS		_IOR('f', 1, int)
#define FS_IOC32_SETFLAGS		_IOW('f', 2, int)
#define FS_IOC32_GETVERSION		_IOR('v', 1, int)
#define FS_IOC32_SETVERSION		_IOW('v', 2, int)

/*
 * Inode flags (FS_IOC_GETFLAGS / FS_IOC_SETFLAGS)
 */
#define	FS_SECRM_FL			0x00000001 /* Secure deletion */
#define	FS_UNRM_FL			0x00000002 /* Undelete */
#define	FS_COMPR_FL			0x00000004 /* Compress file */
#define FS_SYNC_FL			0x00000008 /* Synchronous updates */
#define FS_IMMUTABLE_FL			0x00000010 /* Immutable file */
#define FS_APPEND_FL			0x00000020 /* writes to file may only append */
#define FS_NODUMP_FL			0x00000040 /* do not dump file */
#define FS_NOATIME_FL			0x00000080 /* do not update atime */
/* Reserved for compression usage... */
#define FS_DIRTY_FL			0x00000100
#define FS_COMPRBLK_FL			0x00000200 /* One or more compressed clusters */
#define FS_NOCOMP_FL			0x00000400 /* Don't compress */
#define FS_ECOMPR_FL			0x00000800 /* Compression error */
/* End compression flags --- maybe not all used */
#define FS_BTREE_FL			0x00001000 /* btree format dir */
#define FS_INDEX_FL			0x00001000 /* hash-indexed directory */
#define FS_IMAGIC_FL			0x00002000 /* AFS directory */
#define FS_JOURNAL_DATA_FL		0x00004000 /* Reserved for ext3 */
#define FS_NOTAIL_FL			0x00008000 /* file tail should not be merged */
#define FS_DIRSYNC_FL			0x00010000 /* dirsync behaviour (directories only) */
#define FS_TOPDIR_FL			0x00020000 /* Top of directory hierarchies*/
#define FS_EXTENT_FL			0x00080000 /* Extents */
#define FS_DIRECTIO_FL			0x00100000 /* Use direct i/o */
#define FS_NOCOW_FL			0x00800000 /* Do not cow file */
#define FS_PROJINHERIT_FL		0x20000000 /* Create with parents projid */
#define FS_RESERVED_FL			0x80000000 /* reserved for ext2 lib */

#define FS_FL_USER_VISIBLE		0x0003DFFF /* User visible flags */
#define FS_FL_USER_MODIFIABLE		0x000380FF /* User modifiable flags */


#define SYNC_FILE_RANGE_WAIT_BEFORE	1
#define SYNC_FILE_RANGE_WRITE		2
#define SYNC_FILE_RANGE_WAIT_AFTER	4


#endif /* _UAPI_LINUX_FS_H */
