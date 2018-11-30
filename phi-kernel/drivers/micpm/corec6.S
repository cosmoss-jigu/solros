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

#define max_apic_id 248
#define MIC_STATE_C6	2	
#define MIC_STATE_PC6	4
#define MIC_STATE_C1	1
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
#  to implement core C6 exit and entry for intel MIC devices.

 	 
.code64

ENTRY(pkgstate_exit)	#PC6 exit trampoline enters kernel here.
	movq	$MIC_STATE_PC6, %r12 
	jmp	common
ENTRY(cc6_exit_64)
	/*
	 * At this point the CPU runs in 64bit mode CS.L = 1 CS.D = 1,
	 * and someone has loaded a mapped page table.
	 *
	 * %esi holds a physical pointer to real_mode_data.
	 *
	 * We come here either from from trampoline.S (using virtual addresses).
	 *
	 * Using virtual addresses from trampoline.S removes the need
	 * to have any identity mapped pages in the kernel page table
	 * after the boot processor executes this code.
	 */

	movl	$(X86_CR4_PAE), %eax
	movq	%rax, %cr4
	lgdt	pC6GDT(%rip)
	movq	$MIC_STATE_C6, %r12
common:
	/* Setup early boot stage 4 level pagetables. */
	movq	$(init_level4_pgt - __START_KERNEL_map), %rax
	addq	phys_base(%rip), %rax
	movq	%rax, %cr3

	/* Ensure I am executing from virtual addresses */
	movq	$1f, %rax
	jmp	*%rax
1:

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

	/* Setup cr0 */
#define CR0_STATE	(X86_CR0_PE | X86_CR0_MP | X86_CR0_ET | \
			 X86_CR0_NE | X86_CR0_WP | X86_CR0_AM | \
			 X86_CR0_PG)
	movl	$CR0_STATE, %eax
	/* Make changes effective */
	movq	%rax, %cr0
/*	
	Skip loading the gdt and setting up data segments. We will restore processor state anyway
	before calling/jumping to C code.
*/		
	/*
	 * We must switch to a new descriptor in kernel space for the GDT
	 * because soon the kernel won't have access anymore to the userspace
	 * addresses where we're currently running on. We have to do that here
	 * because in 32bit we couldn't load a 64bit linear address.
	 */
	/* Finally jump to run C code and to be on real kernel address
	 * Since we are running on identity-mapped space we have to jump
	 * to the full 64bit address, this is only possible as indirect
	 * jump.  In addition we need to ensure %cs is set so we make this
	 * a far return.
	 */
	movw	$__KERNEL_DS, %ax
	movw	%ax, %ss	
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movq    apicid_ptr(%rip), %rax	#Get virtual address for apic_id. We save that during CC6 init.
	movq	(%rax), %rax
	shrq	 $0x17, %rax
	andq	$0x1ff, %rax
	movl	$max_apic_id,%ebx
	cmpl	%ebx,%eax
	jbe	2f
bad_apicid:
	jmp	bad_apicid			
2:	
	movq	%rax,%rcx
	rdtsc
	shlq	$0x20,%rdx
	orq	%rax,%rdx
	movq	%rcx,%rax
	movq	%rdx,cc6_waketime(,%rax,8)		
	movq	cc6_cntxt(,%rax,8) , %rax
	movq	saved_context_cr4(%rax), %rbx
	movq	%rbx, %cr4
	movq	saved_context_cr3(%rax), %rbx
	movq	%rbx, %cr3
	movq	saved_context_cr2(%rax), %rbx
	movq	%rbx, %cr2
	movq	saved_context_cr0(%rax), %rbx
	movq	%rbx, %cr0
	movq	pt_regs_sp(%rax), %rsp
	pushq	%r12	#Save the return value
	pushq	%rax
	movq	%rax,%rdi	#pointer to cntx
	call	mic_restore_processor_state
	popq	%rax	
	movq	pt_regs_bp(%rax), %rbp
	movq	pt_regs_si(%rax), %rsi
	movq	pt_regs_di(%rax), %rdi
	movq	pt_regs_bx(%rax), %rbx
	movq	pt_regs_cx(%rax), %rcx
	movq	pt_regs_dx(%rax), %rdx
	movq	pt_regs_r8(%rax), %r8
	movq	pt_regs_r9(%rax), %r9
	movq	pt_regs_r10(%rax), %r10
	movq	pt_regs_r11(%rax), %r11
	movq	pt_regs_r12(%rax), %r12
	movq	pt_regs_r13(%rax), %r13
	movq	pt_regs_r14(%rax), %r14
	movq	pt_regs_r15(%rax), %r15
	pushq	pt_regs_flags(%rax)
	popfq
	popq	%rax	#return value tells the caller if we cane through the
			#CC6 trampoline or the PC6.
	ret		
ENDPROC(cc6_exit_64)

ENTRY(do_cc6entry_lowlevel)
	movq	%rdi,%rax		/* Context struct ptr passed as parameter	*/
	movq	%rsp, pt_regs_sp(%rax)
	movq	%rbp, pt_regs_bp(%rax)
	movq	%rsi, pt_regs_si(%rax)
	movq	%rdi, pt_regs_di(%rax)
	movq	%rbx, pt_regs_bx(%rax)
	movq	%rcx, pt_regs_cx(%rax)
	movq	%rdx, pt_regs_dx(%rax)
	movq	%r8, pt_regs_r8(%rax)
	movq	%r9, pt_regs_r9(%rax)
	movq	%r10, pt_regs_r10(%rax)
	movq	%r11, pt_regs_r11(%rax)
	movq	%r12, pt_regs_r12(%rax)
	movq	%r13, pt_regs_r13(%rax)
	movq	%r14, pt_regs_r14(%rax)
	movq	%r15, pt_regs_r15(%rax)
	pushfq
	popq	pt_regs_flags(%rax)
	test	%esi,%esi
	jz	1f
	wbinvd
1:	sti
	hlt
	cli
/*
	TBD: Need to save VPU registers here	
*/
	movq	$MIC_STATE_C1,%rax
	ret
ENDPROC(do_cc6entry_lowlevel)
	.data
	.align 16
ENTRY(c6_gdt_table)
        .quad   0x0000000000000000      /* NULL descriptor */
        .quad   0x0                     /* unused */
        .quad   0x00af9a000000ffff      /* __KERNEL_CS */
        .quad   0x00cf92000000ffff      /* __KERNEL_DS */
/*Use the next slot for the descriptor bootstrap uses for code segment	*/
	.quad	0x00a09b0000000000	
/*      .quad   0x00cffa000000ffff      *//* __USER32_CS */
        .quad   0x00cff2000000ffff      /* __USER_DS, __USER32_DS  */
        .quad   0x00affa000000ffff      /* __USER_CS */
        .quad   0x00cf9a000000ffff      /* __KERNEL32_CS */
        .quad   0,0                     /* TSS */
        .quad   0,0                     /* LDT */
        .quad   0,0,0                   /* three TLS descriptors */
        .quad   0x0000f40000000000      /* node/CPU stored in limit */
c6_gdt_end:
        .globl pC6GDT
pC6GDT:
        .word   c6_gdt_end-c6_gdt_table-1
        .quad   c6_gdt_table
	
	
