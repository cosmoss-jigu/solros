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

/*
 * include/asm-x86_64/vpu.h
 *
 * Save/Restore VPU State
 *
 * Author: Stephan.Zeisset@intel.com
 *
 */

#ifndef __ASM_X86_MIC_NI_H
#define __ASM_X86_MIC_NI_H
#include <linux/sched.h>
#include <linux/stringify.h>
#include <asm/processor.h>
#include <asm/sigcontext.h>
#include <asm/user.h>
#include <asm/thread_info.h>
#include <asm/uaccess.h>

#ifdef CONFIG_MK1OM

/*
 *  VXCSR initialization values.
 *  The "invalid" value will cause us to take #UD exceptions
 *  when any VPU instruction is executed.
 *  Note: DUE bit(bit21) has to be 1 for K1OM
 */
#define VXCSR_INVALID 0x00200000
#define VXCSR_DEFAULT 0x00200000

// These bits are reserved and must be zero.  Debuggers using ptrace can change these bits.
// 7-12, 16-19, 20 DDO (POC only), 22-31
#define VXCSR_MBZ 0xffdf1f80

#define VSTORED_DISP32_EAX(v, disp32) ".byte 0x62, 0xf1 ^ (" #v " & 0x10) ^ ((" #v " & 0x8) << 4), 0x78, 0x08, 0x29, 0x80 + ((" #v " & 0x7) << 3); .long " #disp32 "\n"

#define VLOADD_DISP32_EAX(v, disp32) ".byte 0x62, 0xf1 ^ (" #v " & 0x10) ^ ((" #v " & 0x8) << 4), 0x78, 0x08, 0x28, 0x80 + ((" #v " & 0x7) << 3); .long " #disp32 "\n"

#define VKMOV_TO_EBX(k) ".byte 0xc5, 0xf8, 0x93, 0xd8 + (" #k ")\n"

#define VKMOV_FROM_EBX(k) ".byte 0xc5, 0xf8, 0x92, 0xc3 + (" #k " << 3)\n"

#define STVXCSR_DISP32_EAX(disp32) ".byte 0x0f, 0xae, 0x98; .long " #disp32 "\n"

#define LDVXCSR_DISP32_EAX(disp32) "orl $"__stringify(VXCSR_DEFAULT)","#disp32"(%%rax)\n\t" \
				   "andl $~"__stringify(VXCSR_MBZ)","#disp32"(%%rax)\n\t" \
				   "ldmxcsr "#disp32"(%%rax)\n"

#else

/*
 *  VXCSR initialization values.
 *  The "invalid" value will cause us to take #UD exceptions
 *  when any VPU instruction is executed.
 */
#define VXCSR_INVALID 0
#define VXCSR_DEFAULT 0x9fc0

#define VSTORED_DISP32_EAX(v, disp32) ".byte 0xd6, (" #v " >> 1) & 0xc, 0x0, 0x14, 0x80 + ((" #v " & 0x7) << 3); .long " #disp32 "\n"

#define VLOADD_DISP32_EAX(v, disp32) ".byte 0xd6, (" #v " >> 1) & 0xc, 0x0, 0x10, 0x80 + ((" #v " & 0x7) << 3); .long " #disp32 "\n"

#define VKMOV_TO_EBX(k) ".byte 0x62, 0xdc, 0xc3 + (" #k " << 3)\n"

#define VKMOV_FROM_EBX(k) ".byte 0x62, 0xec, 0xd8 + (" #k ")\n"

#define STVXCSR_DISP32_EAX(disp32) ".byte 0x62, 0xeb, 0x98; .long " #disp32 "\n"

#define LDVXCSR_DISP32_EAX(disp32) ".byte 0x62, 0xeb, 0x90; .long " #disp32 "\n"

#endif


#define CHK_VSTORED_DISP32_EAX(v, disp32)	\
	"1: "					\
	VSTORED_DISP32_EAX(v, disp32)		\
	".section __ex_table,\"a\"\n"		\
	"   .align 8\n"				\
	"   .quad  1b,3f\n"			\
	".previous\n"

#define CHK_VLOADD_DISP32_EAX(v, disp32)	\
	"1: "					\
	VLOADD_DISP32_EAX(v, disp32)		\
	".section __ex_table,\"a\"\n"		\
	"   .align 8\n"				\
	"   .quad  1b,3f\n"			\
	".previous\n"

#define VKSTORE_DISP32_EAX(k, disp32)		\
	VKMOV_TO_EBX(k)				\
	"movw %%bx, " #disp32 "(%%rax)\n"

#define VKLOAD_DISP32_EAX(k, disp32)		\
	"movw " #disp32 "(%%rax), %%bx\n"	\
	VKMOV_FROM_EBX(k)

#define CHK_VKSTORE_DISP32_EAX(k, disp32)	\
	VKMOV_TO_EBX(k)				\
	"1: movw %%bx, " #disp32 "(%%rax)\n"	\
	".section __ex_table,\"a\"\n"		\
	"   .align 8\n"				\
	"   .quad  1b,3f\n"			\
	".previous\n"

#define CHK_VKLOAD_DISP32_EAX(k, disp32)	\
	"1: movw " #disp32 "(%%rax), %%bx\n"	\
	VKMOV_FROM_EBX(k)			\
	".section __ex_table,\"a\"\n"		\
	"   .align 8\n"				\
	"   .quad  1b,3f\n"			\
	".previous\n"

#define CHK_STVXCSR_DISP32_EAX(disp32)		\
	"1: "					\
	STVXCSR_DISP32_EAX(disp32)		\
	".section __ex_table,\"a\"\n"		\
	"   .align 8\n"				\
	"   .quad  1b,3f\n"			\
	".previous\n"

#define CHK_LDVXCSR_DISP32_EAX(disp32)		\
	"1: "					\
	LDVXCSR_DISP32_EAX(disp32)		\
	".section __ex_table,\"a\"\n"		\
	"   .align 8\n"				\
	"   .quad  1b,3f\n"			\
	".previous\n"


static inline int __mic_restore_mask_regs(struct vpustate_struct *vpustate)
{
	int err;

	asm volatile(
		CHK_VKLOAD_DISP32_EAX(0, 0x800)
		CHK_VKLOAD_DISP32_EAX(1, 0x802)
		CHK_VKLOAD_DISP32_EAX(2, 0x804)
		CHK_VKLOAD_DISP32_EAX(3, 0x806)
		CHK_VKLOAD_DISP32_EAX(4, 0x808)
		CHK_VKLOAD_DISP32_EAX(5, 0x80a)
		CHK_VKLOAD_DISP32_EAX(6, 0x80c)
		CHK_VKLOAD_DISP32_EAX(7, 0x80e)
		"2:\n"
		".section .fixup,\"ax\"\n"
		"3:  movl $-1,%[err]\n"
		"    jmp  2b\n"
		".previous\n"
		: [err] "=r" (err)
		: [fx] "a" (vpustate), "m" (*vpustate), "0" (0) : "ebx"
	);
	return err;
}

static inline int restore_vpu_checking_nomask(struct vpustate_struct *vpustate)
{
	int err;

	asm volatile(
		CHK_VLOADD_DISP32_EAX(0, 0x00)
		CHK_VLOADD_DISP32_EAX(1, 0x40)
		CHK_VLOADD_DISP32_EAX(2, 0x80)
		CHK_VLOADD_DISP32_EAX(3, 0xc0)
		CHK_VLOADD_DISP32_EAX(4, 0x100)
		CHK_VLOADD_DISP32_EAX(5, 0x140)
		CHK_VLOADD_DISP32_EAX(6, 0x180)
		CHK_VLOADD_DISP32_EAX(7, 0x1c0)
		CHK_VLOADD_DISP32_EAX(8, 0x200)
		CHK_VLOADD_DISP32_EAX(9, 0x240)
		CHK_VLOADD_DISP32_EAX(10, 0x280)
		CHK_VLOADD_DISP32_EAX(11, 0x2c0)
		CHK_VLOADD_DISP32_EAX(12, 0x300)
		CHK_VLOADD_DISP32_EAX(13, 0x340)
		CHK_VLOADD_DISP32_EAX(14, 0x380)
		CHK_VLOADD_DISP32_EAX(15, 0x3c0)
		CHK_VLOADD_DISP32_EAX(16, 0x400)
		CHK_VLOADD_DISP32_EAX(17, 0x440)
		CHK_VLOADD_DISP32_EAX(18, 0x480)
		CHK_VLOADD_DISP32_EAX(19, 0x4c0)
		CHK_VLOADD_DISP32_EAX(20, 0x500)
		CHK_VLOADD_DISP32_EAX(21, 0x540)
		CHK_VLOADD_DISP32_EAX(22, 0x580)
		CHK_VLOADD_DISP32_EAX(23, 0x5c0)
		CHK_VLOADD_DISP32_EAX(24, 0x600)
		CHK_VLOADD_DISP32_EAX(25, 0x640)
		CHK_VLOADD_DISP32_EAX(26, 0x680)
		CHK_VLOADD_DISP32_EAX(27, 0x6c0)
		CHK_VLOADD_DISP32_EAX(28, 0x700)
		CHK_VLOADD_DISP32_EAX(29, 0x740)
		CHK_VLOADD_DISP32_EAX(30, 0x780)
		CHK_VLOADD_DISP32_EAX(31, 0x7c0)
		CHK_LDVXCSR_DISP32_EAX(0x810)
		"2:\n"
		".section .fixup,\"ax\"\n"
		"3:  movl $-1,%[err]\n"
		"    jmp  2b\n"
		".previous\n"
		: [err] "=r" (err)
		: [fx] "a" (vpustate), "m" (*vpustate), "0" (0) : "ebx"
	);

	return err;
}

static inline int restore_vpu_checking(struct vpustate_struct *vpustate)
{
	int err;

	err = restore_vpu_checking_nomask(vpustate);
	if (!err)
		err = __mic_restore_mask_regs(vpustate);
	return err;
}

static inline int save_vpu_checking(struct vpustate_struct *vpustate)
{
	int err;

	asm volatile(
		CHK_VSTORED_DISP32_EAX(0, 0x00)
		CHK_VSTORED_DISP32_EAX(1, 0x40)
		CHK_VSTORED_DISP32_EAX(2, 0x80)
		CHK_VSTORED_DISP32_EAX(3, 0xc0)
		CHK_VSTORED_DISP32_EAX(4, 0x100)
		CHK_VSTORED_DISP32_EAX(5, 0x140)
		CHK_VSTORED_DISP32_EAX(6, 0x180)
		CHK_VSTORED_DISP32_EAX(7, 0x1c0)
		CHK_VSTORED_DISP32_EAX(8, 0x200)
		CHK_VSTORED_DISP32_EAX(9, 0x240)
		CHK_VSTORED_DISP32_EAX(10, 0x280)
		CHK_VSTORED_DISP32_EAX(11, 0x2c0)
		CHK_VSTORED_DISP32_EAX(12, 0x300)
		CHK_VSTORED_DISP32_EAX(13, 0x340)
		CHK_VSTORED_DISP32_EAX(14, 0x380)
		CHK_VSTORED_DISP32_EAX(15, 0x3c0)
		CHK_VSTORED_DISP32_EAX(16, 0x400)
		CHK_VSTORED_DISP32_EAX(17, 0x440)
		CHK_VSTORED_DISP32_EAX(18, 0x480)
		CHK_VSTORED_DISP32_EAX(19, 0x4c0)
		CHK_VSTORED_DISP32_EAX(20, 0x500)
		CHK_VSTORED_DISP32_EAX(21, 0x540)
		CHK_VSTORED_DISP32_EAX(22, 0x580)
		CHK_VSTORED_DISP32_EAX(23, 0x5c0)
		CHK_VSTORED_DISP32_EAX(24, 0x600)
		CHK_VSTORED_DISP32_EAX(25, 0x640)
		CHK_VSTORED_DISP32_EAX(26, 0x680)
		CHK_VSTORED_DISP32_EAX(27, 0x6c0)
		CHK_VSTORED_DISP32_EAX(28, 0x700)
		CHK_VSTORED_DISP32_EAX(29, 0x740)
		CHK_VSTORED_DISP32_EAX(30, 0x780)
		CHK_VSTORED_DISP32_EAX(31, 0x7c0)
		CHK_VKSTORE_DISP32_EAX(0, 0x800)
		CHK_VKSTORE_DISP32_EAX(1, 0x802)
		CHK_VKSTORE_DISP32_EAX(2, 0x804)
		CHK_VKSTORE_DISP32_EAX(3, 0x806)
		CHK_VKSTORE_DISP32_EAX(4, 0x808)
		CHK_VKSTORE_DISP32_EAX(5, 0x80a)
		CHK_VKSTORE_DISP32_EAX(6, 0x80c)
		CHK_VKSTORE_DISP32_EAX(7, 0x80e)
		CHK_STVXCSR_DISP32_EAX(0x810)
		"2:\n"
		".section .fixup,\"ax\"\n"
		"3:  movl $-1,%[err]\n"
		"    jmp  2b\n"
		".previous\n"
		: [err] "=r" (err), "=m" (*vpustate)
		: [fx] "a" (vpustate), "0" (0) : "ebx"
	);

	if (unlikely(err) && __clear_user(vpustate, sizeof(struct vpustate_struct)))
		err = -EFAULT;
	/* No need to clear here because the caller clears USED_MATH */
	return err;
}

static inline void __vpu_clear(struct xsave_struct *fx)
{
	__asm__ __volatile__(
		VSTORED_DISP32_EAX(0, 0x00)
		VSTORED_DISP32_EAX(1, 0x40)
		VSTORED_DISP32_EAX(2, 0x80)
		VSTORED_DISP32_EAX(3, 0xc0)
		VSTORED_DISP32_EAX(4, 0x100)
		VSTORED_DISP32_EAX(5, 0x140)
		VSTORED_DISP32_EAX(6, 0x180)
		VSTORED_DISP32_EAX(7, 0x1c0)
		VSTORED_DISP32_EAX(8, 0x200)
		VSTORED_DISP32_EAX(9, 0x240)
		VSTORED_DISP32_EAX(10, 0x280)
		VSTORED_DISP32_EAX(11, 0x2c0)
		VSTORED_DISP32_EAX(12, 0x300)
		VSTORED_DISP32_EAX(13, 0x340)
		VSTORED_DISP32_EAX(14, 0x380)
		VSTORED_DISP32_EAX(15, 0x3c0)
		VSTORED_DISP32_EAX(16, 0x400)
		VSTORED_DISP32_EAX(17, 0x440)
		VSTORED_DISP32_EAX(18, 0x480)
		VSTORED_DISP32_EAX(19, 0x4c0)
		VSTORED_DISP32_EAX(20, 0x500)
		VSTORED_DISP32_EAX(21, 0x540)
		VSTORED_DISP32_EAX(22, 0x580)
		VSTORED_DISP32_EAX(23, 0x5c0)
		VSTORED_DISP32_EAX(24, 0x600)
		VSTORED_DISP32_EAX(25, 0x640)
		VSTORED_DISP32_EAX(26, 0x680)
		VSTORED_DISP32_EAX(27, 0x6c0)
		VSTORED_DISP32_EAX(28, 0x700)
		VSTORED_DISP32_EAX(29, 0x740)
		VSTORED_DISP32_EAX(30, 0x780)
		VSTORED_DISP32_EAX(31, 0x7c0)
		VKSTORE_DISP32_EAX(0, 0x800)
		VKSTORE_DISP32_EAX(1, 0x802)
		VKSTORE_DISP32_EAX(2, 0x804)
		VKSTORE_DISP32_EAX(3, 0x806)
		VKSTORE_DISP32_EAX(4, 0x808)
		VKSTORE_DISP32_EAX(5, 0x80a)
		VKSTORE_DISP32_EAX(6, 0x80c)
		VKSTORE_DISP32_EAX(7, 0x80e)
		STVXCSR_DISP32_EAX(0x810)

		:: "a" (&fx->vpu) : "ebx"
	);
}

static inline void save_init_vpu(struct xsave_struct *fx)
{
	__vpu_clear(fx);
}
#endif /* __ASM_X86_64_VPU_H */
