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
 *  drivers/cpufreq/cpufreq_knc_governor.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *            (C)  2009 Alexander Clouter <alex@digriz.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>

/*
 * dbs is used in this file as a shortform for demandbased switching
 * It helps to keep variable names smaller, simpler
 */


#define DEF_FREQUENCY_TDP_THRESHOLD		(80)	/* Load threshold before freq can be bumped to TDP value */
#define DEF_FREQUENCY_TDP_DIFFERENTIAL		(3)	/*  Up differential from TDP threshold before the policy will 
								    start to increase core frequency if turbo/overclocking
								    enabled.
							       	*/								    
#define DEF_FREQUENCY_DOWN_DIFFERENTIAL		(3)	/*   Down differential from TDP_THRESHOLD before the policy will
							            	     start to reduce core freq.
								*/
										
																    								    

/*
 * The polling frequency of this governor depends on the capability of
 * the processor. Default polling frequency is 1000 times the transition
 * latency of the processor. The governor will work on any processor with
 * transition latency <= 10mS, using appropriate sampling
 * rate.
 * For CPUs with transition latency > 10mS (mostly drivers with CPUFREQ_ETERNAL)
 * this governor will not work.
 * All times here are in uS.
 */
#define MIN_SAMPLING_RATE_RATIO			(2)

static unsigned int min_sampling_rate;

#define LATENCY_MULTIPLIER			(1000)
#define MIN_LATENCY_MULTIPLIER			(100)
#define TRANSITION_LATENCY_LIMIT		(10 * 1000 * 1000)


#define	TURBO_MODE_OFF	(0)
#define    TURBO_MODE_ON	(1)
#define    OVERCLOCKING_OFF      (0)
#define    OVERCLOCKING_ON        (1)


static void do_dbs_timer(struct work_struct *work);

struct cpu_dbs_info_s {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_nice;
	struct cpufreq_policy *cur_policy;
	struct delayed_work work;
#if 0
	unsigned int requested_freq;
#endif	
	int cpu;
	unsigned int enable:1;
	/*
	 * percpu mutex that serializes governor limit change with
	 * do_dbs_timer invocation. We do not want do_dbs_timer to run
	 * when user is changing the governor or limits.
	 */
	struct mutex timer_mutex;
};
static DEFINE_PER_CPU(struct cpu_dbs_info_s, cs_cpu_dbs_info);

static unsigned int dbs_enable;	/* number of CPUs using this policy */

/*
 * dbs_mutex protects data in dbs_tuners_ins from concurrent changes on
 * different CPUs. It protects dbs_enable in governor start/stop.
 */
static DEFINE_MUTEX(dbs_mutex);

static struct workqueue_struct	*knc_wq;

static struct dbs_tuners {
	unsigned int sampling_rate;
	unsigned int ignore_nice;
	unsigned int freq_step;
	unsigned int tdp_threshold;
	unsigned int tdp_differential;
	unsigned int down_differential;
	unsigned int turbo_mode;
	unsigned int overclocking;	
} dbs_tuners_ins = {
	.down_differential = DEF_FREQUENCY_DOWN_DIFFERENTIAL,
	.ignore_nice = 0,
	.freq_step = 5,
	.tdp_threshold = DEF_FREQUENCY_TDP_THRESHOLD,
	.tdp_differential = DEF_FREQUENCY_TDP_DIFFERENTIAL,
	.turbo_mode = TURBO_MODE_OFF,
	.overclocking = OVERCLOCKING_OFF,
};

static inline cputime64_t get_cpu_idle_time_jiffy(unsigned int cpu,
							cputime64_t *wall)
{
	cputime64_t idle_time;
	cputime64_t cur_wall_time;
	cputime64_t busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());
	busy_time = cputime64_add(kstat_cpu(cpu).cpustat.user,
			kstat_cpu(cpu).cpustat.system);

	busy_time = cputime64_add(busy_time, kstat_cpu(cpu).cpustat.irq);
	busy_time = cputime64_add(busy_time, kstat_cpu(cpu).cpustat.softirq);
	busy_time = cputime64_add(busy_time, kstat_cpu(cpu).cpustat.steal);
	busy_time = cputime64_add(busy_time, kstat_cpu(cpu).cpustat.nice);

	idle_time = cputime64_sub(cur_wall_time, busy_time);
	if (wall)
		*wall = (cputime64_t)jiffies_to_usecs(cur_wall_time);

	return (cputime64_t)jiffies_to_usecs(idle_time);;
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, wall);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);

	return idle_time;
}

#if 0
/* keep track of frequency transitions */
static int
dbs_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
		     void *data)
{
	struct cpufreq_freqs *freq = data;
	struct cpu_dbs_info_s *this_dbs_info = &per_cpu(cs_cpu_dbs_info,
							freq->cpu);

	struct cpufreq_policy *policy;

	if (!this_dbs_info->enable)
		return 0;

	policy = this_dbs_info->cur_policy;

	/*
	 * we only care if our internally tracked freq moves outside
	 * the 'valid' ranges of freqency available to us otherwise
	 * we do not change it
	*/
	if (this_dbs_info->requested_freq > policy->max
			|| this_dbs_info->requested_freq < policy->min)
		this_dbs_info->requested_freq = freq->new;

	return 0;
}

#endif

#if 0
static struct notifier_block dbs_cpufreq_notifier_block = {
	.notifier_call = dbs_cpufreq_notifier
};
#endif

/************************** sysfs interface ************************/
static ssize_t show_sampling_rate_max(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	printk_once(KERN_INFO "CPUFREQ: KNC governor sampling_rate_max "
		    "sysfs file is deprecated - used by: %s\n", current->comm);
	return sprintf(buf, "%u\n", -1U);
}

static ssize_t show_sampling_rate_min(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", min_sampling_rate);
}

#define define_one_ro(_name)		\
static struct global_attr _name =	\
__ATTR(_name, 0444, show_##_name, NULL)

define_one_ro(sampling_rate_max);
define_one_ro(sampling_rate_min);

/* cpufreq_knc Governor Tunables */
#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return sprintf(buf, "%u\n", dbs_tuners_ins.object);		\
}
show_one(sampling_rate, sampling_rate);
show_one(ignore_nice_load, ignore_nice);
show_one(freq_step, freq_step);
show_one(tdp_threshold, tdp_threshold);
show_one(tdp_differential, tdp_differential);
show_one(down_differential,down_differential);
show_one(turbo_mode,turbo_mode);
show_one(overclocking,overclocking);

#if 0
/*** delete after deprecation time ***/
#define DEPRECATION_MSG(file_name)					\
	printk_once(KERN_INFO "CPUFREQ: Per core knc governor sysfs "	\
		"interface is deprecated - " #file_name "\n");

#define show_one_old(file_name)						\
static ssize_t show_##file_name##_old					\
(struct cpufreq_policy *unused, char *buf)				\
{									\
	printk_once(KERN_INFO "CPUFREQ: Per core knc governor sysfs "	\
		"interface is deprecated - " #file_name "\n");		\
	return show_##file_name(NULL, NULL, buf);			\
}
show_one_old(sampling_rate);
show_one_old(sampling_down_factor);
show_one_old(up_threshold);
show_one_old(down_threshold);
show_one_old(ignore_nice_load);
show_one_old(freq_step);
show_one_old(sampling_rate_min);
show_one_old(sampling_rate_max);

#define define_one_ro_old(object, _name)	\
static struct freq_attr object =		\
__ATTR(_name, 0444, show_##_name##_old, NULL)

define_one_ro_old(sampling_rate_min_old, sampling_rate_min);
define_one_ro_old(sampling_rate_max_old, sampling_rate_max);

#endif

/*** delete after deprecation time ***/


static ssize_t store_sampling_rate(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	mutex_lock(&dbs_mutex);
	dbs_tuners_ins.sampling_rate = max(input, min_sampling_rate);
	mutex_unlock(&dbs_mutex);

	return count;
}

static ssize_t store_tdp_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	mutex_lock(&dbs_mutex);
	if (ret != 1 || input > 100 ||
			input <= dbs_tuners_ins.down_differential) {
		mutex_unlock(&dbs_mutex);
		return -EINVAL;
	}

	dbs_tuners_ins.tdp_threshold = input;
	mutex_unlock(&dbs_mutex);

	return count;
}

static ssize_t store_down_differential(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	mutex_lock(&dbs_mutex);
	if (ret != 1 || input > 100 ||
			input >= dbs_tuners_ins.tdp_threshold) {
		mutex_unlock(&dbs_mutex);
		return -EINVAL;
	}
	dbs_tuners_ins.down_differential = input;
	mutex_unlock(&dbs_mutex);

	return count;
}

static ssize_t store_tdp_differential(struct kobject *a, struct attribute *b,
			       const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;
		
	mutex_lock(&dbs_mutex);
	if (input > (100-dbs_tuners_ins.tdp_threshold))
		input = 100-dbs_tuners_ins.tdp_threshold ;
	dbs_tuners_ins.tdp_differential = input;
	mutex_unlock(&dbs_mutex);

	return count;
}

static ssize_t store_ignore_nice_load(struct kobject *a, struct attribute *b,
				      const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	mutex_lock(&dbs_mutex);
	if (input == dbs_tuners_ins.ignore_nice) { /* nothing to do */
		mutex_unlock(&dbs_mutex);
		return count;
	}
	dbs_tuners_ins.ignore_nice = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(cs_cpu_dbs_info, j);
		dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&dbs_info->prev_cpu_wall);
		if (dbs_tuners_ins.ignore_nice)
			dbs_info->prev_cpu_nice = kstat_cpu(j).cpustat.nice;
	}
	mutex_unlock(&dbs_mutex);

	return count;
}

static ssize_t store_freq_step(struct kobject *a, struct attribute *b,
			       const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;
	if (input > 100)
		input = 100;

	/* no need to test here if freq_step is zero as the user might actually
	 * want this, they would be crazy though :) */
	mutex_lock(&dbs_mutex);
	dbs_tuners_ins.freq_step = input;
	mutex_unlock(&dbs_mutex);

	return count;
}
static ssize_t store_turbo_mode(struct kobject *a, struct attribute *b,
			       const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* no need to test here if freq_step is zero as the user might actually
	 * want this, they would be crazy though :) */
	mutex_lock(&dbs_mutex);
	dbs_tuners_ins.turbo_mode = input;
	mutex_unlock(&dbs_mutex);

	return count;
}
static ssize_t store_overclocking(struct kobject *a, struct attribute *b,
			       const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* no need to test here if freq_step is zero as the user might actually
	 * want this, they would be crazy though :) */
	mutex_lock(&dbs_mutex);
	dbs_tuners_ins.overclocking = input;
	mutex_unlock(&dbs_mutex);

	return count;
}

#define define_one_rw(_name) \
static struct global_attr _name = \
__ATTR(_name, 0644, show_##_name, store_##_name)

define_one_rw(sampling_rate);
define_one_rw(ignore_nice_load);
define_one_rw(freq_step);
define_one_rw(tdp_threshold);
define_one_rw(tdp_differential);
define_one_rw(down_differential);
define_one_rw(turbo_mode);
define_one_rw(overclocking);

static struct attribute *dbs_attributes[] = {
	&sampling_rate_max.attr,
	&sampling_rate_min.attr,
	&sampling_rate.attr,
	&ignore_nice_load.attr,
	&freq_step.attr,
	&tdp_threshold.attr,
	&tdp_differential.attr,
	&down_differential.attr,
	&turbo_mode.attr,
	&overclocking.attr,		
	NULL
};

static struct attribute_group dbs_attr_group = {
	.attrs = dbs_attributes,
	.name = "knc_governor",
};

/*** delete after deprecation time ***/
#if 0
#define write_one_old(file_name)					\
static ssize_t store_##file_name##_old					\
(struct cpufreq_policy *unused, const char *buf, size_t count)		\
{									\
	printk_once(KERN_INFO "CPUFREQ: Per core conservative sysfs "	\
		"interface is deprecated - " #file_name "\n");	\
	return store_##file_name(NULL, NULL, buf, count);		\
}
write_one_old(sampling_rate);
write_one_old(sampling_down_factor);
write_one_old(up_threshold);
write_one_old(down_threshold);
write_one_old(ignore_nice_load);
write_one_old(freq_step);

#define define_one_rw_old(object, _name)	\
static struct freq_attr object =		\
__ATTR(_name, 0644, show_##_name##_old, store_##_name##_old)

define_one_rw_old(sampling_rate_old, sampling_rate);
define_one_rw_old(sampling_down_factor_old, sampling_down_factor);
define_one_rw_old(up_threshold_old, up_threshold);
define_one_rw_old(down_threshold_old, down_threshold);
define_one_rw_old(ignore_nice_load_old, ignore_nice_load);
define_one_rw_old(freq_step_old, freq_step);

static struct attribute *dbs_attributes_old[] = {
	&sampling_rate_max_old.attr,
	&sampling_rate_min_old.attr,
	&sampling_rate_old.attr,
	&sampling_down_factor_old.attr,
	&up_threshold_old.attr,
	&down_threshold_old.attr,
	&ignore_nice_load_old.attr,
	&freq_step_old.attr,
	NULL
};

static struct attribute_group dbs_attr_group_old = {
	.attrs = dbs_attributes_old,
	.name = "conservative",
};

#endif
/*** delete after deprecation time ***/

/************************** sysfs end ************************/

static void dbs_check_cpu(struct cpu_dbs_info_s *this_dbs_info)
{
	unsigned int load = 0;
	unsigned int max_load = 0;
	unsigned int freq_target;

	struct cpufreq_policy *policy;
	unsigned int j;

	policy = this_dbs_info->cur_policy;

	/*
	 * Every sampling_rate, we check, if current  cpu load is less
	 * than the TDP_THRESHOLD setting (default=80%), then we try decrease frequency
	 * so that the cpu load gets upto the threshold. If the cpu load equals or exceeds TDP_THRESHOLD then
	 * the frequency is bumped up to the TDP value. If the cpu load is in the guard band around TDP
	 * we do nothing. If the cpu load exceeds the TDP guard band and if turbo mode or overclocking is enabled
	 * then we increase freq in steps till we reach the max_freq value.
	  */

	/* Get Absolute Load */
	for_each_cpu(j, policy->cpus) {
		struct cpu_dbs_info_s *j_dbs_info;
		cputime64_t cur_wall_time, cur_idle_time;
		unsigned int idle_time, wall_time;

		j_dbs_info = &per_cpu(cs_cpu_dbs_info, j);

		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time);

		wall_time = (unsigned int) cputime64_sub(cur_wall_time,
				j_dbs_info->prev_cpu_wall);
		j_dbs_info->prev_cpu_wall = cur_wall_time;

		idle_time = (unsigned int) cputime64_sub(cur_idle_time,
				j_dbs_info->prev_cpu_idle);
		j_dbs_info->prev_cpu_idle = cur_idle_time;
#if 0
		if (dbs_tuners_ins.ignore_nice) {
			cputime64_t cur_nice;
			unsigned long cur_nice_jiffies;

			cur_nice = cputime64_sub(kstat_cpu(j).cpustat.nice,
					 j_dbs_info->prev_cpu_nice);
			/*
			 * Assumption: nice time between sampling periods will
			 * be less than 2^32 jiffies for 32 bit sys
			 */
			cur_nice_jiffies = (unsigned long)
					cputime64_to_jiffies64(cur_nice);

			j_dbs_info->prev_cpu_nice = kstat_cpu(j).cpustat.nice;
			idle_time += jiffies_to_usecs(cur_nice_jiffies);
		}
#endif
		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		load = 100 * (wall_time - idle_time) / wall_time;

		if (load > max_load)
			max_load = load;
	}

#if 0
	/*
	 * break out if we 'cannot' reduce the speed as the user might
	 * want freq_step to be zero
	 */
	if (dbs_tuners_ins.freq_step == 0)
		return;
#endif
	/* Check for frequency increase */
	if (max_load > dbs_tuners_ins.tdp_threshold) {

		/* if we are already at full speed then break out early */
#if 0		
		if (this_dbs_info->requested_freq == policy->max)
			return;
#endif
		/* if Turbo mode or Overclocking enabled and if load > (tdp_threshold+tdp_differential) 
		   then increase freq by freq step.
		*/    	
		if(!dbs_tuners_ins.turbo_mode && !dbs_tuners_ins.overclocking)
			return;
		if(max_load <  dbs_tuners_ins.tdp_threshold + dbs_tuners_ins.tdp_differential)			
			return;
		freq_target = (dbs_tuners_ins.freq_step * policy->max) / 100;

		/* max freq cannot be less than 100. But who knows.... */
		if (unlikely(freq_target == 0))
			freq_target = 5;
#if 0
		this_dbs_info->requested_freq += freq_target;
		if (this_dbs_info->requested_freq > policy->max)
			this_dbs_info->requested_freq = policy->max;
#endif
		freq_target += policy->cur;
		if(freq_target > policy->max)
			freq_target = policy->max;
#if 0
		__cpufreq_driver_target(policy, this_dbs_info->requested_freq,
			CPUFREQ_RELATION_H);
#endif
		__cpufreq_driver_target(policy, freq_target,
			CPUFREQ_RELATION_H);
		return;
	}

	/* Check for frequency decrease */
	/* if we cannot reduce the frequency anymore, break out early */
	if (policy->cur == policy->min)
		return;

	/*
	 * The optimal frequency is the frequency that is the lowest that
	 * can support the current CPU usage without triggering the up
	 * policy. To be safe, we focus 10 points under the threshold.
	 */
	if (max_load <
	    (dbs_tuners_ins.tdp_threshold - dbs_tuners_ins.down_differential)) {
		unsigned int freq_next;
		freq_next = (max_load * policy->cur) /
				(dbs_tuners_ins.tdp_threshold -
				 dbs_tuners_ins.down_differential);
		if (freq_next < policy->min)
			freq_next = policy->min;
		__cpufreq_driver_target(policy, freq_next,
				CPUFREQ_RELATION_L);
	}
}

static void do_dbs_timer(struct work_struct *work)
{
	struct cpu_dbs_info_s *dbs_info =
		container_of(work, struct cpu_dbs_info_s, work.work);
	unsigned int cpu = dbs_info->cpu;

	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);

	delay -= jiffies % delay;

	mutex_lock(&dbs_info->timer_mutex);

	dbs_check_cpu(dbs_info);

	queue_delayed_work_on(cpu, knc_wq, &dbs_info->work, delay);
	mutex_unlock(&dbs_info->timer_mutex);
}

static inline void dbs_timer_init(struct cpu_dbs_info_s *dbs_info)
{
	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);
	delay -= jiffies % delay;

	dbs_info->enable = 1;
	INIT_DELAYED_WORK_DEFERRABLE(&dbs_info->work, do_dbs_timer);
	queue_delayed_work_on(dbs_info->cpu, knc_wq, &dbs_info->work,
				delay);
}

static inline void dbs_timer_exit(struct cpu_dbs_info_s *dbs_info)
{
	dbs_info->enable = 0;
	cancel_delayed_work_sync(&dbs_info->work);
}

static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	unsigned int cpu = policy->cpu;
	struct cpu_dbs_info_s *this_dbs_info;
	unsigned int j;
	int rc;

	this_dbs_info = &per_cpu(cs_cpu_dbs_info, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

		mutex_lock(&dbs_mutex);
#if 0
		rc = sysfs_create_group(&policy->kobj, &dbs_attr_group_old);
		if (rc) {
			mutex_unlock(&dbs_mutex);
			return rc;
		}
#endif
		for_each_cpu(j, policy->cpus) {
			struct cpu_dbs_info_s *j_dbs_info;
			j_dbs_info = &per_cpu(cs_cpu_dbs_info, j);
			j_dbs_info->cur_policy = policy;

			j_dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&j_dbs_info->prev_cpu_wall);
			if (dbs_tuners_ins.ignore_nice) {
				j_dbs_info->prev_cpu_nice =
						kstat_cpu(j).cpustat.nice;
			}
		}
#if 0
		this_dbs_info->requested_freq = policy->cur;
#endif
		mutex_init(&this_dbs_info->timer_mutex);
		dbs_enable++;
		/*
		 * Start the timerschedule work, when this governor
		 * is used for first time
		 */
		if (dbs_enable == 1) {
			unsigned int latency;
			/* policy latency is in nS. Convert it to uS first */
			latency = policy->cpuinfo.transition_latency / 1000;
			if (latency == 0)
				latency = 1;

			rc = sysfs_create_group(cpufreq_global_kobject,
						&dbs_attr_group);
			if (rc) {
				mutex_unlock(&dbs_mutex);
				return rc;
			}

			/*
			 * conservative does not implement micro like ondemand
			 * governor, thus we are bound to jiffes/HZ
			 */
			min_sampling_rate =
				MIN_SAMPLING_RATE_RATIO * jiffies_to_usecs(10);
			/* Bring kernel and HW constraints together */
			min_sampling_rate = max(min_sampling_rate,
					MIN_LATENCY_MULTIPLIER * latency);
			dbs_tuners_ins.sampling_rate =
				max(min_sampling_rate,
				    latency * LATENCY_MULTIPLIER);
#if 0
			cpufreq_register_notifier(
					&dbs_cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
#endif					
		}
		mutex_unlock(&dbs_mutex);

		dbs_timer_init(this_dbs_info);

		break;

	case CPUFREQ_GOV_STOP:
		dbs_timer_exit(this_dbs_info);

		mutex_lock(&dbs_mutex);
#if 0
		sysfs_remove_group(&policy->kobj, &dbs_attr_group_old);
#endif
		dbs_enable--;
		mutex_destroy(&this_dbs_info->timer_mutex);

		/*
		 * Stop the timerschedule work, when this governor
		 * is used for first time
		 */
#if 0
		if (dbs_enable == 0)
			cpufreq_unregister_notifier(
					&dbs_cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
#endif
		mutex_unlock(&dbs_mutex);
		if (!dbs_enable)
			sysfs_remove_group(cpufreq_global_kobject,
					   &dbs_attr_group);

		break;

	case CPUFREQ_GOV_LIMITS:
		mutex_lock(&this_dbs_info->timer_mutex);
		if (policy->max < this_dbs_info->cur_policy->cur)
			__cpufreq_driver_target(
					this_dbs_info->cur_policy,
					policy->max, CPUFREQ_RELATION_H);
		else if (policy->min > this_dbs_info->cur_policy->cur)
			__cpufreq_driver_target(
					this_dbs_info->cur_policy,
					policy->min, CPUFREQ_RELATION_L);
		mutex_unlock(&this_dbs_info->timer_mutex);

		break;
	}
	return 0;
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_KNC
static
#endif
struct cpufreq_governor cpufreq_gov_knc = {
	.name			= "KnightsCorner",
	.governor		= cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

static int __init cpufreq_gov_dbs_init(void)
{
	int err;

	knc_wq = create_workqueue("knc");
	if (!knc_wq) {
		printk(KERN_ERR "Creation of knc work queue failed\n");
		return -EFAULT;
	}

	err = cpufreq_register_governor(&cpufreq_gov_knc);
	if (err)
		destroy_workqueue(knc_wq);

	return err;
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_knc);
	destroy_workqueue(knc_wq);
}


MODULE_AUTHOR("Ramesh Caushik <ramesh.caushik@intel.com>");
MODULE_DESCRIPTION("'cpufreq_governor_knc' - A dynamic cpufreq governor for "
		"the Intel MIC processors like KnightsCorner ");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_KNC
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
