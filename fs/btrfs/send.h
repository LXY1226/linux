#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Copyright (C) 2012 Alexander Block.  All rights reserved.
 * Copyright (C) 2012 STRATO.  All rights reserved.
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

#include "ctree.h"

#define BTRFS_SEND_STREAM_MAGIC "btrfs-stream"
#define BTRFS_SEND_STREAM_VERSION 1

#define BTRFS_SEND_BUF_SIZE SZ_64K
#define BTRFS_SEND_READ_SIZE (48 * SZ_1K)

enum btrfs_tlv_type {
	BTRFS_TLV_U8,
	BTRFS_TLV_U16,
	BTRFS_TLV_U32,
	BTRFS_TLV_U64,
	BTRFS_TLV_BINARY,
	BTRFS_TLV_STRING,
	BTRFS_TLV_UUID,
	BTRFS_TLV_TIMESPEC,
};

struct btrfs_stream_header {
	char magic[sizeof(BTRFS_SEND_STREAM_MAGIC)];
	__le32 version;
} __attribute__ ((__packed__));

struct btrfs_cmd_header {
	/* len excluding the header */
	__le32 len;
	__le16 cmd;
	/* crc including the header with zero crc field */
	__le32 crc;
} __attribute__ ((__packed__));

struct btrfs_tlv_header {
	__le16 tlv_type;
	/* len excluding the header */
	__le16 tlv_len;
} __attribute__ ((__packed__));

/* commands */
enum btrfs_send_cmd {
	BTRFS_SEND_C_UNSPEC,

	BTRFS_SEND_C_SUBVOL,
	BTRFS_SEND_C_SNAPSHOT,

	BTRFS_SEND_C_MKFILE,
	BTRFS_SEND_C_MKDIR,
	BTRFS_SEND_C_MKNOD,
	BTRFS_SEND_C_MKFIFO,
	BTRFS_SEND_C_MKSOCK,
	BTRFS_SEND_C_SYMLINK,

	BTRFS_SEND_C_RENAME,
	BTRFS_SEND_C_LINK,
	BTRFS_SEND_C_UNLINK,
	BTRFS_SEND_C_RMDIR,

	BTRFS_SEND_C_SET_XATTR,
	BTRFS_SEND_C_REMOVE_XATTR,

	BTRFS_SEND_C_WRITE,
	BTRFS_SEND_C_CLONE,

	BTRFS_SEND_C_TRUNCATE,
	BTRFS_SEND_C_CHMOD,
	BTRFS_SEND_C_CHOWN,
	BTRFS_SEND_C_UTIMES,

	BTRFS_SEND_C_END,
	BTRFS_SEND_C_UPDATE_EXTENT,
#ifdef MY_DEF_HERE
	BTRFS_SEND_C_SUBVOL_FLAG,
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
	BTRFS_SEND_C_FALLOCATE,
#endif /* MY_DEF_HERE */
	__BTRFS_SEND_C_MAX,
};
#define BTRFS_SEND_C_MAX (__BTRFS_SEND_C_MAX - 1)

/* attributes in send stream */
enum {
	BTRFS_SEND_A_UNSPEC,

	BTRFS_SEND_A_UUID,
	BTRFS_SEND_A_CTRANSID,

	BTRFS_SEND_A_INO,
	BTRFS_SEND_A_SIZE,
	BTRFS_SEND_A_MODE,
	BTRFS_SEND_A_UID,
	BTRFS_SEND_A_GID,
	BTRFS_SEND_A_RDEV,
	BTRFS_SEND_A_CTIME,
	BTRFS_SEND_A_MTIME,
	BTRFS_SEND_A_ATIME,
	BTRFS_SEND_A_OTIME,

	BTRFS_SEND_A_XATTR_NAME,
	BTRFS_SEND_A_XATTR_DATA,

	BTRFS_SEND_A_PATH,
	BTRFS_SEND_A_PATH_TO,
	BTRFS_SEND_A_PATH_LINK,

	BTRFS_SEND_A_FILE_OFFSET,
	BTRFS_SEND_A_DATA,

	BTRFS_SEND_A_CLONE_UUID,
	BTRFS_SEND_A_CLONE_CTRANSID,
	BTRFS_SEND_A_CLONE_PATH,
	BTRFS_SEND_A_CLONE_OFFSET,
	BTRFS_SEND_A_CLONE_LEN,

#ifdef MY_DEF_HERE
	BTRFS_SEND_A_FLAG,
#endif /* MY_DEF_HERE */
#ifdef MY_DEF_HERE
	BTRFS_SEND_A_FALLOCATE_FLAGS,
#endif /* MY_DEF_HERE */
	__BTRFS_SEND_A_MAX,
};
#define BTRFS_SEND_A_MAX (__BTRFS_SEND_A_MAX - 1)

#ifdef MY_DEF_HERE
#define BTRFS_SEND_A_FALLOCATE_FLAG_KEEP_SIZE   (1 << 0)
#define BTRFS_SEND_A_FALLOCATE_FLAG_PUNCH_HOLE  (1 << 1)

#define BTRFS_SEND_PUNCH_HOLE_FALLOC_FLAGS        \
		(BTRFS_SEND_A_FALLOCATE_FLAG_KEEP_SIZE |  \
		 BTRFS_SEND_A_FALLOCATE_FLAG_PUNCH_HOLE)
#endif /* MY_DEF_HERE */

#ifdef __KERNEL__
long btrfs_ioctl_send(struct file *mnt_file, void __user *arg);
#endif
