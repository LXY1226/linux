#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
   raid0.c : Multiple Devices driver for Linux
	     Copyright (C) 1994-96 Marc ZYNGIER
	     <zyngier@ufr-info-p7.ibp.fr> or
	     <maz@gloups.fdn.fr>
	     Copyright (C) 1999, 2000 Ingo Molnar, Red Hat

   RAID-0 management functions.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/blkdev.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/slab.h>
#ifdef MY_ABC_HERE
#include <linux/ratelimit.h>
#endif /* MY_ABC_HERE */
#include <trace/events/block.h>
#include "md.h"
#include "raid0.h"
#include "raid5.h"

static int default_layout = 0;
module_param(default_layout, int, 0644);

static int raid0_congested(struct mddev *mddev, int bits)
{
	struct r0conf *conf = mddev->private;
	struct md_rdev **devlist = conf->devlist;
	int raid_disks = conf->strip_zone[0].nb_dev;
	int i, ret = 0;

#ifdef MY_ABC_HERE
	/* when raid0 lose one of disks, it is not normally,
	 * So we just do a fake report that it is fine,
	 * Nor will encounter NULL pointer access in devlist[i]->bdev.
	 */
	if(mddev->degraded) {
		/* just report it's fine */
		return ret;
	}
#endif /* MY_ABC_HERE */

	for (i = 0; i < raid_disks && !ret ; i++) {
		struct request_queue *q = bdev_get_queue(devlist[i]->bdev);

		ret |= bdi_congested(q->backing_dev_info, bits);
	}
	return ret;
}

/*
 * inform the user of the raid configuration
*/
static void dump_zones(struct mddev *mddev)
{
	int j, k;
	sector_t zone_size = 0;
	sector_t zone_start = 0;
	char b[BDEVNAME_SIZE];
	struct r0conf *conf = mddev->private;
	int raid_disks = conf->strip_zone[0].nb_dev;
	printk(KERN_INFO "md: RAID0 configuration for %s - %d zone%s\n",
	       mdname(mddev),
	       conf->nr_strip_zones, conf->nr_strip_zones==1?"":"s");
	for (j = 0; j < conf->nr_strip_zones; j++) {
		printk(KERN_INFO "md: zone%d=[", j);
		for (k = 0; k < conf->strip_zone[j].nb_dev; k++)
			printk(KERN_CONT "%s%s", k?"/":"",
			bdevname(conf->devlist[j*raid_disks
						+ k]->bdev, b));
		printk(KERN_CONT "]\n");

		zone_size  = conf->strip_zone[j].zone_end - zone_start;
		printk(KERN_INFO "      zone-offset=%10lluKB, "
				"device-offset=%10lluKB, size=%10lluKB\n",
			(unsigned long long)zone_start>>1,
			(unsigned long long)conf->strip_zone[j].dev_start>>1,
			(unsigned long long)zone_size>>1);
		zone_start = conf->strip_zone[j].zone_end;
	}
	printk(KERN_INFO "\n");
}

static int create_strip_zones(struct mddev *mddev, struct r0conf **private_conf)
{
	int i, c, err;
	sector_t curr_zone_end, sectors;
	struct md_rdev *smallest, *rdev1, *rdev2, *rdev, **dev;
	struct strip_zone *zone;
	int cnt;
	char b[BDEVNAME_SIZE];
	char b2[BDEVNAME_SIZE];
	struct r0conf *conf = kzalloc(sizeof(*conf), GFP_KERNEL);
	unsigned blksize = 512;

	if (!conf)
		return -ENOMEM;
	rdev_for_each(rdev1, mddev) {
		pr_debug("md/raid0:%s: looking at %s\n",
			 mdname(mddev),
			 bdevname(rdev1->bdev, b));
		c = 0;

		/* round size to chunk_size */
		sectors = rdev1->sectors;
		sector_div(sectors, mddev->chunk_sectors);
		rdev1->sectors = sectors * mddev->chunk_sectors;

		blksize = max(blksize, queue_logical_block_size(
				      rdev1->bdev->bd_disk->queue));

		rdev_for_each(rdev2, mddev) {
			pr_debug("md/raid0:%s:   comparing %s(%llu)"
				 " with %s(%llu)\n",
				 mdname(mddev),
				 bdevname(rdev1->bdev,b),
				 (unsigned long long)rdev1->sectors,
				 bdevname(rdev2->bdev,b2),
				 (unsigned long long)rdev2->sectors);
			if (rdev2 == rdev1) {
				pr_debug("md/raid0:%s:   END\n",
					 mdname(mddev));
				break;
			}
			if (rdev2->sectors == rdev1->sectors) {
				/*
				 * Not unique, don't count it as a new
				 * group
				 */
				pr_debug("md/raid0:%s:   EQUAL\n",
					 mdname(mddev));
				c = 1;
				break;
			}
			pr_debug("md/raid0:%s:   NOT EQUAL\n",
				 mdname(mddev));
		}
		if (!c) {
			pr_debug("md/raid0:%s:   ==> UNIQUE\n",
				 mdname(mddev));
			conf->nr_strip_zones++;
			pr_debug("md/raid0:%s: %d zones\n",
				 mdname(mddev), conf->nr_strip_zones);
		}
	}
	pr_debug("md/raid0:%s: FINAL %d zones\n",
		 mdname(mddev), conf->nr_strip_zones);

	if (conf->nr_strip_zones == 1) {
		conf->layout = RAID0_ORIG_LAYOUT;
	} else if (mddev->layout == RAID0_ORIG_LAYOUT ||
		   mddev->layout == RAID0_ALT_MULTIZONE_LAYOUT) {
		conf->layout = mddev->layout;
	} else if (default_layout == RAID0_ORIG_LAYOUT ||
		   default_layout == RAID0_ALT_MULTIZONE_LAYOUT) {
		conf->layout = default_layout;
	} else {
		pr_err("md/raid0:%s: cannot assemble multi-zone RAID0 with default_layout setting\n",
		       mdname(mddev));
		pr_err("md/raid0: please set raid0.default_layout to 1 or 2\n");
		err = -ENOTSUPP;
		goto abort;
	}
	/*
	 * now since we have the hard sector sizes, we can make sure
	 * chunk size is a multiple of that sector size
	 */
	if ((mddev->chunk_sectors << 9) % blksize) {
		printk(KERN_ERR "md/raid0:%s: chunk_size of %d not multiple of block size %d\n",
		       mdname(mddev),
		       mddev->chunk_sectors << 9, blksize);
		err = -EINVAL;
		goto abort;
	}

	err = -ENOMEM;
	conf->strip_zone = kzalloc(sizeof(struct strip_zone)*
				conf->nr_strip_zones, GFP_KERNEL);
	if (!conf->strip_zone)
		goto abort;
	conf->devlist = kzalloc(sizeof(struct md_rdev*)*
				conf->nr_strip_zones*mddev->raid_disks,
				GFP_KERNEL);
	if (!conf->devlist)
		goto abort;

	/* The first zone must contain all devices, so here we check that
	 * there is a proper alignment of slots to devices and find them all
	 */
	zone = &conf->strip_zone[0];
	cnt = 0;
	smallest = NULL;
	dev = conf->devlist;
	err = -EINVAL;
	rdev_for_each(rdev1, mddev) {
		int j = rdev1->raid_disk;

		if (mddev->level == 10) {
			/* taking over a raid10-n2 array */
			j /= 2;
			rdev1->new_raid_disk = j;
		}

		if (mddev->level == 1) {
			/* taiking over a raid1 array-
			 * we have only one active disk
			 */
			j = 0;
			rdev1->new_raid_disk = j;
		}

		if (j < 0) {
			printk(KERN_ERR
			       "md/raid0:%s: remove inactive devices before converting to RAID0\n",
			       mdname(mddev));
			goto abort;
		}
		if (j >= mddev->raid_disks) {
			printk(KERN_ERR "md/raid0:%s: bad disk number %d - "
			       "aborting!\n", mdname(mddev), j);
			goto abort;
		}
		if (dev[j]) {
			printk(KERN_ERR "md/raid0:%s: multiple devices for %d - "
			       "aborting!\n", mdname(mddev), j);
			goto abort;
		}
		dev[j] = rdev1;

		if (!smallest || (rdev1->sectors < smallest->sectors))
			smallest = rdev1;
		cnt++;
	}
	if (cnt != mddev->raid_disks) {
		printk(KERN_ERR "md/raid0:%s: too few disks (%d of %d) - "
		       "aborting!\n", mdname(mddev), cnt, mddev->raid_disks);
#ifdef MY_ABC_HERE
		/* for raid0 status consistense to other raid type */
		mddev->degraded = mddev->raid_disks - cnt;
		zone->nb_dev = mddev->raid_disks;
		mddev->private = conf;
		return -ENODEV;
#else /* MY_ABC_HERE */
		goto abort;
#endif /* MY_ABC_HERE */
	}
	zone->nb_dev = cnt;
	zone->zone_end = smallest->sectors * cnt;

	curr_zone_end = zone->zone_end;

	/* now do the other zones */
	for (i = 1; i < conf->nr_strip_zones; i++)
	{
		int j;

		zone = conf->strip_zone + i;
		dev = conf->devlist + i * mddev->raid_disks;

		pr_debug("md/raid0:%s: zone %d\n", mdname(mddev), i);
		zone->dev_start = smallest->sectors;
		smallest = NULL;
		c = 0;

		for (j=0; j<cnt; j++) {
			rdev = conf->devlist[j];
			if (rdev->sectors <= zone->dev_start) {
				pr_debug("md/raid0:%s: checking %s ... nope\n",
					 mdname(mddev),
					 bdevname(rdev->bdev, b));
				continue;
			}
			pr_debug("md/raid0:%s: checking %s ..."
				 " contained as device %d\n",
				 mdname(mddev),
				 bdevname(rdev->bdev, b), c);
			dev[c] = rdev;
			c++;
			if (!smallest || rdev->sectors < smallest->sectors) {
				smallest = rdev;
				pr_debug("md/raid0:%s:  (%llu) is smallest!.\n",
					 mdname(mddev),
					 (unsigned long long)rdev->sectors);
			}
		}

		zone->nb_dev = c;
		sectors = (smallest->sectors - zone->dev_start) * c;
		pr_debug("md/raid0:%s: zone->nb_dev: %d, sectors: %llu\n",
			 mdname(mddev),
			 zone->nb_dev, (unsigned long long)sectors);

		curr_zone_end += sectors;
		zone->zone_end = curr_zone_end;

		pr_debug("md/raid0:%s: current zone start: %llu\n",
			 mdname(mddev),
			 (unsigned long long)smallest->sectors);
	}
#ifdef MY_ABC_HERE
	if (conf->nr_strip_zones == 1) {
		mddev->has_raid0_layout_feature = 0;
		mddev->layout = -1;
	}
#endif /* MY_ABC_HERE */

	pr_debug("md/raid0:%s: done.\n", mdname(mddev));
	*private_conf = conf;

	return 0;
abort:
	kfree(conf->strip_zone);
	kfree(conf->devlist);
	kfree(conf);
	*private_conf = ERR_PTR(err);
	return err;
}

/* Find the zone which holds a particular offset
 * Update *sectorp to be an offset in that zone
 */
static struct strip_zone *find_zone(struct r0conf *conf,
				    sector_t *sectorp)
{
	int i;
	struct strip_zone *z = conf->strip_zone;
	sector_t sector = *sectorp;

	for (i = 0; i < conf->nr_strip_zones; i++)
		if (sector < z[i].zone_end) {
			if (i)
				*sectorp = sector - z[i-1].zone_end;
			return z + i;
		}
	BUG();
}

/*
 * remaps the bio to the target device. we separate two flows.
 * power 2 flow and a general flow for the sake of performance
*/
static struct md_rdev *map_sector(struct mddev *mddev, struct strip_zone *zone,
				sector_t sector, sector_t *sector_offset)
{
	unsigned int sect_in_chunk;
	sector_t chunk;
	struct r0conf *conf = mddev->private;
	int raid_disks = conf->strip_zone[0].nb_dev;
	unsigned int chunk_sects = mddev->chunk_sectors;

	if (is_power_of_2(chunk_sects)) {
		int chunksect_bits = ffz(~chunk_sects);
		/* find the sector offset inside the chunk */
		sect_in_chunk  = sector & (chunk_sects - 1);
		sector >>= chunksect_bits;
		/* chunk in zone */
		chunk = *sector_offset;
		/* quotient is the chunk in real device*/
		sector_div(chunk, zone->nb_dev << chunksect_bits);
	} else{
		sect_in_chunk = sector_div(sector, chunk_sects);
		chunk = *sector_offset;
		sector_div(chunk, chunk_sects * zone->nb_dev);
	}
	/*
	*  position the bio over the real device
	*  real sector = chunk in device + starting of zone
	*	+ the position in the chunk
	*/
	*sector_offset = (chunk * chunk_sects) + sect_in_chunk;
	return conf->devlist[(zone - conf->strip_zone)*raid_disks
			     + sector_div(sector, zone->nb_dev)];
}

static sector_t raid0_size(struct mddev *mddev, sector_t sectors, int raid_disks)
{
	sector_t array_sectors = 0;
	struct md_rdev *rdev;

	WARN_ONCE(sectors || raid_disks,
		  "%s does not support generic reshape\n", __func__);

	rdev_for_each(rdev, mddev)
		array_sectors += (rdev->sectors &
				  ~(sector_t)(mddev->chunk_sectors-1));

	return array_sectors;
}

static void raid0_free(struct mddev *mddev, void *priv);

static int raid0_run(struct mddev *mddev)
{
	struct r0conf *conf;
	int ret;

#ifdef MY_ABC_HERE
	mddev->degraded = 0;
#endif /* MY_ABC_HERE */

	if (mddev->chunk_sectors == 0) {
		printk(KERN_ERR "md/raid0:%s: chunk size must be set.\n",
		       mdname(mddev));
		return -EINVAL;
	}
	if (md_check_no_bitmap(mddev))
		return -EINVAL;

	/* if private is not null, we are here after takeover */
	if (mddev->private == NULL) {
		ret = create_strip_zones(mddev, &conf);
#ifdef MY_ABC_HERE
		if (ret == -ENODEV) {
#ifdef MY_ABC_HERE
			if (MD_CRASHED_ASSEMBLE != mddev->nodev_and_crashed) {
				mddev->nodev_and_crashed = MD_CRASHED;
			}
#endif /* MY_ABC_HERE */
			/* The size must greater than zero,
			 * otherwise this partition would not present in /proc/partitions
			 */
			mddev->array_sectors = raid0_size(mddev, 0, 0);
			/* pretend success for printing mdstatus otherwise it will not show raid0 status when it fail on boot */
			return 0;
		}
#endif /* MY_ABC_HERE */
		if (ret < 0)
			return ret;
		mddev->private = conf;
	}
	conf = mddev->private;
	if (mddev->queue) {
		struct md_rdev *rdev;
		bool discard_supported = false;

		blk_queue_max_hw_sectors(mddev->queue, mddev->chunk_sectors);
		blk_queue_max_write_same_sectors(mddev->queue, mddev->chunk_sectors);
		blk_queue_max_discard_sectors(mddev->queue, mddev->chunk_sectors);

		blk_queue_io_min(mddev->queue, mddev->chunk_sectors << 9);
		blk_queue_io_opt(mddev->queue,
				 (mddev->chunk_sectors << 9) * mddev->raid_disks);

		rdev_for_each(rdev, mddev) {
			disk_stack_limits(mddev->gendisk, rdev->bdev,
					  rdev->data_offset << 9);
			if (blk_queue_discard(bdev_get_queue(rdev->bdev)))
				discard_supported = true;
		}
		if (!discard_supported)
			queue_flag_clear_unlocked(QUEUE_FLAG_DISCARD, mddev->queue);
		else
			queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, mddev->queue);
	}

	/* calculate array device size */
	md_set_array_sectors(mddev, raid0_size(mddev, 0, 0));

	printk(KERN_INFO "md/raid0:%s: md_size is %llu sectors.\n",
	       mdname(mddev),
	       (unsigned long long)mddev->array_sectors);

	if (mddev->queue) {
		/* calculate the max read-ahead size.
		 * For read-ahead of large files to be effective, we need to
		 * readahead at least twice a whole stripe. i.e. number of devices
		 * multiplied by chunk size times 2.
		 * If an individual device has an ra_pages greater than the
		 * chunk size, then we will not drive that device as hard as it
		 * wants.  We consider this a configuration error: a larger
		 * chunksize should be used in that case.
		 */
		int stripe = mddev->raid_disks *
			(mddev->chunk_sectors << 9) / PAGE_SIZE;
		if (mddev->queue->backing_dev_info->ra_pages < 2* stripe)
			mddev->queue->backing_dev_info->ra_pages = 2* stripe;
	}

	dump_zones(mddev);

	ret = md_integrity_register(mddev);

	return ret;
}

static void raid0_free(struct mddev *mddev, void *priv)
{
	struct r0conf *conf = priv;

	kfree(conf->strip_zone);
	kfree(conf->devlist);
	kfree(conf);
}

#ifdef MY_ABC_HERE
/**
 * This is end_io callback function.
 * We can use this for bad sector report and device error
 * handing. Prevent umount panic from file system
 *
 * @author \$Author: khchen $
 * @version \$Revision: 1.1
 *
 * @param bio    Should not be NULL. Passing from block layer
 * @param error  error number
 */
static void Raid0EndRequest(struct bio *bio)
{
	int bio_error = bio->bi_error;
	struct mddev *mddev = NULL;
	struct md_rdev *rdev = NULL;
	struct bio *orig_bio;

	orig_bio = bio->bi_private;

	rdev = (struct md_rdev *)orig_bio->bi_next;
	mddev = rdev->mddev;

	orig_bio->bi_next = bio->bi_next;
	orig_bio->bi_error = bio->bi_error;

	if (bio_error) {
		if (-ENODEV == bio_error) {
			syno_md_error(mddev, rdev);
		} else {
			/* Let raid0 could keep read.(md_error would let it become read-only) */
#ifdef MY_ABC_HERE
			sector_t mapped_sector, orig_sector, report_sector;
			struct strip_zone *zone;
			struct r0conf *conf = mddev->private;
			struct md_rdev *tmp_dev = NULL;

			mapped_sector = orig_sector = orig_bio->bi_iter.bi_sector;
			zone = find_zone(conf, &mapped_sector);
			switch (conf->layout) {
				case RAID0_ORIG_LAYOUT:
					tmp_dev = map_sector(mddev, zone, orig_sector, &mapped_sector);
					break;
				case RAID0_ALT_MULTIZONE_LAYOUT:
					tmp_dev = map_sector(mddev, zone, mapped_sector, &mapped_sector);
					break;
				default:
					break;
			}

			if (likely(tmp_dev))
				report_sector = mapped_sector + zone->dev_start + tmp_dev->data_offset;
			else
				report_sector = bio->bi_iter.bi_sector;

#ifdef MY_ABC_HERE
			if (bio_flagged(bio, BIO_AUTO_REMAP)) {
				SynoReportBadSector(report_sector, bio->bi_rw, mddev->md_minor,
					bio->bi_bdev, __FUNCTION__);
			}
#else /* MY_ABC_HERE */
			SynoReportBadSector(report_sector, bio->bi_rw, mddev->md_minor,
				bio->bi_bdev, __FUNCTION__);
#endif /* MY_ABC_HERE */
#endif /* MY_ABC_HERE */
			md_error(mddev, rdev);
		}
	}

	atomic_dec(&rdev->nr_pending);
	bio_put(bio);
	/* Let mount could successful and bad sector could keep accessing */
	bio_endio(orig_bio);
}
#endif /* MY_ABC_HERE */
/*
 * Is io distribute over 1 or more chunks ?
*/
static inline int is_io_in_chunk_boundary(struct mddev *mddev,
			unsigned int chunk_sects, struct bio *bio)
{
	if (likely(is_power_of_2(chunk_sects))) {
		return chunk_sects >=
			((bio->bi_iter.bi_sector & (chunk_sects-1))
					+ bio_sectors(bio));
	} else{
		sector_t sector = bio->bi_iter.bi_sector;
		return chunk_sects >= (sector_div(sector, chunk_sects)
						+ bio_sectors(bio));
	}
}

static void raid0_make_request(struct mddev *mddev, struct bio *bio)
{
	struct r0conf *conf = mddev->private;
	struct strip_zone *zone;
	struct md_rdev *tmp_dev;
	sector_t bio_sector;
	sector_t sector;
	sector_t orig_sector;
	unsigned chunk_sects;
	unsigned sectors;

#ifdef MY_ABC_HERE
	struct bio *cloned_bio, *orig_bio;
#endif /* MY_ABC_HERE */

	if (unlikely(bio->bi_rw & REQ_FLUSH)) {
		md_flush_request(mddev, bio);
		return;
	}

	bio_sector = bio->bi_iter.bi_sector;
	sector = bio_sector;
	chunk_sects = mddev->chunk_sectors;

	sectors = chunk_sects -
		(likely(is_power_of_2(chunk_sects))
		 ? (sector & (chunk_sects-1))
		 : sector_div(sector, chunk_sects));

#ifdef MY_ABC_HERE
	/**
	 * if there has any device offline, we don't make any request to
	 * our raid0 md array
	 */
#ifdef MY_ABC_HERE
	if (mddev->nodev_and_crashed) {
#else /* MY_ABC_HERE */
	if (mddev->degraded) {
#endif /* MY_ABC_HERE */
#ifdef  MY_ABC_HERE
		syno_flashcache_return_error(bio);
#else
		bio->bi_error = -EIO;
		bio_endio(bio);
#endif /* MY_ABC_HERE */
		return;
	}
#endif /* MY_ABC_HERE */

	/* Restore due to sector_div */
	sector = bio_sector;

	if (sectors < bio_sectors(bio)) {
		struct bio *split = bio_split(bio, sectors, GFP_NOIO, mddev->bio_set);
		bio_chain(split, bio);
#ifdef MY_ABC_HERE
		bio_set_flag(bio, BIO_SEND_SELF);
#endif /* MY_ABC_HERE */
#ifdef MY_ABC_HERE
		bio_set_flag(bio, BIO_DELAYED);
#endif /* MY_ABC_HERE */
		generic_make_request(bio);
		bio = split;
	}

	orig_sector = sector;
	zone = find_zone(mddev->private, &sector);
	switch (conf->layout) {
	case RAID0_ORIG_LAYOUT:
		tmp_dev = map_sector(mddev, zone, orig_sector, &sector);
		break;
	case RAID0_ALT_MULTIZONE_LAYOUT:
		tmp_dev = map_sector(mddev, zone, sector, &sector);
		break;
	default:
		WARN(1, "md/raid0:%s: Invalid layout\n", mdname(mddev));
		bio_io_error(bio);
		return;
	}
#ifdef MY_ABC_HERE
	cloned_bio = bio_clone_mddev(bio, GFP_NOIO, mddev);
	if (cloned_bio) {
		cloned_bio->bi_end_io = Raid0EndRequest;
		cloned_bio->bi_private = bio;
		atomic_inc(&tmp_dev->nr_pending);

		orig_bio = bio;
		orig_bio->bi_next = (void *)tmp_dev;
		bio = cloned_bio;
	}
#endif /* MY_ABC_HERE */

	bio->bi_bdev = tmp_dev->bdev;
	bio->bi_iter.bi_sector = sector + zone->dev_start +
		tmp_dev->data_offset;

	if (unlikely((bio->bi_rw & REQ_DISCARD) &&
				!blk_queue_discard(bdev_get_queue(bio->bi_bdev)))) {
		/* Just ignore it */
#ifdef MY_ABC_HERE
		if (cloned_bio) {
			atomic_dec(&tmp_dev->nr_pending);
			orig_bio->bi_next = bio->bi_next;
			bio_put(bio);
			bio = orig_bio;
		}
#endif /* MY_ABC_HERE */
		bio_endio(bio);
	} else {
		if (mddev->gendisk)
			trace_block_bio_remap(bdev_get_queue(bio->bi_bdev),
					      bio, disk_devt(mddev->gendisk),
					      bio_sector);
		generic_make_request(bio);
	}
}

#ifdef MY_ABC_HERE
	static void
syno_raid0_status(struct seq_file *seq, struct mddev *mddev)
{
	int k;
	struct r0conf *conf = mddev->private;
	struct md_rdev *rdev = NULL;

	seq_printf(seq, " %dk chunks", mddev->chunk_sectors/2);
	seq_printf(seq, " [%d/%d] [", mddev->raid_disks, mddev->raid_disks - mddev->degraded);
	for (k = 0; k < conf->strip_zone[0].nb_dev; k++) {
		rdev = conf->devlist[k];
		if(rdev) {
#ifdef MY_ABC_HERE
			seq_printf (seq, "%s", 
						test_bit(In_sync, &rdev->flags) ? 
						(test_bit(DiskError, &rdev->flags) ? "E" : "U") : "_");
#else /* MY_ABC_HERE */
			seq_printf (seq, "%s", "U");
#endif /* MY_ABC_HERE */
		} else {
			seq_printf (seq, "%s", "_");
		}
	}
	seq_printf (seq, "]");
}
#else /* MY_ABC_HERE */
static void raid0_status(struct seq_file *seq, struct mddev *mddev)
{
	seq_printf(seq, " %dk chunks", mddev->chunk_sectors / 2);
	return;
}
#endif /* MY_ABC_HERE */

#ifdef MY_ABC_HERE
int SynoRaid0RemoveDisk(struct mddev *mddev, struct md_rdev *rdev)
{
	int err = 0;
	char nm[20];
	struct r0conf *conf = mddev->private;

	if (!rdev) {
		goto END;
	}

	if (atomic_read(&rdev->nr_pending)) {
		/* lost the race, try later */
		err = -EBUSY;
		goto END;
	}

	/**
	 * raid0 don't has their own thread, we just remove it's sysfs
	 * when there has no other pending request
	 */
	sprintf(nm,"rd%d", rdev->raid_disk);
	sysfs_remove_link(&mddev->kobj, nm);
	conf->devlist[rdev->raid_disk] = NULL;
	rdev->raid_disk = -1;
END:
	return err;
}

/**
 * This is our implement for raid handler.
 * It mainly for handling device hotplug.
 * We let it look like other raid type.
 * Set it faulty could let SDK know it's status
 *
 * @author \$Author: khchen $
 * @version \$Revision: 1.1  *
 *
 * @param mddev  Should not be NULL. passing from md.c
 * @param rdev   Should not be NULL. passing from md.c
 */
static void SynoRaid0Error(struct mddev *mddev, struct md_rdev *rdev)
{
	char b[BDEVNAME_SIZE];
	printk(KERN_ALERT
		"md/raid:%s: Disk failure on %s, disabling device.\n",
		mdname(mddev), bdevname(rdev->bdev, b));
	if (test_and_clear_bit(In_sync, &rdev->flags)) {
		if (mddev->degraded < mddev->raid_disks) {
			SYNO_UPDATE_SB_WORK *update_sb = NULL;
			mddev->degraded++;
#ifdef MY_ABC_HERE
			if (MD_CRASHED_ASSEMBLE != mddev->nodev_and_crashed) {
				mddev->nodev_and_crashed = MD_CRASHED;
			}
#endif /* MY_ABC_HERE */
			set_bit(Faulty, &rdev->flags);
#ifdef MY_ABC_HERE
			clear_bit(DiskError, &rdev->flags);
#endif /* MY_ABC_HERE */
			set_bit(MD_CHANGE_DEVS, &mddev->flags);

			if (NULL == (update_sb = kzalloc(sizeof(SYNO_UPDATE_SB_WORK), GFP_ATOMIC))) {
				WARN_ON(!update_sb);
				goto END;
			}

			INIT_WORK(&update_sb->work, SynoUpdateSBTask);
			update_sb->mddev = mddev;
			schedule_work(&update_sb->work);
		}
	} else {
		set_bit(Faulty, &rdev->flags);
	}
END:
	return;
}

/**
 * This is our implement for raid handler.
 * It mainly for mdadm set device faulty. We let it look like
 * other raid type. Let it become read only (scemd would remount
 * if it find DiskError)
 *
 * @author \$Author: khchen $
 * @version \$Revision: 1.1  *
 *
 * @param mddev  Should not be NULL. passing from md.c
 * @param rdev   Should not be NULL. passing from md.c
 */
static void SynoRaid0ErrorInternal(struct mddev *mddev, struct md_rdev *rdev)
{
	char b[BDEVNAME_SIZE];
#ifdef MY_ABC_HERE
	printk_ratelimited(KERN_ALERT
#else /* MY_ABC_HERE */
	printk(KERN_ALERT
#endif /* MY_ABC_HERE */
		"md/raid:%s: Disk failure on %s, disabling device.\n",
		mdname(mddev), bdevname(rdev->bdev, b));
#ifdef MY_ABC_HERE
	if (!test_bit(DiskError, &rdev->flags)) {
		SYNO_UPDATE_SB_WORK *update_sb = NULL;

		set_bit(DiskError, &rdev->flags);
		if (NULL == (update_sb = kzalloc(sizeof(SYNO_UPDATE_SB_WORK), GFP_ATOMIC))) {
			WARN_ON(!update_sb);
			goto END;
		}

		INIT_WORK(&update_sb->work, SynoUpdateSBTask);
		update_sb->mddev = mddev;
		schedule_work(&update_sb->work);
		set_bit(MD_CHANGE_DEVS, &mddev->flags);
	}

END:
#endif /* MY_ABC_HERE */
	return;
}
#endif /* MY_ABC_HERE */

static void *raid0_takeover_raid45(struct mddev *mddev)
{
	struct md_rdev *rdev;
	struct r0conf *priv_conf;

	if (mddev->degraded != 1) {
		printk(KERN_ERR "md/raid0:%s: raid5 must be degraded! Degraded disks: %d\n",
		       mdname(mddev),
		       mddev->degraded);
		return ERR_PTR(-EINVAL);
	}

	rdev_for_each(rdev, mddev) {
		/* check slot number for a disk */
		if (rdev->raid_disk == mddev->raid_disks-1) {
			printk(KERN_ERR "md/raid0:%s: raid5 must have missing parity disk!\n",
			       mdname(mddev));
			return ERR_PTR(-EINVAL);
		}
		rdev->sectors = mddev->dev_sectors;
	}

	/* Set new parameters */
	mddev->new_level = 0;
	mddev->new_layout = 0;
	mddev->new_chunk_sectors = mddev->chunk_sectors;
	mddev->raid_disks--;
	mddev->delta_disks = -1;
	/* make sure it will be not marked as dirty */
	mddev->recovery_cp = MaxSector;

	create_strip_zones(mddev, &priv_conf);
	return priv_conf;
}

static void *raid0_takeover_raid10(struct mddev *mddev)
{
	struct r0conf *priv_conf;

	/* Check layout:
	 *  - far_copies must be 1
	 *  - near_copies must be 2
	 *  - disks number must be even
	 *  - all mirrors must be already degraded
	 */
	if (mddev->layout != ((1 << 8) + 2)) {
		printk(KERN_ERR "md/raid0:%s:: Raid0 cannot takover layout: 0x%x\n",
		       mdname(mddev),
		       mddev->layout);
		return ERR_PTR(-EINVAL);
	}
	if (mddev->raid_disks & 1) {
		printk(KERN_ERR "md/raid0:%s: Raid0 cannot takover Raid10 with odd disk number.\n",
		       mdname(mddev));
		return ERR_PTR(-EINVAL);
	}
	if (mddev->degraded != (mddev->raid_disks>>1)) {
		printk(KERN_ERR "md/raid0:%s: All mirrors must be already degraded!\n",
		       mdname(mddev));
		return ERR_PTR(-EINVAL);
	}

	/* Set new parameters */
	mddev->new_level = 0;
	mddev->new_layout = 0;
	mddev->new_chunk_sectors = mddev->chunk_sectors;
	mddev->delta_disks = - mddev->raid_disks / 2;
	mddev->raid_disks += mddev->delta_disks;
	mddev->degraded = 0;
	/* make sure it will be not marked as dirty */
	mddev->recovery_cp = MaxSector;

	create_strip_zones(mddev, &priv_conf);
	return priv_conf;
}

static void *raid0_takeover_raid1(struct mddev *mddev)
{
	struct r0conf *priv_conf;
	int chunksect;

	/* Check layout:
	 *  - (N - 1) mirror drives must be already faulty
	 */
	if ((mddev->raid_disks - 1) != mddev->degraded) {
		printk(KERN_ERR "md/raid0:%s: (N - 1) mirrors drives must be already faulty!\n",
		       mdname(mddev));
		return ERR_PTR(-EINVAL);
	}

	/*
	 * a raid1 doesn't have the notion of chunk size, so
	 * figure out the largest suitable size we can use.
	 */
	chunksect = 64 * 2; /* 64K by default */

	/* The array must be an exact multiple of chunksize */
	while (chunksect && (mddev->array_sectors & (chunksect - 1)))
		chunksect >>= 1;

	if ((chunksect << 9) < PAGE_SIZE)
		/* array size does not allow a suitable chunk size */
		return ERR_PTR(-EINVAL);

	/* Set new parameters */
	mddev->new_level = 0;
	mddev->new_layout = 0;
	mddev->new_chunk_sectors = chunksect;
	mddev->chunk_sectors = chunksect;
	mddev->delta_disks = 1 - mddev->raid_disks;
	mddev->raid_disks = 1;
	/* make sure it will be not marked as dirty */
	mddev->recovery_cp = MaxSector;

	create_strip_zones(mddev, &priv_conf);
	return priv_conf;
}

static void *raid0_takeover(struct mddev *mddev)
{
	/* raid0 can take over:
	 *  raid4 - if all data disks are active.
	 *  raid5 - providing it is Raid4 layout and one disk is faulty
	 *  raid10 - assuming we have all necessary active disks
	 *  raid1 - with (N -1) mirror drives faulty
	 */

	if (mddev->bitmap) {
		printk(KERN_ERR "md/raid0: %s: cannot takeover array with bitmap\n",
		       mdname(mddev));
		return ERR_PTR(-EBUSY);
	}
	if (mddev->level == 4)
		return raid0_takeover_raid45(mddev);

	if (mddev->level == 5) {
		if (mddev->layout == ALGORITHM_PARITY_N)
			return raid0_takeover_raid45(mddev);

		printk(KERN_ERR "md/raid0:%s: Raid can only takeover Raid5 with layout: %d\n",
		       mdname(mddev), ALGORITHM_PARITY_N);
	}

	if (mddev->level == 10)
		return raid0_takeover_raid10(mddev);

	if (mddev->level == 1)
		return raid0_takeover_raid1(mddev);

	printk(KERN_ERR "Takeover from raid%i to raid0 not supported\n",
		mddev->level);

	return ERR_PTR(-EINVAL);
}

static void raid0_quiesce(struct mddev *mddev, int state)
{
}

static struct md_personality raid0_personality=
{
	.name		= "raid0",
	.level		= 0,
	.owner		= THIS_MODULE,
	.make_request	= raid0_make_request,
	.run		= raid0_run,
	.free		= raid0_free,
#ifdef MY_ABC_HERE
	.status		= syno_raid0_status,
#else /* MY_ABC_HERE */
	.status		= raid0_status,
#endif /* MY_ABC_HERE */
	.size		= raid0_size,
#ifdef MY_ABC_HERE
	.hot_remove_disk    = SynoRaid0RemoveDisk,
	.error_handler      = SynoRaid0ErrorInternal,
	.syno_error_handler = SynoRaid0Error,
#endif /* MY_ABC_HERE */
	.takeover	= raid0_takeover,
	.quiesce	= raid0_quiesce,
	.congested	= raid0_congested,
};

static int __init raid0_init (void)
{
	return register_md_personality (&raid0_personality);
}

static void raid0_exit (void)
{
	unregister_md_personality (&raid0_personality);
}

module_init(raid0_init);
module_exit(raid0_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RAID0 (striping) personality for MD");
MODULE_ALIAS("md-personality-2"); /* RAID0 */
MODULE_ALIAS("md-raid0");
MODULE_ALIAS("md-level-0");
