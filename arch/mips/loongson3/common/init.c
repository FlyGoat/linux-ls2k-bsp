/*
 * Copyright (C) 2009 Lemote Inc.
 * Author: Wu Zhangjin, wuzhangjin@gmail.com
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/bootmem.h>

#include <loongson.h>

#include <south-bridge.h>

extern struct plat_smp_ops loongson3_smp_ops;
extern void __init prom_init_numa_memory(void);

/* Loongson CPU address windows config space base address */
unsigned long __maybe_unused _loongson_addrwincfg_base;

struct south_bridge *ls_south_bridge;

void south_bridge_register(struct south_bridge *sb)
{
	ls_south_bridge = sb;
}

static int __init prom_arch_initcall(void)
{
	if (ls_south_bridge->sb_arch_initcall)
		ls_south_bridge->sb_arch_initcall();

	return 0;
}

static int __init prom_device_initcall(void)
{
	if (ls_south_bridge->sb_device_initcall)
		ls_south_bridge->sb_device_initcall();

	return 0;
}

void __init prom_init(void)
{
#ifdef CONFIG_CPU_SUPPORTS_ADDRWINCFG
	_loongson_addrwincfg_base = (unsigned long)
		ioremap(LOONGSON_ADDRWINCFG_BASE, LOONGSON_ADDRWINCFG_SIZE);
#endif

	prom_init_cmdline();
	prom_init_env();
#ifdef CONFIG_DMA_NONCOHERENT
	if(ls_south_bridge->sb_early_config)
		ls_south_bridge->sb_early_config();
#endif
	/* init base address of io space */
	set_io_port_base((unsigned long)
		ioremap(LOONGSON_PCIIO_BASE, LOONGSON_PCIIO_SIZE));

#ifdef CONFIG_NUMA
	prom_init_numa_memory();
#else
	prom_init_memory();
#endif

	/*init the uart base address */
	prom_init_uart_base();
#if defined(CONFIG_SMP)
	register_smp_ops(&loongson3_smp_ops);
#endif
}

void __init prom_free_prom_memory(void)
{
}

arch_initcall(prom_arch_initcall);
device_initcall(prom_device_initcall);
