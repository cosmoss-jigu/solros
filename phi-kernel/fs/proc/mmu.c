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
/* mmu.c: mmu memory info files
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <asm/pgtable.h>
#include "internal.h"

#ifdef	CONFIG_KDB
#include <linux/kdb.h>
#endif

void get_vmalloc_info(struct vmalloc_info *vmi)
{
	struct vm_struct *vma;
	unsigned long free_area_size;
	unsigned long prev_end;
#ifdef	CONFIG_KDB
	int get_lock = !KDB_IS_RUNNING();
#else
#define	get_lock 1
#endif


	vmi->used = 0;

	if (!vmlist) {
		vmi->largest_chunk = VMALLOC_TOTAL;
	}
	else {
		vmi->largest_chunk = 0;

		prev_end = VMALLOC_START;

		if (get_lock)
			read_lock(&vmlist_lock);

		for (vma = vmlist; vma; vma = vma->next) {
			unsigned long addr = (unsigned long) vma->addr;

			/*
			 * Some archs keep another range for modules in vmlist
			 */
			if (addr < VMALLOC_START)
				continue;
			if (addr >= VMALLOC_END)
				break;

			vmi->used += vma->size;

			free_area_size = addr - prev_end;
			if (vmi->largest_chunk < free_area_size)
				vmi->largest_chunk = free_area_size;

			prev_end = vma->size + addr;
		}

		if (VMALLOC_END - prev_end > vmi->largest_chunk)
			vmi->largest_chunk = VMALLOC_END - prev_end;

		if (get_lock)
			read_unlock(&vmlist_lock);
	}
}
