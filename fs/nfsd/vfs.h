#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Copyright (C) 1995-1997 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef LINUX_NFSD_VFS_H
#define LINUX_NFSD_VFS_H

#ifdef MY_ABC_HERE
#include <linux/sched.h>
#endif /* MY_ABC_HERE */
#include "nfsfh.h"
#include "nfsd.h"

/*
 * Flags for nfsd_permission
 */
#define NFSD_MAY_NOP			0
#define NFSD_MAY_EXEC			0x001 /* == MAY_EXEC */
#define NFSD_MAY_WRITE			0x002 /* == MAY_WRITE */
#define NFSD_MAY_READ			0x004 /* == MAY_READ */
#define NFSD_MAY_SATTR			0x008
#define NFSD_MAY_TRUNC			0x010
#define NFSD_MAY_LOCK			0x020
#define NFSD_MAY_MASK			0x03f

/* extra hints to permission and open routines: */
#define NFSD_MAY_OWNER_OVERRIDE		0x040
#define NFSD_MAY_LOCAL_ACCESS		0x080 /* for device special files */
#define NFSD_MAY_BYPASS_GSS_ON_ROOT	0x100
#define NFSD_MAY_NOT_BREAK_LEASE	0x200
#define NFSD_MAY_BYPASS_GSS		0x400
#define NFSD_MAY_READ_IF_EXEC		0x800

#define NFSD_MAY_64BIT_COOKIE		0x1000 /* 64 bit readdir cookies for >= NFSv3 */

#ifdef MY_ABC_HERE
#define NFSD_MAY_SYNO_NOP		0x2000
#define NFSD_MAY_APPEND			0x4000
#endif /* MY_ABC_HERE */

#define NFSD_MAY_CREATE		(NFSD_MAY_EXEC|NFSD_MAY_WRITE)
#define NFSD_MAY_REMOVE		(NFSD_MAY_EXEC|NFSD_MAY_WRITE|NFSD_MAY_TRUNC)

#ifdef MY_ABC_HERE
#define NFSD_COPYBUFFERSIZE                     (1<<17)
#define NFSD_PAGESIZE                           (1<<12)
#endif

/*
 * Callback function for readdir
 */
typedef int (*nfsd_filldir_t)(void *, const char *, int, loff_t, u64, unsigned);

/* nfsd/vfs.c */
int		nfsd_racache_init(int);
void		nfsd_racache_shutdown(void);
int		nfsd_cross_mnt(struct svc_rqst *rqstp, struct dentry **dpp,
		                struct svc_export **expp);
__be32		nfsd_lookup(struct svc_rqst *, struct svc_fh *,
				const char *, unsigned int, struct svc_fh *);
__be32		 nfsd_lookup_dentry(struct svc_rqst *, struct svc_fh *,
				const char *, unsigned int,
				struct svc_export **, struct dentry **);
__be32		nfsd_setattr(struct svc_rqst *, struct svc_fh *,
				struct iattr *, int, time_t);
int nfsd_mountpoint(struct dentry *, struct svc_export *);
#ifdef CONFIG_NFSD_V4
__be32          nfsd4_set_nfs4_label(struct svc_rqst *, struct svc_fh *,
		    struct xdr_netobj *);
__be32		nfsd4_vfs_fallocate(struct svc_rqst *, struct svc_fh *,
				    struct file *, loff_t, loff_t, int);
__be32		nfsd4_clone_file_range(struct file *, u64, struct file *,
			u64, u64);
#endif /* CONFIG_NFSD_V4 */
__be32		nfsd_create(struct svc_rqst *, struct svc_fh *,
				char *name, int len, struct iattr *attrs,
				int type, dev_t rdev, struct svc_fh *res);
#ifdef CONFIG_NFSD_V3
__be32		nfsd_access(struct svc_rqst *, struct svc_fh *, u32 *, u32 *);
__be32		do_nfsd_create(struct svc_rqst *, struct svc_fh *,
				char *name, int len, struct iattr *attrs,
				struct svc_fh *res, int createmode,
				u32 *verifier, bool *truncp, bool *created);
__be32		nfsd_commit(struct svc_rqst *, struct svc_fh *,
				loff_t, unsigned long);
#endif /* CONFIG_NFSD_V3 */
__be32		nfsd_open(struct svc_rqst *, struct svc_fh *, umode_t,
				int, struct file **);
struct raparms;
__be32		nfsd_splice_read(struct svc_rqst *,
				struct file *, loff_t, unsigned long *
#ifdef MY_ABC_HERE
				, ktime_t
#endif /* MY_ABC_HERE */
				);

__be32		nfsd_readv(struct file *, loff_t, struct kvec *, int,
				unsigned long *
#ifdef MY_ABC_HERE
				, struct svc_rqst *, ktime_t
#endif /* MY_ABC_HERE */
				);

__be32 		nfsd_read(struct svc_rqst *, struct svc_fh *,
				loff_t, struct kvec *, int, unsigned long *);
__be32 		nfsd_write(struct svc_rqst *, struct svc_fh *,struct file *,
				loff_t, struct kvec *,int, unsigned long *, int *);
#ifdef MY_ABC_HERE
__be32		nfsd_writezero(struct svc_rqst *, struct svc_fh *,
                                loff_t, unsigned long *);
__be32		nfsd_synocopy(const char *, struct svc_rqst *, struct svc_fh *,
                                loff_t, unsigned long *, bool);
#ifdef MY_DEF_HERE
__be32		nfsd_synoclone(const char *, struct svc_rqst *, struct svc_fh *);
#endif /* MY_DEF_HERE */
#endif
__be32		nfsd_vfs_write(struct svc_rqst *rqstp, struct svc_fh *fhp,
				struct file *file, loff_t offset,
				struct kvec *vec, int vlen, unsigned long *cnt,
				int *stablep);
__be32		nfsd_readlink(struct svc_rqst *, struct svc_fh *,
				char *, int *);
__be32		nfsd_symlink(struct svc_rqst *, struct svc_fh *,
				char *name, int len, char *path,
				struct svc_fh *res);
__be32		nfsd_link(struct svc_rqst *, struct svc_fh *,
				char *, int, struct svc_fh *);
__be32		nfsd_rename(struct svc_rqst *,
				struct svc_fh *, char *, int,
				struct svc_fh *, char *, int);
__be32		nfsd_unlink(struct svc_rqst *, struct svc_fh *, int type,
				char *name, int len);
__be32		nfsd_readdir(struct svc_rqst *, struct svc_fh *,
			     loff_t *, struct readdir_cd *, nfsd_filldir_t);
__be32		nfsd_statfs(struct svc_rqst *, struct svc_fh *,
				struct kstatfs *, int access);

__be32		nfsd_permission(struct svc_rqst *, struct svc_export *,
				struct dentry *, int);

struct raparms *nfsd_init_raparms(struct file *file);
void		nfsd_put_raparams(struct file *file, struct raparms *ra);

static inline int fh_want_write(struct svc_fh *fh)
{
	int ret;

	if (fh->fh_want_write)
		return 0;
	ret = mnt_want_write(fh->fh_export->ex_path.mnt);
	if (!ret)
		fh->fh_want_write = true;
	return ret;
}

static inline void fh_drop_write(struct svc_fh *fh)
{
	if (fh->fh_want_write) {
		fh->fh_want_write = false;
		mnt_drop_write(fh->fh_export->ex_path.mnt);
	}
}

static inline __be32 fh_getattr(struct svc_fh *fh, struct kstat *stat)
{
	struct path p = {.mnt = fh->fh_export->ex_path.mnt,
			 .dentry = fh->fh_dentry};
#ifdef MY_ABC_HERE
	int err = 0;

	err = vfs_getattr(&p, stat);
	if (!err && IS_SYNOACL(fh->fh_dentry) && uid_eq(current_fsuid(), GLOBAL_ROOT_UID))
		stat->mode |= (S_IRWXU|S_IRWXG|S_IRWXO);
	return nfserrno(err);
#else
	return nfserrno(vfs_getattr(&p, stat));
#endif
}

static inline int nfsd_create_is_exclusive(int createmode)
{
	return createmode == NFS3_CREATE_EXCLUSIVE
	       || createmode == NFS4_CREATE_EXCLUSIVE4_1;
}

#endif /* LINUX_NFSD_VFS_H */
