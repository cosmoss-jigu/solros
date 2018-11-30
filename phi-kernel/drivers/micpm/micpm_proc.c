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
 * micpm_proc.c:  proc fs support for power management drivers 
 * 		  for Intel MIC devices.
 *
 * (C) Copyright 2008 Intel Corporation
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/cpuidle.h>
#include <linux/io.h>
#include <linux/irqflags.h>
#include <asm/pgtable.h>
#include <asm/proto.h>
#include <asm/mtrr.h>
#include <asm/page.h>
#include <asm/mce.h>
#include <asm/xcr.h>
#include <asm/io_apic.h>
#include <asm/suspend.h>
#include <asm/debugreg.h>
#include "mic_pm_card.h"
#include "micpm_common.h"
#include "mic_cpuidle.h"
#include "micidle_common.h"
#include "mic_event.h"
#include "pm_scif.h"
#include "mic_menu.h"

#define MAX_CPUS 248

#define MICPM_PROC_ENTRY
#ifdef MICPM_PROC_ENTRY
struct proc_dir_entry *micpm_proc;
extern unsigned int cc6entries[];
extern unsigned int pc3_select_count;
extern unsigned int pc3_entry_count;
extern unsigned int pc6_entry_count;
extern unsigned long cc6residency[];
extern unsigned long cc6latency[];
extern unsigned long pc3_ticks;
extern unsigned long pc6_ticks;
extern cpumask_t pc3readymask;
#ifdef PC3_POLICY_DEBUG
extern struct activity actrecs[MAX_CPUS];
#endif

#define fopsstruct(object) \
static int object##stats_show(struct seq_file *m, void *v); \
static int object##stats_open(struct inode *inode, struct file *filp) \
{ \
	return single_open(filp, object##stats_show, NULL); \
} \
static const struct file_operations object##stats_fops = { \
	.open		= object##stats_open, \
	.read		= seq_read, \
	.llseek		= seq_lseek, \
	.release	= single_release, \
};

#ifdef CONFIG_MIC_CPUIDLE
struct proc_dir_entry *micpm_cpuidle_proc;
fopsstruct(cc6);
fopsstruct(pc6);
fopsstruct(pc3);
#endif
static int cc6stats_show(struct seq_file *m, void *v)
{
	int i,freq;
	
	seq_printf(m,"\n\nCore C6 entries per core\n");
	for(i=1;i<nr_cpu_ids;i+=4){
		seq_printf(m,"%d,",cc6entries[i]);
	}
	seq_printf(m,"\n\n");

	freq=tsc_khz?(tsc_khz/1000):1000;
	
	seq_printf(m,"\n\nAverage Core C6 residency per cpu in usecs\n");
	for(i=0;i<nr_cpu_ids;i++){
		if(cc6entries[i])
			seq_printf(m,"%lu ",cc6residency[i]/(freq*cc6entries[i]));
		else
			seq_printf(m,"0 ");
	}		
	
	seq_printf(m,"\n\nAverage Core C6 latency per cpu in usecs\n");
	for(i=0;i<nr_cpu_ids;i++){
		if(cc6entries[i])
			seq_printf(m,"%lu ",cc6latency[i]/(freq*cc6entries[i]));
		else
			seq_printf(m,"0 ");
	}			
	return 0;
}	
static int pc3stats_show(struct seq_file *m, void *v)
{
	seq_printf(m,"\n\nPackage C3 stats\n");
	seq_printf(m,"<pc3 select count> <pc3 entry count> <pc3 residency(us)>\n");
	seq_printf(m,"	%d	%d	%lu\n",pc3_select_count,pc3_entry_count,
		pc3_entry_count ? pc3_ticks/pc3_entry_count:0);
	return 0;
}		
static int pc6stats_show(struct seq_file *m, void *v)
{
	#ifdef PC3_POLICY_DEBUG
	int cpu,i;
	#endif
	
	seq_printf(m,"\n\nPackage C6 stats\n");
	seq_printf(m,"<pc6 entry count> <pc6 residency(us)> <pc3readmask>\n");
	seq_printf(m,"	%d	%d	%lu       ",pc6_entry_count,
		pc6_entry_count ? pc6_ticks/pc6_entry_count:0);	
	seq_cpumask(m,&pc3readymask);
	seq_printf(m,"\n\n");
	#ifdef PC3_POLICY_DEBUG
	seq_printf(m,"Per-cpu activity\n");
	for_each_online_cpu(cpu) {
		if(actrecs[cpu].times) {
			seq_printf(m,"CPU %d activity. <pid> <name>  <time-delta from start of activity timer>\n",cpu);
			seq_printf(m,"Total of %d times in userland\n",actrecs[cpu].times);
			for(i=0;i<actrecs[cpu].times && i < MAX_ACTIVITY_RECORDS;i++){
				seq_printf(m,"pid = %d (%s) time = %ld\n",actrecs[cpu].actarr[i].pid,
						actrecs[cpu].actarr[i].name,actrecs[cpu].actarr[i].t_after);
			}
		}
	}
	#endif
	return 0;
}		
	 
void micpm_proc_init(int cc6, int pc3, int pc6, int cpufreq)
{
	struct proc_dir_entry *pentry;

	
	if((micpm_proc = create_proc_entry("micpm", S_IFDIR | S_IRUGO, NULL)) != NULL) {
#ifdef CONFIG_MIC_CPUIDLE	
		if(cc6 || pc3 || pc6){
			if((micpm_cpuidle_proc = create_proc_entry("cpuidle", S_IFDIR | S_IRUGO,
				micpm_proc))!= NULL) {
				if(cc6) {
					pentry = proc_create("corec6", 0644,micpm_cpuidle_proc,
					&cc6stats_fops);
					if(!pentry) 
					  printk(KERN_INFO "Failed to create cc6 proc entry\n");
				}
				if(pc3) {
					pentry = proc_create("packagec3", 0644,micpm_cpuidle_proc,
					&pc3stats_fops);
					if(!pentry)
					  printk(KERN_INFO "Failed to create pc3 proc entry\n");
				}
				if(pc6) {
					pentry = proc_create("packagec6", 0644,micpm_cpuidle_proc,
					&pc6stats_fops);
					if(!pentry)
					  printk(KERN_INFO "Failed to create pc6 proc entry\n");
				}
					
			}
		}
#endif	/* CONFIG_MIC_CPUIDLE */		
	/* Add code to create proc entries for cpufreq, and event data */	
	}
}			
			
void micpm_proc_exit(void)
{
#ifdef CONFIG_MIC_CPUIDLE	
	if(micpm_cpuidle_proc) {
		remove_proc_entry("corec6",micpm_cpuidle_proc);
		remove_proc_entry("packagec3",micpm_cpuidle_proc);
		remove_proc_entry("packagec6",micpm_cpuidle_proc);
	}
	if(micpm_proc) {
		remove_proc_entry("cpuidle",micpm_proc);
	}
#endif
	remove_proc_entry("micpm",NULL);
}				

#endif


