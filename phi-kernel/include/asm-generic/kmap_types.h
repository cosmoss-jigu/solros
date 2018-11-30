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
#ifndef _ASM_GENERIC_KMAP_TYPES_H
#define _ASM_GENERIC_KMAP_TYPES_H

#ifdef __WITH_KM_FENCE
# define KMAP_D(n) __KM_FENCE_##n ,
#else
# define KMAP_D(n)
#endif

enum km_type {
KMAP_D(0)	KM_BOUNCE_READ,
KMAP_D(1)	KM_SKB_SUNRPC_DATA,
KMAP_D(2)	KM_SKB_DATA_SOFTIRQ,
KMAP_D(3)	KM_USER0,
KMAP_D(4)	KM_USER1,
KMAP_D(5)	KM_BIO_SRC_IRQ,
KMAP_D(6)	KM_BIO_DST_IRQ,
KMAP_D(7)	KM_PTE0,
KMAP_D(8)	KM_PTE1,
KMAP_D(9)	KM_IRQ0,
KMAP_D(10)	KM_IRQ1,
KMAP_D(11)	KM_SOFTIRQ0,
KMAP_D(12)	KM_SOFTIRQ1,
KMAP_D(13)	KM_SYNC_ICACHE,
KMAP_D(14)	KM_SYNC_DCACHE,
/* UML specific, for copy_*_user - used in do_op_one_page */
KMAP_D(15)	KM_UML_USERCOPY,
KMAP_D(16)	KM_IRQ_PTE,
KMAP_D(17)	KM_NMI,
KMAP_D(18)	KM_NMI_PTE,
KMAP_D(19)	KM_KDB,
/*
 * Remember to update debug_kmap_atomic() when adding new kmap types!
 */
KMAP_D(20)	KM_TYPE_NR
};

#undef KMAP_D

#endif
