/*
 * Copyright (C) 2017 Etnaviv Project
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//#include <drm/drm_mm.h>
#include <stdio.h>
#include "etnaviv_compat.h"
#include "etnaviv_cmdbuf.h"
#include "etnaviv_gpu.h"
#include "etnaviv_drv.h"
//#include "etnaviv_mmu.h"

#define SUBALLOC_SIZE		(256 * 1024)
#define SUBALLOC_GRANULE	(4096)
#define SUBALLOC_GRANULES	(SUBALLOC_SIZE / SUBALLOC_GRANULE)

struct etnaviv_cmdbuf_suballoc {
	/* suballocated dma buffer properties */
	struct etnaviv_gpu *gpu;
	void *vaddr;
	dma_addr_t paddr;

	/* GPU mapping */
	u32 iova;
	//struct drm_mm_node vram_node; /* only used on MMUv2 */

	/* allocation management */
	//struct mutex lock;
	//DECLARE_BITMAP(granule_map, SUBALLOC_GRANULES);
	int free_space;
	//wait_queue_head_t free_event;
};

struct etnaviv_cmdbuf_suballoc *
etnaviv_cmdbuf_suballoc_new(struct etnaviv_gpu * gpu)
{
	struct etnaviv_cmdbuf_suballoc *suballoc;
	int ret = 0;

	suballoc = kzalloc(sizeof(*suballoc), GFP_KERNEL);
	if (!suballoc)
		return ERR_PTR(-ENOMEM);

	suballoc->gpu = gpu;
	//mutex_init(&suballoc->lock);
	//init_waitqueue_head(&suballoc->free_event);

	suballoc->paddr = (int) sysmalloc(SUBALLOC_SIZE); //dma_alloc_wc(gpu->dev, SUBALLOC_SIZE,
	suballoc->vaddr = (void*) suballoc->paddr;
//				       &suballoc->paddr, GFP_KERNEL);
	if (!suballoc->vaddr)
		goto free_suballoc;

	//ret = etnaviv_iommu_get_suballoc_va(gpu, suballoc->paddr,
	//				    &suballoc->vram_node, SUBALLOC_SIZE,
	//				    &suballoc->iova);
	if (ret)
		goto free_dma;

	return suballoc;

free_dma:
	sysfree(suballoc->vaddr);
	//dma_free_wc(gpu->dev, SUBALLOC_SIZE, suballoc->vaddr, suballoc->paddr);
free_suballoc:
	kfree(suballoc);

	return NULL;
}

void etnaviv_cmdbuf_suballoc_destroy(struct etnaviv_cmdbuf_suballoc *suballoc)
{
	//etnaviv_iommu_put_suballoc_va(suballoc->gpu, &suballoc->vram_node,
	//			      SUBALLOC_SIZE, suballoc->iova);
	//dma_free_wc(suballoc->gpu->dev, SUBALLOC_SIZE, suballoc->vaddr,
	//	    suballoc->paddr);
	kfree(suballoc->vaddr);
	kfree(suballoc);
}

struct etnaviv_cmdbuf *
etnaviv_cmdbuf_new(struct etnaviv_cmdbuf_suballoc *suballoc, u32 size,
		   size_t nr_bos)
{
	struct etnaviv_cmdbuf *cmdbuf;
	printf("trace %s %d\n", __func__, __LINE__);
	size_t sz = size_vstruct(nr_bos, sizeof(cmdbuf->bo_map[0]),
				 sizeof(*cmdbuf));
	printf("trace %s %d\n", __func__, __LINE__);
	int granule_offs, /* order, */ ret;

	printf("trace %s %d\n", __func__, __LINE__);
	cmdbuf = kzalloc(sz, GFP_KERNEL);
	if (!cmdbuf)
		return NULL;

	printf("trace %s %d\n", __func__, __LINE__);
	cmdbuf->suballoc = suballoc;
	cmdbuf->size = size;

	printf("trace %s %d\n", __func__, __LINE__);
	//order = order_base_2(ALIGN(size, SUBALLOC_GRANULE) / SUBALLOC_GRANULE);
retry:
	//mutex_lock(&suballoc->lock);
	granule_offs = 0;//bitmap_find_free_region(suballoc->granule_map,
			//		SUBALLOC_GRANULES, order);
	printf("trace %s %d\n", __func__, __LINE__);
	if (granule_offs < 0) {
		suballoc->free_space = 0;
		ret = 0;
	//	mutex_unlock(&suballoc->lock);
	//	ret = wait_event_interruptible_timeout(suballoc->free_event,
	//					       suballoc->free_space,
	//					       msecs_to_jiffies(10 * 1000));
		if (!ret) {
		//	dev_err(suballoc->gpu->dev,
			log_error(	"Timeout waiting for cmdbuf space\n");
			return NULL;
		}
		goto retry;
	}
	printf("trace %s %d\n", __func__, __LINE__);
	//mutex_unlock(&suballoc->lock);
	cmdbuf->suballoc_offset = granule_offs * SUBALLOC_GRANULE;
	printf("trace %s %d\n", __func__, __LINE__);
	printf("suballoc=%p, cmdbuf=%p\n", suballoc, cmdbuf);
	cmdbuf->vaddr = suballoc->vaddr + cmdbuf->suballoc_offset;
	printf("trace %s %d\n", __func__, __LINE__);

	return cmdbuf;
}

void etnaviv_cmdbuf_free(struct etnaviv_cmdbuf *cmdbuf)
{
	struct etnaviv_cmdbuf_suballoc *suballoc = cmdbuf->suballoc;
	//int order = order_base_2(ALIGN(cmdbuf->size, SUBALLOC_GRANULE) /
	//			 SUBALLOC_GRANULE);

	//mutex_lock(&suballoc->lock);
	//bitmap_release_region(suballoc->granule_map,
	//		      cmdbuf->suballoc_offset / SUBALLOC_GRANULE,
	//		      order);
	suballoc->free_space = 1;
	//mutex_unlock(&suballoc->lock);
	//wake_up_all(&suballoc->free_event);
	kfree(cmdbuf);
}

u32 etnaviv_cmdbuf_get_va(struct etnaviv_cmdbuf *buf)
{
	return buf->suballoc->iova + buf->suballoc_offset;
}

dma_addr_t etnaviv_cmdbuf_get_pa(struct etnaviv_cmdbuf *buf)
{
	return buf->suballoc->paddr + buf->suballoc_offset;
}
