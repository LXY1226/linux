#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * OF helpers for the MDIO (Ethernet PHY) API
 *
 * Copyright (c) 2009 Secret Lab Technologies, Ltd.
 *
 * This file is released under the GPLv2
 *
 * This file provides helper functions for extracting PHY device information
 * out of the OpenFirmware device tree and using it to populate an mii_bus.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/err.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/module.h>

MODULE_AUTHOR("Grant Likely <grant.likely@secretlab.ca>");
MODULE_LICENSE("GPL");

/* Extract the clause 22 phy ID from the compatible string of the form
 * ethernet-phy-idAAAA.BBBB */
static int of_get_phy_id(struct device_node *device, u32 *phy_id)
{
	struct property *prop;
	const char *cp;
	unsigned int upper, lower;

	of_property_for_each_string(device, "compatible", prop, cp) {
		if (sscanf(cp, "ethernet-phy-id%4x.%4x", &upper, &lower) == 2) {
			*phy_id = ((upper & 0xFFFF) << 16) | (lower & 0xFFFF);
			return 0;
		}
	}
	return -EINVAL;
}

static int of_mdiobus_register_phy(struct mii_bus *mdio, struct device_node *child,
				   u32 addr)
{
	struct phy_device *phy;
	bool is_c45;
	int rc;
	u32 phy_id;

	is_c45 = of_device_is_compatible(child,
					 "ethernet-phy-ieee802.3-c45");

	if (!is_c45 && !of_get_phy_id(child, &phy_id))
		phy = phy_device_create(mdio, addr, phy_id, 0, NULL);
	else
		phy = get_phy_device(mdio, addr, is_c45);
	if (!phy || IS_ERR(phy))
		return 1;

	rc = irq_of_parse_and_map(child, 0);
	if (rc > 0) {
		phy->irq = rc;
		mdio->irq[addr] = rc;
	} else {
		phy->irq = mdio->irq[addr];
	}

	if (of_property_read_bool(child, "broken-turn-around"))
		mdio->phy_ignore_ta_mask |= 1 << addr;

	/* Associate the OF node with the device structure so it
	 * can be looked up later */
	of_node_get(child);
#if defined(MY_ABC_HERE) || defined(MY_DEF_HERE)
	phy->mdio.dev.of_node = child;
#else /* MY_ABC_HERE || MY_DEF_HERE */
	phy->dev.of_node = child;
#endif /* MY_ABC_HERE || MY_DEF_HERE */

	/* All data is now stored in the phy struct;
	 * register it */
	rc = phy_device_register(phy);
	if (rc) {
		phy_device_free(phy);
		of_node_put(child);
		return 1;
	}

	dev_dbg(&mdio->dev, "registered phy %s at address %i\n",
		child->name, addr);

	return 0;
}

#if defined(MY_ABC_HERE) || defined(MY_DEF_HERE)
static int of_mdiobus_register_device(struct mii_bus *mdio,
				      struct device_node *child,
				      u32 addr)
{
	struct mdio_device *mdiodev;
	int rc;

	mdiodev = mdio_device_create(mdio, addr);
	if (!mdiodev || IS_ERR(mdiodev))
		return 1;

	/* Associate the OF node with the device structure so it
	 * can be looked up later.
	 */
	of_node_get(child);
	mdiodev->dev.of_node = child;

	/* All data is now stored in the mdiodev struct; register it. */
	rc = mdio_device_register(mdiodev);
	if (rc) {
		mdio_device_free(mdiodev);
		of_node_put(child);
		return 1;
	}

	dev_dbg(&mdio->dev, "registered mdio device %s at address %i\n",
		child->name, addr);

	return 0;
}
#endif /* MY_ABC_HERE || MY_DEF_HERE */

int of_mdio_parse_addr(struct device *dev, const struct device_node *np)
{
	u32 addr;
	int ret;

	ret = of_property_read_u32(np, "reg", &addr);
	if (ret < 0) {
		dev_err(dev, "%s has invalid PHY address\n", np->full_name);
		return ret;
	}

	/* A PHY must have a reg property in the range [0-31] */
	if (addr >= PHY_MAX_ADDR) {
		dev_err(dev, "%s PHY address %i is too large\n",
			np->full_name, addr);
		return -EINVAL;
	}

	return addr;
}
EXPORT_SYMBOL(of_mdio_parse_addr);

#if defined(MY_ABC_HERE) || defined(MY_DEF_HERE)
/*
 * Return true if the child node is for a phy. It must either:
 * o Compatible string of "ethernet-phy-idX.X"
 * o Compatible string of "ethernet-phy-ieee802.3-c45"
 * o Compatible string of "ethernet-phy-ieee802.3-c22"
 * o No compatibility string
 *
 * A device which is not a phy is expected to have a compatible string
 * indicating what sort of device it is.
 */
static bool of_mdiobus_child_is_phy(struct device_node *child)
{
	u32 phy_id;

	if (of_get_phy_id(child, &phy_id) != -EINVAL)
		return true;

	if (of_device_is_compatible(child, "ethernet-phy-ieee802.3-c45"))
		return true;

	if (of_device_is_compatible(child, "ethernet-phy-ieee802.3-c22"))
		return true;

	if (!of_find_property(child, "compatible", NULL))
		return true;

	return false;
}
#endif /* MY_ABC_HERE || MY_DEF_HERE */

/**
 * of_mdiobus_register - Register mii_bus and create PHYs from the device tree
 * @mdio: pointer to mii_bus structure
 * @np: pointer to device_node of MDIO bus.
 *
 * This function registers the mii_bus structure and registers a phy_device
 * for each child node of @np.
 */
int of_mdiobus_register(struct mii_bus *mdio, struct device_node *np)
{
	struct device_node *child;
	const __be32 *paddr;
	bool scanphys = false;
#if defined(MY_ABC_HERE) || defined(MY_DEF_HERE)
	int addr, rc;
#else /* MY_ABC_HERE || MY_DEF_HERE */
	int addr, rc, i;
#endif /* MY_ABC_HERE || MY_DEF_HERE */

	/* Mask out all PHYs from auto probing.  Instead the PHYs listed in
	 * the device tree are populated after the bus has been registered */
	mdio->phy_mask = ~0;

#if defined(MY_ABC_HERE) || defined(MY_DEF_HERE)
//do nothing
#else /* MY_ABC_HERE || MY_DEF_HERE */
	/* Clear all the IRQ properties */
	if (mdio->irq)
		for (i=0; i<PHY_MAX_ADDR; i++)
			mdio->irq[i] = PHY_POLL;
#endif /* MY_ABC_HERE || MY_DEF_HERE */

	mdio->dev.of_node = np;

	/* Register the MDIO bus */
	rc = mdiobus_register(mdio);
	if (rc)
		return rc;

#if defined(MY_ABC_HERE) || defined(MY_DEF_HERE)
	/* Loop over the child nodes and register a phy_device for each phy */
#else /* MY_ABC_HERE || MY_DEF_HERE */
	/* Loop over the child nodes and register a phy_device for each one */
#endif /* MY_ABC_HERE || MY_DEF_HERE */
	for_each_available_child_of_node(np, child) {
		addr = of_mdio_parse_addr(&mdio->dev, child);
		if (addr < 0) {
			scanphys = true;
			continue;
		}

#if defined(MY_ABC_HERE) || defined(MY_DEF_HERE)
		if (of_mdiobus_child_is_phy(child))
			of_mdiobus_register_phy(mdio, child, addr);
		else
			of_mdiobus_register_device(mdio, child, addr);
#else /* MY_ABC_HERE || MY_DEF_HERE */
		rc = of_mdiobus_register_phy(mdio, child, addr);
		if (rc)
			continue;
#endif /* MY_ABC_HERE || MY_DEF_HERE */
	}

	if (!scanphys)
		return 0;

	/* auto scan for PHYs with empty reg property */
	for_each_available_child_of_node(np, child) {
		/* Skip PHYs with reg property set */
		paddr = of_get_property(child, "reg", NULL);
		if (paddr)
			continue;

		for (addr = 0; addr < PHY_MAX_ADDR; addr++) {
			/* skip already registered PHYs */
#if defined(MY_ABC_HERE) || defined(MY_DEF_HERE)
			if (mdiobus_is_registered_device(mdio, addr))
#else /* MY_ABC_HERE || MY_DEF_HERE */
			if (mdio->phy_map[addr])
#endif /* MY_ABC_HERE || MY_DEF_HERE */
				continue;

			/* be noisy to encourage people to set reg property */
			dev_info(&mdio->dev, "scan phy %s at address %i\n",
				 child->name, addr);

#if defined(MY_ABC_HERE) || defined(MY_DEF_HERE)
			if (of_mdiobus_child_is_phy(child))
				of_mdiobus_register_phy(mdio, child, addr);
#else /* MY_ABC_HERE || MY_DEF_HERE */
			rc = of_mdiobus_register_phy(mdio, child, addr);
			if (rc)
				continue;
#endif /* MY_ABC_HERE || MY_DEF_HERE */
		}
	}

	return 0;
}
EXPORT_SYMBOL(of_mdiobus_register);

/* Helper function for of_phy_find_device */
static int of_phy_match(struct device *dev, void *phy_np)
{
	return dev->of_node == phy_np;
}

/**
 * of_phy_find_device - Give a PHY node, find the phy_device
 * @phy_np: Pointer to the phy's device tree node
 *
 * If successful, returns a pointer to the phy_device with the embedded
 * struct device refcount incremented by one, or NULL on failure.
 */
struct phy_device *of_phy_find_device(struct device_node *phy_np)
{
	struct device *d;
	if (!phy_np)
		return NULL;

	d = bus_find_device(&mdio_bus_type, NULL, phy_np, of_phy_match);
	return d ? to_phy_device(d) : NULL;
}
EXPORT_SYMBOL(of_phy_find_device);

/**
 * of_phy_connect - Connect to the phy described in the device tree
 * @dev: pointer to net_device claiming the phy
 * @phy_np: Pointer to device tree node for the PHY
 * @hndlr: Link state callback for the network device
 * @iface: PHY data interface type
 *
 * If successful, returns a pointer to the phy_device with the embedded
 * struct device refcount incremented by one, or NULL on failure. The
 * refcount must be dropped by calling phy_disconnect() or phy_detach().
 */
struct phy_device *of_phy_connect(struct net_device *dev,
				  struct device_node *phy_np,
				  void (*hndlr)(struct net_device *), u32 flags,
				  phy_interface_t iface)
{
	struct phy_device *phy = of_phy_find_device(phy_np);
	int ret;

	if (!phy)
		return NULL;

	phy->dev_flags = flags;

	ret = phy_connect_direct(dev, phy, hndlr, iface);

	/* refcount is held by phy_connect_direct() on success */
#if defined(MY_ABC_HERE) || defined(MY_DEF_HERE)
	put_device(&phy->mdio.dev);
#else /* MY_ABC_HERE || MY_DEF_HERE */
	put_device(&phy->dev);
#endif /* MY_ABC_HERE || MY_DEF_HERE */

	return ret ? NULL : phy;
}
EXPORT_SYMBOL(of_phy_connect);

/**
 * of_phy_attach - Attach to a PHY without starting the state machine
 * @dev: pointer to net_device claiming the phy
 * @phy_np: Node pointer for the PHY
 * @flags: flags to pass to the PHY
 * @iface: PHY data interface type
 *
 * If successful, returns a pointer to the phy_device with the embedded
 * struct device refcount incremented by one, or NULL on failure. The
 * refcount must be dropped by calling phy_disconnect() or phy_detach().
 */
struct phy_device *of_phy_attach(struct net_device *dev,
				 struct device_node *phy_np, u32 flags,
				 phy_interface_t iface)
{
	struct phy_device *phy = of_phy_find_device(phy_np);
	int ret;

	if (!phy)
		return NULL;

	ret = phy_attach_direct(dev, phy, flags, iface);

	/* refcount is held by phy_attach_direct() on success */
#if defined(MY_ABC_HERE) || defined(MY_DEF_HERE)
	put_device(&phy->mdio.dev);
#else /* MY_ABC_HERE || MY_DEF_HERE */
	put_device(&phy->dev);
#endif /* MY_ABC_HERE || MY_DEF_HERE */

	return ret ? NULL : phy;
}
EXPORT_SYMBOL(of_phy_attach);

#if defined(CONFIG_FIXED_PHY)
/*
 * of_phy_is_fixed_link() and of_phy_register_fixed_link() must
 * support two DT bindings:
 * - the old DT binding, where 'fixed-link' was a property with 5
 *   cells encoding various informations about the fixed PHY
 * - the new DT binding, where 'fixed-link' is a sub-node of the
 *   Ethernet device.
 */
bool of_phy_is_fixed_link(struct device_node *np)
{
	struct device_node *dn;
	int len, err;
	const char *managed;

	/* New binding */
	dn = of_get_child_by_name(np, "fixed-link");
	if (dn) {
		of_node_put(dn);
		return true;
	}

	err = of_property_read_string(np, "managed", &managed);
	if (err == 0 && strcmp(managed, "auto") != 0)
		return true;

	/* Old binding */
	if (of_get_property(np, "fixed-link", &len) &&
	    len == (5 * sizeof(__be32)))
		return true;

	return false;
}
EXPORT_SYMBOL(of_phy_is_fixed_link);

int of_phy_register_fixed_link(struct device_node *np)
{
	struct fixed_phy_status status = {};
	struct device_node *fixed_link_node;
	const __be32 *fixed_link_prop;
	int link_gpio;
	int len, err;
	struct phy_device *phy;
	const char *managed;

	err = of_property_read_string(np, "managed", &managed);
	if (err == 0) {
		if (strcmp(managed, "in-band-status") == 0) {
			/* status is zeroed, namely its .link member */
			phy = fixed_phy_register(PHY_POLL, &status, -1, np);
			return IS_ERR(phy) ? PTR_ERR(phy) : 0;
		}
	}

	/* New binding */
	fixed_link_node = of_get_child_by_name(np, "fixed-link");
	if (fixed_link_node) {
		status.link = 1;
		status.duplex = of_property_read_bool(fixed_link_node,
						      "full-duplex");
		if (of_property_read_u32(fixed_link_node, "speed",
					 &status.speed)) {
			of_node_put(fixed_link_node);
			return -EINVAL;
		}
		status.pause = of_property_read_bool(fixed_link_node, "pause");
		status.asym_pause = of_property_read_bool(fixed_link_node,
							  "asym-pause");
		link_gpio = of_get_named_gpio_flags(fixed_link_node,
						    "link-gpios", 0, NULL);
		of_node_put(fixed_link_node);
		if (link_gpio == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		phy = fixed_phy_register(PHY_POLL, &status, link_gpio, np);
		return IS_ERR(phy) ? PTR_ERR(phy) : 0;
	}

	/* Old binding */
	fixed_link_prop = of_get_property(np, "fixed-link", &len);
	if (fixed_link_prop && len == (5 * sizeof(__be32))) {
		status.link = 1;
		status.duplex = be32_to_cpu(fixed_link_prop[1]);
		status.speed = be32_to_cpu(fixed_link_prop[2]);
		status.pause = be32_to_cpu(fixed_link_prop[3]);
		status.asym_pause = be32_to_cpu(fixed_link_prop[4]);
		phy = fixed_phy_register(PHY_POLL, &status, -1, np);
		return IS_ERR(phy) ? PTR_ERR(phy) : 0;
	}

	return -ENODEV;
}
EXPORT_SYMBOL(of_phy_register_fixed_link);
#endif
