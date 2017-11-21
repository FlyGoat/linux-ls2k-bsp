/*
 *  Copyright (C) 2013, Loongson Technology Corporation Limited, Inc.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 */
#include <linux/init.h>
#include <asm/io.h>
#include <boot_param.h>
#include <loongson-pch.h>

#include <linux/serial_8250.h>
#include <linux/platform_device.h>

#include <linux/i2c.h>
#include <linux/i2c-gpio.h>

extern void ls7a_init_irq(void);
extern void ls7a_irq_dispatch(void);
extern int ls2h_platform_init(void);

extern int ls7a_pcibios_map_irq(struct pci_dev *dev, u8 slot, u8 pin);
extern int ls7a_pcibios_dev_init(struct pci_dev *dev);

/*
 * UART
 */
struct plat_serial8250_port ls7a_uart8250_data[] = {
	[0] = {
		.mapbase = CKSEG1ADDR(LS7A_UART0_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS7A_UART0_REG_BASE),	.irq = LS7A_IOAPIC_UART0_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[1] = {
		.mapbase = CKSEG1ADDR(LS7A_UART1_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS7A_UART1_REG_BASE),	.irq = LS7A_IOAPIC_UART0_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[2] = {
		.mapbase = CKSEG1ADDR(LS7A_UART2_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS7A_UART2_REG_BASE),	.irq = LS7A_IOAPIC_UART0_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[3] = {
		.mapbase = CKSEG1ADDR(LS7A_UART3_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS7A_UART3_REG_BASE),	.irq = LS7A_IOAPIC_UART0_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	{}
};

static struct platform_device uart8250_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM1,
	.dev = {
		.platform_data = ls7a_uart8250_data,
	}
};

/*
 * I2C
 */
static struct resource ls7a_i2c0_resources[] = {
	[0] = {
		.start = LS7A_I2C0_REG_BASE,
		.end   = LS7A_I2C0_REG_BASE + 0x8,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = LS7A_IOAPIC_I2C0_IRQ,
		.end   = LS7A_IOAPIC_I2C0_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device ls7a_i2c0_device = {
	.name           = "ls7a-i2c",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(ls7a_i2c0_resources),
	.resource       = ls7a_i2c0_resources,
};

static struct resource ls7a_i2c1_resources[] = {
	[0] = {
		.start = LS7A_I2C1_REG_BASE,
		.end   = LS7A_I2C1_REG_BASE + 0x8,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = LS7A_IOAPIC_I2C0_IRQ,
		.end   = LS7A_IOAPIC_I2C0_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device ls7a_i2c1_device = {
	.name           = "ls7a-i2c",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(ls7a_i2c1_resources),
	.resource       = ls7a_i2c1_resources,
};

static struct resource ls7a_i2c2_resources[] = {
	[0] = {
		.start = LS7A_I2C2_REG_BASE,
		.end   = LS7A_I2C2_REG_BASE + 0x8,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = LS7A_IOAPIC_I2C0_IRQ,
		.end   = LS7A_IOAPIC_I2C0_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device ls7a_i2c2_device = {
	.name           = "ls7a-i2c",
	.id             = 2,
	.num_resources  = ARRAY_SIZE(ls7a_i2c2_resources),
	.resource       = ls7a_i2c2_resources,
};

static struct resource ls7a_i2c3_resources[] = {
	[0] = {
		.start = LS7A_I2C3_REG_BASE,
		.end   = LS7A_I2C3_REG_BASE + 0x8,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = LS7A_IOAPIC_I2C0_IRQ,
		.end   = LS7A_IOAPIC_I2C0_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device ls7a_i2c3_device = {
	.name           = "ls7a-i2c",
	.id             = 3,
	.num_resources  = ARRAY_SIZE(ls7a_i2c3_resources),
	.resource       = ls7a_i2c3_resources,
};

static struct resource ls7a_i2c4_resources[] = {
	[0] = {
		.start = LS7A_I2C4_REG_BASE,
		.end   = LS7A_I2C4_REG_BASE + 0x8,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = LS7A_IOAPIC_I2C0_IRQ,
		.end   = LS7A_IOAPIC_I2C0_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device ls7a_i2c4_device = {
	.name           = "ls7a-i2c",
	.id             = 4,
	.num_resources  = ARRAY_SIZE(ls7a_i2c4_resources),
	.resource       = ls7a_i2c4_resources,
};

static struct resource ls7a_i2c5_resources[] = {
	[0] = {
		.start = LS7A_I2C5_REG_BASE,
		.end   = LS7A_I2C5_REG_BASE + 0x8,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = LS7A_IOAPIC_I2C0_IRQ,
		.end   = LS7A_IOAPIC_I2C0_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device ls7a_i2c5_device = {
	.name           = "ls7a-i2c",
	.id             = 5,
	.num_resources  = ARRAY_SIZE(ls7a_i2c5_resources),
	.resource       = ls7a_i2c5_resources,
};

/*
 * RTC
 */
static struct resource ls7a_rtc_resources[] = {
	[0] = {
		.start  = LS7A_RTC_REG_BASE,
		.end    = (LS7A_RTC_REG_BASE + 0xff),
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = LS7A_IOAPIC_RTC_INT0_IRQ,
		.end    = LS7A_IOAPIC_RTC_INT0_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device ls7a_rtc_device = {
	.name   = "ls7a-rtc",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(ls7a_rtc_resources),
	.resource       = ls7a_rtc_resources,
};


/*
 * I2C-GPIO
 */
static struct i2c_gpio_platform_data i2c_gpio_pdata = {
	.sda_pin		= 2 + 16,
	.sda_is_open_drain	= 0,
	.scl_pin		= 3 + 16,
	.scl_is_open_drain	= 0,
	.udelay			= 100,
};

static struct platform_device ls7a_i2c_gpio_device = {
	.name			= "i2c-gpio",
	.id			= 6,
	.num_resources		= 0,
	.resource		= NULL,
	.dev			= {
		.platform_data  = &i2c_gpio_pdata,
	},
};

/* platform devices define */
static struct platform_device *ls7a_platform_devices[] = {
	&uart8250_device,
	&ls7a_i2c0_device,
	&ls7a_i2c1_device,
	&ls7a_i2c2_device,
	&ls7a_i2c3_device,
	&ls7a_i2c4_device,
	&ls7a_i2c5_device,
	&ls7a_rtc_device,

	&ls7a_i2c_gpio_device,
};


static void ls7a_early_config(void)
{
}

static void __init ls7a_arch_initcall(void)
{
}

const struct i2c_board_info __initdata ls7a_fb_ch7034_eep_info = {
	I2C_BOARD_INFO("dvo1-ch7034", 0x75),
};

const struct i2c_board_info __initdata ls7a_fb_edid_eep_info = {
	I2C_BOARD_INFO("eeprom-edid", 0x50),
};

static void __init ls7a_device_initcall(void)
{
	i2c_register_board_info(6, &ls7a_fb_ch7034_eep_info, 1);
	i2c_register_board_info(6, &ls7a_fb_edid_eep_info, 1);

	platform_add_devices(ls7a_platform_devices,
			ARRAY_SIZE(ls7a_platform_devices));
}

const struct platform_controller_hub ls7a_pch = {
	.board_type		= LS7A,
	.pcidev_max_funcs 	= 7,
	.early_config		= ls7a_early_config,
	.init_irq		= ls7a_init_irq,
	.irq_dispatch		= ls7a_irq_dispatch,
	.pcibios_map_irq	= ls7a_pcibios_map_irq,
	.pcibios_dev_init	= ls7a_pcibios_dev_init,
	.pch_arch_initcall	= ls7a_arch_initcall,
	.pch_device_initcall	= ls7a_device_initcall,
};
