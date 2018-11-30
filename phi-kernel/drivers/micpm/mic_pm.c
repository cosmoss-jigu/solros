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
 * mic_pm.c - Power Management driver for the Intel MIC device
 *
 *   * (C) Copyright 2008 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>	
#include <linux/pm_qos_params.h>
#include <linux/clockchips.h>
#include <linux/cpuidle.h>
#include <linux/irqflags.h>
#include <linux/sysfs.h>
#include <linux/cpu.h>
#include "pm_scif.h"
#include "micras.h"
#include "micpm_proc.h"



#define MIC_PM_DEBUG

#ifdef	MIC_PM_DEBUG
#define dprintk(msg...) printk(KERN_INFO PREFIX msg);
#else
#define dprintk
#endif

#define SBOX_BASE	0x08007D0000ULL     /* PCIE Box Registers */
#define MIC_SBOX_MMIO_SIZE	(64*1024) // 64 KB = 2^16 // already defined ?


/* Command line parameters to micpm	*/
int mic_etc_enabled = 1;	/* Turned ON by default */
EXPORT_SYMBOL_GPL(mic_etc_enabled);

static int mic_cpufreq_driver=1;
static int mic_cpuidle_cc6=0;
static int mic_cpuidle_pc3=1;
static int mic_cpuidle_pc6=1;



extern void mic_pm_event_exit(void);
extern int mic_pm_event_init(int cpufreq, int corec6);
extern int  mic_cpufreq_init(void);
extern void mic_cpufreq_exit(void);

#ifdef	CONFIG_MIC_CPUIDLE
extern void mic_exit_menu(void);
extern int  mic_init_menu(int pc3_config);
extern void mic_cpuidle_exit(void);
extern int mic_cpuidle_init(int cc6,int pc3,int pc6);

extern unsigned int mic_cpuidle_get_pc3_select_count(void);
extern unsigned int mic_cpuidle_get_pc3_entered_count(void);
extern unsigned int mic_cpuidle_get_pc3_disable_count(void);
extern unsigned int mic_cpuidle_get_pc6_entered_count(void);
extern unsigned int mic_cpuidle_get_last_pkgstate_cpu(void);
extern ssize_t 	mic_menu_get_residency_data(char *buf);
extern ssize_t mic_cpuidle_get_cpuidle_data(char *buf);

/* sysfs controls to turn CC6 ON/OFF dynamically */
extern void mic_cpuidle_set_cc6policy(int policy);
extern unsigned int mic_cpuidle_get_cc6policy(void);

/* sysfs controls to turn PC3 ON/OFF through governor policy */
extern void mic_menu_set_pc3_inactivity(int time);	/*Time in secs.	*/
extern unsigned int mic_menu_get_pc3_inactivity(void);
extern void mic_menu_set_pc3policy(int policy);
extern unsigned int mic_menu_get_pc3policy(void);
#endif
extern unsigned int mic_event_get_dietemp(void);
extern int mic_event_get_turboavail(void);    
extern unsigned int mic_event_get_turbostate(void);
#ifdef	USE_SW_THRESHOLDS
extern unsigned int mic_event_get_swthreshold1(void);
extern unsigned int mic_event_get_swthreshold2(void);
#endif
#ifdef MIC_EVENT_SYSFS_DEBUG
extern unsigned int mic_event_debug_get_prochot(void);
extern void mic_event_debug_set_ctrl(int state);
extern unsigned int  mic_event_debug_get_ctrl(void);
extern void mic_event_debug_set_thresh1(int ctrl);
extern unsigned int mic_event_debug_get_thresh1(void);
extern void mic_event_debug_set_thresh2(int ctrl);
extern unsigned int mic_event_debug_get_thresh2(void);
extern void mic_event_debug_set_turbopt(int point);
extern int mic_event_debug_get_turbopt(void);
extern void mic_event_debug_set_pwralrt(int ctrl);
extern int mic_event_debug_get_pwralrt(void);
extern void mic_event_debug_set_prochot(int prochot);
#endif

extern void mic_event_set_turbostate(int state);
#ifdef	USE_SW_THRESHOLDS
extern void mic_event_set_swthreshold1(int thresh);
extern void mic_event_set_swthreshold2(int thresh);
#endif
/* This actually sets the percent of TDP that we throttle to 
   when we get a PWRALRT event.
*/     
extern void mic_event_set_pwralertlimit(int freq);
extern unsigned int mic_event_get_pwralertlimit(void); 		

static void ras_setturbostate(int state);
static int ras_getturbostate(void);
static int ras_getpmcfg(void);

extern void *mic_sbox_mmio_va;


/* This is the main file for the PM driver for the Intel
*  MIC devices. This glues together other functional 
*  elements of the PM driver such as the cpuidle driver
*  the cpuidle governor and the eventhandler.
*/
#define PREFIX	"mic_pm: "
struct kobject *micpm_global_kobject;  

struct mic_pmscif_handle *pmscif;
int pmscif_registered;

static struct micpm_params * ras_parms;

int cpuidle_inited,cpufreq_inited;    
	    
#define define_one_ro(_name)	\
static struct global_attr _name =	\
__ATTR(_name, 0444, show_##_name, NULL)

#define define_one_rw(_name) \
static struct global_attr _name = \
__ATTR(_name, 0644, show_##_name, store_##_name)

#define show_one(file_name, module)				\
static ssize_t show_##file_name				\
(struct kobject *kobj, struct attribute *attr, char *buf)              	\
{							\
	return sprintf(buf, "%u\n", module##_get_##file_name());	\
}

#define store_one(file_name, module)				\
static ssize_t store_##file_name				\
(struct kobject *kobj, struct attribute *attr, const char *buf,size_t count)	\
{							\
	unsigned int input; 					\
	int ret; 						\
	ret = sscanf(buf, "%u", &input);			\
	module##_set_##file_name(input);			\
	return count;					\
}

#ifdef	CONFIG_MIC_CPUIDLE
show_one(pc3_select_count,mic_cpuidle);
show_one(pc3_entered_count, mic_cpuidle);
show_one(pc3_disable_count, mic_cpuidle);
show_one(pc6_entered_count, mic_cpuidle);

define_one_ro(pc3_select_count);
define_one_ro(pc3_entered_count);
define_one_ro(pc3_disable_count);
define_one_ro(pc6_entered_count);



static struct attribute *pkgstate_attributes[] = {
	&pc3_select_count.attr,
	&pc3_entered_count.attr,
	&pc3_disable_count.attr,
	&pc6_entered_count.attr,
	NULL
};

static struct attribute_group pkgstate_attr_group = {
	.attrs = pkgstate_attributes,
	.name = "pkgstate",
};


#define show_one_debug_data(file_name,module)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)              \
{									\
	return (module##_get_##file_name(buf));		\
}


show_one(cc6policy,mic_cpuidle);
store_one(cc6policy,mic_cpuidle);
define_one_rw(cc6policy);		/* Choose cc6 through governor policy */

/* PC3 by policy stuff */
show_one(pc3policy,mic_menu);
store_one(pc3policy,mic_menu);
define_one_rw(pc3policy);	
					   
show_one(pc3_inactivity,mic_menu);
store_one(pc3_inactivity,mic_menu);
define_one_rw(pc3_inactivity);		/* Number of secs of inactivity (no user time)on the 
					   cpu before the governor decides to choose PC6 for 
					   the cpu idle state.
					*/

show_one_debug_data(cpuidle_data,mic_cpuidle);
define_one_ro(cpuidle_data);

#define show_one_force_state(file_name, module)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)              \
{									\
	return sprintf(buf, "%u\n", module##_get_forced_state());		\
}


#define store_one_force_state(file_name, module)					\
static ssize_t store_##file_name						\
(struct kobject *kobj, struct attribute *attr, const char *buf,size_t count)              \
{									\
	unsigned int input; \
	int ret; \
	ret = sscanf(buf, "%u", &input); \
	if (ret != 1) \
		return -EINVAL; \
	if(input > 4) return -EINVAL; 	\
	module##_set_forced_state(input);		\
	return count;	\
}

static struct attribute *mic_pm_debug_attributes[] = {
	&cpuidle_data.attr,
	&cc6policy.attr,
	&pc3_inactivity.attr,
	&pc3policy.attr,
	NULL
};

static struct attribute_group micdebug_attr_group = {
	.attrs = mic_pm_debug_attributes,
	.name = "debug",
};

#endif //CONFIG_MIC_CPUIDLE


/* mic event config defines */

show_one(turbostate,mic_event);
show_one(turboavail,mic_event);
#ifdef	USE_SW_THRESHOLDS
show_one(swthreshold1,mic_event);
show_one(swthreshold2,mic_event);
#endif
show_one(dietemp,mic_event);
show_one(pwralertlimit,mic_event);

store_one(turbostate,mic_event);
#ifdef	USE_SW_THRESHOLDS
store_one(swthreshold1,mic_event);
store_one(swthreshold2,mic_event);
#endif
store_one(pwralertlimit,mic_event);


define_one_ro(turboavail);
define_one_rw(turbostate);
#ifdef	USE_SW_THRESHOLDS
define_one_rw(swthreshold1);
define_one_rw(swthreshold2);
#endif
define_one_ro(dietemp);
define_one_rw(pwralertlimit);


static struct attribute *eventconfig_attributes[] = {
	&turboavail.attr,
	&turbostate.attr,
	#ifdef	USE_SW_THRESHOLDS
	&swthreshold1.attr,
	&swthreshold2.attr,
	#endif
	&dietemp.attr,
	&pwralertlimit.attr,	
	NULL
};
#ifdef MIC_EVENT_SYSFS_DEBUG
/* mic event debug defines */
show_one(ctrl,mic_event_debug);
show_one(prochot,mic_event_debug);
show_one(thresh1,mic_event_debug);
show_one(thresh2,mic_event_debug);
show_one(turbopt,mic_event_debug);
show_one(pwralrt,mic_event_debug);

store_one(ctrl,mic_event_debug);
store_one(prochot,mic_event_debug);
store_one(thresh1,mic_event_debug);
store_one(thresh2,mic_event_debug);
store_one(turbopt,mic_event_debug);
store_one(pwralrt,mic_event_debug);

define_one_rw(ctrl);
define_one_rw(prochot);
define_one_rw(thresh1);
define_one_rw(thresh2);
define_one_rw(turbopt);
define_one_rw(pwralrt);

static struct attribute *eventdebug_attributes[] = {
	&ctrl.attr,
	&prochot.attr,
	&thresh1.attr,	
	&thresh2.attr,
	&pwralrt.attr,
	&turbopt.attr,
	NULL
};
#endif

static struct attribute_group eventconfig_attr_group = {
	.attrs = eventconfig_attributes,
	.name = "evconfig",
};

#ifdef MIC_EVENT_SYSFS_DEBUG
static struct attribute_group eventdebug_attr_group = {
	.attrs = eventdebug_attributes,
	.name = "evdbg",
};    
#endif

static int micpm_sysfs_init(void)
{
	int rc = 0;

#ifdef	CONFIG_MIC_CPUIDLE
	if(mic_cpuidle_pc3) {	 
		rc = sysfs_create_group(micpm_global_kobject,
					&pkgstate_attr_group);
		if (rc) {
				printk(KERN_INFO PREFIX"Failed to create pkgstate sysfs entry\n");
				return rc;
		}
	}	
	if(mic_cpuidle_pc3 || mic_cpuidle_cc6) {
	
		rc = sysfs_create_group(micpm_global_kobject,
					&micdebug_attr_group);
		if (rc) {
				printk(KERN_INFO PREFIX"Failed to create debug sysfs entry\n");
		}
	}
#endif
	rc = sysfs_create_group(micpm_global_kobject,
				&eventconfig_attr_group);
	if (rc) {
			printk(KERN_INFO PREFIX"Failed to create event config sysfs entry\n");
	}
	#ifdef MIC_EVENT_SYSFS_DEBUG
	rc = sysfs_create_group(micpm_global_kobject,
				&eventdebug_attr_group);
	if (rc) {
			printk(KERN_INFO PREFIX"Failed to create event debug sysfs entry\n");
	}
	#endif			
	return rc;
}        

  
static int __init mic_pm_init(void)
{    
      
	int result = 0;
	
	if(!mic_sbox_mmio_va) {
		mic_sbox_mmio_va = ioremap(SBOX_BASE, MIC_SBOX_MMIO_SIZE);
		if(!mic_sbox_mmio_va) {
			printk(KERN_ERR"Failed to map SBOX MMIO space into kernel");
			return  -EINVAL;
		}
	}		
	if((result = mic_pm_event_init(mic_cpufreq_driver,mic_cpuidle_cc6))) {
		printk(KERN_DEBUG PREFIX"Failed to init pm events\n");
	}
	
	#ifdef	CONFIG_MIC_CPUFREQ
	if(mic_cpufreq_driver) {
		result = mic_cpufreq_init();
		if(result) {
			printk(KERN_DEBUG PREFIX"Failed to init MIC cpufreq driver\n");
			return result;
		}
		cpufreq_inited=1;
	}
	#endif	
		
	pmscif=NULL;
	
	#ifdef	CONFIG_MIC_CPUIDLE
	if(mic_cpuidle_cc6 || mic_cpuidle_pc3) {	 
		result = mic_init_menu(mic_cpuidle_pc3);
		if(result) {
			printk(KERN_DEBUG PREFIX"Failed to init cpuidle governor\n");
			return result;
		}
		result = mic_cpuidle_init(mic_cpuidle_cc6,mic_cpuidle_pc3,mic_cpuidle_pc6);
		if(result) {
			printk(KERN_DEBUG PREFIX"Failed to init cpuidle device\n");
			mic_exit_menu();
			return result;
		}
		cpuidle_inited=1;
		
	#endif
	}
	micpm_global_kobject = kobject_create_and_add("micpm",
					&cpu_sysdev_class.kset.kobj);
	BUG_ON(!micpm_global_kobject);
	micpm_sysfs_init();					
	micpm_proc_init(mic_cpuidle_cc6,mic_cpuidle_pc3,mic_cpuidle_pc6,mic_cpufreq_driver);
	return result;
}	    
  
static void __init mic_pm_exit(void)
{

	#ifdef	CONFIG_MIC_CPUIDLE
	if (cpuidle_inited) {
		mic_exit_menu();
		mic_cpuidle_exit();
	}
	#endif
	#ifdef	CONFIG_MIC_CPUFREQ
	if(cpufreq_inited)
		mic_cpufreq_exit();
	#endif
	mic_pm_event_exit();
	micpm_proc_exit();	
}	 	  
static BLOCKING_NOTIFIER_HEAD(pm_scif_notifier);

static ATOMIC_NOTIFIER_HEAD(micpm_device_notifier);


void pm_scif_notifier_register(struct notifier_block *n)
{
	blocking_notifier_chain_register(&pm_scif_notifier, n);
}
EXPORT_SYMBOL(pm_scif_notifier_register);

void pm_scif_notifier_unregister(struct notifier_block *n)
{
	blocking_notifier_chain_unregister(&pm_scif_notifier, n);
}
EXPORT_SYMBOL_GPL(pm_scif_notifier_unregister);


void pm_scif_recv(pm_msg_header *header, void *msg)
{
	pm_msg_pm_options payload;
	/* HACK: If the received message is a test message, loop it back
	 * to the host. Otherwise call the notifier chain
	 */
	if(!pmscif_registered || !pmscif || !try_module_get(pmscif->owner))  
		return;
	if(header) {
		if(header->opcode == PM_MESSAGE_TEST) {
			pmscif->pm_scif_uos2host(header->opcode, msg, header->len);
		} else	{
			if(header->opcode==PM_MESSAGE_OPEN) {
				payload.pc3_enabled=mic_cpuidle_pc3;
				payload.pc6_enabled=mic_cpuidle_pc6;
				payload.version.major_version = PM_MAJOR_VERSION;
				payload.version.minor_version = PM_MINOR_VERSION;
				pmscif->pm_scif_uos2host(PM_MESSAGE_OPEN_ACK,&payload, sizeof(payload));
			}
			blocking_notifier_call_chain(&pm_scif_notifier, header->opcode, msg);
		}
	}
	module_put(pmscif->owner);
}	

int pm_scif_send2host(PM_MESSAGE opcode, void *msg, size_t len)
{
	int ret;
	
	if(!pmscif_registered || !pmscif || !try_module_get(pmscif->owner))
		return -1;
	ret = pmscif->pm_scif_uos2host(opcode,msg,len);
	module_put(pmscif->owner);
	return ret;
}		

void pm_scif_unregister(struct mic_pmscif_handle *ppmscif)
{
	pmscif_registered=0;
}
EXPORT_SYMBOL_GPL(pm_scif_unregister);

void micpm_device_register(struct notifier_block *n)
{
	atomic_notifier_chain_register(&micpm_device_notifier, n);
}
EXPORT_SYMBOL_GPL(micpm_device_register);

void micpm_device_unregister(struct notifier_block *n)
{
	atomic_notifier_chain_unregister(&micpm_device_notifier, n);
}
EXPORT_SYMBOL_GPL(micpm_device_unregister);

int micpm_device_notify(unsigned long event,void *msg)
{
	return(atomic_notifier_call_chain(&micpm_device_notifier,event,msg));
}

int pm_scif_register(struct mic_pmscif_handle *ppmscif)
{
	pmscif = ppmscif;	
	if(pmscif) { 
		pmscif->pm_scif_host2uos = pm_scif_recv;
		smp_wmb();
		pmscif_registered = 1;
	}
	return 0;
} 
EXPORT_SYMBOL_GPL(pm_scif_register);

static int (*ras_getfreqtable)(struct micpm_params *);

void ras_ttl(int who, int what)
{
	if (ras_parms) {
		printk("ras_ttl: Calling mt_ttl with %d:%d\n",who,what);
		ras_parms->mt_ttl(who, what);
	}
}

int
micpm_ras_register(struct micpm_callbacks *cb,
		   struct micpm_params *mp)
{
	extern void ras_preset(void);
	int	i;

	/*
	 * Exchange pointers with RAS module
	 */
	if (cb) {
		cb->micpm_get_turbo = ras_getturbostate;
		cb->micpm_set_turbo = ras_setturbostate;
		cb->micpm_vf_refresh = 0;	/* Not supported at this time */
		cb->micpm_get_pmcfg = ras_getpmcfg;	
	}
	ras_parms = mp;

	/*
	 * Fill in Core Voltage and Core Frequency tables.
	 * Separate function if tables can change.
	 */
	if (mp && ras_getfreqtable){
		ras_getfreqtable(ras_parms);
		dprintk("micpm: Freq/volt table returned to RAS\n");
		dprintk("index       freq       voltage\n");
		for(i=0;i < *(ras_parms->volt_len);i++) {
			dprintk("%d      %d       %d\n",i,ras_parms->freq_lst[i],ras_parms->volt_lst[i]);
		}
	}
	printk("micpm: RAS module registered\n");

	/*
	 *KAA: if throttle is active right now, then tell RAS module
	 */
	ras_preset();

	return 0;
}
EXPORT_SYMBOL_GPL(micpm_ras_register);

void
micpm_ras_unregister(void)
{
	ras_parms = 0;
	printk("micpm: RAS module unregistered\n");	  
}
EXPORT_SYMBOL_GPL(micpm_ras_unregister);

/**
* micpm_cpufreq_register: Register function for the mic_cpufreq driver
* to register the function that fills in the freq/voltage information
* that the RAS module needs.
*@getmicfreqvolt - function pointer inside the mic_cpufreq driver.
**/
void micpm_cpufreq_register(int (*getmicfreqvolt)(struct micpm_params *))
{
	ras_getfreqtable = getmicfreqvolt;
}
EXPORT_SYMBOL_GPL(micpm_cpufreq_register);

static void ras_setturbostate(int state)
{
	mic_event_set_turbostate(state);
}		

static int ras_getturbostate(void)
{
	unsigned int val=0;
	
	if(mic_event_get_turboavail()){
		val |= MR_PM_AVAIL;
		if(mic_event_get_turbostate())
			val |= MR_PM_STATE;
	}		
	return val;
}
static int ras_getpmcfg(void)
{
	return((mic_cpufreq_driver <<  PMCFG_PSTATES_BIT)| 
		(mic_cpuidle_cc6 << PMCFG_COREC6_BIT)|
		(mic_cpuidle_pc3 << PMCFG_PC3_BIT)|
		(mic_cpuidle_pc6 << PMCFG_PC6_BIT)
		);
}
static int __init set_cmdline_micpm(char *str)
{
	
	printk("micpm: setup string passed in is %s\n",str);
	if(strstr(str,"etc_off"))
		mic_etc_enabled=0;
	if(strstr(str,"cpufreq_off"))
		mic_cpufreq_driver=0;
	if(strstr(str,"corec6_on"))
		mic_cpuidle_cc6=1;
	if(strstr(str,"pc3_off"))
		mic_cpuidle_pc3=0;
	if(strstr(str,"pc6_off"))
		mic_cpuidle_pc6=0;
	printk(KERN_INFO"micpm config setup: etc=%d cpufreq=%d, corec6=%d pc3=%d pc6 = %d\n",
			 mic_etc_enabled,mic_cpufreq_driver,mic_cpuidle_cc6,mic_cpuidle_pc3,
			 mic_cpuidle_pc6);
	return 1;
};
__setup("micpm=",set_cmdline_micpm);


MODULE_LICENSE("GPL");
module_init(mic_pm_init);
module_exit(mic_pm_exit);
