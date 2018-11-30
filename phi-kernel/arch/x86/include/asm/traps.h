/* * Copyright (c) Intel Corporation (2011).
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

#ifndef _ASM_X86_TRAPS_H
#define _ASM_X86_TRAPS_H

#include <asm/debugreg.h>
#include <asm/siginfo.h>			/* TRAP_TRACE, ... */

#ifdef CONFIG_X86_32
#define dotraplinkage
#else
#define dotraplinkage asmlinkage
#endif

asmlinkage void divide_error(void);
asmlinkage void debug(void);
asmlinkage void nmi(void);
asmlinkage void int3(void);
asmlinkage void xen_debug(void);
asmlinkage void xen_int3(void);
asmlinkage void xen_stack_segment(void);
asmlinkage void overflow(void);
asmlinkage void bounds(void);
asmlinkage void invalid_op(void);
asmlinkage void device_not_available(void);
#ifdef CONFIG_X86_64
asmlinkage void double_fault(void);
#endif
asmlinkage void coprocessor_segment_overrun(void);
asmlinkage void invalid_TSS(void);
asmlinkage void segment_not_present(void);
asmlinkage void stack_segment(void);
asmlinkage void general_protection(void);
asmlinkage void page_fault(void);
asmlinkage void async_page_fault(void);
asmlinkage void spurious_interrupt_bug(void);
asmlinkage void coprocessor_error(void);
asmlinkage void alignment_check(void);
#ifdef CONFIG_X86_MCE
asmlinkage void machine_check(void);
#endif /* CONFIG_X86_MCE */
asmlinkage void simd_coprocessor_error(void);

dotraplinkage void do_divide_error(struct pt_regs *, long);
dotraplinkage void do_debug(struct pt_regs *, long);
dotraplinkage void do_nmi(struct pt_regs *, long);
dotraplinkage void do_int3(struct pt_regs *, long);
dotraplinkage void do_overflow(struct pt_regs *, long);
dotraplinkage void do_bounds(struct pt_regs *, long);
dotraplinkage void do_invalid_op(struct pt_regs *, long);
dotraplinkage void do_device_not_available(struct pt_regs *, long);
dotraplinkage void do_coprocessor_segment_overrun(struct pt_regs *, long);
dotraplinkage void do_invalid_TSS(struct pt_regs *, long);
dotraplinkage void do_segment_not_present(struct pt_regs *, long);
dotraplinkage void do_stack_segment(struct pt_regs *, long);
#ifdef CONFIG_X86_64
dotraplinkage void do_double_fault(struct pt_regs *, long);
asmlinkage __kprobes struct pt_regs *sync_regs(struct pt_regs *);
#endif
dotraplinkage void do_general_protection(struct pt_regs *, long);
dotraplinkage void do_page_fault(struct pt_regs *, unsigned long);
dotraplinkage void do_spurious_interrupt_bug(struct pt_regs *, long);
dotraplinkage void do_coprocessor_error(struct pt_regs *, long);
dotraplinkage void do_alignment_check(struct pt_regs *, long);
#ifdef CONFIG_X86_MCE
dotraplinkage void do_machine_check(struct pt_regs *, long);
#endif
dotraplinkage void do_simd_coprocessor_error(struct pt_regs *, long);
#ifdef CONFIG_X86_32
dotraplinkage void do_iret_error(struct pt_regs *, long);
#endif

static inline int get_si_code(unsigned long condition)
{
	if (condition & DR_STEP)
		return TRAP_TRACE;
	else if (condition & (DR_TRAP0|DR_TRAP1|DR_TRAP2|DR_TRAP3))
		return TRAP_HWBKPT;
	else
		return TRAP_BRKPT;
}

extern int panic_on_unrecovered_nmi;

void math_error(struct pt_regs *, int, int);
void math_emulate(struct math_emu_info *);
#ifndef CONFIG_X86_32
asmlinkage void smp_thermal_interrupt(void);
asmlinkage void mce_threshold_interrupt(void);
#endif

#endif /* _ASM_X86_TRAPS_H */
