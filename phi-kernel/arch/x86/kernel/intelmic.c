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
 * intelmic.c: Intel MIC platform specific setup code
 *
 * (C) Copyright 2010 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sfi.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/irqflags.h>
#include <linux/time.h>

#include <asm/setup.h>
#include <asm/mpspec_def.h>
#include <asm/hw_irq.h>
#include <asm/apic.h>
#include <asm/io_apic.h>
#include <asm/io.h>
#include <asm/i8259.h>
#include <asm/reboot.h>
#include <asm/mic_def.h>
#include <asm/mce.h>

#ifdef CONFIG_X86_MIC_EMULATION
#define MIC_CORE_FREQ 200000UL /* 200 KHz for emulation */
#endif

#define MIC_SBOX_BASE			0x08007D0000ULL     /* PCIE SBox Registers */
#define MIC_SBOX_MMIO_SIZE		(64*1024)

#define SNOOP_ON                (0 << 0)
#define SNOOP_OFF               (1 << 0)
#define MIC_SYSTEM_PAGE_SHIFT   34ULL
#define NUM_SMPT_ENTRIES_IN_USE 32
#define MIC_SYSTEM_PAGE_SIZE    0x0400000000ULL
#define SBOX_SMPT00             0x00003100

/* Sbox Smpt Reg Bits:
 * Bits 	31:2	Host address
 * Bits 	1	RSVD
 * Bits		0	No snoop
 */
#define BUILD_SMPT(NO_SNOOP, HOST_ADDR)  \
	(uint32_t)(((((HOST_ADDR)<< 2) & (~0x03)) | ((NO_SNOOP) & (0x01))))

#ifdef CONFIG_MK1OM
#define SBOX_SDBIC1		0x0000CC94
#else
#define SBOX_SDBIC1		0x00009034
#endif

/* vnet/mic_shutdown/hvc/virtio/pc35_wakeup */
unsigned int sbox_irqs[5];

void __iomem *mic_sbox_mmio_va;
EXPORT_SYMBOL(mic_sbox_mmio_va);

#ifdef CONFIG_MK1OM
/* Function to init the SBOX ETC timer.	*/
extern void mic_timer_init(void);
extern int mic_etc_enabled;
#define	MIC_SBOX_CURRENT_CLK_RATIO	0x0000402C
#define	MIC_SBOX_SCRATCH4		0x0000AB30
#define	CORE_VCO			4000
#define	BITS_TO_DIV(x)			(((x) == 3) ? 1 : ((x) == 2) ? 2 : 4)
union sboxScratch4RegDef
{
	uint32_t m_value;
	struct
	{
		uint32_t thread_mask		: 4;    // bit 3:0   (Mask of enabled threads per core)
		uint32_t cache_size		: 2;    // bit 5:4   (0,1,2 = 512KB, 3 = 256KB)
		uint32_t gbox_channel_count	: 4;    // bit 9:6   (0 based channel count since we have to have at least 1 channel active)
		uint32_t			: 15;    // bit 24:10
		uint32_t icc_divider		: 5;    // bit 29:25 (4000 / IccDivider) = reference clock
		uint32_t soft_reset		: 1;    // bit 30:30 (1 if current boot was from a soft reset, 0 otherwise)
		uint32_t internal_flash		: 1;    // bit 31:31 (1 if flash is an internal build, 0 for external)
	};
};

union mclkRatioEncoding
{
	uint32_t value;
	struct
	{
		uint32_t			: 1;    // bits 0:0
		uint32_t fb			: 8;    // bits 8:1     (feedback divider)
		uint32_t ff			: 2;    // bits 10:9    (feedforward divider)
		uint32_t			: 21;   // bits 31:11
	};
};

static int get_core_freq(void)
{
	int core_vco;
	int ref_clk;
	union sboxScratch4RegDef s4_reg;
	union mclkRatioEncoding encoded_value;

	// Read ICC divider so we know what the ICC output is
	s4_reg.m_value = readl(mic_sbox_mmio_va + MIC_SBOX_SCRATCH4);

	// Read the current clock ratio
	encoded_value.value = readl(mic_sbox_mmio_va + MIC_SBOX_CURRENT_CLK_RATIO);

	// Calculate reference clock (reference is constant, divider changes)
	ref_clk = CORE_VCO / s4_reg.icc_divider;

	// Calculate the core clock
	core_vco = (ref_clk * encoded_value.fb) / BITS_TO_DIV(encoded_value.ff);

	return core_vco;
}
#else
/* PLL Data structure - This has sufficient data to generate Gpu and Memory frequency
 * table dynamically and thus avoiding the long memory consuming table. And also it is useful in
 * converting the frequencies from Mhz to Ratio/Divider format and vice versa
 * This data structure is indexed by group(range) of frequencies having same Divider values
 */
struct pll_info
{
	uint8_t clk_div;     /* clock divider */
	uint8_t min_ratio;   /* Minimum ratio */
	uint8_t max_ratio;   /* Maximum ratio */
	uint16_t min_clk;    /* Minimum Clk (frequency) */
	uint16_t max_clk;    /* Maximum Clk (frequency) */
	uint8_t step_size;   /* By how much are the frequency incrementing */
};

#define MIC_SBOX_CURRENT_CLK_RATIO	0x00003004
#define CLK_DIVIDER			0xC0
#define CLK_RATIO			0x3F
#define B_CLK				100

/* This table is specific to ML1OM core. */
static struct pll_info pll_info_gpu[] = {
	{ 1, 20, 40, 2000, 4000, 100},
	{ 2, 20, 39, 1000, 1950, 50},
	{ 4, 20, 39, 500, 975, 25}
};

/*************************************************************************
    Core Frequency
*************************************************************************/
union sboxCorefreqReg
{
	uint32_t value;
	struct
	{
		uint32_t ratio		: 8; /* bit 0-7 Ratio */
		uint32_t rsvd0		: 8; /* bit 8-15 */
		uint32_t asyncmode	: 1; /* bit 16 Async Mode. Field pulled out from extclkfreq by SW team */
		uint32_t extclkfreq	: 4; /* bit 17-20 ExtClkFreq */
		uint32_t rsvd1		: 6; /* bit 21-26 */
		uint32_t ratiostep	: 3; /* bit 27-29 Power throttle ratio-step */
		uint32_t jumpratio	: 1; /* bit 30 Power throttle jump at once */
		uint32_t switch_bit	: 1; /* bit 31 Switch to the value from fuse or the register */
	} bits;
};

/*
 * Convert ratio to Mhz
 * @param freq_ratio in ratio/divider format
 * @param pll_info, pll Table
 * @param len, Length of the table
 * @return ratio , 0 as failure
 */
static int ratio_to_mhz(int freq_ratio, struct pll_info *pll_table, int len)
{
	int i, ret_val = 0;

	/*
	 * Inverting the clock divider can be used as index for PLL
	 * table This is derived by hand from the table of encoded PLL
	 * frequencies It won't have to change until K1OM (which will
	 * require substantial changes, apparently).
	 */
	i = ~((freq_ratio & CLK_DIVIDER) >> 6) & 0x3;
	freq_ratio &= CLK_RATIO;

	/*
	 * check if the given ratio and divider matches the core PLL
	 * table's range
	 */
	if (i < len &&
		(int)pll_table[i].min_ratio <= freq_ratio &&
		(int)pll_table[i].max_ratio >= freq_ratio) {
		ret_val = freq_ratio * B_CLK;
		(void) do_div(ret_val, (int)pll_table[i].clk_div);
	}
	return ret_val;
}
#endif

void __init mic_sbox_md_init(void);

#ifdef CONFIG_MK1OM
static inline void __delay_atleast_ms(int cnt)
{
	int i, j;
	for (i = 0; i < cnt; i++)
		for (j = 0; j < 2000; j++) {
			/* 1000 cycles assumes 1GZ clock rate
			 * Looping for 2000 times makes sure that we delay
			 * for atleast one ms for any frequency upto 2GZ
			 */
			asm volatile ("delay %0" :: "r" (1000) : "memory");
		}
}
#endif

static int __init __calculate_tsc_comp(int64_t hz)
{
	int64_t deltahz;
#ifdef CONFIG_MK1OM
	/* max compensation due to ICC SSC comp is 2.5%
	 * So the max deviation is 1/2 * 2.5%
	 */
	int64_t max_tsc_comp = (5 * hz) / 400;
	uint64_t tsc1, tsc2, deltatsc;
	uint64_t etc1, etc2, deltaetc;
	int attempts = 0;
	unsigned long flags;
#define MAX_ATTEMPTS 10
	do {
		local_irq_save(flags);
		rdtscll(tsc1);
		etc1 = mic_etc_read(NULL);
		local_irq_restore(flags);
		/* Random value of 3secs
		 * That should be big enough
		 */
		__delay_atleast_ms(3000);

		local_irq_save(flags);
		rdtscll(tsc2);
		etc2 = mic_etc_read(NULL);
		local_irq_restore(flags);

		deltaetc = etc2 - etc1;
		deltatsc = tsc2 - tsc1;
		deltahz  = (deltatsc * (ETC_TICK_RATE + etc_comp)) / deltaetc;
		deltahz -= hz;
		attempts ++;
	} while (attempts < MAX_ATTEMPTS && (deltahz > max_tsc_comp ||
						deltahz < -max_tsc_comp));
	if (deltahz > max_tsc_comp || deltahz < -max_tsc_comp)
		printk("Tsc compensation %lld is > max_tsc_comp(%lld) or <"
				"min_tsc_comp(%lld)\n",
				deltahz, max_tsc_comp, -max_tsc_comp);

	deltahz = min(max_tsc_comp, deltahz);
	deltahz = max(-max_tsc_comp, deltahz);
	printk("tsc_comp = %lld, etc_comp = %d\n", deltahz, etc_comp);
#else
	deltahz = 0;
#endif
	return deltahz;
}

extern int delay_us_cycles;
extern int delay_us_cycles_extra;
static unsigned long __init intel_mic_calibrate_tsc(void)
{
	unsigned long khz;
#ifdef CONFIG_ML1OM
	union sboxCorefreqReg core_freq_reg;
#endif
	mic_sbox_md_init();
#ifdef CONFIG_X86_MIC_EMULATION
	khz = MIC_CORE_FREQ;
	(void) do_div(khz, 1000);
#elif defined CONFIG_ML1OM
	core_freq_reg.value = readl(mic_sbox_mmio_va + MIC_SBOX_CURRENT_CLK_RATIO);
	delay_us_cycles_extra = ratio_to_mhz(core_freq_reg.bits.ratio, pll_info_gpu, ARRAY_SIZE(pll_info_gpu));
#else
	delay_us_cycles_extra = get_core_freq();
#endif
#ifndef CONFIG_X86_MIC_EMULATION
	delay_us_cycles_extra *= 1000 * 1000;
	delay_us_cycles_extra += __calculate_tsc_comp(delay_us_cycles_extra);
	delay_us_cycles_extra /= 1000;
	khz = delay_us_cycles_extra;
	delay_us_cycles_extra /= 1000;
	delay_us_cycles_extra -= delay_us_cycles;
#endif
	return khz;
}

#ifdef CONFIG_ML1OM
/*------------------------------------------------------------------------------
 *  FUNCTION: mic_smpt_init
 *
 *  DESCRIPTION: Initializes 1-1 mapping
 *  i.e 0-512GB -> MIC_SYSTEM_BASE-(MIC_SYSTEM_BASE + 512GB)
 *
 *  PARAMETERS: None
 *
 *  RETURNS: None
*------------------------------------------------------------------------------*/
void __init mic_smpt_init(void)
{
	uint32_t *smpt_offset;
	uint32_t smpt_reg_val;
	int i;

	if(!mic_sbox_mmio_va)
		mic_sbox_md_init();

	smpt_offset = mic_sbox_mmio_va + SBOX_SMPT00;

	for (i = 0; i < NUM_SMPT_ENTRIES_IN_USE; i++) {
		smpt_reg_val = BUILD_SMPT(SNOOP_ON, (i * MIC_SYSTEM_PAGE_SIZE)
					 >> MIC_SYSTEM_PAGE_SHIFT);
		writel(smpt_reg_val, &smpt_offset[i]);
	}
}
#else
void __init mic_smpt_init(void)
{
}
#endif

static unsigned long intel_mic_get_wallclock(void)
{
	return 0UL;
}

static int intel_mic_set_wallclock(unsigned long nowtime)
{
	return -1;
}

void __init mic_sbox_md_init(void)
{
	/* When do we unmap sbox or do we even care? */
	/* We need to map sbox mmio when we init the ETC counter
 	   on KNC. 
	*/
	if(!mic_sbox_mmio_va){ 
		mic_sbox_mmio_va = ioremap(MIC_SBOX_BASE, MIC_SBOX_MMIO_SIZE);
		BUG_ON(!mic_sbox_mmio_va);
	}
	return;
}

#ifdef	CONFIG_MK1OM
static void mic_timer_init_common(void)
{
#ifndef CONFIG_X86_MIC_EMULATION
	if (mic_etc_enabled)
		mic_timer_init();
	else
#endif
		x86_init_noop();
}
#endif

/*------------------------------------------------------------------------------
 *  FUNCTION: mic_shutdown
 *
 *  DESCRIPTION: Notifies the host about a MIC shutdown/poweroff/restart via
 *  writing to a doorbell register which interrupts the host.
 *
 *  PARAMETERS: system state.
 *
 *  RETURNS: none.
*------------------------------------------------------------------------------*/
void mic_shutdown(uint16_t state)
{
	uint32_t db_reg;
#define SBOX_SDBIC1_DBREQ_BIT   0x80000000

	db_reg = state | SBOX_SDBIC1_DBREQ_BIT;
	writel(db_reg, mic_sbox_mmio_va + SBOX_SDBIC1);
	printk(KERN_ALERT "%s: system state %d dbreg 0x%x\n",
			__func__, state, db_reg);
}

void _mic_shutdown(void)
{
	native_machine_shutdown();
	mic_shutdown(system_state);
}

#ifdef CONFIG_KEXEC
void _mic_crash_shutdown(struct pt_regs *regs)
{
	native_machine_crash_shutdown(regs);
	/*
	 * Only alert host outside of MC handling context
	 */
	if (! atomic_read(&mce_entry))
		mic_shutdown(0xdead);
	/*
	 * Halt since MIC does not want to load crash kernel itself because
	 * host kernel will capture the kernel core dump.
	 */
	while(1)
		halt();
}
#endif

/*------------------------------------------------------------------------------
 *  FUNCTION: mic_shutdown_isr
 *
 *  DESCRIPTION: This interrupt service routine is executed when the host
 *  triggers an interrupt telling the uOS that an orderly shutdown should be
 *  performed by writing to SBOX RDMASR0.
*------------------------------------------------------------------------------*/
static irqreturn_t mic_shutdown_isr(int irq, void *unused)
{
	orderly_poweroff(true);
	return IRQ_HANDLED;
}

/*------------------------------------------------------------------------------
 *  FUNCTION: mic_setup_isr
 *
 *  DESCRIPTION: This function registers an interrupts for regular host
 *  interrupts and host shutdown interrupt.
 *  Host requests an orderly uOS shutdown by writing to SBOX APICICR1.
*------------------------------------------------------------------------------*/
void __init mic_setup_isr(void)
{
	int ret;
	int i;
	arch_setup_sbox_irqs(sbox_irqs, ARRAY_SIZE(sbox_irqs));
	if (ret = request_irq(sbox_irqs[1], mic_shutdown_isr, IRQF_DISABLED, "Shutdown", NULL)) {
		printk("ret value=%d, irq=%d\n", ret, sbox_irqs[1]);
		BUG_ON(1);
	}
	printk("irq for shutdown isr %d\n", sbox_irqs[1]);
}
late_initcall(mic_setup_isr);

int get_sbox_irq(int index)
{
	BUG_ON(index >= ARRAY_SIZE(sbox_irqs));
	return sbox_irqs[index];
}
EXPORT_SYMBOL(get_sbox_irq);

/*
 * Intel MIC specific x86_init function overrides and early setup
 * calls.
 */
void __init x86_mic_early_setup(void)
{
	x86_init.resources.probe_roms = x86_init_noop;
	x86_init.resources.reserve_resources = x86_init_noop;

#ifdef CONFIG_MK1OM
	x86_init.timers.timer_init = mic_timer_init_common;
#else
	x86_init.timers.timer_init = x86_init_noop;
#endif	
	x86_init.irqs.pre_vector_init = x86_init_noop;

	x86_platform.calibrate_tsc = intel_mic_calibrate_tsc;
	x86_platform.get_wallclock = intel_mic_get_wallclock;
	x86_platform.set_wallclock = intel_mic_set_wallclock;

	machine_ops.shutdown  = _mic_shutdown;
#ifdef CONFIG_KEXEC
	machine_ops.crash_shutdown = _mic_crash_shutdown;
#endif
	legacy_pic = &null_legacy_pic;
	mic_sbox_mmio_va = NULL;
	no_sync_cmos_clock = 1;
}

static inline void assign_to_mp_irq(struct mpc_intsrc *m,
				    struct mpc_intsrc *mp_irq)
{
	memcpy(mp_irq, m, sizeof(struct mpc_intsrc));
}

static inline int mp_irq_cmp(struct mpc_intsrc *mp_irq,
				struct mpc_intsrc *m)
{
	return memcmp(mp_irq, m, sizeof(struct mpc_intsrc));
}

static inline void __init MP_intsrc_info(struct mpc_intsrc *m)
{
	int i;

	for (i = 0; i < mp_irq_entries; i++) {
		if (!mp_irq_cmp(&mp_irqs[i], m))
			return;
	}
	assign_to_mp_irq(m, &mp_irqs[mp_irq_entries]);
	if (++mp_irq_entries == MAX_IRQ_SOURCES)
		panic("Max # of irq sources exceeded!!\n");
}

void  __init mic_construct_default_ioirq_mptable(int mpc_default_type)
{
	struct mpc_intsrc intsrc;
	int i;

	/*
	 * 0	DMA Completion Interrupt for Channel 0 NOT USED
	 * 1	DMA Completion Interrupt for Channel 1 NOT USED
	 * 2	DMA Completion Interrupt for Channel 2 NOT USED
	 * 3	DMA Completion Interrupt for Channel 3 NOT USED
	 * 4	DMA Completion Interrupt for Channel 4
	 * 5	DMA Completion Interrupt for Channel 5
	 * 6	DMA Completion Interrupt for Channel 6
	 * 7	DMA Completion Interrupt for Channel 7
	 * 8	Display Rendering Related Interrupt A (RRA) NOT USED
	 * 9	Display Rendering Related Interrupt B (RRB) NOT USED
	 * 10	Display Non-Rendering Related Interrupt (NRR) NOT USED
	 * 11	Unused NOT USED
	 * 12	PMU Interrupt
	 * 13	Thermal Interrupt
	 * 14	SPI Interrupt NOT USED
	 * 15	Sbox Global APIC Error Interrupt
	 * 16	Machine Check Architecture (MCA) Interrupt
	 * 17	Remote DMA Completion Interrupt for Channel 0
	 * 18	Remote DMA Completion Interrupt for Channel 1
	 * 19	Remote DMA Completion Interrupt for Channel 2
	 * 20	Remote DMA Completion Interrupt for Channel 3
	 * 21	Remote DMA Completion Interrupt for Channel 4
	 * 22	Remote DMA Completion Interrupt for Channel 5
	 * 23	Remote DMA Completion Interrupt for Channel 6
	 * 24	Remote DMA Completion Interrupt for Channel 7
	 * 25	PSMI Interrupt NOT USED
	 */

	intsrc.type = MP_INTSRC;
	intsrc.srcbus = 0;
	intsrc.dstapic = MP_APIC_ALL;
	intsrc.irqtype = mp_INT;

	/* SBOX interrupts are all edge trigged */
	intsrc.irqflag = 5;
	for (i = 4; i < 8; i++){
		intsrc.srcbusirq = i;
		intsrc.dstirq = i;
		MP_intsrc_info(&intsrc);
	}

	intsrc.irqflag = 5;
	for (i = 12; i < 14; i++) {
		intsrc.srcbusirq = i;
		intsrc.dstirq = i;
		MP_intsrc_info(&intsrc);
	}

	intsrc.irqflag = 13;
	for (i = 15; i < 16; i++) {
		intsrc.srcbusirq = i;
		intsrc.dstirq = i;
		MP_intsrc_info(&intsrc);
	}

	intsrc.irqflag = 5;
	for (i = 16; i < 25; i++) {
		intsrc.srcbusirq = i;
		intsrc.dstirq = i;
		MP_intsrc_info(&intsrc);
	}
}

void save_APICICR_setup(uint64_t *apicicr)
{
	int i;
	for (i = 0; i < 8; i++) {
		apicicr[i] = readl(mic_sbox_mmio_va + SBOX_APICICR0 + i * 8 + 4);
		apicicr[i] <<= 32;
		apicicr[i] |= readl(mic_sbox_mmio_va + SBOX_APICICR0 + i * 8);
	}
}

void restore_APICICR_setup(uint64_t *apicicr)
{
	int i;
	for (i = 0; i < 8; i++) {
		writel(apicicr[i], mic_sbox_mmio_va + SBOX_APICICR0 + i * 8);
		writel((apicicr[i] >> 32), mic_sbox_mmio_va + SBOX_APICICR0 + i * 8 + 4);
	}
}
