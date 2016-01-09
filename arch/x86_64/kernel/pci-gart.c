/*
 * Dynamic DMA mapping support for AMD Hammer.
 * 
 * Use the integrated AGP GART in the Hammer northbridge as an IOMMU for PCI.
 * This allows to use PCI devices that only support 32bit addresses on systems
 * with more than 4GB. 
 *
 * See Documentation/DMA-mapping.txt for the interface specification.
 * 
 * Copyright 2002 Andi Kleen, SuSE Labs.
 * $Id: pci-gart.c,v 1.8 2002/07/16 15:30:04 ak Exp $
 */

/* 
 * Notebook:

agpgart_be
 check if the simple reservation scheme is enough.

possible future tuning: 
 fast path for sg streaming mappings 
 more intelligent flush strategy - flush only a single NB?
 move boundary between IOMMU and AGP in GART dynamically
 could use exact fit in the gart in alloc_consistent, not order of two.
*/ 

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/agp_backend.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <asm/io.h>
#include <asm/mtrr.h>
#include <asm/bitops.h>
#include <asm/pgtable.h>
#include "pci-x86_64.h"

extern unsigned long start_pfn, end_pfn; 

unsigned long iommu_bus_base;	/* GART remapping area (physical) */
static unsigned long iommu_size; 	/* size of remapping area bytes */
static unsigned long iommu_pages;	/* .. and in pages */

u32 *iommu_gatt_base; 		/* Remapping table */

static int no_iommu; 
static int no_agp; 
static int force_mmu = 1;

/* Allocation bitmap for the remapping area */ 
static spinlock_t iommu_bitmap_lock = SPIN_LOCK_UNLOCKED;
static unsigned long *iommu_gart_bitmap; /* guarded by iommu_bitmap_lock */

/* y must be power of two */
#define round_up(x,y) (((x) + (y) - 1) & ~((y)-1))
#define round_down(x,y) ((x) & ~((y)-1))

#define GPTE_MASK 0xfffffff000
#define GPTE_VALID    1
#define GPTE_COHERENT 2
#define GPTE_ENCODE(x,flag) (((x) & 0xfffffff0) | ((x) >> 28) | GPTE_VALID | (flag))
#define GPTE_DECODE(x) (((x) & 0xfffff000) | (((x) & 0xff0) << 28))

#define for_all_nb(dev) \
	pci_for_each_dev(dev) \
		if (dev->bus->number == 0 && PCI_FUNC(dev->devfn) == 3 && \
		    (PCI_SLOT(dev->devfn) >= 24) && (PCI_SLOT(dev->devfn) <= 31))

#define EMERGENCY_PAGES 32 /* = 128KB */ 

/* backdoor interface to AGP driver */
extern void amd_x86_64_tlbflush(void *); 
extern int agp_init(void);
extern u64 amd_x86_64_configure (struct pci_dev *hammer, u64);

extern int agp_memory_reserved;
extern __u32 *agp_gatt_table;

static unsigned long next_bit;  /* protected by iommu_bitmap_lock */

static unsigned long alloc_iommu(int size) 
{ 	
	unsigned long offset, flags;

	spin_lock_irqsave(&iommu_bitmap_lock, flags);	

	offset = find_next_zero_string(iommu_gart_bitmap,next_bit,iommu_pages,size);
	if (offset == -1) 
	       	offset = find_next_zero_string(iommu_gart_bitmap,0,next_bit,size);
	if (offset != -1) { 
		set_bit_string(iommu_gart_bitmap, offset, size); 
		next_bit = offset+size; 
		if (next_bit >= iommu_pages) 
			next_bit = 0;
	} 
	spin_unlock_irqrestore(&iommu_bitmap_lock, flags);      
	return offset;
} 

static void free_iommu(unsigned long offset, int size)
{ 
	unsigned long flags;
	spin_lock_irqsave(&iommu_bitmap_lock, flags);
	clear_bit_string(iommu_gart_bitmap, offset, size);
	next_bit = offset;
	spin_unlock_irqrestore(&iommu_bitmap_lock, flags);
} 

static inline void flush_gart(void) 
{ 
	amd_x86_64_tlbflush(NULL); 	
} 

void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
			   dma_addr_t *dma_handle)
{
	void *memory;
	int gfp = GFP_ATOMIC;
	int order, i;
	unsigned long iommu_page;

	if (hwdev == NULL || hwdev->dma_mask < 0xffffffff || no_iommu)
		gfp |= GFP_DMA;

	/* 
	 * First try to allocate continuous and use directly if already 
	 * in lowmem. 
	 */ 
	order = get_order(size);
	memory = (void *)__get_free_pages(gfp, order);
	if (memory == NULL) {
		return NULL; 
	} else {
		int high = (unsigned long)virt_to_bus(memory) + size
			>= 0xffffffff;
		int mmu = high;
		if (force_mmu) 
			mmu = 1;
		if (no_iommu) { 
			if (high) goto error;
			mmu = 0; 
		} 	
		memset(memory, 0, size); 
		if (!mmu) { 
			*dma_handle = virt_to_bus(memory);
			return memory;
		}
	} 

	iommu_page = alloc_iommu(1<<order);
	if (iommu_page == -1)
		goto error; 

   	/* Fill in the GATT, allocating pages as needed. */
	for (i = 0; i < 1<<order; i++) { 
		unsigned long phys_mem; 
		void *mem = memory + i*PAGE_SIZE;
		if (i > 0) 
			atomic_inc(&virt_to_page(mem)->count); 
		phys_mem = virt_to_phys(mem); 
		BUG_ON(phys_mem & ~PTE_MASK); 
		iommu_gatt_base[iommu_page + i] = GPTE_ENCODE(phys_mem,GPTE_COHERENT); 
	} 

	flush_gart();
	*dma_handle = iommu_bus_base + (iommu_page << PAGE_SHIFT);
	return memory; 
	
 error:
	free_pages((unsigned long)memory, order); 
	return NULL; 
}

/* 
 * Unmap consistent memory.
 * The caller must ensure that the device has finished accessing the mapping.
 */
void pci_free_consistent(struct pci_dev *hwdev, size_t size,
			 void *vaddr, dma_addr_t bus)
{
	u64 pte;
	int order = get_order(size);
	unsigned long iommu_page;
	int i;

	if (bus < iommu_bus_base || bus > iommu_bus_base + iommu_size) { 
		free_pages((unsigned long)vaddr, order); 		
		return;
	} 
	iommu_page = (bus - iommu_bus_base) / PAGE_SIZE;
	for (i = 0; i < 1<<order; i++) {
		pte = iommu_gatt_base[iommu_page + i];
		BUG_ON((pte & GPTE_VALID) == 0); 
		iommu_gatt_base[iommu_page + i] = 0; 		
		free_page((unsigned long) __va(GPTE_DECODE(pte)));
	} 
	flush_gart(); 
	free_iommu(iommu_page, 1<<order);
}

static void iommu_full(struct pci_dev *dev, void *addr, size_t size, int dir)
{
	/* 
	 * Ran out of IOMMU space for this operation. This is very bad.
	 * Unfortunately the drivers cannot handle this operation properly.
	 * Return some non mapped prereserved space in the aperture and 
	 * let the Northbridge deal with it. This will result in garbage
	 * in the IO operation. When the size exceeds the prereserved space
	 * memory corruption will occur or random memory will be DMAed 
	 * out. Hopefully no network devices use single mappings that big.
	 */ 
	
	printk(KERN_ERR 
  "PCI-DMA: Error: ran out out IOMMU space for %p size %lu at device %s[%s]\n",
	       addr,size, dev ? dev->name : "?", dev ? dev->slot_name : "?");

	if (size > PAGE_SIZE*EMERGENCY_PAGES) {
		if (dir == PCI_DMA_FROMDEVICE || dir == PCI_DMA_BIDIRECTIONAL)
			printk(KERN_ERR "PCI-DMA: Memory will be corrupted\n");
		if (dir == PCI_DMA_TODEVICE || dir == PCI_DMA_BIDIRECTIONAL) 
			printk(KERN_ERR "PCI-DMA: Random memory will be DMAed\n"); 
	} 
} 

static inline int need_iommu(struct pci_dev *dev, unsigned long addr, size_t size)
{ 
	u64 mask = dev ? dev->dma_mask : 0xffffffff;
	int high = (~mask & (unsigned long)(addr + size)) != 0;
	int mmu = high;
	if (force_mmu) 
		mmu = 1; 
	if (no_iommu) { 
		if (high) 
			panic("pci_map_single: high address but no IOMMU.\n"); 
		mmu = 0; 
	} 	
	return mmu; 
}

dma_addr_t pci_map_single(struct pci_dev *dev, void *addr, size_t size,int dir)
{ 
	unsigned long iommu_page;
	unsigned long phys_mem, bus;
	int i, npages;

	BUG_ON(dir == PCI_DMA_NONE);

	phys_mem = virt_to_phys(addr); 
	if (!need_iommu(dev, phys_mem, size))
		return phys_mem; 

	npages = round_up(size, PAGE_SIZE) >> PAGE_SHIFT;

	iommu_page = alloc_iommu(npages); 
	if (iommu_page == -1) {
		iommu_full(dev, addr, size, dir); 
		return iommu_bus_base; 
	} 

	phys_mem &= PAGE_MASK;
	for (i = 0; i < npages; i++, phys_mem += PAGE_SIZE) {
		BUG_ON(phys_mem & ~PTE_MASK); 
		
		/* 
		 * Set coherent mapping here to avoid needing to flush
		 * the caches on mapping.
		 */
		iommu_gatt_base[iommu_page + i] = GPTE_ENCODE(phys_mem, GPTE_COHERENT);
	}
	flush_gart(); 

	bus = iommu_bus_base + iommu_page*PAGE_SIZE; 
	return bus + ((unsigned long)addr & ~PAGE_MASK); 	
} 

/*
 * Free a temporary PCI mapping.
 */ 
void pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr,
		      size_t size, int direction)
{
	unsigned long iommu_page; 
	int i, npages;
	if (dma_addr < iommu_bus_base + EMERGENCY_PAGES*PAGE_SIZE || 
	    dma_addr > iommu_bus_base + iommu_size)
		return;
	iommu_page = (dma_addr - iommu_bus_base)>>PAGE_SHIFT;	
	npages = round_up(size, PAGE_SIZE) >> PAGE_SHIFT;
	for (i = 0; i < npages; i++) { 
		iommu_gatt_base[iommu_page + i] = 0; 
	}
	flush_gart(); 
	free_iommu(iommu_page, npages);
}

EXPORT_SYMBOL(pci_map_single);
EXPORT_SYMBOL(pci_unmap_single);

static inline unsigned long check_iommu_size(unsigned long aper,
					     unsigned long aper_size, 
					     unsigned long iommu_size)
{ 
	unsigned long a; 
	if (!iommu_size) { 
		iommu_size = aper_size; 
		if (!no_agp) 
			iommu_size /= 2; 
	} 

	a = aper + iommu_size; 
	iommu_size -= round_up(a, LARGE_PAGE_SIZE) - a;

	if (iommu_size < 128*1024*1024) 
		printk(KERN_WARNING
  "PCI-DMA: Warning: Small IOMMU %luMB. Consider increasing the AGP aperture in BIOS\n",iommu_size>>20); 
	
	return iommu_size;
} 

/* 
 * Private Northbridge GATT initialization in case we cannot use the AGP driver for 
 * some reason. 
 */
static __init int init_k8_gatt(agp_kern_info *info)
{ 
	struct pci_dev *dev;
	void *gatt;
	u32 aper_size;
	u32 gatt_size;

#if 1 /* BIOS bug workaround for now */
	goto nommu; 
#endif

	aper_size = 0;
	for_all_nb(dev) { 
		pci_read_config_dword(dev, 0x90, &aper_size);
		aper_size = 32 << ((aper_size>>1) & 7);
		break;
	}

	info->aper_size = aper_size; 
	if (!aper_size) { 
		printk("PCI-DMA: Cannot fetch aperture size\n"); 
		goto nommu;
	} 

	gatt_size = ((aper_size * 1024 * 1024) / PAGE_SIZE) * sizeof(u32); 
	gatt = (void *)__get_free_pages(GFP_KERNEL, get_order(gatt_size)); 
	if (!gatt) 
		panic("Cannot allocate GATT table"); 
	memset(gatt, 0, gatt_size); 
	change_page_attr(virt_to_page(gatt), gatt_size/PAGE_SIZE, PAGE_KERNEL_NOCACHE);
	agp_gatt_table = gatt;

	for_all_nb(dev) { 
		info->aper_base = amd_x86_64_configure(dev, virt_to_bus(gatt));
	}
	if (info->aper_base)
		return 0;
	printk("Cannot find an K8\n"); 

 nommu:
	if (end_pfn >= 0xffffffff>>PAGE_SHIFT)
		printk(KERN_ERR "PCI-DMA: More than 4GB of RAM and no IOMMU\n"
		       KERN_ERR "32bit PCI IO may malfunction."); 
	return -1; 
} 

void __init pci_iommu_init(void)
{ 
	agp_kern_info info;
	unsigned long aper_size;
	unsigned long iommu_start;

	if (no_agp || (agp_init() < 0) || (agp_copy_info(&info) < 0)) { 
		printk(KERN_INFO "PCI-DMA: Disabling AGP.\n");
		no_agp = 1;
		if (init_k8_gatt(&info) < 0) { 
			printk(KERN_INFO "PCI-DMA: Disabling IOMMU.\n"); 
			no_iommu = 1;
			return; 
		}
	} 
	
	aper_size = info.aper_size * 1024 * 1024;
	iommu_size = check_iommu_size(info.aper_base, aper_size, iommu_size); 
	iommu_pages = iommu_size >> PAGE_SHIFT; 

	iommu_gart_bitmap = (void*)__get_free_pages(GFP_KERNEL, 
						    get_order(iommu_pages/8)); 
	if (!iommu_gart_bitmap) 
		panic("Cannot allocate iommu bitmap\n"); 
	memset(iommu_gart_bitmap, 0, iommu_pages/8);

	/* 
	 * Out of IOMMU space handling.
	 * Reserve some invalid pages at the beginning of the GART. 
	 */ 
	set_bit_string(iommu_gart_bitmap, 0, EMERGENCY_PAGES); 

	agp_memory_reserved = iommu_size;	
	printk(KERN_INFO"PCI-DMA: Reserving %luMB of IOMMU area in the AGP aperture\n",
	       iommu_size>>20); 

	iommu_start = aper_size - iommu_size;	
	iommu_bus_base = info.aper_base + iommu_start; 
	iommu_gatt_base = agp_gatt_table + (iommu_start>>PAGE_SHIFT);

	asm volatile("wbinvd" ::: "memory");
} 

/* iommu=[size][,noagp][,off][,force][,noforce] 
   noagp don't initialize the AGP driver and use full aperture.
   off   don't use the IOMMU
*/
static __init int iommu_setup(char *opt) 
{ 
    int arg;
    char *p;
    while ((p = strsep(&opt, ",")) != NULL) { 
	    if (strcmp(p,"noagp"))
		    no_agp = 1; 
	    if (strcmp(p,"off"))
		    no_iommu = 1;
	    if (strcmp(p,"force"))
		    force_mmu = 1;
	    if (strcmp(p,"noforce"))
		    force_mmu = 0;
	    if (isdigit(*p) && get_option(&p, &arg)) 
		    iommu_size = arg;
    }	
    return 1;
} 

__setup("iommu=", iommu_setup); 
