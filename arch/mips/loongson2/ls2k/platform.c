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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>
#include <linux/serial_8250.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/delay.h>
#include <linux/stmmac.h>
#include <linux/i2c.h>
#include <linux/phy.h>
#include <linux/dma-mapping.h>

#include <ls2k.h>
#include <asm/gpio.h>
#include <ls2k_int.h>
#include <linux/i2c-gpio.h>
#include <linux/mtd/spinand.h>
#include <linux/can/platform/sja1000.h>

/*
 * UART
 */
struct plat_serial8250_port ls2k_uart8250_data[] = {
	[0] = {
		.mapbase = CKSEG1ADDR(LS2K_UART0_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART0_REG_BASE),	.irq = LS2K_UART0_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[1] = {
		.mapbase = CKSEG1ADDR(LS2K_UART1_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART1_REG_BASE),	.irq = LS2K_UART0_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[2] = {
		.mapbase = CKSEG1ADDR(LS2K_UART2_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART2_REG_BASE),	.irq = LS2K_UART0_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[3] = {
		.mapbase = CKSEG1ADDR(LS2K_UART3_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART3_REG_BASE),	.irq = LS2K_UART0_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[4] = {
		.mapbase = CKSEG1ADDR(LS2K_UART4_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART4_REG_BASE),	.irq = LS2K_UART4_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[5] = {
		.mapbase = CKSEG1ADDR(LS2K_UART5_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART5_REG_BASE),	.irq = LS2K_UART4_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[6] = {
		.mapbase = CKSEG1ADDR(LS2K_UART6_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART6_REG_BASE),	.irq = LS2K_UART4_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[7] = {
		.mapbase = CKSEG1ADDR(LS2K_UART7_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART7_REG_BASE),	.irq = LS2K_UART4_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[8] = {
		.mapbase = CKSEG1ADDR(LS2K_UART8_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART8_REG_BASE),	.irq = LS2K_UART8_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[9] = {
		.mapbase = CKSEG1ADDR(LS2K_UART9_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART9_REG_BASE),	.irq = LS2K_UART8_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[10] = {
		.mapbase = CKSEG1ADDR(LS2K_UART10_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART10_REG_BASE),	.irq = LS2K_UART8_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	[11] = {
		.mapbase = CKSEG1ADDR(LS2K_UART11_REG_BASE),	.uartclk = 125000000,
		.membase = (void *)CKSEG1ADDR(LS2K_UART11_REG_BASE),	.irq = LS2K_UART8_IRQ,
		.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, 	.iotype = UPIO_MEM,
		.regshift = 0,
	},
	{}
};

static struct platform_device uart8250_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM1,
	.dev = {
		.platform_data = ls2k_uart8250_data,
	}
};

/*
 * NAND
 */
static struct mtd_partition ls2k_nand_partitions[]={
	[0] = {
		.name   = "kernel",
		.offset = 0,
		.size   = 0x01400000,
	},
	[1] = {
		.name   = "os",
		.offset = 0x01400000,
		.size   = 0x0,

	},
};

static struct ls2k_nand_plat_data ls2k_nand_parts = {
        .enable_arbiter = 1,
        .parts          = ls2k_nand_partitions,
        .nr_parts       = ARRAY_SIZE(ls2k_nand_partitions),
	.chip_ver	= LS2K_VER3,
	.cs = 2,
	.csrdy = 0x11<<(2*9),
};

static struct resource ls2k_nand_resources[] = {
	[0] = {
		.start      = LS2K_NAND_DMA_ACC_REG,
		.end        = LS2K_NAND_DMA_ACC_REG,
		.flags      = IORESOURCE_DMA,
	},
	[1] = {
		.start      = LS2K_NAND_REG_BASE,
		.end        = LS2K_NAND_REG_BASE + 0x20,
		.flags      = IORESOURCE_MEM,
	},
	[2] = {
		.start      = LS2K_DMA_ORDER_REG,
		.end        = LS2K_DMA_ORDER_REG,
		.flags      = IORESOURCE_MEM,
	},
	[3] = {
		.start      = LS2K_DMA0_IRQ,
		.end        = LS2K_DMA0_IRQ,
		.flags      = IORESOURCE_IRQ,
	},
};

struct platform_device ls2k_nand_device = {
	.name       = "ls2k-nand",
	.id         = 0,
	.dev        = {
		.platform_data = &ls2k_nand_parts,
	},
	.num_resources  = ARRAY_SIZE(ls2k_nand_resources),
	.resource       = ls2k_nand_resources,
};

/*
 * I2C
 */
static struct resource ls2k_i2c0_resources[] = {
	[0] = {
		.start = LS2K_I2C0_REG_BASE,
		.end   = LS2K_I2C0_REG_BASE + 0x8,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = LS2K_I2C0_IRQ,
		.end   = LS2K_I2C0_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device ls2k_i2c0_device = {
	.name           = "ls2k-i2c",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(ls2k_i2c0_resources),
	.resource       = ls2k_i2c0_resources,
};

static struct resource ls2k_i2c1_resources[] = {
	[0] = {
		.start = LS2K_I2C1_REG_BASE,
		.end   = LS2K_I2C1_REG_BASE + 0x8,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = LS2K_I2C1_IRQ,
		.end   = LS2K_I2C1_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device ls2k_i2c1_device = {
	.name           = "ls2k-i2c",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(ls2k_i2c1_resources),
	.resource       = ls2k_i2c1_resources,
};

/*
 * RTC
 */
static struct resource ls2k_rtc_resources[] = {
       [0] = {
               .start  = LS2K_RTC_REG_BASE,
               .end    = (LS2K_RTC_REG_BASE + 0xff),
               .flags  = IORESOURCE_MEM,
       },
       [1] = {
               .start  = LS2K_RTC_INT0_IRQ,
               .end    = LS2K_RTC_INT0_IRQ,
               .flags  = IORESOURCE_IRQ,
       },
};

static struct platform_device ls2k_rtc_device = {
       .name   = "ls2k-rtc",
       .id     = 0,
       .num_resources  = ARRAY_SIZE(ls2k_rtc_resources),
       .resource       = ls2k_rtc_resources,
};


/*
 * GPIO-I2C
 */
static struct i2c_gpio_platform_data dvo0_pdata = {
	.sda_pin                = LS2K_GPIO_PIN_1,
	.sda_is_open_drain      = 0,
	.scl_pin                = LS2K_GPIO_PIN_0,
	.scl_is_open_drain      = 0,
	.udelay                 = 100,
};

static struct platform_device ls2k_dvo0_gpio_i2c_device = {
	.name                   = "i2c-gpio",
	.id                     = 2,
	.num_resources          = 0,
	.resource               = NULL,
	.dev                    = {
		.platform_data  = &dvo0_pdata,
	},
};

static struct i2c_gpio_platform_data dvo1_pdata = {
	.sda_pin                = LS2K_GPIO_PIN_32,
	.sda_is_open_drain      = 0,
	.scl_pin                = LS2K_GPIO_PIN_33,
	.scl_is_open_drain      = 0,
	.udelay                 = 100,
};

static struct platform_device ls2k_dvo1_gpio_i2c_device = {
	.name                   = "i2c-gpio",
	.id                     = 3,
	.num_resources          = 0,
	.resource               = NULL,
	.dev                    = {
		.platform_data  = &dvo1_pdata,
	},
};

/*** sdio ***/
static struct resource ls2k_sdio_resources[] = { 
	[0] = {
		.start = LS2K_SDIO_REG_BASE,
		.end   = (LS2K_SDIO_REG_BASE + 0xfff),
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = LS2K_SDIO_IRQ,
		.end   = LS2K_SDIO_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start      = LS2K_DMA_ORDER_REG+0x10,
		.end        = LS2K_DMA_ORDER_REG+0x10,
		.flags      = IORESOURCE_MEM,
	},
};


static struct platform_device ls2k_sdio_device = {
	.name           = "ls2k_sdio",
	.id             = 0,
	.dev = {
		.dma_mask = &ls2k_sdio_device.dev.coherent_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data = 0,
	},
	.num_resources  = ARRAY_SIZE(ls2k_sdio_resources),
	.resource       = ls2k_sdio_resources,
};

static struct resource ls2k_spi0_resources[]={
	[0]={
		.start      =   LS2K_SPI_REG_BASE,
		.end        =   LS2K_SPI_REG_BASE+6,
		.flags      =   IORESOURCE_MEM,
	},  
};

static struct platform_device ls2k_spi0_device={
	.name   =   "ls2k-spi",
	.id     =       0,
	.num_resources  =ARRAY_SIZE(ls2k_spi0_resources),
	.resource       =ls2k_spi0_resources,
};


static struct mtd_partition ls2h_spi_parts[] = {
	[0] = {
		.name		= "spinand0",
		.offset		= 0,
		.size		= 32 * 1024 * 1024,
	},
	[1] = {
		.name		= "spinand1",
		.offset		= 32 * 1024 * 1024,
		.size		= 0,	
	},
};

struct spi_nand_platform_data ls2h_spi_nand = {
	.name		= "LS_Nand_Flash",
	.parts		= ls2h_spi_parts,
	.nr_parts	= ARRAY_SIZE(ls2h_spi_parts),
};


static struct mtd_partition partitions[] = {
	[0] = {
#if 1
		.name		= "pmon",
		.offset		= 0,//512 * 1024,
		.size		= 1024 * 1024,	//512KB
	//	.mask_flags	= MTD_WRITEABLE,
	},
	[1] = {
#endif
		.name		= "data",
		.offset		= 1024 * 1024,
		.size		= 14 * 1024 * 1024,	//3.5MB
	//	.mask_flags	= MTD_WRITEABLE,
	},
};

struct flash_platform_data flash = {
	.name		= "m25p80",
	.parts		= partitions,
	.nr_parts	= ARRAY_SIZE(partitions),
	.type		= "gd25q128",
};



static struct spi_board_info ls2h_spi_devices[] = {
	{	/* spi nand Flash chip */
		.modalias	= "mt29f",	
		.bus_num 		= 0,
		.chip_select	= 1,
		.max_speed_hz	= 20000000,
		.platform_data	= &ls2h_spi_nand,
	},

	{	/* DataFlash chip */
		.modalias	= "m25p80",		//"m25p80",
		.bus_num 		= 0,
		.chip_select	= 0,
		.max_speed_hz	= 40000000,
	//	.platform_data	= &flash,
	},
};

/*
 * CAN
 */
static struct resource ls2k_can0_resources[] = {
	{
		.start   = LS2K_CAN0_REG_BASE,
		.end     = LS2K_CAN0_REG_BASE+0xff,
		.flags   = IORESOURCE_MEM,
	}, {
		.start   = LS2K_CAN0_IRQ,
		.end     = LS2K_CAN0_IRQ,
		.flags   = IORESOURCE_IRQ,
	},
};
static struct resource ls2k_can1_resources[] = {
	{
		.start   = LS2K_CAN1_REG_BASE,
		.end     = LS2K_CAN1_REG_BASE+0xff,
		.flags   = IORESOURCE_MEM,
	}, {
		.start   = LS2K_CAN1_IRQ,
		.end     = LS2K_CAN1_IRQ,
		.flags   = IORESOURCE_IRQ,
	},
};

struct sja1000_platform_data ls2k_sja1000_platform_data = {
	.osc_freq	= 125000000,
	.ocr		= 0x40 | 0x18,
	.cdr		= 0x40,
};

static struct platform_device ls2k_can0_device = {
	.name = "sja1000_platform",
	.id = 1,
	.dev = {
		.platform_data = &ls2k_sja1000_platform_data,
	},
	.resource = ls2k_can0_resources,
	.num_resources = ARRAY_SIZE(ls2k_can0_resources),
};
static struct platform_device ls2k_can1_device = {
	.name = "sja1000_platform",
	.id = 2,
	.dev = {
		.platform_data = &ls2k_sja1000_platform_data,
	},
	.resource = ls2k_can1_resources,
	.num_resources = ARRAY_SIZE(ls2k_can1_resources),
};

/*
 * I2S_UDA1342
 */
#ifdef CONFIG_SOUND_LS2K_UDA1342
static struct resource ls2k_uda1342_resource[] = {
       [0]={
               .start    = LS2K_I2S_REG_BASE,
               .end      = (LS2K_I2S_REG_BASE + 0x10),
               .flags    = IORESOURCE_MEM,
       },
       [1] = {
               .start    = LS2K_DMA2_IRQ,
               .end      = LS2K_DMA2_IRQ,
               .flags    = IORESOURCE_IRQ,
       },
       [2] = {
               .start    = LS2K_DMA3_IRQ,
               .end      = LS2K_DMA3_IRQ,
               .flags    = IORESOURCE_IRQ,
       },
       [3] = {
               .start    = LS2K_DMA2_REG,
               .end      = LS2K_DMA2_REG,
               .flags    = IORESOURCE_DMA,
       },
       [4] = {
               .start    = LS2K_DMA3_REG,
               .end      = LS2K_DMA3_REG,
               .flags    = IORESOURCE_DMA,
       },
};

static struct platform_device ls2k_audio_device = {
       .name           = "ls2k-audio",
       .id             = -1,
       .num_resources  = ARRAY_SIZE(ls2k_uda1342_resource),
       .resource       = ls2k_uda1342_resource,
};
#endif





static struct platform_device *ls2k_platform_devices[] = {
	&uart8250_device,
	&ls2k_can0_device,
	&ls2k_can1_device,
	&ls2k_spi0_device,
	&ls2k_sdio_device,
	&ls2k_nand_device,
	&ls2k_i2c0_device,
	&ls2k_i2c1_device,
	&ls2k_dvo0_gpio_i2c_device,
	&ls2k_dvo1_gpio_i2c_device,
	&ls2k_rtc_device,
#ifdef CONFIG_SOUND_LS2K_UDA1342
        &ls2k_audio_device,
#endif
};

const struct i2c_board_info __initdata ls2k_fb_eep_info = {
	I2C_BOARD_INFO("eeprom-edid", 0x50),
};

const struct i2c_board_info __initdata ls2k_dvi_fb_eep_info = {
	I2C_BOARD_INFO("dvi-eeprom-edid", 0x50),
};


const struct i2c_board_info __initdata ls2k_codec_uda1342_info = {
	I2C_BOARD_INFO("codec_uda1342", 0x1a),
};

const struct i2c_board_info __initdata ls2k_ds1338_info = {
	I2C_BOARD_INFO("ds1338", 0x68),
};


int ls2k_platform_init(void)
{

#define I2C_BUS_0 0
#define I2C_BUS_1 1

	i2c_register_board_info(I2C_BUS_1, &ls2k_ds1338_info, 1);
	i2c_register_board_info(I2C_BUS_1, &ls2k_fb_eep_info, 1);
	i2c_register_board_info(2, &ls2k_dvi_fb_eep_info, 1);
	/*sdio use dma1*/
	ls2k_writel((ls2k_readl(LS2K_APBDMA_CONFIG_REG)&~(7<<15))|(1<<15), LS2K_APBDMA_CONFIG_REG);
	/*enable can pin*/
	ls2k_writel(ls2k_readl(LS2K_GEN_CONFIG0_REG)|(3<<16), LS2K_GEN_CONFIG0_REG);

	spi_register_board_info(ls2h_spi_devices, ARRAY_SIZE(ls2h_spi_devices));
#ifdef CONFIG_SOUND_LS2K_UDA1342
	i2c_register_board_info(1, &ls2k_codec_uda1342_info, 1);
#endif
	return platform_add_devices(ls2k_platform_devices,
			ARRAY_SIZE(ls2k_platform_devices));
}

device_initcall(ls2k_platform_init);
