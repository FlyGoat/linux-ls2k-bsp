/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2005 Embedded Alley Solutions, Inc
 * Copyright (C) 2005 Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2009 Jiajie Chen (chenjiajie@cse.buaa.edu.cn)
 */
#ifndef __ASM_MACH_LOONGSON3_KERNEL_ENTRY_H
#define __ASM_MACH_LOONGSON3_KERNEL_ENTRY_H

/*
 * Override macros used in arch/mips/kernel/head.S.
 */
	.macro	kernel_entry_setup
#ifdef CONFIG_NUMA
	.set	push
	.set	mips64
#ifdef CONFIG_CPU_LOONGSON3
	/* enable stfill buffer */
	mfc0	t0, $16, 6
	or	t0, 0x100
	mtc0	t0, $16, 6
#endif
	/* Set LPA on LOONGSON3 config3 */
	mfc0	t0, $16, 3
	or	t0, (0x1 << 7)
	mtc0	t0, $16, 3
	/* Set ELPA on LOONGSON3 pagegrain */
	mfc0	t0, $5, 1
	or	t0, (0x1 << 29)
	mtc0	t0, $5, 1
	_ehb
	.set	pop
#endif
	.endm

/*
 * Do SMP slave processor setup.
 */
	.macro	smp_slave_setup
#ifdef CONFIG_NUMA
	.set	push
	.set	mips64
#ifdef CONFIG_CPU_LOONGSON3
	/* enable stfill buffer */
	mfc0	t0, $16, 6
	or	t0, 0x100
	mtc0	t0, $16, 6
#endif
	/* Set LPA on LOONGSON3 config3 */
	mfc0	t0, $16, 3
	or	t0, (0x1 << 7)
	mtc0	t0, $16, 3
	/* Set ELPA on LOONGSON3 pagegrain */
	mfc0	t0, $5, 1
	or	t0, (0x1 << 29)
	mtc0	t0, $5, 1
	_ehb
	.set	pop
#endif
	.endm

#endif /* __ASM_MACH_LOONGSON3_KERNEL_ENTRY_H */
