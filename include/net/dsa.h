#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * include/net/dsa.h - Driver for Distributed Switch Architecture switch chips
 * Copyright (c) 2008-2009 Marvell Semiconductor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __LINUX_NET_DSA_H
#define __LINUX_NET_DSA_H

#include <linux/if_ether.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/ethtool.h>

enum dsa_tag_protocol {
	DSA_TAG_PROTO_NONE = 0,
	DSA_TAG_PROTO_DSA,
	DSA_TAG_PROTO_TRAILER,
	DSA_TAG_PROTO_EDSA,
	DSA_TAG_PROTO_BRCM,
#if defined(MY_ABC_HERE)
	DSA_TAG_LAST,		/* MUST BE LAST */
#endif /* MY_ABC_HERE */
};

#define DSA_MAX_SWITCHES	4
#define DSA_MAX_PORTS		12

struct dsa_chip_data {
	/*
	 * How to access the switch configuration registers.
	 */
	struct device	*host_dev;
	int		sw_addr;

	/* set to size of eeprom if supported by the switch */
	int		eeprom_len;

	/* Device tree node pointer for this specific switch chip
	 * used during switch setup in case additional properties
	 * and resources needs to be used
	 */
	struct device_node *of_node;

	/*
	 * The names of the switch's ports.  Use "cpu" to
	 * designate the switch port that the cpu is connected to,
	 * "dsa" to indicate that this port is a DSA link to
	 * another switch, NULL to indicate the port is unused,
	 * or any other string to indicate this is a physical port.
	 */
	char		*port_names[DSA_MAX_PORTS];
	struct device_node *port_dn[DSA_MAX_PORTS];

#if defined(MY_ABC_HERE)
	/*
	 * An array of which element [a] indicates which port on this
	 * switch should be used to send packets to that are destined
	 * for switch a. Can be NULL if there is only one switch chip.
	 */
	s8		rtable[DSA_MAX_SWITCHES];
#else /* MY_ABC_HERE */
	/*
	 * An array (with nr_chips elements) of which element [a]
	 * indicates which port on this switch should be used to
	 * send packets to that are destined for switch a.  Can be
	 * NULL if there is only one switch chip.
	 */
	s8		*rtable;
#endif /* MY_ABC_HERE */
};

struct dsa_platform_data {
	/*
	 * Reference to a Linux network interface that connects
	 * to the root switch chip of the tree.
	 */
	struct device	*netdev;
	struct net_device *of_netdev;

	/*
	 * Info structs describing each of the switch chips
	 * connected via this network interface.
	 */
	int		nr_chips;
	struct dsa_chip_data	*chip;
};

struct packet_type;

struct dsa_switch_tree {
#if defined(MY_ABC_HERE)
	struct list_head	list;

	/* Tree identifier */
	u32 tree;

	/* Number of switches attached to this tree */
	struct kref refcount;

	/* Has this tree been applied to the hardware? */
	bool applied;
#endif /* MY_ABC_HERE */

	/*
	 * Configuration data for the platform device that owns
	 * this dsa switch tree instance.
	 */
	struct dsa_platform_data	*pd;

	/*
	 * Reference to network device to use, and which tagging
	 * protocol to use.
	 */
	struct net_device	*master_netdev;
	int			(*rcv)(struct sk_buff *skb,
				       struct net_device *dev,
				       struct packet_type *pt,
				       struct net_device *orig_dev);

#if defined(MY_ABC_HERE)
	/*
	 * Original copy of the master netdev ethtool_ops
	 */
	struct ethtool_ops	master_ethtool_ops;
	const struct ethtool_ops *master_orig_ethtool_ops;
#else /* MY_ABC_HERE */
	enum dsa_tag_protocol	tag_protocol;
#endif /* MY_ABC_HERE */

	/*
	 * The switch and port to which the CPU is attached.
	 */
	s8			cpu_switch;
	s8			cpu_port;

#if defined(MY_ABC_HERE)
	/*
	 * Data for the individual switch chips.
	 */
	struct dsa_switch	*ds[DSA_MAX_SWITCHES];

	/*
	 * Tagging protocol operations for adding and removing an
	 * encapsulation tag.
	 */
	const struct dsa_device_ops *tag_ops;
#else /* MY_ABC_HERE */
	/*
	 * Link state polling.
	 */
	int			link_poll_needed;
	struct work_struct	link_poll_work;
	struct timer_list	link_poll_timer;

	/*
	 * Data for the individual switch chips.
	 */
	struct dsa_switch	*ds[DSA_MAX_SWITCHES];
#endif /* MY_ABC_HERE */
};

#if defined(MY_ABC_HERE)
struct dsa_port {
	struct net_device	*netdev;
	struct device_node	*dn;
};
#endif /* MY_ABC_HERE */

struct dsa_switch {
#if defined(MY_ABC_HERE)
	struct device *dev;
#endif /* MY_ABC_HERE */

	/*
	 * Parent switch tree, and switch index.
	 */
	struct dsa_switch_tree	*dst;
	int			index;

#if defined(MY_ABC_HERE)
	/*
	 * Give the switch driver somewhere to hang its private data
	 * structure.
	 */
	void *priv;
#else /* MY_ABC_HERE */
	/*
	 * Tagging protocol understood by this switch
	 */
	enum dsa_tag_protocol	tag_protocol;
#endif /* MY_ABC_HERE */

	/*
	 * Configuration data for this switch.
	 */
#if defined(MY_ABC_HERE)
	struct dsa_chip_data	*cd;
#else /* MY_ABC_HERE */
	struct dsa_chip_data	*pd;
#endif /* MY_ABC_HERE */

	/*
	 * The used switch driver.
	 */
	struct dsa_switch_driver	*drv;

#if defined(MY_ABC_HERE)
	/*
	 * An array of which element [a] indicates which port on this
	 * switch should be used to send packets to that are destined
	 * for switch a. Can be NULL if there is only one switch chip.
	 */
	s8		rtable[DSA_MAX_SWITCHES];
#else /* MY_ABC_HERE */
	/*
	 * Reference to host device to use.
	 */
	struct device		*master_dev;
#endif /* MY_ABC_HERE */

#ifdef CONFIG_NET_DSA_HWMON
	/*
	 * Hardware monitoring information
	 */
	char			hwmon_name[IFNAMSIZ + 8];
	struct device		*hwmon_dev;
#endif

#if defined(MY_ABC_HERE)
	/*
	 * The lower device this switch uses to talk to the host
	 */
	struct net_device *master_netdev;
#endif /* MY_ABC_HERE */

	/*
	 * Slave mii_bus and devices for the individual ports.
	 */
	u32			dsa_port_mask;
#if defined(MY_ABC_HERE)
	u32			cpu_port_mask;
	u32			enabled_port_mask;
	u32			phys_mii_mask;
	struct dsa_port		ports[DSA_MAX_PORTS];
	struct mii_bus		*slave_mii_bus;
#else /* MY_ABC_HERE */
	u32			phys_port_mask;
	u32			phys_mii_mask;
	struct mii_bus		*slave_mii_bus;
	struct net_device	*ports[DSA_MAX_PORTS];
#endif /* MY_ABC_HERE */
};

static inline bool dsa_is_cpu_port(struct dsa_switch *ds, int p)
{
	return !!(ds->index == ds->dst->cpu_switch && p == ds->dst->cpu_port);
}

static inline bool dsa_is_dsa_port(struct dsa_switch *ds, int p)
{
	return !!((ds->dsa_port_mask) & (1 << p));
}

static inline bool dsa_is_port_initialized(struct dsa_switch *ds, int p)
{
#if defined(MY_ABC_HERE)
	return ds->enabled_port_mask & (1 << p) && ds->ports[p].netdev;
#else /* MY_ABC_HERE */
	return ds->phys_port_mask & (1 << p) && ds->ports[p];
#endif /* MY_ABC_HERE */
}

static inline u8 dsa_upstream_port(struct dsa_switch *ds)
{
	struct dsa_switch_tree *dst = ds->dst;

	/*
	 * If this is the root switch (i.e. the switch that connects
	 * to the CPU), return the cpu port number on this switch.
	 * Else return the (DSA) port number that connects to the
	 * switch that is one hop closer to the cpu.
	 */
	if (dst->cpu_switch == ds->index)
		return dst->cpu_port;
	else
#if defined(MY_ABC_HERE)
		return ds->rtable[dst->cpu_switch];
#else /* MY_ABC_HERE */
		return ds->pd->rtable[dst->cpu_switch];
#endif /* MY_ABC_HERE */
}

struct switchdev_trans;
struct switchdev_obj;
struct switchdev_obj_port_fdb;
struct switchdev_obj_port_vlan;

struct dsa_switch_driver {
	struct list_head	list;

	enum dsa_tag_protocol	tag_protocol;
#if defined(MY_ABC_HERE)
//do nothing
#else /* MY_ABC_HERE */
	int			priv_size;
#endif /* MY_ABC_HERE */

	/*
	 * Probing and setup.
	 */
#if defined(MY_ABC_HERE)
	const char	*(*probe)(struct device *dsa_dev,
				  struct device *host_dev, int sw_addr,
				  void **priv);
#else /* MY_ABC_HERE */
	char	*(*probe)(struct device *host_dev, int sw_addr);
#endif /* MY_ABC_HERE */
	int	(*setup)(struct dsa_switch *ds);
	int	(*set_addr)(struct dsa_switch *ds, u8 *addr);
	u32	(*get_phy_flags)(struct dsa_switch *ds, int port);

	/*
	 * Access to the switch's PHY registers.
	 */
	int	(*phy_read)(struct dsa_switch *ds, int port, int regnum);
	int	(*phy_write)(struct dsa_switch *ds, int port,
			     int regnum, u16 val);

#if defined(MY_ABC_HERE)
#else /* MY_ABC_HERE */
//do nothing
	/*
	 * Link state polling and IRQ handling.
	 */
	void	(*poll_link)(struct dsa_switch *ds);
#endif /* MY_ABC_HERE */

	/*
	 * Link state adjustment (called from libphy)
	 */
	void	(*adjust_link)(struct dsa_switch *ds, int port,
				struct phy_device *phydev);
	void	(*fixed_link_update)(struct dsa_switch *ds, int port,
				struct fixed_phy_status *st);

	/*
	 * ethtool hardware statistics.
	 */
	void	(*get_strings)(struct dsa_switch *ds, int port, uint8_t *data);
	void	(*get_ethtool_stats)(struct dsa_switch *ds,
				     int port, uint64_t *data);
	int	(*get_sset_count)(struct dsa_switch *ds);

	/*
	 * ethtool Wake-on-LAN
	 */
	void	(*get_wol)(struct dsa_switch *ds, int port,
			   struct ethtool_wolinfo *w);
	int	(*set_wol)(struct dsa_switch *ds, int port,
			   struct ethtool_wolinfo *w);

	/*
	 * Suspend and resume
	 */
	int	(*suspend)(struct dsa_switch *ds);
	int	(*resume)(struct dsa_switch *ds);

	/*
	 * Port enable/disable
	 */
	int	(*port_enable)(struct dsa_switch *ds, int port,
			       struct phy_device *phy);
	void	(*port_disable)(struct dsa_switch *ds, int port,
				struct phy_device *phy);

	/*
	 * EEE setttings
	 */
	int	(*set_eee)(struct dsa_switch *ds, int port,
			   struct phy_device *phydev,
			   struct ethtool_eee *e);
	int	(*get_eee)(struct dsa_switch *ds, int port,
			   struct ethtool_eee *e);

#ifdef CONFIG_NET_DSA_HWMON
	/* Hardware monitoring */
	int	(*get_temp)(struct dsa_switch *ds, int *temp);
	int	(*get_temp_limit)(struct dsa_switch *ds, int *temp);
	int	(*set_temp_limit)(struct dsa_switch *ds, int temp);
	int	(*get_temp_alarm)(struct dsa_switch *ds, bool *alarm);
#endif

	/* EEPROM access */
	int	(*get_eeprom_len)(struct dsa_switch *ds);
	int	(*get_eeprom)(struct dsa_switch *ds,
			      struct ethtool_eeprom *eeprom, u8 *data);
	int	(*set_eeprom)(struct dsa_switch *ds,
			      struct ethtool_eeprom *eeprom, u8 *data);

	/*
	 * Register access.
	 */
	int	(*get_regs_len)(struct dsa_switch *ds, int port);
	void	(*get_regs)(struct dsa_switch *ds, int port,
			    struct ethtool_regs *regs, void *p);

	/*
	 * Bridge integration
	 */
#if defined(MY_ABC_HERE)
	int	(*port_bridge_join)(struct dsa_switch *ds, int port,
				    struct net_device *bridge);
	void	(*port_bridge_leave)(struct dsa_switch *ds, int port);
	void	(*port_stp_state_set)(struct dsa_switch *ds, int port,
				      u8 state);
#else /* MY_ABC_HERE */
	int	(*port_join_bridge)(struct dsa_switch *ds, int port,
				    u32 br_port_mask);
	int	(*port_leave_bridge)(struct dsa_switch *ds, int port,
				     u32 br_port_mask);
	int	(*port_stp_update)(struct dsa_switch *ds, int port,
				   u8 state);
#endif /* MY_ABC_HERE */

	/*
	 * VLAN support
	 */
#if defined(MY_ABC_HERE)
	int	(*port_vlan_filtering)(struct dsa_switch *ds, int port,
				       bool vlan_filtering);
#endif /* MY_ABC_HERE */
	int	(*port_vlan_prepare)(struct dsa_switch *ds, int port,
				     const struct switchdev_obj_port_vlan *vlan,
				     struct switchdev_trans *trans);
#if defined(MY_ABC_HERE)
	void	(*port_vlan_add)(struct dsa_switch *ds, int port,
				 const struct switchdev_obj_port_vlan *vlan,
				 struct switchdev_trans *trans);
	int	(*port_vlan_del)(struct dsa_switch *ds, int port,
				 const struct switchdev_obj_port_vlan *vlan);
	int	(*port_vlan_dump)(struct dsa_switch *ds, int port,
				  struct switchdev_obj_port_vlan *vlan,
				  int (*cb)(struct switchdev_obj *obj));
#else /* MY_ABC_HERE */
	int	(*port_vlan_add)(struct dsa_switch *ds, int port,
				 const struct switchdev_obj_port_vlan *vlan,
				 struct switchdev_trans *trans);
	int	(*port_vlan_del)(struct dsa_switch *ds, int port,
				 const struct switchdev_obj_port_vlan *vlan);
	int	(*port_pvid_get)(struct dsa_switch *ds, int port, u16 *pvid);
	int	(*vlan_getnext)(struct dsa_switch *ds, u16 *vid,
				unsigned long *ports, unsigned long *untagged);
#endif /* MY_ABC_HERE */

	/*
	 * Forwarding database
	 */
	int	(*port_fdb_prepare)(struct dsa_switch *ds, int port,
				    const struct switchdev_obj_port_fdb *fdb,
				    struct switchdev_trans *trans);
#if defined(MY_ABC_HERE)
	void	(*port_fdb_add)(struct dsa_switch *ds, int port,
				const struct switchdev_obj_port_fdb *fdb,
				struct switchdev_trans *trans);
#else /* MY_ABC_HERE */
	int	(*port_fdb_add)(struct dsa_switch *ds, int port,
				const struct switchdev_obj_port_fdb *fdb,
				struct switchdev_trans *trans);
#endif /* MY_ABC_HERE */
	int	(*port_fdb_del)(struct dsa_switch *ds, int port,
				const struct switchdev_obj_port_fdb *fdb);
	int	(*port_fdb_dump)(struct dsa_switch *ds, int port,
				 struct switchdev_obj_port_fdb *fdb,
				 int (*cb)(struct switchdev_obj *obj));
};

void register_switch_driver(struct dsa_switch_driver *type);
void unregister_switch_driver(struct dsa_switch_driver *type);
struct mii_bus *dsa_host_dev_to_mii_bus(struct device *dev);

static inline void *ds_to_priv(struct dsa_switch *ds)
{
#if defined(MY_ABC_HERE)
	return ds->priv;
#else /* MY_ABC_HERE */
	return (void *)(ds + 1);
#endif /* MY_ABC_HERE */
}

static inline bool dsa_uses_tagged_protocol(struct dsa_switch_tree *dst)
{
	return dst->rcv != NULL;
}

#if defined(MY_ABC_HERE)
void dsa_unregister_switch(struct dsa_switch *ds);
int dsa_register_switch(struct dsa_switch *ds, struct device_node *np);
#endif /* MY_ABC_HERE */
#endif
