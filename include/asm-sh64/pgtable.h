#ifndef __ASM_SH64_PGTABLE_H
#define __ASM_SH64_PGTABLE_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/pgtable.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 * Copyright (C) 2003  Paul Mundt
 *
 * This file contains the functions and defines necessary to modify and use
 * the SuperH page table tree.
 */

#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/pgtable-3level.h>
#include <linux/threads.h>
#include <linux/config.h>

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];
extern void paging_init(void);

extern void flush_cache_all(void);
extern void flush_cache_mm(struct mm_struct *mm);
extern void flush_cache_range(struct mm_struct *mm, unsigned long start,
			      unsigned long end);
extern void flush_cache_page(struct vm_area_struct *vma, unsigned long addr);

extern void flush_page_to_ram(struct page *page);

extern void flush_icache_range(unsigned long start, unsigned long end);
extern void flush_icache_page(struct vm_area_struct *vma, struct page *pg);

extern void flush_dcache_page(struct page *pg);

extern void flush_cache_sigtramp(unsigned long start, unsigned long end);

#ifdef CONFIG_DCACHE_DISABLED

#define sh64_dcache_purge_sets(base,sets)		do { } while (0)
#define sh64_dcache_purge_all()				do { } while (0)
#define sh64_dcache_purge_kernel_range(start,end)	do { } while (0)
#define sh64_dcache_purge_coloured_phy_page(addr,eaddr)	do { } while (0)
#define sh64_dcache_purge_phy_page(pg)			do { } while (0)
#define sh64_dcache_purge_virt_page(mm,eaddr)		do { } while (0)
#define sh64_dcache_purge_user_page(mm,eaddr)		do { } while (0)
#define sh64_dcache_purge_user_range(mm,start,end)	do { } while (0)
#define sh64_dcache_wback_current_user_range(start,end) do { } while (0)

#define copy_user_page(to, from, addr)	memcpy(to, from, PAGE_SIZE)
#define clear_user_page(to, addr)	memset(to, 0, PAGE_SIZE)

#endif /* CONFIG_DCACHE_DISABLED */

#ifdef CONFIG_ICACHE_DISABLED

#define sh64_icache_inv_all()				do { } while (0)
#define sh64_icache_inv_kernel_range(start,end)		do { } while (0)
#define sh64_icache_inv_user_page(vma,eaddr)		do { } while (0)
#define sh64_icache_inv_user_page_range(mm,start,end)	do { } while (0)
#define sh64_icache_inv_user_small_range(mm,start,len)	do { } while (0)
#define sh64_icache_inv_current_user_range(start,end)	do { } while (0)

#endif /* CONFIG_ICACHE_DISABLED */

/* We provide our own get_unmapped_area to avoid cache synonym issue */
#define HAVE_ARCH_UNMAPPED_AREA

/*
 * Basically we have the same two-level (which is the logical three level
 * Linux page table layout folded) page tables as the i386.
 */

/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
extern unsigned char empty_zero_page[PAGE_SIZE];
#define ZERO_PAGE(vaddr) (mem_map + MAP_NR(empty_zero_page))

#endif /* !__ASSEMBLY__ */

#include <asm/pgtable-3level.h>

#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

/* Round it up ! */
#define USER_PTRS_PER_PGD	((TASK_SIZE+PGDIR_SIZE-1)/PGDIR_SIZE)
#define FIRST_USER_PGD_NR	0

#ifndef __ASSEMBLY__
#define VMALLOC_END	0xff000000
#define VMALLOC_START	0xf0000000
#define VMALLOC_VMADDR(x) ((unsigned long)(x))

#define IOBASE_VADDR	0xff000000
#define IOBASE_END	0xffffffff

/*
 * PTEL coherent flags.
 * See Chapter 17 ST50 CPU Core Volume 1, Architecture.
 */
/* The bits that are required in the SH-5 TLB are placed in the h/w-defined
   positions, to avoid expensive bit shuffling on every refill.  The remaining
   bits are used for s/w purposes and masked out on each refill.

   Note, the PTE slots are used to hold data of type swp_entry_t when a page is
   swapped out.  Only the _PAGE_PRESENT flag is significant when the page is
   swapped out, and it must be placed so that it doesn't overlap either the
   type or offset fields of swp_entry_t.  For x86, offset is at [31:8] and type
   at [6:1], with _PAGE_PRESENT at bit 0 for both pte_t and swp_entry_t.  This
   scheme doesn't map to SH-5 because bit [0] controls cacheability.  So bit
   [2] is used for _PAGE_PRESENT and the type field of swp_entry_t is split
   into 2 pieces.  That is handled by SWP_ENTRY and SWP_TYPE below. */
#define _PAGE_WT	0x001  /* CB0: uncachable/device or write-back/write-thru */
#define _PAGE_CACHABLE	0x002  /* CB1: uncachable/cachable */
#define _PAGE_PRESENT	0x004  /* software: if allocated */
#define _PAGE_SIZE0	0x008  /* SZ0-bit : size of page */
#define _PAGE_SIZE1	0x010  /* SZ1-bit : size of page */
#define _PAGE_SHARED	0x020  /* software: reflects PTEH's SH */
#define _PAGE_READ	0x040  /* PR0-bit : read access allowed */
#define _PAGE_EXECUTE	0x080  /* PR1-bit : execute access allowed */
#define _PAGE_WRITE	0x100  /* PR2-bit : write access allowed */
#define _PAGE_USER	0x200  /* PR3-bit : user space access allowed */
#define _PAGE_DIRTY	0x400  /* software: page accessed in write */
#define _PAGE_ACCESSED	0x800  /* software: page referenced */

/* Mask which drops software flags */
#define _PAGE_FLAGS_HARDWARE_MASK	0xfffffffffffff3dbLL
/* Flags default: 4KB, Read, Not write, Not execute, Not user */
#define _PAGE_FLAGS_HARDWARE_DEFAULT	0x0000000000000040LL

/*
 * Default flags for a Kernel page.
 * This is fundametally also SHARED because the main use of this define
 * (other than for PGD/PMD entries) is for the VMALLOC pool which is
 * contextless.
 *
 * _PAGE_EXECUTE is required for modules
 *
 */
#define _KERNPG_TABLE	(_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | \
			 _PAGE_EXECUTE | \
			 _PAGE_CACHABLE | _PAGE_ACCESSED | _PAGE_DIRTY | \
			 _PAGE_SHARED)

/* Default flags for a User page */
#define _PAGE_TABLE	(_KERNPG_TABLE | _PAGE_USER)

#define _PAGE_CHG_MASK	(PTE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)

#define PAGE_NONE	__pgprot(_PAGE_CACHABLE | _PAGE_ACCESSED)
#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | \
				 _PAGE_CACHABLE | _PAGE_ACCESSED | _PAGE_USER | \
				 _PAGE_SHARED)
/* We need to include PAGE_EXECUTE in PAGE_COPY because it is the default
 * protection mode for the stack. */
#define PAGE_COPY	__pgprot(_PAGE_PRESENT | _PAGE_READ | _PAGE_CACHABLE | \
				 _PAGE_ACCESSED | _PAGE_USER | _PAGE_EXECUTE)
#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_READ | _PAGE_CACHABLE | \
				 _PAGE_ACCESSED | _PAGE_USER)
#define PAGE_KERNEL	__pgprot(_KERNPG_TABLE)


/*
 * In ST50 we have full permissions (Read/Write/Execute/Shared).
 * Just match'em all. These are for mmap(), therefore all at least
 * User/Cachable/Present/Accessed. No point in making Fault on Write.
 */
#define __MMAP_COMMON	(_PAGE_PRESENT | _PAGE_USER | _PAGE_CACHABLE | _PAGE_ACCESSED)
       /* sxwr */
#define __P000	__pgprot(__MMAP_COMMON)
#define __P001	__pgprot(__MMAP_COMMON | _PAGE_READ)
#define __P010	__pgprot(__MMAP_COMMON)
#define __P011	__pgprot(__MMAP_COMMON | _PAGE_READ)
#define __P100	__pgprot(__MMAP_COMMON | _PAGE_EXECUTE)
#define __P101	__pgprot(__MMAP_COMMON | _PAGE_EXECUTE | _PAGE_READ)
#define __P110	__pgprot(__MMAP_COMMON | _PAGE_EXECUTE)
#define __P111	__pgprot(__MMAP_COMMON | _PAGE_EXECUTE | _PAGE_READ)

#define __S000	__pgprot(__MMAP_COMMON | _PAGE_SHARED)
#define __S001	__pgprot(__MMAP_COMMON | _PAGE_SHARED | _PAGE_READ)
#define __S010	__pgprot(__MMAP_COMMON | _PAGE_SHARED | _PAGE_WRITE)
#define __S011	__pgprot(__MMAP_COMMON | _PAGE_SHARED | _PAGE_READ | _PAGE_WRITE)
#define __S100	__pgprot(__MMAP_COMMON | _PAGE_SHARED | _PAGE_EXECUTE)
#define __S101	__pgprot(__MMAP_COMMON | _PAGE_SHARED | _PAGE_EXECUTE | _PAGE_READ)
#define __S110	__pgprot(__MMAP_COMMON | _PAGE_SHARED | _PAGE_EXECUTE | _PAGE_WRITE)
#define __S111	__pgprot(__MMAP_COMMON | _PAGE_SHARED | _PAGE_EXECUTE | _PAGE_READ | _PAGE_WRITE)

/*
 * Handling allocation failures during page table setup.
 */
extern void __handle_bad_pmd_kernel(pmd_t * pmd);
#define __handle_bad_pmd(x)	__handle_bad_pmd_kernel(x)

/*
 * PTE level access routines.
 *
 * Note1:
 * It's the tree walk leaf. This is physical address to be stored.
 *
 * Note 2:
 * Regarding the choice of _PTE_EMPTY:

   We must choose a bit pattern that cannot be valid, whether or not the page
   is present.  bit[2]==1 => present, bit[2]==0 => swapped out.  If swapped
   out, bits [31:8], [6:3], [1:0] are under swapper control, so only bit[7] is
   left for us to select.  If we force bit[7]==0 when swapped out, we could use
   the combination bit[7,2]=2'b10 to indicate an empty PTE.  Alternatively, if
   we force bit[7]==1 when swapped out, we can use all zeroes to indicate
   empty.  This is convenient, because the page tables get cleared to zero
   when they are allocated.

 */
#define _PTE_EMPTY	0x0
#define pte_present(x)	(pte_val(x) & _PAGE_PRESENT)
#define pte_clear(xp)	(set_pte(xp, __pte(_PTE_EMPTY)))
#define pte_none(x)	(pte_val(x) == _PTE_EMPTY)

/*
 * Some definitions to translate between mem_map, PTEs, and page
 * addresses:
 */

/*
 * Given a PTE, return the index of the mem_map[] entry corresponding
 * to the page frame the PTE. Get the absolute physical address, make
 * a relative physical address and translate it to an index.
 */
#define pte_pagenr(x)		(((unsigned long) (pte_val(x)) - \
				 __MEMORY_START) >> PAGE_SHIFT)

/*
 * Given a PTE, return the "struct page *".
 */
#define pte_page(x)		(mem_map + pte_pagenr(x))

/*
 * Return number of (down rounded) MB corresponding to x pages.
 */
#define pages_to_mb(x) ((x) >> (20-PAGE_SHIFT))


/*
 * The following have defined behavior only work if pte_present() is true.
 */
extern inline int pte_read(pte_t pte) { return pte_val(pte) & _PAGE_READ; }
extern inline int pte_exec(pte_t pte) { return pte_val(pte) & _PAGE_EXECUTE; }
extern inline int pte_dirty(pte_t pte){ return pte_val(pte) & _PAGE_DIRTY; }
extern inline int pte_young(pte_t pte){ return pte_val(pte) & _PAGE_ACCESSED; }
extern inline int pte_write(pte_t pte){ return pte_val(pte) & _PAGE_WRITE; }

extern inline pte_t pte_rdprotect(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_READ)); return pte; }
extern inline pte_t pte_wrprotect(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_WRITE)); return pte; }
extern inline pte_t pte_exprotect(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_EXECUTE)); return pte; }
extern inline pte_t pte_mkclean(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_DIRTY)); return pte; }
extern inline pte_t pte_mkold(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) & ~_PAGE_ACCESSED)); return pte; }

extern inline pte_t pte_mkread(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_READ)); return pte; }
extern inline pte_t pte_mkwrite(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_WRITE)); return pte; }
extern inline pte_t pte_mkexec(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_EXECUTE)); return pte; }
extern inline pte_t pte_mkdirty(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_DIRTY)); return pte; }
extern inline pte_t pte_mkyoung(pte_t pte)	{ set_pte(&pte, __pte(pte_val(pte) | _PAGE_ACCESSED)); return pte; }

/*
 * Conversion functions: convert a page and protection to a page entry.
 *
 * extern pte_t mk_pte(struct page *page, pgprot_t pgprot)
 */
#define mk_pte(page,pgprot)							\
({										\
	pte_t __pte;								\
										\
	set_pte(&__pte, __pte((((page)-mem_map) << PAGE_SHIFT) | 		\
		__MEMORY_START | pgprot_val((pgprot))));			\
	__pte;									\
})

/*
 * This takes a (absolute) physical page address that is used
 * by the remapping functions
 */
#define mk_pte_phys(physpage, pgprot) \
({ pte_t __pte; set_pte(&__pte, __pte(physpage | pgprot_val(pgprot))); __pte; })

extern inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{ set_pte(&pte, __pte((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot))); return pte; }

#define page_pte_prot(page, prot) mk_pte(page, prot)
#define page_pte(page) page_pte_prot(page, __pgprot(0))

#define pte_same(A,B)        (pte_val(A) == pte_val(B)) 


extern void update_mmu_cache(struct vm_area_struct * vma,
			     unsigned long address, pte_t pte);

/* Encode and de-code a swap entry */
#define SWP_TYPE(x)			(((x).val & 3) + (((x).val >> 1) & 0x3c))
#define SWP_OFFSET(x)			((x).val >> 8)

/* Avoid bit 2 for type */
static inline swp_entry_t SWP_ENTRY(unsigned long type, unsigned long offset)
{
	unsigned long result;
	/* Assert bit[7], to make swapped out page table entries distinct
	   from unused/uninitialised ones */
	result = (offset << 8) + 0x80 + ((type & 0x3c) << 1) + (type & 3);
	return (swp_entry_t) {result};
}
#define pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define swp_entry_to_pte(x)		((pte_t) { (x).val })

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define PageSkip(page)		(0)
#define kern_addr_valid(addr)	(1)

#define io_remap_page_range remap_page_range
#endif /* !__ASSEMBLY__ */

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()    do { } while (0)

/* This must be implemented for ptrace() */
#if 0
#define flush_icache_user_range(vma,pg,adr,len)      do { } while (0)  
#endif


#endif /* __ASM_SH64_PGTABLE_H */
