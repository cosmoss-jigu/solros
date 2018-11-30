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
 * sfi.c - x86 architecture SFI support.
 *
 * Copyright (c) 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#define KMSG_COMPONENT "SFI"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/sfi.h>
#include <linux/io.h>

#include <asm/io_apic.h>
#include <asm/mpspec.h>
#include <asm/setup.h>
#include <asm/apic.h>

#ifdef CONFIG_X86_LOCAL_APIC
static unsigned long sfi_lapic_addr __initdata = APIC_DEFAULT_PHYS_BASE;

/* All CPUs enumerated by SFI must be present and enabled */
static void __cpuinit mp_sfi_register_lapic(u8 id)
{
	if (MAX_LOCAL_APIC - id <= 0) {
		pr_warning("Processor #%d invalid (max %d)\n",
			id, MAX_LOCAL_APIC);
		return;
	}

#ifndef CONFIG_X86_MIC_EMULATION  /* skip to save time */
	pr_info("registering lapic[%d]\n", id);
#endif

	generic_processor_info(id, GET_APIC_VERSION(apic_read(APIC_LVR)));
}

static int __init sfi_parse_cpus(struct sfi_table_header *table)
{
	struct sfi_table_simple *sb;
	struct sfi_cpu_table_entry *pentry;
	int i;
	int cpu_num;

	sb = (struct sfi_table_simple *)table;
	cpu_num = SFI_GET_NUM_ENTRIES(sb, struct sfi_cpu_table_entry);
	pentry = (struct sfi_cpu_table_entry *)sb->pentry;

	for (i = 0; i < cpu_num; i++) {
		mp_sfi_register_lapic(pentry->apic_id);
		pentry++;
	}

	smp_found_config = 1;
	return 0;
}
#endif /* CONFIG_X86_LOCAL_APIC */

#ifdef CONFIG_X86_IO_APIC

static int __init sfi_parse_ioapic(struct sfi_table_header *table)
{
	struct sfi_table_simple *sb;
	struct sfi_apic_table_entry *pentry;
	int i, num;

	sb = (struct sfi_table_simple *)table;
	num = SFI_GET_NUM_ENTRIES(sb, struct sfi_apic_table_entry);
	pentry = (struct sfi_apic_table_entry *)sb->pentry;

	for (i = 0; i < num; i++) {
		mp_register_ioapic(i, pentry->phys_addr, gsi_top);
		pentry++;
	}

	WARN(pic_mode, KERN_WARNING
		"SFI: pic_mod shouldn't be 1 when IOAPIC table is present\n");
	pic_mode = 0;
	return 0;
}
#endif /* CONFIG_X86_IO_APIC */

/*
 * sfi_platform_init(): register lapics & io-apics
 */
int __init sfi_platform_init(void)
{
#ifdef CONFIG_X86_LOCAL_APIC
	register_lapic_address(sfi_lapic_addr);
	sfi_table_parse(SFI_SIG_CPUS, NULL, NULL, sfi_parse_cpus);
#endif
#ifdef CONFIG_X86_IO_APIC
	sfi_table_parse(SFI_SIG_APIC, NULL, NULL, sfi_parse_ioapic);
#endif
	return 0;
}
