/* * Copyright (c) 2010 - 2012 Intel Corporation.
*
* Disclaimer: The codes contained in these modules may be specific to the
* Intel Software Development Platform codenamed: Knights Ferry, and the 
* Intel product codenamed: Knights Corner, and are not backward compatible 
* with other Intel products. Additionally, Intel will NOT support the codes 
* or instruction set in future products.
*
* Intel offers no warranty of any kind regarding the code.  This code is
* licensed on an "AS IS" basis and Intel is not obligated to provide any support,
* assistance, installation, training, or other services of any kind.  Intel is 
* also not obligated to provide any updates, enhancements or extensions.  Intel 
* specifically disclaims any warranty of merchantability, non-infringement, 
* fitness for any particular purpose, and any other warranty.
*
* Further, Intel disclaims all liability of any kind, including but not
* limited to liability for infringement of any proprietary rights, relating
* to the use of the code, even if Intel is notified of the possibility of
* such liability.  Except as expressly stated in an Intel license agreement
* provided with this code and agreed upon with Intel, no license, express
* or implied, by estoppel or otherwise, to any intellectual property rights
* is granted herein.
*/
#ifndef _ASM_X86_CACHEFLUSH_H
#define _ASM_X86_CACHEFLUSH_H

#ifdef CONFIG_X86_EARLYMIC
void mic_flush_icache(void);
void mic_icache_dirty(struct page *page);

#endif

/* Caches aren't brain-dead on the intel. */
#include <asm-generic/cacheflush.h>

#ifdef CONFIG_X86_PAT
/*
 * X86 PAT uses page flags WC and Uncached together to keep track of
 * memory type of pages that have backing page struct. X86 PAT supports 3
 * different memory types, _PAGE_CACHE_WB, _PAGE_CACHE_WC and
 * _PAGE_CACHE_UC_MINUS and fourth state where page's memory type has not
 * been changed from its default (value of -1 used to denote this).
 * Note we do not support _PAGE_CACHE_UC here.
 */

#define _PGMT_DEFAULT		0
#define _PGMT_WC		(1UL << PG_arch_1)
#define _PGMT_UC_MINUS		(1UL << PG_uncached)
#define _PGMT_WB		(1UL << PG_uncached | 1UL << PG_arch_1)
#define _PGMT_MASK		(1UL << PG_uncached | 1UL << PG_arch_1)
#define _PGMT_CLEAR_MASK	(~_PGMT_MASK)

static inline unsigned long get_page_memtype(struct page *pg)
{
	unsigned long pg_flags = pg->flags & _PGMT_MASK;

	if (pg_flags == _PGMT_DEFAULT)
		return -1;
	else if (pg_flags == _PGMT_WC)
		return _PAGE_CACHE_WC;
	else if (pg_flags == _PGMT_UC_MINUS)
		return _PAGE_CACHE_UC_MINUS;
	else
		return _PAGE_CACHE_WB;
}

static inline void set_page_memtype(struct page *pg, unsigned long memtype)
{
	unsigned long memtype_flags = _PGMT_DEFAULT;
	unsigned long old_flags;
	unsigned long new_flags;

	switch (memtype) {
	case _PAGE_CACHE_WC:
		memtype_flags = _PGMT_WC;
		break;
	case _PAGE_CACHE_UC_MINUS:
		memtype_flags = _PGMT_UC_MINUS;
		break;
	case _PAGE_CACHE_WB:
		memtype_flags = _PGMT_WB;
		break;
	}

	do {
		old_flags = pg->flags;
		new_flags = (old_flags & _PGMT_CLEAR_MASK) | memtype_flags;
	} while (cmpxchg(&pg->flags, old_flags, new_flags) != old_flags);
}
#else
static inline unsigned long get_page_memtype(struct page *pg) { return -1; }
static inline void set_page_memtype(struct page *pg, unsigned long memtype) { }
#endif

/*
 * The set_memory_* API can be used to change various attributes of a virtual
 * address range. The attributes include:
 * Cachability   : UnCached, WriteCombining, WriteBack
 * Executability : eXeutable, NoteXecutable
 * Read/Write    : ReadOnly, ReadWrite
 * Presence      : NotPresent
 *
 * Within a catagory, the attributes are mutually exclusive.
 *
 * The implementation of this API will take care of various aspects that
 * are associated with changing such attributes, such as:
 * - Flushing TLBs
 * - Flushing CPU caches
 * - Making sure aliases of the memory behind the mapping don't violate
 *   coherency rules as defined by the CPU in the system.
 *
 * What this API does not do:
 * - Provide exclusion between various callers - including callers that
 *   operation on other mappings of the same physical page
 * - Restore default attributes when a page is freed
 * - Guarantee that mappings other than the requested one are
 *   in any state, other than that these do not violate rules for
 *   the CPU you have. Do not depend on any effects on other mappings,
 *   CPUs other than the one you have may have more relaxed rules.
 * The caller is required to take care of these.
 */

int _set_memory_uc(unsigned long addr, int numpages);
int _set_memory_wc(unsigned long addr, int numpages);
int _set_memory_wb(unsigned long addr, int numpages);
int set_memory_uc(unsigned long addr, int numpages);
int set_memory_wc(unsigned long addr, int numpages);
int set_memory_wb(unsigned long addr, int numpages);
int set_memory_x(unsigned long addr, int numpages);
int set_memory_nx(unsigned long addr, int numpages);
int set_memory_ro(unsigned long addr, int numpages);
int set_memory_rw(unsigned long addr, int numpages);
int set_memory_np(unsigned long addr, int numpages);
int set_memory_4k(unsigned long addr, int numpages);

int set_memory_array_uc(unsigned long *addr, int addrinarray);
int set_memory_array_wc(unsigned long *addr, int addrinarray);
int set_memory_array_wb(unsigned long *addr, int addrinarray);

int set_pages_array_uc(struct page **pages, int addrinarray);
int set_pages_array_wc(struct page **pages, int addrinarray);
int set_pages_array_wb(struct page **pages, int addrinarray);

/*
 * For legacy compatibility with the old APIs, a few functions
 * are provided that work on a "struct page".
 * These functions operate ONLY on the 1:1 kernel mapping of the
 * memory that the struct page represents, and internally just
 * call the set_memory_* function. See the description of the
 * set_memory_* function for more details on conventions.
 *
 * These APIs should be considered *deprecated* and are likely going to
 * be removed in the future.
 * The reason for this is the implicit operation on the 1:1 mapping only,
 * making this not a generally useful API.
 *
 * Specifically, many users of the old APIs had a virtual address,
 * called virt_to_page() or vmalloc_to_page() on that address to
 * get a struct page* that the old API required.
 * To convert these cases, use set_memory_*() on the original
 * virtual address, do not use these functions.
 */

int set_pages_uc(struct page *page, int numpages);
int set_pages_wb(struct page *page, int numpages);
int set_pages_x(struct page *page, int numpages);
int set_pages_nx(struct page *page, int numpages);
int set_pages_ro(struct page *page, int numpages);
int set_pages_rw(struct page *page, int numpages);


void clflush_cache_range(void *addr, unsigned int size);

#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 0

#ifdef CONFIG_X86_EARLYMIC
static inline void flush_dcache_page(struct page *page)
{
	mic_icache_dirty(page);
}

static inline void flush_icache_range(unsigned long start,
				      unsigned long end)
{
	mic_flush_icache();
}

static inline void flush_icache_user_range(struct vm_area_struct *vma,
					   struct page *page,
					   unsigned long addr,
					   unsigned long len)
{
	mic_flush_icache();
}
#endif

#ifdef CONFIG_DEBUG_RODATA
void mark_rodata_ro(void);
extern const int rodata_test_data;
extern int kernel_set_to_readonly;
void set_kernel_text_rw(void);
void set_kernel_text_ro(void);
#else
static inline void set_kernel_text_rw(void) { }
static inline void set_kernel_text_ro(void) { }
#endif

#ifdef CONFIG_DEBUG_RODATA_TEST
int rodata_test(void);
#else
static inline int rodata_test(void)
{
	return 0;
}
#endif

#endif /* _ASM_X86_CACHEFLUSH_H */
