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
#ifndef MIC_COMMON_H
#define MIC_COMMON_H

#define U32 uint32_t
#define U64 uint64_t

#define MIC_MBYTES(x) (x*1024*1024)

#ifndef KASSERT
#define KASSERT(x, y, args...)		\
	do {				\
		if(!x)			\
			printk(y, ## args);\
		BUG_ON(!x);		\
	} while(0)			
#endif

/* vnet/mic_shutdown/hvc/virtio */
#define VNET_SBOX_INT_IDX	0
#define MIC_SHT_SBOX_INT_IDX	1
#define HVC_SBOX_INT_IDX	2
#define VIRTIO_SBOX_INT_IDX	3

int get_sbox_irq(int index);

enum
{
	KNC_A_STEP = 0,
	KNC_B0_STEP,
	KNC_C_STEP,
	KNC_B1_STEP
};

enum
{
	FAMILY_UNKNOWN = 0,
	FAMILY_ABR,
	FAMILY_KNC
};

/*------------------------------------------------------------------------------
 *  FUNCTION: mic_hw_family
 *
 *  DESCRIPTION: Obtains MIC Silicon Family.
 *
 *  PARAMETERS: unused. Useful to have same definition as Host Driver.
 *
 *  RETURNS: family.
*------------------------------------------------------------------------------*/
static __always_inline int mic_hw_family(int unused)
{
	struct cpuinfo_x86 *c = &cpu_data(0);
	switch (c->x86_model) {
	case 0:
		return FAMILY_ABR;
	case 1:
		return FAMILY_KNC;
	default:
		return FAMILY_UNKNOWN;
	}
}

/*------------------------------------------------------------------------------
 *  FUNCTION: mic_hw_stepping
 *
 *  DESCRIPTION: Obtains MIC Silicon Stepping.
 *
 *  PARAMETERS: unused. Useful to have same definition as Host Driver.
 *
 *  RETURNS: stepping.
*------------------------------------------------------------------------------*/
static __always_inline int mic_hw_stepping(int unused)
{
	struct cpuinfo_x86 *c = &cpu_data(0);
	return c->x86_mask;
}
#endif
