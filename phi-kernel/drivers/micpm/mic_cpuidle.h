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
#ifndef _MIC_CPUIDLE_H_
#define _MIC_CPUIDLE_H_

/*
 * mic_cpuidle.h - header file for cpuidle driver for the Intel MIC device
 *
 *   * (C) Copyright 2008 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

/*
* Context save struct for save/restore of cpu context during MIC cpuidle device 
* entry/exit Core C6 state.
*/
#define DEFAULT_PC3_POLICY	1	/* By default choose package C3 state by explicit 
					   command.
				 	*/ 
#define	PC3_LATENCY		3000

DECLARE_PER_CPU(int, policychanged);

struct cc6_saved_context {
	struct pt_regs regs;
	u16 ds, es, fs, gs, ss;
	unsigned long gs_base, gs_kernel_base, fs_base;
	unsigned long cr0, cr2, cr3, cr4, cr8;
	unsigned long efer;
	u16 gdt_pad;
	u16 gdt_limit;
	unsigned long gdt_base;
	u16 idt_pad;
	u16 idt_limit;
	unsigned long idt_base;
	u16 ldt;
	u16 tss;
	unsigned long tr;
	unsigned long safety;
	unsigned long return_address;
	unsigned long mcg_ctl, mc0_ctl, mc1_ctl, mc2_ctl;
	unsigned long pmu_glbl,pmu_ctr0,pmu_ctr1,pmu_evt0,pmu_evt1,pmu_filt;
} __attribute__((packed));

struct mic_apic_state {
	/*
	 * 'active' is true if the local APIC was enabled by us and
	 * not the BIOS; this signifies that we are also responsible
	 * for disabling it before entering apm/acpi suspend
	 */
	int active;
	/* r/w apic fields */
	unsigned int apic_id;
	unsigned int apic_taskpri;
	unsigned int apic_ldr;
	unsigned int apic_dfr;
	unsigned int apic_spiv;
	unsigned int apic_lvtt;
	unsigned int apic_lvtpc;
	unsigned int apic_lvt0;
	unsigned int apic_lvt1;
	unsigned int apic_lvterr;
	unsigned int apic_tmict;
	unsigned int apic_tdcr;
	unsigned int apic_thmr;
};

#endif 

