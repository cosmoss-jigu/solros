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
 * mic_tmu.c - Event handler (mostly from the Thermal Monitoring Unit)
 * for MIC cpu devices .
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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/cpufreq.h>
#include "micpm_common.h"
#include "mic_event.h"

#define PREFIX "mic_event: "

#define MIC_EVENT_DEBUG

#ifdef	MIC_EVENT_DEBUG
#ifdef CONFIG_GCOV_KERNEL
#define dprintk(msg...) printk(KERN_INFO PREFIX msg);
#else
#define dprintk(msg...) pr_debug(PREFIX msg);
#endif
#else
#define dprintk 
#endif


#define	K1OM_THERMAL_STATUS_INTERRUPT_MASK  0xaa   /* Mask includes ALERT bit */
#define K1OM_THERMAL_STATUS_INTERRUPT_CLEAR 0xa2   /* This clears log bits for
							MCLK,PROCHOT,PWRALRT.
						   */
#define	K1OM_THERMAL_STATUS_CLEAR	0x50
#define	K1OM_THERMAL_DISABLE_INTR	0x7feffc00 /* Disables all thermal interrupts	*/
#define	K1OM_TMU_INTERRUPT		13
#define K1OM_ENABLE_MCLK_INTR           0x20

#define	K1OM_THERMAL_THRESHOLD_1   	95
#define	K1OM_THERMAL_THRESHOLD_2	98
#define MIC_EVENT_DELAY			50	//in msecs.
#define SW_THRESHOLD2_FREQ		90	//percent of the TDP freq we throttle to 
						//when sw thresh2 is exceeded.
#define SW_THRESHOLD1_FREQ		100
#define SW_THRESHOLD1_MASK		0x00000010U		
#define SW_THRESHOLD2_MASK		0x00000080U		
#define PROCHOT_MASK			0x00000010U
#define SW_THRESHOLD1_TEMP_MASK		0x001FFC00U /* Bits 10 to 20 */
#define SW_THRESHOLD2_TEMP_MASK		0xFFE00000U
#define SW_THRESHOLDS_MASK		0xFFFFFC00U /* Mask of sw thresholds temp and enable bits */
#define PWRLART_MASK			0x00000040U

#define PWRALRT_THROTTLE_FREQ		95 	/* Percent of TDP we throttle to when we hit a 
						   PWRALRT event. 95% of the TDP freq should fall
						   between P1 and P2 on all SKUs and hence the 
						   governor would end up choosing P2
						*/    							 
#define PROCHOT_VID_DELAY		2	/* Delay this many cycles before lowering vid */

static struct delayed_work micevent_work;
static struct workqueue_struct *micevent_wq;

static int 	pstate;
static int 	pstatedriver;

#ifdef	USE_SW_THRESHOLDS	/* Only if we are using SW thermal
					thresholds.
				*/
//sw thermal threshold events crossed going up
static atomic_t	swthresh1 = ATOMIC_INIT(0);
static atomic_t	swthresh2 = ATOMIC_INIT(0);
#endif
static int 	turbostate;	//Instantaneous state of turbo. 
static atomic_t inProcHot = ATOMIC_INIT(0); //If there is a PROCHOT condition in progress.
static int 	inPwrAlert;	//If a Power Alert condition is in progress.
static int	PwrAlertLimit; 	// Percent of TDP to throttle to on power limit event.
				// Defaults to PWRALRT_THROTTLE_FREQ.
				
static unsigned long procHotRecoveryFreq; //Core freq we want to return to after a procHot condition
static unsigned long procHotCoreFreq;	//Core freq the h/w sets on a prochot condition
static int prochotviddelay;		//delay this many loops of the worker thread before 
//we bring vid lower; CCB 104167.  
static unsigned long mic_event_delay;
#ifdef	USE_SW_THRESHOLDS
static int thresh2throttled;
static int thresh1throttled;
#endif
#ifdef PARTIAL_CORE_TURBO
static int turbop_req;		//Requested turbo point
static int turbop_curr;		//Current turbo point
static spinlock_t teventlock;	//Turbo event lock
#endif
static spinlock_t miceventlock;
static spinlock_t tmu_lock;
static int ProcHotAsserted;
static int maxfreq;		//At any time this is the maximum core freq (kHz) 
//enforced in the policy
static int tinited;

//turbo enable or disable from CP or sysfs, or during init.
static int turboctrl;
struct  mic_cpufreq_object freqobj;	

#ifdef MIC_EVENT_SYSFS_DEBUG
/* sysfs based event debug	*/
static int mic_event_debug; /* Set this to turn ON event debug through sysfs */ 
static unsigned int dbgreg_thstintr; /* software thermal status interrupt
					register 
					*/
static unsigned int dbgreg_thst; /* software thermal status register */
#endif
enum event_type {EVENT_OFF,EVENT_ON,EVENT_NOTHING};

static void mic_event_handler(struct work_struct *work);
void micpm_turboctrl(int state);
static void toggle_tmu_reg(int regoffset, uint32_t mask, uint32_t val);
static ATOMIC_NOTIFIER_HEAD(micpm_atomic_event_notifier);
static BLOCKING_NOTIFIER_HEAD(micpm_event_notifier);

void micpm_atomic_notifier_register(struct notifier_block *n)
{
	atomic_notifier_chain_register(&micpm_atomic_event_notifier, n);
}
EXPORT_SYMBOL(micpm_atomic_notifier_register);

void micpm_atomic_notifier_unregister(struct notifier_block *n)
{
	atomic_notifier_chain_unregister(&micpm_atomic_event_notifier, n);
}
EXPORT_SYMBOL_GPL(micpm_atomic_notifier_unregister);

void micpm_notifier_register(struct notifier_block *n)
{
	blocking_notifier_chain_register(&micpm_event_notifier, n);
}
EXPORT_SYMBOL(micpm_notifier_register);

void micpm_notifier_unregister(struct notifier_block *n)
{
	blocking_notifier_chain_unregister(&micpm_event_notifier, n);
}
EXPORT_SYMBOL_GPL(micpm_notifier_unregister);

int micevent_notify(int event, void *v,enum notifier_type type)
{
	if(type == BLOCKING) 
		return(blocking_notifier_call_chain(&micpm_event_notifier, event, v));
	else
		return(atomic_notifier_call_chain(&micpm_atomic_event_notifier, event, v));
}
EXPORT_SYMBOL_GPL(micevent_notify);

static void  GoalRatioReachedEvent(void){
	micevent_notify(EVENT_MCLK,NULL,ATOMIC);
}

static void ProcHotEvent(int event, void *v)
{
	/* HW automatically switches core freq to min value. */
	enum notifier_type etype;
	char *msg[3]={"OFF","ON","VIDCHANGE"};
	int i=2;

	etype=(event == EVENT_PROCHOT_ON)?ATOMIC:BLOCKING;
	micevent_notify(event,v,etype);
	if(event == EVENT_PROCHOT_OFF)
		i=0;
	else if(event == EVENT_PROCHOT_ON)
		i=1;
	dprintk("ProcHot %s event\n",msg[i]);		
		
}	
static void PwrAlertEvent(enum event_type event)
{
	char *msg[2]={"OFF","ON"};
	
	if(event >= EVENT_NOTHING) return;

	if(event == EVENT_OFF)
		micevent_notify(EVENT_PWR_ALERT_OFF,NULL,ATOMIC);
	else {
		micevent_notify(EVENT_PWR_ALERT_ON,NULL,ATOMIC);
	}
	
	dprintk("PowerAlert %s event\n",msg[event]);  

}
static inline unsigned int set_max_freq(void)
{
	if(turbostate)
		return(freqobj.tinfo.turbopts[0].freq);
	return freqobj.tdpfreq;
}		
static inline void enable_interrupts(void)
{
	sboxThermalInterruptEnableReg	intrreg; 	

	intrreg.value = 0;
	intrreg.bits.mclk_ratio_interrupt_enable = 0;
	intrreg.bits.gpuhot_interrupt_enable = 1;
	intrreg.bits.pwralert_interrupt_enable=1;
	toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,intrreg.value,intrreg.value);
}	 	
static inline void tmu_toggle_mclk_interrupt(int  state)
{
	sboxThermalInterruptEnableReg	intrreg; 	

	intrreg.value = 0;
	intrreg.bits.mclk_ratio_interrupt_enable = 1;
	if(state)
		toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,intrreg.value,intrreg.value);
	else
		toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,intrreg.value,0);
}

static inline void tmu_toggle_alert_interrupt(int state)
{
	sboxThermalInterruptEnableReg	intrreg; 	

	intrreg.value = 0;
	intrreg.bits.alert_interrupt_enable = 1;
	if(state)
		toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,intrreg.value,intrreg.value);
	else
		toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,intrreg.value,0);
}	

void tmu_toggle_prochot_interrupt(int state)
{
	sboxThermalInterruptEnableReg	intrreg; 	

	intrreg.value = 0;
	intrreg.bits.gpuhot_interrupt_enable = 1;
	if(state)
		toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,intrreg.value,intrreg.value);
	else
		toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,intrreg.value,0);
}
EXPORT_SYMBOL_GPL(tmu_toggle_prochot_interrupt);

void tmu_toggle_pwralert_interrupt(int state)
{
	sboxThermalInterruptEnableReg	intrreg; 	

	intrreg.value = 0;
	intrreg.bits.pwralert_interrupt_enable = 1;
	if(state)
		toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,intrreg.value,intrreg.value);
	else
		toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,intrreg.value,0);
}
EXPORT_SYMBOL_GPL(tmu_toggle_pwralert_interrupt);

#ifdef	USE_SW_THRESHOLDS
static inline void mic_set_swthresholds(void) 
{
	sboxThermalInterruptEnableReg	reg;
	sboxGpuHotConfigReg	hreg;

	reg.value=0;
	reg.bits.sw_threshold1_temp = K1OM_THERMAL_THRESHOLD_1;
	reg.bits.sw_threshold2_temp = K1OM_THERMAL_THRESHOLD_2;
	reg.bits.sw_threshold1_enable = 1;
	reg.bits.sw_threshold2_enable = 1;
	toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,SW_THRESHOLDS_MASK,reg.value);

	hreg.value=0;
	hreg.bits.xxgpuhot_enable = 1;
	toggle_tmu_reg(SBOX_GPU_HOT_CONFIG,hreg.value,hreg.value);
}
#endif

static inline void clear_thermal_status(void)
{
	toggle_tmu_reg(SBOX_THERMAL_STATUS_INTERRUPT,K1OM_THERMAL_STATUS_INTERRUPT_MASK,K1OM_THERMAL_STATUS_INTERRUPT_CLEAR);
	toggle_tmu_reg(SBOX_THERMAL_STATUS,K1OM_THERMAL_STATUS_CLEAR,K1OM_THERMAL_STATUS_CLEAR);
}

/*
 * RAS module just registered,
 * Set it's initial throttle state.
 * Updates from then will happen through the notification chains.
 */
void ras_preset(void)
{
	extern void ras_ttl(int, int);

	if (atomic_read(&inProcHot))
		ras_ttl(0, 1);	/* Power ttl -> on */ 
	if (inPwrAlert)
		ras_ttl(1, 1);	/* Thermal ttl -> on */ 
}
static void raise_pwralert_event(void)
{
	unsigned long flags;
	
	spin_lock_irqsave(&miceventlock,flags);
	if(!inPwrAlert) {
		printk(KERN_INFO"Power alert interrupt");
		inPwrAlert=1;
		turbostate = 0;
		PwrAlertEvent(EVENT_ON);
	}
	spin_unlock_irqrestore(&miceventlock,flags);
}


/*
 * Called from tmu_interrupt_handler as well as 
 * when the cpufreq driver registers with the event
 * module. 
 */
static inline void raise_prochot_event(void)
{

	unsigned long flags;
	
	/* Detect the condition where we get back-2-back prochot interrupts 
	   before the background thread ever getting a chance to finish 
	   processing the previous de-assertion of prochot signal
	*/  		    
	spin_lock_irqsave(&miceventlock,flags);
	if(!ProcHotAsserted) {
		ProcHotAsserted=1;
		spin_unlock_irqrestore(&miceventlock,flags);
		printk(KERN_INFO"PROCHOT interrupt\n");
		if(atomic_inc_return(&inProcHot)== 1) {
			turbostate = 0;
			ProcHotEvent(EVENT_PROCHOT_ON,NULL);
			prochotviddelay=0;
		}
	}
	else
		spin_unlock_irqrestore(&miceventlock,flags);
}
static irqreturn_t tmu_interrupt_handler(int irq, void *data)
{
	sboxThermalStatusInterruptReg	statusreg;
	sboxThermalStatusReg		thermalreg;
	unsigned long			flags;

	if(!mic_sbox_mmio_va)
		return IRQ_NONE;
	MIC_READ_MMIO_REG(statusreg,SBOX_THERMAL_STATUS_INTERRUPT);
	MIC_READ_MMIO_REG(thermalreg,SBOX_THERMAL_STATUS);	

	//	dprintk("TMU interrupt handler statusreg = %x thermalreg = %x\n",statusreg.value,thermalreg.value);	
	/* proc hot status remains asserted till condition goes away. Log bit 
	   is asserted on the rising edge. So use log bit to detect prochot
	   interrupt.
	   */ 
	if(statusreg.bits.gpuhot_log) {
		raise_prochot_event();
	}
	if(statusreg.bits.pwralert_status) {
		raise_pwralert_event();
	}
	if(statusreg.bits.mclk_ratio_log) {
		//		dprintk("MCLK ratio change interrupt\n");
		GoalRatioReachedEvent();
	}
#ifdef	USE_SW_THRESHOLDS
	/* Make sure we got this interrupt due to a threshold event(log bit)
	   and the event is a upward crossing
	   */     
	if(thermalreg.bits.thermal_threshold2_log && 
			thermalreg.bits.thermal_threshold2_status){

		dprintk("SW thermal threshold 2 interrupt\n");
		atomic_inc(&swthresh2);
		turbostate = 0;
	}
	if(thermalreg.bits.thermal_threshold1_log &&
			thermalreg.bits.thermal_threshold1_status){

		dprintk("SW thermal threshold 1 interrupt\n");
		atomic_inc(&swthresh1);
		turbostate = 0;
	}
#endif
	clear_thermal_status();
	MIC_READ_MMIO_REG(statusreg,SBOX_THERMAL_STATUS_INTERRUPT);
	//	dprintk("Status reg on intr handler exit = %x\n",statusreg.value);
	return IRQ_HANDLED;
}

static inline unsigned int mic_read_dietemp(int sensor) 
{
	sboxCurrentDieTemp0Reg	dietemp;
	U32 value;

	MIC_READ_MMIO_REG(dietemp,SBOX_CURRENT_DIE_TEMP0);
	switch(sensor) {
		case 0:		
			value = dietemp.bits.sensor0_temp;
			break;
		case 1:
			value = dietemp.bits.sensor1_temp;
			break;
		case 2:
			value = dietemp.bits.sensor2_temp;
			break;
		default:
			value = 0;		
	}		
	return value;
}

static inline int mic_pmevent_init(void) 
{
	/* Initialize TMU Interrupt and status registers	*/
	int i;


	toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,~K1OM_THERMAL_DISABLE_INTR,0);
	clear_thermal_status();

	for(i=0;i<3;i++) {
		dprintk("Dietemp %d = %x \n",i,mic_read_dietemp(i));
	}
	/* Setup interrupt handler	*/
	if(request_irq(K1OM_TMU_INTERRUPT, tmu_interrupt_handler, IRQF_DISABLED, "TMU",NULL )) {
		printk("Failed to set up TMU interrupt handler");
		return 1;
	}
	return 0;			
}

static inline void mic_pmevent_uninit(void)
{
	/* Clear TMU Interrupt and status registers	*/

	toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,~K1OM_THERMAL_DISABLE_INTR,0);
	clear_thermal_status();

	free_irq(K1OM_TMU_INTERRUPT,NULL);
}	

static inline unsigned int getstatusreg(int reg)
{
	sboxThermalStatusInterruptReg	statusreg;
	unsigned int val = 0;

	switch(reg) {
		case SBOX_THERMAL_STATUS_INTERRUPT:
#ifdef MIC_EVENT_SYSFS_DEBUG
			val = dbgreg_thstintr;
			if(!mic_event_debug) {
#endif
				MIC_READ_MMIO_REG(statusreg,SBOX_THERMAL_STATUS_INTERRUPT);
				val = statusreg.value;
#ifdef MIC_EVENT_SYSFS_DEBUG
			}
#endif		
			break;
		case SBOX_THERMAL_STATUS:
#ifdef MIC_EVENT_SYSFS_DEBUG
			val = dbgreg_thst;
			if(!mic_event_debug) {
#endif
				MIC_READ_MMIO_REG(statusreg,SBOX_THERMAL_STATUS);
				val = statusreg.value;
#ifdef MIC_EVENT_SYSFS_DEBUG
			}
#endif		
			break;
		default:
			break;
	}
	return val;
}					
#ifdef MIC_EVENT_SYSFS_DEBUG
/*	sysfs based event debug hooks.	*/
void mic_event_debug_set_ctrl(int ctrl)
{
	mic_event_debug = ctrl;
}

unsigned int  mic_event_debug_get_ctrl(void)
{
	return(mic_event_debug);
}
#ifdef	USE_SW_THRESHOLDS
/* Simulate SW threshold1 event */
void mic_event_debug_set_thresh1(int ctrl)
{
	if(ctrl) {
		atomic_inc(&swthresh1);
		dbgreg_thst |= SW_THRESHOLD1_MASK;
		turbostate = 0;
	}	
	else
		dbgreg_thst &= !SW_THRESHOLD1_MASK;
}
unsigned int mic_event_debug_get_thresh1(void)
{
	return (dbgreg_thst & SW_THRESHOLD1_MASK);
}	

void mic_event_debug_set_thresh2(int ctrl)
{
	if(ctrl) {
		atomic_inc(&swthresh2);
		dbgreg_thst |= SW_THRESHOLD2_MASK;
		turbostate = 0;

	}	
	else
		dbgreg_thst &= !SW_THRESHOLD2_MASK;
}
unsigned int mic_event_debug_get_thresh2(void)
{
	return (dbgreg_thst & SW_THRESHOLD2_MASK);
}	
#endif
void mic_event_debug_set_prochot(int ctrl)
{
	if(!mic_event_debug)
		return;
	if(ctrl)  {
		dbgreg_thstintr |= PROCHOT_MASK;
		atomic_inc(&inProcHot);
		turbostate = 0;
		ProcHotEvent();	
	}	
	else
		dbgreg_thst &= !PROCHOT_MASK;
}
unsigned int mic_event_debug_get_prochot(void)
{
	return (dbgreg_thstintr & PROCHOT_MASK);
}

void mic_event_debug_set_pwralrt(int ctrl)
{
	if(!mic_event_debug)
		return;
	if(ctrl) {
		dbgreg_thstintr |= PWRLART_MASK;
		inPwrAlert = 1;
		turbostate = 0;
	}
	else
		dbgreg_thstintr &= !PWRLART_MASK;
}			
int mic_event_debug_get_pwralrt(void)
{
	return(dbgreg_thstintr & PWRLART_MASK);
}			

void mic_event_debug_set_turbopt(int point)
{
	micevent_update_turbo(point);
}

int mic_event_debug_get_turbopt(void)
{
	return 	turbop_curr;
}
#endif
#ifdef	USE_SW_THRESHOLDS
/* Set the value (in degress C) of SW threshold 1. Use the
   parameter thresh as a delta from current die temp.
   */

void mic_event_set_swthreshold1(int thresh)
{
	sboxThermalInterruptEnableReg	reg;

	reg.value = 0;
	reg.bits.sw_threshold1_temp = thresh;
	reg.bits.sw_threshold1_enable = 1;
	toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,SW_THRESHOLD1_TEMP_MASK,reg.value);
}

unsigned int mic_event_get_swthreshold1(void)
{
	sboxThermalInterruptEnableReg	reg;

	MIC_READ_MMIO_REG(reg,SBOX_THERMAL_INTERRUPT_ENABLE);
	return reg.bits.sw_threshold1_temp;
}	

unsigned int mic_event_get_swthreshold2(void)
{
	sboxThermalInterruptEnableReg	reg;

	MIC_READ_MMIO_REG(reg,SBOX_THERMAL_INTERRUPT_ENABLE);
	return reg.bits.sw_threshold2_temp;
}	

void mic_event_set_swthreshold2(int thresh)
{
	sboxThermalInterruptEnableReg	reg;

	reg.value = 0;
	reg.bits.sw_threshold2_temp = thresh;
	reg.bits.sw_threshold2_enable = 1;
	toggle_tmu_reg(SBOX_THERMAL_INTERRUPT_ENABLE,SW_THRESHOLD2_TEMP_MASK,reg.value);
}
#endif

/* mic_event_set_pwralertlimit - Set the percent of TDP we need to 
   to throttle to on a pwrlimit event.
   */	
void mic_event_set_pwralertlimit(int freq)
{
	if(freq < 100)
		PwrAlertLimit = freq;
}
unsigned int mic_event_get_pwralertlimit(void)
{
	return  PwrAlertLimit;
}				
/*mic_event_get_turbostate - Returns status of turbo availablility 
  for SKU
  */	     
int mic_event_get_turboavail(void)
{
	return pstatedriver;
}	
/*mic_event_get_turbostate - Returns (a) current turbo state
  (b) Availability of turbo for this part
  */
unsigned int mic_event_get_turbostate(void)
{
	return turbostate;
}

unsigned int mic_event_get_dietemp(void)
{
	/*KAA: Suggest to get die temp from SBOX_THERMAL_STATUS_2 bits 19:10
	 *     That is the temperature SMC sees, and the highest of the 7 die sensors */
	/* Just get sensor 0 temp */
	return(mic_read_dietemp(0));
}	

void mic_event_set_turbostate(int state)
{

	micpm_turboctrl(state);
}	


/* Change the freq governor policy to set the max_freq to freq	*/
static void mic_change_policy(unsigned int freq)
{
	struct cpufreq_policy policy;
	int cpu = smp_processor_id();

	if(maxfreq!=freq) {
		printk("Setting maxfreq to %d \n",freq);
		maxfreq=freq;
		smp_wmb();
		if (!cpufreq_get_policy(&policy, cpu)) {
			cpufreq_update_policy(cpu);
			dprintk("updated cpufreq policy\n");
		}
		else
			printk(KERN_ERR"Failed to update cpufreq policy\n");
	}		
}			

static int bh_procHot(unsigned int *freq)
{
	sboxThermalStatusInterruptReg	statusreg;
	int retval = 0;
	unsigned long			flags;
	
	/* Use a while loop here to make sure that when we are processing a PROCHOT off 
	   condition we don't miss another prochot assertion (which might be caused by 
	   either a thermal hot or a power condition (PL0). The interrupt handler does 
	   not raise a ProcHotEvent while one is in process.
	*/       
	while(atomic_read(&inProcHot)) {
		spin_lock_irqsave(&miceventlock,flags);
		/* Make sure that the PROCHOT is still ON	*/
		statusreg.value = getstatusreg(SBOX_THERMAL_STATUS_INTERRUPT);
		if(statusreg.bits.gpuhot_status) {
		/* PROCHOT condition still ON	*/
			spin_unlock_irqrestore(&miceventlock,flags);
			*freq = procHotCoreFreq;
			if(prochotviddelay++ == PROCHOT_VID_DELAY)
				ProcHotEvent(EVENT_PROCHOT_VIDCHANGE,NULL);
			retval=1;		
			break;	
		}
		/* proc hot condition went away */
		ProcHotAsserted=0;
		spin_unlock_irqrestore(&miceventlock,flags);
		ProcHotEvent(EVENT_PROCHOT_OFF,(void *)&procHotRecoveryFreq);
		*freq = set_max_freq();
		if(atomic_dec_return(&inProcHot)) {	//OOPS looks like we got one more
			turbostate=0;
			prochotviddelay=0;
			ProcHotEvent(EVENT_PROCHOT_ON,NULL);
		}
	}
	return retval;
}
static int bh_pwralert(unsigned int *freq)
{
	sboxThermalStatusInterruptReg	statusreg;
	int retval = 0;
	
	if(inPwrAlert) {
		retval=1;
		statusreg.value = getstatusreg(SBOX_THERMAL_STATUS_INTERRUPT);
		if(statusreg.bits.pwralert_status) {
			/* Throttle freq to P2 if turbo is not ON or NA, else 
 			 * throttle to TDP. HSD4844711
 			 */
 			if(turboctrl) 
				*freq = freqobj.tdpfreq;
			else
				*freq = (freqobj.tdpfreq/100) * PWRALRT_THROTTLE_FREQ;
			
		}				
		else {			
			*freq = freqobj.tdpfreq;
			printk("Power alert deasserted\n");
			inPwrAlert=0;
			PwrAlertEvent(EVENT_OFF);
		}	
	}
	return retval;
}
#ifdef	USE_SW_THRESHOLDS
static int bh_therm(int signal, unsigned int *freq)
{
	sboxThermalStatusReg	thermalreg;
	int retval = 0;
	int status;
	atomic_t *thresh;
	unsigned int flimit;
	int *thflag;
		
	thresh = (signal == 1) ? &swthresh1 : &swthresh2;
	flimit = (signal == 1) ? SW_THRESHOLD1_FREQ : SW_THRESHOLD2_FREQ;
	thflag = (signal == 1) ? &thresh1throttled : &thresh2throttled;
	  			  		
	/* We need a while loop so we handle the condition where between us 
	   decrementing the swthreshx atomic and exiting this handler another swthresh
	   interrupt occurs and the atomic gets incremented again.
	*/   
	while(atomic_read(thresh)) {
		retval=1;
		thermalreg.value = getstatusreg(SBOX_THERMAL_STATUS);
		status = (signal == 1) ? thermalreg.bits.thermal_threshold1_status : 
				 thermalreg.bits.thermal_threshold2_status;
		if(status){
			if(!*thflag) {
				*thflag=1;
				dprintk("processing swthresh %d\n",(signal == 1) ? 1:2);
				//Send therm ON event ??
			}
			*freq = (freqobj.tdpfreq/100) * flimit; //Change max freq to SW_THRESHOLD_FREQ
			break;
		}
		else {
			*thflag=0;
			dprintk("finished with swthresh %d\n",(signal == 1) ? 1:2);	
		}					
		*freq = set_max_freq();
		if(!atomic_dec_return(thresh))
			break;
	}
	return retval;
}
#endif
#ifdef PARTIAL_CORE_TURBO
static void bh_turbo(unsigned int *freq)
{
	unsigned long flags;
	unsigned int  tp;
			
	if(pstatedriver && turbostate) {
		spin_lock_irqsave(&teventlock,flags);
		tp=turbop_req;
		smp_wmb();
		if(tp != turbop_curr){
			turbop_curr=tp;
			*freq=freqobj.tinfo.turbopts[tp].freq;
		}
		spin_unlock_irqrestore(&teventlock,flags);
	}
}
#endif
static void mic_event_handler(struct work_struct *work)
{

	unsigned int freq = ~0U, tmp=0;
	#ifdef	USE_SW_THRESHOLDS
	int i;
	#endif
	
	if(!bh_procHot(&freq)) {
		/* Come here only if a prochot is not in progress. */
#ifdef	USE_SW_THRESHOLDS
		/* Process sw therm threshold 2 first. Assumption is if both sw thresh2 
		   and thresh1 are set then thresh2 is always the higher limit.
		*/    
		for(i=2;i>=1;i--) {
			if(bh_therm(i,&tmp)) {
				if(tmp < freq)
					freq=tmp;
			}
		}
#endif
		/* Need to add call to bh_pwralert here. */
		if(bh_pwralert(&tmp)) {
			if(tmp < freq)
				freq=tmp;
		}
		
		/* Init turbo if we haven't done so. Make sure the cpufreq and cpuidle drivers
		   have registered their notification handlers and turbo is turned ON.
		*/    
		if(tinited) {
		 			
#ifdef PARTIAL_CORE_TURBO

			/* Bump turbo up only after a off to on transition is complete 
			   and we come back again and execute the worker thread.
			*/   
			if(turbostate) 
				bh_turbo(&freq);		
			
			else {
#endif
				if(freq == ~0U) {
					/* None of the thermal and pwralert throttling is ON. 
					So we can restart turbo.
					*/   
					if(turboctrl) {
#ifdef PARTIAL_CORE_TURBO
						spin_lock_irqsave(&teventlock,flags);
						turbop_curr = turbop_req = 0;
#endif
						turbostate = turboctrl;
#ifdef PARTIAL_CORE_TURBO
					spin_unlock_irqrestore(&teventlock,flags);
#endif
					}					
				}	
				tmp = set_max_freq();
				if(tmp < freq)
					freq=tmp;	
#ifdef PARTIAL_CORE_TURBO
			}
#endif
			
		}	
	}	
	if(freq != ~0U)
		mic_change_policy(freq);
	if(freqobj.rtccomp)
		(*freqobj.rtccomp)();
	queue_delayed_work(micevent_wq, &micevent_work,mic_event_delay);
}				

static void toggle_tmu_reg(int regoffset, uint32_t mask, uint32_t val)
{
	sboxThermalInterruptEnableReg reg; /* Pick some reg. */
	unsigned long flags;
	spin_lock_irqsave(&tmu_lock,flags);
	MIC_READ_MMIO_REG(reg,regoffset);
	reg.value &= (~mask);
	reg.value |= val;
	MIC_WRITE_MMIO_REG(reg,regoffset);
	spin_unlock_irqrestore(&tmu_lock,flags);
}

void clear_tmu_alert(void)
{
	sboxThermalStatusInterruptReg reg;

	reg.value=0;
	reg.bits.alert_log=1;
	toggle_tmu_reg(SBOX_THERMAL_STATUS_INTERRUPT,K1OM_THERMAL_STATUS_INTERRUPT_MASK,reg.value);
}
EXPORT_SYMBOL_GPL(clear_tmu_alert);

/**
 * micevent_cpufreq_register - Register the mic_cpufreq driver with the mic event
 * driver.
 * @:mic_cpufreq_object-Object contains information on available turbo points and also
 *
 *
 * If the current turbo point is lesser than the requested point then an increase 
 * in core freq is requested. This is done in the event handler in process context.
 * If current turbo point is > requested point then we need to reduce core freq 
 * which is done immediately by firing a notifier event into the cpufreq register.  
 * If they are the same nothing is done.
 */	
void micevent_cpufreq_register(struct notifier_block *atomic_notifier, 
	struct notifier_block *blocking_notifier,struct  mic_cpufreq_object *micobj)
{
	int i;
	sboxThermalStatusInterruptReg	statusreg;

	dprintk("cpufreq driver registered\n");
	micpm_atomic_notifier_register(atomic_notifier);
	micpm_notifier_register(blocking_notifier);
	memcpy(&freqobj,micobj,sizeof(struct mic_cpufreq_object));
	dprintk("TDP freq = %d\n",freqobj.tdpfreq);
	dprintk("Number of turbo points = %d\n",freqobj.tinfo.tpoints);
	if(freqobj.tinfo.tpoints) {
		for(i=0;i<freqobj.tinfo.tpoints;i++) {
			dprintk("Turbo point %d freq = %d\n",i,freqobj.tinfo.turbopts[i].freq); 
		}
		pstatedriver=1;	
	}
	maxfreq = freqobj.tdpfreq;
	procHotRecoveryFreq = maxfreq;
	smp_wmb();
	/* Take care of the condition where we might be booting up with PROCHOT ON */
	statusreg.value = getstatusreg(SBOX_THERMAL_STATUS_INTERRUPT);
	if(statusreg.bits.gpuhot_status) 
		raise_prochot_event(); 
	/* Same for PowerAlert event */
	if(statusreg.bits.pwralert_status)
		raise_pwralert_event();
}	 		


#ifdef PARTIAL_CORE_TURBO
/**
 * micevent_update_turbo - update partial core turbo point
 * @:turbopoint- requested turbo point (0=P01 (all cores active),1=P02, 2=P03)
 *
 * If the current turbo point is lesser than the requested point then an increase 
 * in core freq is requested. This is done in the event handler in process context.
 * If current turbo point is > requested point then we need to reduce core freq 
 * which is done immediately by firing a notifier event into the cpufreq register.  
 * If they are the same nothing is done.
 */																					
void micevent_update_turbo(int turbopoint)
{
	unsigned long flags;
	int tp;
	
	if((turbostate) && (turbopoint <= freqobj.tinfo.tpoints)) {
		spin_lock_irqsave(&teventlock,flags);
		if((turbop_curr != turbopoint) || (turbop_req != turbopoint)) {
			dprintk("Update turbo point from curr = %d to %d\n",
				turbop_curr,turbopoint);
		}
		tp = turbop_req;
		turbop_req = turbopoint;
		smp_wmb();
		if(tp != turbop_req)
			micevent_notify(EVENT_TURBO_LIMIT,&turbop_req,BLOCKING);
		spin_unlock_irqrestore(&teventlock,flags);
	}
}					
EXPORT_SYMBOL_GPL(micevent_update_turbo);
#endif

void micpm_turboctrl(int state)
{
	if(pstatedriver) {
		turboctrl = state;
		if(!tinited) {
#ifdef PARTIAL_CORE_TURBO
			micevent_notify(EVENT_TURBO_INIT,&freqobj.tinfo,BLOCKING);
#endif
			tinited++;
		}
		if(turbostate)
			turbostate=turboctrl;
	}		
}	 
EXPORT_SYMBOL_GPL(micpm_turboctrl);	

int event_worker_init(void)
{
	
	mic_event_delay = usecs_to_jiffies(MIC_EVENT_DELAY * 1000);

	micevent_wq = create_singlethread_workqueue("kmicevent");
	if (!micevent_wq) {
		printk(KERN_ERR "Creation of kmicevent failed\n");
		return -EFAULT;
	}
	INIT_DELAYED_WORK(&micevent_work,mic_event_handler);
	queue_delayed_work(micevent_wq, &micevent_work,mic_event_delay);
	return 0;
}	

static int micevent_cpufreq_notifier(struct notifier_block *nb,
					 unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	
	switch(event) {
	case	CPUFREQ_ADJUST:
			printk("micevent_cpufreq_notifier entered for CPUFREQ_ADJUST\n");
			policy->max = maxfreq;
			break;
	case	CPUFREQ_NOTIFY:			
			dprintk("micevent_cpufreq_notifier entered for CPUFREQ_NOTIFY\n");
			if(strcmp(policy->governor->name,"userspace") == 0) {
				dprintk("userspace governor chosen. turning off pstates\n");
				pstate = 0;
			}	
			else 
				pstate=1;
			break;
					
									 
	}
	return 0;
}	


static struct notifier_block micevent_cpufreq_notifier_block = {
	.notifier_call = micevent_cpufreq_notifier,
};

int mic_pm_event_init(void)
{
	int result;

	spin_lock_init(&tmu_lock);
	result = mic_pmevent_init();
	if(!result) {
		spin_lock_init(&miceventlock);
#ifdef PARTIAL_CORE_TURBO
		spin_lock_init(&teventlock);
#endif
		procHotCoreFreq=DEFAULT_PROCHOT_COREFREQ;
		PwrAlertLimit = PWRALRT_THROTTLE_FREQ; 
		event_worker_init();
		enable_interrupts();
#ifdef	USE_SW_THRESHOLDS
		mic_set_swthresholds();
#endif
		result = cpufreq_register_notifier(&micevent_cpufreq_notifier_block,
				      CPUFREQ_POLICY_NOTIFIER);
	}
	
	return result;
}

void mic_pm_event_exit(void)
{
	mic_pmevent_uninit();
	cpufreq_unregister_notifier(&micevent_cpufreq_notifier_block,
				      CPUFREQ_POLICY_NOTIFIER);

}				
