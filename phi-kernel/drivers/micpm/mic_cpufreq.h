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
 * mic_cpufreq.h : Header file for the Intel MIC cpufreq driver
 *
 * (C) Copyright 2008 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _MIC_CPUFREQ_H
#define _MIC_CPUFREQ_H
/*
#include "mic_common.h"
#include "micsboxdefine.h"
#include "micsboxstruct.h"
*/
#include <asm/mic_def.h>



#define SBOX_BASE           0x08007D0000ULL     /* PCIE Box Registers */
#define MIC_SBOX_MMIO_SIZE	(64*1024) // 64 KB = 2^16 // already defined ?


#define MAX_RATIO_ENTRIES 	4	/* Maximum number of entries in the mic_ratio_data array	*/ 


/*
  Additional data elements to go into the cpufreq_frequency_table 
  to support the MIC cpufreq driver
*/
struct mic_cpufreq_viddata {
	unsigned int	voltage;		/* voltage in mV */
	unsigned int	vid;		/* voltage code to be set in VR  */
};

/* Static data on freq ratios and dividers */
struct mic_ratio_data {
	unsigned int divider;
	unsigned int minratio;
	unsigned int maxratio;
};	

/* Static data on the core clock PLLs */ 
struct mic_pll_data {
	unsigned int	bclk	;	/* Base clock  in MHz*/
	unsigned int	ffpos	;	/* Position of the feedback divider bits in ratio code	*/
	struct mic_ratio_data ratio_data[MAX_RATIO_ENTRIES];
};

/* Frequency and vid code pair	*/
struct mic_freqvid {
	unsigned int	freq;	/* Core freq ratio */
	unsigned int	vid;	/* Voltage code	*/
};	


/* Include L1OM specific sbox struct defines.	*/
#define L1OM_SBOX_CURRENT_CLK_RATIO  	0x00003004
#define L1OM_SBOX_COREFREQ              0x00004040
#define L1OM_SBOX_COREVOLT              0x00004044


typedef union _sboxCurrentClkRatioReg
{
    uint32_t value;
    struct
    {
        uint32_t current_clk_ratio                              : 8; // bit 0-7 Samples current value of signal LPCCurrentRatioInMnnnH[7:0].
        uint32_t rsvd0                                          :24; // bit 8-31
    } bits;
} sboxCurrentClkRatioReg,l1om_sboxCurrentClkRatioReg;

typedef union _l1omsboxCorevoltReg
{
    uint32_t value;
    struct
    {
        uint32_t vid                                            : 8; // bit 0-7 VID
        uint32_t rsvd0                                          :24; // bit 8-31
    } bits;
} l1om_sboxCorevoltReg;

typedef union _l1omsboxCorefreqReg
{
    uint32_t value;
    struct
    {
        uint32_t ratio                                          : 8; // bit 0-7 Ratio
        uint32_t rsvd0                                          : 8; // bit 8-15
        uint32_t asyncmode                                      : 1; // bit 16 Async Mode. Field pulled out from extclkfreq by SW team
        uint32_t extclkfreq                                     : 4; // bit 17-20 ExtClkFreq
        uint32_t rsvd1                                          : 6; // bit 21-26
        uint32_t ratiostep                                      : 3; // bit 27-29 Power throttle ratio-step
        uint32_t jumpratio                                      : 1; // bit 30 Power throttle jump at once
        uint32_t switch_bit                                     : 1; // bit 31 Switch to the value from fuse or the register
    } bits;
} l1om_sboxCorefreqReg;

#endif  //_MIC_CPUFREQ_H
						
