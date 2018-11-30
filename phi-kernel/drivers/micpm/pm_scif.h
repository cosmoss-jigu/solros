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
/* ************************************************************************* *\
File Name: pm_scif.h
Description: Header file for the interface between pm_scif module and rest of PM code.
\* ************************************************************************* */

#if !defined(__PM_SCIF_H)
#define __PM_SCIF_H
#include "mic_pm_card.h"

struct mic_pmscif_handle{
	int	(*pm_scif_uos2host)(PM_MESSAGE opcode, void *msg, size_t len);
	void	(*pm_scif_host2uos)(pm_msg_header *header, void *msg);
	struct	module	*owner;
};

extern int pm_scif_register(struct mic_pmscif_handle *pmscif);
extern void pm_scif_unregister(struct mic_pmscif_handle *pmscif);

#endif //__PM_SCIF_H
