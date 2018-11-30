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
File Name: autobaseaddress.h
Description:  "Raw" register offsets & bit specifications for MIC
Notes:
*******************************************************************************/
#ifndef _MIC_AUTOBASEDEFINE_REGISTERS_H_
#define _MIC_AUTOBASEDEFINE_REGISTERS_H_


#define CBOX_BASE           0x0ULL              /* P54C Core */
//#define APIC_BASE           0xFEE00000ULL       /* Separate "box" for local apic registers which are at a completely different base_address */
#define TXS0_BASE           0x0800780000ULL     /* Texture Sampler */
#define TXS1_BASE           0x0800770000ULL     /* Texture Sampler */
#define TXS2_BASE           0x0800760000ULL     /* Texture Sampler */
#define TXS3_BASE           0x0800750000ULL     /* Texture Sampler */
#define TXS4_BASE           0x0800740000ULL     /* Texture Sampler */
#define TXS5_BASE           0x0800730000ULL     /* Texture Sampler */
#define TXS6_BASE           0x0800720000ULL     /* Texture Sampler */
#define TXS7_BASE           0x0800710000ULL     /* Texture Sampler */
#define DBOX0_BASE          0x08007C0000ULL     /* Display Box Registers */
#define DBOX1_BASE          0x0800620000ULL     /* Display Box Registers */
#define TD0_BASE            0x08007C0000ULL     /* Dbox Tag Directory TD */
#define TD1_BASE            0x0800620000ULL     /* Dbox Tag Directory TD */
#define VBOX_BASE           0x08007B0000ULL     /* Video Box Registers */
#define SBOX_BASE           0x08007D0000ULL     /* PCIE Box Registers */
#define GBOX0_BASE          0x08007A0000ULL     /* Gbox Front Box Registers */
#define GBOX1_BASE          0x0800790000ULL     /* Gbox Front Box Registers */
#define GBOX2_BASE          0x0800700000ULL     /* Gbox Front Box Registers */
#define GBOX3_BASE          0x08006F0000ULL     /* Gbox Front Box Registers */
#define GBOX4_BASE          0x08006D0000ULL     /* Gbox Front Box Registers */
#define GBOX5_BASE          0x08006C0000ULL     /* Gbox Front Box Registers */
#define GBOX6_BASE          0x08006B0000ULL     /* Gbox Front Box Registers */
#define GBOX7_BASE          0x08006A0000ULL     /* Gbox Front Box Registers */
#define REUT0_BASE          0x08007A0000ULL     /* Gbox REUT interface Registers */
#define REUT1_BASE          0x0800790000ULL     /* Gbox REUT interface Registers */
#define REUT2_BASE          0x0800700000ULL     /* Gbox REUT interface Registers */
#define REUT3_BASE          0x08006F0000ULL     /* Gbox REUT interface Registers */
#define REUT4_BASE          0x08006D0000ULL     /* Gbox REUT interface Registers */
#define REUT5_BASE          0x08006C0000ULL     /* Gbox REUT interface Registers */
#define REUT6_BASE          0x08006B0000ULL     /* Gbox REUT interface Registers */
#define REUT7_BASE          0x08006A0000ULL     /* Gbox REUT interface Registers */

#define GBOX_CHANNEL0_BASE  0x0
#define GBOX_CHANNEL1_BASE  0x800
#define GBOX_CHANNEL2_BASE  0x800
#define GBOX_CHANNEL3_BASE  0x1000

#endif
