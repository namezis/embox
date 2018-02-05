/*
 * Copyright (C) 2016 Etnaviv Project
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

#include <asm-generic/dma-mapping.h>

#include "etnaviv_compat.h"
#include "etnaviv_cmdbuf.h"
#include "etnaviv_gpu.h"
#include "etnaviv_mmu.h"
#include "etnaviv_drv.h"
#include "etnaviv_iommu.h"
#include "state.xml.h"
#include "state_hi.xml.h"

#define MMUv2_PTE_PRESENT		BIT(0)
#define MMUv2_PTE_EXCEPTION		BIT(1)
#define MMUv2_PTE_WRITEABLE		BIT(2)

#define MMUv2_MTLB_MASK			0xffc00000
#define MMUv2_MTLB_SHIFT		22
#define MMUv2_STLB_MASK			0x003ff000
#define MMUv2_STLB_SHIFT		12

#define MMUv2_MAX_STLB_ENTRIES		1024

static struct etnaviv_iommuv2_domain *to_etnaviv_domain(void *domain)
{
	return (void*) domain;//container_of(domain, struct etnaviv_iommuv2_domain, domain);
}

#if 0
static int etnaviv_iommuv2_map(struct iommu_domain *domain, unsigned long iova,
	   phys_addr_t paddr, size_t size, int prot)
{
	struct etnaviv_iommuv2_domain *etnaviv_domain =
			to_etnaviv_domain(domain);
	int mtlb_entry, stlb_entry;
	u32 entry = (u32)paddr | MMUv2_PTE_PRESENT;

	if (size != SZ_4K)
		return -EINVAL;

	if (prot & IOMMU_WRITE)
		entry |= MMUv2_PTE_WRITEABLE;

	mtlb_entry = (iova & MMUv2_MTLB_MASK) >> MMUv2_MTLB_SHIFT;
	stlb_entry = (iova & MMUv2_STLB_MASK) >> MMUv2_STLB_SHIFT;

	etnaviv_domain->stlb_cpu[mtlb_entry][stlb_entry] = entry;

	return 0;
}

static size_t etnaviv_iommuv2_unmap(struct iommu_domain *domain,
	unsigned long iova, size_t size)
{
	struct etnaviv_iommuv2_domain *etnaviv_domain =
			to_etnaviv_domain(domain);
	int mtlb_entry, stlb_entry;

	if (size != SZ_4K)
		return -EINVAL;

	mtlb_entry = (iova & MMUv2_MTLB_MASK) >> MMUv2_MTLB_SHIFT;
	stlb_entry = (iova & MMUv2_STLB_MASK) >> MMUv2_STLB_SHIFT;

	etnaviv_domain->stlb_cpu[mtlb_entry][stlb_entry] = MMUv2_PTE_EXCEPTION;

	return SZ_4K;
}

static phys_addr_t etnaviv_iommuv2_iova_to_phys(struct iommu_domain *domain,
	dma_addr_t iova)
{
	struct etnaviv_iommuv2_domain *etnaviv_domain =
			to_etnaviv_domain(domain);
	int mtlb_entry, stlb_entry;

	mtlb_entry = (iova & MMUv2_MTLB_MASK) >> MMUv2_MTLB_SHIFT;
	stlb_entry = (iova & MMUv2_STLB_MASK) >> MMUv2_STLB_SHIFT;

	return etnaviv_domain->stlb_cpu[mtlb_entry][stlb_entry] & ~(SZ_4K - 1);
}
#endif
static int etnaviv_iommuv2_init(struct etnaviv_gpu *gpu) {
	uint32_t *p;
	int ret, i, j;

	struct etnaviv_iommuv2_domain *etnaviv_domain = (void*) gpu->mmu.domain;
	/* allocate scratch page */
	etnaviv_domain->bad_page_cpu = dma_alloc_coherent(etnaviv_domain->dev,
						  SZ_4K,
						  &etnaviv_domain->bad_page_dma,
						  GFP_KERNEL);
	if (!etnaviv_domain->bad_page_cpu) {
		ret = -ENOMEM;
		goto fail_mem;
	}
	p = etnaviv_domain->bad_page_cpu;
	for (i = 0; i < SZ_4K / 4; i++)
		*p++ = 0xdead55aa;

	etnaviv_domain->mtlb_cpu = dma_alloc_coherent(etnaviv_domain->dev,
						  SZ_4K,
						  &etnaviv_domain->mtlb_dma,
						  GFP_KERNEL);
	if (!etnaviv_domain->mtlb_cpu) {
		ret = -ENOMEM;
		goto fail_mem;
	}

	/* pre-populate STLB pages (may want to switch to on-demand later) */
	for (i = 0; i < MMUv2_MAX_STLB_ENTRIES; i++) {
		etnaviv_domain->stlb_cpu[i] =
				dma_alloc_coherent(etnaviv_domain->dev,
						   SZ_4K,
						   &etnaviv_domain->stlb_dma[i],
						   GFP_KERNEL);
		if (!etnaviv_domain->stlb_cpu[i]) {
			ret = -ENOMEM;
			goto fail_mem;
		}
		p = etnaviv_domain->stlb_cpu[i];
		for (j = 0; j < SZ_4K / 4; j++)
			*p++ = MMUv2_PTE_EXCEPTION;

		etnaviv_domain->mtlb_cpu[i] = etnaviv_domain->stlb_dma[i] |
					      MMUv2_PTE_PRESENT;
	}

	return 0;

fail_mem:
	if (etnaviv_domain->bad_page_cpu)
		dma_free_coherent(etnaviv_domain->dev, SZ_4K,
				  etnaviv_domain->bad_page_cpu,
				  etnaviv_domain->bad_page_dma);

	if (etnaviv_domain->mtlb_cpu)
		dma_free_coherent(etnaviv_domain->dev, SZ_4K,
				  etnaviv_domain->mtlb_cpu,
				  etnaviv_domain->mtlb_dma);

	for (i = 0; i < MMUv2_MAX_STLB_ENTRIES; i++) {
		if (etnaviv_domain->stlb_cpu[i])
			dma_free_coherent(etnaviv_domain->dev, SZ_4K,
					  etnaviv_domain->stlb_cpu[i],
					  etnaviv_domain->stlb_dma[i]);
	}

	return ret;
}
#if 0
static void etnaviv_iommuv2_domain_free(struct iommu_domain *domain)
{
	struct etnaviv_iommuv2_domain *etnaviv_domain =
			to_etnaviv_domain(domain);
	int i;

	dma_free_coherent(etnaviv_domain->dev, SZ_4K,
			  etnaviv_domain->bad_page_cpu,
			  etnaviv_domain->bad_page_dma);

	dma_free_coherent(etnaviv_domain->dev, SZ_4K,
			  etnaviv_domain->mtlb_cpu,
			  etnaviv_domain->mtlb_dma);

	for (i = 0; i < MMUv2_MAX_STLB_ENTRIES; i++) {
		if (etnaviv_domain->stlb_cpu[i])
			dma_free_coherent(etnaviv_domain->dev, SZ_4K,
					  etnaviv_domain->stlb_cpu[i],
					  etnaviv_domain->stlb_dma[i]);
	}

	vfree(etnaviv_domain);
}

static size_t etnaviv_iommuv2_dump_size(struct iommu_domain *domain)
{
	struct etnaviv_iommuv2_domain *etnaviv_domain =
			to_etnaviv_domain(domain);
	size_t dump_size = SZ_4K;
	int i;

	for (i = 0; i < MMUv2_MAX_STLB_ENTRIES; i++)
		if (etnaviv_domain->mtlb_cpu[i] & MMUv2_PTE_PRESENT)
			dump_size += SZ_4K;

	return dump_size;
}

static void etnaviv_iommuv2_dump(struct iommu_domain *domain, void *buf)
{
	struct etnaviv_iommuv2_domain *etnaviv_domain =
			to_etnaviv_domain(domain);
	int i;

	memcpy(buf, etnaviv_domain->mtlb_cpu, SZ_4K);
	buf += SZ_4K;
	for (i = 0; i < MMUv2_MAX_STLB_ENTRIES; i++, buf += SZ_4K)
		if (etnaviv_domain->mtlb_cpu[i] & MMUv2_PTE_PRESENT)
			memcpy(buf, etnaviv_domain->stlb_cpu[i], SZ_4K);
}

static const struct etnaviv_iommu_ops etnaviv_iommu_ops = {
	.ops = {
		.domain_free = etnaviv_iommuv2_domain_free,
		.map = etnaviv_iommuv2_map,
		.unmap = etnaviv_iommuv2_unmap,
		.iova_to_phys = etnaviv_iommuv2_iova_to_phys,
		.pgsize_bitmap = SZ_4K,
	},
	.dump_size = etnaviv_iommuv2_dump_size,
	.dump = etnaviv_iommuv2_dump,
};
#endif
void etnaviv_iommuv2_restore(struct etnaviv_gpu *gpu)
{
	struct etnaviv_iommuv2_domain *etnaviv_domain =
			to_etnaviv_domain(gpu->mmu.domain);
	uint16_t prefetch;

	/* If the MMU is already enabled the state is still there. */
	if (gpu_read(gpu, VIVS_MMUv2_CONTROL) & VIVS_MMUv2_CONTROL_ENABLE)
		return;

	prefetch = etnaviv_buffer_config_mmuv2(gpu,
				(u32)etnaviv_domain->mtlb_dma,
				(u32)etnaviv_domain->bad_page_dma);
	etnaviv_gpu_start_fe(gpu, (u32)etnaviv_cmdbuf_get_pa(gpu->buffer),
			     prefetch);
	etnaviv_gpu_wait_idle(gpu, 100);

	gpu_write(gpu, VIVS_MMUv2_CONTROL, VIVS_MMUv2_CONTROL_ENABLE);
}

int etnaviv_iommuv2_domain_init(struct etnaviv_gpu *gpu) {
	gpu->mmu.start_addr = 0;
	gpu->mmu.end_addr = 4096 - 1;

	memset(gpu->mmu.domain, 0, sizeof(struct etnaviv_iommuv2_domain));
	if (etnaviv_iommuv2_init(gpu)) {
		return -1;
	}

	return 0;
}
