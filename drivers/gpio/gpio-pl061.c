#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Copyright (C) 2008, 2009 Provigent Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Driver for the ARM PrimeCell(tm) General Purpose Input/Output (PL061)
 *
 * Data sheet: ARM DDI 0190B, September 2000
 */
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/bitops.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl061.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm.h>

#define GPIODIR 0x400
#define GPIOIS  0x404
#define GPIOIBE 0x408
#define GPIOIEV 0x40C
#define GPIOIE  0x410
#define GPIORIS 0x414
#define GPIOMIS 0x418
#define GPIOIC  0x41C

#define PL061_GPIO_NR	8

#ifdef CONFIG_PM
struct pl061_context_save_regs {
	u8 gpio_data;
	u8 gpio_dir;
	u8 gpio_is;
	u8 gpio_ibe;
	u8 gpio_iev;
	u8 gpio_ie;
};
#endif

struct pl061_gpio {
	spinlock_t		lock;

	void __iomem		*base;
	struct gpio_chip	gc;

#ifdef CONFIG_PM
	struct pl061_context_save_regs csave_regs;
#endif
};

static int pl061_direction_input(struct gpio_chip *gc, unsigned offset)
{
	struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
	unsigned long flags;
	unsigned char gpiodir;

	if (offset >= gc->ngpio)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock, flags);
	gpiodir = readb(chip->base + GPIODIR);
	gpiodir &= ~(BIT(offset));
	writeb(gpiodir, chip->base + GPIODIR);
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int pl061_direction_output(struct gpio_chip *gc, unsigned offset,
		int value)
{
	struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
	unsigned long flags;
	unsigned char gpiodir;

	if (offset >= gc->ngpio)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock, flags);
	writeb(!!value << offset, chip->base + (BIT(offset + 2)));
	gpiodir = readb(chip->base + GPIODIR);
	gpiodir |= BIT(offset);
	writeb(gpiodir, chip->base + GPIODIR);

	/*
	 * gpio value is set again, because pl061 doesn't allow to set value of
	 * a gpio pin before configuring it in OUT mode.
	 */
	writeb(!!value << offset, chip->base + (BIT(offset + 2)));
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int pl061_get_value(struct gpio_chip *gc, unsigned offset)
{
	struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);

	return !!readb(chip->base + (BIT(offset + 2)));
}

static void pl061_set_value(struct gpio_chip *gc, unsigned offset, int value)
{
	struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);

	writeb(!!value << offset, chip->base + (BIT(offset + 2)));
}

static int pl061_irq_type(struct irq_data *d, unsigned trigger)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
	int offset = irqd_to_hwirq(d);
	unsigned long flags;
	u8 gpiois, gpioibe, gpioiev;
	u8 bit = BIT(offset);

	if (offset < 0 || offset >= PL061_GPIO_NR)
		return -EINVAL;

	if ((trigger & (IRQ_TYPE_LEVEL_HIGH | IRQ_TYPE_LEVEL_LOW)) &&
	    (trigger & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING)))
	{
#if defined(MY_ABC_HERE)
		dev_err(gc->parent,
#else /* MY_ABC_HERE */
		dev_err(gc->dev,
#endif /* MY_ABC_HERE */
			"trying to configure line %d for both level and edge "
			"detection, choose one!\n",
			offset);
		return -EINVAL;
	}


	spin_lock_irqsave(&chip->lock, flags);

	gpioiev = readb(chip->base + GPIOIEV);
	gpiois = readb(chip->base + GPIOIS);
	gpioibe = readb(chip->base + GPIOIBE);

	if (trigger & (IRQ_TYPE_LEVEL_HIGH | IRQ_TYPE_LEVEL_LOW)) {
		bool polarity = trigger & IRQ_TYPE_LEVEL_HIGH;

		/* Disable edge detection */
		gpioibe &= ~bit;
		/* Enable level detection */
		gpiois |= bit;
		/* Select polarity */
		if (polarity)
			gpioiev |= bit;
		else
			gpioiev &= ~bit;
		irq_set_handler_locked(d, handle_level_irq);
#if defined(MY_ABC_HERE)
		dev_dbg(gc->parent, "line %d: IRQ on %s level\n",
#else /* MY_ABC_HERE */
		dev_dbg(gc->dev, "line %d: IRQ on %s level\n",
#endif /* MY_ABC_HERE */
			offset,
			polarity ? "HIGH" : "LOW");
	} else if ((trigger & IRQ_TYPE_EDGE_BOTH) == IRQ_TYPE_EDGE_BOTH) {
		/* Disable level detection */
		gpiois &= ~bit;
		/* Select both edges, setting this makes GPIOEV be ignored */
		gpioibe |= bit;
		irq_set_handler_locked(d, handle_edge_irq);
#if defined(MY_ABC_HERE)
		dev_dbg(gc->parent, "line %d: IRQ on both edges\n", offset);
#else /* MY_ABC_HERE */
		dev_dbg(gc->dev, "line %d: IRQ on both edges\n", offset);
#endif /* MY_ABC_HERE */
	} else if ((trigger & IRQ_TYPE_EDGE_RISING) ||
		   (trigger & IRQ_TYPE_EDGE_FALLING)) {
		bool rising = trigger & IRQ_TYPE_EDGE_RISING;

		/* Disable level detection */
		gpiois &= ~bit;
		/* Clear detection on both edges */
		gpioibe &= ~bit;
		/* Select edge */
		if (rising)
			gpioiev |= bit;
		else
			gpioiev &= ~bit;
		irq_set_handler_locked(d, handle_edge_irq);
#if defined(MY_ABC_HERE)
		dev_dbg(gc->parent, "line %d: IRQ on %s edge\n",
#else /* MY_ABC_HERE */
		dev_dbg(gc->dev, "line %d: IRQ on %s edge\n",
#endif /* MY_ABC_HERE */
			offset,
			rising ? "RISING" : "FALLING");
	} else {
		/* No trigger: disable everything */
		gpiois &= ~bit;
		gpioibe &= ~bit;
		gpioiev &= ~bit;
		irq_set_handler_locked(d, handle_bad_irq);
#if defined(MY_ABC_HERE)
		dev_warn(gc->parent, "no trigger selected for line %d\n",
#else /* MY_ABC_HERE */
		dev_warn(gc->dev, "no trigger selected for line %d\n",
#endif /* MY_ABC_HERE */
			 offset);
	}

	writeb(gpiois, chip->base + GPIOIS);
	writeb(gpioibe, chip->base + GPIOIBE);
	writeb(gpioiev, chip->base + GPIOIEV);

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static void pl061_irq_handler(struct irq_desc *desc)
{
	unsigned long pending;
	int offset;
	struct gpio_chip *gc = irq_desc_get_handler_data(desc);
	struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
	struct irq_chip *irqchip = irq_desc_get_chip(desc);

	chained_irq_enter(irqchip, desc);

	pending = readb(chip->base + GPIOMIS);
	if (pending) {
		for_each_set_bit(offset, &pending, PL061_GPIO_NR)
			generic_handle_irq(irq_find_mapping(gc->irqdomain,
							    offset));
	}

	chained_irq_exit(irqchip, desc);
}

static void pl061_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
	u8 mask = BIT(irqd_to_hwirq(d) % PL061_GPIO_NR);
	u8 gpioie;

	spin_lock(&chip->lock);
	gpioie = readb(chip->base + GPIOIE) & ~mask;
	writeb(gpioie, chip->base + GPIOIE);
	spin_unlock(&chip->lock);
}

static void pl061_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
	u8 mask = BIT(irqd_to_hwirq(d) % PL061_GPIO_NR);
	u8 gpioie;

	spin_lock(&chip->lock);
	gpioie = readb(chip->base + GPIOIE) | mask;
	writeb(gpioie, chip->base + GPIOIE);
	spin_unlock(&chip->lock);
}

/**
 * pl061_irq_ack() - ACK an edge IRQ
 * @d: IRQ data for this IRQ
 *
 * This gets called from the edge IRQ handler to ACK the edge IRQ
 * in the GPIOIC (interrupt-clear) register. For level IRQs this is
 * not needed: these go away when the level signal goes away.
 */
static void pl061_irq_ack(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
	u8 mask = BIT(irqd_to_hwirq(d) % PL061_GPIO_NR);

	spin_lock(&chip->lock);
	writeb(mask, chip->base + GPIOIC);
	spin_unlock(&chip->lock);
}

static struct irq_chip pl061_irqchip = {
	.name		= "pl061",
	.irq_ack	= pl061_irq_ack,
	.irq_mask	= pl061_irq_mask,
	.irq_unmask	= pl061_irq_unmask,
	.irq_set_type	= pl061_irq_type,
};

static int pl061_probe(struct amba_device *adev, const struct amba_id *id)
{
	struct device *dev = &adev->dev;
	struct pl061_platform_data *pdata = dev_get_platdata(dev);
	struct pl061_gpio *chip;
	int ret, irq, i, irq_base;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;

	if (pdata) {
		chip->gc.base = pdata->gpio_base;
		irq_base = pdata->irq_base;
		if (irq_base <= 0) {
			dev_err(&adev->dev, "invalid IRQ base in pdata\n");
			return -ENODEV;
		}
	} else {
		chip->gc.base = -1;
		irq_base = 0;
	}

	chip->base = devm_ioremap_resource(dev, &adev->res);
	if (IS_ERR(chip->base))
		return PTR_ERR(chip->base);

	spin_lock_init(&chip->lock);
	if (of_property_read_bool(dev->of_node, "gpio-ranges")) {
		chip->gc.request = gpiochip_generic_request;
		chip->gc.free = gpiochip_generic_free;
	}

	chip->gc.direction_input = pl061_direction_input;
	chip->gc.direction_output = pl061_direction_output;
	chip->gc.get = pl061_get_value;
	chip->gc.set = pl061_set_value;
	chip->gc.ngpio = PL061_GPIO_NR;
	chip->gc.label = dev_name(dev);
#if defined(MY_ABC_HERE)
	chip->gc.parent = dev;
#else /* MY_ABC_HERE */
	chip->gc.dev = dev;
#endif /* MY_ABC_HERE */
	chip->gc.owner = THIS_MODULE;

	ret = gpiochip_add(&chip->gc);
	if (ret)
		return ret;

	/*
	 * irq_chip support
	 */
	writeb(0, chip->base + GPIOIE); /* disable irqs */
	irq = adev->irq[0];
	if (irq < 0) {
		dev_err(&adev->dev, "invalid IRQ\n");
		return -ENODEV;
	}

	ret = gpiochip_irqchip_add(&chip->gc, &pl061_irqchip,
				   irq_base, handle_bad_irq,
				   IRQ_TYPE_NONE);
	if (ret) {
		dev_info(&adev->dev, "could not add irqchip\n");
		return ret;
	}
	gpiochip_set_chained_irqchip(&chip->gc, &pl061_irqchip,
				     irq, pl061_irq_handler);

	for (i = 0; i < PL061_GPIO_NR; i++) {
		if (pdata) {
			if (pdata->directions & (BIT(i)))
				pl061_direction_output(&chip->gc, i,
						pdata->values & (BIT(i)));
			else
				pl061_direction_input(&chip->gc, i);
		}
	}

	amba_set_drvdata(adev, chip);
	dev_info(&adev->dev, "PL061 GPIO chip @%pa registered\n",
		 &adev->res.start);

	return 0;
}

#ifdef CONFIG_PM
static int pl061_suspend(struct device *dev)
{
	struct pl061_gpio *chip = dev_get_drvdata(dev);
	int offset;

	chip->csave_regs.gpio_data = 0;
	chip->csave_regs.gpio_dir = readb(chip->base + GPIODIR);
	chip->csave_regs.gpio_is = readb(chip->base + GPIOIS);
	chip->csave_regs.gpio_ibe = readb(chip->base + GPIOIBE);
	chip->csave_regs.gpio_iev = readb(chip->base + GPIOIEV);
	chip->csave_regs.gpio_ie = readb(chip->base + GPIOIE);

	for (offset = 0; offset < PL061_GPIO_NR; offset++) {
		if (chip->csave_regs.gpio_dir & (BIT(offset)))
			chip->csave_regs.gpio_data |=
				pl061_get_value(&chip->gc, offset) << offset;
	}

	return 0;
}

static int pl061_resume(struct device *dev)
{
	struct pl061_gpio *chip = dev_get_drvdata(dev);
	int offset;

	for (offset = 0; offset < PL061_GPIO_NR; offset++) {
		if (chip->csave_regs.gpio_dir & (BIT(offset)))
			pl061_direction_output(&chip->gc, offset,
					chip->csave_regs.gpio_data &
					(BIT(offset)));
		else
			pl061_direction_input(&chip->gc, offset);
	}

	writeb(chip->csave_regs.gpio_is, chip->base + GPIOIS);
	writeb(chip->csave_regs.gpio_ibe, chip->base + GPIOIBE);
	writeb(chip->csave_regs.gpio_iev, chip->base + GPIOIEV);
	writeb(chip->csave_regs.gpio_ie, chip->base + GPIOIE);

	return 0;
}

static const struct dev_pm_ops pl061_dev_pm_ops = {
	.suspend = pl061_suspend,
	.resume = pl061_resume,
	.freeze = pl061_suspend,
	.restore = pl061_resume,
};
#endif

static struct amba_id pl061_ids[] = {
	{
		.id	= 0x00041061,
		.mask	= 0x000fffff,
	},
	{ 0, 0 },
};

MODULE_DEVICE_TABLE(amba, pl061_ids);

static struct amba_driver pl061_gpio_driver = {
	.drv = {
		.name	= "pl061_gpio",
#ifdef CONFIG_PM
		.pm	= &pl061_dev_pm_ops,
#endif
	},
	.id_table	= pl061_ids,
	.probe		= pl061_probe,
};

static int __init pl061_gpio_init(void)
{
	return amba_driver_register(&pl061_gpio_driver);
}
module_init(pl061_gpio_init);

MODULE_AUTHOR("Baruch Siach <baruch@tkos.co.il>");
MODULE_DESCRIPTION("PL061 GPIO driver");
MODULE_LICENSE("GPL");
