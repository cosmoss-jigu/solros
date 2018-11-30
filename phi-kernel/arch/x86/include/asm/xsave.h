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

#ifndef __ASM_X86_XSAVE_H
#define __ASM_X86_XSAVE_H

#include <linux/types.h>
#include <asm/processor.h>
#include <asm/i387.h>  //MICBUGBUG: include file loop!
#ifdef CONFIG_X86_EARLYMIC
#include <asm/mic_ni.h>
#endif

#define XSTATE_CPUID		0x0000000d

#define XSTATE_FP	0x1
#define XSTATE_SSE	0x2
#define XSTATE_YMM	0x4
#ifdef CONFIG_X86_EARLYMIC
#define XSTATE_ZMM	0x8
#endif

#define XSTATE_FPSSE	(XSTATE_FP | XSTATE_SSE)

#define FXSAVE_SIZE	512

#define XSAVE_HDR_SIZE	    64
#define XSAVE_HDR_OFFSET    FXSAVE_SIZE

#define XSAVE_YMM_SIZE	    256
#define XSAVE_YMM_OFFSET    (XSAVE_HDR_SIZE + XSAVE_HDR_OFFSET)

/*
 * These are the features that the OS can handle currently.
 */
#ifdef CONFIG_X86_EARLYMIC
#define XCNTXT_MASK	(XSTATE_FP | XSTATE_SSE | XSTATE_YMM | XSTATE_ZMM)
#else
#define XCNTXT_MASK	(XSTATE_FP | XSTATE_SSE | XSTATE_YMM)
#endif


#ifdef CONFIG_X86_64
#define REX_PREFIX	"0x48, "
#else
#define REX_PREFIX
#endif

extern unsigned int xstate_size;
extern u64 pcntxt_mask;
extern u64 xstate_fx_sw_bytes[USER_XSTATE_FX_SW_WORDS];

extern void xsave_init(void);
extern void update_regset_xstate_info(unsigned int size, u64 xstate_mask);
extern int init_fpu(struct task_struct *child);
extern int check_for_xstate(struct i387_fxsave_struct __user *buf,
			    void __user *fpstate,
			    struct _fpx_sw_bytes *sw);

//MICBUGBUG: workaround include file loop
static inline int fxrstor_checking(struct i387_fxsave_struct *fx);
static inline int fxsave_user(struct i387_fxsave_struct __user *fx);
//static inline void fpu_fxsave(struct xsave_struct *tsk);

#ifdef CONFIG_MK1OM
static inline int _mic_restore_mask_regs(struct xsave_struct *fx)
{
       return __mic_restore_mask_regs(&fx->vpu);
}
#endif

static inline int fpu_xrstor_checking(struct fpu *fpu)
{
	struct xsave_struct *fx = &fpu->state->xsave;
	int err;
#ifdef CONFIG_X86_EARLYMIC
	err = fxrstor_checking(&fx->i387);
#ifdef CONFIG_ML1OM
	if (!err)
		err = restore_vpu_checking(&fx->vpu);
#else
	if (!err)
		err = restore_vpu_checking_nomask(&fx->vpu);
#endif
#else
	asm volatile("1: .byte " REX_PREFIX "0x0f,0xae,0x2f\n\t"
		     "2:\n"
		     ".section .fixup,\"ax\"\n"
		     "3:  movl $-1,%[err]\n"
		     "    jmp  2b\n"
		     ".previous\n"
		     _ASM_EXTABLE(1b, 3b)
		     : [err] "=r" (err)
		     : "D" (fx), "m" (*fx), "a" (-1), "d" (-1), "0" (0)
		     : "memory");
#endif

	return err;
}

static inline int xsave_user(struct xsave_struct __user *buf)
{
	int err;
#ifdef CONFIG_X86_EARLYMIC
	struct xsave_struct *xstate = ((__force struct xsave_struct *)buf);
#endif
	/*
	 * Clear the xsave header first, so that reserved fields are
	 * initialized to zero.
	 */
	err = __clear_user(&buf->xsave_hdr,
			   sizeof(struct xsave_hdr_struct));
	if (unlikely(err))
		return -EFAULT;
#ifdef CONFIG_X86_EARLYMIC
	err = fxsave_user(&xstate->i387);
	if (!err)
		err = save_vpu_checking(&xstate->vpu);
#else
	__asm__ __volatile__("1: .byte " REX_PREFIX "0x0f,0xae,0x27\n"
			     "2:\n"
			     ".section .fixup,\"ax\"\n"
			     "3:  movl $-1,%[err]\n"
			     "    jmp  2b\n"
			     ".previous\n"
			     ".section __ex_table,\"a\"\n"
			     _ASM_ALIGN "\n"
			     _ASM_PTR "1b,3b\n"
			     ".previous"
			     : [err] "=r" (err)
			     : "D" (buf), "a" (-1), "d" (-1), "0" (0)
			     : "memory");
#endif
	if (unlikely(err) && __clear_user(buf, xstate_size))
		err = -EFAULT;
	/* No need to clear here because the caller clears USED_MATH */
	return err;
}

static inline int xrestore_user(struct xsave_struct __user *buf, u64 mask)
{
	int err;
	struct xsave_struct *xstate = ((__force struct xsave_struct *)buf);

#ifdef CONFIG_X86_EARLYMIC
	err = fxrstor_checking(&xstate->i387);
	if (!err)
		err = restore_vpu_checking(&xstate->vpu);
#else
	u32 lmask = mask;
	u32 hmask = mask >> 32;

	__asm__ __volatile__("1: .byte " REX_PREFIX "0x0f,0xae,0x2f\n"
			     "2:\n"
			     ".section .fixup,\"ax\"\n"
			     "3:  movl $-1,%[err]\n"
			     "    jmp  2b\n"
			     ".previous\n"
			     ".section __ex_table,\"a\"\n"
			     _ASM_ALIGN "\n"
			     _ASM_PTR "1b,3b\n"
			     ".previous"
			     : [err] "=r" (err)
			     : "D" (xstate), "a" (lmask), "d" (hmask), "0" (0)
			     : "memory");	/* memory required? */
#endif
	return err;
}

static inline void xrstor_state(struct xsave_struct *fx, u64 mask)
{
#ifndef CONFIG_X86_EARLYMIC
	u32 lmask = mask;
	u32 hmask = mask >> 32;

	asm volatile(".byte " REX_PREFIX "0x0f,0xae,0x2f\n\t"
		     : : "D" (fx), "m" (*fx), "a" (lmask), "d" (hmask)
		     :   "memory");
#endif
}

static inline void xsave_state(struct xsave_struct *fx, u64 mask)
{
//#ifdef CONFIG_X86_EARLYMIC
//	fpu_fxsave(fx->fpu);
//	save_init_vpu(fx);
//#else
	u32 lmask = mask;
	u32 hmask = mask >> 32;

	asm volatile(".byte " REX_PREFIX "0x0f,0xae,0x27\n\t"
		     : : "D" (fx), "m" (*fx), "a" (lmask), "d" (hmask)
		     :   "memory");
//#endif
}

static inline void fpu_xsave(struct fpu *fpu)
{
	/* This, however, we can work around by forcing the compiler to select
	   an addressing mode that doesn't require extended registers. */
	alternative_input(
		".byte " REX_PREFIX "0x0f,0xae,0x27",
		".byte " REX_PREFIX "0x0f,0xae,0x37",
		X86_FEATURE_XSAVEOPT,
		[fx] "D" (&fpu->state->xsave), "a" (-1), "d" (-1) :
		"memory");
}
#endif
