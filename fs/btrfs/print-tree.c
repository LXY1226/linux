#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
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
#include "disk-io.h"
#include "print-tree.h"

static void print_chunk(struct extent_buffer *eb, struct btrfs_chunk *chunk)
{
	int num_stripes = btrfs_chunk_num_stripes(eb, chunk);
	int i;
	printk(KERN_INFO "\t\tchunk length %llu owner %llu type %llu "
	       "num_stripes %d\n",
	       btrfs_chunk_length(eb, chunk), btrfs_chunk_owner(eb, chunk),
	       btrfs_chunk_type(eb, chunk), num_stripes);
	for (i = 0 ; i < num_stripes ; i++) {
		printk(KERN_INFO "\t\t\tstripe %d devid %llu offset %llu\n", i,
		      btrfs_stripe_devid_nr(eb, chunk, i),
		      btrfs_stripe_offset_nr(eb, chunk, i));
	}
}
static void print_dev_item(struct extent_buffer *eb,
			   struct btrfs_dev_item *dev_item)
{
	printk(KERN_INFO "\t\tdev item devid %llu "
	       "total_bytes %llu bytes used %llu\n",
	       btrfs_device_id(eb, dev_item),
	       btrfs_device_total_bytes(eb, dev_item),
	       btrfs_device_bytes_used(eb, dev_item));
}
static void print_extent_data_ref(struct extent_buffer *eb,
				  struct btrfs_extent_data_ref *ref)
{
	printk(KERN_INFO "\t\textent data backref root %llu "
	       "objectid %llu offset %llu count %u\n",
	       btrfs_extent_data_ref_root(eb, ref),
	       btrfs_extent_data_ref_objectid(eb, ref),
	       btrfs_extent_data_ref_offset(eb, ref),
	       btrfs_extent_data_ref_count(eb, ref));
}

static void print_extent_item(struct extent_buffer *eb, int slot, int type)
{
	struct btrfs_extent_item *ei;
	struct btrfs_extent_inline_ref *iref;
	struct btrfs_extent_data_ref *dref;
	struct btrfs_shared_data_ref *sref;
	struct btrfs_disk_key key;
	unsigned long end;
	unsigned long ptr;
	u32 item_size = btrfs_item_size_nr(eb, slot);
	u64 flags;
	u64 offset;

	if (item_size < sizeof(*ei)) {
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		struct btrfs_extent_item_v0 *ei0;
		BUG_ON(item_size != sizeof(*ei0));
		ei0 = btrfs_item_ptr(eb, slot, struct btrfs_extent_item_v0);
		printk(KERN_INFO "\t\textent refs %u\n",
		       btrfs_extent_refs_v0(eb, ei0));
		return;
#else
		BUG();
#endif
	}

	ei = btrfs_item_ptr(eb, slot, struct btrfs_extent_item);
	flags = btrfs_extent_flags(eb, ei);

	printk(KERN_INFO "\t\textent refs %llu gen %llu flags %llu\n",
	       btrfs_extent_refs(eb, ei), btrfs_extent_generation(eb, ei),
	       flags);

	if ((type == BTRFS_EXTENT_ITEM_KEY) &&
	    flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
		struct btrfs_tree_block_info *info;
		info = (struct btrfs_tree_block_info *)(ei + 1);
		btrfs_tree_block_key(eb, info, &key);
		printk(KERN_INFO "\t\ttree block key (%llu %u %llu) "
		       "level %d\n",
		       btrfs_disk_key_objectid(&key), key.type,
		       btrfs_disk_key_offset(&key),
		       btrfs_tree_block_level(eb, info));
		iref = (struct btrfs_extent_inline_ref *)(info + 1);
	} else {
		iref = (struct btrfs_extent_inline_ref *)(ei + 1);
	}

	ptr = (unsigned long)iref;
	end = (unsigned long)ei + item_size;
	while (ptr < end) {
		iref = (struct btrfs_extent_inline_ref *)ptr;
		type = btrfs_extent_inline_ref_type(eb, iref);
		offset = btrfs_extent_inline_ref_offset(eb, iref);
		switch (type) {
		case BTRFS_TREE_BLOCK_REF_KEY:
			printk(KERN_INFO "\t\ttree block backref "
				"root %llu\n", offset);
			break;
		case BTRFS_SHARED_BLOCK_REF_KEY:
			printk(KERN_INFO "\t\tshared block backref "
				"parent %llu\n", offset);
			break;
		case BTRFS_EXTENT_DATA_REF_KEY:
			dref = (struct btrfs_extent_data_ref *)(&iref->offset);
			print_extent_data_ref(eb, dref);
			break;
		case BTRFS_SHARED_DATA_REF_KEY:
			sref = (struct btrfs_shared_data_ref *)(iref + 1);
			printk(KERN_INFO "\t\tshared data backref "
			       "parent %llu count %u\n",
			       offset, btrfs_shared_data_ref_count(eb, sref));
			break;
		default:
			btrfs_err(eb->fs_info,
				  "extent %llu has invalid ref type %d",
				  eb->start, type);
			return;
		}
		ptr += btrfs_extent_inline_ref_size(type);
	}
	WARN_ON(ptr > end);
}

#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
static void print_extent_ref_v0(struct extent_buffer *eb, int slot)
{
	struct btrfs_extent_ref_v0 *ref0;

	ref0 = btrfs_item_ptr(eb, slot, struct btrfs_extent_ref_v0);
	printk("\t\textent back ref root %llu gen %llu "
		"owner %llu num_refs %lu\n",
		btrfs_ref_root_v0(eb, ref0),
		btrfs_ref_generation_v0(eb, ref0),
		btrfs_ref_objectid_v0(eb, ref0),
		(unsigned long)btrfs_ref_count_v0(eb, ref0));
}
#endif

static void print_uuid_item(struct extent_buffer *l, unsigned long offset,
			    u32 item_size)
{
	if (!IS_ALIGNED(item_size, sizeof(u64))) {
		pr_warn("BTRFS: uuid item with illegal size %lu!\n",
			(unsigned long)item_size);
		return;
	}
	while (item_size) {
		__le64 subvol_id;

		read_extent_buffer(l, &subvol_id, offset, sizeof(subvol_id));
		printk(KERN_INFO "\t\tsubvol_id %llu\n",
		       (unsigned long long)le64_to_cpu(subvol_id));
		item_size -= sizeof(u64);
		offset += sizeof(u64);
	}
}

void btrfs_print_leaf(struct extent_buffer *l)
{
	struct btrfs_fs_info *fs_info;
	int i;
	u32 type, nr;
	struct btrfs_item *item;
	struct btrfs_root_item *ri;
	struct btrfs_dir_item *di;
	struct btrfs_inode_item *ii;
	struct btrfs_block_group_item *bi;
	struct btrfs_file_extent_item *fi;
	struct btrfs_extent_data_ref *dref;
	struct btrfs_shared_data_ref *sref;
	struct btrfs_dev_extent *dev_extent;
	struct btrfs_key key;
	struct btrfs_key found_key;

	if (!l)
		return;

	fs_info = l->fs_info;
	nr = btrfs_header_nritems(l);

	btrfs_info(fs_info,
		   "leaf %llu gen %llu total ptrs %d free space %d owner %llu",
		   btrfs_header_bytenr(l), btrfs_header_generation(l), nr,
		   btrfs_leaf_free_space(fs_info->tree_root, l), btrfs_header_owner(l));
	for (i = 0 ; i < nr ; i++) {
		item = btrfs_item_nr(i);
		btrfs_item_key_to_cpu(l, &key, i);
		type = key.type;
		printk(KERN_INFO "\titem %d key (%llu %u %llu) itemoff %d "
		       "itemsize %d\n",
			i, key.objectid, type, key.offset,
			btrfs_item_offset(l, item), btrfs_item_size(l, item));
		switch (type) {
		case BTRFS_INODE_ITEM_KEY:
			ii = btrfs_item_ptr(l, i, struct btrfs_inode_item);
			printk(KERN_INFO "\t\tinode generation %llu size %llu "
			       "mode %o\n",
			       btrfs_inode_generation(l, ii),
			       btrfs_inode_size(l, ii),
			       btrfs_inode_mode(l, ii));
			break;
		case BTRFS_DIR_ITEM_KEY:
			di = btrfs_item_ptr(l, i, struct btrfs_dir_item);
			btrfs_dir_item_key_to_cpu(l, di, &found_key);
			printk(KERN_INFO "\t\tdir oid %llu type %u\n",
				found_key.objectid,
				btrfs_dir_type(l, di));
			break;
		case BTRFS_ROOT_ITEM_KEY:
			ri = btrfs_item_ptr(l, i, struct btrfs_root_item);
			printk(KERN_INFO "\t\troot data bytenr %llu refs %u\n",
				btrfs_disk_root_bytenr(l, ri),
				btrfs_disk_root_refs(l, ri));
			break;
		case BTRFS_EXTENT_ITEM_KEY:
		case BTRFS_METADATA_ITEM_KEY:
			print_extent_item(l, i, type);
			break;
		case BTRFS_TREE_BLOCK_REF_KEY:
			printk(KERN_INFO "\t\ttree block backref\n");
			break;
		case BTRFS_SHARED_BLOCK_REF_KEY:
			printk(KERN_INFO "\t\tshared block backref\n");
			break;
		case BTRFS_EXTENT_DATA_REF_KEY:
			dref = btrfs_item_ptr(l, i,
					      struct btrfs_extent_data_ref);
			print_extent_data_ref(l, dref);
			break;
		case BTRFS_SHARED_DATA_REF_KEY:
			sref = btrfs_item_ptr(l, i,
					      struct btrfs_shared_data_ref);
			printk(KERN_INFO "\t\tshared data backref count %u\n",
			       btrfs_shared_data_ref_count(l, sref));
			break;
		case BTRFS_EXTENT_DATA_KEY:
			fi = btrfs_item_ptr(l, i,
					    struct btrfs_file_extent_item);
			if (btrfs_file_extent_type(l, fi) ==
			    BTRFS_FILE_EXTENT_INLINE) {
				printk(KERN_INFO "\t\tinline extent data "
				       "size %u\n",
				       btrfs_file_extent_inline_len(l, i, fi));
				break;
			}
			printk(KERN_INFO "\t\textent data disk bytenr %llu "
			       "nr %llu\n",
			       btrfs_file_extent_disk_bytenr(l, fi),
			       btrfs_file_extent_disk_num_bytes(l, fi));
			printk(KERN_INFO "\t\textent data offset %llu "
			       "nr %llu ram %llu\n",
			       btrfs_file_extent_offset(l, fi),
			       btrfs_file_extent_num_bytes(l, fi),
			       btrfs_file_extent_ram_bytes(l, fi));
			break;
		case BTRFS_EXTENT_REF_V0_KEY:
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
			print_extent_ref_v0(l, i);
#else
			BUG();
#endif
			break;
		case BTRFS_BLOCK_GROUP_ITEM_KEY:
			bi = btrfs_item_ptr(l, i,
					    struct btrfs_block_group_item);
			printk(KERN_INFO "\t\tblock group used %llu\n",
			       btrfs_disk_block_group_used(l, bi));
			break;
		case BTRFS_CHUNK_ITEM_KEY:
			print_chunk(l, btrfs_item_ptr(l, i,
						      struct btrfs_chunk));
			break;
		case BTRFS_DEV_ITEM_KEY:
			print_dev_item(l, btrfs_item_ptr(l, i,
					struct btrfs_dev_item));
			break;
		case BTRFS_DEV_EXTENT_KEY:
			dev_extent = btrfs_item_ptr(l, i,
						    struct btrfs_dev_extent);
			printk(KERN_INFO "\t\tdev extent chunk_tree %llu\n"
			       "\t\tchunk objectid %llu chunk offset %llu "
			       "length %llu\n",
			       btrfs_dev_extent_chunk_tree(l, dev_extent),
			       btrfs_dev_extent_chunk_objectid(l, dev_extent),
			       btrfs_dev_extent_chunk_offset(l, dev_extent),
			       btrfs_dev_extent_length(l, dev_extent));
			break;
		case BTRFS_DEV_STATS_KEY:
			printk(KERN_INFO "\t\tdevice stats\n");
			break;
		case BTRFS_DEV_REPLACE_KEY:
			printk(KERN_INFO "\t\tdev replace\n");
			break;
		case BTRFS_UUID_KEY_SUBVOL:
		case BTRFS_UUID_KEY_RECEIVED_SUBVOL:
			print_uuid_item(l, btrfs_item_ptr_offset(l, i),
					btrfs_item_size_nr(l, i));
			break;
		};
	}
}

void btrfs_print_tree(struct extent_buffer *c, bool follow)
{
	struct btrfs_fs_info *fs_info;
	int i; u32 nr;
	struct btrfs_key key;
	int level;

	if (!c)
		return;
	fs_info = c->fs_info;
	nr = btrfs_header_nritems(c);
	level = btrfs_header_level(c);
	if (level == 0) {
		btrfs_print_leaf(c);
		return;
	}
	btrfs_info(fs_info,
		   "node %llu level %d gen %llu total ptrs %d free spc %u owner %llu",
		   btrfs_header_bytenr(c), level, btrfs_header_generation(c),
		   nr, (u32)BTRFS_NODEPTRS_PER_BLOCK(fs_info->tree_root) - nr,
		   btrfs_header_owner(c));
	for (i = 0; i < nr; i++) {
		btrfs_node_key_to_cpu(c, &key, i);
		printk(KERN_INFO "\tkey %d (%llu %u %llu) block %llu gen %llu\n",
		       i, key.objectid, key.type, key.offset,
		       btrfs_node_blockptr(c, i),
		       btrfs_node_ptr_generation(c, i));
	}
	if (!follow)
		return;
	for (i = 0; i < nr; i++) {
		struct btrfs_key first_key;
		struct extent_buffer *next;

		btrfs_node_key_to_cpu(c, &first_key, i);
#ifdef MY_DEF_HERE
		/*
		 * Passing tree_root rather than the actual root searching for this
		 * block will make perf stat count on wrong root, but this is error case,
		 * so it's ok.
		 */
#endif /* MY_DEF_HERE */
		next = read_tree_block(fs_info->tree_root, btrfs_node_blockptr(c, i),
				       btrfs_node_ptr_generation(c, i),
				       level - 1, &first_key);
		if (IS_ERR(next)) {
			continue;
		} else if (!extent_buffer_uptodate(next)) {
			free_extent_buffer(next);
			continue;
		}

		if (btrfs_is_leaf(next) &&
		   level != 1)
			BUG();
		if (btrfs_header_level(next) !=
		       level - 1)
			BUG();
		btrfs_print_tree(next, follow);
		free_extent_buffer(next);
	}
}
