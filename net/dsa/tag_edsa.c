#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * net/dsa/tag_edsa.c - Ethertype DSA tagging
 * Copyright (c) 2008-2009 Marvell Semiconductor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/etherdevice.h>
#include <linux/list.h>
#include <linux/slab.h>
#include "dsa_priv.h"

#define DSA_HLEN	4
#define EDSA_HLEN	8

static struct sk_buff *edsa_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct dsa_slave_priv *p = netdev_priv(dev);
	u8 *edsa_header;

	/*
	 * Convert the outermost 802.1q tag to a DSA tag and prepend
	 * a DSA ethertype field is the packet is tagged, or insert
	 * a DSA ethertype plus DSA tag between the addresses and the
	 * current ethertype field if the packet is untagged.
	 */
	if (skb->protocol == htons(ETH_P_8021Q)) {
		if (skb_cow_head(skb, DSA_HLEN) < 0)
			goto out_free;
		skb_push(skb, DSA_HLEN);

		memmove(skb->data, skb->data + DSA_HLEN, 2 * ETH_ALEN);

		/*
		 * Construct tagged FROM_CPU DSA tag from 802.1q tag.
		 */
		edsa_header = skb->data + 2 * ETH_ALEN;
		edsa_header[0] = (ETH_P_EDSA >> 8) & 0xff;
		edsa_header[1] = ETH_P_EDSA & 0xff;
		edsa_header[2] = 0x00;
		edsa_header[3] = 0x00;
		edsa_header[4] = 0x60 | p->parent->index;
		edsa_header[5] = p->port << 3;

		/*
		 * Move CFI field from byte 6 to byte 5.
		 */
		if (edsa_header[6] & 0x10) {
			edsa_header[5] |= 0x01;
			edsa_header[6] &= ~0x10;
		}
	} else {
		if (skb_cow_head(skb, EDSA_HLEN) < 0)
			goto out_free;
		skb_push(skb, EDSA_HLEN);

		memmove(skb->data, skb->data + EDSA_HLEN, 2 * ETH_ALEN);

		/*
		 * Construct untagged FROM_CPU DSA tag.
		 */
		edsa_header = skb->data + 2 * ETH_ALEN;
		edsa_header[0] = (ETH_P_EDSA >> 8) & 0xff;
		edsa_header[1] = ETH_P_EDSA & 0xff;
		edsa_header[2] = 0x00;
		edsa_header[3] = 0x00;
		edsa_header[4] = 0x40 | p->parent->index;
		edsa_header[5] = p->port << 3;
		edsa_header[6] = 0x00;
		edsa_header[7] = 0x00;
	}

	return skb;

out_free:
	kfree_skb(skb);
	return NULL;
}

static int edsa_rcv(struct sk_buff *skb, struct net_device *dev,
		    struct packet_type *pt, struct net_device *orig_dev)
{
	struct dsa_switch_tree *dst = dev->dsa_ptr;
	struct dsa_switch *ds;
	u8 *edsa_header;
	int source_device;
	int source_port;

	if (unlikely(dst == NULL))
		goto out_drop;

	skb = skb_unshare(skb, GFP_ATOMIC);
	if (skb == NULL)
		goto out;

	if (unlikely(!pskb_may_pull(skb, EDSA_HLEN)))
		goto out_drop;

	/*
	 * Skip the two null bytes after the ethertype.
	 */
	edsa_header = skb->data + 2;

	/*
	 * Check that frame type is either TO_CPU or FORWARD.
	 */
	if ((edsa_header[0] & 0xc0) != 0x00 && (edsa_header[0] & 0xc0) != 0xc0)
		goto out_drop;

	/*
	 * Determine source device and port.
	 */
	source_device = edsa_header[0] & 0x1f;
	source_port = (edsa_header[1] >> 3) & 0x1f;

	/*
	 * Check that the source device exists and that the source
	 * port is a registered DSA port.
	 */
#if defined(MY_ABC_HERE)
	if (source_device >= DSA_MAX_SWITCHES)
		goto out_drop;
#else /* MY_ABC_HERE */
	if (source_device >= dst->pd->nr_chips)
		goto out_drop;
#endif /* MY_ABC_HERE */

	ds = dst->ds[source_device];
#if defined(MY_ABC_HERE)
	if (!ds)
		goto out_drop;

	if (source_port >= DSA_MAX_PORTS || !ds->ports[source_port].netdev)
		goto out_drop;
#else /* MY_ABC_HERE */
	if (source_port >= DSA_MAX_PORTS || ds->ports[source_port] == NULL)
		goto out_drop;
#endif /* MY_ABC_HERE */

	/*
	 * If the 'tagged' bit is set, convert the DSA tag to a 802.1q
	 * tag and delete the ethertype part.  If the 'tagged' bit is
	 * clear, delete the ethertype and the DSA tag parts.
	 */
	if (edsa_header[0] & 0x20) {
		u8 new_header[4];

		/*
		 * Insert 802.1q ethertype and copy the VLAN-related
		 * fields, but clear the bit that will hold CFI (since
		 * DSA uses that bit location for another purpose).
		 */
		new_header[0] = (ETH_P_8021Q >> 8) & 0xff;
		new_header[1] = ETH_P_8021Q & 0xff;
		new_header[2] = edsa_header[2] & ~0x10;
		new_header[3] = edsa_header[3];

		/*
		 * Move CFI bit from its place in the DSA header to
		 * its 802.1q-designated place.
		 */
		if (edsa_header[1] & 0x01)
			new_header[2] |= 0x10;

		skb_pull_rcsum(skb, DSA_HLEN);

		/*
		 * Update packet checksum if skb is CHECKSUM_COMPLETE.
		 */
		if (skb->ip_summed == CHECKSUM_COMPLETE) {
			__wsum c = skb->csum;
			c = csum_add(c, csum_partial(new_header + 2, 2, 0));
			c = csum_sub(c, csum_partial(edsa_header + 2, 2, 0));
			skb->csum = c;
		}

		memcpy(edsa_header, new_header, DSA_HLEN);

		memmove(skb->data - ETH_HLEN,
			skb->data - ETH_HLEN - DSA_HLEN,
			2 * ETH_ALEN);
	} else {
		/*
		 * Remove DSA tag and update checksum.
		 */
		skb_pull_rcsum(skb, EDSA_HLEN);
		memmove(skb->data - ETH_HLEN,
			skb->data - ETH_HLEN - EDSA_HLEN,
			2 * ETH_ALEN);
	}

#if defined(MY_ABC_HERE)
	skb->dev = ds->ports[source_port].netdev;
#else /* MY_ABC_HERE */
	skb->dev = ds->ports[source_port];
#endif /* MY_ABC_HERE */
	skb_push(skb, ETH_HLEN);
	skb->pkt_type = PACKET_HOST;
	skb->protocol = eth_type_trans(skb, skb->dev);

	skb->dev->stats.rx_packets++;
	skb->dev->stats.rx_bytes += skb->len;

	netif_receive_skb(skb);

	return 0;

out_drop:
	kfree_skb(skb);
out:
	return 0;
}

const struct dsa_device_ops edsa_netdev_ops = {
	.xmit	= edsa_xmit,
	.rcv	= edsa_rcv,
};
