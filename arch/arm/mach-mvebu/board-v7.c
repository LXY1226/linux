#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Device Tree support for Armada 370 and XP platforms.
 *
 * Copyright (C) 2012 Marvell
 *
 * Lior Amsalem <alior@marvell.com>
 * Gregory CLEMENT <gregory.clement@free-electrons.com>
 * Thomas Petazzoni <thomas.petazzoni@free-electrons.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/clocksource.h>
#include <linux/dma-mapping.h>
#include <linux/memblock.h>
#include <linux/mbus.h>
#include <linux/slab.h>
#include <linux/irqchip.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/smp_scu.h>
#include "armada-370-xp.h"
#include "common.h"
#include "coherency.h"
#include "mvebu-soc-id.h"

#if defined(MY_ABC_HERE)
#define SCU_CTRL		0x00
#endif /* MY_ABC_HERE */
static void __iomem *scu_base;

/*
 * Enables the SCU when available. Obviously, this is only useful on
 * Cortex-A based SOCs, not on PJ4B based ones.
 */
static void __init mvebu_scu_enable(void)
{
#if defined(MY_ABC_HERE)
	u32 scu_ctrl;
#endif /* MY_ABC_HERE */
	struct device_node *np =
		of_find_compatible_node(NULL, NULL, "arm,cortex-a9-scu");
	if (np) {
		scu_base = of_iomap(np, 0);

#if defined(MY_ABC_HERE)
		scu_ctrl = readl_relaxed(scu_base + SCU_CTRL);
		/* already enabled? */
		if (!(scu_ctrl & 1)) {
			/* Enable SCU Speculative linefills to L2 */
			scu_ctrl |= (1 << 3);
			writel_relaxed(scu_ctrl, scu_base + SCU_CTRL);
		}
#endif /* MY_ABC_HERE */

		scu_enable(scu_base);
		of_node_put(np);
	}
}

void __iomem *mvebu_get_scu_base(void)
{
	return scu_base;
}

/*
 * When returning from suspend, the platform goes through the
 * bootloader, which executes its DDR3 training code. This code has
 * the unfortunate idea of using the first 10 KB of each DRAM bank to
 * exercise the RAM and calculate the optimal timings. Therefore, this
 * area of RAM is overwritten, and shouldn't be used by the kernel if
 * suspend/resume is supported.
 */

#ifdef CONFIG_SUSPEND
#define MVEBU_DDR_TRAINING_AREA_SZ (10 * SZ_1K)
static int __init mvebu_scan_mem(unsigned long node, const char *uname,
				 int depth, void *data)
{
	const char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	const __be32 *reg, *endp;
	int l;

	if (type == NULL || strcmp(type, "memory"))
		return 0;

	reg = of_get_flat_dt_prop(node, "linux,usable-memory", &l);
	if (reg == NULL)
		reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));
	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		memblock_reserve(base, MVEBU_DDR_TRAINING_AREA_SZ);
	}

	return 0;
}

static void __init mvebu_memblock_reserve(void)
{
	of_scan_flat_dt(mvebu_scan_mem, NULL);
}
#else
static void __init mvebu_memblock_reserve(void) {}
#endif

#if defined(MY_ABC_HERE)
void __init mvebu_l2_optimizations(void)
{
	void __iomem *l2x0_base;
	struct device_node *np;
	unsigned int val;

	np = of_find_compatible_node(NULL, NULL, "arm,pl310-cache");
	if (!np)
		return;

	l2x0_base = of_iomap(np, 0);
	if (!l2x0_base) {
		of_node_put(np);
		return;
	}

	/* Configure the L2 PREFETCH and POWER registers */
	val = 0x58800000;
	/*
	*  Support the following configuration:
	*  Incr double linefill enable
	*  Data prefetch enable
	*  Double linefill enable
	*  Double linefill on WRAP disable
	*  NO prefetch drop enable
	 */
	writel_relaxed(val, l2x0_base + L310_PREFETCH_CTRL);
	val = L310_DYNAMIC_CLK_GATING_EN;
	writel_relaxed(val, l2x0_base + L310_POWER_CTRL);

	iounmap(l2x0_base);
	of_node_put(np);
}
#endif /* MY_ABC_HERE */

static void __init mvebu_init_irq(void)
{
#if defined(MY_ABC_HERE)
	mvebu_l2_optimizations();
#endif /* MY_ABC_HERE */
	irqchip_init();
	mvebu_scu_enable();
	coherency_init();
#if defined(MY_ABC_HERE)

	/* In case we are running from MSYS, skip mbus initialization. The
	 * mvebu_mbus_dt_init was executed earlier in msys_irqchip_init. This
	 * was required by switch interrupt driver (marvell,swic), which had
	 * to have access to switch region (decoding windows had to be opened).
	 */
	if (!(of_machine_is_compatible("marvell,msys")))
		BUG_ON(mvebu_mbus_dt_init(coherency_available()));
#else /* MY_ABC_HERE */
	BUG_ON(mvebu_mbus_dt_init(coherency_available()));
#endif /* MY_ABC_HERE */
}

#if defined(MY_ABC_HERE)
static void __init msys_irqchip_init(void)
{
	/* Because the switch interrupt driver (marvell,swic) uses register from
	* the switch region space, the decoding window for switch must be
	* initialized, before calling interrupt drivers.
	*/
	BUG_ON(mvebu_mbus_dt_init(coherency_available()));
	mvebu_init_irq();
}
#endif /* MY_ABC_HERE */

static void __init i2c_quirk(void)
{
	struct device_node *np;
	u32 dev, rev;

	/*
	 * Only revisons more recent than A0 support the offload
	 * mechanism. We can exit only if we are sure that we can
	 * get the SoC revision and it is more recent than A0.
	 */
	if (mvebu_get_soc_id(&dev, &rev) == 0 && rev > MV78XX0_A0_REV)
		return;

	for_each_compatible_node(np, NULL, "marvell,mv78230-i2c") {
		struct property *new_compat;

		new_compat = kzalloc(sizeof(*new_compat), GFP_KERNEL);

		new_compat->name = kstrdup("compatible", GFP_KERNEL);
		new_compat->length = sizeof("marvell,mv78230-a0-i2c");
		new_compat->value = kstrdup("marvell,mv78230-a0-i2c",
						GFP_KERNEL);

		of_update_property(np, new_compat);
	}
	return;
}

static void __init mvebu_dt_init(void)
{
	if (of_machine_is_compatible("marvell,armadaxp"))
		i2c_quirk();

	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

static const char * const armada_370_xp_dt_compat[] __initconst = {
	"marvell,armada-370-xp",
	NULL,
};

DT_MACHINE_START(ARMADA_370_XP_DT, "Marvell Armada 370/XP (Device Tree)")
	.l2c_aux_val	= 0,
	.l2c_aux_mask	= ~0,
/*
 * The following field (.smp) is still needed to ensure backward
 * compatibility with old Device Trees that were not specifying the
 * cpus enable-method property.
 */
	.smp		= smp_ops(armada_xp_smp_ops),
	.init_machine	= mvebu_dt_init,
	.init_irq       = mvebu_init_irq,
	.restart	= mvebu_restart,
	.reserve        = mvebu_memblock_reserve,
	.dt_compat	= armada_370_xp_dt_compat,
MACHINE_END

static const char * const armada_375_dt_compat[] __initconst = {
	"marvell,armada375",
	NULL,
};

DT_MACHINE_START(ARMADA_375_DT, "Marvell Armada 375 (Device Tree)")
	.l2c_aux_val	= 0,
	.l2c_aux_mask	= ~0,
	.init_irq       = mvebu_init_irq,
	.init_machine	= mvebu_dt_init,
	.restart	= mvebu_restart,
	.dt_compat	= armada_375_dt_compat,
MACHINE_END

static const char * const armada_38x_dt_compat[] __initconst = {
	"marvell,armada380",
	"marvell,armada385",
	NULL,
};

DT_MACHINE_START(ARMADA_38X_DT, "Marvell Armada 380/385 (Device Tree)")
	.l2c_aux_val	= 0,
	.l2c_aux_mask	= ~0,
	.init_irq       = mvebu_init_irq,
	.restart	= mvebu_restart,
#if defined(MY_ABC_HERE)
	.reserve        = mvebu_memblock_reserve,
#endif /* MY_ABC_HERE */
	.dt_compat	= armada_38x_dt_compat,
MACHINE_END

static const char * const armada_39x_dt_compat[] __initconst = {
	"marvell,armada390",
	"marvell,armada398",
	NULL,
};

DT_MACHINE_START(ARMADA_39X_DT, "Marvell Armada 39x (Device Tree)")
	.l2c_aux_val	= 0,
	.l2c_aux_mask	= ~0,
	.init_irq       = mvebu_init_irq,
	.restart	= mvebu_restart,
	.dt_compat	= armada_39x_dt_compat,
MACHINE_END

#if defined(MY_ABC_HERE)
static const char * const msys_dt_compat[] __initconst = {
	"marvell,msys",
	NULL,
};


DT_MACHINE_START(MSYS_DT, "Marvell SYS (Device Tree)")
	.l2c_aux_val	= 0,
	.l2c_aux_mask	= ~0,
/*
 * The following field (.smp) is still needed to ensure backward
 * compatibility with old Device Trees that were not specifying the
 * cpus enable-method property.
 */
	.smp		= smp_ops(armada_xp_smp_ops),
	.init_machine	= mvebu_dt_init,
	.init_irq       = msys_irqchip_init,
	.restart	= mvebu_restart,
	.reserve        = mvebu_memblock_reserve,
	.dt_compat	= msys_dt_compat,
MACHINE_END
#endif /* MY_ABC_HERE */
