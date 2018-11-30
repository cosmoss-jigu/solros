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
/*
 *	CoreC6_trampoline.S - trampoline_64.S with some changes to work for intel MIC device core C6 idle state
 *	entry and exit.	
 *			      
 *	Trampoline.S	Derived from Setup.S by Linus Torvalds
 *
 *	4 Jan 1997 Michael Chastain: changed to gnu as.
 *	15 Sept 2005 Eric Biederman: 64bit PIC support
 *
 *	Entry: CS:IP point to the start of our code, we are 
 *	in real mode with no stack, but the rest of the 
 *	trampoline page to make our stack and everything else
 *	is a mystery.
 *
 *	On entry to trampoline_data, the processor is in real mode
 *	with 16-bit addressing and 16-bit data.  CS has some value
 *	and IP is zero.  Thus, data addresses need to be absolute
 *	(no relocation) and are taken with regard to r_base.
 *
 *	With the addition of trampoline_level4_pgt this code can
 *	now enter a 64bit kernel that lives at arbitrary 64bit
 *	physical addresses.
 *
 *	If you work on this file, check the object module with objdump
 *	--full-contents --reloc to make sure there are no relocation
 *	entries.
 */
 

#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/pgtable_types.h>
#include <asm/page_types.h>
#include <asm/msr.h>
#include <asm/segment.h>
#include <asm/processor-flags.h>

.section .rodata, "a", @progbits
.code16

ENTRY(cc6_trampoline_data)
r_base = .
	cli			# We should be safe anyway
#	wbinvd	/* don't do this	*/
	mov	%cs, %ax	# Code and data in the same place
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %ss
	mov	%cs, %ax
	movzx	%ax, %esi		# Find the 32bit trampoline location
	shll	$4, %esi
/*					# Fixup the vectors
	addl	%esi, startup_32_vector - r_base
	addl	%esi, startup_64_vector - r_base
	addl	%esi, tgdt + 2 - r_base	# Fixup the gdt pointer
*/
	/*
	 * GDT tables in non default location kernel can be beyond 16MB and
	 * lgdt will not be able to load the address as in real mode default
	 * operand size is 16bit. Use lgdtl instead to force operand size
	 * to 32 bit.
	 */

	lidtl	tidt - r_base	# load idt with 0, 0
	lgdtl	tgdt - r_base	# load gdt with whatever is appropriate

	mov	$X86_CR0_PE, %ax	# protected mode (PE) bit
	lmsw	%ax			# into protected mode

	# flush prefetch and jump to startup_32
	ljmpl	*(startup_32_vector - r_base)

	.code32
	.balign 4
startup_32:
	movl	$__KERNEL_DS, %eax	# Initialize the %ds segment register
	movl	%eax, %ds

	movl	$X86_CR4_PAE, %eax
	movl	%eax, %cr4		# Enable PAE mode

					# Setup trampoline 4 level pagetables
	leal	(cc6_trampoline_level4_pgt - r_base)(%esi), %eax
	movl	%eax, %cr3

	movl	$MSR_EFER, %ecx
	movl	$(1 << _EFER_LME), %eax	# Enable Long Mode
	xorl	%edx, %edx
	wrmsr

	# Enable paging and in turn activate Long Mode
	# Enable protected mode
	movl	$(X86_CR0_PG | X86_CR0_PE), %eax
	movl	%eax, %cr0

	/*
	 * At this point we're in long mode but in 32bit compatibility mode
	 * with EFER.LME = 1, CS.L = 0, CS.D = 1 (and in turn
	 * EFER.LMA = 1). Now we want to jump in 64bit mode, to do that we use
	 * the new gdt/idt that has __KERNEL_CS with CS.L = 1.
	 */
	ljmp	*(startup_64_vector - r_base)(%esi)

	.code64
	.balign 4
startup_64:
	# Now jump into the kernel using virtual addresses
	movq	$cc6_exit_64, %rax
	jmp	*%rax

	.code16

	# Careful these need to be in the same 64K segment as the above;
tidt:
	.word	0			# idt limit = 0
	.word	0, 0			# idt base = 0L

	# Duplicate the global descriptor table
	# so the kernel can live anywhere
	.balign 4
ENTRY(tgdt)
	.short	tgdt_end - tgdt		# gdt limit
	.long	tgdt - r_base
	.short 0
	.quad	0x00cf9b000000ffff	# __KERNEL32_CS
	.quad	0x00af9b000000ffff	# __KERNEL_CS
	.quad	0x00cf93000000ffff	# __KERNEL_DS
tgdt_end:

	.balign 4

ENTRY(startup_32_vector)
	.long	startup_32 - r_base
	.word	__KERNEL32_CS, 0

	.balign 4
ENTRY(startup_64_vector)
	.long	startup_64 - r_base
	.word	__KERNEL_CS, 0
	
	.balign PAGE_SIZE		
ENTRY(cc6_trampoline_level4_pgt)
	.quad	level3_ident_pgt - __START_KERNEL_map + _KERNPG_TABLE
	.fill	510,8,0
	.quad	level3_kernel_pgt - __START_KERNEL_map + _KERNPG_TABLE
ENTRY(cc6_trampoline_end)
