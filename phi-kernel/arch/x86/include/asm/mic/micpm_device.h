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
#ifndef _MICPM_DEVICE_H_
#define _MICPM_DEVICE_H_

#define MICPM_DEVEVENT_SUSPEND		1
#define MICPM_DEVEVENT_RESUME		2
#define MICPM_DEVEVENT_FAIL_SUSPEND	3

/* Event ids for power and thermal event notifications */
//PROCHOT event caused either by thermal or by the SMC when 
//PL0 power limit is caused. Do we care which of these caused it ??
#define EVENT_PROCHOT_ON			1

//PROCHOT condition off event
#define EVENT_PROCHOT_OFF		2
//Event raised by the SMC when PL_1 card power limit is crossed
#define EVENT_PWR_ALERT_ON		3

#define EVENT_PWR_ALERT_OFF		4
#define	EVENT_TURBO_CTRL		6

#ifdef PARTIAL_CORE_TURBO
//Event raised to  initialize turbo 
#define EVENT_TURBO_INIT		5
//Event raised to toggle turbo state
//Event raised to change turbo point
#define EVENT_TURBO_LIMIT		7
#endif

#define EVENT_MCLK			8
#define EVENT_ALERT			9
#define EVENT_PROCHOT_VIDCHANGE		10
#define EVENT_PC3_ENTRY			11
#define EVENT_PC3_EXIT			12
#define EVENT_LAST			EVENT_PC3_EXIT

enum notifier_type {BLOCKING,ATOMIC};

/*These are for atomic event notifications */
void micpm_atomic_notifier_register(struct notifier_block *n);
void micpm_atomic_notifier_unregister(struct notifier_block *n);

/*These are for blocking event notifications */
void micpm_notifier_register(struct notifier_block *n);
void micpm_notifier_unregister(struct notifier_block *n);

/*These are for pm suspend/resume notofications */
void micpm_device_register(struct notifier_block *n);
void micpm_device_unregister(struct notifier_block *n);

int micevent_notify(int event, void *v,enum notifier_type type);

#endif //_MICPM_DEVICE_H_

