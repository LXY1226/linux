#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * MOXA ART SoCs GPIO driver.
 *
 * Copyright (C) 2013 Jonas Jensen
 *
 * Jonas Jensen <jonas.jensen@gmail.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/bitops.h>
#include <linux/basic_mmio_gpio.h>

#define GPIO_DATA_OUT		0x00
#define GPIO_DATA_IN		0x04
#define GPIO_PIN_DIRECTION	0x08

static int moxart_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct bgpio_chip *bgc;
	void __iomem *base;
	int ret;

	bgc = devm_kzalloc(dev, sizeof(*bgc), GFP_KERNEL);
	if (!bgc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	ret = bgpio_init(bgc, dev, 4, base + GPIO_DATA_IN,
			 base + GPIO_DATA_OUT, NULL,
			 base + GPIO_PIN_DIRECTION, NULL,
			 BGPIOF_READ_OUTPUT_REG_SET);
	if (ret) {
		dev_err(&pdev->dev, "bgpio_init failed\n");
		return ret;
	}

	bgc->gc.label = "moxart-gpio";
	bgc->gc.request = gpiochip_generic_request;
	bgc->gc.free = gpiochip_generic_free;
	bgc->data = bgc->read_reg(bgc->reg_set);
	bgc->gc.base = 0;
	bgc->gc.ngpio = 32;
#if defined(MY_ABC_HERE)
	bgc->gc.parent = dev;
#else /* MY_ABC_HERE */
	bgc->gc.dev = dev;
#endif /* MY_ABC_HERE */
	bgc->gc.owner = THIS_MODULE;

	ret = gpiochip_add(&bgc->gc);
	if (ret) {
		dev_err(dev, "%s: gpiochip_add failed\n",
			dev->of_node->full_name);
		return ret;
	}

	return ret;
}

static const struct of_device_id moxart_gpio_match[] = {
	{ .compatible = "moxa,moxart-gpio" },
	{ }
};

static struct platform_driver moxart_gpio_driver = {
	.driver	= {
		.name		= "moxart-gpio",
		.of_match_table	= moxart_gpio_match,
	},
	.probe	= moxart_gpio_probe,
};
module_platform_driver(moxart_gpio_driver);

MODULE_DESCRIPTION("MOXART GPIO chip driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jonas Jensen <jonas.jensen@gmail.com>");
