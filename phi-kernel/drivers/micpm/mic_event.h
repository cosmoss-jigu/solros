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
 * mic_event.h - Power and Thermal event handler header file 
 * for the Intel MIC device
 *
 *   * (C) Copyright 2008 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#ifndef _MIC_EVENT_H_
#define _MIC_EVENT_H_
#include <asm/mic/micpm_device.h>

#define MAX_TURBO_POINTS		4	//Read from the flash turbo record.

#ifdef PARTIAL_CORE_TURBO
#define TURBO_UP_MARK			1	//Low water mark for active corelimits. When the number
						//of active cores falls below corelimit-TURBO_UP_MARK for 
						//that turbo point we turbo up to the next turbo point.
#define TURBO_DN_MARK			0	//High water mark for turbo down to next lower freq.

#endif

#define PROCHOT_FREQUENCY		600000  /* The core freq is set by H/W to this freq
						   when we hit a PROCHOT condition.

						*/
//Default core freq after a PROCHOT condition. 
#define DEFAULT_PROCHOT_COREFREQ		800000

void pm_scif_notifier_register(struct notifier_block *n);
void pm_scif_notifier_unregister(struct notifier_block *n);

/* Turbo points are available turbo frequencies above the normal operating core freq
   and are dependent upon the number of active cores (ie. cores not in CC6 idle state).
   Turbo point 0 is the all cores active turbo freq. Turbo disable turns off all core
   turbo too.
*/   
struct turbopoint {
	unsigned int	freq;	//Core freq in KHz.
	int	corelimit;	//Max number of cores that can be active (not in CC6)
				// for this point
};	   

struct turbofreq {
	int	tpoints;
	struct turbopoint turbopts[MAX_TURBO_POINTS]; //Turbo points are arranged in 
						      //decreasing order of active cores 
						      //limit. 						
};


struct mic_cpufreq_object {
	unsigned int tdpfreq;	//Need this so the event handler can change policy
				//to have max_freq = tdpfreq when turbo gets disabled.
	struct turbofreq tinfo;
	void (*rtccomp)(void);
};

#ifdef PARTIAL_CORE_TURBO
///The mic_cpuidle code calls this to update the target turbo point based
//on its current active core count.
void micevent_update_turbo(int turbopoint);
#endif
			
void micevent_cpufreq_register(struct notifier_block *atomic_notifier, 
	struct notifier_block *blocking_notifier,struct  mic_cpufreq_object *micobj);
void micevent_cpufreq_unregister(void);
void clear_tmu_alert(void);
void tmu_toggle_prochot_interrupt(int state);
void tmu_toggle_pwralert_interrupt(int state);
#endif 
