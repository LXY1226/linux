#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 *  linux/fs/ext4/file.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/file.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  ext4 fs regular file handling primitives
 *
 *  64-bit file support on 64-bit platforms by Jakub Jelinek
 *	(jj@sunsite.ms.mff.cuni.cz)
 */

#include <linux/time.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/path.h>
#include <linux/dax.h>
#include <linux/quotaops.h>
#include <linux/pagevec.h>
#include <linux/uio.h>
#if defined(MY_ABC_HERE)
#ifdef CONFIG_SPLICE_FROM_SOCKET
#include <linux/backing-dev.h>
#include <linux/fsnotify.h>
#include <linux/swap.h>
#include <net/sock.h>
#endif
#endif /* MY_ABC_HERE */
#include "ext4.h"
#include "ext4_jbd2.h"
#include "xattr.h"
#include "acl.h"
#ifdef MY_ABC_HERE
#include "syno_acl.h"
#endif /* MY_ABC_HERE */

/*
 * Called when an inode is released. Note that this is different
 * from ext4_file_open: open gets called at every open, but release
 * gets called only when /all/ the files are closed.
 */
static int ext4_release_file(struct inode *inode, struct file *filp)
{
	if (ext4_test_inode_state(inode, EXT4_STATE_DA_ALLOC_CLOSE)) {
		ext4_alloc_da_blocks(inode);
		ext4_clear_inode_state(inode, EXT4_STATE_DA_ALLOC_CLOSE);
	}
	/* if we are the last writer on the inode, drop the block reservation */
	if ((filp->f_mode & FMODE_WRITE) &&
			(atomic_read(&inode->i_writecount) == 1) &&
		        !EXT4_I(inode)->i_reserved_data_blocks)
	{
		down_write(&EXT4_I(inode)->i_data_sem);
		ext4_discard_preallocations(inode);
		up_write(&EXT4_I(inode)->i_data_sem);
	}
	if (is_dx(inode) && filp->private_data)
		ext4_htree_free_dir_info(filp->private_data);

	return 0;
}

static void ext4_unwritten_wait(struct inode *inode)
{
	wait_queue_head_t *wq = ext4_ioend_wq(inode);

	wait_event(*wq, (atomic_read(&EXT4_I(inode)->i_unwritten) == 0));
}

/*
 * This tests whether the IO in question is block-aligned or not.
 * Ext4 utilizes unwritten extents when hole-filling during direct IO, and they
 * are converted to written only after the IO is complete.  Until they are
 * mapped, these blocks appear as holes, so dio_zero_block() will assume that
 * it needs to zero out portions of the start and/or end block.  If 2 AIO
 * threads are at work on the same unwritten block, they must be synchronized
 * or one thread will zero the other's data, causing corruption.
 */
static int
ext4_unaligned_aio(struct inode *inode, struct iov_iter *from, loff_t pos)
{
	struct super_block *sb = inode->i_sb;
	int blockmask = sb->s_blocksize - 1;

	if (pos >= ALIGN(i_size_read(inode), sb->s_blocksize))
		return 0;

	if ((pos | iov_iter_alignment(from)) & blockmask)
		return 1;

	return 0;
}

static ssize_t
ext4_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file_inode(iocb->ki_filp);
	struct mutex *aio_mutex = NULL;
	struct blk_plug plug;
	int o_direct = iocb->ki_flags & IOCB_DIRECT;
	int overwrite = 0;
	ssize_t ret;

	/*
	 * Unaligned direct AIO must be serialized; see comment above
	 * In the case of O_APPEND, assume that we must always serialize
	 */
	if (o_direct &&
	    ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS) &&
	    !is_sync_kiocb(iocb) &&
	    (iocb->ki_flags & IOCB_APPEND ||
	     ext4_unaligned_aio(inode, from, iocb->ki_pos))) {
		aio_mutex = ext4_aio_mutex(inode);
		mutex_lock(aio_mutex);
		ext4_unwritten_wait(inode);
	}

	inode_lock(inode);
	ret = generic_write_checks(iocb, from);
	if (ret <= 0)
		goto out;

	/*
	 * If we have encountered a bitmap-format file, the size limit
	 * is smaller than s_maxbytes, which is for extent-mapped files.
	 */
	if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))) {
		struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);

		if (iocb->ki_pos >= sbi->s_bitmap_maxbytes) {
			ret = -EFBIG;
			goto out;
		}
		iov_iter_truncate(from, sbi->s_bitmap_maxbytes - iocb->ki_pos);
	}

	iocb->private = &overwrite;
	if (o_direct) {
		size_t length = iov_iter_count(from);
		loff_t pos = iocb->ki_pos;
		blk_start_plug(&plug);

		/* check whether we do a DIO overwrite or not */
		if (ext4_should_dioread_nolock(inode) && !aio_mutex &&
		    !file->f_mapping->nrpages && pos + length <= i_size_read(inode)) {
			struct ext4_map_blocks map;
			unsigned int blkbits = inode->i_blkbits;
			int err, len;

			map.m_lblk = pos >> blkbits;
			map.m_len = (EXT4_BLOCK_ALIGN(pos + length, blkbits) >> blkbits)
				- map.m_lblk;
			len = map.m_len;

			err = ext4_map_blocks(NULL, inode, &map, 0);
			/*
			 * 'err==len' means that all of blocks has
			 * been preallocated no matter they are
			 * initialized or not.  For excluding
			 * unwritten extents, we need to check
			 * m_flags.  There are two conditions that
			 * indicate for initialized extents.  1) If we
			 * hit extent cache, EXT4_MAP_MAPPED flag is
			 * returned; 2) If we do a real lookup,
			 * non-flags are returned.  So we should check
			 * these two conditions.
			 */
			if (err == len && (map.m_flags & EXT4_MAP_MAPPED))
				overwrite = 1;
		}
	}

	ret = __generic_file_write_iter(iocb, from);
	inode_unlock(inode);

	if (ret > 0) {
		ssize_t err;

		err = generic_write_sync(file, iocb->ki_pos - ret, ret);
		if (err < 0)
			ret = err;
	}
	if (o_direct)
		blk_finish_plug(&plug);

	if (aio_mutex)
		mutex_unlock(aio_mutex);
	return ret;

out:
	inode_unlock(inode);
	if (aio_mutex)
		mutex_unlock(aio_mutex);
	return ret;
}

#ifdef CONFIG_FS_DAX
static void ext4_end_io_unwritten(struct buffer_head *bh, int uptodate)
{
	struct inode *inode = bh->b_assoc_map->host;
	/* XXX: breaks on 32-bit > 16TB. Is that even supported? */
	loff_t offset = (loff_t)(uintptr_t)bh->b_private << inode->i_blkbits;
	int err;
	if (!uptodate)
		return;
	WARN_ON(!buffer_unwritten(bh));
	err = ext4_convert_unwritten_extents(NULL, inode, offset, bh->b_size);
}

static int ext4_dax_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	int result;
	handle_t *handle = NULL;
	struct inode *inode = file_inode(vma->vm_file);
	struct super_block *sb = inode->i_sb;
	bool write = vmf->flags & FAULT_FLAG_WRITE;

	if (write) {
		sb_start_pagefault(sb);
		file_update_time(vma->vm_file);
		down_read(&EXT4_I(inode)->i_mmap_sem);
		handle = ext4_journal_start_sb(sb, EXT4_HT_WRITE_PAGE,
						EXT4_DATA_TRANS_BLOCKS(sb));
	} else
		down_read(&EXT4_I(inode)->i_mmap_sem);

	if (IS_ERR(handle))
		result = VM_FAULT_SIGBUS;
	else
		result = __dax_fault(vma, vmf, ext4_get_block_dax,
						ext4_end_io_unwritten);

	if (write) {
		if (!IS_ERR(handle))
			ext4_journal_stop(handle);
		up_read(&EXT4_I(inode)->i_mmap_sem);
		sb_end_pagefault(sb);
	} else
		up_read(&EXT4_I(inode)->i_mmap_sem);

	return result;
}

static int ext4_dax_pmd_fault(struct vm_area_struct *vma, unsigned long addr,
						pmd_t *pmd, unsigned int flags)
{
	int result;
	handle_t *handle = NULL;
	struct inode *inode = file_inode(vma->vm_file);
	struct super_block *sb = inode->i_sb;
	bool write = flags & FAULT_FLAG_WRITE;

	if (write) {
		sb_start_pagefault(sb);
		file_update_time(vma->vm_file);
		down_read(&EXT4_I(inode)->i_mmap_sem);
		handle = ext4_journal_start_sb(sb, EXT4_HT_WRITE_PAGE,
				ext4_chunk_trans_blocks(inode,
							PMD_SIZE / PAGE_SIZE));
	} else
		down_read(&EXT4_I(inode)->i_mmap_sem);

	if (IS_ERR(handle))
		result = VM_FAULT_SIGBUS;
	else
		result = __dax_pmd_fault(vma, addr, pmd, flags,
				ext4_get_block_dax, ext4_end_io_unwritten);

	if (write) {
		if (!IS_ERR(handle))
			ext4_journal_stop(handle);
		up_read(&EXT4_I(inode)->i_mmap_sem);
		sb_end_pagefault(sb);
	} else
		up_read(&EXT4_I(inode)->i_mmap_sem);

	return result;
}

static int ext4_dax_mkwrite(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	int err;
	struct inode *inode = file_inode(vma->vm_file);

	sb_start_pagefault(inode->i_sb);
	file_update_time(vma->vm_file);
	down_read(&EXT4_I(inode)->i_mmap_sem);
	err = __dax_mkwrite(vma, vmf, ext4_get_block_dax,
			    ext4_end_io_unwritten);
	up_read(&EXT4_I(inode)->i_mmap_sem);
	sb_end_pagefault(inode->i_sb);

	return err;
}

/*
 * Handle write fault for VM_MIXEDMAP mappings. Similarly to ext4_dax_mkwrite()
 * handler we check for races agaist truncate. Note that since we cycle through
 * i_mmap_sem, we are sure that also any hole punching that began before we
 * were called is finished by now and so if it included part of the file we
 * are working on, our pte will get unmapped and the check for pte_same() in
 * wp_pfn_shared() fails. Thus fault gets retried and things work out as
 * desired.
 */
static int ext4_dax_pfn_mkwrite(struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	struct inode *inode = file_inode(vma->vm_file);
	struct super_block *sb = inode->i_sb;
	int ret = VM_FAULT_NOPAGE;
	loff_t size;

	sb_start_pagefault(sb);
	file_update_time(vma->vm_file);
	down_read(&EXT4_I(inode)->i_mmap_sem);
	size = (i_size_read(inode) + PAGE_SIZE - 1) >> PAGE_SHIFT;
	if (vmf->pgoff >= size)
		ret = VM_FAULT_SIGBUS;
	up_read(&EXT4_I(inode)->i_mmap_sem);
	sb_end_pagefault(sb);

	return ret;
}

static const struct vm_operations_struct ext4_dax_vm_ops = {
	.fault		= ext4_dax_fault,
	.pmd_fault	= ext4_dax_pmd_fault,
	.page_mkwrite	= ext4_dax_mkwrite,
	.pfn_mkwrite	= ext4_dax_pfn_mkwrite,
};
#else
#define ext4_dax_vm_ops	ext4_file_vm_ops
#endif

static const struct vm_operations_struct ext4_file_vm_ops = {
	.fault		= ext4_filemap_fault,
	.map_pages	= filemap_map_pages,
	.page_mkwrite   = ext4_page_mkwrite,
};

static int ext4_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct inode *inode = file->f_mapping->host;

	if (ext4_encrypted_inode(inode)) {
		int err = ext4_get_encryption_info(inode);
		if (err)
			return 0;
		if (ext4_encryption_info(inode) == NULL)
			return -ENOKEY;
	}
	file_accessed(file);
	if (IS_DAX(file_inode(file))) {
		vma->vm_ops = &ext4_dax_vm_ops;
		vma->vm_flags |= VM_MIXEDMAP | VM_HUGEPAGE;
	} else {
		vma->vm_ops = &ext4_file_vm_ops;
	}
	return 0;
}

static int ext4_file_open(struct inode * inode, struct file * filp)
{
	struct super_block *sb = inode->i_sb;
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	struct vfsmount *mnt = filp->f_path.mnt;
	struct path path;
	char buf[64], *cp;
	int ret;

	if (unlikely(!(sbi->s_mount_flags & EXT4_MF_MNTDIR_SAMPLED) &&
		     !(sb->s_flags & MS_RDONLY))) {
		sbi->s_mount_flags |= EXT4_MF_MNTDIR_SAMPLED;
		/*
		 * Sample where the filesystem has been mounted and
		 * store it in the superblock for sysadmin convenience
		 * when trying to sort through large numbers of block
		 * devices or filesystem images.
		 */
		memset(buf, 0, sizeof(buf));
		path.mnt = mnt;
		path.dentry = mnt->mnt_root;
		cp = d_path(&path, buf, sizeof(buf));
		if (!IS_ERR(cp)) {
			handle_t *handle;
			int err;

			handle = ext4_journal_start_sb(sb, EXT4_HT_MISC, 1);
			if (IS_ERR(handle))
				return PTR_ERR(handle);
			BUFFER_TRACE(sbi->s_sbh, "get_write_access");
			err = ext4_journal_get_write_access(handle, sbi->s_sbh);
			if (err) {
				ext4_journal_stop(handle);
				return err;
			}
			strlcpy(sbi->s_es->s_last_mounted, cp,
				sizeof(sbi->s_es->s_last_mounted));
			ext4_handle_dirty_super(handle, sb);
			ext4_journal_stop(handle);
		}
	}
	if (ext4_encrypted_inode(inode)) {
		ret = ext4_get_encryption_info(inode);
		if (ret)
			return -EACCES;
		if (ext4_encryption_info(inode) == NULL)
			return -ENOKEY;
	}
	/*
	 * Set up the jbd2_inode if we are opening the inode for
	 * writing and the journal is present
	 */
	if (filp->f_mode & FMODE_WRITE) {
		ret = ext4_inode_attach_jinode(inode);
		if (ret < 0)
			return ret;
	}
	return dquot_file_open(inode, filp);
}

#if defined(MY_ABC_HERE)
#ifdef CONFIG_SPLICE_FROM_SOCKET
ssize_t ext4_splice_from_socket(struct file *file, struct socket *sock,
				loff_t __user *ppos, size_t count_req)
{
	struct address_space *mapping = file->f_mapping;
	const struct address_space_operations *a_ops = mapping->a_ops;
	struct inode *inode = mapping->host;
	int err = 0, remaining;
	struct kvec iov;
	struct msghdr msg = { 0 };
	size_t written = 0, verified_sz;
	struct kiocb iocb;
	struct iov_iter iter;

	init_sync_kiocb(&iocb, file);

	if (unlikely(iocb.ki_flags & IOCB_DIRECT))
		return -EINVAL;

	if (copy_from_user(&iocb.ki_pos, ppos, sizeof(loff_t)))
		return -EFAULT;

	/* minimal init of iter, used by write_check only */
	iov_iter_init(&iter, WRITE, NULL, 0, count_req);

	file_start_write(file);

	mutex_lock(&inode->i_mutex);
	verified_sz = generic_write_checks(&iocb, &iter);
	if (verified_sz <= 0) {
		pr_debug("%s: generic_write_checks err, verified_sz %zd\n",
			 __func__, verified_sz);
		err = verified_sz;
		goto cleanup;
	}

	/*
	 * If we have encountered a bitmap-format file, the size limit
	 * is smaller than s_maxbytes, which is for extent-mapped files.
	 */
	if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))) {
		struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);

		if (iocb.ki_pos >= sbi->s_bitmap_maxbytes) {
			err = -EFBIG;
			goto cleanup;
		}
		iov_iter_truncate(&iter, sbi->s_bitmap_maxbytes - iocb.ki_pos);
	}

	/* We can write back this queue in page reclaim */
	current->backing_dev_info = inode_to_bdi(inode);

	err = file_remove_privs(file);
	if (err) {
		pr_debug("%s: file_remove_privs, err %d\n", __func__, err);
		goto cleanup;
	}

	err = file_update_time(file);
	if (err) {
		pr_debug("%s: file_update_time, err %d\n", __func__, err);
		goto cleanup;
	}

	remaining = iter.count;

	while (remaining > 0) {
		unsigned long offset;	/* Offset into pagecache page */
		unsigned long bytes;	/* Bytes to write to page */
		int copied;		/* Bytes copied from net */
		struct page *page;
		void *fsdata;
		long rcvtimeo;
		char *paddr;

		offset = (iocb.ki_pos & (PAGE_CACHE_SIZE - 1));
		bytes = min_t(unsigned long, PAGE_CACHE_SIZE - offset,
			      remaining);

		err = a_ops->write_begin(file, mapping, iocb.ki_pos,
					 bytes, AOP_FLAG_UNINTERRUPTIBLE,
					 &page, &fsdata);
		if (unlikely(err)) {
			pr_debug("%s: write_begin err %d\n", __func__, err);
			break;
		}

		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);

		/* save page address for partial recvmsg case */
		paddr = kmap(page) + offset;
		iov.iov_base = paddr;
		iov.iov_len = bytes;

		rcvtimeo = sock->sk->sk_rcvtimeo;
		sock->sk->sk_rcvtimeo = 5 * HZ;

		/* IOV is ready, receive the data from socket now */
		copied = kernel_recvmsg(sock, &msg, &iov, 1,
					bytes, MSG_WAITALL);

		sock->sk->sk_rcvtimeo = rcvtimeo;

		/* kernel_recvmsg returned an error or no data */
		if (unlikely(copied <= 0)) {
			kunmap(page);

			/* update error and quit */
			err = copied;

			pr_debug("%s: kernel_recvmsg err %d\n", __func__, err);

			/* release pagecache */
			a_ops->write_end(file, mapping, iocb.ki_pos,
					 bytes, 0, page, fsdata);
			break;
		}

		if (unlikely(copied != bytes)) {
			char *kaddr;
			char *buff;

			/* recvmsg failed to write the requested bytes, this
			 * can happen from NEED_RESCHED signal or socket
			 * timeout. Partial writes are not allowed so we write
			 * the recvmsg portion and finish splice, this will
			 * force the caller to redo the remaining.
			 */

			pr_debug("%s: partial bytes %ld copied %d\n",
				 __func__, bytes, copied);

			/* alloc buffer for recvmsg data */
			buff = kmalloc(copied, GFP_KERNEL);
			if (unlikely(!buff)) {
				err = -ENOMEM;
				break;
			}
			/* copy recvmsg bytes to buffer */
			memcpy(buff, paddr, copied);

			/* and free the partial page */
			kunmap(page);
			err = a_ops->write_end(file, mapping, iocb.ki_pos,
					       bytes, 0, page, fsdata);
			if (unlikely(err < 0)) {
				kfree(buff);
				pr_debug("%s: write_end partial, err %d\n",
					 __func__, err);
				break;
			}

			/* allocate a new page with recvmsg size */
			err = a_ops->write_begin(file, mapping, iocb.ki_pos, copied,
						 AOP_FLAG_UNINTERRUPTIBLE,
						 &page, &fsdata);
			if (unlikely(err)) {
				kfree(buff);
				pr_debug("%s: write_begin partial, err %d\n",
					 __func__, err);
				break;
			}

			if (mapping_writably_mapped(mapping))
				flush_dcache_page(page);

			/* copy the buffer to new page */
			kaddr = kmap_atomic(page) + offset;
			memcpy(kaddr, buff, copied);

			kfree(buff);
			kunmap_atomic(kaddr);

			/* and write it */
			mark_page_accessed(page);
			err = a_ops->write_end(file, mapping, iocb.ki_pos,
					       copied, copied, page, fsdata);
			if (unlikely(err < 0)) {
				pr_debug("%s: write_end partial, err %d\n",
					 __func__, err);
				break;
			}

			/* update written counters */
			iocb.ki_pos += copied;
			written += copied;

			WARN_ON(copied != err);

			break;
		}

		kunmap(page);

		/* page written w/o recvmsg error */
		mark_page_accessed(page);
		err = a_ops->write_end(file, mapping, iocb.ki_pos, bytes,
				       copied, page, fsdata);

		if (unlikely(err < 0)) {
			pr_debug("%s: write_end, err %d\n", __func__, err);
			break;
		}

		/* write success, update counters */
		remaining -= copied;
		iocb.ki_pos += copied;
		written += copied;

		if (WARN_ON(copied != err))
			break;
	}

	if (written > 0)
		balance_dirty_pages_ratelimited(mapping);

cleanup:
	current->backing_dev_info = NULL;

	mutex_unlock(&inode->i_mutex);

	if (written > 0) {
		err = generic_write_sync(file, iocb.ki_pos - written, written);
		if (err < 0) {
			written = 0;
			goto done;
		}
		fsnotify_modify(file);

		if (copy_to_user(ppos, &iocb.ki_pos, sizeof(loff_t))) {
			written = 0;
			err = -EFAULT;
		}
	}
done:
	file_end_write(file);

	return written ? written : err;
}
#endif
#endif /* MY_ABC_HERE */

/*
 * Here we use ext4_map_blocks() to get a block mapping for a extent-based
 * file rather than ext4_ext_walk_space() because we can introduce
 * SEEK_DATA/SEEK_HOLE for block-mapped and extent-mapped file at the same
 * function.  When extent status tree has been fully implemented, it will
 * track all extent status for a file and we can directly use it to
 * retrieve the offset for SEEK_DATA/SEEK_HOLE.
 */

/*
 * When we retrieve the offset for SEEK_DATA/SEEK_HOLE, we would need to
 * lookup page cache to check whether or not there has some data between
 * [startoff, endoff] because, if this range contains an unwritten extent,
 * we determine this extent as a data or a hole according to whether the
 * page cache has data or not.
 */
static int ext4_find_unwritten_pgoff(struct inode *inode,
				     int whence,
				     struct ext4_map_blocks *map,
				     loff_t *offset)
{
	struct pagevec pvec;
	unsigned int blkbits;
	pgoff_t index;
	pgoff_t end;
	loff_t endoff;
	loff_t startoff;
	loff_t lastoff;
	int found = 0;

	blkbits = inode->i_sb->s_blocksize_bits;
	startoff = *offset;
	lastoff = startoff;
	endoff = (loff_t)(map->m_lblk + map->m_len) << blkbits;

	index = startoff >> PAGE_CACHE_SHIFT;
	end = endoff >> PAGE_CACHE_SHIFT;

	pagevec_init(&pvec, 0);
	do {
		int i, num;
		unsigned long nr_pages;

		num = min_t(pgoff_t, end - index, PAGEVEC_SIZE - 1) + 1;
		nr_pages = pagevec_lookup(&pvec, inode->i_mapping, index,
					  (pgoff_t)num);
		if (nr_pages == 0)
			break;

		for (i = 0; i < nr_pages; i++) {
			struct page *page = pvec.pages[i];
			struct buffer_head *bh, *head;

			/*
			 * If current offset is smaller than the page offset,
			 * there is a hole at this offset.
			 */
			if (whence == SEEK_HOLE && lastoff < endoff &&
			    lastoff < page_offset(pvec.pages[i])) {
				found = 1;
				*offset = lastoff;
				goto out;
			}

			if (page->index > end)
				goto out;

			lock_page(page);

			if (unlikely(page->mapping != inode->i_mapping)) {
				unlock_page(page);
				continue;
			}

			if (!page_has_buffers(page)) {
				unlock_page(page);
				continue;
			}

			if (page_has_buffers(page)) {
				lastoff = page_offset(page);
				bh = head = page_buffers(page);
				do {
					if (lastoff + bh->b_size <= startoff)
						goto next;
					if (buffer_uptodate(bh) ||
					    buffer_unwritten(bh)) {
						if (whence == SEEK_DATA)
							found = 1;
					} else {
						if (whence == SEEK_HOLE)
							found = 1;
					}
					if (found) {
						*offset = max_t(loff_t,
							startoff, lastoff);
						unlock_page(page);
						goto out;
					}
next:
					lastoff += bh->b_size;
					bh = bh->b_this_page;
				} while (bh != head);
			}

			lastoff = page_offset(page) + PAGE_SIZE;
			unlock_page(page);
		}

		/* The no. of pages is less than our desired, we are done. */
		if (nr_pages < num)
			break;

		index = pvec.pages[i - 1]->index + 1;
		pagevec_release(&pvec);
	} while (index <= end);

	if (whence == SEEK_HOLE && lastoff < endoff) {
		found = 1;
		*offset = lastoff;
	}
out:
	pagevec_release(&pvec);
	return found;
}

/*
 * ext4_seek_data() retrieves the offset for SEEK_DATA.
 */
static loff_t ext4_seek_data(struct file *file, loff_t offset, loff_t maxsize)
{
	struct inode *inode = file->f_mapping->host;
	struct ext4_map_blocks map;
	struct extent_status es;
	ext4_lblk_t start, last, end;
	loff_t dataoff, isize;
	int blkbits;
	int ret = 0;

	inode_lock(inode);

	isize = i_size_read(inode);
	if (offset < 0 || offset >= isize) {
		mutex_unlock(&inode->i_mutex);
		return -ENXIO;
	}

	blkbits = inode->i_sb->s_blocksize_bits;
	start = offset >> blkbits;
	last = start;
	end = isize >> blkbits;
	dataoff = offset;

	do {
		map.m_lblk = last;
		map.m_len = end - last + 1;
		ret = ext4_map_blocks(NULL, inode, &map, 0);
		if (ret > 0 && !(map.m_flags & EXT4_MAP_UNWRITTEN)) {
			if (last != start)
				dataoff = (loff_t)last << blkbits;
			break;
		}

		/*
		 * If there is a delay extent at this offset,
		 * it will be as a data.
		 */
		ext4_es_find_delayed_extent_range(inode, last, last, &es);
		if (es.es_len != 0 && in_range(last, es.es_lblk, es.es_len)) {
			if (last != start)
				dataoff = (loff_t)last << blkbits;
			break;
		}

		/*
		 * If there is a unwritten extent at this offset,
		 * it will be as a data or a hole according to page
		 * cache that has data or not.
		 */
		if (map.m_flags & EXT4_MAP_UNWRITTEN) {
			int unwritten;
			unwritten = ext4_find_unwritten_pgoff(inode, SEEK_DATA,
							      &map, &dataoff);
			if (unwritten)
				break;
		}

		last++;
		dataoff = (loff_t)last << blkbits;
	} while (last <= end);

	inode_unlock(inode);

	if (dataoff > isize)
		return -ENXIO;

	return vfs_setpos(file, dataoff, maxsize);
}

/*
 * ext4_seek_hole() retrieves the offset for SEEK_HOLE.
 */
static loff_t ext4_seek_hole(struct file *file, loff_t offset, loff_t maxsize)
{
	struct inode *inode = file->f_mapping->host;
	struct ext4_map_blocks map;
	struct extent_status es;
	ext4_lblk_t start, last, end;
	loff_t holeoff, isize;
	int blkbits;
	int ret = 0;

	inode_lock(inode);

	isize = i_size_read(inode);
	if (offset < 0 || offset >= isize) {
		mutex_unlock(&inode->i_mutex);
		return -ENXIO;
	}

	blkbits = inode->i_sb->s_blocksize_bits;
	start = offset >> blkbits;
	last = start;
	end = isize >> blkbits;
	holeoff = offset;

	do {
		map.m_lblk = last;
		map.m_len = end - last + 1;
		ret = ext4_map_blocks(NULL, inode, &map, 0);
		if (ret > 0 && !(map.m_flags & EXT4_MAP_UNWRITTEN)) {
			last += ret;
			holeoff = (loff_t)last << blkbits;
			continue;
		}

		/*
		 * If there is a delay extent at this offset,
		 * we will skip this extent.
		 */
		ext4_es_find_delayed_extent_range(inode, last, last, &es);
		if (es.es_len != 0 && in_range(last, es.es_lblk, es.es_len)) {
			last = es.es_lblk + es.es_len;
			holeoff = (loff_t)last << blkbits;
			continue;
		}

		/*
		 * If there is a unwritten extent at this offset,
		 * it will be as a data or a hole according to page
		 * cache that has data or not.
		 */
		if (map.m_flags & EXT4_MAP_UNWRITTEN) {
			int unwritten;
			unwritten = ext4_find_unwritten_pgoff(inode, SEEK_HOLE,
							      &map, &holeoff);
			if (!unwritten) {
				last += ret;
				holeoff = (loff_t)last << blkbits;
				continue;
			}
		}

		/* find a hole */
		break;
	} while (last <= end);

	inode_unlock(inode);

	if (holeoff > isize)
		holeoff = isize;

	return vfs_setpos(file, holeoff, maxsize);
}

/*
 * ext4_llseek() handles both block-mapped and extent-mapped maxbytes values
 * by calling generic_file_llseek_size() with the appropriate maxbytes
 * value for each.
 */
loff_t ext4_llseek(struct file *file, loff_t offset, int whence)
{
	struct inode *inode = file->f_mapping->host;
	loff_t maxbytes;

	if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)))
		maxbytes = EXT4_SB(inode->i_sb)->s_bitmap_maxbytes;
	else
		maxbytes = inode->i_sb->s_maxbytes;

	switch (whence) {
	case SEEK_SET:
	case SEEK_CUR:
	case SEEK_END:
		return generic_file_llseek_size(file, offset, whence,
						maxbytes, i_size_read(inode));
	case SEEK_DATA:
		return ext4_seek_data(file, offset, maxbytes);
	case SEEK_HOLE:
		return ext4_seek_hole(file, offset, maxbytes);
	}

	return -EINVAL;
}

const struct file_operations ext4_file_operations = {
	.llseek		= ext4_llseek,
	.read_iter	= generic_file_read_iter,
	.write_iter	= ext4_file_write_iter,
	.unlocked_ioctl = ext4_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ext4_compat_ioctl,
#endif
	.mmap		= ext4_file_mmap,
	.open		= ext4_file_open,
	.release	= ext4_release_file,
	.fsync		= ext4_sync_file,
	.splice_read	= generic_file_splice_read,
	.splice_write	= iter_file_splice_write,
#if defined(CONFIG_SENDFILE_PATCH)
	.splice_from_socket = generic_splice_from_socket,
#endif
#if defined(MY_ABC_HERE)
#ifdef CONFIG_SPLICE_FROM_SOCKET
	.splice_from_socket = ext4_splice_from_socket,
#endif
#endif /* MY_ABC_HERE */
	.fallocate	= ext4_fallocate,
};

const struct inode_operations ext4_file_inode_operations = {
#ifdef MY_ABC_HERE
	.syno_getattr	= ext4_syno_getattr,
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	.syno_get_archive_ver	= ext4_syno_get_archive_ver,
	.syno_set_archive_ver	= ext4_syno_set_archive_ver,
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	.syno_pattern_check = ext4_syno_pattern_check,
#endif /* MY_ABC_HERE */
	.setattr	= ext4_setattr,
	.getattr	= ext4_getattr,
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.listxattr	= ext4_listxattr,
	.removexattr	= generic_removexattr,
#ifdef MY_ABC_HERE
	.syno_acl_get   = ext4_get_syno_acl,
	.syno_acl_set	= ext4_set_syno_acl,
#else
	.get_acl	= ext4_get_acl,
	.set_acl	= ext4_set_acl,
#endif /* MY_ABC_HERE */
	.fiemap		= ext4_fiemap,
#ifdef MY_ABC_HERE
	.syno_rbd_meta_file_activate    = ext4_rbd_meta_file_activate,
	.syno_rbd_meta_file_deactivate  = ext4_rbd_meta_file_deactivate,
	.syno_rbd_meta_file_mapping 	= ext4_rbd_meta_file_mapping,
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
	.fsdev_mapping = ext4_fsdev_mapping,
#endif /* MY_ABC_HERE */
};

