#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * CPU kernel entry/exit control
 *
 * Copyright (C) 2013 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/acpi.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/string.h>
#include <asm/acpi.h>
#include <asm/cpu_ops.h>
#include <asm/smp_plat.h>

extern const struct cpu_operations smp_spin_table_ops;
extern const struct cpu_operations cpu_psci_ops;

#if defined(CONFIG_ARCH_RTD129X) && defined(MY_DEF_HERE) || \
	defined(CONFIG_RTK_PLATFORM) && defined(CONFIG_SYNO_LSP_RTD1619)
extern const struct cpu_operations rtk_smp_spin_table_ops;
#endif /* CONFIG_ARCH_RTD129X && MY_DEF_HERE ||
		  CONFIG_RTK_PLATFORM && CONFIG_SYNO_LSP_RTD1619 */

const struct cpu_operations *cpu_ops[NR_CPUS];

static const struct cpu_operations *supported_cpu_ops[] __initconst = {
	&smp_spin_table_ops,
#if defined(CONFIG_ARCH_RTD129X) && defined(MY_DEF_HERE) || \
	defined(CONFIG_RTK_PLATFORM) && defined(CONFIG_SYNO_LSP_RTD1619)
	&rtk_smp_spin_table_ops,
#endif /* CONFIG_ARCH_RTD129X && MY_DEF_HERE ||
		  CONFIG_RTK_PLATFORM && CONFIG_SYNO_LSP_RTD1619 */
	&cpu_psci_ops,
	NULL,
};

static const struct cpu_operations * __init cpu_get_ops(const char *name)
{
	const struct cpu_operations **ops = supported_cpu_ops;

	while (*ops) {
		if (!strcmp(name, (*ops)->name))
			return *ops;

		ops++;
	}

	return NULL;
}

static const char *__init cpu_read_enable_method(int cpu)
{
	const char *enable_method;

	if (acpi_disabled) {
		struct device_node *dn = of_get_cpu_node(cpu, NULL);

		if (!dn) {
			if (!cpu)
				pr_err("Failed to find device node for boot cpu\n");
			return NULL;
		}

		enable_method = of_get_property(dn, "enable-method", NULL);
		if (!enable_method) {
			/*
			 * The boot CPU may not have an enable method (e.g.
			 * when spin-table is used for secondaries).
			 * Don't warn spuriously.
			 */
			if (cpu != 0)
				pr_err("%s: missing enable-method property\n",
					dn->full_name);
		}
		of_node_put(dn);
	} else {
		enable_method = acpi_get_enable_method(cpu);
		if (!enable_method)
			pr_err("Unsupported ACPI enable-method\n");
	}

	return enable_method;
}
/*
 * Read a cpu's enable method and record it in cpu_ops.
 */
int __init cpu_read_ops(int cpu)
{
	const char *enable_method = cpu_read_enable_method(cpu);

	if (!enable_method)
		return -ENODEV;

	cpu_ops[cpu] = cpu_get_ops(enable_method);
	if (!cpu_ops[cpu]) {
		pr_warn("Unsupported enable-method: %s\n", enable_method);
		return -EOPNOTSUPP;
	}

	return 0;
}
