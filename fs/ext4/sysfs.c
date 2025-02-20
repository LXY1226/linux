#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 *  linux/fs/ext4/sysfs.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Theodore Ts'o (tytso@mit.edu)
 *
 */

#include <linux/time.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#include "ext4.h"
#include "ext4_jbd2.h"

typedef enum {
	attr_noop,
	attr_delayed_allocation_blocks,
	attr_session_write_kbytes,
	attr_lifetime_write_kbytes,
	attr_reserved_clusters,
	attr_inode_readahead,
	attr_trigger_test_error,
	attr_feature,
	attr_pointer_ui,
	attr_pointer_atomic,
#ifdef MY_ABC_HERE
	attr_syno_fs_error_new_event_flag,
	attr_syno_fs_error_mounted,
	attr_syno_fs_error_count,
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	attr_lazyinit_info,
	attr_lazyinit_speed,
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	attr_incompat_supp,
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	attr_compat_ro_supp,
#endif /* MY_ABC_HERE */
} attr_id_t;

typedef enum {
	ptr_explicit,
	ptr_ext4_sb_info_offset,
	ptr_ext4_super_block_offset,
} attr_ptr_t;

static const char *proc_dirname = "fs/ext4";
static struct proc_dir_entry *ext4_proc_root;

struct ext4_attr {
	struct attribute attr;
	short attr_id;
	short attr_ptr;
	union {
		int offset;
		void *explicit_ptr;
	} u;
};

static ssize_t session_write_kbytes_show(struct ext4_attr *a,
					 struct ext4_sb_info *sbi, char *buf)
{
	struct super_block *sb = sbi->s_buddy_cache->i_sb;

	if (!sb->s_bdev->bd_part)
		return snprintf(buf, PAGE_SIZE, "0\n");
	return snprintf(buf, PAGE_SIZE, "%lu\n",
			(part_stat_read(sb->s_bdev->bd_part, sectors[1]) -
			 sbi->s_sectors_written_start) >> 1);
}

static ssize_t lifetime_write_kbytes_show(struct ext4_attr *a,
					  struct ext4_sb_info *sbi, char *buf)
{
	struct super_block *sb = sbi->s_buddy_cache->i_sb;

	if (!sb->s_bdev->bd_part)
		return snprintf(buf, PAGE_SIZE, "0\n");
	return snprintf(buf, PAGE_SIZE, "%llu\n",
			(unsigned long long)(sbi->s_kbytes_written +
			((part_stat_read(sb->s_bdev->bd_part, sectors[1]) -
			  EXT4_SB(sb)->s_sectors_written_start) >> 1)));
}

static ssize_t inode_readahead_blks_store(struct ext4_attr *a,
					  struct ext4_sb_info *sbi,
					  const char *buf, size_t count)
{
	unsigned long t;
	int ret;

	ret = kstrtoul(skip_spaces(buf), 0, &t);
	if (ret)
		return ret;

	if (t && (!is_power_of_2(t) || t > 0x40000000))
		return -EINVAL;

	sbi->s_inode_readahead_blks = t;
	return count;
}

static ssize_t reserved_clusters_store(struct ext4_attr *a,
				   struct ext4_sb_info *sbi,
				   const char *buf, size_t count)
{
	unsigned long long val;
	ext4_fsblk_t clusters = (ext4_blocks_count(sbi->s_es) >>
				 sbi->s_cluster_bits);
	int ret;

	ret = kstrtoull(skip_spaces(buf), 0, &val);
	if (ret || val >= clusters)
		return -EINVAL;

	atomic64_set(&sbi->s_resv_clusters, val);
	return count;
}

#ifdef MY_ABC_HERE
static ssize_t syno_fs_error_new_event_flag_store(struct ext4_attr *a,
				       struct ext4_sb_info *sbi, const char *buf, size_t count)
{
	long t;
	int ret;

	ret = kstrtol(skip_spaces(buf), 0, &t);
	if (ret)
		return ret;

	if (!(1 == t || 0 == t)) {
		return -EINVAL;
	}
	sbi->s_new_error_fs_event_flag = (int)t;
	return count;
}
#endif /* MY_ABC_HERE */

static ssize_t trigger_test_error(struct ext4_attr *a,
				  struct ext4_sb_info *sbi,
				  const char *buf, size_t count)
{
	int len = count;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (len && buf[len-1] == '\n')
		len--;

	if (len)
		ext4_error(sbi->s_sb, "%.*s", len, buf);
	return count;
}

#define EXT4_ATTR(_name,_mode,_id)					\
static struct ext4_attr ext4_attr_##_name = {				\
	.attr = {.name = __stringify(_name), .mode = _mode },		\
	.attr_id = attr_##_id,						\
}

#define EXT4_ATTR_FUNC(_name,_mode)  EXT4_ATTR(_name,_mode,_name)

#define EXT4_ATTR_FEATURE(_name)   EXT4_ATTR(_name, 0444, feature)

#define EXT4_ATTR_OFFSET(_name,_mode,_id,_struct,_elname)	\
static struct ext4_attr ext4_attr_##_name = {			\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.attr_id = attr_##_id,					\
	.attr_ptr = ptr_##_struct##_offset,			\
	.u = {							\
		.offset = offsetof(struct _struct, _elname),\
	},							\
}

#define EXT4_RO_ATTR_ES_UI(_name,_elname)				\
	EXT4_ATTR_OFFSET(_name, 0444, pointer_ui, ext4_super_block, _elname)

#define EXT4_RW_ATTR_SBI_UI(_name,_elname)	\
	EXT4_ATTR_OFFSET(_name, 0644, pointer_ui, ext4_sb_info, _elname)

#define EXT4_ATTR_PTR(_name,_mode,_id,_ptr) \
static struct ext4_attr ext4_attr_##_name = {			\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.attr_id = attr_##_id,					\
	.attr_ptr = ptr_explicit,				\
	.u = {							\
		.explicit_ptr = _ptr,				\
	},							\
}

#define ATTR_LIST(name) &ext4_attr_##name.attr

EXT4_ATTR_FUNC(delayed_allocation_blocks, 0444);
EXT4_ATTR_FUNC(session_write_kbytes, 0444);
EXT4_ATTR_FUNC(lifetime_write_kbytes, 0444);
EXT4_ATTR_FUNC(reserved_clusters, 0644);

EXT4_ATTR_OFFSET(inode_readahead_blks, 0644, inode_readahead,
		 ext4_sb_info, s_inode_readahead_blks);
EXT4_RW_ATTR_SBI_UI(inode_goal, s_inode_goal);
EXT4_RW_ATTR_SBI_UI(mb_stats, s_mb_stats);
EXT4_RW_ATTR_SBI_UI(mb_max_to_scan, s_mb_max_to_scan);
EXT4_RW_ATTR_SBI_UI(mb_min_to_scan, s_mb_min_to_scan);
EXT4_RW_ATTR_SBI_UI(mb_order2_req, s_mb_order2_reqs);
EXT4_RW_ATTR_SBI_UI(mb_stream_req, s_mb_stream_request);
EXT4_RW_ATTR_SBI_UI(mb_group_prealloc, s_mb_group_prealloc);
EXT4_RW_ATTR_SBI_UI(extent_max_zeroout_kb, s_extent_max_zeroout_kb);
EXT4_ATTR(trigger_fs_error, 0200, trigger_test_error);
EXT4_RW_ATTR_SBI_UI(err_ratelimit_interval_ms, s_err_ratelimit_state.interval);
EXT4_RW_ATTR_SBI_UI(err_ratelimit_burst, s_err_ratelimit_state.burst);
EXT4_RW_ATTR_SBI_UI(warning_ratelimit_interval_ms, s_warning_ratelimit_state.interval);
EXT4_RW_ATTR_SBI_UI(warning_ratelimit_burst, s_warning_ratelimit_state.burst);
EXT4_RW_ATTR_SBI_UI(msg_ratelimit_interval_ms, s_msg_ratelimit_state.interval);
EXT4_RW_ATTR_SBI_UI(msg_ratelimit_burst, s_msg_ratelimit_state.burst);
EXT4_RO_ATTR_ES_UI(errors_count, s_error_count);
EXT4_RO_ATTR_ES_UI(first_error_time, s_first_error_time);
EXT4_RO_ATTR_ES_UI(last_error_time, s_last_error_time);
#ifdef MY_ABC_HERE
EXT4_ATTR_FUNC(syno_fs_error_new_event_flag, 0644);
EXT4_ATTR_FUNC(syno_fs_error_mounted, 0444);
EXT4_ATTR_FUNC(syno_fs_error_count, 0444);
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
EXT4_ATTR_FUNC(lazyinit_info, 0444);
EXT4_ATTR_FUNC(lazyinit_speed, 0444);
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
EXT4_ATTR_FUNC(incompat_supp, 0444);
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
EXT4_ATTR_FUNC(compat_ro_supp, 0444);
#endif /* MY_ABC_HERE */

static unsigned int old_bump_val = 128;
EXT4_ATTR_PTR(max_writeback_mb_bump, 0444, pointer_ui, &old_bump_val);

static struct attribute *ext4_attrs[] = {
	ATTR_LIST(delayed_allocation_blocks),
	ATTR_LIST(session_write_kbytes),
	ATTR_LIST(lifetime_write_kbytes),
	ATTR_LIST(reserved_clusters),
	ATTR_LIST(inode_readahead_blks),
	ATTR_LIST(inode_goal),
	ATTR_LIST(mb_stats),
	ATTR_LIST(mb_max_to_scan),
	ATTR_LIST(mb_min_to_scan),
	ATTR_LIST(mb_order2_req),
	ATTR_LIST(mb_stream_req),
	ATTR_LIST(mb_group_prealloc),
	ATTR_LIST(max_writeback_mb_bump),
	ATTR_LIST(extent_max_zeroout_kb),
	ATTR_LIST(trigger_fs_error),
	ATTR_LIST(err_ratelimit_interval_ms),
	ATTR_LIST(err_ratelimit_burst),
	ATTR_LIST(warning_ratelimit_interval_ms),
	ATTR_LIST(warning_ratelimit_burst),
	ATTR_LIST(msg_ratelimit_interval_ms),
	ATTR_LIST(msg_ratelimit_burst),
	ATTR_LIST(errors_count),
	ATTR_LIST(first_error_time),
	ATTR_LIST(last_error_time),
#ifdef MY_ABC_HERE
	ATTR_LIST(syno_fs_error_new_event_flag),
	ATTR_LIST(syno_fs_error_mounted),
	ATTR_LIST(syno_fs_error_count),
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	ATTR_LIST(lazyinit_info),
	ATTR_LIST(lazyinit_speed),
#endif /* MY_ABC_HERE */
	NULL,
};

/* Features this copy of ext4 supports */
EXT4_ATTR_FEATURE(lazy_itable_init);
EXT4_ATTR_FEATURE(batched_discard);
EXT4_ATTR_FEATURE(meta_bg_resize);
#ifdef CONFIG_EXT4_FS_ENCRYPTION
EXT4_ATTR_FEATURE(encryption);
#endif
EXT4_ATTR_FEATURE(metadata_csum_seed);

static struct attribute *ext4_feat_attrs[] = {
	ATTR_LIST(lazy_itable_init),
	ATTR_LIST(batched_discard),
	ATTR_LIST(meta_bg_resize),
#ifdef CONFIG_EXT4_FS_ENCRYPTION
	ATTR_LIST(encryption),
#endif
	ATTR_LIST(metadata_csum_seed),
#ifdef MY_ABC_HERE
	ATTR_LIST(incompat_supp),
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	ATTR_LIST(compat_ro_supp),
#endif /* MY_ABC_HERE */
	NULL,
};

static void *calc_ptr(struct ext4_attr *a, struct ext4_sb_info *sbi)
{
	switch (a->attr_ptr) {
	case ptr_explicit:
		return a->u.explicit_ptr;
	case ptr_ext4_sb_info_offset:
		return (void *) (((char *) sbi) + a->u.offset);
	case ptr_ext4_super_block_offset:
		return (void *) (((char *) sbi->s_es) + a->u.offset);
	}
	return NULL;
}

static ssize_t ext4_attr_show(struct kobject *kobj,
			      struct attribute *attr, char *buf)
{
	struct ext4_sb_info *sbi = container_of(kobj, struct ext4_sb_info,
						s_kobj);
	struct ext4_attr *a = container_of(attr, struct ext4_attr, attr);
	void *ptr = calc_ptr(a, sbi);

	switch (a->attr_id) {
	case attr_delayed_allocation_blocks:
		return snprintf(buf, PAGE_SIZE, "%llu\n",
				(s64) EXT4_C2B(sbi,
		       percpu_counter_sum(&sbi->s_dirtyclusters_counter)));
	case attr_session_write_kbytes:
		return session_write_kbytes_show(a, sbi, buf);
	case attr_lifetime_write_kbytes:
		return lifetime_write_kbytes_show(a, sbi, buf);
	case attr_reserved_clusters:
		return snprintf(buf, PAGE_SIZE, "%llu\n",
				(unsigned long long)
				atomic64_read(&sbi->s_resv_clusters));
	case attr_inode_readahead:
	case attr_pointer_ui:
		if (!ptr)
			return 0;
		if (a->attr_ptr == ptr_ext4_super_block_offset)
			return snprintf(buf, PAGE_SIZE, "%u\n",
					le32_to_cpup(ptr));
		else
			return snprintf(buf, PAGE_SIZE, "%u\n",
					*((unsigned int *) ptr));
	case attr_pointer_atomic:
		if (!ptr)
			return 0;
		return snprintf(buf, PAGE_SIZE, "%d\n",
				atomic_read((atomic_t *) ptr));
	case attr_feature:
		return snprintf(buf, PAGE_SIZE, "supported\n");
#ifdef MY_ABC_HERE
	case attr_syno_fs_error_new_event_flag:
		return snprintf(buf, PAGE_SIZE, "%d\n", sbi->s_new_error_fs_event_flag);
	case attr_syno_fs_error_mounted:
		return snprintf(buf, PAGE_SIZE, "%s\n",
				sbi->s_mount_path ? sbi->s_mount_path : "NULL");
	case attr_syno_fs_error_count:
		return snprintf(buf, PAGE_SIZE, "%d\n", sbi->s_es->s_error_count); 
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	case attr_lazyinit_info:
		return snprintf(buf, PAGE_SIZE, "%u %u\n",
				sbi->s_li_request ? sbi->s_li_request->lr_next_group : sbi->s_groups_count,
				sbi->s_groups_count);
	case attr_lazyinit_speed:
		return snprintf(buf, PAGE_SIZE, "%lu\n",
				sbi->s_li_request ? sbi->s_li_request->lr_timeout : 0);
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	case attr_incompat_supp:
		return snprintf(buf, PAGE_SIZE, "%u\n", EXT4_FEATURE_INCOMPAT_SUPP);
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	case attr_compat_ro_supp:
		return snprintf(buf, PAGE_SIZE, "%u\n", EXT4_FEATURE_RO_COMPAT_SUPP);
#endif /* MY_ABC_HERE */
	}

	return 0;
}

static ssize_t ext4_attr_store(struct kobject *kobj,
			       struct attribute *attr,
			       const char *buf, size_t len)
{
	struct ext4_sb_info *sbi = container_of(kobj, struct ext4_sb_info,
						s_kobj);
	struct ext4_attr *a = container_of(attr, struct ext4_attr, attr);
	void *ptr = calc_ptr(a, sbi);
	unsigned long t;
	int ret;

	switch (a->attr_id) {
	case attr_reserved_clusters:
		return reserved_clusters_store(a, sbi, buf, len);
	case attr_pointer_ui:
		if (!ptr)
			return 0;
		ret = kstrtoul(skip_spaces(buf), 0, &t);
		if (ret)
			return ret;
		if (a->attr_ptr == ptr_ext4_super_block_offset)
			*((__le32 *) ptr) = cpu_to_le32(t);
		else
			*((unsigned int *) ptr) = t;
		return len;
	case attr_inode_readahead:
		return inode_readahead_blks_store(a, sbi, buf, len);
	case attr_trigger_test_error:
		return trigger_test_error(a, sbi, buf, len);
#ifdef MY_ABC_HERE
	case attr_syno_fs_error_new_event_flag:
		return syno_fs_error_new_event_flag_store(a, sbi, buf, len);
#endif /* MY_ABC_HERE */
	}
	return 0;
}

static void ext4_sb_release(struct kobject *kobj)
{
	struct ext4_sb_info *sbi = container_of(kobj, struct ext4_sb_info,
						s_kobj);
	complete(&sbi->s_kobj_unregister);
}

static const struct sysfs_ops ext4_attr_ops = {
	.show	= ext4_attr_show,
	.store	= ext4_attr_store,
};

static struct kobj_type ext4_sb_ktype = {
	.default_attrs	= ext4_attrs,
	.sysfs_ops	= &ext4_attr_ops,
	.release	= ext4_sb_release,
};

static struct kobj_type ext4_ktype = {
	.sysfs_ops	= &ext4_attr_ops,
};

static struct kset ext4_kset = {
	.kobj   = {.ktype = &ext4_ktype},
};

static struct kobj_type ext4_feat_ktype = {
	.default_attrs	= ext4_feat_attrs,
	.sysfs_ops	= &ext4_attr_ops,
};

static struct kobject ext4_feat = {
	.kset	= &ext4_kset,
};

#define PROC_FILE_SHOW_DEFN(name) \
static int name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, ext4_seq_##name##_show, PDE_DATA(inode)); \
} \
\
static const struct file_operations ext4_seq_##name##_fops = { \
	.owner		= THIS_MODULE, \
	.open		= name##_open, \
	.read		= seq_read, \
	.llseek		= seq_lseek, \
	.release	= single_release, \
}

#define PROC_FILE_LIST(name) \
	{ __stringify(name), &ext4_seq_##name##_fops }

PROC_FILE_SHOW_DEFN(es_shrinker_info);
PROC_FILE_SHOW_DEFN(options);

static struct ext4_proc_files {
	const char *name;
	const struct file_operations *fops;
} proc_files[] = {
	PROC_FILE_LIST(options),
	PROC_FILE_LIST(es_shrinker_info),
	PROC_FILE_LIST(mb_groups),
	{ NULL, NULL },
};

int ext4_register_sysfs(struct super_block *sb)
{
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	struct ext4_proc_files *p;
	int err;

	sbi->s_kobj.kset = &ext4_kset;
	init_completion(&sbi->s_kobj_unregister);
	err = kobject_init_and_add(&sbi->s_kobj, &ext4_sb_ktype, NULL,
				   "%s", sb->s_id);
	if (err)
		return err;

	if (ext4_proc_root)
		sbi->s_proc = proc_mkdir(sb->s_id, ext4_proc_root);

	if (sbi->s_proc) {
		for (p = proc_files; p->name; p++)
			proc_create_data(p->name, S_IRUGO, sbi->s_proc,
					 p->fops, sb);
	}
	return 0;
}

void ext4_unregister_sysfs(struct super_block *sb)
{
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	struct ext4_proc_files *p;

	if (sbi->s_proc) {
		for (p = proc_files; p->name; p++)
			remove_proc_entry(p->name, sbi->s_proc);
		remove_proc_entry(sb->s_id, ext4_proc_root);
	}
	kobject_del(&sbi->s_kobj);
}

int __init ext4_init_sysfs(void)
{
	int ret;

	kobject_set_name(&ext4_kset.kobj, "ext4");
	ext4_kset.kobj.parent = fs_kobj;
	ret = kset_register(&ext4_kset);
	if (ret)
		return ret;

	ret = kobject_init_and_add(&ext4_feat, &ext4_feat_ktype,
				   NULL, "features");
	if (ret)
		kset_unregister(&ext4_kset);
	else
		ext4_proc_root = proc_mkdir(proc_dirname, NULL);
	return ret;
}

void ext4_exit_sysfs(void)
{
	kobject_put(&ext4_feat);
	kset_unregister(&ext4_kset);
	remove_proc_entry(proc_dirname, NULL);
	ext4_proc_root = NULL;
}

