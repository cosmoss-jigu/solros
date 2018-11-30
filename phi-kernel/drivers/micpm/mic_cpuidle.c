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
 * mic_cpuidle - cpuidle driver for the Intel MIC device
 *
 *   * (C) Copyright 2008 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>	/* need_resched() */
#include <linux/pm_qos_params.h>
#include <linux/clockchips.h>
#include <linux/cpuidle.h>
#include <linux/irqflags.h>
#include <linux/io.h>
#include <linux/time.h>
#include <linux/clocksource.h>
#include <asm/pgtable.h>
#include <asm/proto.h>
#include <asm/mtrr.h>
#include <asm/page.h>
#include <asm/mce.h>
#include <asm/xcr.h>
#include <asm/io_apic.h>
#include <asm/suspend.h>
#include <asm/debugreg.h>
#include <asm/timer.h>
#include <linux/delay.h>
#include <asm/idle.h>
#include <asm/mic/mic_common.h>
#include "mic_pm_card.h"
#include "micpm_common.h"
#include "mic_cpuidle.h"
#include "micidle_common.h"
#include "mic_event.h"
#include "pm_scif.h"
#include <linux/completion.h>


#define PREFIX "mic_cpuidle: "

#define MIC_CPUIDLE_DEBUG

#ifdef	MIC_CPUIDLE_DEBUG
#ifdef CONFIG_GCOV_KERNEL
#define dprintk(msg...) printk(KERN_INFO PREFIX msg);
#else
#define dprintk(msg...) pr_debug(PREFIX msg);
#endif
#else
#define dprintk
#endif

#define MAX_KNC_CSTATES		3	/* Core C1, Core C6 & Package C6 */
#define MIC_CX_DESC_LEN		32

#define MIC_CC6_STATUS		0x342	/* MSR for Core C6 Status Register      */

/* TODO: Need to get this number from bootstrap	*/
#define MAX_GBOXES		8
#define MIC_GBOX_MMIO_SIZE	4096	/* Map just one page for each GBox      */
#define GBOX_WAKEUP_LATENCY	12	/* Worst case wakeup latency from M3 
					   in millisec.
					*/ 
#define GBOX_M2M3_THRESHOLD     5000000 /* Minimum package state residency
					   to allow GBoxes to go to M3. This is
					   the value of M3 timeout counter(~ 3 secs currently
					   plus a margin of 2 secs.
					*/	  
#define GBOX_LOOP_DELAY	50
#define GBOX_REPEAT_COUNT	5	/* Repeat GBOX writes for max of this many times to
					   workaround 3498934.
					*/ 	
#define PACKAGE_C3_ENTRY_DELAY	225	/*uS wait before entering PC3 */
#define PACKAGE_C3_EXIT_DELAY	3	/* same for exit        */

#define CPUIDLE_FLAG_PKGSTATE	(0x200)	/* may need to change this if cpuidle.h includes more flag bits.        */

#define CC6_TRAMPOLINE_ADDRESS	(0x60000)
#define CC6_TRAMPOLINE_SIZE roundup(cc6_trampoline_end - cc6_trampoline_data,PAGE_SIZE)
#define PC6_TRAMPOLINE_ADDRESS  (0x70000)
#define PC6_TRAMPOLINE_SIZE roundup(pc6_trampoline_end - pc6_exit_64, PAGE_SIZE)
#define PC6_PAGETABLE_SIZE  roundup(pc6_trampoline_end - pc6_trampoline_pgt,PAGE_SIZE)
#define CC6_PAGETABLE_SIZE  roundup(cc6_trampoline_end - cc6_trampoline_level4_pgt,PAGE_SIZE)
#define MAX_CPUS	248
#define MAX_CORES (MAX_CPUS/4)

#define MSR_KNC_IA32_PERF_GLOBAL_CTRL	0x0000002f
#define MSR_KNC_PERF_FILT_MASK		0x0000002c



#define	MIC_READ_GBOX_MMIO_REG(var,offset)	\
var.value = readl((const volatile void __iomem *)((unsigned long)(mic_gbox_mmio_va) +  (offset)));

#define	MIC_WRITE_GBOX_MMIO_REG(var,offset) \
writel(var.value,(volatile void __iomem *)((unsigned long)(mic_gbox_mmio_va)+(offset)));

static unsigned long long gboxBaseAddress[MAX_GBOXES] = {
	GBOX0_BASE, GBOX1_BASE,
	GBOX2_BASE, GBOX3_BASE,
	GBOX4_BASE, GBOX5_BASE,
	GBOX6_BASE, GBOX7_BASE,
};
u32 __iomem *mic_gbox_mapped_address[MAX_GBOXES];

u32 __iomem *mic_gbox_mmio_va;

/* This defined in drivers/host/driver/mic_interrupts.h also. Keep them coherent */
#define PC35_WAKEUP_INT_IDX	4
extern int get_sbox_irq(int irq);

extern const unsigned char pc6_exit_64[];
extern const unsigned char pc6_trampoline_end[];
extern const unsigned char pc6_trampoline_pgt[];
extern const unsigned char trampoline_level4_pgt[];
extern const unsigned char cc6_trampoline_level4_pgt[];
extern const unsigned char cc6_trampoline_data[];
extern const unsigned char cc6_trampoline_end[];

extern const unsigned char tgdt[];
extern const unsigned char startup_32_vector[];
extern const unsigned char startup_64_vector[];

extern unsigned long set_mtrr_state(void);
extern void pat_init(void);

volatile u32 *apicid_ptr;
struct cc6_saved_context *cc6_cntxt[MAX_CPUS];
struct mic_apic_state lapic_store[MAX_CPUS];
extern int do_cc6entry_lowlevel(struct cc6_saved_context *cntxt, int cflush);
static int cntxt_created;
static struct IO_APIC_route_entry **ioapic_entries;
static uint64_t apicicr[8]; /* There are 8 APICICR registers */

static spinlock_t cc6lock[MAX_CORES];	/* Hold this lock for updating cc6status. */

static atomic_t cc6cpucnt[MAX_CORES];	//Holds the count of cpus in CC6 in each core  
static long cc6_cpumask[MAX_CORES];	//Holds the cc6 enable mask for each cpu in core   
unsigned long cc6_waketime[MAX_CPUS];	//Use this to calculate CC6 exit latency.

static atomic_t pc3cpucnt = ATOMIC_INIT(0);	//
static atomic_t stopcnt = ATOMIC_INIT(0);	//Count of cpus that have stopped their 
						//apic timers ready for PC3. 
						
static cpumask_var_t idlemask;	/* Mask of all cpus idle */
static spinlock_t idlelock;	/* Hold this lock for updating idlemask. */

int mic_cpuidle_inited = 0;
EXPORT_SYMBOL_GPL(mic_cpuidle_inited);

static int pc6exitdone;
static int devicesuspended;
static int gbox_m2m3_on;	/* Remember state of GBOX M2/M3 */ 
static unsigned long time_in_pc6; /* Time elapsed between each pc6 entry/exit */
extern int micpm_device_notify(unsigned long event, void *msg);
static int sbox_mmio_setup(void);

void tick_resume_oneshot(void);


#if 0
static int mic_cpuidle_atomic_notifier(struct notifier_block *nb, unsigned long event,void *data);


static struct notifier_block mic_cpuidle_atomic_notifier_block = {
	.notifier_call = mic_cpuidle_atomic_notifier,
};
#endif

/** sysfs support stuff	**/
unsigned int pc3_select_count;
unsigned int pc3_entry_count;
extern unsigned int pc3_disable_count;
unsigned int pc6_entry_count;

static unsigned int cc6bypolicy;/*Flag to turn ON/OFF CC6 at runtime.*/
static int mic_cc6_on;		/* If Core C6 state is available.       */
static int mic_pc6_on;
static int mic_pc3_on;

/* syfs controls to turn ON/OFF CC6 dynamically */
void mic_cpuidle_set_cc6policy(int policy)
{
	cc6bypolicy=policy;
}
unsigned int mic_cpuidle_get_cc6policy(void)
{

	return cc6bypolicy;
}

unsigned int mic_cpuidle_get_pc3_select_count(void)
{
	return (pc3_select_count);
}
unsigned int mic_cpuidle_get_pc3_entered_count(void)
{
	return (pc3_entry_count);
}
unsigned int mic_cpuidle_get_pc3_disable_count(void)
{
	return (pc3_disable_count);
}
unsigned int mic_cpuidle_get_pc6_entered_count(void)
{
	return (pc6_entry_count);
}
/* Make these visible to the proc routines that display stats */ 
unsigned int cc6entries[MAX_CPUS];
unsigned long cc6latency[MAX_CPUS];
unsigned long cc6residency[MAX_CPUS];

static unsigned long cc6entrytime[MAX_CORES];

DEFINE_PER_CPU(int, timer_stopped);

unsigned long pc6_ticks;
static ktime_t pc6_start;
unsigned long pc3_ticks;
static ktime_t pc3_start;

ssize_t mic_cpuidle_get_cpuidle_data(char *buf)
{
	int len = 0;
	int i, col;

	len += snprintf(buf + len, PAGE_SIZE - len, "\nCC6 entries:\n");
	for (i = 0, col = 0; i < MAX_CPUS; i++) {
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "%d,", cc6entries[i]);
		if (col++ > 16) {
			len += snprintf(buf + len, PAGE_SIZE - len, "\n");
			col = 0;
		}
	}
	return len;
};

DEFINE_PER_CPU(struct cpuidle_device *, pcidle);
struct cpuidle_driver mic_cpuidle_driver = {
	.name = "mic_cpuidle",
	.owner = THIS_MODULE,
};

//static int mic_device ;
static cpumask_var_t regdevices;
static unsigned int latency_factor __read_mostly = 2;

static cpumask_var_t idlemask;	/* Mask of all cpus idle */
static spinlock_t idlelock;	/* Hold this lock for updating idlemask.        */

static u8 uospmstate;		//shadow of the UOSPMSTATE H/W register

static int pkgstate_ready;	/* Flag to signal host is ready for package state
				   messages.
				*/
static int pc3aborted;

extern int pm_scif_send2host(PM_MESSAGE opcode, void *msg, size_t len);

//extern void mic_udelay(int n);

struct mic_cpuidle_states {
	int latency;
	int power;
	u32 address;
	char desc[MIC_CX_DESC_LEN];

};

static struct mic_cpuidle_states mic_cpuidle_data[MAX_KNC_CSTATES] = {
	{			/*data for Core C1      */
	 .latency = 20,
	 .power = 0,
	 .desc = "CC1 :Core C1 idle state"},
	{			/* data for Core C6 state       */
	 .latency = 4000,
	 .power = 0,
	 .desc = "CC6: Core C6 idle state"},
	{			/*data for Package C6   */
	 .latency = 800000,
	 .power = 0,
	 .desc = "PC3: Package C3 idle state"},

};

static void fix_processor_context(void)
{
	int cpu = smp_processor_id();
	struct tss_struct *t = &per_cpu(init_tss, cpu);

	set_tss_desc(cpu, t);	/*
				 * This just modifies memory; should not be
				 * necessary. But... This is necessary, because
				 * 386 hardware has concept of busy TSS or some
				 * similar stupidity.
				 */

	get_cpu_gdt_table(cpu)[GDT_ENTRY_TSS].type = 9;

	syscall_init();		/* This sets MSR_*STAR and related */
	load_TR_desc();		/* This does ltr */
	load_LDT(&current->active_mm->context);	/* This does lldt */
}

static void mic_save_processor_state(int apicid)
{
	struct cc6_saved_context *ctxt = cc6_cntxt[apicid];
	/*
	 * descriptor tables
	 */
	store_gdt((struct desc_ptr *)&ctxt->gdt_limit);
	store_idt((struct desc_ptr *)&ctxt->idt_limit);
	store_tr(ctxt->tr);

	/* XMM0..XMM15 should be handled by kernel_fpu_begin(). */
	/*
	 * segment registers
	 */
	asm volatile ("movw %%ds, %0":"=m" (ctxt->ds));
	asm volatile ("movw %%es, %0":"=m" (ctxt->es));
	asm volatile ("movw %%fs, %0":"=m" (ctxt->fs));
	asm volatile ("movw %%gs, %0":"=m" (ctxt->gs));
	asm volatile ("movw %%ss, %0":"=m" (ctxt->ss));

	rdmsrl(MSR_FS_BASE, ctxt->fs_base);
	rdmsrl(MSR_GS_BASE, ctxt->gs_base);
	rdmsrl(MSR_KERNEL_GS_BASE, ctxt->gs_kernel_base);

	rdmsrl(MSR_EFER, ctxt->efer);
	/*
	 * control registers
	 */
	ctxt->cr0 = read_cr0();
	ctxt->cr2 = read_cr2();
	ctxt->cr3 = read_cr3();
	ctxt->cr4 = read_cr4();
	ctxt->cr8 = read_cr8();
	/*
	 * MCA related per-CPU MSRs
	 */
	rdmsrl(MSR_IA32_MCG_CTL, ctxt->mcg_ctl);
	rdmsrl(MSR_IA32_MCx_CTL(0), ctxt->mc0_ctl);
	rdmsrl(MSR_IA32_MCx_CTL(1), ctxt->mc1_ctl);
	rdmsrl(MSR_IA32_MCx_CTL(2), ctxt->mc2_ctl);

	rdmsrl(MSR_KNC_IA32_PERF_GLOBAL_CTRL, ctxt->pmu_glbl);
	rdmsrl(MSR_KNC_PERFCTR0,ctxt->pmu_ctr0);
	rdmsrl(MSR_KNC_PERFCTR1,ctxt->pmu_ctr1);
	rdmsrl(MSR_KNC_EVNTSEL0,ctxt->pmu_evt0);
	rdmsrl(MSR_KNC_EVNTSEL1,ctxt->pmu_evt1);
	rdmsrl(MSR_KNC_PERF_FILT_MASK,ctxt->pmu_filt);
}

/**
 *	__restore_processor_state - restore the contents of CPU registers saved
 *		by __save_processor_state()
 *	@ctxt - structure to load the registers contents from
 */
void mic_restore_processor_state(struct cc6_saved_context *ctxt)
{
	/*
	 * control registers
	 */
	/* cr4 was introduced in the Pentium CPU */
	wrmsrl(MSR_EFER, ctxt->efer);
	write_cr8(ctxt->cr8);
	/* cr0 .. cr4 saved by caller */
	/*
	 * now restore the descriptor tables to their proper values
	 * ltr is done i fix_processor_context().
	 */
	load_gdt((const struct desc_ptr *)&ctxt->gdt_limit);
	load_idt((const struct desc_ptr *)&ctxt->idt_limit);
	/*
	 * segment registers
	 */
	asm volatile ("movw %0, %%ds"::"r" (ctxt->ds));
	asm volatile ("movw %0, %%es"::"r" (ctxt->es));
	asm volatile ("movw %0, %%fs"::"r" (ctxt->fs));
	load_gs_index(ctxt->gs);
	asm volatile ("movw %0, %%ss"::"r" (ctxt->ss));

	wrmsrl(MSR_FS_BASE, ctxt->fs_base);
	wrmsrl(MSR_GS_BASE, ctxt->gs_base);
	wrmsrl(MSR_KERNEL_GS_BASE, ctxt->gs_kernel_base);
	/*
	 * restore XCR0 for xsave capable cpu's.
	 */
	if (cpu_has_xsave)
		xsetbv(XCR_XFEATURE_ENABLED_MASK, pcntxt_mask);

	fix_processor_context();
	set_mtrr_state();
	pat_init();
	/*
	 * MCA related per-CPU MSRs
	 */
	wrmsrl(MSR_IA32_MCx_CTL(0), ctxt->mc0_ctl);
	wrmsrl(MSR_IA32_MCx_CTL(1), ctxt->mc1_ctl);
	wrmsrl(MSR_IA32_MCx_CTL(2), ctxt->mc2_ctl);
	wrmsrl(MSR_IA32_MCG_CTL, ctxt->mcg_ctl);

	wrmsrl(MSR_KNC_IA32_PERF_GLOBAL_CTRL, ctxt->pmu_glbl);
	wrmsrl(MSR_KNC_PERFCTR0,ctxt->pmu_ctr0);
	wrmsrl(MSR_KNC_PERFCTR1,ctxt->pmu_ctr1);
	wrmsrl(MSR_KNC_EVNTSEL0,ctxt->pmu_evt0);
	wrmsrl(MSR_KNC_EVNTSEL1,ctxt->pmu_evt1);
	wrmsrl(MSR_KNC_PERF_FILT_MASK,ctxt->pmu_filt);
}

EXPORT_SYMBOL(mic_restore_processor_state);

static int lapic_save(struct mic_apic_state *lapic_saved)
{
	int maxlvt;

	maxlvt = lapic_get_maxlvt();
	lapic_saved->apic_id = apic_read(APIC_ID);
	lapic_saved->apic_taskpri = apic_read(APIC_TASKPRI);
	lapic_saved->apic_ldr = apic_read(APIC_LDR);
	lapic_saved->apic_dfr = apic_read(APIC_DFR);
	lapic_saved->apic_spiv = apic_read(APIC_SPIV);
	lapic_saved->apic_lvtt = apic_read(APIC_LVTT);
	if (maxlvt >= 4)
		lapic_saved->apic_lvtpc = apic_read(APIC_LVTPC);
	lapic_saved->apic_lvt0 = apic_read(APIC_LVT0);
	lapic_saved->apic_lvt1 = apic_read(APIC_LVT1);
	lapic_saved->apic_lvterr = apic_read(APIC_LVTERR);
	lapic_saved->apic_tmict = apic_read(APIC_TMICT);
	lapic_saved->apic_tdcr = apic_read(APIC_TDCR);
#ifdef CONFIG_X86_THERMAL_VECTOR
	if (maxlvt >= 5)
		lapic_saved->apic_thmr = apic_read(APIC_LVTTHMR);
#endif

	lapic_saved->active = 1;
	return 0;
}

static int lapic_restore(struct mic_apic_state *lapic_saved)
{
	int maxlvt;
	int ret = 0;
	if (!lapic_saved->active)
		return 0;

	maxlvt = lapic_get_maxlvt();
	apic_write(APIC_LVTERR, ERROR_APIC_VECTOR | APIC_LVT_MASKED);
	apic_write(APIC_ID, lapic_saved->apic_id);
	apic_write(APIC_DFR, lapic_saved->apic_dfr);
	apic_write(APIC_LDR, lapic_saved->apic_ldr);
	apic_write(APIC_TASKPRI, lapic_saved->apic_taskpri);
	apic_write(APIC_SPIV, lapic_saved->apic_spiv);
	apic_write(APIC_LVT0, lapic_saved->apic_lvt0);
	apic_write(APIC_LVT1, lapic_saved->apic_lvt1);
#if defined(CONFIG_X86_MCE_P4THERMAL) || defined(CONFIG_X86_MCE_INTEL)
	if (maxlvt >= 5)
		apic_write(APIC_LVTTHMR, lapic_saved->apic_thmr);
#endif
	if (maxlvt >= 4)
		apic_write(APIC_LVTPC, lapic_saved->apic_lvtpc);
	apic_write(APIC_LVTT, lapic_saved->apic_lvtt);
	apic_write(APIC_TDCR, lapic_saved->apic_tdcr);
	apic_write(APIC_TMICT, lapic_saved->apic_tmict);
	apic_write(APIC_ESR, 0);
	apic_read(APIC_ESR);
	apic_write(APIC_LVTERR, lapic_saved->apic_lvterr);
	apic_write(APIC_ESR, 0);
	apic_read(APIC_ESR);
	return ret;
}
static void free_contextdata(void)
{
	int i, cpu;
	struct cc6_saved_context *cntxt;

	for_each_online_cpu(cpu) {
		i = per_cpu(x86_cpu_to_apicid, cpu);
		cntxt = cc6_cntxt[i];
		if (!cntxt)
			kfree(cntxt);
		cc6_cntxt[i] = 0;
		cc6_waketime[i]=0;
	}
}

static int create_contextdata(void)
{
	struct cc6_saved_context *cntxt;
	int cpu, i;

	for_each_online_cpu(cpu) {
		i = per_cpu(x86_cpu_to_apicid, cpu);
		cntxt = kzalloc(sizeof(struct cc6_saved_context), GFP_KERNEL);
		if (!cntxt)
			goto freeexit;
		cc6_cntxt[i] = cntxt;
	}
	cntxt_created = 1;
	return 0;
freeexit:
	free_contextdata();
	return -ENOMEM;
}
static inline int pc6_through_pc3(void)
{
	return(mic_pc6_on && (boot_cpu_data.x86_mask == KNC_C_STEP)); 
}
static int mic_setup_pc6(void)
{
	void *base;

	/* Set up context save structures       */
	if (create_contextdata())
		return -ENOMEM;
	/* Create ioapic state save struct      */
	ioapic_entries = alloc_ioapic_entries();
	if (!ioapic_entries) {
		pr_err("Allocate ioapic_entries failed\n");
		return -ENOMEM;
	}
	/* Set up trampoline    */

	/* Copy levelpgt from boot trampoline. */
	memcpy((void *)pc6_trampoline_pgt, (const void *)trampoline_level4_pgt,
	       PC6_PAGETABLE_SIZE);
	base = __va(PC6_TRAMPOLINE_ADDRESS);
	memcpy(base, (const void *)pc6_exit_64, PC6_TRAMPOLINE_SIZE);
	wbinvd();
	return 0;
}

/**
* mic_setup_cc6 - does required initial setup to enable core c6
* Returns 1 on success 0 otherwise.
**/
static int mic_setup_c6(void)
{
	void *base;
	int i;
	unsigned int *vector;
	sboxC6ScratchReg reg;

	sbox_mmio_setup();

	/* Program SBOX scratch0 register to turn ON caches during CC6 exit     */
	MIC_READ_MMIO_REG(reg, SBOX_C6_SCRATCH0);
	reg.value |= 0x8000;	//Bit 15 of scratch 0 (C1-CC6 MAS)      
	MIC_WRITE_MMIO_REG(reg, SBOX_C6_SCRATCH0);

	/* reserve low memory for cc6 exit trampoline code (one time only)      */

	/* Fix up startup_32 and startup_64 vectors for trampoline load address */
	vector = (unsigned int *)(tgdt + 2);
	*vector += CC6_TRAMPOLINE_ADDRESS;
	vector = (unsigned int *)startup_32_vector;
	*vector += CC6_TRAMPOLINE_ADDRESS;
	vector = (unsigned int *)startup_64_vector;
	*vector += CC6_TRAMPOLINE_ADDRESS;
	/*set up cc6 exit trampoline (one time only) */
	memcpy((void *)cc6_trampoline_level4_pgt,
	       (const void *)trampoline_level4_pgt, CC6_PAGETABLE_SIZE);
	smp_mb();
	base = __va(CC6_TRAMPOLINE_ADDRESS);
	memcpy(base, (const void *)cc6_trampoline_data, CC6_TRAMPOLINE_SIZE);
	wbinvd();
	for (i = 0; i < MAX_CORES; i++) {
		spin_lock_init(&cc6lock[i]);
		cc6cpucnt[i].counter = 0;
	}
	/* Allocate context save structure and store pointer to struct */
	if (!cntxt_created)
		return (create_contextdata());
	return 0;
}

#define	CC6STATUS_READ(lo,hi) rdmsr(MIC_CC6_STATUS,lo,hi);
#define CC6STATUS_WRITE(lo,hi) wrmsr(MIC_CC6_STATUS,lo,hi);

static inline int mic_mapunmap_gboxes(int op)
{
	int i;

	for (i = 0; i < MAX_GBOXES; i++) {
	/* Map gbox mmio space  */
		if(op == 0) {
			mic_gbox_mmio_va = ioremap(gboxBaseAddress[i], MIC_GBOX_MMIO_SIZE);
			if (!mic_gbox_mmio_va) {
				printk(
				KERN_ERR "Failed to map GBOX MMIO space"
				);
				return -EINVAL;
			}
			mic_gbox_mapped_address[i]= mic_gbox_mmio_va;
		}
		else {
			mic_gbox_mmio_va = mic_gbox_mapped_address[i];
			if(!mic_gbox_mmio_va)
				iounmap(mic_gbox_mmio_va);
		}
	}
	return 0;
}

static inline void _mic_print_gbox(int box)
{
	gboxFboxPmConfigurationReg pmconfig;

	MIC_READ_GBOX_MMIO_REG(pmconfig, GBOX_FBOX_PM_CONTROL);
	printk("GBOX_CONTROL(%1d) : %x\n", box, pmconfig.value);
	MIC_READ_GBOX_MMIO_REG(pmconfig, GBOX_FBOX_PM_STATE);
	printk("GBOX_STATE(%1d) : %x\n", box, pmconfig.value);
	MIC_READ_GBOX_MMIO_REG(pmconfig, GBOX_FBOX_PM_CONFIGURATION);
	printk("GBOX_CONFIG(%1d) : %x\n", box, pmconfig.value);
	MIC_READ_GBOX_MMIO_REG(pmconfig, GBOX_FBOX_PM_COUNTERS);
	printk("GBOX_COUNTERS(%1d) : %x\n", box, pmconfig.value);

}

static inline int mic_gbox_forcewakeup(int box)
{
	gboxFboxPmControlReg pmcontrol;
	gboxFboxPmStateReg pmstate;
	int delayloops=GBOX_WAKEUP_LATENCY * 1000/GBOX_LOOP_DELAY; 
	int i=0;

	mic_gbox_mmio_va = mic_gbox_mapped_address[box];
	if(!mic_gbox_mmio_va)
		return -EINVAL;
/* Set forcewakeup = 1  */
	MIC_READ_GBOX_MMIO_REG(pmcontrol, GBOX_FBOX_PM_CONTROL);
	/* Do this GBOX_REPEAT_COUNT times to work around Si sighting 3498934 */
	while((i++<GBOX_REPEAT_COUNT) && (!pmcontrol.bits.forcewakeup)){
		pmcontrol.bits.forcewakeup=1;
		MIC_WRITE_GBOX_MMIO_REG(pmcontrol, GBOX_FBOX_PM_CONTROL);
		MIC_READ_GBOX_MMIO_REG(pmcontrol, GBOX_FBOX_PM_CONTROL);
	}
	while(delayloops) {
		MIC_READ_GBOX_MMIO_REG(pmstate, GBOX_FBOX_PM_STATE);
		if ((pmstate.bits.statech0 == 0)&&  (pmstate.bits.statech1 == 0))
			return 0; 
 		udelay(GBOX_LOOP_DELAY);
		delayloops--;
	}
	dprintk("Failed to wake up gbox %d \n", box); 	
	return -EINVAL;
}
static inline int mic_forcewakeup_gboxes(void)
{	
	int i;

	for (i = 0; i < MAX_GBOXES; i++) {
		if(mic_gbox_forcewakeup(i))
			return -EINVAL;
	}
	return 0;
}	
static inline int mic_gbox_pm_toggle(int box,int state)
{

	gboxFboxPmConfigurationReg pmconfig;
	gboxFboxPmControlReg pmcontrol;
	gboxMboxPmConfigValidReg pmvalid;
	int i=0;

	mic_gbox_mmio_va = mic_gbox_mapped_address[box];
	if(!mic_gbox_mmio_va)
		return -EINVAL;

	/* Set PM enable bits   */
	/* Do this GBOX_REPEAT_COUNT times to work around Si sighting 3498934 */
	MIC_READ_GBOX_MMIO_REG(pmconfig, GBOX_FBOX_PM_CONFIGURATION);
	while((i++<GBOX_REPEAT_COUNT) && (pmconfig.bits.m2_enable != state) 
		&& (pmconfig.bits.m3_enable != state)){
		pmconfig.bits.m2_enable = state;
		pmconfig.bits.m3_enable = state;
		MIC_WRITE_GBOX_MMIO_REG(pmconfig, GBOX_FBOX_PM_CONFIGURATION);
		MIC_READ_GBOX_MMIO_REG(pmconfig, GBOX_FBOX_PM_CONFIGURATION);
	}
	pmvalid.value = 0;
	i=0;
	while(i++<GBOX_REPEAT_COUNT) {
		MIC_WRITE_GBOX_MMIO_REG(pmvalid, GBOX_MBOX_PM_CONFIG_VALID);
		MIC_WRITE_GBOX_MMIO_REG(pmvalid, 
					GBOX_MBOX_PM_CONFIG_VALID+0x800
					);
	}
	i=0;
	MIC_READ_GBOX_MMIO_REG(pmcontrol, GBOX_FBOX_PM_CONTROL);
	while((i++<GBOX_REPEAT_COUNT) && (pmcontrol.bits.forcewakeup)){
		pmcontrol.bits.forcewakeup=0;
		MIC_WRITE_GBOX_MMIO_REG(pmcontrol, GBOX_FBOX_PM_CONTROL);
		MIC_READ_GBOX_MMIO_REG(pmcontrol, GBOX_FBOX_PM_CONTROL);
	}
	return 0;
}

static inline void mic_packagec3_control(void)
{
	sboxC3TimersReg c3reg;

	MIC_READ_MMIO_REG(c3reg, SBOX_C3_TIMERS);
	c3reg.bits.c3_entry_timer = PACKAGE_C3_ENTRY_DELAY;
	c3reg.bits.c3_exit_timer = PACKAGE_C3_EXIT_DELAY;
	MIC_WRITE_MMIO_REG(c3reg, SBOX_C3_TIMERS);
}

static int sbox_mmio_setup(void)
{
	if (!mic_sbox_mmio_va) {
		dprintk("Mapping MIC sbox mmio in mic_cpuidle");
		mic_sbox_mmio_va = ioremap(SBOX_BASE, MIC_SBOX_MMIO_SIZE);
		if (!mic_sbox_mmio_va) {
			printk(KERN_ERR
			       "Failed to map SBOX MMIO space into kernel");
			return -EINVAL;
		}
	}
	return 0;
}

static inline int __mic_setup_gboxes(int state)
{
	int i;

	if(state != gbox_m2m3_on) {
		if(mic_forcewakeup_gboxes())
			return -EINVAL;	
		for (i = 0; i < MAX_GBOXES; i++) {
			if (mic_gbox_pm_toggle(i,state)) {
				printk(KERN_ERR "Failed to setup gboxes ");
				return -EINVAL;
			}
//		_mic_print_gbox(i);
		}
		gbox_m2m3_on=state;
	}	
	return 0;
}
/**
* __mic_read_uosflag
* Read the uosPMState H/W flag.
*Returns with contents of uosPMstate register.
**/
static inline u8 __mic_read_uosflag(void)
{
	sboxUospmstateReg sreg;
	MIC_READ_MMIO_REG(sreg, SBOX_UOSPMSTATE);
	return (sreg.bits.uos_pm_status);
}

/**
* __mic_write_uosflag
* Write the uosPMState H/W flag.
*
**/
static inline void __mic_write_uosflag(int state)
{
	sboxUospmstateReg sreg;

	uospmstate = state;
	MIC_READ_MMIO_REG(sreg, SBOX_UOSPMSTATE);
	sreg.bits.uos_pm_status = state;
	MIC_WRITE_MMIO_REG(sreg, SBOX_UOSPMSTATE);

}


/**
* mic_setup_pc3 - does required initial H/W setup to enable package C3
* gets called twice, once during cpuidle init and then every time 
* we come out of package C6 to reinitialize the GBoxes. 
* @atinit - true if called during init false otherwise.
* Returns 1 on success 0 otherwise.
**/
static inline int mic_setup_pc3(void)
{
	if (sbox_mmio_setup())
		return -EINVAL;
	/*  Enable GBox for power management    */
	if(mic_mapunmap_gboxes(0))
		return -EINVAL;
	__mic_setup_gboxes(0);
	
	/* Setup PC3 related timers and such ... */
	if(pc6_through_pc3())
		mic_packagec3_control();
	gbox_m2m3_on=0;
	__mic_write_uosflag(PM_IDLE_STATE_PC0);
	return 0;
}
/**
* __mic_read_hostflag
* Read the hostPMState H/W flag.
*Returns with contents of hostPMstate register.
**/
static inline u8 __mic_read_hostflag(void)
{
	sboxHostpmstateReg sreg;

	MIC_READ_MMIO_REG(sreg, SBOX_HOSTPMSTATE);
	return (sreg.bits.host_pm_status);
}
static inline void stop_lapictimer(void);
static void mic_pkgstate_wakeup(void);
static irqreturn_t mic_pc35wakeup_isr(int irq, void *unused)
{
	dprintk("Wake up interrupt from host\n");
	mic_pkgstate_wakeup();
	return IRQ_HANDLED;
}

static inline int __init_pc35wakeup_intr(void)
{
	int ret;
	
	if ((ret = request_irq(get_sbox_irq(PC35_WAKEUP_INT_IDX),
		mic_pc35wakeup_isr, IRQF_DISABLED, 
		"PC35_Wakeup", NULL))) {
		printk("Failed to setup PC35 wakeup intr, ret value = %d\n",ret);
		return 0;
	}
	dprintk("PC35 wakeup irq = %d\n",get_sbox_irq(PC35_WAKEUP_INT_IDX));
	return 1;
}
#if 0
static unsigned int saved_mca_reg;
 
static void mca_int_toggle(int state)
{
	sboxHostpmstateReg sreg;
	
	if(state==0) {
		saved_mca_reg=MIC_READ_MMIO_REG(sreg,SBOX_MCA_INT_EN);
		sreg.value=0;	
	}
	else {
		sreg.value=saved_mca_reg;
	}
	MIC_WRITE_MMIO_REG(sreg,SBOX_MCA_INT_EN);
}
#endif 	

static int mic_sendscifmessage(PM_MESSAGE msg, void *payload, size_t size)
{

	return (pm_scif_send2host(msg, payload, size));
}
static inline void abort_pc3attempt(void)
{
	int i;

	for_each_online_cpu(i) {
		per_cpu(policychanged,i) = 1;
	}
	smp_wmb();
	/* Wake up all threads */
	apic->send_IPI_allbutself(RESCHEDULE_VECTOR);
}
static void mic_pkgstate_wakeup(void)
{
	unsigned long flags;

	/*We must have come here either due to a PC3 entry abort by host or
 	* due to an exit from PC3
 	**/

	spin_lock_irqsave(&idlelock, flags);
	if(!pc3aborted) {
		if(uospmstate == PM_IDLE_STATE_PC3){
			__mic_setup_gboxes(0);
			barrier();
			/* Bump up the core freq/vid */
			micevent_notify(EVENT_PC3_EXIT,NULL,ATOMIC);
			barrier();
			dprintk("Resuming timekeeping\n");
			timekeeping_resume();
			/* Woke up from a PC3/6 state.  */
			pc3_entry_count++;
			pc3_ticks +=
	    			ktime_to_us(ktime_sub(ktime_get_real(),
				pc3_start));
			dprintk("PC3 average residency = %ld\n",
				pc3_ticks/pc3_entry_count);
		}
		/* Enable TSC watchdog even if we did not go into
 		 * PC3 to make sure that we skip watchdog calc once
 		 * to handle possible clock drift.
 		 *
 		 */
		watchdog_tsc_enable();
		smp_wmb();
		pc3aborted=1;
		__mic_write_uosflag(PM_IDLE_STATE_PC0);
		abort_pc3attempt();
	}
	spin_unlock_irqrestore(&idlelock, flags);
}

static int mic_cpuidle_scif_notifier(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	unsigned long flags;
	
	switch (event) {
		case PM_MESSAGE_OPEN:
			dprintk("PM OPEN command recvd from host\n");
			__init_pc35wakeup_intr();
			pkgstate_ready=1;
			break;
		case PM_MESSAGE_CLOSE:
			dprintk("PM CLOSE command recvd from host\n");
			pkgstate_ready=0;
			smp_wmb();
			spin_lock_irqsave(&idlelock, flags);
			__mic_write_uosflag(PM_IDLE_STATE_PC0);
			smp_wmb();
			abort_pc3attempt();
			spin_unlock_irqrestore(&idlelock, flags);
			break;	

	default:
		printk(KERN_INFO "Unexpected message %lx from host\n", event);
		break;
	}
	return 0;
}

static struct notifier_block mic_cpuidle_scif_notifier_block = {
	.notifier_call = mic_cpuidle_scif_notifier,
};

/*
 * Callers should disable interrupts before the call and enable
 * interrupts after return.
 */
static void mic_safe_halt(void)
{
	current_thread_info()->status &= ~TS_POLLING;
	/*
	 * TS_POLLING-cleared state must be visible before we
	 * test NEED_RESCHED:
	 */
	smp_mb();
	if (!need_resched()) {
		safe_halt();
		local_irq_disable();
	}
	current_thread_info()->status |= TS_POLLING;
}

/**
 * __mic_idle_enter_c1 - enters the MIC C1 state
 * @dev: the target CPU
 * @state: the state data
 *
 **/
static int __mic_idle_enter_c1(struct cpuidle_device *dev,
			       struct cpuidle_state *state)
{
	ktime_t kt1, kt2;
	unsigned long idle_time = 1000;

	kt1 = ktime_get_real();
	mic_safe_halt();
	kt2 = ktime_get_real();
	idle_time = ktime_to_us(ktime_sub(kt2, kt1));
	return idle_time;
}

static unsigned int idle_count[MAX_CPUS];

static int mic_idle_enter_c1(struct cpuidle_device *dev,
			     struct cpuidle_state *state)
{
	unsigned long idle_time;
	int cpu = dev->cpu;

	idle_count[cpu]++;
	idle_time = __mic_idle_enter_c1(dev, state);
	local_irq_enable();
	return idle_time;
}

/*
 * mic_switch_cc6_cpu - Enable / Disable core C6 for cpu .
 * @core - core going in/out of C6
 * @state - 1 = going idle, 0 = waking up 
 */
static int mic_switch_cc6_cpu(int core, int state)
{
	u32 lo, hi;

	spin_lock(&cc6lock[core]);
	CC6STATUS_READ(lo, hi);
	lo &= ~0x1f;
	lo |= cc6_cpumask[core];
	/* Turn on the core enable */
	lo |= state;
	CC6STATUS_WRITE(lo, hi);
	spin_unlock(&cc6lock[core]);
	return (lo);
}

/**
* mic_prepare_pkgstateentry - Prepare the cpu for DPC3/PC6 entry 
* Sends scif msg to host and waits for disconnect to happen.
* Programs the uosflag.
**/
static int mic_prepare_pkgstate_entry(int cpu)
{
	unsigned long flags;
	PM_MESSAGE msg;
	pm_msg_pc3Ready payload;
#ifndef	CONFIG_X86_MIC_EMULATION
	int ret,pc3cpus;
#endif	
	int hoststate =__mic_read_hostflag();
	int i,cpus_stopped;
	
	msg = PM_MESSAGE_PC3READY;
	pc3cpus = atomic_inc_return(&pc3cpucnt);
	if(uospmstate == PM_IDLE_STATE_PC0){
		if(hoststate == PM_IDLE_STATE_PC0) {
			if(pc3cpus == nr_cpu_ids) {
				spin_lock_irqsave(&idlelock, flags);
			/* Make sure someone else did'nt beat me to it */
				if(uospmstate == PM_IDLE_STATE_PC0){
					if(mic_sendscifmessage(msg, 
					(void *)&payload,sizeof(payload)) >=0){
						dprintk
						("Sent PC3 ready message\n"
						);
						 __mic_write_uosflag
						(PM_IDLE_STATE_PC3_READY
						);
						pc3aborted=0;
						smp_wmb();
					}	
					else	{
						printk("Failed to send PC3 message\n");
						for_each_online_cpu(i) {
							per_cpu(policychanged,i) = 1;
						}
						smp_wmb();
						spin_unlock_irqrestore(&idlelock, flags);
						return -1;
					}
				}
				spin_unlock_irqrestore(&idlelock, flags);
			}
		}
		else	{
			/* For some reason host has not reset state. So 
 			*  forget about PC3 for now and act normal
 			*/
				printk("Host state not PC0\n");
				return -1;
		}
	}	
	/*uospmstate == PC3_READY */
	if(hoststate == PM_IDLE_STATE_PC3){
		per_cpu(timer_stopped,cpu)=1;
		cpus_stopped = atomic_inc_return(&stopcnt);
		spin_lock_irqsave(&idlelock, flags);
		if((atomic_read(&pc3cpucnt) == nr_cpu_ids) && !pc3aborted &&
			cpus_stopped == nr_cpu_ids) {	
			/* Start putting card into PC3 */
			stop_lapictimer();
			if(pc6_through_pc3()){
				save_APICICR_setup(apicicr);
				ret = save_IO_APIC_setup(ioapic_entries);
			}
			dprintk("cpu %d set to enter package state\n",cpu);
			pc3_start = pc6_start = ktime_get_real();
			pc3_select_count++;
			barrier();
			watchdog_tsc_disable();
			dprintk("Suspending timekeeping\n");
			timekeeping_suspend();
			barrier();
			/* Reduce freq and vid to a minimum. */
			micevent_notify(EVENT_PC3_ENTRY,NULL,ATOMIC);
			barrier();
			/* Enable gboxes for M2/M3	*/
			__mic_setup_gboxes(1);
			barrier();
			__mic_write_uosflag(PM_IDLE_STATE_PC3);
			spin_unlock_irqrestore(&idlelock, flags);
			return 0;
		}
		spin_unlock_irqrestore(&idlelock, flags);
		stop_lapictimer();	
	}
	return -1;
}

static int __mic_idle_enter_c6(struct cpuidle_device *dev,
			       struct cpuidle_state *state, int cflush)
{
	int retval, apicid;
	unsigned long entrytime = 0, exittime = 0;
	int core, tmp;
	int thr = dev->cpu % 4;

	apicid = per_cpu(x86_cpu_to_apicid, dev->cpu);
	core = apicid / 4;
	/* Save cpu state       */
	rdtscll(entrytime);
	mic_save_processor_state(apicid);
	atomic_set_mask(1 << (thr + 1), &cc6_cpumask[core]);
	tmp = atomic_inc_return(&cc6cpucnt[core]);
	if (tmp == 4) {
		retval = mic_switch_cc6_cpu(core, 1);
		rdtscll(cc6entrytime[core]);
	}
	rdtscll(exittime);
	cc6latency[dev->cpu] += exittime - entrytime;
	do {
		retval = do_cc6entry_lowlevel(cc6_cntxt[apicid], cflush);
		if (retval == MIC_STATE_C6) {
			rdtscll(exittime);
			if(exittime > cc6_waketime[apicid])
				cc6latency[dev->cpu] += exittime - cc6_waketime[apicid];
			entrytime = cc6entrytime[core];
			if (entrytime > cc6_waketime[apicid]) {
				dprintk(KERN_INFO "RDTSC mismatch on cpu%d\n",
				       dev->cpu);
			} else {
				cc6residency[dev->cpu] += cc6_waketime[apicid] - entrytime;
				cc6entries[dev->cpu] += 1;
			}
			}
		cflush = 0;
	} while (retval == MIC_STATE_C6);
	atomic_clear_mask(1 << (thr + 1), &cc6_cpumask[core]);
	tmp=atomic_dec_return(&cc6cpucnt[core]);
	if(tmp == 3) 
		mic_switch_cc6_cpu(core, 0);
	return retval;
}

static unsigned long __read_mostly coremask = ~0UL;

static inline int corec6_allowed(int cpu) 
{
	int core = per_cpu(x86_cpu_to_apicid, cpu) / 4;
	int cpu0core = per_cpu(x86_cpu_to_apicid, 0) / 4;

	return (mic_cc6_on && (coremask & (1UL << core)) && 
		(core != cpu0core) && cc6bypolicy); 
}		


static inline void stop_lapictimer(void)
{
	
	apic_write(APIC_TMICT,0);
}
static inline void start_lapictimer(void)
{
	tick_resume_oneshot();
}
 
/**
*mic_idle_enter_cc6 - Core C6 idle state handler for cpu.  
**/
static int mic_idle_enter_cc6(struct cpuidle_device *dev,
			      struct cpuidle_state *state)
{
	unsigned long idle_time=0;
	ktime_t kt1;
	int cpu = dev->cpu;

	if (!corec6_allowed(cpu)) {
		return (mic_idle_enter_c1(dev, state));
	}
	current_thread_info()->status &= ~TS_POLLING;
	/*
	 * TS_POLLING-cleared state must be visible before we
	 * test NEED_RESCHED:
	 */
	smp_mb();
	if (!need_resched()) {
		idle_count[cpu]++;
		kt1 = ktime_get_real();
		__mic_idle_enter_c6(dev, state, 0);
		idle_time = ktime_to_us(ktime_sub(ktime_get_real(), kt1));
	}
	current_thread_info()->status |= TS_POLLING;
	local_irq_enable();
	return idle_time;
}

/**
* mic_idle_enter_pc3 - Package C3 idle state handler for cpu.  
**/
static int mic_idle_enter_pc3(struct cpuidle_device *dev,
			      struct cpuidle_state *state)
{

	int idle_time=0, pc3cpus;
	int cpu = dev->cpu;
	int apicid = per_cpu(x86_cpu_to_apicid, cpu);
	int retval,i;
	unsigned long flags;
	ktime_t kt1;
	int flush = 0;
	
	if(!pkgstate_ready)
		return(mic_idle_enter_c1(dev,state));
	
	current_thread_info()->status &= ~TS_POLLING;
	smp_mb();
	if (need_resched()) goto bail; 
	idle_count[cpu]++;
	kt1 = ktime_get_real();
	sched_clock_idle_sleep_event();
	smp_mb();
	retval=mic_prepare_pkgstate_entry(cpu);
	if (!retval) {
		spin_lock_irqsave(&idlelock, flags);
		if(atomic_read(&pc3cpucnt) == nr_cpu_ids) {
		/*Call DEVEVENT_SUSPEND on all steppings. Even on B0/B1 
 		* steppings that have only DPC3,SCIF needs to know that
 		* it is suspending (though there is no state save/restore
 		* needed) so P2P can work well (HSD4845665).
 		*/
			dprintk("mic_idle_enter_pc3: Calling suspend\n");
			micpm_device_notify(MICPM_DEVEVENT_SUSPEND, NULL);
			devicesuspended = 1;
			pc6exitdone=0;
		}
		spin_unlock_irqrestore(&idlelock, flags);
	}
	/* Save lapic state     */
	if(pc6_through_pc3()){
		lapic_save(&lapic_store[cpu]);
		flush=1;
	}
	/* Don't allow cpu 0 and its siblings to go to CC6 - 
 	 * it needs to handle MC events. */
	if(corec6_allowed(cpu)) {
		retval = __mic_idle_enter_c6(dev, state, flush);
	} else {
		/* Save cpu state       */
		if(pc6_through_pc3()) {
			mic_save_processor_state(apicid);
			retval = do_cc6entry_lowlevel(cc6_cntxt[apicid], 1);	//always flush cache
		}
		else {
			__mic_idle_enter_c1(dev,state);
			retval = MIC_STATE_C1;
		}	
	}
	smp_mb();
	pc3cpus = atomic_dec_return(&pc3cpucnt);
	if (retval == MIC_STATE_PC6) {
		lapic_restore(&lapic_store[cpu]);
		if (pc3cpus == 0) {
#ifndef	CONFIG_X86_MIC_EMULATION
			restore_APICICR_setup(apicicr);
			restore_IO_APIC_setup(ioapic_entries);
			/* HSD4868790. Perform INIT level de-assert during
			 * PC6 exit to fix APIC checksum errors that happen
			 * on running certain workloads.
			 * BIOS already performs this de-assert to sync the
			 * arbitration IDs on every PC6 exit but the IDs get out
			 * of sync on running certain workloads
			 */
			apic_write(APIC_ICR, 0x0c8500);

#endif
			/* Reinitialize the GBoxes */
			__mic_setup_gboxes(0); //Turn OFF M2/M3
			barrier();
			micevent_notify(EVENT_PC3_EXIT,NULL,ATOMIC);
			barrier();
			timekeeping_resume();
			watchdog_tsc_enable();
			barrier();
			time_in_pc6 = ktime_to_us(ktime_sub(ktime_get_real(), pc6_start));
			/* Call resume notifier */
			micpm_device_notify(MICPM_DEVEVENT_RESUME, NULL);
			__mic_write_uosflag(PM_IDLE_STATE_PC0);
			barrier();
			/* Wait for the host to go to PC0       */
			while (__mic_read_hostflag() != PM_IDLE_STATE_PC0)
				cpu_relax();
			devicesuspended = 0;
			dprintk("Woke up from PC6 after %lu us\n", time_in_pc6);
			pc6_ticks += time_in_pc6;
			pc6_entry_count++;
			/* Reset policychanged so we restart the PC6 policy
			   calculation in the governor.
			*/    
			for_each_online_cpu(i) {
				per_cpu(policychanged,i) = 1;
			}
			smp_wmb();
			pc6exitdone = 1;

		}
		do {
			cpu_relax();
		} while (!pc6exitdone);
		/* Increment core c6 entry count */
		if (corec6_allowed(cpu)){
			cc6entries[cpu] += 1;
		}		

	} else {
		if (pc3cpus == nr_cpu_ids - 1) {
			spin_lock_irqsave(&idlelock, flags);
			if(devicesuspended) {
				dprintk("mic_idle_enter_pc3: Calling fail suspend\n");
				micpm_device_notify(MICPM_DEVEVENT_FAIL_SUSPEND, NULL);
				devicesuspended = 0;
			}
			if(uospmstate == PM_IDLE_STATE_PC3){
				  dprintk("cpu %d woke up while in PC3.\n",cpu);
				__mic_setup_gboxes(0);
				__mic_write_uosflag(PM_IDLE_STATE_PC3_READY);
				micevent_notify(EVENT_PC3_EXIT,NULL,ATOMIC);
				dprintk("Resume timekeeping. Wake up due to external interrupt\n");
				timekeeping_resume();
				watchdog_tsc_enable();
			}	
			spin_unlock_irqrestore(&idlelock, flags);
		}
	}
	sched_clock_idle_wakeup_event(0);
	if(per_cpu(timer_stopped,cpu)) {
		atomic_dec_return(&stopcnt);
		per_cpu(timer_stopped,cpu)=0;
		start_lapictimer();
	}
//	if(uospmstate == PM_IDLE_STATE_PC0)
//		start_lapictimer();
	idle_time = ktime_to_us(ktime_sub(ktime_get_real(), kt1));
bail:
	current_thread_info()->status |= TS_POLLING;
	local_irq_enable();
	return (idle_time);
}

/**
 * mic_setup_cpuidle - prepares and configures CPUIDLE for device
 * Since core state selection C6 and package state selections PC3
 * and PC6 are done by the governor with per-cpu idlness data 
 * all the three could potentially result in a package wide idle state.
**/
static int mic_setup_cpuidle(struct cpuidle_device *dev, int cpu)
{
	int i, count = CPUIDLE_DRIVER_STATE_START, k;
	struct cpuidle_state *state;
	int max_cstate = MAX_KNC_CSTATES;
	struct mic_cpuidle_states *cx;

	dev->cpu = cpu;
	for (i = 0; i < CPUIDLE_STATE_MAX; i++) {
		dev->states[i].name[0] = '\0';
		dev->states[i].desc[0] = '\0';
	}
	for (i = CPUIDLE_DRIVER_STATE_START, k = 1; k <= max_cstate; k++) {
		state = &dev->states[i];
		state->flags = 0;
		switch (k) {
		case MIC_STATE_C1:
			state->flags =
			    CPUIDLE_FLAG_SHALLOW | CPUIDLE_FLAG_TIME_VALID;
			state->enter = mic_idle_enter_c1;
			dev->safe_state = state;
			break;
		case MIC_STATE_C6:
			if (!mic_cc6_on)
				continue;
			state->flags =
			    CPUIDLE_FLAG_DEEP | CPUIDLE_FLAG_TIME_VALID;
			state->enter = mic_idle_enter_cc6;
			break;

		case MIC_STATE_PC3:
			if (!mic_pc3_on)
				continue;
			state->flags =
			    CPUIDLE_FLAG_SHALLOW | CPUIDLE_FLAG_TIME_VALID |
			    CPUIDLE_FLAG_PKGSTATE;
			state->enter = mic_idle_enter_pc3;
			break;
		}
		cx = &mic_cpuidle_data[k - 1];
		strncpy(state->name, cx->desc, 3);
		state->name[3] = '\0';
		strncpy(state->desc, cx->desc, CPUIDLE_DESC_LEN);
		state->exit_latency = cx->latency;
		state->target_residency = cx->latency * latency_factor;
		state->power_usage = cx->power;
		i++;
		count++;
		if (count == CPUIDLE_STATE_MAX)
			break;
	}
	dev->state_count = count;
	if (!count)
		return -EINVAL;
	return 0;
}

/**
 * mic_cpuidle_cleanup - Cleanup driver before unloading.
 * @mask: the mask of all cpuidle devices that have been registered.
 */
static void mic_cpuidle_cleanup(cpumask_var_t mask)
{
	unsigned int cpu;
	struct cpuidle_device *dev;

	for_each_cpu(cpu, mask) {
		dev = per_cpu(pcidle, cpu);
		if (dev) {
			cpuidle_unregister_device(dev);
			kfree(dev);
		}
	}
}

int __init mic_cpuidle_init(int cc6, int pc3, int pc6)
{
	unsigned int cpu;
	struct cpuidle_device *cidle;
	int result;

	mic_cc6_on = cc6;
	mic_pc3_on = pc3;
	mic_pc6_on = pc6;

	dprintk("Registering cpuidle driver\n");
	result = cpuidle_register_driver(&mic_cpuidle_driver);
	if (result < 0) {
		printk(KERN_DEBUG PREFIX "Failed to register cpuidle driver\n");
		return result;
	}
	if (!zalloc_cpumask_var(&regdevices, GFP_KERNEL)) {
		return -ENOMEM;
	}
	apicid_ptr = (volatile u32 *)(fix_to_virt(FIX_APIC_BASE) + APIC_ID);
	if (mic_pc3_on) {
		result = mic_setup_pc3();
		if (result < 0)
			goto fail;
	}
	if (mic_pc6_on) {
		if(boot_cpu_data.x86_mask == KNC_C_STEP) 
			if (mic_setup_pc6()) {
			printk(KERN_INFO "Failed PC6 setup\n");
			mic_pc6_on=0;
			goto fail;
		}
	}
	/* Setup driver for core and package idle states.       */

	if ((mic_cc6_on) && (boot_cpu_data.x86_mask == KNC_C_STEP)) {
		if (mic_setup_c6()) {
			printk(KERN_INFO "Failed CC6 setup\n");
			goto fail;
		}
		cc6bypolicy=1;		//Turn ON CC6 by default
	}
	else {
		/* No CC6 support for other steppings */
		mic_cc6_on=0;
	}
	if (!zalloc_cpumask_var(&idlemask, GFP_KERNEL)) {
		goto fail;
	}
	cpumask_clear(idlemask);
	spin_lock_init(&idlelock);
	/* Allocate cpuidle_device for each cpu and and init it */
	for_each_online_cpu(cpu) {
		cidle = kzalloc(sizeof(struct cpuidle_device), GFP_KERNEL);
		if (cidle == NULL)
			goto fail;
		cpumask_set_cpu(cpu, regdevices);
		per_cpu(pcidle, cpu) = cidle;
		if (mic_setup_cpuidle(cidle, cpu))
			goto fail;
		if (cpuidle_register_device(cidle)) {
			printk(KERN_DEBUG PREFIX
			       "Failed to register cpuidle device");
			goto fail;
		}

	}
	/*  Register with TMU interrupt notifier. */
#ifdef PARTIAL_CORE_TURBO
	micpm_notifier_register(&mic_cpuidle_blocking_notifier_block);
	micpm_atomic_notifier_register(&mic_cpuidle_atomic_notifier_block);
#endif	
	/*  Register for scif notifications. */
	pm_scif_notifier_register(&mic_cpuidle_scif_notifier_block);
	mic_cpuidle_inited = 1;
	return 0;

fail:
	printk(KERN_INFO "Failed to init cpuidle devices\n");
#ifdef PARTIAL_CORE_TURBO
	micpm_notifier_unregister(&mic_cpuidle_blocking_notifier_block);
	micpm_atomic_notifier_unregister(&mic_cpuidle_atomic_notifier_block);
#endif
	pm_scif_notifier_unregister(&mic_cpuidle_scif_notifier_block);
	mic_cpuidle_cleanup(regdevices);
	free_contextdata();
	free_cpumask_var(regdevices);
	free_cpumask_var(idlemask);
	mic_mapunmap_gboxes(1);
	return -EIO;

}

void __exit mic_cpuidle_exit(void)
{
	mic_cpuidle_cleanup(regdevices);
	free_cpumask_var(regdevices);
	free_contextdata();
	mic_mapunmap_gboxes(1);
	cpuidle_unregister_driver(&mic_cpuidle_driver);
}
