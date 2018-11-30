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
File Name: micsboxdefine.h
Description:  "Raw" register offsets & bit specifications for Intel MIC (KNC)
Notes:
    Auto gen sbox register header
*******************************************************************************/
#ifndef _MIC_SBOXDEFINE_REGISTERS_H_
#define _MIC_SBOXDEFINE_REGISTERS_H_

#define SBOX_DCAR_0						0x00000000
#define SBOX_DHPR_0						0x00000004
#define SBOX_DTPR_0						0x00000008
#define SBOX_DAUX_LO_0						0x0000000C
#define SBOX_DAUX_HI_0						0x00000010
#define SBOX_DRAR_LO_0						0x00000014
#define SBOX_DRAR_HI_0						0x00000018
#define SBOX_DITR_0						0x0000001C
#define SBOX_DTPWBR_LO_0					0x00000024
#define SBOX_DTPWBR_HI_0					0x00000028
#define SBOX_DCAR_1						0x00000040
#define SBOX_DHPR_1						0x00000044
#define SBOX_DTPR_1						0x00000048
#define SBOX_DAUX_LO_1						0x0000004C
#define SBOX_DAUX_HI_1						0x00000050
#define SBOX_DRAR_LO_1						0x00000054
#define SBOX_DRAR_HI_1						0x00000058
#define SBOX_DITR_1						0x0000005C
#define SBOX_DTPWBR_LO_1					0x00000064
#define SBOX_DTPWBR_HI_1					0x00000068
#define SBOX_DCAR_2						0x00000080
#define SBOX_DHPR_2						0x00000084
#define SBOX_DTPR_2						0x00000088
#define SBOX_DAUX_LO_2						0x0000008C
#define SBOX_DAUX_HI_2						0x00000090
#define SBOX_DRAR_LO_2						0x00000094
#define SBOX_DRAR_HI_2						0x00000098
#define SBOX_DITR_2						0x0000009C
#define SBOX_DTPWBR_LO_2					0x000000A4
#define SBOX_DTPWBR_HI_2					0x000000A8
#define SBOX_DCAR_3						0x000000C0
#define SBOX_DHPR_3						0x000000C4
#define SBOX_DTPR_3						0x000000C8
#define SBOX_DAUX_LO_3						0x000000CC
#define SBOX_DAUX_HI_3						0x000000D0
#define SBOX_DRAR_LO_3						0x000000D4
#define SBOX_DRAR_HI_3						0x000000D8
#define SBOX_DITR_3						0x000000DC
#define SBOX_DTPWBR_LO_3					0x000000E4
#define SBOX_DTPWBR_HI_3					0x000000E8
#define SBOX_DCAR_4						0x00000100
#define SBOX_DHPR_4						0x00000104
#define SBOX_DTPR_4						0x00000108
#define SBOX_DAUX_LO_4						0x0000010C
#define SBOX_DAUX_HI_4						0x00000110
#define SBOX_DRAR_LO_4						0x00000114
#define SBOX_DRAR_HI_4						0x00000118
#define SBOX_DITR_4						0x0000011C
#define SBOX_DTPWBR_LO_4					0x00000124
#define SBOX_DTPWBR_HI_4					0x00000128
#define SBOX_DCAR_5						0x00000140
#define SBOX_DHPR_5						0x00000144
#define SBOX_DTPR_5						0x00000148
#define SBOX_DAUX_LO_5						0x0000014C
#define SBOX_DAUX_HI_5						0x00000150
#define SBOX_DRAR_LO_5						0x00000154
#define SBOX_DRAR_HI_5						0x00000158
#define SBOX_DITR_5						0x0000015C
#define SBOX_DTPWBR_LO_5					0x00000164
#define SBOX_DTPWBR_HI_5					0x00000168
#define SBOX_DCAR_6						0x00000180
#define SBOX_DHPR_6						0x00000184
#define SBOX_DTPR_6						0x00000188
#define SBOX_DAUX_LO_6						0x0000018C
#define SBOX_DAUX_HI_6						0x00000190
#define SBOX_DRAR_LO_6						0x00000194
#define SBOX_DRAR_HI_6						0x00000198
#define SBOX_DITR_6						0x0000019C
#define SBOX_DTPWBR_LO_6					0x000001A4
#define SBOX_DTPWBR_HI_6					0x000001A8
#define SBOX_DCAR_7						0x000001C0
#define SBOX_DHPR_7						0x000001C4
#define SBOX_DTPR_7						0x000001C8
#define SBOX_DAUX_LO_7						0x000001CC
#define SBOX_DAUX_HI_7						0x000001D0
#define SBOX_DRAR_LO_7						0x000001D4
#define SBOX_DRAR_HI_7						0x000001D8
#define SBOX_DITR_7						0x000001DC
#define SBOX_DTPWBR_LO_7					0x000001E4
#define SBOX_DTPWBR_HI_7					0x000001E8
#define SBOX_DCR						0x00000280
#define SBOX_OC_I2C_ICR						0x00001000
#define SBOX_THERMAL_STATUS					0x00001018
#define SBOX_THERMAL_INTERRUPT_ENABLE				0x0000101C
#define SBOX_STATUS_FAN1					0x00001024
#define SBOX_STATUS_FAN2					0x00001028
#define SBOX_SPEED_OVERRIDE_FAN					0x0000102C
#define SBOX_BOARD_TEMP1					0x00001030
#define SBOX_BOARD_TEMP2					0x00001034
#define SBOX_BOARD_VOLTAGE_SENSE				0x00001038
#define SBOX_CURRENT_DIE_TEMP0					0x0000103C
#define SBOX_CURRENT_DIE_TEMP1					0x00001040
#define SBOX_CURRENT_DIE_TEMP2					0x00001044
#define SBOX_MAX_DIE_TEMP0					0x00001048
#define SBOX_MAX_DIE_TEMP1					0x0000104C
#define SBOX_MAX_DIE_TEMP2					0x00001050
#define SBOX_GPU_HOT_CONFIG                                     0x00001068
#define SBOX_ELAPSED_TIME_LOW					0x00001074
#define SBOX_ELAPSED_TIME_HIGH					0x00001078
#define SBOX_THERMAL_STATUS_INTERRUPT				0x0000107C
#define SBOX_SICR0						0x00002004
#define SBOX_SICE0						0x0000200C
#define SBOX_SICC0						0x00002010
#define SBOX_SDBIC0						0x00002030
#define SBOX_SDBIC1						0x00002034
#define SBOX_SDBIC2						0x00002038
#define SBOX_SDBIC3						0x0000203C
#define SBOX_SDBIC4						0x00002040
#define SBOX_MXAR0						0x00002044
#define SBOX_MXAR1						0x00002048
#define SBOX_MXAR2						0x0000204C
#define SBOX_MXAR3						0x00002050
#define SBOX_MXAR4						0x00002054
#define SBOX_MXAR5						0x00002058
#define SBOX_MXAR6						0x0000205C
#define SBOX_MXAR7						0x00002060
#define SBOX_MXAR8						0x00002064
#define SBOX_MXAR9						0x00002068
#define SBOX_MXAR10						0x0000206C
#define SBOX_MXAR11						0x00002070
#define SBOX_MXAR12						0x00002074
#define SBOX_MXAR13						0x00002078
#define SBOX_MXAR14						0x0000207C
#define SBOX_MXAR15						0x00002080
#define SBOX_MSIXPBACR						0x00002084
#define SBOX_MCX_CTL_LO						0x00003090
#define SBOX_MCX_STATUS_LO					0x00003098
#define SBOX_MCX_STATUS_HI					0x0000309C
#define SBOX_MCX_ADDR_LO					0x000030A0
#define SBOX_MCX_ADDR_HI					0x000030A4
#define SBOX_MCX_MISC						0x000030A8
#define SBOX_MCX_MISC2						0x000030AC
#define SBOX_SMPT00						0x00003100
#define SBOX_SMPT02						0x00003108
#define SBOX_RGCR						0x00004010
#define SBOX_DSTAT						0x00004014
#define SBOX_CURRENTRATIO					0x0000402C
#define SBOX_COREFREQ						0x00004100
#define SBOX_COREVOLT						0x00004104
#define SBOX_MEMORYFREQ						0x00004108
#define SBOX_MEMVOLT						0x0000410C
#define SBOX_SVIDCONTROL					0x00004110
#define SBOX_PCUCONTROL						0x00004114
#define SBOX_HOSTPMSTATE					0x00004118
#define SBOX_UOSPMSTATE						0x0000411C
#define SBOX_C3WAKEUP_TIMER					0x00004120
#define SBOX_C3_TIMERS						0x00004128
#define SBOX_UOS_PCUCONTROL					0x0000412C
#define SBOX_PCIE_VENDOR_ID_DEVICE_ID				0x00005800
#define SBOX_PCIE_PCI_REVISION_ID_AND_C_0X8			0x00005808
#define SBOX_PCIE_BAR_ENABLE					0x00005CD4
#define SBOX_APICIDR						0x0000A800
#define SBOX_APICVER						0x0000A804
#define SBOX_APICAPR						0x0000A808
#define SBOX_APICRT0						0x0000A840
#define SBOX_APICRT1						0x0000A848
#define SBOX_APICRT2						0x0000A850
#define SBOX_APICRT3						0x0000A858
#define SBOX_APICRT4						0x0000A860
#define SBOX_APICRT5						0x0000A868
#define SBOX_APICRT6						0x0000A870
#define SBOX_APICRT7						0x0000A878
#define SBOX_APICRT8						0x0000A880
#define SBOX_APICRT9						0x0000A888
#define SBOX_APICRT10						0x0000A890
#define SBOX_APICRT11						0x0000A898
#define SBOX_APICRT12						0x0000A8A0
#define SBOX_APICRT13						0x0000A8A8
#define SBOX_APICRT14						0x0000A8B0
#define SBOX_APICRT15						0x0000A8B8
#define SBOX_APICRT16						0x0000A8C0
#define SBOX_APICRT17						0x0000A8C8
#define SBOX_APICRT18						0x0000A8D0
#define SBOX_APICRT19						0x0000A8D8
#define SBOX_APICRT20						0x0000A8E0
#define SBOX_APICRT21						0x0000A8E8
#define SBOX_APICRT22						0x0000A8F0
#define SBOX_APICRT23						0x0000A8F8
#define SBOX_APICRT24						0x0000A900
#define SBOX_APICRT25						0x0000A908
#define SBOX_APICICR0						0x0000A9D0
#define SBOX_APICICR1						0x0000A9D8
#define SBOX_APICICR2						0x0000A9E0
#define SBOX_APICICR3						0x0000A9E8
#define SBOX_APICICR4						0x0000A9F0
#define SBOX_APICICR5						0x0000A9F8
#define SBOX_APICICR6						0x0000AA00
#define SBOX_APICICR7						0x0000AA08
#define SBOX_MCA_INT_STAT					0x0000AB00
#define SBOX_MCA_INT_EN						0x0000AB04
#define SBOX_SCRATCH0						0x0000AB20
#define SBOX_SCRATCH1						0x0000AB24
#define SBOX_SCRATCH2						0x0000AB28
#define SBOX_SCRATCH3						0x0000AB2C
#define SBOX_SCRATCH4						0x0000AB30
#define SBOX_SCRATCH5						0x0000AB34
#define SBOX_SCRATCH6						0x0000AB38
#define SBOX_SCRATCH7						0x0000AB3C
#define SBOX_SCRATCH8						0x0000AB40
#define SBOX_SCRATCH9						0x0000AB44
#define SBOX_SCRATCH10						0x0000AB48
#define SBOX_SCRATCH11						0x0000AB4C
#define SBOX_SCRATCH12						0x0000AB50
#define SBOX_SCRATCH13						0x0000AB54
#define SBOX_SCRATCH14						0x0000AB58
#define SBOX_SCRATCH15						0x0000AB5C
#define SBOX_SBQ_FLUSH						0x0000B1A0
#define SBOX_TLB_FLUSH						0x0000B1A4
#define SBOX_C6_SCRATCH0					0x0000C000
#define SBOX_C6_SCRATCH1					0x0000C004
#define SBOX_C6_SCRATCH2					0x0000C008
#define SBOX_C6_SCRATCH3					0x0000C00C
#define SBOX_C6_SCRATCH4					0x0000C010
#define SBOX_C6_SCRATCH5					0x0000C014
#define SBOX_C6_SCRATCH6					0x0000C018
#define SBOX_C6_SCRATCH7					0x0000C01C
#define SBOX_C6_SCRATCH8					0x0000C020
#define SBOX_C6_SCRATCH9					0x0000C024
#define SBOX_C6_SCRATCH10					0x0000C028
#define SBOX_C6_SCRATCH11					0x0000C02C
#define SBOX_C6_SCRATCH12					0x0000C030
#define SBOX_C6_SCRATCH13					0x0000C034
#define SBOX_C6_SCRATCH14					0x0000C038
#define SBOX_C6_SCRATCH15					0x0000C03C
#define SBOX_C6_SCRATCH16					0x0000C040
#define SBOX_C6_SCRATCH17					0x0000C044
#define SBOX_C6_SCRATCH18					0x0000C048
#define SBOX_C6_SCRATCH19					0x0000C04C
#define SBOX_C6_SCRATCH20					0x0000C050
#define SBOX_C6_SCRATCH21					0x0000C054
#define SBOX_GTT_PHY_BASE					0x0000C118
#define SBOX_EMON_CNT0						0x0000CC28
#define SBOX_EMON_CNT1						0x0000CC2C
#define SBOX_EMON_CNT2						0x0000CC30
#define SBOX_EMON_CNT3						0x0000CC34
#define SBOX_RSC0						0x0000CC54
#define SBOX_RSC1						0x0000CC58

#endif
