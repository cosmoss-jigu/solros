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
/******************************************************************************
File Name: micgboxstruct.h
Description:  "Raw" register offsets & bit specifications for Intel MIC (KNC)
Notes:
    Auto gen gbox register header
*******************************************************************************/
#ifndef _MIC_GBOXSTRUCT_REGISTERS_H_
#define _MIC_GBOXSTRUCT_REGISTERS_H_

#include "../micreghelper.h"


/************************************************************************* 
    PM status register for FBox 
*************************************************************************/
typedef union _gboxFboxPmStateReg
{
    U32 value;
    struct
    {
	U32 statech0					   : 4; // bit 0-3 Indicates the state of Channel 0
	U32 statech1					   : 4; // bit 4-7 Indicates the state of Channel 1
	U32 rsvd0					   :24; // bit 8-31
    } bits;
} gboxFboxPmStateReg;

STATIC_ASSERT(sizeof(gboxFboxPmStateReg) == sizeof(U32));

/************************************************************************* 
    PM register for FBox 
*************************************************************************/
typedef union _gboxFboxPmConfigurationReg
{
    U32 value;
    struct
    {
	U32 powermanagement_enable			   : 1; // bit 0 Indicates whether PM is enabled
	U32 m1_enable					   : 1; // bit 1 Indicates whether M1 is enabled
	U32 m2_enable					   : 1; // bit 2 Indicates whether M2 is enabled
	U32 m3_enable					   : 1; // bit 3 Indicates whether M3 is enabled
	U32 cke_enable					   : 1; // bit 4 Indicates whether CKE is enabled
	U32 pm_mode					   : 1; // bit 5 Indicates the behaviour of the PM logic
	U32 fboxdfxclkgateen				   : 1; // bit 6 Indicates whether DFX on demand clk gating is enabled
	U32 fboxclkgateen				   : 1; // bit 7 Indicates whether on demand clk gating is enabled
	U32 m2_timerenable				   : 1; // bit 8 Indicates whether M2 Timer is enabled. Needs to be qualified with M2_Enable
	U32 m3_timerenable				   : 1; // bit 9 Indicates whether M3 Timer is enabled. Needs to be qualified with M2_Enable, M3_Enable
	U32 fboxarbiterlock				   : 1; // bit 10 If set, will lock Fbox Arbiter after Gbox detect Idle in both channels. 
								// After that, Gbox will remain as idle and subsequent wakeup event from Ring will be ignored until this bit is cleared.
	U32 rsvd0					   :21; // bit 11-31
    } bits;
} gboxFboxPmConfigurationReg;

STATIC_ASSERT(sizeof(gboxFboxPmConfigurationReg) == sizeof(U32));
/************************************************************************* 
    PM control register for FBox 
*************************************************************************/
typedef union _gboxFboxPmControlReg
{
    U32 value;
    struct
    {
	U32 enablech0					   : 1; // bit 0 Indicates whether channel 0 is enabled
	U32 enablech1					   : 1; // bit 1 Indicates whether channel 1 is enabled
	U32 enablech0valid				   : 1; // bit 2 Indicates whether channel 0 enabled is valid
	U32 enablech1valid				   : 1; // bit 3 Indicates whether channel 1 enabled is valid
	U32 nextstatech0				   : 4; // bit 4-7 Indicates next state for Ch0
	U32 nextstatech1				   : 4; // bit 8-11 Indicates next state for Ch1
	U32 forcem2					   : 1; // bit 12 Force M2 state
	U32 forcem3					   : 1; // bit 13 Force M3 state
	U32 exitmemprsv					   : 1; // bit 14 Signal Config Done, will trigger Memory Preservation Interrupt
	U32 forcewakeup					   : 1; // bit 15 WakeUp all the channels
	U32 rsvd0					   :16; // bit 16-31
    } bits;
} gboxFboxPmControlReg;

STATIC_ASSERT(sizeof(gboxFboxPmControlReg) == sizeof(U32));

/************************************************************************* 
    PM control register for FBox with values for the counters 
*************************************************************************/
typedef union _gboxFboxPmCountersReg
{
    U32 value;
    struct
    {
	U32 idlesignaldelay				   : 8; // bit 0-7 Number of cycles of delay for the Idle signal
	U32 idlecondition				   : 2; // bit 8-9 PM Status linking to the Idle signal
	U32 earlyidlesignaldelay			   : 8; // bit 10-17 Number of cycles of delay for the Early Idle signal
	U32 rsvd0					   :14; // bit 18-31
    } bits;
} gboxFboxPmCountersReg;

STATIC_ASSERT(sizeof(gboxFboxPmCountersReg) == sizeof(U32));

/************************************************************************* 
    
	      Trigger to latch FBOX Power Management CR values used in MBOX.
	     
*************************************************************************/
typedef union _gboxMboxPmConfigValidReg
{
    U32 value;
    struct
    {
	U32 unused					   :32; // bit 0-31 unused
    } bits;
} gboxMboxPmConfigValidReg;

STATIC_ASSERT(sizeof(gboxMboxPmConfigValidReg) == sizeof(U32));
#endif
