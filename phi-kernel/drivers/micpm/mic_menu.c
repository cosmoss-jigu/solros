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
 * mic_menu.c - the menu idle governor for MIC cpu devices 
 *
 *  (C) Copyright 2008 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/pm_qos_params.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/sched.h>
#include <linux/math64.h>
#include <linux/slab.h>
#include <linux/kernel_stat.h>
#include "micidle_common.h"
#include "mic_menu.h"
#include "micpm_proc.h"
#include "pm_scif.h"
#include "mic_event.h"

/*
 * Concepts and ideas behind the menu governor
 *
 * For the menu governor, there are 3 decision factors for picking a C
 * state:
 * 1) Energy break even point
 * 2) Performance impact
 * 3) Latency tolerance (from pmqos infrastructure)
 * These these three factors are treated independently.
 *
 * Energy break even point
 * -----------------------
 * C state entry and exit have an energy cost, and a certain amount of time in
 * the  C state is required to actually break even on this cost. CPUIDLE
 * provides us this duration in the "target_residency" field. So all that we
 * need is a good prediction of how long we'll be idle. Like the traditional
 * menu governor, we start with the actual known "next timer event" time.
 *
 * Since there are other source of wakeups (interrupts for example) than
 * the next timer event, this estimation is rather optimistic. To get a
 * more realistic estimate, a correction factor is applied to the estimate,
 * that is based on historic behavior. For example, if in the past the actual
 * duration always was 50% of the next timer tick, the correction factor will
 * be 0.5.
 *
 * menu uses a running average for this correction factor, however it uses a
 * set of factors, not just a single factor. This stems from the realization
 * that the ratio is dependent on the order of magnitude of the expected
 * duration; if we expect 500 milliseconds of idle time the likelihood of
 * getting an interrupt very early is much higher than if we expect 50 micro
 * seconds of idle time. A second independent factor that has big impact on
 * the actual factor is if there is (disk) IO outstanding or not.
 * (as a special twist, we consider every sleep longer than 50 milliseconds
 * as perfect; there are no power gains for sleeping longer than this)
 *
 * For these two reasons we keep an array of 12 independent factors, that gets
 * indexed based on the magnitude of the expected duration as well as the
 * "is IO outstanding" property.
 *
 * Limiting Performance Impact
 * ---------------------------
 * C states, especially those with large exit latencies, can have a real
 * noticable impact on workloads, which is not acceptable for most sysadmins,
 * and in addition, less performance has a power price of its own.
 *
 * As a general rule of thumb, menu assumes that the following heuristic
 * holds:
 *     The busier the system, the less impact of C states is acceptable
 *
 * This rule-of-thumb is implemented using a performance-multiplier:
 * If the exit latency times the performance multiplier is longer than
 * the predicted duration, the C state is not considered a candidate
 * this multiplier is, the longer we need to be idle to pick a deep C
 * for selection due to a too high performance impact. So the higher
 * state, and thus the less likely a busy CPU will hit such a deep
 * C state.
 *
 * Two factors are used in determing this multiplier:
 * a value of 10 is added for each point of "per cpu load average" we have.
 * a value of 5 points is added for each process that is waiting for
 * IO on this CPU.
 * (these values are experimentally determined)
 *
 * The load average factor gives a longer term (few seconds) input to the
 * decision, while the iowait value gives a cpu local instantanious input.
 * The iowait factor may look low, but realize that this is also already
 * represented in the system load average.
 *
 */

#define MAX_CPUS	248

#ifdef MICPM_PROC_ENTRY
DEFINE_PER_CPU(struct menu_device, menu_devices);
cpumask_t pc3readymask;	
#else
static DEFINE_PER_CPU(struct menu_device, menu_devices);
static cpumask_t pc3readymask;
#endif

static void menu_update(struct cpuidle_device *dev);

static int __read_mostly mic_pkgstate_ready;

#ifdef PC3_POLICY_DEBUG
struct activity actrecs[MAX_CPUS];
void mic_update_activity(struct task_struct *p)
{
	struct pc3record *pr = &__get_cpu_var(pc3data);
	int cpu = smp_processor_id();
	int times = actrecs[cpu].times;
	
	if(pc3policy) {
		if(!(__get_cpu_var(policychanged))) {
			if(times < MAX_ACTIVITY_RECORDS) {
				actrecs[cpu].actarr[times].pid=task_pid_nr(p);
				strcpy(actrecs[cpu].actarr[times].name,p->comm);
				actrecs[cpu].actarr[times].user=1;
				actrecs[cpu].actarr[times].t_after=ktime_us_delta(ktime_get(),pr->tstamp);
				}
			actrecs[cpu].times += 1;
		}
	}
}
#endif
DEFINE_PER_CPU(struct pc3record,pc3data);
DEFINE_PER_CPU(int, policychanged);

static int __read_mostly pc3policy = 0;
static int __read_mostly pc3_config;	/* PC3 config setting in micx.conf file */
 
static unsigned long __read_mostly pc3_inactivity;
unsigned int pc3_disable_count;

static void mic_policy_callback(void *v)
{
	/* we already woke the CPUS up, nothing more to do */
}

static int mic_policy_change_notify(void)
{
	smp_call_function(mic_policy_callback, NULL, 1);
	return 0;
}

/**
 * mic_menu_set_pc3policy - Turn on/off policy based choice of PC3 package state
 * @policy: 0 means PC3 is disabled.
 *	    >0 means the governor will choose PC3 state for cpu based on 
 *	       userland (in)activity for this cpu over a length of time.  
 */
void mic_menu_set_pc3policy(int policy)
{
	int i;
	
	preempt_disable();
	if(pc3policy != policy) {
		if(policy) {
			for_each_online_cpu(i) {
				per_cpu(policychanged,i) = 1;
				#ifdef PC3_POLICY_DEBUG
				actrecs[i].times=0;
				#endif
			}
		}
		dprintk("Setting pc3policy to %d\n",policy);
		pc3policy=policy;
		smp_wmb();
		mic_policy_change_notify();	
	}
	preempt_enable();		
}
EXPORT_SYMBOL(mic_menu_set_pc3policy);

unsigned int mic_menu_get_pc3policy(void)
{
	dprintk("Returning pc3policy = %d\n",pc3policy);
	return(pc3policy);
}
EXPORT_SYMBOL(mic_menu_get_pc3policy);

/**
 * mic_menu_set_pc3inactivity - Set the amount of continous time in secs the cpu 
 				goes without spending any time in userland. When this
				threshold is crossed we choose PC3 for the cpu.	  
 * @time: 			In secs.  
 */
void mic_menu_set_pc3_inactivity(int time)
{
	int i;
	
	if(time >= MIN_PC3_INACTIVITY) {
		dprintk("Setting pc3 inactivity = %d\n",time);
		pc3_inactivity = time;
		smp_wmb();
		if(pc3policy) {
			for_each_online_cpu(i) {
				per_cpu(policychanged,i) = 1;
				#ifdef PC3_POLCY_DEBUG
				actrecs[i].times=0;
				#endif
			}
		}
	}	
}
			
unsigned int mic_menu_get_pc3_inactivity(void)
{
	dprintk("Returning pc3 inactivity = %lu\n",pc3_inactivity);
	return pc3_inactivity;
}	

#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)


static int get_loadavg(void)
{
	unsigned long this = this_cpu_load();


	return LOAD_INT(this) * 10 + LOAD_FRAC(this) / 10;
}

static inline int which_bucket(unsigned int duration)
{
	int bucket = 0;

	/*
	 * We keep two groups of stats; one with no
	 * IO pending, one without.
	 * This allows us to calculate
	 * E(duration)|iowait
	 */
#if 0	 
	if (nr_iowait_cpu())
		bucket = BUCKETS/2;
#endif
	if (duration < 10)
		return bucket;
	if (duration < 100)
		return bucket + 1;
	if (duration < 1000)
		return bucket + 2;
	if (duration < 10000)
		return bucket + 3;
	if (duration < 100000)
		return bucket + 4;
	return bucket + 5;
}

/*
 * Return a multiplier for the exit latency that is intended
 * to take performance requirements into account.
 * The more performance critical we estimate the system
 * to be, the higher this multiplier, and thus the higher
 * the barrier to go to an expensive C state.
 */
static inline int performance_multiplier(void)
{
	int mult = 1;

	/* for higher loadavg, we are more reluctant */

	mult += 2 * get_loadavg();
#if 0
	/* for IO wait tasks (per cpu!) we add 5x each */
	mult += 10 * nr_iowait_cpu();
#endif
	return mult;
}

void mic_pc3_enable(void)
{
	struct pc3record *pr = &__get_cpu_var(pc3data);

	atomic_dec(&pr->pc3_disable);
}

void mic_pc3_disable(void)
{
	struct pc3record *pr = &__get_cpu_var(pc3data);

	atomic_inc(&pr->pc3_disable);
}

/* This implements DIV_ROUND_CLOSEST but avoids 64 bit division */
static u64 div_round64(u64 dividend, u32 divisor)
{
	return div_u64(dividend + (divisor / 2), divisor);
}

static int mic_test_pc3ready(int cpu)
{
	struct pc3record *pr = &__get_cpu_var(pc3data);
	cputime64_t user = kstat_cpu(cpu).cpustat.user + kstat_cpu(cpu).cpustat.nice;
	
	if(!mic_pkgstate_ready || !pc3policy)
		return 0;
	if(__get_cpu_var(policychanged)) {
		__get_cpu_var(policychanged)=0;
		cpumask_clear(&pc3readymask);
	}
	else { 
		if(pr->usertime == user) {
			if(ktime_to_us(ktime_sub(ktime_get(),pr->tstamp)) > 
		 pc3_inactivity*1000000)  {
				if (atomic_read(&pr->pc3_disable)) {
					pc3_disable_count++;
				} else {
					cpumask_set_cpu(cpu,&pc3readymask); 
		   			return 1;
				}
			}
			return 0;
		}
		/* Come here if there has been some usertime on the cpu meanwhile */ 
		cpumask_clear_cpu(cpu,&pc3readymask);
	}		
	pr->usertime = user;
	pr->tstamp = ktime_get();		
	return 0;
}

/**
 * mic_select - selects the next MIC idle state to enter. Mostly does MIC device 
 * specific processing based on the idle state chosen by the generic menu_select
 * algorithm.
 * @dev: the CPU idle device
 * @index: index of idle state chosen by the generic select alg.
 * @data: per_cpu menu_device pointer. 
 *
 */
static int mic_select(int index, struct menu_device *data, struct cpuidle_device  *dev)
{
	int cpu = dev->cpu;
	int newstate = index;
	
	if(pc3_config && mic_test_pc3ready(cpu)) {
	/* Choose the deepest state possible */
		newstate = dev->state_count-1;
	}
	return newstate;
}		

/**
 * menu_select - selects the next idle state to enter
 * @dev: the CPU
 **/
static int menu_select(struct cpuidle_device *dev)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);
/*	int latency_req = pm_qos_requirement(PM_QOS_CPU_DMA_LATENCY);  */
	int latency_req = MIC_LATENCY_REQ;
	int i;
	int multiplier;

	if (data->needs_update) {
		menu_update(dev);
		data->needs_update = 0;
	}

	data->last_state_idx = 0;
	data->exit_us = 0;

	/* Special case when user has set very strict latency requirement */
	if (unlikely(latency_req == 0))
		return 0;

	/* determine the expected residency time, round up */
	data->expected_us =
	    DIV_ROUND_UP((u32)ktime_to_ns(tick_nohz_get_sleep_length()), 1000);

	
	data->bucket = which_bucket(data->expected_us);

	multiplier = performance_multiplier();

	/*
	 * if the correction factor is 0 (eg first time init or cpu hotplug
	 * etc), we actually want to start out with a unity factor.
	 */
	if (data->correction_factor[data->bucket] == 0)
		data->correction_factor[data->bucket] = RESOLUTION * DECAY;

	/* Make sure to round up for half microseconds */
	data->predicted_us = div_round64(data->expected_us * data->correction_factor[data->bucket],
					 RESOLUTION * DECAY);

	/*
	 * We want to default to C1 (hlt), not to busy polling
	 * unless the timer is happening really really soon.
	 */
	if (data->expected_us > 5)
		data->last_state_idx = CPUIDLE_DRIVER_STATE_START;


	/* find the deepest idle state that satisfies our constraints */
	for (i = CPUIDLE_DRIVER_STATE_START; i < dev->state_count; i++) {
		struct cpuidle_state *s = &dev->states[i];

		if (s->target_residency > data->predicted_us)
			break;
		if (s->exit_latency > latency_req)
			break;
		if (s->exit_latency * multiplier > data->predicted_us)
			break;
		data->exit_us = s->exit_latency;
		data->last_state_idx = i;
	}
	return mic_select(data->last_state_idx,data,dev);
}

/**
 * menu_reflect - records that data structures need update
 * @dev: the CPU
 *
 * NOTE: it's important to be fast here because this operation will add to
 *       the overall exit latency.
**/
static void menu_reflect(struct cpuidle_device *dev)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);
	data->needs_update = 1;
}


/**
 * menu_update - attempts to guess what happened after entry
 * @dev: the CPU
 */
static void menu_update(struct cpuidle_device *dev)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);
	int last_idx = data->last_state_idx;
	unsigned int last_idle_us = cpuidle_get_last_residency(dev);
	struct cpuidle_state *target = &dev->states[last_idx];
	unsigned int measured_us;
	u64 new_factor;

	/*
	 * Ugh, this idle state doesn't support residency measurements, so we
	 * are basically lost in the dark.  As a compromise, assume we slept
	 * for the whole expected time.
	 */
	if (unlikely(!(target->flags & CPUIDLE_FLAG_TIME_VALID)))
		last_idle_us = data->expected_us;


	measured_us = last_idle_us;

	/*
	 * We correct for the exit latency; we are assuming here that the
	 * exit latency happens after the event that we're interested in.
	 */
	if (measured_us > data->exit_us)
		measured_us -= data->exit_us;


	/* update our correction ratio */

	new_factor = data->correction_factor[data->bucket]
			* (DECAY - 1) / DECAY;

	if (data->expected_us > 0 && measured_us < MAX_INTERESTING)
		new_factor += RESOLUTION * measured_us / data->expected_us;
	else
		/*
		 * we were idle so long that we count it as a perfect
		 * prediction
		 */
		new_factor += RESOLUTION;

	/*
	 * We don't want 0 as factor; we always want at least
	 * a tiny bit of estimated time.
	 */
	if (new_factor == 0)
		new_factor = 1;
	
	data->correction_factor[data->bucket] = new_factor;
	
}
static int mic_menu_scif_notifier(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	switch (event) {
		case PM_MESSAGE_OPEN:
			mic_pkgstate_ready=1;
			pr_debug("PM_OPEN message handled in mic_menu\n");
			break;
		case PM_MESSAGE_CLOSE:
			mic_pkgstate_ready=0;
			break;	
	default:
			break;
	}
	return NOTIFY_OK;
}
static struct notifier_block mic_menu_scif_notifier_block = {
	.notifier_call = mic_menu_scif_notifier,
};

/**
 * menu_enable_device - scans a CPU's states and does setup
 * @dev: the CPU
 */
static int menu_enable_device(struct cpuidle_device *dev)
{
	struct menu_device *data = &per_cpu(menu_devices, dev->cpu);

	memset(data, 0, sizeof(struct menu_device));
	if(pc3policy) {
		per_cpu(policychanged,dev->cpu) = 1;
	}
	#ifdef PC3_POLICY_DEBUG
	actrecs[dev->cpu].times=0;
	#endif
	cpumask_clear(&pc3readymask);
	return 0;
}

static struct cpuidle_governor mic_menu_governor = {
	.name =		"mic_menu",
	.rating =	40,
	.enable =	menu_enable_device,
	.select =	menu_select,
	.reflect =	menu_reflect,
	.owner =	THIS_MODULE,
};



/*
 * mic_init_menu - initializes the governor
 */
int __init mic_init_menu(int pc3)
{
	
	printk(KERN_INFO"mic_menu: Registering governor with pc3_config = %d\n",
			pc3_config);				
	pc3_config=pc3;
	mic_pkgstate_ready=0;
	pc3policy = DEFAULT_PC3_POLICY;
	pc3_inactivity = DEFAULT_PC3_INACTIVITY;
	if(pc3_config)
		pm_scif_notifier_register(&mic_menu_scif_notifier_block);
	return cpuidle_register_governor(&mic_menu_governor);
}

/**
 * mic_exit_menu - exits the mic_menu governor
 */
void __exit mic_exit_menu(void)
{
	pm_scif_notifier_unregister(&mic_menu_scif_notifier_block);
	cpuidle_unregister_governor(&mic_menu_governor);
}

