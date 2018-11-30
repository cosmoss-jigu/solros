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
 * micpm_common.h - Common header file for PM driver modules 
 * for the Intel MIC device.
 *
 *   * (C) Copyright 2008 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _MICPM_COMMON_H_
#define _MICPM_COMMON_H_
#include <asm/mic_def.h>

extern void __iomem  	*mic_sbox_mmio_va;
   
#define	MIC_READ_MMIO_REG(var,offset)	\
              var.value = readl((const volatile void __iomem *)((unsigned long)(mic_sbox_mmio_va) +  (offset)));
 		
#define	MIC_WRITE_MMIO_REG(var,offset) \
	writel(var.value,(volatile void __iomem *)((unsigned long)(mic_sbox_mmio_va) +  (offset))); 	 

#endif  // _MICPM_COMMON_H_



