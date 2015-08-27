/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2006, 07  Ralf Baechle <ralf@linux-mips.org>
 * Copyright (C) 2007 Lemote, Inc. & Institute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 */
#ifndef __ASM_MACH_LOONGSON3_DMA_COHERENCE_H
#define __ASM_MACH_LOONGSON3_DMA_COHERENCE_H

#ifdef CONFIG_SWIOTLB
#include <linux/swiotlb.h>
#endif

struct device;

extern dma_addr_t phys_to_dma(struct device *dev, phys_addr_t paddr);
extern phys_addr_t dma_to_phys(struct device *dev, dma_addr_t daddr);
static inline dma_addr_t plat_map_dma_mem(struct device *dev, void *addr,
					  size_t size)
{
#ifdef CONFIG_CPU_LOONGSON3
	long nid;
	dma_addr_t daddr;

	daddr = virt_to_phys(addr) < 0x10000000 ?
			(virt_to_phys(addr) | 0x0000000080000000) : virt_to_phys(addr);
#ifdef CONFIG_PHYS48_TO_HT40
	 /* We extract 2bit node id (bit 44~47, only bit 44~45 used now) from
	  * Loongson3's 48bit address space and embed it into 40bit */
	nid = (daddr >> 44) & 0x3;
	daddr = ((nid << 44 ) ^ daddr) | (nid << 37);
#endif
	return daddr;
#else
	return virt_to_phys(addr) | 0x80000000;
#endif
}

static inline dma_addr_t plat_map_dma_mem_page(struct device *dev,
					       struct page *page)
{
#ifdef CONFIG_CPU_LOONGSON3
	long nid;
	dma_addr_t daddr;

	daddr = page_to_phys(page) < 0x10000000 ?
			(page_to_phys(page) | 0x0000000080000000) : page_to_phys(page);
#ifdef CONFIG_PHYS48_TO_HT40
	 /* We extract 2bit node id (bit 44~47, only bit 44~45 used now) from
	  * Loongson3's 48bit address space and embed it into 40bit */
	nid = (daddr >> 44) & 0x3;
	daddr = ((nid << 44 ) ^ daddr) | (nid << 37);
#endif
	return daddr;
#else
	return page_to_phys(page) | 0x80000000;
#endif
}

static inline unsigned long plat_dma_addr_to_phys(struct device *dev,
	dma_addr_t dma_addr)
{
#if defined(CONFIG_CPU_LOONGSON2F) && defined(CONFIG_64BIT)
	return (dma_addr > 0x8fffffff) ? dma_addr : (dma_addr & 0x0fffffff);
#elif defined(CONFIG_CPU_LOONGSON3) && defined(CONFIG_64BIT)
	long nid;

	dma_addr = (dma_addr < 0x90000000 && dma_addr >= 0x80000000) ?
			(dma_addr & 0x0fffffff) : dma_addr;
#ifdef CONFIG_PHYS48_TO_HT40
	nid = (dma_addr >> 37) & 0x3;
	dma_addr = ((nid << 37 ) ^ dma_addr) | (nid << 44);
#endif
	return dma_addr;
#else
	return dma_addr & 0x7fffffff;
#endif
}

static inline void plat_unmap_dma_mem(struct device *dev, dma_addr_t dma_addr,
	size_t size, enum dma_data_direction direction)
{
}

static inline int plat_dma_supported(struct device *dev, u64 mask)
{
	/*
	 * we fall back to GFP_DMA when the mask isn't all 1s,
	 * so we can't guarantee allocations that must be
	 * within a tighter range than GFP_DMA..
	 */
	if (mask < DMA_BIT_MASK(24))
		return 0;

	return 1;
}

static inline void plat_extra_sync_for_device(struct device *dev)
{
}

static inline int plat_dma_mapping_error(struct device *dev,
					 dma_addr_t dma_addr)
{
	return 0;
}

static inline int plat_device_is_coherent(struct device *dev)
{
#ifdef CONFIG_DMA_NONCOHERENT
	return 0;
#else
	return 1;
#endif /* CONFIG_DMA_NONCOHERENT */
}

#endif /* __ASM_MACH_LOONGSON3_DMA_COHERENCE_H */
