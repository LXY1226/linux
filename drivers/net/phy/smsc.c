#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * drivers/net/phy/smsc.c
 *
 * Driver for SMSC PHYs
 *
 * Author: Herbert Valerio Riedel
 *
 * Copyright (c) 2006 Herbert Valerio Riedel <hvr@gnu.org>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Support added for SMSC LAN8187 and LAN8700 by steve.glendinning@shawell.net
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/netdevice.h>
#include <linux/smscphy.h>

static int smsc_phy_config_intr(struct phy_device *phydev)
{
	int rc = phy_write (phydev, MII_LAN83C185_IM,
			((PHY_INTERRUPT_ENABLED == phydev->interrupts)
			? MII_LAN83C185_ISF_INT_PHYLIB_EVENTS
			: 0));

	return rc < 0 ? rc : 0;
}

static int smsc_phy_ack_interrupt(struct phy_device *phydev)
{
	int rc = phy_read (phydev, MII_LAN83C185_ISF);

	return rc < 0 ? rc : 0;
}

static int smsc_phy_config_init(struct phy_device *phydev)
{
	int __maybe_unused len;
#if defined(MY_ABC_HERE)
	struct device *dev __maybe_unused = &phydev->mdio.dev;
#else /* MY_ABC_HERE */
	struct device *dev __maybe_unused = &phydev->dev;
#endif /* MY_ABC_HERE */
	struct device_node *of_node __maybe_unused = dev->of_node;
	int rc = phy_read(phydev, MII_LAN83C185_CTRL_STATUS);
	int enable_energy = 1;

	if (rc < 0)
		return rc;

	if (of_find_property(of_node, "smsc,disable-energy-detect", &len))
		enable_energy = 0;

	if (enable_energy) {
		/* Enable energy detect mode for this SMSC Transceivers */
		rc = phy_write(phydev, MII_LAN83C185_CTRL_STATUS,
			       rc | MII_LAN83C185_EDPWRDOWN);
		if (rc < 0)
			return rc;
	}

	return smsc_phy_ack_interrupt(phydev);
}

static int smsc_phy_reset(struct phy_device *phydev)
{
	int rc = phy_read(phydev, MII_LAN83C185_SPECIAL_MODES);
	if (rc < 0)
		return rc;

	/* If the SMSC PHY is in power down mode, then set it
	 * in all capable mode before using it.
	 */
	if ((rc & MII_LAN83C185_MODE_MASK) == MII_LAN83C185_MODE_POWERDOWN) {
		int timeout = 50000;

		/* set "all capable" mode and reset the phy */
		rc |= MII_LAN83C185_MODE_ALL;
		phy_write(phydev, MII_LAN83C185_SPECIAL_MODES, rc);
		phy_write(phydev, MII_BMCR, BMCR_RESET);

		/* wait end of reset (max 500 ms) */
		do {
			udelay(10);
			if (timeout-- == 0)
				return -1;
			rc = phy_read(phydev, MII_BMCR);
		} while (rc & BMCR_RESET);
	}
	return 0;
}

static int lan911x_config_init(struct phy_device *phydev)
{
	return smsc_phy_ack_interrupt(phydev);
}

/*
 * The LAN87xx suffers from rare absence of the ENERGYON-bit when Ethernet cable
 * plugs in while LAN87xx is in Energy Detect Power-Down mode. This leads to
 * unstable detection of plugging in Ethernet cable.
 * This workaround disables Energy Detect Power-Down mode and waiting for
 * response on link pulses to detect presence of plugged Ethernet cable.
 * The Energy Detect Power-Down mode is enabled again in the end of procedure to
 * save approximately 220 mW of power if cable is unplugged.
 */
static int lan87xx_read_status(struct phy_device *phydev)
{
	int err = genphy_read_status(phydev);
	int i;

	if (!phydev->link) {
		/* Disable EDPD to wake up PHY */
		int rc = phy_read(phydev, MII_LAN83C185_CTRL_STATUS);
		if (rc < 0)
			return rc;

		rc = phy_write(phydev, MII_LAN83C185_CTRL_STATUS,
			       rc & ~MII_LAN83C185_EDPWRDOWN);
		if (rc < 0)
			return rc;

		/* Wait max 640 ms to detect energy */
		for (i = 0; i < 64; i++) {
			/* Sleep to allow link test pulses to be sent */
			msleep(10);
			rc = phy_read(phydev, MII_LAN83C185_CTRL_STATUS);
			if (rc < 0)
				return rc;
			if (rc & MII_LAN83C185_ENERGYON)
				break;
		}

		/* Re-enable EDPD */
		rc = phy_read(phydev, MII_LAN83C185_CTRL_STATUS);
		if (rc < 0)
			return rc;

		rc = phy_write(phydev, MII_LAN83C185_CTRL_STATUS,
			       rc | MII_LAN83C185_EDPWRDOWN);
		if (rc < 0)
			return rc;
	}

	return err;
}

static struct phy_driver smsc_phy_driver[] = {
{
	.phy_id		= 0x0007c0a0, /* OUI=0x00800f, Model#=0x0a */
	.phy_id_mask	= 0xfffffff0,
	.name		= "SMSC LAN83C185",

	.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause),
	.flags		= PHY_HAS_INTERRUPT | PHY_HAS_MAGICANEG,

	/* basic functions */
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.config_init	= smsc_phy_config_init,
	.soft_reset	= smsc_phy_reset,

	/* IRQ related */
	.ack_interrupt	= smsc_phy_ack_interrupt,
	.config_intr	= smsc_phy_config_intr,

	.suspend	= genphy_suspend,
	.resume		= genphy_resume,

#if defined(MY_ABC_HERE)
//do nothing
#else /* MY_ABC_HERE */
	.driver		= { .owner = THIS_MODULE, }
#endif /* MY_ABC_HERE */
}, {
	.phy_id		= 0x0007c0b0, /* OUI=0x00800f, Model#=0x0b */
	.phy_id_mask	= 0xfffffff0,
	.name		= "SMSC LAN8187",

	.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause),
	.flags		= PHY_HAS_INTERRUPT | PHY_HAS_MAGICANEG,

	/* basic functions */
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.config_init	= smsc_phy_config_init,
	.soft_reset	= smsc_phy_reset,

	/* IRQ related */
	.ack_interrupt	= smsc_phy_ack_interrupt,
	.config_intr	= smsc_phy_config_intr,

	.suspend	= genphy_suspend,
	.resume		= genphy_resume,

#if defined(MY_ABC_HERE)
//do nothing
#else /* MY_ABC_HERE */
	.driver		= { .owner = THIS_MODULE, }
#endif /* MY_ABC_HERE */
}, {
	.phy_id		= 0x0007c0c0, /* OUI=0x00800f, Model#=0x0c */
	.phy_id_mask	= 0xfffffff0,
	.name		= "SMSC LAN8700",

	.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause),
	.flags		= PHY_HAS_INTERRUPT | PHY_HAS_MAGICANEG,

	/* basic functions */
	.config_aneg	= genphy_config_aneg,
	.read_status	= lan87xx_read_status,
	.config_init	= smsc_phy_config_init,
	.soft_reset	= smsc_phy_reset,

	/* IRQ related */
	.ack_interrupt	= smsc_phy_ack_interrupt,
	.config_intr	= smsc_phy_config_intr,

	.suspend	= genphy_suspend,
	.resume		= genphy_resume,

#if defined(MY_ABC_HERE)
//do nothing
#else /* MY_ABC_HERE */
	.driver		= { .owner = THIS_MODULE, }
#endif /* MY_ABC_HERE */
}, {
	.phy_id		= 0x0007c0d0, /* OUI=0x00800f, Model#=0x0d */
	.phy_id_mask	= 0xfffffff0,
	.name		= "SMSC LAN911x Internal PHY",

	.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause),
	.flags		= PHY_HAS_INTERRUPT | PHY_HAS_MAGICANEG,

	/* basic functions */
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.config_init	= lan911x_config_init,

	/* IRQ related */
	.ack_interrupt	= smsc_phy_ack_interrupt,
	.config_intr	= smsc_phy_config_intr,

	.suspend	= genphy_suspend,
	.resume		= genphy_resume,

#if defined(MY_ABC_HERE)
//do nothing
#else /* MY_ABC_HERE */
	.driver		= { .owner = THIS_MODULE, }
#endif /* MY_ABC_HERE */
}, {
	.phy_id		= 0x0007c0f0, /* OUI=0x00800f, Model#=0x0f */
	.phy_id_mask	= 0xfffffff0,
	.name		= "SMSC LAN8710/LAN8720",

	.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause),
	.flags		= PHY_HAS_INTERRUPT | PHY_HAS_MAGICANEG,

	/* basic functions */
	.config_aneg	= genphy_config_aneg,
	.read_status	= lan87xx_read_status,
	.config_init	= smsc_phy_config_init,
	.soft_reset	= smsc_phy_reset,

	/* IRQ related */
	.ack_interrupt	= smsc_phy_ack_interrupt,
	.config_intr	= smsc_phy_config_intr,

	.suspend	= genphy_suspend,
	.resume		= genphy_resume,

#if defined(MY_ABC_HERE)
//do nothing
#else /* MY_ABC_HERE */
	.driver		= { .owner = THIS_MODULE, }
#endif /* MY_ABC_HERE */
} };

module_phy_driver(smsc_phy_driver);

MODULE_DESCRIPTION("SMSC PHY driver");
MODULE_AUTHOR("Herbert Valerio Riedel");
MODULE_LICENSE("GPL");

static struct mdio_device_id __maybe_unused smsc_tbl[] = {
	{ 0x0007c0a0, 0xfffffff0 },
	{ 0x0007c0b0, 0xfffffff0 },
	{ 0x0007c0c0, 0xfffffff0 },
	{ 0x0007c0d0, 0xfffffff0 },
	{ 0x0007c0f0, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, smsc_tbl);
