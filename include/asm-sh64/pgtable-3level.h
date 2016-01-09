#ifndef __ASM_SH64_PGTABLE_3LEVEL_H
#define __ASM_SH64_PGTABLE_3LEVEL_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/pgtable-3level.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 * Supporting 32-bit addressing mode for now and, of course, 64-bit pointers.
 * Supporting variable NEFF/NPHYS defintions.
 *
 */

/*
 * NEFF and NPHYS related defines.
 */
#define NEFF		32
#define	NEFF_SIGN	(1LL << (NEFF - 1))
#define	NEFF_MASK	(-1LL << NEFF)

#define NPHYS		32
#define	NPHYS_SIGN	(1LL << (NPHYS - 1))
#define	NPHYS_MASK	(-1LL << NPHYS)

/*
 * three-level asymmetric paging structure: PGD is top level.
 * The asymmetry comes from 32-bit pointers and 64-bit PTEs.
 */
/* bottom level: PTE. It's 9 bits = 512 pointers */
#define PTRS_PER_PTE	((1<<PAGE_SHIFT)/sizeof(unsigned long long))
#define PTE_MAGNITUDE	3	      /* sizeof(unsigned long long) magnit. */
#define PTE_SHIFT	PAGE_SHIFT
#define PTE_BITS	(PAGE_SHIFT - PTE_MAGNITUDE)

/* middle level: PMD. It's 10 bits = 1024 pointers */
#define PTRS_PER_PMD	((1<<PAGE_SHIFT)/sizeof(unsigned long long *))
#define PMD_MAGNITUDE	2	      /* sizeof(unsigned long long *) magnit. */
#define PMD_SHIFT	(PTE_SHIFT + PTE_BITS)
#define PMD_BITS	(PAGE_SHIFT - PMD_MAGNITUDE)

/* top level: PMD. It's 1 bit = 2 pointers */
#define PGDIR_SHIFT	(PMD_SHIFT + PMD_BITS)
#define PGD_BITS	(NEFF - PGDIR_SHIFT)
#define PTRS_PER_PGD	(1<<PGD_BITS)

/*
 * Error outputs.
 */
#define pte_ERROR(e) \
	printk("%s:%d: bad pte %016Lx.\n", __FILE__, __LINE__, pte_val(e))
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %08lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pgd_val(e))

/*
 * Table setting routines. Used within arch/mm only.
 */
#define set_pgd(pgdptr, pgdval) (*(pgdptr) = pgdval)
#define set_pmd(pmdptr, pmdval) (*(pmdptr) = pmdval)

static __inline__ void set_pte(pte_t *pteptr, pte_t pteval)
{
	unsigned long long x = ((unsigned long long) pteval.pte);
	unsigned long long *xp = (unsigned long long *) pteptr;
	/*
	 * Sign-extend based on NPHYS.
	 */
	*(xp) = (x & NPHYS_SIGN) ? (x | NPHYS_MASK) : x;
}

static __inline__ void pmd_set(pmd_t *pmdp,pte_t *ptep)
{
	pmd_val(*pmdp) = (unsigned long) ptep; 
}

  


/*
 * PGD defines. Top level.
 */

/* To find an entry in a generic PGD. */
#define pgd_index(address) (((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))
#define __pgd_offset(address) pgd_index(address)
#define pgd_offset(mm, address) ((mm)->pgd+pgd_index(address))

/* To find an entry in a kernel PGD. */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)


/*
 * PGD level access routines.
 *
 * Note1:
 * There's no need to use physical addresses since the tree walk is all
 * in performed in software, until the PTE translation.
 *
 * Note 2:
 * A PGD entry can be uninitialized (_PGD_UNUSED), generically bad,
 * clear (_PGD_EMPTY), present. When present, lower 3 nibbles contain
 * _KERNPG_TABLE. Being a kernel virtual pointer also bit 31 must
 * be 1. Assuming an arbitrary clear value of bit 31 set to 0 and
 * lower 3 nibbles set to 0xFFF (_PGD_EMPTY) any other value is a
 * bad pgd that must be notified via printk().
 *
 */
#define _PGD_EMPTY		0x0
#define pgd_present(pgd_entry)	(1)
#define pgd_clear(pgd_entry_p)	(set_pgd((pgd_entry_p), __pgd(_PGD_EMPTY)))
#define pgd_none(pgd_entry)	(pgd_val((pgd_entry)) == _PGD_EMPTY)
/* TODO: Think later about what a useful definition of 'bad' would be now. */
#define pgd_bad(pgd_entry)	(0)
#define pgd_page(pgd_entry)	((unsigned long) (pgd_val(pgd_entry) & PAGE_MASK))


/*
 * PMD defines. Middle level.
 */

/* PGD to PMD dereferencing */
#define __pmd_offset(address) \
		(((address) >> PMD_SHIFT) & (PTRS_PER_PMD-1))
#define pmd_offset(dir, addr) \
		((pmd_t *) ((pgd_val(*(dir))) & PAGE_MASK) + __pmd_offset((addr)))


/*
 * PMD level access routines. Same notes as above.
 */
#define _PMD_EMPTY		0x0
/* Either the PMD is empty or present, it's not paged out */
#define pmd_present(pmd_entry)	(1)
#define pmd_clear(pmd_entry_p)	(set_pmd((pmd_entry_p), __pmd(_PMD_EMPTY)))
#define pmd_none(pmd_entry)	(pmd_val((pmd_entry)) == _PMD_EMPTY)
/* TODO: Think later about what a useful definition of 'bad' would be now. */
#define pmd_bad(pmd_entry)	(0)
#define pmd_page(pmd_entry)	((unsigned long) (pmd_val(pmd_entry) & PAGE_MASK))

/* PMD to PTE dereferencing */
#define __pte_offset(address) \
		((address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

#define pte_offset(dir, addr) \
		((pte_t *) ((pmd_val(*(dir))) & PAGE_MASK) + __pte_offset((addr)))

#endif /* __ASM_SH64_PGTABLE_3LEVEL_H */
