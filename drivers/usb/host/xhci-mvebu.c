#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Copyright (C) 2014 Marvell
 * Author: Gregory CLEMENT <gregory.clement@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/mbus.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "xhci-mvebu.h"
#if defined(MY_ABC_HERE)
#include "xhci.h"
#endif /* MY_ABC_HERE */

#define USB3_MAX_WINDOWS	4
#define USB3_WIN_CTRL(w)	(0x0 + ((w) * 8))
#define USB3_WIN_BASE(w)	(0x4 + ((w) * 8))

static void xhci_mvebu_mbus_config(void __iomem *base,
			const struct mbus_dram_target_info *dram)
{
	int win;

	/* Clear all existing windows */
	for (win = 0; win < USB3_MAX_WINDOWS; win++) {
		writel(0, base + USB3_WIN_CTRL(win));
		writel(0, base + USB3_WIN_BASE(win));
	}

	/* Program each DRAM CS in a seperate window */
	for (win = 0; win < dram->num_cs; win++) {
		const struct mbus_dram_window *cs = dram->cs + win;

		writel(((cs->size - 1) & 0xffff0000) | (cs->mbus_attr << 8) |
		       (dram->mbus_dram_target_id << 4) | 1,
		       base + USB3_WIN_CTRL(win));

		writel((cs->base & 0xffff0000), base + USB3_WIN_BASE(win));
	}
}

#if defined(MY_ABC_HERE)
static void xhci_mvebu_quirks(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);

	xhci->quirks |= XHCI_RESET_ON_RESUME;
}
#endif /* MY_ABC_HERE */

int xhci_mvebu_mbus_init_quirk(struct platform_device *pdev)
{
	struct resource	*res;
	void __iomem *base;
	const struct mbus_dram_target_info *dram;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -ENODEV;

	/*
	 * We don't use devm_ioremap() because this mapping should
	 * only exists for the duration of this probe function.
	 */
	base = ioremap(res->start, resource_size(res));
	if (!base)
		return -ENODEV;

	dram = mv_mbus_dram_info();
	xhci_mvebu_mbus_config(base, dram);

	/*
	 * This memory area was only needed to configure the MBus
	 * windows, and is therefore no longer useful.
	 */
	iounmap(base);

#if defined(MY_ABC_HERE)
	xhci_mvebu_quirks(pdev);
#endif /* MY_ABC_HERE */

	return 0;
}
