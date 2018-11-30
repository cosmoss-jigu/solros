.text
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
#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/pgtable_types.h>
#include <asm/page_types.h>
#include <asm/msr.h>
#include <asm/asm-offsets.h>
#include <asm/processor-flags.h>

/*
 *  linux/arch/x86_64/kernel/head.S -- start in 32bit and switch to 64bit
 *
 *  Copyright (C) 2000 Andrea Arcangeli <andrea@suse.de> SuSE
 *  Copyright (C) 2000 Pavel Machek <pavel@suse.cz>
 *  Copyright (C) 2000 Karsten Keil <kkeil@suse.de>
 *  Copyright (C) 2001,2002 Andi Kleen <ak@suse.de>
 *  Copyright (C) 2005 Eric Biederman <ebiederm@xmission.com>
 */

# Copyright 2003 Pavel Machek <pavel@suse.cz>, distribute under GPLv2

# This is a combination of secondary_startup_64 routine and the wakeup_long64 routine 
#  to implement Package C6 exit for intel MIC devices.

 	 
.code64

ENTRY(pc6_exit_64)
	/*
	 * The bootloader puts us here on a PC6 Exit.
	 * All cpus come here in no particular order.
	 * We inherit the level4 page table setup by .
	 * the regular boot process.  
	 */

#	movl	$(X86_CR4_PAE), %eax
#	movq	%rax, %cr4

	/* Setup early boot stage 4 level pagetables. */
	lea	pc6_trampoline_pgt(%rip),%rax
	movq	%rax, %cr3

	/* Ensure I am executing from virtual addresses */
	movq	$1f, %rax
	jmp	*%rax
1:
	movq	$pC6GDT,%rax
	lgdt	(%rax)
	jmp	centry
	/* Check if nx is implemented */
	movl	$0x80000001, %eax
	cpuid
	movl	%edx,%edi

	/* Setup EFER (Extended Feature Enable Register) */
	movl	$MSR_EFER, %ecx
	rdmsr
	btsl	$_EFER_SCE, %eax	/* Enable System Call */
	btl	$20,%edi		/* No Execute supported? */
	jnc     1f
	btsl	$_EFER_NX, %eax
1:	wrmsr				/* Make changes effective */
	lgdt	pC6GDT(%rip)
	
	/* Setup cr0 */
#define CR0_STATE	(X86_CR0_PE | X86_CR0_MP | X86_CR0_ET | \
			 X86_CR0_NE | X86_CR0_WP | X86_CR0_AM | \
			 X86_CR0_PG)
centry:
	movl	$CR0_STATE, %eax
	/* Make changes effective */
	movq	%rax, %cr0
#	movq	$(pkgstate_exit - __START_KERNEL_map),%rax

	pushq	$0		# fake return address to stop unwinder
	pushq	$__KERNEL_CS	# set correct cs
	movq	$pkgstate_exit,%rax
	pushq	%rax		# target address in negative space
	lretq
#	ret
ENDPROC(pc6_exit_64)
	.balign	PAGE_SIZE;
ENTRY(pc6_trampoline_pgt)
	.quad	level3_ident_pgt - __START_KERNEL_map + _KERNPG_TABLE
	.fill	510,8,0
	.quad	level3_kernel_pgt - __START_KERNEL_map + _KERNPG_TABLE
ENTRY(pc6_trampoline_end)

