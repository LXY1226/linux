#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Synopsys DesignWare I2C adapter driver (master only).
 *
 * Based on the TI DAVINCI I2C adapter driver.
 *
 * Copyright (C) 2006 Texas Instruments.
 * Copyright (C) 2007 MontaVista Software Inc.
 * Copyright (C) 2009 Provigent Ltd.
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * ----------------------------------------------------------------------------
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/dmi.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/platform_data/i2c-designware.h>
#include "i2c-designware-core.h"
#ifdef MY_DEF_HERE
#include <linux/synobios.h>
#endif /* MY_DEF_HERE */
#if defined(MY_DEF_HERE)
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/syno_gpio.h>
#include <linux/synolib.h>
#endif /* MY_DEF_HERE */

static u32 i2c_dw_get_clk_rate_khz(struct dw_i2c_dev *dev)
{
	return clk_get_rate(dev->clk)/1000;
}

#ifdef CONFIG_ACPI
/*
 * The HCNT/LCNT information coming from ACPI should be the most accurate
 * for given platform. However, some systems get it wrong. On such systems
 * we get better results by calculating those based on the input clock.
 */
static const struct dmi_system_id dw_i2c_no_acpi_params[] = {
	{
		.ident = "Dell Inspiron 7348",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Inspiron 7348"),
		},
	},
	{ }
};

static void dw_i2c_acpi_params(struct platform_device *pdev, char method[],
			       u16 *hcnt, u16 *lcnt, u32 *sda_hold)
{
	struct acpi_buffer buf = { ACPI_ALLOCATE_BUFFER };
	acpi_handle handle = ACPI_HANDLE(&pdev->dev);
	union acpi_object *obj;

	if (dmi_check_system(dw_i2c_no_acpi_params))
		return;

	if (ACPI_FAILURE(acpi_evaluate_object(handle, method, NULL, &buf)))
		return;

	obj = (union acpi_object *)buf.pointer;
	if (obj->type == ACPI_TYPE_PACKAGE && obj->package.count == 3) {
		const union acpi_object *objs = obj->package.elements;

		*hcnt = (u16)objs[0].integer.value;
		*lcnt = (u16)objs[1].integer.value;
		if (sda_hold)
			*sda_hold = (u32)objs[2].integer.value;
	}

	kfree(buf.pointer);
}

static int dw_i2c_acpi_configure(struct platform_device *pdev)
{
	struct dw_i2c_dev *dev = platform_get_drvdata(pdev);
	const struct acpi_device_id *id;

	dev->adapter.nr = -1;
	dev->tx_fifo_depth = 32;
	dev->rx_fifo_depth = 32;

	/*
	 * Try to get SDA hold time and *CNT values from an ACPI method if
	 * it exists for both supported speed modes.
	 */
	dw_i2c_acpi_params(pdev, "SSCN", &dev->ss_hcnt, &dev->ss_lcnt, NULL);
	dw_i2c_acpi_params(pdev, "FMCN", &dev->fs_hcnt, &dev->fs_lcnt,
			   &dev->sda_hold_time);

	id = acpi_match_device(pdev->dev.driver->acpi_match_table, &pdev->dev);
	if (id && id->driver_data)
		dev->accessor_flags |= (u32)id->driver_data;

	return 0;
}

static const struct acpi_device_id dw_i2c_acpi_match[] = {
	{ "INT33C2", 0 },
	{ "INT33C3", 0 },
	{ "INT3432", 0 },
	{ "INT3433", 0 },
	{ "80860F41", 0 },
	{ "808622C1", 0 },
	{ "AMD0010", ACCESS_INTR_MASK },
	{ "AMDI0010", ACCESS_INTR_MASK },
	{ "AMDI0510", 0 },
	{ "APMC0D0F", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, dw_i2c_acpi_match);
#else
static inline int dw_i2c_acpi_configure(struct platform_device *pdev)
{
	return -ENODEV;
}
#endif

#if defined(MY_DEF_HERE)
static int syno_dw_recovery_proc_show(struct seq_file *m, void *v)
{
	struct dw_i2c_dev *dev = (struct dw_i2c_dev *)m->private;
	u8 iomux_val = 0;

	// record iomux value of scl_gpio before setting to gpio 
	iomux_val = readb(dev->iomux_base + dev->rinfo.scl_gpio);
	
	//set scl_gpio to gpio mode
	writeb(0x2, dev->iomux_base + dev->rinfo.scl_gpio);

	pulse_try_cnt++;
	if (0 == i2c_recover_bus(&dev->adapter)) {
		pulse_suc_cnt++;
		printk("i2c recover work\n");
	} 

	delay_try_cnt++;
	if (0 == syno_dw_delay_recovery(&dev->adapter)) {
		delay_suc_cnt++;
		printk("smbus recover work\n");
	}

	seq_printf(m, "Force i2c recovery\n");
	
	// restore iomux value of scl_gpio
	writeb(iomux_val, dev->iomux_base + dev->rinfo.scl_gpio);
	return 0;
}

static int syno_dw_recovery_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, syno_dw_recovery_proc_show, PDE_DATA(inode));
}

static const struct file_operations syno_dw_recovery_proc_fops = {
        .open           = syno_dw_recovery_proc_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};

static int syno_dw_recovery_cnt_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "42ms delay try cnt : %lu\n", delay_try_cnt);
	seq_printf(m, "42ms delay suc cnt : %lu\n", delay_suc_cnt);
	seq_printf(m, "more pulse try cnt : %lu\n", pulse_try_cnt);
	seq_printf(m, "more pulse suc cnt : %lu\n", pulse_suc_cnt);

	return 0;
}

static int syno_dw_recovery_cnt_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, syno_dw_recovery_cnt_proc_show, PDE_DATA(inode));
}

static const struct file_operations syno_dw_recovery_cnt_proc_fops = {
        .open           = syno_dw_recovery_cnt_proc_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};

static int proc_syno_dw_recovery_cnt_init(struct dw_i2c_dev *dev, struct proc_dir_entry* parent)
{
	int iResult = 0;
	struct proc_dir_entry *p;
	p = proc_create_data("syno_dw_recovery_cnt", 0, parent, &syno_dw_recovery_cnt_proc_fops, dev);
	if (NULL == p) {
		printk("Fail to cat syno_dw_recovery_cnt proc\n");
		iResult = -1;
	}

	return iResult;
}

static int proc_syno_dw_recovery_init(struct dw_i2c_dev *dev, struct proc_dir_entry* parent)
{
	int iResult = 0;
	struct proc_dir_entry *p;
	p = proc_create_data("syno_dw_recovery", 0, parent, &syno_dw_recovery_proc_fops, dev);
	if (NULL == p) {
		printk("Fail to create syno_dw_recovery proc\n");
		iResult = -1;
	}

	return iResult;
}

static int i2c_dw_init_recovery_info(struct dw_i2c_dev *dev)
{
	struct i2c_bus_recovery_info *rinfo = &dev->rinfo;
	struct i2c_adapter *adap = &dev->adapter;

	// FIXME: customized for AMD V/R1000 i2c3 ONLY
	rinfo->scl_gpio = 19;
	rinfo->sda_gpio = 20;
	rinfo->recover_bus = i2c_generic_scl_recovery;

	rinfo->set_scl = syno_dw_i2c_set_scl;
	rinfo->get_scl = syno_dw_i2c_get_scl;
	rinfo->get_sda = syno_dw_i2c_get_sda;

	adap->bus_recovery_info = rinfo;

	return 0;
}
#endif /* MY_DEF_HERE */

static int dw_i2c_plat_probe(struct platform_device *pdev)
{
	struct dw_i2c_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct dw_i2c_dev *dev;
	struct i2c_adapter *adap;
	struct resource *mem;
	int irq, r;
	u32 clk_freq, ht = 0;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	dev = devm_kzalloc(&pdev->dev, sizeof(struct dw_i2c_dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dev->base = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(dev->base))
		return PTR_ERR(dev->base);

	dev->dev = &pdev->dev;
	dev->irq = irq;
	platform_set_drvdata(pdev, dev);

	/* fast mode by default because of legacy reasons */
#ifdef MY_DEF_HERE
	if (syno_is_hw_version(HW_DS1621p) || syno_is_hw_version(HW_DS1821p)) {
		clk_freq = 400000;
	} else {
		clk_freq = 100000;
	}
#else /* MY_DEF_HERE */
	clk_freq = 400000;
#endif /* MY_DEF_HERE */

	if (pdata) {
		clk_freq = pdata->i2c_scl_freq;
	} else {
#ifdef MY_DEF_HERE
		if (syno_is_hw_version(HW_RS822p) || syno_is_hw_version(HW_RS822rpp)) {
			ht = 100;
		}
#else /* MY_DEF_HERE */
		device_property_read_u32(&pdev->dev, "i2c-sda-hold-time-ns",
					 &ht);
#endif /* MY_DEF_HERE */
		device_property_read_u32(&pdev->dev, "i2c-sda-falling-time-ns",
					 &dev->sda_falling_time);
		device_property_read_u32(&pdev->dev, "i2c-scl-falling-time-ns",
					 &dev->scl_falling_time);
		device_property_read_u32(&pdev->dev, "clock-frequency",
					 &clk_freq);
	}

	if (has_acpi_companion(&pdev->dev))
		dw_i2c_acpi_configure(pdev);

	/*
	 * Only standard mode at 100kHz and fast mode at 400kHz are supported.
	 */
	if (clk_freq != 100000 && clk_freq != 400000) {
		dev_err(&pdev->dev, "Only 100kHz and 400kHz supported");
		return -EINVAL;
	}

	r = i2c_dw_eval_lock_support(dev);
	if (r)
		return r;

	dev->functionality =
		I2C_FUNC_I2C |
		I2C_FUNC_10BIT_ADDR |
		I2C_FUNC_SMBUS_BYTE |
		I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA |
		I2C_FUNC_SMBUS_I2C_BLOCK;
	if (clk_freq == 100000)
		dev->master_cfg =  DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE |
			DW_IC_CON_RESTART_EN | DW_IC_CON_SPEED_STD;
	else
		dev->master_cfg =  DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE |
			DW_IC_CON_RESTART_EN | DW_IC_CON_SPEED_FAST;

	dev->clk = devm_clk_get(&pdev->dev, NULL);
	dev->get_clk_rate_khz = i2c_dw_get_clk_rate_khz;
	if (IS_ERR(dev->clk))
		return PTR_ERR(dev->clk);
	clk_prepare_enable(dev->clk);

	if (!dev->sda_hold_time && ht) {
		u32 ic_clk = dev->get_clk_rate_khz(dev);

		dev->sda_hold_time = div_u64((u64)ic_clk * ht + 500000,
					     1000000);
	}

	if (!dev->tx_fifo_depth) {
		u32 param1 = i2c_dw_read_comp_param(dev);

		dev->tx_fifo_depth = ((param1 >> 16) & 0xff) + 1;
		dev->rx_fifo_depth = ((param1 >> 8)  & 0xff) + 1;
		dev->adapter.nr = pdev->id;
	}

	adap = &dev->adapter;
	adap->owner = THIS_MODULE;
#if defined(MY_DEF_HERE) || defined(MY_DEF_HERE) || defined(MY_DEF_HERE)
	adap->class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
#else /* MY_DEF_HERE || MY_DEF_HERE || MY_DEF_HERE */
	adap->class = I2C_CLASS_DEPRECATED;
#endif /* MY_DEF_HERE || MY_DEF_HERE || MY_DEF_HERE */
	ACPI_COMPANION_SET(&adap->dev, ACPI_COMPANION(&pdev->dev));
	adap->dev.of_node = pdev->dev.of_node;

	if (dev->pm_runtime_disabled) {
		pm_runtime_forbid(&pdev->dev);
	} else {
		pm_runtime_set_autosuspend_delay(&pdev->dev, 1000);
		pm_runtime_use_autosuspend(&pdev->dev);
		pm_runtime_set_active(&pdev->dev);
		pm_runtime_enable(&pdev->dev);
	}

	r = i2c_dw_probe(dev);

#if defined(MY_DEF_HERE)
	if (syno_is_hw_version(HW_RS422p) && 1 == adap->nr) {
		dev->gpio_base = ioremap(AMD_GPIO_BASE, AMD_GPIO_BASE_SIZE);
		dev->iomux_base = ioremap(AMD_IOMUX_BASE, AMD_IOMUX_BASE_SIZE);
		r = i2c_dw_init_recovery_info(dev);

		snprintf(dev->proc_dir_name, sizeof(dev->proc_dir_name), "syno-dw-i2c-%d", adap->nr);
		dev->i2c_proc_dir = proc_mkdir(dev->proc_dir_name, NULL);

		proc_syno_dw_recovery_init(dev, dev->i2c_proc_dir);
		proc_syno_dw_recovery_cnt_init(dev, dev->i2c_proc_dir);
	}
#endif /* MY_DEF_HERE */

	if (r && !dev->pm_runtime_disabled)
		pm_runtime_disable(&pdev->dev);

	return r;
}

static int dw_i2c_plat_remove(struct platform_device *pdev)
{
	struct dw_i2c_dev *dev = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&pdev->dev);

#if defined(MY_DEF_HERE)
	if (syno_is_hw_version(HW_RS422p) && 1 == dev->adapter.nr) {
		remove_proc_entry("syno_dw_recovery", dev->i2c_proc_dir);
		remove_proc_entry("syno_dw_recovery_cnt", dev->i2c_proc_dir);
		remove_proc_entry(dev->proc_dir_name, NULL);

		if (dev->iomux_base) {
			iounmap(dev->iomux_base);
		}
		if (dev->gpio_base) {
			iounmap(dev->gpio_base);
		}
	}
#endif /* MY_DEF_HERE */

	i2c_del_adapter(&dev->adapter);

	i2c_dw_disable(dev);

	pm_runtime_dont_use_autosuspend(&pdev->dev);
	pm_runtime_put_sync(&pdev->dev);
	if (!dev->pm_runtime_disabled)
		pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dw_i2c_of_match[] = {
	{ .compatible = "snps,designware-i2c", },
	{},
};
MODULE_DEVICE_TABLE(of, dw_i2c_of_match);
#endif

#ifdef CONFIG_PM_SLEEP
static int dw_i2c_plat_prepare(struct device *dev)
{
	return pm_runtime_suspended(dev);
}

static void dw_i2c_plat_complete(struct device *dev)
{
	if (dev->power.direct_complete)
		pm_request_resume(dev);
}
#else
#define dw_i2c_plat_prepare	NULL
#define dw_i2c_plat_complete	NULL
#endif

#ifdef CONFIG_PM
static int dw_i2c_plat_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_i2c_dev *i_dev = platform_get_drvdata(pdev);

	i2c_dw_disable(i_dev);
	clk_disable_unprepare(i_dev->clk);

	return 0;
}

static int dw_i2c_plat_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_i2c_dev *i_dev = platform_get_drvdata(pdev);

	clk_prepare_enable(i_dev->clk);

	if (!i_dev->pm_runtime_disabled)
		i2c_dw_init(i_dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dw_i2c_plat_suspend(struct device *dev)
{
	pm_runtime_resume(dev);
	return dw_i2c_plat_runtime_suspend(dev);
}
#endif

static const struct dev_pm_ops dw_i2c_dev_pm_ops = {
	.prepare = dw_i2c_plat_prepare,
	.complete = dw_i2c_plat_complete,
	SET_SYSTEM_SLEEP_PM_OPS(dw_i2c_plat_suspend, dw_i2c_plat_resume)
	SET_RUNTIME_PM_OPS(dw_i2c_plat_runtime_suspend,
			   dw_i2c_plat_resume,
			   NULL)
};

#define DW_I2C_DEV_PMOPS (&dw_i2c_dev_pm_ops)
#else
#define DW_I2C_DEV_PMOPS NULL
#endif

/* work with hotplug and coldplug */
MODULE_ALIAS("platform:i2c_designware");

static struct platform_driver dw_i2c_driver = {
	.probe = dw_i2c_plat_probe,
	.remove = dw_i2c_plat_remove,
	.driver		= {
		.name	= "i2c_designware",
		.of_match_table = of_match_ptr(dw_i2c_of_match),
		.acpi_match_table = ACPI_PTR(dw_i2c_acpi_match),
		.pm	= DW_I2C_DEV_PMOPS,
	},
};

static int __init dw_i2c_init_driver(void)
{
	return platform_driver_register(&dw_i2c_driver);
}
subsys_initcall(dw_i2c_init_driver);

static void __exit dw_i2c_exit_driver(void)
{
	platform_driver_unregister(&dw_i2c_driver);
}
module_exit(dw_i2c_exit_driver);

MODULE_AUTHOR("Baruch Siach <baruch@tkos.co.il>");
MODULE_DESCRIPTION("Synopsys DesignWare I2C bus adapter");
MODULE_LICENSE("GPL");
