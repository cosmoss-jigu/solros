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
 * mic_def.h: Header file for Intel MIC devices
 *
 * (C) Copyright 2009 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 */
#include <linux/irqreturn.h>
#ifndef	ASM_X86_MIC_DEF_H
#define	ASM_X86_MIC_DEF_
#include "mic/mic_common.h"
#include "mic/micreghelper.h"
#ifdef CONFIG_MK1OM
#include <linux/clocksource.h>
/*
 * SBOX ETC is a 64-bit counter with the LSB 5 bits hardwired to zero.
 * The input clock is 15.625MHz and the counter increments by 100000b
 * every 64nsecs.
 */
#define ETC_TICK_RATE           15625000
extern	int etc_comp;
cycle_t mic_etc_read(struct clocksource *cs);
extern struct clocksource clocksource_micetc;
#endif

#define MIC_NUM_IOAPIC_ENTRIES	25
void arch_setup_sbox_irqs(unsigned int *irqs, int n);
void save_APICICR_setup(uint64_t *apicicr);
void restore_APICICR_setup(uint64_t *apicicr);
extern void __iomem *mic_sbox_mmio_va;
#define SBOX_BASE           0x08007D0000ULL     /* PCIE Box Registers */
#define MIC_SBOX_MMIO_SIZE	(64*1024) // 64 KB = 2^16 // already defined ?

#include "mic/mic_knc/micsboxdefine.h"
#include "mic/mic_knc/micsboxstruct.h"
#include "mic/mic_knc/micgboxdefine.h"
#include "mic/mic_knc/micgboxstruct.h"
#include "mic/mic_knc/autobaseaddress.h"

#endif 


