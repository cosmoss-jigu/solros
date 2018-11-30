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
#ifndef	_ASM_KDB_H
#define _ASM_KDB_H

/*
 * Kernel Debugger Architecture Dependent (x86) Global Headers
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1999-2008 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * KDB_ENTER() is a macro which causes entry into the kernel
 * debugger from any point in the kernel code stream.  If it
 * is intended to be used from interrupt level, it must  use
 * a non-maskable entry method. The vector is KDB_VECTOR,
 * defined in hw_irq.h
 */
#define KDB_ENTER()	do {if (kdb_on && !KDB_IS_RUNNING()) { asm("\tint $129\n"); }} while(0)

/*
 * Needed for exported symbols.
 */
typedef unsigned long kdb_machreg_t;

/*
 * Per cpu arch specific kdb state.  Must be in range 0xff000000.
 */
#define KDB_STATE_A_IF		0x01000000	/* Saved IF flag */


#ifdef CONFIG_X86_32

#define kdb_machreg_fmt		"0x%lx"
#define kdb_machreg_fmt0	"0x%08lx"
#define kdb_bfd_vma_fmt		"0x%lx"
#define kdb_bfd_vma_fmt0	"0x%08lx"
#define kdb_elfw_addr_fmt	"0x%x"
#define kdb_elfw_addr_fmt0	"0x%08x"
#define kdb_f_count_fmt		"%ld"

#else	/* CONFIG_X86_32 */

#define kdb_machreg_fmt		"0x%lx"
#define kdb_machreg_fmt0	"0x%016lx"
#define kdb_bfd_vma_fmt		"0x%lx"
#define kdb_bfd_vma_fmt0	"0x%016lx"
#define kdb_elfw_addr_fmt	"0x%x"
#define kdb_elfw_addr_fmt0	"0x%016x"
#define kdb_f_count_fmt		"%ld"

/*
 * Functions to safely read and write kernel areas.  The {to,from}_xxx
 * addresses are not necessarily valid, these functions must check for
 * validity.  If the arch already supports get and put routines with
 * suitable validation and/or recovery on invalid addresses then use
 * those routines, otherwise check it yourself.
 */

/*
 * asm-i386 uaccess.h supplies __copy_to_user which relies on MMU to
 * trap invalid addresses in the _xxx fields.  Verify the other address
 * of the pair is valid by accessing the first and last byte ourselves,
 * then any access violations should only be caused by the _xxx
 * addresses,
 */

#include <asm/uaccess.h>

static inline int
__kdba_putarea_size(unsigned long to_xxx, void *from, size_t size)
{
	mm_segment_t oldfs = get_fs();
	int r;
	char c;
	c = *((volatile char *)from);
	c = *((volatile char *)from + size - 1);
	c = c;

	if (to_xxx < PAGE_OFFSET) {
		return kdb_putuserarea_size(to_xxx, from, size);
	}

	set_fs(KERNEL_DS);
	r = __copy_to_user_inatomic((void *)to_xxx, from, size);
	set_fs(oldfs);
	printk("%s r=%d, to=%lx, __pa(to)=%lx, from=%p, size=%d\n", __FUNCTION__, r, to_xxx, __pa(to_xxx), from, (int)size);
	return r;
}

static inline int
__kdba_getarea_size(void *to, unsigned long from_xxx, size_t size)
{
	mm_segment_t oldfs = get_fs();
	int r;
	*((volatile char *)to) = '\0';
	*((volatile char *)to + size - 1) = '\0';

	if (from_xxx < PAGE_OFFSET) {
		return kdb_getuserarea_size(to, from_xxx, size);
	}

	set_fs(KERNEL_DS);
	r = __copy_to_user_inatomic(to, (void *)from_xxx, size);
	set_fs(oldfs);
	return r;
}

/* For numa with replicated code/data, the platform must supply its own
 * kdba_putarea_size and kdba_getarea_size routines.  Without replication kdb
 * uses the standard architecture routines.
 */
#ifdef CONFIG_NUMA_REPLICATE
extern int kdba_putarea_size(unsigned long to_xxx, void *from, size_t size);
extern int kdba_getarea_size(void *to, unsigned long from_xxx, size_t size);
#else
#define kdba_putarea_size __kdba_putarea_size
#define kdba_getarea_size __kdba_getarea_size
#endif

static inline int
kdba_verify_rw(unsigned long addr, size_t size)
{
	unsigned char data[size];
	return(kdba_getarea_size(data, addr, size) || kdba_putarea_size(addr, data, size));
}

#endif	/* !CONFIG_X86_32 */

static inline unsigned long
kdba_funcptr_value(void *fp)
{
	return (unsigned long)fp;
}

#ifdef CONFIG_SMP
extern void kdba_giveback_vector(int);
#endif

#endif	/* !_ASM_KDB_H */
