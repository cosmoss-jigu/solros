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
File Name: micgboxdefine.h
Description:  "Raw" register offsets & bit specifications for Intel MIC (KNC)
Notes:
    Auto gen gbox register header
*******************************************************************************/
#ifndef _MIC_GBOXDEFINE_REGISTERS_H_
#define _MIC_GBOXDEFINE_REGISTERS_H_

#define GBOX_FBOX_PM_STATE					0x00000060
#define GBOX_FBOX_MCA_STATUS_LO					0x00000064
#define GBOX_FBOX_MCA_ADDR_LO					0x0000006C
#define GBOX_FBOX_MCA_MISC					0x00000074
#define GBOX_FBOX_PM_CONFIGURATION				0x00000098
#define GBOX_FBOX_PM_CONTROL					0x0000009C
#define GBOX_FBOX_PM_COUNTERS					0x000000C0
#define GBOX_MBOX_PM_CONFIG_VALID				0x000001B4
#define GBOX_MBOX_UC_RW_ADDR					0x00000700
#define GBOX_MBOX_UC_START_IP_ADDR				0x00000720
#define GBOX_MBOX_UC_CMD_START					0x00000724
#define GBOX_CBOX_AGF_MASTER_DELAY_OUT				0x00001138
#define GBOX_CBOX_AGF_MASTER_DELAY_IN				0x0000113C
#define GBOX_CBOX_AGF_MASTER_MCLK_RANGE				0x00001140

#endif
