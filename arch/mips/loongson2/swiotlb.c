#include <linux/mm.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/swiotlb.h>
#include <linux/bootmem.h>

#include <asm/bootinfo.h>
#include <asm/dma-mapping.h>
#include <dma-coherence.h>

static inline void *dma_addr_to_virt(struct device *dev,
	dma_addr_t dma_addr)
{
	unsigned long addr = plat_dma_addr_to_phys(dev, dma_addr);

	return phys_to_virt(addr);
}

static void *loongson_dma_alloc_coherent(struct device *dev, size_t size,
				dma_addr_t *dma_handle, gfp_t gfp, struct dma_attrs *attrs)
{
	void *ret;

	if (dma_alloc_from_coherent(dev, size, dma_handle, &ret))
		return ret;

	/* ignore region specifiers */
	gfp &= ~(__GFP_DMA | __GFP_DMA32 | __GFP_HIGHMEM);

#ifdef CONFIG_ZONE_DMA
	if (dev == NULL)
		gfp |= __GFP_DMA;
	else if (dev->coherent_dma_mask <= DMA_BIT_MASK(24))
		gfp |= __GFP_DMA;
	else
#endif
#ifdef CONFIG_ZONE_DMA32
	if (dev == NULL)
		gfp |= __GFP_DMA32;
	else if (dev->coherent_dma_mask <= DMA_BIT_MASK(32))
		gfp |= __GFP_DMA32;
	else
#endif
	;
	gfp |= __GFP_NORETRY | __GFP_NOWARN;

	ret = swiotlb_alloc_coherent(dev, size, dma_handle, gfp);
	if(ret)
	{
		if (!plat_device_is_coherent(dev)) {
			dma_cache_sync(dev, dma_addr_to_virt(dev,
					*dma_handle), size, DMA_TO_DEVICE);
			ret = UNCAC_ADDR(ret);
		}
	}

	mb();
	return ret;
}

static void loongson_dma_free_coherent(struct device *dev, size_t size,
				void *vaddr, dma_addr_t daddr, struct dma_attrs *attrs)
{
	int order = get_order(size);

	if (dma_release_from_coherent(dev, order, vaddr))
		return;

	if (!plat_device_is_coherent(dev)) {
		vaddr = CAC_ADDR(vaddr);
		dma_cache_sync(dev, dma_addr_to_virt(dev,
				daddr), size, DMA_FROM_DEVICE);
	}

	swiotlb_free_coherent(dev, size, vaddr, daddr);
}

static dma_addr_t loongson_dma_map_page(struct device *dev, struct page *page,
				unsigned long offset, size_t size,
				enum dma_data_direction dir,
				struct dma_attrs *attrs)
{
	dma_addr_t daddr = swiotlb_map_page(dev, page, offset, size,
					dir, attrs);
	if (!plat_device_is_coherent(dev)) {
		dma_cache_sync(dev, dma_addr_to_virt(dev,
				daddr), size, dir);
	}
	mb();
	return daddr;
}

static void loongson_dma_unmap_page(struct device *dev, dma_addr_t daddr,
			size_t size, enum dma_data_direction dir,
			struct dma_attrs *attrs)
{
	if (!plat_device_is_coherent(dev))
		dma_cache_sync(dev, dma_addr_to_virt(dev, daddr), size,
				dir);
	swiotlb_unmap_page(dev, daddr, size, dir, attrs);
}

static int loongson_dma_map_sg(struct device *dev, struct scatterlist *sgl,
				int nents, enum dma_data_direction dir,
				struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i;
	int r = swiotlb_map_sg_attrs(dev, sgl, nents, dir, NULL);

	if (!plat_device_is_coherent(dev))
	{
		for_each_sg(sgl, sg, nents, i) {
			dma_cache_sync(dev, dma_addr_to_virt(dev,
					sg_dma_address(sg)), sg_dma_len(sg), dir);
		}
	}
	mb();

	return r;
}

static void loongson_dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sgl,
			int nelems, enum dma_data_direction dir,
			struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i;
	if (!plat_device_is_coherent(dev) &&
			dir != DMA_TO_DEVICE) {
		for_each_sg(sgl, sg, nelems, i) {
				dma_cache_sync(dev, dma_addr_to_virt(dev,
						sg_dma_address(sg)),sg_dma_len(sg), dir);
		}
	}

	swiotlb_unmap_sg_attrs(dev, sgl, nelems, dir, attrs);
}

static void loongson_dma_sync_single_for_cpu(struct device *dev, dma_addr_t daddr,
			    size_t size, enum dma_data_direction dir)
{
	if (!plat_device_is_coherent(dev))
		dma_cache_sync(dev, dma_addr_to_virt(dev, daddr), size, dir);
	swiotlb_sync_single_for_cpu(dev, daddr, size, dir);
}

static void loongson_dma_sync_single_for_device(struct device *dev,
				dma_addr_t daddr, size_t size,
				enum dma_data_direction dir)
{
	swiotlb_sync_single_for_device(dev, daddr, size, dir);
	if (!plat_device_is_coherent(dev))
		dma_cache_sync(dev, dma_addr_to_virt(dev, daddr), size, dir);
	mb();
}

static void loongson_dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sgl,
			int nelems, enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	if (!plat_device_is_coherent(dev))
	{
		for_each_sg(sgl, sg, nelems, i) {
			dma_cache_sync(dev, dma_addr_to_virt(dev,
					sg_dma_address(sg)), sg_dma_len(sg), dir);
		}
	}

	swiotlb_sync_sg_for_cpu(dev, sgl, nelems, dir);
}

static void loongson_dma_sync_sg_for_device(struct device *dev,
				struct scatterlist *sgl, int nents,
				enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	swiotlb_sync_sg_for_device(dev, sgl, nents, dir);
	if (!plat_device_is_coherent(dev))
	{
		for_each_sg(sgl, sg, nents, i) {
			dma_cache_sync(dev, dma_addr_to_virt(dev,
					sg_dma_address(sg)), sg_dma_len(sg), dir);
		}
	}
	mb();
}

static struct dma_map_ops loongson2_linear_dma_map_ops = {
		.alloc = loongson_dma_alloc_coherent,
		.free = loongson_dma_free_coherent,
		.map_page = loongson_dma_map_page,
		.unmap_page = loongson_dma_unmap_page,
		.map_sg = loongson_dma_map_sg,
		.unmap_sg = loongson_dma_unmap_sg_attrs,
		.sync_single_for_cpu = loongson_dma_sync_single_for_cpu,
		.sync_single_for_device = loongson_dma_sync_single_for_device,
		.sync_sg_for_cpu = loongson_dma_sync_sg_for_cpu,
		.sync_sg_for_device = loongson_dma_sync_sg_for_device,
		.mapping_error = swiotlb_dma_mapping_error,
		.dma_supported = swiotlb_dma_supported,
};

void __init plat_swiotlb_setup(void)
{
	pr_info("swiotlb:restricted 32bit dma!\n");
	swiotlb_init(1);
	mips_dma_map_ops = &loongson2_linear_dma_map_ops;
}
