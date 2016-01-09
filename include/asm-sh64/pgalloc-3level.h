#ifndef __ASM_SH64_PGALLOC_3LEVEL_H
#define __ASM_SH64_PGALLOC_3LEVEL_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/pgalloc-3level.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 */

#if 0

extern __inline__ pmd_t *get_pmd_slow(void)
{
	pmd_t *ret = (pmd_t *)__get_free_page(GFP_KERNEL);

	if (ret)
		memset(ret, 0, PAGE_SIZE);
	return ret;
}

extern __inline__ pmd_t *get_pmd_fast(void)
{
	unsigned long *ret;

	if ((ret = pmd_quicklist) != NULL) {
		pmd_quicklist = (unsigned long *)(*ret);
		ret[0] = 0;
		pgtable_cache_size--;
	} else
		ret = (unsigned long *)get_pmd_slow();
	return (pmd_t *)ret;
}

extern __inline__ void free_pmd_fast(pmd_t *pmd)
{
	*(unsigned long *)pmd = (unsigned long) pmd_quicklist;
	pmd_quicklist = (unsigned long *) pmd;
	pgtable_cache_size++;
}

extern __inline__ void free_pmd_slow(pmd_t *pmd)
{
	free_page((unsigned long)pmd);
}

extern inline pmd_t * pmd_alloc(pgd_t *pgd, unsigned long address)
{
	if (!pgd)
		BUG();
	address = (address >> PMD_SHIFT) & (PTRS_PER_PMD - 1);
	if (pgd_none(*pgd)) {
		pmd_t *page = get_pmd_fast();

		if (!page)
			page = get_pmd_slow();
		if (page) {
			if (pgd_none(*pgd)) {
				set_pgd(pgd, __pgd((unsigned long) page | _KERNPG_TABLE));

				/* Flush TLB not required, SW tree walk */
				return page + address;
			} else
				free_pmd_fast(page);
		} else
			return NULL;
	}
	return (pmd_t *)pgd_page(*pgd) + address;
}

#endif


#endif /* __ASM_SH64_PGALLOC_3LEVEL_H */
