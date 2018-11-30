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
#ifndef _ASM_X86_KDEBUG_H
#define _ASM_X86_KDEBUG_H

#include <linux/notifier.h>

struct pt_regs;

/* Grossly misnamed. */
enum die_val {
	DIE_OOPS = 1,
	DIE_INT3,
	DIE_DEBUG,
	DIE_PANIC,
	DIE_NMI,
	DIE_DIE,
	DIE_NMIWATCHDOG,
	DIE_KERNELDEBUG,
	DIE_TRAP,
	DIE_GPF,
	DIE_CALL,
	DIE_PAGE_FAULT,
	DIE_NMIUNKNOWN,
	DIE_KDEBUG_ENTER,
	DIE_KDEBUG_LEAVE,
};

extern void printk_address(unsigned long address, int reliable);
extern void die(const char *, struct pt_regs *,long);
extern int __must_check __die(const char *, struct pt_regs *, long);
extern void show_registers(struct pt_regs *regs);
extern void show_trace(struct task_struct *t, struct pt_regs *regs,
		       unsigned long *sp);
extern void __show_regs(struct pt_regs *regs, int all);
extern void show_regs(struct pt_regs *regs);
extern unsigned long oops_begin(void);
extern void oops_end(unsigned long, struct pt_regs *, int signr);
#ifdef CONFIG_KEXEC
extern int in_crash_kexec;
#else
/* no crash dump is ever in progress if no crash kernel can be kexec'd */
#define in_crash_kexec 0
#endif

#endif /* _ASM_X86_KDEBUG_H */
