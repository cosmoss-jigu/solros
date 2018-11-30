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
 * mic_menu.h:  Header file for the menu governor for 
 * 		Intel MIC devices.
 *
 * (C) Copyright 2008 Intel Corporation
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef	_MIC_MENU_H_
#define _MIC_MENU_H_

#define BUCKETS 12
#define RESOLUTION 1024
#define DECAY 4
#define MAX_INTERESTING 70000
#define MIC_LATENCY_REQ	165000	/* Max permissible latency (us) across all idle states */
#define MIN_PC3_INACTIVITY   1 /*inactivity time (no user time consumed on cpu for 
				     this many continous secs) before PC3 is chosen on cpu.
				*/
#define DEFAULT_PC3_INACTIVITY 15
#define DEFAULT_PC3_POLICY    1	/* Do we choose PC3 state through the policy governor
				   by default ?
				*/
#define PREFIX "mic_menu: "

#define MIC_MENU_DEBUG

#ifdef	MIC_MENU_DEBUG
#ifdef CONFIG_GCOV_KERNEL
#define dprintk(msg...) printk(msg)
#else
#define dprintk(msg...) pr_debug(msg)
#endif
#else
#define dprintk
#endif

struct menu_device {
	int		last_state_idx;
	int             needs_update;

	unsigned int	expected_us;
	u64		predicted_us;
	unsigned int	exit_us;
	unsigned int	bucket;
	u64		correction_factor[BUCKETS];
};

/* Per-cpu data structure to keep a running record of 
   userland time spent by this cpu and start timestamp
   of the monitoring time window.
*/     
struct pc3record {
	cputime64_t usertime;
	ktime_t tstamp;
	atomic_t pc3_disable;
};	

#define MAX_ACTIVITY_RECORDS	50

struct cpuactivity {
	int 	pid; //pid that caused activity
	char    name[20];
	int	user;
	int 	nice;
	unsigned long t_after;
};

struct  activity {
	int	times;
	struct cpuactivity actarr[MAX_ACTIVITY_RECORDS];
};

DECLARE_PER_CPU(struct menu_device,menu_devices);
DECLARE_PER_CPU(int,multiplier);

DECLARE_PER_CPU(struct pc3record, pc3data);
DECLARE_PER_CPU(int, policychanged);

void mic_pc3_disable(void);
void mic_pc3_enable(void);
#endif

