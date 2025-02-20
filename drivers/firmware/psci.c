#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright (C) 2015 ARM Limited
 */

#define pr_fmt(fmt) "psci: " fmt

#include <linux/arm-smccc.h>
#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/printk.h>
#include <linux/psci.h>
#include <linux/reboot.h>
#include <linux/suspend.h>

#include <uapi/linux/psci.h>

#include <asm/cputype.h>
#include <asm/system_misc.h>
#include <asm/smp_plat.h>
#include <asm/suspend.h>

#ifdef MY_ABC_HERE
#include <linux/delay.h>
#include <linux/serial_reg.h>

#define PORT1_BASE              0xD0012200
#define UART1_WRITE(val, base, offset) iowrite32(val, base + offset)
#define UART1_READ(base, offset) ioread32(base + offset)
#define SET8N1			0x0
#define UART_CTL		0x4
#define UART_1BYTE_TX_HOLDING	0x1C
#define SOFTWARE_SHUTDOWN	0x31
#define SOFTWARE_REBOOT		0x43
#endif /* MY_ABC_HERE */
/*
 * While a 64-bit OS can make calls with SMC32 calling conventions, for some
 * calls it is necessary to use SMC64 to pass or return 64-bit values.
 * For such calls PSCI_FN_NATIVE(version, name) will choose the appropriate
 * (native-width) function ID.
 */
#ifdef CONFIG_64BIT
#define PSCI_FN_NATIVE(version, name)	PSCI_##version##_FN64_##name
#else
#define PSCI_FN_NATIVE(version, name)	PSCI_##version##_FN_##name
#endif

/*
 * The CPU any Trusted OS is resident on. The trusted OS may reject CPU_OFF
 * calls to its resident CPU, so we must avoid issuing those. We never migrate
 * a Trusted OS even if it claims to be capable of migration -- doing so will
 * require cooperation with a Trusted OS driver.
 */
static int resident_cpu = -1;

bool psci_tos_resident_on(int cpu)
{
	return cpu == resident_cpu;
}

struct psci_operations psci_ops = {
	.conduit = PSCI_CONDUIT_NONE,
	.smccc_version = SMCCC_VERSION_1_0,
};

typedef unsigned long (psci_fn)(unsigned long, unsigned long,
				unsigned long, unsigned long);
static psci_fn *invoke_psci_fn;

enum psci_function {
	PSCI_FN_CPU_SUSPEND,
	PSCI_FN_CPU_ON,
	PSCI_FN_CPU_OFF,
	PSCI_FN_MIGRATE,
	PSCI_FN_MAX,
};

static u32 psci_function_id[PSCI_FN_MAX];

#define PSCI_0_2_POWER_STATE_MASK		\
				(PSCI_0_2_POWER_STATE_ID_MASK | \
				PSCI_0_2_POWER_STATE_TYPE_MASK | \
				PSCI_0_2_POWER_STATE_AFFL_MASK)

#define PSCI_1_0_EXT_POWER_STATE_MASK		\
				(PSCI_1_0_EXT_POWER_STATE_ID_MASK | \
				PSCI_1_0_EXT_POWER_STATE_TYPE_MASK)

static u32 psci_cpu_suspend_feature;

static inline bool psci_has_ext_power_state(void)
{
	return psci_cpu_suspend_feature &
				PSCI_1_0_FEATURES_CPU_SUSPEND_PF_MASK;
}

bool psci_power_state_loses_context(u32 state)
{
	const u32 mask = psci_has_ext_power_state() ?
					PSCI_1_0_EXT_POWER_STATE_TYPE_MASK :
					PSCI_0_2_POWER_STATE_TYPE_MASK;

	return state & mask;
}

bool psci_power_state_is_valid(u32 state)
{
	const u32 valid_mask = psci_has_ext_power_state() ?
			       PSCI_1_0_EXT_POWER_STATE_MASK :
			       PSCI_0_2_POWER_STATE_MASK;

	return !(state & ~valid_mask);
}

static unsigned long __invoke_psci_fn_hvc(unsigned long function_id,
			unsigned long arg0, unsigned long arg1,
			unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

static unsigned long __invoke_psci_fn_smc(unsigned long function_id,
			unsigned long arg0, unsigned long arg1,
			unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

static int psci_to_linux_errno(int errno)
{
	switch (errno) {
	case PSCI_RET_SUCCESS:
		return 0;
	case PSCI_RET_NOT_SUPPORTED:
		return -EOPNOTSUPP;
	case PSCI_RET_INVALID_PARAMS:
	case PSCI_RET_INVALID_ADDRESS:
		return -EINVAL;
	case PSCI_RET_DENIED:
		return -EPERM;
	};

	return -EINVAL;
}

static u32 psci_get_version(void)
{
	return invoke_psci_fn(PSCI_0_2_FN_PSCI_VERSION, 0, 0, 0);
}

static int psci_cpu_suspend(u32 state, unsigned long entry_point)
{
	int err;
	u32 fn;

	fn = psci_function_id[PSCI_FN_CPU_SUSPEND];
	err = invoke_psci_fn(fn, state, entry_point, 0);
	return psci_to_linux_errno(err);
}

static int psci_cpu_off(u32 state)
{
	int err;
	u32 fn;

	fn = psci_function_id[PSCI_FN_CPU_OFF];
	err = invoke_psci_fn(fn, state, 0, 0);
	return psci_to_linux_errno(err);
}

static int psci_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	int err;
	u32 fn;

	fn = psci_function_id[PSCI_FN_CPU_ON];
	err = invoke_psci_fn(fn, cpuid, entry_point, 0);
	return psci_to_linux_errno(err);
}

static int psci_migrate(unsigned long cpuid)
{
	int err;
	u32 fn;

	fn = psci_function_id[PSCI_FN_MIGRATE];
	err = invoke_psci_fn(fn, cpuid, 0, 0);
	return psci_to_linux_errno(err);
}

static int psci_affinity_info(unsigned long target_affinity,
		unsigned long lowest_affinity_level)
{
	return invoke_psci_fn(PSCI_FN_NATIVE(0_2, AFFINITY_INFO),
			      target_affinity, lowest_affinity_level, 0);
}

static int psci_migrate_info_type(void)
{
	return invoke_psci_fn(PSCI_0_2_FN_MIGRATE_INFO_TYPE, 0, 0, 0);
}

static unsigned long psci_migrate_info_up_cpu(void)
{
	return invoke_psci_fn(PSCI_FN_NATIVE(0_2, MIGRATE_INFO_UP_CPU),
			      0, 0, 0);
}

static void set_conduit(enum psci_conduit conduit)
{
	switch (conduit) {
	case PSCI_CONDUIT_HVC:
		invoke_psci_fn = __invoke_psci_fn_hvc;
		break;
	case PSCI_CONDUIT_SMC:
		invoke_psci_fn = __invoke_psci_fn_smc;
		break;
	default:
		WARN(1, "Unexpected PSCI conduit %d\n", conduit);
	}

	psci_ops.conduit = conduit;
}

static int get_set_conduit_method(struct device_node *np)
{
	const char *method;

	pr_info("probing for conduit method from DT.\n");

	if (of_property_read_string(np, "method", &method)) {
		pr_warn("missing \"method\" property\n");
		return -ENXIO;
	}

	if (!strcmp("hvc", method)) {
		set_conduit(PSCI_CONDUIT_HVC);
	} else if (!strcmp("smc", method)) {
		set_conduit(PSCI_CONDUIT_SMC);
	} else {
		pr_warn("invalid \"method\" property: %s\n", method);
		return -EINVAL;
	}
	return 0;
}
#ifdef MY_ABC_HERE
#define  CTRL_TXFIFO_RST	BIT(15)
#define  CTRL_RXFIFO_RST	BIT(14)
#define  CTRL_BRK_DET_INT	BIT(3)
#define  CTRL_FRM_ERR_INT	BIT(2)
#define  CTRL_PAR_ERR_INT	BIT(1)
#define  CTRL_OVR_ERR_INT	BIT(0)
#define  CTRL_BRK_INT		(CTRL_BRK_DET_INT | CTRL_FRM_ERR_INT\
				 | CTRL_PAR_ERR_INT | CTRL_OVR_ERR_INT)
#define UART_EXT_CTRL2		0x20
#define EXT_CTRL2_RX_RDY_INT_1B	BIT(5)

extern void __iomem *syno_uart1_base;

static void synology_init_uart(void __iomem *uart1_base)
{
	u32 val = 0;
	if (NULL == uart1_base) {
		printk("NULL uart1 base!!\n");
		goto END;
	}
	UART1_WRITE(CTRL_TXFIFO_RST | CTRL_RXFIFO_RST, uart1_base, UART_CTL);
	udelay(1);
	UART1_WRITE(CTRL_BRK_INT, uart1_base, UART_CTL);

	val = UART1_READ(uart1_base, UART_EXT_CTRL2);
	val |= EXT_CTRL2_RX_RDY_INT_1B;
	UART1_WRITE(val, uart1_base, UART_EXT_CTRL2);

	UART1_WRITE(SET8N1, uart1_base, UART_CTL);

END:
	return;
}

static void synology_restart(enum reboot_mode reboot_mode, const char *cmd)
{
	void __iomem *uart1_base;

	printk("Synology_reboot\n");
	if (NULL != syno_uart1_base) {
		uart1_base = syno_uart1_base;
	} else {
		uart1_base = ioremap(PORT1_BASE, 0x2C);
	}
	synology_init_uart(uart1_base);
	UART1_WRITE(SOFTWARE_REBOOT, uart1_base, UART_1BYTE_TX_HOLDING);

	/* delay for uart1 send the request to uP */
	mdelay(5000);
	/* models without microp will go here */
	printk("Reboot failed -- psci reset\n");
	invoke_psci_fn(PSCI_0_2_FN_SYSTEM_RESET, 0, 0, 0);
	local_irq_disable();
	printk("PSCI Reboot failed - system halt \n");
	while (1);
}

static void synology_power_off(void)
{
	void __iomem *uart1_base;

	printk("Synology_power_off\n");
	if (NULL != syno_uart1_base) {
		uart1_base = syno_uart1_base;
	} else {
		uart1_base = ioremap(PORT1_BASE, 0x2C);
	}
	synology_init_uart(uart1_base);
	UART1_WRITE(SOFTWARE_SHUTDOWN, uart1_base, UART_1BYTE_TX_HOLDING);
}
#else /* MY_ABC_HERE */
static void psci_sys_reset(enum reboot_mode reboot_mode, const char *cmd)
{
	invoke_psci_fn(PSCI_0_2_FN_SYSTEM_RESET, 0, 0, 0);
}

static void psci_sys_poweroff(void)
{
	invoke_psci_fn(PSCI_0_2_FN_SYSTEM_OFF, 0, 0, 0);
}
#endif /* MY_ABC_HERE */

static int __init psci_features(u32 psci_func_id)
{
	return invoke_psci_fn(PSCI_1_0_FN_PSCI_FEATURES,
			      psci_func_id, 0, 0);
}

static int psci_system_suspend(unsigned long unused)
{
	return invoke_psci_fn(PSCI_FN_NATIVE(1_0, SYSTEM_SUSPEND),
			      virt_to_phys(cpu_resume), 0, 0);
}

static int psci_system_suspend_enter(suspend_state_t state)
{
	return cpu_suspend(0, psci_system_suspend);
}

static const struct platform_suspend_ops psci_suspend_ops = {
	.valid          = suspend_valid_only_mem,
	.enter          = psci_system_suspend_enter,
};

static void __init psci_init_system_suspend(void)
{
	int ret;

	if (!IS_ENABLED(CONFIG_SUSPEND))
		return;

	ret = psci_features(PSCI_FN_NATIVE(1_0, SYSTEM_SUSPEND));

	if (ret != PSCI_RET_NOT_SUPPORTED)
		suspend_set_ops(&psci_suspend_ops);
}

static void __init psci_init_cpu_suspend(void)
{
	int feature = psci_features(psci_function_id[PSCI_FN_CPU_SUSPEND]);

	if (feature != PSCI_RET_NOT_SUPPORTED)
		psci_cpu_suspend_feature = feature;
}

/*
 * Detect the presence of a resident Trusted OS which may cause CPU_OFF to
 * return DENIED (which would be fatal).
 */
static void __init psci_init_migrate(void)
{
	unsigned long cpuid;
	int type, cpu = -1;

	type = psci_ops.migrate_info_type();

	if (type == PSCI_0_2_TOS_MP) {
		pr_info("Trusted OS migration not required\n");
		return;
	}

	if (type == PSCI_RET_NOT_SUPPORTED) {
		pr_info("MIGRATE_INFO_TYPE not supported.\n");
		return;
	}

	if (type != PSCI_0_2_TOS_UP_MIGRATE &&
	    type != PSCI_0_2_TOS_UP_NO_MIGRATE) {
		pr_err("MIGRATE_INFO_TYPE returned unknown type (%d)\n", type);
		return;
	}

	cpuid = psci_migrate_info_up_cpu();
	if (cpuid & ~MPIDR_HWID_BITMASK) {
		pr_warn("MIGRATE_INFO_UP_CPU reported invalid physical ID (0x%lx)\n",
			cpuid);
		return;
	}

	cpu = get_logical_index(cpuid);
	resident_cpu = cpu >= 0 ? cpu : -1;

	pr_info("Trusted OS resident on physical CPU 0x%lx\n", cpuid);
}

static void __init psci_init_smccc(void)
{
	u32 ver = ARM_SMCCC_VERSION_1_0;
	int feature;

	feature = psci_features(ARM_SMCCC_VERSION_FUNC_ID);

	if (feature != PSCI_RET_NOT_SUPPORTED) {
		u32 ret;
		ret = invoke_psci_fn(ARM_SMCCC_VERSION_FUNC_ID, 0, 0, 0);
		if (ret == ARM_SMCCC_VERSION_1_1) {
			psci_ops.smccc_version = SMCCC_VERSION_1_1;
			ver = ret;
		}
	}

	/*
	 * Conveniently, the SMCCC and PSCI versions are encoded the
	 * same way. No, this isn't accidental.
	 */
	pr_info("SMC Calling Convention v%d.%d\n",
		PSCI_VERSION_MAJOR(ver), PSCI_VERSION_MINOR(ver));

}

static void __init psci_0_2_set_functions(void)
{
	pr_info("Using standard PSCI v0.2 function IDs\n");
	psci_function_id[PSCI_FN_CPU_SUSPEND] =
					PSCI_FN_NATIVE(0_2, CPU_SUSPEND);
	psci_ops.cpu_suspend = psci_cpu_suspend;

	psci_function_id[PSCI_FN_CPU_OFF] = PSCI_0_2_FN_CPU_OFF;
	psci_ops.cpu_off = psci_cpu_off;

	psci_function_id[PSCI_FN_CPU_ON] = PSCI_FN_NATIVE(0_2, CPU_ON);
	psci_ops.cpu_on = psci_cpu_on;

	psci_function_id[PSCI_FN_MIGRATE] = PSCI_FN_NATIVE(0_2, MIGRATE);
	psci_ops.migrate = psci_migrate;

	psci_ops.affinity_info = psci_affinity_info;

	psci_ops.migrate_info_type = psci_migrate_info_type;

#ifdef MY_ABC_HERE
	arm_pm_restart = synology_restart;
	pm_power_off = synology_power_off;
#else /* MY_ABC_HERE */
	arm_pm_restart = psci_sys_reset;

	pm_power_off = psci_sys_poweroff;
#endif /* MY_ABC_HERE */
}

/*
 * Probe function for PSCI firmware versions >= 0.2
 */
static int __init psci_probe(void)
{
	u32 ver = psci_get_version();

	pr_info("PSCIv%d.%d detected in firmware.\n",
			PSCI_VERSION_MAJOR(ver),
			PSCI_VERSION_MINOR(ver));

	if (PSCI_VERSION_MAJOR(ver) == 0 && PSCI_VERSION_MINOR(ver) < 2) {
		pr_err("Conflicting PSCI version detected.\n");
		return -EINVAL;
	}

	psci_0_2_set_functions();

	psci_init_migrate();

	if (PSCI_VERSION_MAJOR(ver) >= 1) {
		psci_init_smccc();
		psci_init_cpu_suspend();
		psci_init_system_suspend();
	}

	return 0;
}

typedef int (*psci_initcall_t)(const struct device_node *);

/*
 * PSCI init function for PSCI versions >=0.2
 *
 * Probe based on PSCI PSCI_VERSION function
 */
static int __init psci_0_2_init(struct device_node *np)
{
	int err;

	err = get_set_conduit_method(np);

	if (err)
		goto out_put_node;
	/*
	 * Starting with v0.2, the PSCI specification introduced a call
	 * (PSCI_VERSION) that allows probing the firmware version, so
	 * that PSCI function IDs and version specific initialization
	 * can be carried out according to the specific version reported
	 * by firmware
	 */
	err = psci_probe();

out_put_node:
	of_node_put(np);
	return err;
}

/*
 * PSCI < v0.2 get PSCI Function IDs via DT.
 */
static int __init psci_0_1_init(struct device_node *np)
{
	u32 id;
	int err;

	err = get_set_conduit_method(np);

	if (err)
		goto out_put_node;

	pr_info("Using PSCI v0.1 Function IDs from DT\n");

	if (!of_property_read_u32(np, "cpu_suspend", &id)) {
		psci_function_id[PSCI_FN_CPU_SUSPEND] = id;
		psci_ops.cpu_suspend = psci_cpu_suspend;
	}

	if (!of_property_read_u32(np, "cpu_off", &id)) {
		psci_function_id[PSCI_FN_CPU_OFF] = id;
		psci_ops.cpu_off = psci_cpu_off;
	}

	if (!of_property_read_u32(np, "cpu_on", &id)) {
		psci_function_id[PSCI_FN_CPU_ON] = id;
		psci_ops.cpu_on = psci_cpu_on;
	}

	if (!of_property_read_u32(np, "migrate", &id)) {
		psci_function_id[PSCI_FN_MIGRATE] = id;
		psci_ops.migrate = psci_migrate;
	}

out_put_node:
	of_node_put(np);
	return err;
}

static const struct of_device_id psci_of_match[] __initconst = {
	{ .compatible = "arm,psci",	.data = psci_0_1_init},
	{ .compatible = "arm,psci-0.2",	.data = psci_0_2_init},
	{ .compatible = "arm,psci-1.0",	.data = psci_0_2_init},
	{},
};

int __init psci_dt_init(void)
{
	struct device_node *np;
	const struct of_device_id *matched_np;
	psci_initcall_t init_fn;

	np = of_find_matching_node_and_match(NULL, psci_of_match, &matched_np);

	if (!np)
		return -ENODEV;

	init_fn = (psci_initcall_t)matched_np->data;
	return init_fn(np);
}

#ifdef CONFIG_ACPI
/*
 * We use PSCI 0.2+ when ACPI is deployed on ARM64 and it's
 * explicitly clarified in SBBR
 */
int __init psci_acpi_init(void)
{
	if (!acpi_psci_present()) {
		pr_info("is not implemented in ACPI.\n");
		return -EOPNOTSUPP;
	}

	pr_info("probing for conduit method from ACPI.\n");

	if (acpi_psci_use_hvc())
		set_conduit(PSCI_CONDUIT_HVC);
	else
		set_conduit(PSCI_CONDUIT_SMC);

	return psci_probe();
}
#endif
