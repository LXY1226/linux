#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
  File: linux/xattr.h

  Extended attributes handling.

  Copyright (C) 2001 by Andreas Gruenbacher <a.gruenbacher@computer.org>
  Copyright (c) 2001-2002 Silicon Graphics, Inc.  All Rights Reserved.
  Copyright (c) 2004 Red Hat, Inc., James Morris <jmorris@redhat.com>
*/

#include <linux/libc-compat.h>

#ifndef _UAPI_LINUX_XATTR_H
#define _UAPI_LINUX_XATTR_H

#if __UAPI_DEF_XATTR
#define __USE_KERNEL_XATTR_DEFS

#define XATTR_CREATE	0x1	/* set value, fail if attr already exists */
#define XATTR_REPLACE	0x2	/* set value, fail if attr does not exist */
#endif

/* Namespaces */
#define XATTR_OS2_PREFIX "os2."
#define XATTR_OS2_PREFIX_LEN (sizeof(XATTR_OS2_PREFIX) - 1)

#ifdef MY_ABC_HERE
#define XATTR_MAC_OSX_PREFIX ""
#else
#define XATTR_MAC_OSX_PREFIX "osx."
#endif /* MY_ABC_HERE */
#define XATTR_MAC_OSX_PREFIX_LEN (sizeof(XATTR_MAC_OSX_PREFIX) - 1)

#define XATTR_BTRFS_PREFIX "btrfs."
#define XATTR_BTRFS_PREFIX_LEN (sizeof(XATTR_BTRFS_PREFIX) - 1)

#define XATTR_SECURITY_PREFIX	"security."
#define XATTR_SECURITY_PREFIX_LEN (sizeof(XATTR_SECURITY_PREFIX) - 1)

#define XATTR_SYSTEM_PREFIX "system."
#define XATTR_SYSTEM_PREFIX_LEN (sizeof(XATTR_SYSTEM_PREFIX) - 1)

#define XATTR_TRUSTED_PREFIX "trusted."
#define XATTR_TRUSTED_PREFIX_LEN (sizeof(XATTR_TRUSTED_PREFIX) - 1)

#define XATTR_USER_PREFIX "user."
#define XATTR_USER_PREFIX_LEN (sizeof(XATTR_USER_PREFIX) - 1)

#ifdef MY_ABC_HERE
#define XATTR_SYNO_PREFIX "syno."
#define XATTR_SYNO_PREFIX_LEN (sizeof (XATTR_SYNO_PREFIX) - 1)
#endif /* MY_ABC_HERE */

#ifdef MY_ABC_HERE
#define XATTR_SYNO_ARCHIVE_BIT "archive_bit"
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
#define XATTR_SYNO_ARCHIVE_VERSION "archive_version"
#define XATTR_SYNO_ARCHIVE_VERSION_VOLUME "archive_version_volume"
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
#define XATTR_SYNO_CREATE_TIME "create_time"
#endif /* MY_ABC_HERE */

#ifdef MY_DEF_HERE
#define XATTR_SYNO_LOCKER_SUFFIX                  "w"
#define XATTR_SYNO_LOCKER_SUFFIX_LEN              (sizeof(XATTR_SYNO_LOCKER_SUFFIX) - 1)
#define XATTR_SYNO_LOCKER                         (XATTR_SYNO_PREFIX XATTR_SYNO_LOCKER_SUFFIX)
#define XATTR_SYNO_LOCKER_LEN                     (sizeof(XATTR_SYNO_LOCKER) - 1)
#endif /* MY_DEF_HERE */

/* Security namespace */
#define XATTR_EVM_SUFFIX "evm"
#define XATTR_NAME_EVM XATTR_SECURITY_PREFIX XATTR_EVM_SUFFIX

#define XATTR_IMA_SUFFIX "ima"
#define XATTR_NAME_IMA XATTR_SECURITY_PREFIX XATTR_IMA_SUFFIX

#define XATTR_SELINUX_SUFFIX "selinux"
#define XATTR_NAME_SELINUX XATTR_SECURITY_PREFIX XATTR_SELINUX_SUFFIX

#define XATTR_SMACK_SUFFIX "SMACK64"
#define XATTR_SMACK_IPIN "SMACK64IPIN"
#define XATTR_SMACK_IPOUT "SMACK64IPOUT"
#define XATTR_SMACK_EXEC "SMACK64EXEC"
#define XATTR_SMACK_TRANSMUTE "SMACK64TRANSMUTE"
#define XATTR_SMACK_MMAP "SMACK64MMAP"
#define XATTR_NAME_SMACK XATTR_SECURITY_PREFIX XATTR_SMACK_SUFFIX
#define XATTR_NAME_SMACKIPIN	XATTR_SECURITY_PREFIX XATTR_SMACK_IPIN
#define XATTR_NAME_SMACKIPOUT	XATTR_SECURITY_PREFIX XATTR_SMACK_IPOUT
#define XATTR_NAME_SMACKEXEC	XATTR_SECURITY_PREFIX XATTR_SMACK_EXEC
#define XATTR_NAME_SMACKTRANSMUTE XATTR_SECURITY_PREFIX XATTR_SMACK_TRANSMUTE
#define XATTR_NAME_SMACKMMAP XATTR_SECURITY_PREFIX XATTR_SMACK_MMAP

#define XATTR_CAPS_SUFFIX "capability"
#define XATTR_NAME_CAPS XATTR_SECURITY_PREFIX XATTR_CAPS_SUFFIX

#define XATTR_POSIX_ACL_ACCESS  "posix_acl_access"
#define XATTR_NAME_POSIX_ACL_ACCESS XATTR_SYSTEM_PREFIX XATTR_POSIX_ACL_ACCESS
#define XATTR_POSIX_ACL_DEFAULT  "posix_acl_default"
#define XATTR_NAME_POSIX_ACL_DEFAULT XATTR_SYSTEM_PREFIX XATTR_POSIX_ACL_DEFAULT


#endif /* _UAPI_LINUX_XATTR_H */
