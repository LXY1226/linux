#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * AHCI glue platform driver for Marvell EBU SOCs
 *
 * Copyright (C) 2014 Marvell
 *
 * Thomas Petazzoni <thomas.petazzoni@free-electrons.com>
 * Marcin Wojtas <mw@semihalf.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/ahci_platform.h>
#include <linux/kernel.h>
#include <linux/mbus.h>
#include <linux/module.h>
#include <linux/of_device.h>
#if defined(MY_ABC_HERE)
#include <linux/of_address.h>
#endif /* MY_ABC_HERE */
#include <linux/platform_device.h>
#include "ahci.h"
#if defined(MY_ABC_HERE)
#include <linux/mv_soc_info.h>
#endif /* MY_ABC_HERE */

#define DRV_NAME "ahci-mvebu"

#define AHCI_VENDOR_SPECIFIC_0_ADDR  0xa0
#define AHCI_VENDOR_SPECIFIC_0_DATA  0xa4

#define AHCI_WINDOW_CTRL(win)	(0x60 + ((win) << 4))
#define AHCI_WINDOW_BASE(win)	(0x64 + ((win) << 4))
#define AHCI_WINDOW_SIZE(win)	(0x68 + ((win) << 4))

#if defined(MY_ABC_HERE)
#define SATA3_VENDOR_ADDRESS			0xA0
#define SATA3_VENDOR_ADDR_OFSSET		0
#define SATA3_VENDOR_ADDR_MASK			(0xFFFFFFFF << SATA3_VENDOR_ADDR_OFSSET)
#define SATA3_VENDOR_DATA			0xA4

#define SATA_CONTROL_REG			0x0
#define SATA3_CTRL_SATA0_PD_OFFSET		6
#define SATA3_CTRL_SATA0_PD_MASK		(1 << SATA3_CTRL_SATA0_PD_OFFSET)
#define SATA3_CTRL_SATA1_PD_OFFSET		14
#define SATA3_CTRL_SATA1_PD_MASK		(1 << SATA3_CTRL_SATA1_PD_OFFSET)
#define SATA3_CTRL_SATA1_ENABLE_OFFSET		22
#define SATA3_CTRL_SATA1_ENABLE_MASK		(1 << SATA3_CTRL_SATA1_ENABLE_OFFSET)
#define SATA3_CTRL_SATA_SSU_OFFSET		23
#define SATA3_CTRL_SATA_SSU_MASK		(1 << SATA3_CTRL_SATA_SSU_OFFSET)

#define SATA_MBUS_SIZE_SELECT_REG		0x4
#define SATA_MBUS_REGRET_EN_OFFSET		7
#define SATA_MBUS_REGRET_EN_MASK		(0x1 << SATA_MBUS_REGRET_EN_OFFSET)

static void reg_set(void __iomem *addr, u32 data, u32 mask)
{
	u32 reg_data;

	reg_data = readl(addr);
	reg_data &= ~mask;
	reg_data |= data;
	writel(reg_data, addr);
}

#endif /* MY_ABC_HERE */
static void ahci_mvebu_mbus_config(struct ahci_host_priv *hpriv,
				   const struct mbus_dram_target_info *dram)
{
	int i;

	for (i = 0; i < 4; i++) {
		writel(0, hpriv->mmio + AHCI_WINDOW_CTRL(i));
		writel(0, hpriv->mmio + AHCI_WINDOW_BASE(i));
		writel(0, hpriv->mmio + AHCI_WINDOW_SIZE(i));
	}

	for (i = 0; i < dram->num_cs; i++) {
		const struct mbus_dram_window *cs = dram->cs + i;

		writel((cs->mbus_attr << 8) |
		       (dram->mbus_dram_target_id << 4) | 1,
		       hpriv->mmio + AHCI_WINDOW_CTRL(i));
		writel(cs->base >> 16, hpriv->mmio + AHCI_WINDOW_BASE(i));
		writel(((cs->size - 1) & 0xffff0000),
		       hpriv->mmio + AHCI_WINDOW_SIZE(i));
	}
}

static void ahci_mvebu_regret_option(struct ahci_host_priv *hpriv)
{
	/*
	 * Enable the regret bit to allow the SATA unit to regret a
	 * request that didn't receive an acknowlegde and avoid a
	 * deadlock
	 */
	writel(0x4, hpriv->mmio + AHCI_VENDOR_SPECIFIC_0_ADDR);
	writel(0x80, hpriv->mmio + AHCI_VENDOR_SPECIFIC_0_DATA);
}

#if defined(MY_ABC_HERE)
//do nothing
#else /* MY_ABC_HERE */
#ifdef CONFIG_PM_SLEEP
static int ahci_mvebu_suspend(struct platform_device *pdev, pm_message_t state)
{
	return ahci_platform_suspend_host(&pdev->dev);
}

static int ahci_mvebu_resume(struct platform_device *pdev)
{
	struct ata_host *host = platform_get_drvdata(pdev);
	struct ahci_host_priv *hpriv = host->private_data;
	const struct mbus_dram_target_info *dram;

	dram = mv_mbus_dram_info();
	if (dram)
		ahci_mvebu_mbus_config(hpriv, dram);

	ahci_mvebu_regret_option(hpriv);

	return ahci_platform_resume_host(&pdev->dev);
}
#else
#define ahci_mvebu_suspend NULL
#define ahci_mvebu_resume NULL
#endif

static const struct ata_port_info ahci_mvebu_port_info = {
	.flags	   = AHCI_FLAG_COMMON,
	.pio_mask  = ATA_PIO4,
	.udma_mask = ATA_UDMA6,
	.port_ops  = &ahci_platform_ops,
};

static struct scsi_host_template ahci_platform_sht = {
	AHCI_SHT(DRV_NAME),
};

#endif /* MY_ABC_HERE */

#ifdef MY_DEF_HERE
extern int syno_compare_dts_ata_port(const struct ata_port *pAtaPort, const struct device_node *pDeviceNode);
/**
 * syno_mvebu_compart_ata_devicetree_info - check the ata_port matches the device_node
 * @ap [IN]:   query ata_port
 * @pNode [IN]: comparing device_node
 *
 * return true: success
          false: fail
 */
bool syno_mvebu_compart_ata_devicetree_info(const struct ata_port *ap, const struct device_node *pNode)
{
	int ret = false;
	struct device_node *pAhciMvebuNode = NULL;
	if (NULL == ap || NULL == pNode) {
		goto END;
	}

	pAhciMvebuNode = of_get_child_by_name(pNode, DT_AHCI_MVEBU);
	if (0 == syno_compare_dts_ata_port(ap, pAhciMvebuNode)) {
		ret = true;
	}
	of_node_put(pAhciMvebuNode);
END:
	return ret;
}
#endif /* MY_DEF_HERE */

#if defined(MY_ABC_HERE)
#if defined(MY_ABC_HERE)
//do nothing
#else /* MY_ABC_HERE */
static void reg_set(void __iomem *addr, u32 data, u32 mask)
{
	u32 reg_data;

	reg_data = readl(addr);
	reg_data &= ~mask;
	reg_data |= data;
	writel(reg_data, addr);
}

#define SATA3_VENDOR_ADDRESS			0xA0
#define SATA3_VENDOR_ADDR_OFSSET		0
#define SATA3_VENDOR_ADDR_MASK			(0xFFFFFFFF << SATA3_VENDOR_ADDR_OFSSET)
#define SATA3_VENDOR_DATA			0xA4

#define SATA_CONTROL_REG			0x0
#define SATA3_CTRL_SATA0_PD_OFFSET		6
#define SATA3_CTRL_SATA0_PD_MASK		(1 << SATA3_CTRL_SATA0_PD_OFFSET)
#define SATA3_CTRL_SATA1_PD_OFFSET		14
#define SATA3_CTRL_SATA1_PD_MASK		(1 << SATA3_CTRL_SATA1_PD_OFFSET)
#define SATA3_CTRL_SATA1_ENABLE_OFFSET		22
#define SATA3_CTRL_SATA1_ENABLE_MASK		(1 << SATA3_CTRL_SATA1_ENABLE_OFFSET)
#define SATA3_CTRL_SATA_SSU_OFFSET		23
#define SATA3_CTRL_SATA_SSU_MASK		(1 << SATA3_CTRL_SATA_SSU_OFFSET)

#define SATA_MBUS_SIZE_SELECT_REG		0x4
#define SATA_MBUS_REGRET_EN_OFFSET		7
#define SATA_MBUS_REGRET_EN_MASK		(0x1 << SATA_MBUS_REGRET_EN_OFFSET)
#endif /* MY_ABC_HERE */
/**
#if defined(MY_ABC_HERE)
 * ahci_mvebu_pll_power_up
#else // MY_ABC_HERE
 * ahci_mvebu_cp_110_power_up
#endif // MY_ABC_HERE
 *
 * @pdev:	A pointer to ahci platform device
 * @hpriv:	A pointer to achi host private structure
#if defined(MY_ABC_HERE)
 * @pd_polarity: 1 or 0 to power down the PLL
#endif // MY_ABC_HERE
 *
 * This function configures corresponding comphy to SATA mode.
 * AHCI driver acquires an handle to the corresponding PHY from
 * the device-tree (In ahci_platform_get_resources).
#if defined(MY_ABC_HERE)
 * Mvebu SATA require the following sequence:
#else // MY_ABC_HERE
 * cp110 require the following sequence:
#endif // MY_ABC_HERE
 *	1. Power down AHCI macs
 *	2. Configure the corresponding comphy (comphy driver).
 *	3. Power up AHCI macs
 *	4. Check if comphy PLL was locked
 *
 * Return: 0 on success; Error code otherwise.
 */
#if defined(MY_ABC_HERE)
static int ahci_mvebu_pll_power_up(struct platform_device *pdev,
				   struct ahci_host_priv *hpriv,
				   u32 pd_polarity)
#else /* MY_ABC_HERE */
static int ahci_mvebu_cp_110_power_up(struct platform_device *pdev,
				      struct ahci_host_priv *hpriv)
#endif /* MY_ABC_HERE */
{
	u32 mask, data, i;
	int err = 0;

	/* Power off AHCI macs */
	reg_set(hpriv->mmio + SATA3_VENDOR_ADDRESS,
		SATA_CONTROL_REG << SATA3_VENDOR_ADDR_OFSSET,
		SATA3_VENDOR_ADDR_MASK);
	/* SATA port 0 power down */
	mask = SATA3_CTRL_SATA0_PD_MASK;
#if defined(MY_ABC_HERE)
	/*
	 * Marvell SoC have different power down polarity.
	 * For Armada 3700, 0 means that power down the PLL, 1 means power up.
	 * but for CP110, 1 means to power down the PLL while 0 for power up.
	 */
#else // MY_ABC_HERE
	data = 0x1 << SATA3_CTRL_SATA0_PD_OFFSET;
	/* SATA port 1 power down */
	mask |= SATA3_CTRL_SATA1_PD_MASK;
	data |= 0x1 << SATA3_CTRL_SATA1_PD_OFFSET;
	/* SATA SSU disable */
	mask |= SATA3_CTRL_SATA1_ENABLE_MASK;
	data |= 0x0 << SATA3_CTRL_SATA1_ENABLE_OFFSET;
	/* SATA port 1 disable
	 * There's no option to disable SATA port 0, so we power down both
	 * ports (during previous steps) but disable onlt SATA port 1
	 */
#endif /* MY_ABC_HERE */
#if defined(MY_ABC_HERE)
	if (pd_polarity)
		data = 0x1 << SATA3_CTRL_SATA0_PD_OFFSET;
	else
		data = 0x0 << SATA3_CTRL_SATA0_PD_OFFSET;
	if (hpriv->nports > 1) {
		/* SATA port 1 power down */
		mask |= SATA3_CTRL_SATA1_PD_MASK;
		data |= 0x1 << SATA3_CTRL_SATA1_PD_OFFSET;
		/* SATA SSU disable */
		mask |= SATA3_CTRL_SATA1_ENABLE_MASK;
		data |= 0x0 << SATA3_CTRL_SATA1_ENABLE_OFFSET;
		/* SATA port 1 disable
		 * There's no option to disable SATA port 0, so we power down both
		 * ports (during previous steps) but disable only SATA port 1
		 */
		mask |= SATA3_CTRL_SATA_SSU_MASK;
		data |= 0x0 << SATA3_CTRL_SATA_SSU_OFFSET;
	}
#else /* MY_ABC_HERE */
	mask |= SATA3_CTRL_SATA_SSU_MASK;
	data |= 0x0 << SATA3_CTRL_SATA_SSU_OFFSET;
#endif /* MY_ABC_HERE */
	reg_set(hpriv->mmio + SATA3_VENDOR_DATA, data, mask);

	/* Configure corresponding comphy
	 * First we need to call phy_power_off because the phy_power_on
	 * was called by generic AHCI code.
	 * Next, we call phy_power_on in order to configure the comphy
	 * while AHCI is powered down.
	 */
	for (i = 0; i < hpriv->nports; i++) {
		err = phy_power_off(hpriv->phys[i]);
		if (err) {
			dev_err(&pdev->dev, "unable to power off SATA comphy\n");
			return -EINVAL;
		}
		err = phy_power_on(hpriv->phys[i]);
		if (err) {
			dev_err(&pdev->dev, "unable to power on SATA comphy\n");
			return -EINVAL;
		}
	}

	/* Power up AHCI macs */
	reg_set(hpriv->mmio + SATA3_VENDOR_ADDRESS,
		SATA_CONTROL_REG << SATA3_VENDOR_ADDR_OFSSET,
		SATA3_VENDOR_ADDR_MASK);
	/* SATA port 0 power up */
	mask = SATA3_CTRL_SATA0_PD_MASK;
#if defined(MY_ABC_HERE)
	/*
	 * Marvell SoC have different power down polarity.
	 * For Armada 3700, 0 means that power down the PLL, 1 means power up.
	 * but for CP110, 1 means to power down the PLL while 0 for power down.
	 */
	if (pd_polarity)
		data = 0x0 << SATA3_CTRL_SATA0_PD_OFFSET;
	else
		data = 0x1 << SATA3_CTRL_SATA0_PD_OFFSET;
	if (hpriv->nports > 1) {
		/* SATA port 1 power up */
		mask |= SATA3_CTRL_SATA1_PD_MASK;
		data |= 0x0 << SATA3_CTRL_SATA1_PD_OFFSET;
		/* SATA SSU enable */
		mask |= SATA3_CTRL_SATA1_ENABLE_MASK;
		data |= 0x1 << SATA3_CTRL_SATA1_ENABLE_OFFSET;
		/* SATA port 1 enable */
		mask |= SATA3_CTRL_SATA_SSU_MASK;
		data |= 0x1 << SATA3_CTRL_SATA_SSU_OFFSET;
	}
#else /* MY_ABC_HERE */
	data = 0x0 << SATA3_CTRL_SATA0_PD_OFFSET;
	/* SATA port 1 power up */
	mask |= SATA3_CTRL_SATA1_PD_MASK;
	data |= 0x0 << SATA3_CTRL_SATA1_PD_OFFSET;
	/* SATA SSU enable */
	mask |= SATA3_CTRL_SATA1_ENABLE_MASK;
	data |= 0x1 << SATA3_CTRL_SATA1_ENABLE_OFFSET;
	/* SATA port 1 enable */
	mask |= SATA3_CTRL_SATA_SSU_MASK;
	data |= 0x1 << SATA3_CTRL_SATA_SSU_OFFSET;
#endif /* MY_ABC_HERE */
	reg_set(hpriv->mmio + SATA3_VENDOR_DATA, data, mask);

	/* MBUS request size and interface select register */
	reg_set(hpriv->mmio + SATA3_VENDOR_ADDRESS,
		SATA_MBUS_SIZE_SELECT_REG << SATA3_VENDOR_ADDR_OFSSET,
		SATA3_VENDOR_ADDR_MASK);
	/* Mbus regret enable */
	reg_set(hpriv->mmio + SATA3_VENDOR_DATA,
		0x1 << SATA_MBUS_REGRET_EN_OFFSET,
		SATA_MBUS_REGRET_EN_MASK);

	/* Check if comphy PLL is locked */
	for (i = 0; i < hpriv->nports; i++) {
		err = phy_is_pll_locked(hpriv->phys[i]);
		if (err) {
			dev_err(&pdev->dev, "port %d: comphy PLL is not locked for SATA. Unable to power on SATA comphy\n",
				i);
			return err;
		}
	}

	return err;
}

#endif /* MY_ABC_HERE */
#if defined(MY_ABC_HERE)

#ifdef CONFIG_PM_SLEEP
static int ahci_mvebu_suspend(struct platform_device *pdev, pm_message_t state)
{
	int err;
	struct ata_host *host = platform_get_drvdata(pdev);
	struct ahci_host_priv *hpriv = host->private_data;

	err = ahci_platform_suspend_host(&pdev->dev);
	if (err)
		return err;

	/* AHCI resources, such as PHY, clock, etc. should be disabled */
	ahci_platform_disable_resources(hpriv);

	return 0;
}

static int ahci_mvebu_resume(struct platform_device *pdev)
{
	struct ata_host *host = platform_get_drvdata(pdev);
	struct ahci_host_priv *hpriv = host->private_data;
	const struct mbus_dram_target_info *dram;
	int err;

	/* AHCI resources, such as PHY, clock, etc. should be enabled first */
	err = ahci_platform_enable_resources(hpriv);
	if (err)
		return err;

	if (of_device_is_compatible(pdev->dev.of_node,
				    "marvell,armada-380-ahci")) {
		dram = mv_mbus_dram_info();
		if (dram)
			ahci_mvebu_mbus_config(hpriv, dram);

		ahci_mvebu_regret_option(hpriv);
	}

	if (of_device_is_compatible(pdev->dev.of_node,
				    "marvell,armada-cp110-ahci"))
		ahci_mvebu_pll_power_up(pdev, hpriv, 1);

	if (of_device_is_compatible(pdev->dev.of_node,
				    "marvell,armada-3700-ahci"))
		ahci_mvebu_pll_power_up(pdev, hpriv, 0);

	return ahci_platform_resume_host(&pdev->dev);
}
#else
#define ahci_mvebu_suspend NULL
#define ahci_mvebu_resume NULL
#endif

static const struct ata_port_info ahci_mvebu_port_info = {
	.flags	   = AHCI_FLAG_COMMON,
	.pio_mask  = ATA_PIO4,
	.udma_mask = ATA_UDMA6,
	.port_ops  = &ahci_platform_ops,
};

static struct scsi_host_template ahci_platform_sht = {
	AHCI_SHT(DRV_NAME),
};

#endif /* MY_ABC_HERE */
static int ahci_mvebu_probe(struct platform_device *pdev)
{
	struct ahci_host_priv *hpriv;
	const struct mbus_dram_target_info *dram;
	int rc;

	hpriv = ahci_platform_get_resources(pdev);
	if (IS_ERR(hpriv))
		return PTR_ERR(hpriv);

	rc = ahci_platform_enable_resources(hpriv);
	if (rc)
		return rc;

#if defined(MY_ABC_HERE)
	if (of_device_is_compatible(pdev->dev.of_node,
				    "marvell,armada-380-ahci")) {
		dram = mv_mbus_dram_info();
		if (!dram)
			return -ENODEV;

		ahci_mvebu_mbus_config(hpriv, dram);
		ahci_mvebu_regret_option(hpriv);
	}

#if defined(MY_ABC_HERE)
#if defined(MY_ABC_HERE)
	/* Call comphy initialization flow */
#else /* MY_ABC_HERE */
	/* Call cp110 comphy initialization flow */
#endif /* MY_ABC_HERE */
#else /* MY_ABC_HERE */
	/* In A8k A0 AHCI unit the port register offsets are not
	 * according to AHCI specification. We need a WA for a8k (cp110).
	 */
#endif /* MY_ABC_HERE */
	if (of_device_is_compatible(pdev->dev.of_node,
#if defined(MY_ABC_HERE)
				    "marvell,armada-cp110-ahci")) {
#else /* MY_ABC_HERE */
	    "marvell,armada-cp110-ahci")) {
#endif /* MY_ABC_HERE */
#if defined(MY_ABC_HERE)
#if defined(MY_ABC_HERE)
		rc = ahci_mvebu_pll_power_up(pdev, hpriv, 1);
#else /* MY_ABC_HERE */
		rc = ahci_mvebu_cp_110_power_up(pdev, hpriv);
#endif /* MY_ABC_HERE */
		if (rc)
			return rc;
#else /* MY_ABC_HERE */
		struct device_node *node;
		void __iomem *gwd_iidr2;
		const unsigned int *reg;
		phys_addr_t paddr;

		/* Read the node which holds the "Global Watchdog Interface
		 * Identification Register (GWD_IIDR2)" address - holds the revision.
		 */
		node = of_find_compatible_node(NULL, NULL,
					       "marvell,ap806-rev-info");
		if (!node) {
			dev_err(&pdev->dev, "unable to read rev-info node\n");
			of_node_put(node);
			return -ENODEV;
		}

		/* Read the offset of the register */
		reg = of_get_property(node, "reg", NULL);
		if (!reg) {
			dev_err(&pdev->dev, "unable to read reg property from rev-info node\n");
			of_node_put(node);
			return -ENODEV;
		}

		/* Translate the offset to phyisical address */
		paddr = of_translate_address(node, reg);
		if (paddr == OF_BAD_ADDR) {
			dev_err(&pdev->dev, "of_translate_address failed\n");
			of_node_put(node);
			return -EINVAL;
		}

		gwd_iidr2 = ioremap(paddr, reg[1]);
		if (!gwd_iidr2) {
			dev_err(&pdev->dev, "rev-info ioremap() failed\n");
			of_node_put(node);
			return -EINVAL;
#endif /* MY_ABC_HERE */
		}

#if defined(MY_ABC_HERE)
#if defined(MY_ABC_HERE)
//do nothing
#else /* MY_ABC_HERE */
	/* Armada-8k revision A0, port register offsets of the aHCI unit are not
	 * according to aHCI specification, so we need a WA for A8k rev A0 (CP110).
	 */
	if (of_device_is_compatible(pdev->dev.of_node, "marvell,armada-cp110-ahci")
		&& mv_soc_info_get_revision() == APN806_REV_ID_A0) {
		pr_debug("setting aHCI port offset WA for A8k revision A0");
		/* Read the correct port base and offset from the
		 * device tree and set hpriv->a8k_a0_wa for future use.
		 */
#endif /* MY_ABC_HERE */
#else /* MY_ABC_HERE */
#define GWD_IIDR2_REV_ID_OFFSET	12
#define GWD_IIDR2_REV_ID_MASK	0xF
#define APN806_REV_ID_A0	0

		/* The workaround is required only for A0 revision.
		 * read gwd_iidr2 register to determing the revision
		 */
		if (((readl(gwd_iidr2) >> GWD_IIDR2_REV_ID_OFFSET) &
		    GWD_IIDR2_REV_ID_MASK) == APN806_REV_ID_A0) {

#endif /* MY_ABC_HERE */
#if defined(MY_ABC_HERE)
	/* Call comphy initialization flow */
	if (of_device_is_compatible(pdev->dev.of_node,
				    "marvell,armada-3700-ahci")) {
		rc = ahci_mvebu_pll_power_up(pdev, hpriv, 0);
		if (rc)
			return rc;
#else /* MY_ABC_HERE */
			/* Read the correct port base and offset from the
			 * device tree and set hpriv->a8k_a0_wa for future use.
			 */
			hpriv->a8k_a0_wa = 1;
			of_property_read_u32(pdev->dev.of_node, "port_base",
					     &hpriv->port_base);
			of_property_read_u32(pdev->dev.of_node, "port_offset",
					     &hpriv->port_offset);
#endif /* MY_ABC_HERE */
		}

#if defined(MY_ABC_HERE)
	if (of_property_read_u32(pdev->dev.of_node, "comwake",
				 &hpriv->comwake))
		hpriv->comwake = 0;

	if (of_property_read_u32(pdev->dev.of_node, "comreset_u",
				 &hpriv->comreset_u))
		hpriv->comreset_u = 0;

#endif /* MY_ABC_HERE */
#if defined(MY_ABC_HERE)
// do nothing
#else /* MY_ABC_HERE */
		/* Release resources */
		iounmap(gwd_iidr2);
		of_node_put(node);
	}
#endif /* MY_ABC_HERE */
#else /* MY_ABC_HERE */
	dram = mv_mbus_dram_info();
	if (!dram)
		return -ENODEV;
	ahci_mvebu_mbus_config(hpriv, dram);
	ahci_mvebu_regret_option(hpriv);
#endif /* MY_ABC_HERE */

#ifdef MY_DEF_HERE
	ahci_platform_ops.syno_compare_node_info = syno_mvebu_compart_ata_devicetree_info;
#endif /* MY_DEF_HERE */

	rc = ahci_platform_init_host(pdev, hpriv, &ahci_mvebu_port_info,
				     &ahci_platform_sht);
	if (rc)
		goto disable_resources;

	return 0;

disable_resources:
	ahci_platform_disable_resources(hpriv);
	return rc;
}

static const struct of_device_id ahci_mvebu_of_match[] = {
	{ .compatible = "marvell,armada-380-ahci", },
#if defined(MY_ABC_HERE)
	{ .compatible = "marvell,armada-3700-ahci", },
	{ .compatible = "marvell,armada-cp110-ahci", },
#endif /* MY_ABC_HERE */
	{ },
};
MODULE_DEVICE_TABLE(of, ahci_mvebu_of_match);

/*
 * We currently don't provide power management related operations,
 * since there is no suspend/resume support at the platform level for
 * Armada 38x for the moment.
 */
static struct platform_driver ahci_mvebu_driver = {
	.probe = ahci_mvebu_probe,
	.remove = ata_platform_remove_one,
	.suspend = ahci_mvebu_suspend,
	.resume = ahci_mvebu_resume,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = ahci_mvebu_of_match,
	},
};
module_platform_driver(ahci_mvebu_driver);

MODULE_DESCRIPTION("Marvell EBU AHCI SATA driver");
MODULE_AUTHOR("Thomas Petazzoni <thomas.petazzoni@free-electrons.com>, Marcin Wojtas <mw@semihalf.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ahci_mvebu");
