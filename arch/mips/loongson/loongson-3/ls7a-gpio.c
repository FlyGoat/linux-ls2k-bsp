/*
 * =====================================================================================
 *
 *       Filename:  gpio.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  04/15/2017 10:37:08 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  hp (Huang Pei), huangpei@loongson.cn
 *        Company:  Loongson Corp.
 *
 * =====================================================================================
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <asm/types.h>
#include <loongson.h>
#include <linux/gpio.h>
#include <asm/gpio.h>

#define LS7A_GPIO_MAX 48
#define	LS7A_GPIO_BASE 16	/* GPIO 0 ~ 15 are used by 3A */

#define ls7a_readl(addr) (*(volatile unsigned int *)TO_UNCAC(addr))
#define ls7a_writel(val,addr) *(volatile unsigned int *)TO_UNCAC(addr) = (val)

unsigned int dc_base = 0;
unsigned int dvo1_base = 0;
unsigned int gpio_val_base = 0;
unsigned int gpio_dir_base = 0;

void init_base(void)
{
	dc_base = *(volatile unsigned int *)TO_UNCAC(0x1a003110);
	dc_base &= 0xfffffff0;
	dvo1_base = dc_base + 0x1250;
	gpio_val_base = dc_base + 0x1650;
	gpio_dir_base = dc_base + 0x1660;
}

int ls7a_gpio_get_val(unsigned gpio)
{
	if (gpio_val_base == 0)
		init_base();
	gpio -= LS7A_GPIO_BASE;
	return ls7a_readl(gpio_val_base) & (1 << gpio);
}
EXPORT_SYMBOL(ls7a_gpio_get_val);

void ls7a_gpio_set_val(unsigned gpio, int value)
{
	unsigned tmp;

	if (gpio_val_base == 0)
		init_base();
	gpio -= LS7A_GPIO_BASE;
	tmp = ls7a_readl(gpio_val_base) & ~(1 << gpio);
	if (value)
		tmp |= 1 << gpio;
	ls7a_writel(tmp, gpio_val_base);
}
EXPORT_SYMBOL(ls7a_gpio_set_val);

static const char *ls7a_gpio_list[LS7A_GPIO_MAX];

static int ls7a_gpio_request(struct gpio_chip *chip,  unsigned gpio)
{
	if (gpio >= LS7A_GPIO_MAX)
		return -EINVAL;

	return 0;
}

static void ls7a_gpio_free(struct gpio_chip *chip, unsigned gpio)
{
	ls7a_gpio_list[gpio] = NULL;
}

static inline int ls7a_gpio_get_value(struct gpio_chip *chip, unsigned gpio)
{
	if (gpio_val_base == 0)
		init_base();
	return ls7a_readl(gpio_val_base) & (1 << gpio);
}

static inline void ls7a_gpio_set_value(struct gpio_chip *chip, unsigned gpio, int value)
{
	unsigned long tmp;

	if (gpio_val_base == 0)
		init_base();
	tmp = ls7a_readl(gpio_val_base) & ~(1 << gpio);
	if (value)
		tmp |= 1ULL << gpio;
	ls7a_writel(tmp, gpio_val_base);
}

static inline int ls7a_gpio_direction_input(struct gpio_chip *chip, unsigned gpio)
{
	if (gpio_val_base == 0)
		init_base();
	if (gpio >= LS7A_GPIO_MAX)
		return -EINVAL;

	ls7a_writel(ls7a_readl(gpio_dir_base) | (1ULL << gpio), gpio_dir_base);

	return 0;
}

static inline int ls7a_gpio_direction_output(struct gpio_chip *chip, unsigned gpio, int value)
{
	if (gpio_val_base == 0)
		init_base();

	if (gpio >= LS7A_GPIO_MAX)
		return -EINVAL;

	ls7a_gpio_set_value(chip, gpio, value);
	ls7a_writel(ls7a_readl(gpio_dir_base) & ~(1ULL << gpio), gpio_dir_base);

	return 0;
}

static struct gpio_chip ls7a_gpio_chip = {
	.label			= "ls7a",
	.free			= ls7a_gpio_free,
	.request		= ls7a_gpio_request,
	.direction_input	= ls7a_gpio_direction_input,
	.get			= ls7a_gpio_get_value,
	.direction_output	= ls7a_gpio_direction_output,
	.set			= ls7a_gpio_set_value,
	.base			= LS7A_GPIO_BASE,
	.ngpio			= 48,
};

static int __init ls7a_gpio_setup(void)
{
	return gpiochip_add(&ls7a_gpio_chip);
}
arch_initcall(ls7a_gpio_setup);
