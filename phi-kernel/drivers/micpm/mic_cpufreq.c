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
 * mic_cpufreq:  cpufreq driver for Intel MIC devices.
 *
 * (C) Copyright 2008 Intel Corporation
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
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/compiler.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/cpufeature.h>
#include <linux/clocksource.h>
#include <asm/mic/mic_common.h>
#include "mic_cpufreq.h"
#include "mic_event.h"
#include "micras.h"

#ifdef PARTIAL_CORE_TURBO
#include "mic_turbo.h"
#endif

//#define dprintk(msg...) cpufreq_debug_printk(CPUFREQ_DEBUG_DRIVER,"mic-cpufreq", msg)
#ifdef CONFIG_GCOV_KERNEL
#define dprintk(msg...) printk(msg)
#else
#define dprintk(msg...) pr_debug(msg)
#endif

MODULE_AUTHOR("Caushik, Ramesh");
MODULE_DESCRIPTION("MIC processor P-States Driver");
MODULE_LICENSE("GPL");


#define	MAX_CORE_COUNT	62
#define	MAX_POSSIBLE_PSTATES	62
#define	POLICY_CPU	0	/* Do normal cpufreq ops only on CPU 0  */
#define	MIC_FAMILY	11
#define	K1OM_MODEL	1
#define	PN		0	/* Pn p-state. Order of freq/vid code seed values. */
#define	P1		1
#define	P0		2
#define	FREQ_STEP_SIZE	50	/* Step size for P-state frequencies    */

static DEFINE_SPINLOCK(mic_cpufreq_lock);
static DECLARE_COMPLETION(mic_cpufreq_driver);

/** Values for K1OM are from PM HAS. Actual values are got from the fuses.
*   TBD: RC - Get the actual data 	
**/
#define	K1OM_PN_FREQ	0x218U	/* Ratio code for       600MHz  - ME */
#define	K1OM_P1_FREQ	0x414U	/* Ratio code for       1000MHz - MNT */
/* The fuse value for P0 in some SKUs is actually the highest possible freq 
   and not the highest turbo freq for the SKU. That comes from the flash turbo 
   record. So default to the same as the P1 freq for those SKUs for now.
*/
#define	K1OM_P0_FREQ	0x614	/* Ratio code for 2000MHz at 200MHz base clock*/
#define K1OM_MAX_FREQ	1500000 /* This is the absolute maximum freq in KHz we can set the
				   part to if the P0 value in fuse is wrong and greater
				   than this.
				*/ 
#define MID_RATIO	(0x410 >> 1)	/* This is the ratio for intermediate step
					when we are changing freq from a code < 0x410 to
					a ratio > 0x410. Right shift is because we treat
					ratio codes in the driver without a LSB.
					*/ 	
#define	K1OM_PN_VID	0x68	/* Correspond to voltage  250 + (0x68 -1) * 5 = 765  mv */
#define K1OM_P1_VID	0xab	/* Correspond to voltage  250 + (0xab -1) * 5 = 1100 mv */
/* Same as above for VID too. The fuse value is the absolute max vid and not 
   the turbo vid.	
*/
#define	K1OM_P0_VID	K1OM_P1_VID
#define	K1OM_MAX_VID	0xe0	/* Correspond to voltage  250 + (0xe0 -1) * 5 = 1365 mv */

/* Turbo point ratio and vid values for testing	*/
#define K1OM_P02_FREQ	0x412
#define K1OM_P01_FREQ	0x414
#define K1OM_P02_VID	0xe0
#define K1OM_P01_VID	0xe0

#define	K1OM_RATIO_MASK	(~0x0FFFU)
#define K1OM_FF_POS	8	/*Bit position of the feed forward divider      */

#define K1OM_FREQ_DELAY	10	/* delay this many usecs, for each loop of the freq set routine */
#define K1OM_VID_DELAY	50	/* delay this many usecs for each loop of the vid set routine   */
#define K1OM_FREQ_LOOPS	50	/*  # of times to loop before we bail                           */
#define K1OM_VID_LOOPS	2
#define	K1OM_VID_STEP	2

#define K1OM_SVID_ADDR	0
#define	K1OM_SETVID_FAST	1
#define	K1OM_SETVID_SLOW	2

#define	K1OM_MCLK_2_FREQ(mclk)		(((mclk) & 0x7FE) >> 1)	/* Convert mclk ratio read from SBOX to freq ratio */
#define	K1OM_SET_MCLK(freq,mclk)	((mclk) = (((mclk) & 0x801) | ((freq) << 1)))

#define K1OM_VOLT_OFFSET	250

#define RTC_MAX_TEMP_BASE	125	/* TJHot fuse value is actually
					   RTC_MAX_TEMP_BASE - actual TJHot
					 */

#define VID_2_VOLTAGE(offset,vid)	((offset) + ((vid)-1) * 5)


#define ICCDIV_POS		25	/* Position of the ICCDiv fuse value
					   in the scratch reg.
					*/
#define ICCDIV_MASK		0x1f
#define ICC_REFCLK		4000
#define MIN_ICCDIV		18
#define MAX_ICCDIV		22


enum {
	DEVICE_K1OM,
	DEVICE_NONE,
};

/*******************************************************/
/* Fixes to take care of Si issue. Remove after C-step */
#define CORE_FREQ_FLOOR (0x410 >> 1)	   /* Freq code for the lowest freq */	
#define DBOX_BASE	0x08007C0000ULL    /* Just use DBox 0 */
#define DBOX_DPLLA_CTRL 0x00006014  	   /* Use only PLLA   */
#define DBOX_MHZ_OK_BIT	(1 << 25)	   /* If this bit set lowest freq
					      possible is CORE_FREQ_FLOOR 	 
					   */
	
/***********************************************/


#define	MIC_READ_MMIO_REG(var,offset)	\
              var.value = readl((const volatile void __iomem *)((unsigned long)(mic_sbox_mmio_va) +  (offset)));

#define	MIC_WRITE_MMIO_REG(var,offset) \
	writel(var.value,(volatile void __iomem *)((unsigned long)(mic_sbox_mmio_va) +  (offset)));

/* Low level access routines for the MIC device	*/
struct micdev {
	unsigned int (*get_core_freq) (void);
	int (*set_core_freq) (int freqcode, int vidcode);
};

struct mic_cpufreq_driver_data {
	const struct mic_pll_data *pll_data[DEVICE_NONE];
	struct mic_cpufreq_viddata *mic_viddata;
	struct cpufreq_frequency_table *freq_table;
	struct micdev mdev;	/* Stick in the device specific 
					routine pointers here.  */
	/* Non-turbo, turbo and partial core turbo p-states     */
	int pstates_nturbo, pstates_turbo, pstates_pturbo;
	int device;
	unsigned int resume;
};

static  struct mic_pll_data pll_data_k1om = {

	.bclk = 200000,
	.ffpos = K1OM_FF_POS,
	.ratio_data = {{8, 8, 15},
		       {4, 8, 15},
		       {2, 8, 15},
		       {1, 8, 15},
		       },
};

static struct mic_cpufreq_driver_data freq_data = {
	.pll_data = {[DEVICE_K1OM] =&pll_data_k1om,}
};

extern void *mic_sbox_mmio_va;	/* exported from arch/x86/kernel/intelmic.c */

static int set_cur_freq(int index);
static int mic_cpufreq_atomic_notifier(struct notifier_block *nb, unsigned long event,
				void *data);
static int mic_cpufreq_blocking_notifier(struct notifier_block *nb, unsigned long event,
				void *data);
static int set_vid_k1om(unsigned int vidcode);
static inline unsigned int freq_from_code(int baseclk, int code, int ffpos);

static struct notifier_block mic_cpufreq_atomic_notifier_block = {
	.notifier_call = mic_cpufreq_atomic_notifier,
};
static struct notifier_block mic_cpufreq_blocking_notifier_block = {
	.notifier_call = mic_cpufreq_blocking_notifier,
};

static atomic_t alertevent = ATOMIC_INIT(0);
static int thermalevent;

/* Read from fuse	*/
static int turbostate = 0;

/* Toggled by event notifications */
static int turboenabled;

/* The maximum core freq that can be set. Varies due to thermal 
   and power throttling and other events like turbo down.
*/

static unsigned int maxcorefreq,maxcorefreq_saved;
static int asynchevent;
static int driverbusy;

/* RTC compensation stuff	*/
static int RTCSlope;
static int VLimit;
static int TJHot;

static int lastvid = K1OM_P1_VID;	//The last vid value set.  
static unsigned int tdpfreq;	//In KHz
static unsigned int lastfreq;	//Last freq set in KHz
static int knc_stepping;
struct turbo_flash_record {
	struct turbofreq tf;
	struct mic_freqvid tcodes[MAX_TURBO_POINTS];
};
static struct turbo_flash_record trecord;
static int dbox_throttle;
static int prochotfrequency;
static int prochotvid;		/* Vid the part has to be set to during prochot */
static int tdpvid;

/*******************************************************/
/* Fixes to take care of Si issue. Remove after C-step */

static void *mic_dbox_mmio_va;
 

/**
 * get_displaypll_mhzok(void)- Gets the MHz_OK bit in the displaypll
 * register. If this bit is set then we will restrict the range of
 * frequencies to between CORE_FREQ_FLOOR and P0. Else we go by the
 * fuse values.	
 * Returns: 0=min freq is by fuse. 1 = CORE_FREQ_FLOOR (800 Mhz).
 */

static inline int get_displaypll_mhzok(void) 
{
	int value;

	/* All boxes have the same MMIO length. So just use MIC_SBOX_MMIO_SIZE */
	mic_dbox_mmio_va = ioremap(DBOX_BASE, MIC_SBOX_MMIO_SIZE);
	if(!mic_dbox_mmio_va) {
		printk("Failed to map DBox \n");
		return 0;
	}
	value = readl(mic_dbox_mmio_va + DBOX_DPLLA_CTRL);
	printk("Dbox PLLA_CTRL register has %x\n",value);
	iounmap(mic_dbox_mmio_va);
	return(value & DBOX_MHZ_OK_BIT);
}
 
/**
* get_baseclk - Computes baseclk value for SKU.
*               Base clk is RefClk/IccDiv where RefClk is a
*		constant and IccDiv is got from SBOX Scratch4 reg.
* 
*Returns computed base clk value in KHz. 0 on failure.
**/

static int get_baseclk(void)
{
	sboxPcieVendorIdDeviceIdReg reg; /* Any reg type would do */
	int iccdiv;

	if (mic_sbox_mmio_va) {
		MIC_READ_MMIO_REG(reg, SBOX_SCRATCH4);
		iccdiv = (reg.value >> ICCDIV_POS) & ICCDIV_MASK;
		if((iccdiv < MIN_ICCDIV) || (iccdiv > MAX_ICCDIV)){
			printk("Iccdiv (%d) from scratch 4 out of range.\n",iccdiv);
			return 0;
		}
		return((ICC_REFCLK * 1000)/iccdiv);
	}
	return 0;
}
#ifdef PARTIAL_CORE_TURBO

static  int get_mic_sku(void)
{
	sboxPcieVendorIdDeviceIdReg reg;
	int skuid=0;

	if (mic_sbox_mmio_va) {
		MIC_READ_MMIO_REG(reg, SBOX_PCIE_VENDOR_ID_DEVICE_ID);
		skuid = reg.bits.device_id & 0x0f;
		printk("SKU id = %d\n", skuid);
		return skuid;
	}
	return -1;
}
static int get_mic_fuseid(void)
{
	sboxPcieVendorIdDeviceIdReg reg;
	int fuseid;
	
	if (mic_sbox_mmio_va) {
		MIC_READ_MMIO_REG(reg, SBOX_SCRATCH7);
		fuseid = reg.value & FUSEREV_MASK;
		printk("Fuse rev id = %d\n",fuseid);
		return (fuseid);
	}
	return -1;
}	

static int process_tpoints(struct TurboPoint *tpoints,struct mic_freqvid *seed,int numpts)
{
	
	int j,k,cores,freq;
	int ffpos, bclk, allcore;

	ffpos = freq_data.pll_data[DEVICE_K1OM]->ffpos;
	bclk = freq_data.pll_data[DEVICE_K1OM]->bclk;
	
	/* Not all SKUS will have a all core turbo point. For SKUS that do 
	   not we will create a dummy all core turbo point with the P1 freq
	   as the turbo freq so that the turbo point array is symmetrical
	   in both cases. 
	 */
	j = k = 0;
	allcore = 1;
	while (j < numpts) {
		cores =
		    tpoints[j].activecores;
		freq =
		    tpoints[j].freq;
		if ((k == 0)
		    && (cores != nr_cpu_ids / 4)) {
			/* SKU has no all core turbo point */
			freq =
			    freq_from_code(bclk,
					   seed[P1].
					   freq,
					   ffpos); 
			cores = nr_cpu_ids / 4;
			allcore = 0;
		}
		trecord.tf.turbopts[k].corelimit =
		    cores;
		trecord.tf.turbopts[k].freq = freq;
		if (freq > freq_from_code(bclk,
					   seed[P0].
					   freq,
					   ffpos)
			    ){
			printk
			    ("Freq (%d) for turbo point %d too high. Clamping to P0.\n",
			     freq, j);
				trecord.tf.turbopts[k].freq =
				freq_from_code(bclk,
				seed[P0].freq,ffpos);
			    }
		if (allcore)
			j++;
		allcore++;
		k++;
	}
	trecord.tf.tpoints = k;
	return 0;
}
static int parse_turbo_block(void *bptr, int sku, struct mic_freqvid *seed,int version)
{
	struct TurboPoint *tpoints;
	
	int retval = -1;
	int i,numrec, numpts;
	int vsku;
	
	printk("Looking for version %d record with sku id = %x\n",version,sku); 
	if(version == 1) 
		numrec = ((struct TurboFlashRecord *)bptr)->numrecords;				  
	else
		numrec = ((struct TurboV2FlashRecord *)bptr)->numrecords;
	printk("Number of records in flash record = %d\n",numrec);
	if (numrec > MAX_SKUS) {
		printk("Too many sku records (%d) in flash record.\n",
			numrec);
		goto fail;
	}
			
	for (i = 0; i < numrec; i++) {
		if(version == 1) {
			vsku = ((struct TurboRecord *)
				(&(((struct TurboFlashRecord *)bptr)->trecords[i])))->skuid;
			numpts = ((struct TurboRecord *)
				(&(((struct TurboFlashRecord *)bptr)->trecords[i])))->num;
			tpoints = ((struct TurboRecord *)
				(&(((struct TurboFlashRecord *)bptr)->trecords[i])))->tpoints; 	
		
		}			
		else {
			vsku = ((struct TurboV2Record *)
				(&(((struct TurboV2FlashRecord *)bptr)->trecords[i])))->skuid;
			numpts = ((struct TurboV2Record *)
				(&(((struct TurboV2FlashRecord *)bptr)->trecords[i])))->num;
			tpoints = ((struct TurboV2Record *)
				(&(((struct TurboV2FlashRecord *)bptr)->trecords[i])))->tpoints;	
		}
		printk("vsku = %x, numpts = %d, address = %lx\n",vsku,numpts,(unsigned long)tpoints);  		
		/* Mask off bits 14 to 32 of skuid because we don't care about 
		   the other info coded.
		*/    
	
		if ((vsku & SKUID_FUSEID_MASK) == sku) {
			if (numpts > MAX_TURBO_POINTS) {
				printk
				    ("Too many turbo points (%d) for sku %d\n",
				     numpts, sku & 0xf);
				break;
			}
			return(process_tpoints(tpoints,seed,numpts));	
		}
	}			
fail:
	return retval;
}

static void *find_flash_record(int *version)
{
	/*Need to detect location of flash overclocking block.
	  The two versions of the block are located at different
	  addresses. 
	  Remove support for older version block later.
	*/
	void *blockptr,*ptr;
	
	/* Try the newer one first*/
	blockptr = ioremap(FLASH_OVERCLOCKING_RECORD_PHYS_ADDR, TURBO_FOOTER_V2_OFFSET);
	if(blockptr) {
		ptr = blockptr + FLASH_TURBO_RECORD_V2_OFFSET;	
		printk("Looking for V2 record at address %lx\n",(unsigned long)ptr);
		printk("Signature on V2 record = %d\n",((struct TurboV2FlashRecord *)ptr)->StartSignature); 
		if(((struct TurboV2FlashRecord *)ptr)->StartSignature 
			== TURBO_RECORD_VALID_START_SIGNATURE) {
			printk("Found V2 turbo record in flash\n");
			*version = 2;
			return(ptr);
		}	 
		ptr = blockptr + FLASH_TURBO_RECORD_V1_OFFSET;		
		if(((struct TurboFlashRecord *)ptr)->StartSignature 
			== TURBO_RECORD_VALID_START_SIGNATURE) {
			printk("Found V1 turbo record in flash\n");
			*version = 1;
			return(ptr);
		}
	}	
	return NULL;
}	
		
static int readturborecord(struct mic_freqvid *seed)
{

	int sku,fuse,version,retval=-1;
	void *blockptr;
		
	/* Get SKU information */
	if(((sku = get_mic_sku()) != -1) &&
		((fuse = get_mic_fuseid()) != -1)) {
		  
		/* Locate the flash overclocking block */
		if ((blockptr = find_flash_record(&version))) {
			/* Build a composite id with sku and fuse ids*/
			/*We will ignore the GDDR, Memory type bits for now */
			if(version == 2) {
				/* HACK-B0PO skus have fuse rev ids in a range 50-100 and are 
 				   all coded to 0x100 in the flash record though they have different 
				   values within the range on the part (scratch 7).
				*/   
				if(fuse <= 100)
					fuse = 100;
				sku = (fuse << 4) | sku;
			}
			if (!parse_turbo_block(blockptr, sku, seed,version)) {
				print_turbo_record();
				retval=0;
			}
			iounmap(blockptr);
		}
	}
	return retval;
}
#endif

static void print_turbo_record(void)
{
	int i;

	printk("# of turbo points: %d\n", trecord.tf.tpoints);
	for (i = 0; i < trecord.tf.tpoints; i++) {
		printk("Turbo point %d : (%d,%d)\n", i,
			trecord.tf.turbopts[i].freq,
			trecord.tf.turbopts[i].corelimit);
	}
}

/* Convert a feedforward divider code to value or vice versa */
static inline int ffcode_to_val(unsigned int code)
{
	return (~(code & 3) & 3);
}

/* Feedforward divider value from ratio code	*/
static inline int ff_from_code(unsigned int code, int ffpos)
{
	return (8 / (1 << ((code >> ffpos) & 3)));
}

/* Feeback divider value from ratio code	*/
static inline unsigned int fb_from_code(unsigned int code, int ffpos)
{
	return (code & (~((~0U) << ffpos)));
}

/* ratio code from FF and FB dividers	*/
static inline unsigned int ratio_from_fffb(int ff, int fb, int ffpos)
{
	return ((ff << ffpos) | fb);
}

/* Frequency value in MHz from ratio code	*/
static inline unsigned int freq_from_code(int baseclk, int code, int ffpos)
{
	int fb, ff;		/* Feedback and Feedforward dividers    */

	ff = ff_from_code(code, ffpos);
	fb = fb_from_code(code, ffpos);
	return (baseclk * fb / ff);
}
static inline int check_booted_bit(void)
{
	sboxCorefreqReg corefreq;

	MIC_READ_MMIO_REG(corefreq, SBOX_COREFREQ);
	return(corefreq.bits.booted);
}
static unsigned int get_cur_freq_k1om(void)
{
	sboxCurrentratioReg clock_ratio;

	if(thermalevent)
		return prochotfrequency;
	
	MIC_READ_MMIO_REG(clock_ratio, SBOX_CURRENTRATIO);
	return (freq_from_code
		(freq_data.pll_data[DEVICE_K1OM]->bclk,
		 K1OM_MCLK_2_FREQ(clock_ratio.bits.mclkratio),
		 freq_data.pll_data[DEVICE_K1OM]->ffpos));
}

static unsigned int svid_cmd_fmt(unsigned int bits)
{
	unsigned int bits_set, bmask;

	bmask = bits;

	for (bits_set = 0; bmask; bits_set++) {

		/* Zero the least significant bit that is set */

		bmask &= (bmask - 1);
	}
	bits <<= 1;		/* Make way for the parity bit      */
	if (bits_set & 1)	/* odd number of 1s */
		bits |= 1;
	return bits;
}

static int svid_poll_write(sboxSvidcontrolReg data)
{
	sboxThermalStatusInterruptReg statusreg;
	int i;

	/* Clear alert status log bit.  */
	clear_tmu_alert();
	MIC_WRITE_MMIO_REG(data, SBOX_SVIDCONTROL);
	for (i = 0; i < K1OM_VID_LOOPS; i++) {
		udelay(K1OM_VID_DELAY);
		MIC_READ_MMIO_REG(statusreg, SBOX_THERMAL_STATUS_INTERRUPT);
		if (statusreg.bits.alert_log) {
			//printk("Alert Status set\n");
			return 0;
		}
	}
	return -EINVAL;
}

static inline unsigned int read_dietemp(int sensor)
{
	sboxCurrentDieTemp0Reg dietemp;
	U32 value;

	MIC_READ_MMIO_REG(dietemp, SBOX_CURRENT_DIE_TEMP0);
	switch (sensor) {
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
static inline int get_cur_vid(void)
{
	sboxCorevoltReg vidreg;

	MIC_READ_MMIO_REG(vidreg, SBOX_COREVOLT);
	return(vidreg.bits.vid);
}

/**
* rtccomp - Compensate vidcode for temp.
*
@vidcode - uncompensated vid code
*Return compensated vidcode on success or the original vidcode on FAILURE.
**/

static unsigned int rtccomp(unsigned int vidcode)
{
	int temp;
	unsigned int tcode, vidticks;

	temp = read_dietemp(0);	/* Use sensor 0 for now */
	if ((temp > (RTC_MAX_TEMP_BASE - TJHot)) || (vidcode > VLimit)
	    || (RTCSlope == 0)) {
		//dprintk("Failed RTC comp\n");
		return vidcode;
	}
	tcode =
	    (RTCSlope * (RTC_MAX_TEMP_BASE - TJHot - temp) *
	     (VLimit - vidcode) * 1000) / 10000;
	vidticks = (tcode / 1000) + (tcode % 1000) / 500;
	//dprintk("RTCcomp:current vid(%d) die temp (%d), comp = %d vid ticks\n",
	//	vidcode, temp, vidticks);
	return (vidcode + vidticks);
}

/**
* do_RTCComp - This function is called periodically by the micpm event
* handler to do voltage compensation based on die temp.
**/
static void do_RTCComp(void)
{
	unsigned long flags;

	spin_lock_irqsave(&mic_cpufreq_lock, flags);
	if (!asynchevent) {
		set_vid_k1om(lastvid);
	}
	spin_unlock_irqrestore(&mic_cpufreq_lock, flags);
}

static int set_vid_k1om(unsigned int vidcode)
{
	sboxSvidcontrolReg svidreg;
	unsigned int currvid, temp, tmpvid;
	int i = 0;

	//dprintk("Setting vid to %x\n", vidcode);

	if (knc_stepping != KNC_A_STEP) {	/* Anything except A0 */
		currvid = get_cur_vid();
		//dprintk("Current vid = %x\n", currvid);
	} else {
		currvid = lastvid;	/* For A0 just set it to the last value */
	}
	tmpvid = vidcode;		/* Remember the uncompensated vid */
	vidcode = rtccomp(vidcode);	/* RTC compensate the vidcode */
	if (currvid == vidcode)
		return 0;
	for (i = 0; i < K1OM_VID_LOOPS; i++) {
		MIC_READ_MMIO_REG(svidreg, SBOX_SVIDCONTROL);
		/* If A0 stepping don't bother to check for svidreg idle */
		if ((knc_stepping == KNC_A_STEP) || svidreg.bits.svid_idle) {

			temp =
			    svid_cmd_fmt((K1OM_SVID_ADDR << 13) |
					 (K1OM_SETVID_SLOW << 8) | vidcode);
			svidreg.bits.svid_cmd =
			    (K1OM_SVID_ADDR << 5) | K1OM_SETVID_SLOW;
			svidreg.bits.svid_dout = temp & 0x1ff;
			svidreg.bits.cmd_start = 1;
			//dprintk("Writing %x to svidctrl reg\n", svidreg.value);
			if (!svid_poll_write(svidreg)) {
				lastvid = tmpvid;
				return 0;
			}
			if (knc_stepping != KNC_A_STEP) {	/* For B0 and later */
				MIC_READ_MMIO_REG(svidreg, SBOX_SVIDCONTROL);
				if (!svidreg.bits.svid_idle
				    || svidreg.bits.svid_error) {
					if (!svidreg.bits.svid_idle) {
						dprintk
						    ("SVID command failed - idle not set ");
						continue;
					} else if ((svidreg.bits.svid_din >> 9)
						   == 2)
						dprintk
						    ("SVID command failed - rx parity error");
					else
						dprintk
						    ("SVID command failed - tx parity error");
					return -EINVAL;
				}
			}
		}
	}
	dprintk("Timed out waiting for SVID idle");
	return -EINVAL;
}

static int set_cur_freq_k1om(int freqcode, int vidcode)
{
	sboxCurrentratioReg curratio;
	sboxCorevoltReg currvid;
	sboxCorefreqReg corefreq;
	int i, vidfirst = 0,j,steps;
	unsigned int tratio,target[2],tcode;

	MIC_READ_MMIO_REG(curratio, SBOX_CURRENTRATIO);
	MIC_READ_MMIO_REG(currvid, SBOX_COREVOLT);
	tratio = K1OM_MCLK_2_FREQ(curratio.bits.mclkratio);

	dprintk("Current ratio=%x Target ratio= %x\n", tratio, freqcode);
	if ((tratio == freqcode) && !asynchevent) /* If we are in a asynch event 
						     go ahead and set freq anyway.
						     In a prochot condition ratio
						     may be the same but booted bit
						     needs to be set.
						  */ 
		goto out;
	if ((tratio < freqcode) || thermalevent)	/* In case we are coming out 
							PROCHOT the tratio read from
							corefreq reg may be < freqcode
							set, but the vid would be at
							MINVID.
							*/ 	
		vidfirst = 1;
	if (vidfirst) {
		/* Increase vid first before bumping up the freq        */
		if (set_vid_k1om(vidcode))
			return -EINVAL;
	}
	/* Add workaround for CD 446 (HSD4845197). If you are going from 
 	a freq lower than a ratio code 0x410 (nominally 800Mhz) to a target > 0x410
	then do it in 2 steps. No need to adjust vid, because we have already set the
	vid to match the ultimate target.
	*/
	steps=1;
	target[0]=freqcode;
	if((tratio < MID_RATIO) && (freqcode > MID_RATIO)) {
		steps=2;
		target[0]=MID_RATIO;target[1]=freqcode;
	}
	for(j=0;j<steps;j++) { 
		tcode=target[j];
		MIC_READ_MMIO_REG(corefreq, SBOX_COREFREQ);
		K1OM_SET_MCLK(tcode, corefreq.bits.ratio);
		corefreq.bits.booted = 1;
		dprintk("Writing %x to core freq register\n", corefreq.value);
		MIC_WRITE_MMIO_REG(corefreq, SBOX_COREFREQ);
		for (i = 0; i < K1OM_FREQ_LOOPS; i++) {
			MIC_READ_MMIO_REG(curratio, SBOX_CURRENTRATIO);
			if (K1OM_MCLK_2_FREQ(curratio.bits.mclkratio) == tcode)
				break;
			udelay(K1OM_FREQ_DELAY);
		}
		if (i == K1OM_FREQ_LOOPS) {
			dprintk("Failed to set core freq to %d",
					freq_from_code(freq_data.pll_data[DEVICE_K1OM]->bclk,
						tcode,freq_data.pll_data[DEVICE_K1OM]->ffpos));
			if (vidfirst)	/* Core freq set failed. Undo the vid set   */
				set_vid_k1om(currvid.bits.vid);
			return -EINVAL;
		}
		if (!vidfirst) {
			if (set_vid_k1om(vidcode))
				/* NOTE: If core freq lowered and the vid set failed
				   we do not undo the freq change.
				   */
				return -EINVAL;
		}
	}
out:
	/* Remember the last freq set so we can go back to it after either a 
 	*  thermal/power event or when we exit out of PC3.
 	*/
	if(!asynchevent)
		lastfreq=freq_from_code(freq_data.pll_data[DEVICE_K1OM]->bclk,
				       freqcode,freq_data.pll_data[DEVICE_K1OM]->ffpos);
	return 0;
}

/**
*asynch_throttle - Asynchronously reduce core freq. Normally core freq changes
*are done through calls to the target method in the cpufreq driver made by the
*cpufreq governor. But we have cases (like a PROCHot condition or the partial
*core turbo down event) where we cannot wait for the governor to come around 
*and do the throttling which might be tens of ms away. 
*This should be called with the mic_cpufreq_lock held. 
*@freq: freq to throttle to.
*			 			
**/
static void asynch_throttle(unsigned int tfreq)
{
	int index = ~0;
	unsigned int freq;
	int pindex = ~0;
	int i;

	asynchevent = 1;
	smp_wmb();
	if (driverbusy)
		return;
	for (i = 0;
	     (freq = freq_data.freq_table[i].frequency) != CPUFREQ_TABLE_END;
	     i++) {
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;
		else if(pindex == ~0)
			pindex=i;			
		if (freq == tfreq) {
			index = i;
			break;
		}
		if (freq > tfreq) {
			index = pindex;
			break;
		}
		pindex=i;
	}
	if (index == ~0) {
		asynchevent=0;
		return;
	}
	set_cur_freq(index);
	asynchevent = 0;
}

static void mic_init_turbo(int device, struct turbofreq *tf)
{
	if (turbostate && trecord.tf.tpoints) {	/* If we have points beyond all core turbo */
		memcpy(tf, &trecord.tf, sizeof(struct turbofreq));
	}
}

/**
* mic_get_freq_points - Get the seed points on the freq/voltage curve.
* Either got from fuse values or hard coded. TBD: RC - On K1OM we need
* to get this from the flash driver because the bootstrap will read fuse values
* and temporarily stash them away in the flash MMIO regs before locking fuses.
@seed - Return seed values of freq  and vid code in this
@device - MIC device 
*Return 0 on success or -EINVAL.
**/
static int mic_get_freq_points(int device, struct mic_freqvid *seed)
{
	sboxEmonCounter0Reg reg;
	unsigned int freq, vid;
	unsigned int maxratio, maxvid;
	int baseclk,ffpos;

	if (device == DEVICE_K1OM) {
		seed[PN].freq = K1OM_PN_FREQ;
		seed[PN].vid = K1OM_PN_VID;
		seed[P1].freq = K1OM_P1_FREQ;
		seed[P1].vid = K1OM_P1_VID;
		seed[P0].freq = K1OM_P0_FREQ;
		seed[P0].vid = K1OM_P0_VID;
		
		ffpos = freq_data.pll_data[DEVICE_K1OM]->ffpos;
		baseclk = get_baseclk();
		printk("Base clk for part is %d\n",baseclk);
		if(baseclk)
			pll_data_k1om.bclk = baseclk;				
		MIC_READ_MMIO_REG(reg, SBOX_EMON_CNT0);
		printk("SBOX_EMON_CNT0 = %x\n",reg.value);
		freq = reg.value & 0x00000fff;
		vid = (reg.value & 0x00ff0000) >> 16;
		turbostate = (reg.value >> 12) & 1;

		maxratio = K1OM_P0_FREQ;
		maxvid = K1OM_MAX_VID;
		printk
		    ("Fuse value read for P0: Freq(%x) VID(%x) Turbo state(%d)\n",
		     freq, vid, turbostate);
		if (vid < maxvid)
			seed[P0].vid = vid;

		if (freq < maxratio)
			seed[P0].freq = freq;
		
		MIC_READ_MMIO_REG(reg, SBOX_EMON_CNT1);
		freq = reg.value & 0x00000fff;
		vid = (reg.value & 0x00ff0000) >> 16;

		printk("Fuse value read for P1: Freq(%x) VID(%x)\n", freq,
			vid);

		if (freq > K1OM_PN_FREQ && freq <= seed[P0].freq)
			seed[P1].freq = freq;
		if (vid > K1OM_PN_VID && vid <= seed[P0].vid)
			seed[P1].vid = vid;
		tdpvid=vid;
		MIC_READ_MMIO_REG(reg, SBOX_EMON_CNT2);
		freq = reg.value & 0x00000fff;
		vid = (reg.value & 0x00ff0000) >> 16;

		printk("Fuse value read for PN: Freq(%x) VID(%x)\n", freq,
			vid);

		if (freq > K1OM_PN_FREQ && freq < seed[P1].freq)
			seed[PN].freq = freq;
		if (vid > K1OM_PN_VID && vid < seed[P1].vid)
			seed[PN].vid = vid;

		/* Rest of code assumes no reserve LSB bit in code      */
		seed[P0].freq = seed[P0].freq >> 1;
		seed[P1].freq = seed[P1].freq >> 1;
		seed[PN].freq = seed[PN].freq >> 1;
		
		/* Some old B0 cards had turbo_enable fuse bit set
		and crazy large P0 value in the fuses. For those
		disable turbo. 
		*/
		if(freq_from_code(baseclk,seed[P0].freq,ffpos) > K1OM_MAX_FREQ)
				turbostate=0;  

		/* Get the turbo points for SKU */
		/* Do not look for a turbo record in flash. RTC44281 */ 
		if (turbostate && (knc_stepping!=KNC_A_STEP))  {
			trecord.tf.tpoints = 1;
			trecord.tf.turbopts[0].corelimit=nr_cpu_ids/4;
			trecord.tf.turbopts[0].freq = freq_from_code(baseclk,seed[P0].freq,ffpos);
			print_turbo_record();
		}
		else {
			trecord.tf.tpoints = 0;
			turbostate=0;
		}
		
		/* Get RTC compensation values */
		MIC_READ_MMIO_REG(reg, SBOX_EMON_CNT3);
		RTCSlope = reg.value & 0x0000007f;
		VLimit = (reg.value >> 13) & 0x1ff;
		TJHot = (reg.value >> 7) & 0x3f;
		printk
		    ("RTC comp fuse values: Slope (%d) VLimit(%d) TJHot (%d)\n",
		     RTCSlope, VLimit, TJHot);
	} else
		return -EINVAL;
	return 0;
}

static int mic_generate_vid_codes(struct mic_freqvid *seed,
				  struct mic_cpufreq_driver_data *data)
{
	struct mic_cpufreq_viddata *vidtable;
	int nturbo, turbo, i, j, vidrange, result = 0;

	nturbo = data->pstates_nturbo;	/*Number of non-turbo p-states   */
	if (!nturbo) {
		dprintk("No non turbo states ? No need for a vidtable");
		return -EINVAL;
	}
	turbo = data->pstates_turbo;	/*Number of turbo p-states        */

	dprintk("Generating vid codes for %d nonturbo and %d turbo states\n",
		nturbo, turbo);
	if (!
	    (vidtable =
	     kzalloc(sizeof(struct mic_cpufreq_viddata) *
		     (nturbo + turbo + MAX_TURBO_POINTS), GFP_KERNEL))) {
		dprintk("Failed to allocate memory for the vidtable");
		return -ENOMEM;
	}
	/* Generate vid values for turbo points */
	if (turbo) {
		vidrange = seed[P0].vid - seed[P1].vid;
		vidtable[nturbo + turbo - 1].vid = seed[P0].vid;
		for (i = nturbo + turbo - 2, j = 1; i >= nturbo; i--, j++) {
			vidtable[i].vid = seed[P0].vid - vidrange * j / turbo;
		}
	}
	vidrange = seed[P1].vid - seed[PN].vid;
	vidtable[nturbo - 1].vid = seed[P1].vid;
	for (i = nturbo - 2, j = 1; i > 0; i--, j++) {
		vidtable[i].vid = seed[P1].vid - vidrange * j / nturbo;
	}
	/* Fill in value for PN */
	vidtable[0].vid = seed[PN].vid;
	data->mic_viddata = vidtable;
	return result;
}

/**
* mic_generate_freq_table - Generate the freq table.
* Since we do not know the number of P-states in the table
* the routine calculates the number of P-states based on PN and P0 
*frequencies and the freq step size. However we cannot do a simple
*(P0-Pn)/stepsize calculation because of jumps in the supported frequencies.
*We allocate a temp buffer create the table in this temp buffer
*and then when we know the actual number of P-states create the actual table and copy
*temp table to the actual table. 
*@seed - Seed values of freq  and vid code in this
@device - MIC device 
*Return 0 on success nonzero otherwise.
**/
static int mic_generate_freq_table(int device, struct mic_freqvid *seed)
{

	int sidx, ffpos, bclk;
	unsigned int nextfreq, lastfreq;
	struct cpufreq_frequency_table *tempbuf, *ftable;
	unsigned int ffval, fbval, nextcode;
	int result = 0, i = 0, skip = 0, pstates = 0,j=0;

	/* Allocate temp buffer for freq table values and voltage values we generate as we find out the number of P-states */
	if (!
	    (tempbuf =
	     kzalloc(sizeof(struct cpufreq_frequency_table) *
		     MAX_POSSIBLE_PSTATES, GFP_KERNEL))) {
		dprintk("Failed to allocate temp memory for cpufreq table");
		return -ENOMEM;
	}

	ffpos = freq_data.pll_data[device]->ffpos;
	bclk = freq_data.pll_data[device]->bclk;

	/* Get the index into pll_data array for the first PN state     */
	sidx = (seed[PN].freq >> ffpos) & 3;

	nextfreq = freq_from_code(bclk, seed[PN].freq, ffpos);
	tdpfreq = freq_from_code(bclk, seed[P1].freq, ffpos);
	/* If turbostate is disabled in the fuse for the SKU then we set
	   max freq to P1.
	*/
	if(turbostate)
		lastfreq = freq_from_code(bclk, seed[P0].freq, ffpos);
	else
		lastfreq=tdpfreq;

	freq_data.pstates_nturbo = freq_data.pstates_turbo = 0;

	ffval = ff_from_code(seed[PN].freq, ffpos);
	fbval = fb_from_code(seed[PN].freq, ffpos);
	nextcode = seed[PN].freq;

	while (nextfreq <= lastfreq) {
		unsigned int currfreq = (fbval * bclk) / ffval;
		if (currfreq >= nextfreq) {
			if (!skip) {
				tempbuf[i].index = nextcode;
				tempbuf[i++].frequency = currfreq;	/* Frequencies in table are in kHz      */

				if (currfreq <= tdpfreq)
					freq_data.pstates_nturbo++;
				else
					freq_data.pstates_turbo++;
				pstates++;
			}
			if (currfreq > nextfreq) {
				skip = 1;
				nextfreq += FREQ_STEP_SIZE;
				continue;
			}
			nextfreq += FREQ_STEP_SIZE;
		}
		skip = 0;
		nextcode++;
		fbval++;
		if (fbval >
		    freq_data.pll_data[device]->ratio_data[sidx].maxratio) {
			sidx += 1;
			if (sidx > MAX_RATIO_ENTRIES) {
				dprintk("Invalid access into pll_data array");
				result = -EINVAL;
				goto freeret;
			}
			fbval =
			    freq_data.pll_data[device]->ratio_data[sidx].
			    minratio;
			ffval /= 2;	/* Feedforward divider halved everytime */
			nextcode = ratio_from_fffb(ffval, fbval, ffpos);
		}

	}
	if (!
	    (ftable =
	     kzalloc(sizeof(struct cpufreq_frequency_table) * (i + 1),
		     GFP_KERNEL))) {
		dprintk("Failed to allocate freq table");
		result = -ENOMEM;
		goto freeret;
	}
	memcpy(ftable, tempbuf, sizeof(struct cpufreq_frequency_table) * i);
	ftable[i].frequency = CPUFREQ_TABLE_END;
	freq_data.freq_table = ftable;
	freq_data.device = device;
	if (mic_generate_vid_codes(seed, &freq_data)) {
		dprintk("Vid code generation failed\n");
		kfree(ftable);
		freq_data.freq_table = NULL;
		result = -EINVAL;
	}
/*	Add a fix here for Si issue where the PN freq is clamped to a lower value. **/
/*	Remove after C-step	*/
	dbox_throttle=0;
	if (knc_stepping != KNC_C_STEP)
		dbox_throttle= !get_displaypll_mhzok();
	prochotfrequency=PROCHOT_FREQUENCY;
	prochotvid=freq_data.mic_viddata[0].vid;
	if(dbox_throttle) {
		for(j=0;j<=i;j++) {
			if(ftable[j].frequency >= 
				freq_from_code(bclk,CORE_FREQ_FLOOR,ffpos))
					 break;
			printk("Marked freq %d not available\n",ftable[j].frequency);
			ftable[j].frequency = CPUFREQ_ENTRY_INVALID;
		}
		prochotfrequency=freq_from_code(bclk,CORE_FREQ_FLOOR,ffpos);
		prochotvid=freq_data.mic_viddata[j].vid;
	}
	if (freq_data.freq_table) {
		dprintk("KNC cpu freq table\n");
		dprintk("------------------\n");
		dprintk("Code  Freq   Vid   \n");

		for (i = 0;
		     freq_data.freq_table[i].frequency != CPUFREQ_TABLE_END;
		     i++) {
			dprintk("%x %d	  %x \n", freq_data.freq_table[i].index,
				freq_data.freq_table[i].frequency,
				freq_data.mic_viddata[i].vid);
		}
	}
freeret:
	kfree(tempbuf);
	return result;
}

/**
 * mic_cpu_init_table - initialize the cpufreq table and the associated mic_data
 * @policy: current policy 
 *  Returns 0 on success, non zero on failure.   
 **/

static int mic_cpu_init_table(struct cpufreq_policy *policy)
{
	int device = freq_data.device;
	struct mic_freqvid seed[3];

	if (mic_get_freq_points(device, &seed[0]))
		return -EINVAL;

	printk("Generating table with PN(%x,%x) P1(%x,%x) P0(%x,%x)\n",
		seed[0].freq, seed[0].vid, seed[1].freq, seed[1].vid,
		seed[2].freq, seed[2].vid);
	/* Generate frequency table */
	if (mic_generate_freq_table(device, &seed[0]))
		return -EINVAL;

	thermalevent = 0;

	return 0;
}

/* Return the current CPU frequency in kHz */
static unsigned int get_cur_freq(unsigned int cpu)
{
	if (freq_data.mdev.get_core_freq)
		return (freq_data.mdev.get_core_freq());	/* Freq values returned in kHz  */
	else
		return 0;
}

static int set_cur_freq(int index)
{
	if (freq_data.mdev.set_core_freq)
		return (*freq_data.mdev.set_core_freq) (freq_data.
							freq_table[index].index,
							freq_data.
							mic_viddata[index].vid);
	else
		return -EINVAL;
}

/**
 * ras_freqtable - Report freq/vid information to RAS module.
 * @micpm_params - Data structure to report vaues in.
 *
 */
static int ras_freqtable(struct micpm_params *mp)
{
	int i, j = 0,k=0;

	if (mp) {
			j = min(mp->volt_siz, mp->freq_siz);
			*(mp->volt_len) = *(mp->freq_len) = 0;
			for (i=0,k=0;freq_data.freq_table[i].frequency != CPUFREQ_TABLE_END
					 && k < j;i++) {
				if(freq_data.freq_table[i].frequency !=  CPUFREQ_ENTRY_INVALID){ 
					mp->freq_lst[k] = freq_data.freq_table[i].frequency;
					mp->volt_lst[k] = VID_2_VOLTAGE(K1OM_VOLT_OFFSET,
					  		freq_data.mic_viddata[i].vid) * 1000;
					k++;
				}
			}
			*(mp->volt_len) = *(mp->freq_len) = k;
	}
	return 0;
}

extern void micpm_cpufreq_register(int (*)(struct micpm_params *));

static int mic_cpu_init(struct cpufreq_policy *policy)
{
	unsigned freq;

	int ret;
	struct mic_cpufreq_object tf;

	dprintk("mic_cpu_init called for cpu %d\n", smp_processor_id());
	dprintk("mic_cpu_init called.Trying to map MMIO space");
	if (!mic_sbox_mmio_va) {
		mic_sbox_mmio_va = ioremap(SBOX_BASE, MIC_SBOX_MMIO_SIZE);
		if (!mic_sbox_mmio_va) {
			printk("Failed to map SBOX MMIO space into kernel");
			return -ENODEV;
		}
	}
	printk("Generating cpu freq table\n");
	if (mic_cpu_init_table(policy)) {
		return -ENODEV;
	}
	ret = cpufreq_frequency_table_cpuinfo(policy, freq_data.freq_table);
	if (ret)
		return (ret);
	policy->cpu = POLICY_CPU;
	/* All cpus on the same policy */
	cpumask_copy(policy->cpus, cpu_online_mask);
	/* 10uS transition latency */
	policy->cpuinfo.transition_latency = 10000;
	lastvid=0;
	freq = get_cur_freq(policy->cpu);
	/*Take care of the case where the LFM (the freq the card drops to on PROCHOT)
	* is not the same as the Pn freq. We may boot the card with PROCHOT enabled
	* (HSD4846479) and if we just report the current freq as what we read then 
	* it can give rise to problems because the freq is not in the table (one 
	* problem is the cpufreq_stats module crashes and the boot fails).
	*/     
	if(freq < policy->cpuinfo.min_freq)
		freq=policy->cpuinfo.min_freq;
	policy->cur = freq;
	lastfreq=freq;
	printk("mic_cpu_init: current freq =%dkHz\n", policy->cur);
	if (knc_stepping != KNC_A_STEP) {	
		lastvid = get_cur_vid();
	} 
	if(lastvid == 0){
		lastvid=tdpvid;
	}
	
	tf.tinfo.tpoints=0;
	mic_init_turbo(freq_data.device, &tf.tinfo);
	tf.tdpfreq = tdpfreq;
	tf.rtccomp = do_RTCComp;
	micevent_cpufreq_register(&mic_cpufreq_atomic_notifier_block,
			&mic_cpufreq_blocking_notifier_block,&tf);
	micpm_cpufreq_register(ras_freqtable);
	dprintk("Setting policy max to tdpfreq: %d\n", tdpfreq);
	/* Set policy max to the TDP freq till turbo is turned ON       */
	policy->max = tdpfreq;
	maxcorefreq = maxcorefreq_saved = policy->cpuinfo.max_freq;
	/* Adjust PROCHOT freq so it is not lower than Pn*/
	if(prochotfrequency < policy->cpuinfo.min_freq)
		prochotfrequency=policy->cpuinfo.min_freq;
	cpufreq_frequency_table_get_attr(freq_data.freq_table, policy->cpu);
	printk("Returning from mic_cpu_init\n");
	return 0;
}

static int mic_cpu_exit(struct cpufreq_policy *policy)
{
	unsigned int cpu = policy->cpu;

	cpufreq_frequency_table_put_attr(cpu);

	return 0;
}

/**
 * mic_verify - verifies a new CPUFreq policy
 * @policy: new policy
 *
 * Limit must be within this model's frequency range at least one
 * border included.
 */
static int mic_verify(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, freq_data.freq_table);
}

/**
 *  mic_cpufreq_target- set a new CPUFreq 
  * @policy: cpufreq policy 
 * @target_freq: the target frequency
 * @relation: how that frequency relates to achieved frequency
 *	(CPUFREQ_RELATION_L or CPUFREQ_RELATION_H)
 *
 * Sets a new CPUFreq .
 */
static int mic_cpufreq_target(struct cpufreq_policy *policy,
			      unsigned int target_freq, unsigned int relation)
{
	unsigned int newstate = 0;
	struct cpufreq_freqs freqs;
	int retval = 0;
	unsigned int tmp;
	unsigned long flags;

	if(thermalevent){
			policy->cur = prochotfrequency;
			return -EINVAL;
	}
	dprintk("mic_cpufreq_target called\n");
	if (unlikely(cpufreq_frequency_table_target(policy,
						    freq_data.freq_table,
						    target_freq, relation,
						    &newstate))) {
		dprintk("Target not in table\n");
		retval = -EINVAL;
	}

	freqs.old = get_cur_freq(policy->cpu);
	freqs.new = freq_data.freq_table[newstate].frequency;

	if (freqs.old == freqs.new) {
		dprintk("Nothing to change.");
		retval = 0;
		goto out;
	}
	/* If an asynchronous freq throttle event is in progress, just quit. */
	if (asynchevent || freqs.new > maxcorefreq) {
		if (asynchevent)
			dprintk("Asynch throttle event in progress\n");
		else
			dprintk("Target freq greater than maxcorefreq = %d\n",
				maxcorefreq);

		retval = -EINVAL;
		goto out;
	}
/* We do notification only for the policy cpu. For KNx the # of cpus is large 
   and running a notification loop for each cpu can increase the latency of the
   core freq ops. Given that TSC is the only client right now and it is synchronized 
   on all cpus this should be OK. 
*/
processthermal:

	freqs.cpu = policy->cpu;
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	spin_lock_irqsave(&mic_cpufreq_lock, flags);
	if (!asynchevent && !thermalevent && (freqs.new <= maxcorefreq)) {
		dprintk("Setting freq code to %x\n",
			freq_data.freq_table[newstate].index);
		driverbusy = 1;
		smp_wmb();
		retval = set_cur_freq(newstate);
		if (!retval)
			policy->cur = freqs.new;
		else {
			if(!check_booted_bit()) /* See if we failed because
					a PROCHOT event just happened */
				freqs.old=prochotfrequency;
			driverbusy=0;
		}
		spin_unlock_irqrestore(&mic_cpufreq_lock, flags);
	
	} else {
		/* If we come here that means a asynch event happened 
		   after the PRECHANGE notification and the core freq 
		   got set to the maxfreq value. So make that the old
		   freq so when we unroll the notification it will be OK
		 */
		freqs.old = maxcorefreq;
		asynchevent = 0;
		spin_unlock_irqrestore(&mic_cpufreq_lock, flags);
		retval = -1;
	}
	if (unlikely(retval)) {
		/*
		 * We have failed halfway through the frequency change.
		 * We have sent callbacks to policy->cpus and
		 * Best effort undo..
		 */
		tmp = freqs.new;
		freqs.new = freqs.old;
		freqs.old = tmp;
		freqs.cpu = policy->cpu;
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
		dprintk("Rolling back freq change\n");
		goto out;
	}
	freqs.cpu = policy->cpu;
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	/* Check if we have had a asynch event meanwhile */
	spin_lock_irqsave(&mic_cpufreq_lock, flags);
	driverbusy = 0;
	smp_wmb();
	if (!asynchevent) {
		spin_unlock_irqrestore(&mic_cpufreq_lock, flags);
	} else {
		asynchevent = 0;
		if (unlikely(cpufreq_frequency_table_target(policy,
							    freq_data.
							    freq_table,
							    maxcorefreq,
							    CPUFREQ_RELATION_H,
							    &newstate))) {
			retval = -EINVAL;
			BUG_ON(retval);
		} else {
			freqs.old = get_cur_freq(policy->cpu);
			freqs.new = freq_data.freq_table[newstate].frequency;
			if (freqs.old == freqs.new) {
				dprintk("In thermal event. Nothing to change.");
				retval = 0;
			} else {
				spin_unlock_irqrestore(&mic_cpufreq_lock,
						       flags);
				goto processthermal;
			}
		}
		spin_unlock_irqrestore(&mic_cpufreq_lock, flags);
		goto out;
	}
	retval = 0;
out:
	dprintk("mic_cpufreq_target returning\n");
	return retval;
}

static int mic_cpufreq_atomic_notifier(struct notifier_block *nb, unsigned long event,
				void *data)
{

	unsigned int param = 0;
	unsigned long flags;
	struct clocksource *cs;

	dprintk("mic_cpufreq_atomic_notifier entered with event = %lu\n", event);
	cs=get_curr_clocksource();

	if (data)
		param = *((unsigned int *)data);
	switch (event) {
	case EVENT_ALERT:
		dprintk("Alert event \n");
		atomic_xchg(&alertevent, 1);
		break;
	case EVENT_PC3_ENTRY:
		/* During entry into PC3 this event is generated to reduce 
 		* core freq to the minimum. 
		*/
		dprintk("PC3 entry event\n");
		spin_lock_irqsave(&mic_cpufreq_lock, flags);
		/* If we are in the middle of a thermal/power event just 
		*  quit because we are already at the lowest freq.
		*/   	 
		if ((!thermalevent) && (maxcorefreq != prochotfrequency)) {
			maxcorefreq = prochotfrequency;
			dprintk("Changing maxcore freq to %d\n",
				maxcorefreq);
			asynch_throttle(maxcorefreq);
			
		}
		spin_unlock_irqrestore(&mic_cpufreq_lock,
					       flags);
		break;
	case EVENT_PC3_EXIT:
		/* During eixit from PC3 this event is generated to restore 
 		* core freq to the previous value. 
		*/
		dprintk("PC3 exit event\n");
		spin_lock_irqsave(&mic_cpufreq_lock, flags);
		/* If we are in the middle of a thermal/power event just 
		*  quit and let the freq be restored when the event goes away.
		*/   	 
		if (!thermalevent) {
			maxcorefreq = maxcorefreq_saved;
			dprintk("PC3 exit: reverting core freq to %d\n",
				lastfreq);
			asynch_throttle(lastfreq);
		}
		spin_unlock_irqrestore(&mic_cpufreq_lock,
					       flags);
		break;	
	case EVENT_PROCHOT_ON:
		/* This event can happen in interrupt context  */
		dprintk("PROCHOT ON event\n");
		spin_lock_irqsave(&mic_cpufreq_lock, flags);
		thermalevent = 1;
		maxcorefreq=prochotfrequency;
		spin_unlock_irqrestore(&mic_cpufreq_lock, flags);
		break;
	}
	return NOTIFY_DONE;
}

static int mic_cpufreq_blocking_notifier(struct notifier_block *nb, unsigned long event,
				void *data)
{
	unsigned long flags;
	struct cpufreq_freqs freqs;

	dprintk("mic_cpufreq_blocking_notifier entered with event = %lu\n", event);

	switch (event) {
	case EVENT_PROCHOT_VIDCHANGE:		/* Lower vid in response to PROCHOT */
		dprintk("PROCHOT vid change event\n");
		spin_lock_irqsave(&mic_cpufreq_lock, flags);
		if(thermalevent) { /* make sure we are still in PROCHOT */
			set_vid_k1om(prochotvid);
			/*Make the notification here. We could not do it in the PROCHOT_ON
 			* handler because we were in interrupt context.
 			*/  		 
			freqs.new = prochotfrequency;
			freqs.old = lastfreq;
			freqs.cpu = POLICY_CPU;	
			spin_unlock_irqrestore(&mic_cpufreq_lock, flags);
			cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
			cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
			break;	
		}
		spin_unlock_irqrestore(&mic_cpufreq_lock, flags);
		break;
	case EVENT_PROCHOT_OFF:
		dprintk("PROCHOT OFF event\n");
		spin_lock_irqsave(&mic_cpufreq_lock, flags);
		maxcorefreq = maxcorefreq_saved;
		freqs.old = prochotfrequency;
		freqs.new = lastfreq;
		freqs.cpu = POLICY_CPU;	
		smp_wmb();
		asynch_throttle(lastfreq);
		thermalevent = 0;
		smp_wmb();
		spin_unlock_irqrestore(&mic_cpufreq_lock, flags);
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
		break;
	}
	return NOTIFY_DONE;
}

static struct freq_attr *mic_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver mic_driver = {
	.name = "mic",
	.init = mic_cpu_init,
	.exit = mic_cpu_exit,
	.verify = mic_verify,
	.target = mic_cpufreq_target,
	.get = get_cur_freq,
	.attr = mic_attr,
	.owner = THIS_MODULE,
};

/**
 *mic_init - initializes the MIC CPUFreq driver
 *
 * Initializes the MIC CPUFreq  support. Returns -ENODEV on
 * unsupported devices and zero on success.
 *
 */
int mic_cpufreq_init(void)
{
	int retval;

	if ((boot_cpu_data.x86_vendor != X86_VENDOR_INTEL)
	    || (boot_cpu_data.x86 != MIC_FAMILY)) {
		printk("Not a MIC cpu device");
		return -ENODEV;
	}
	if (boot_cpu_data.x86_model == K1OM_MODEL) {
		freq_data.device = DEVICE_K1OM;
		freq_data.mdev.get_core_freq = get_cur_freq_k1om;
		freq_data.mdev.set_core_freq = set_cur_freq_k1om;
		knc_stepping = boot_cpu_data.x86_mask;
		printk("K1OM MIC device of stepping %d found\n", knc_stepping);
	} else {
		printk("Unsupported MIC device ");
		return -ENODEV;
	}
	spin_lock_init(&mic_cpufreq_lock);
	printk("Registering MIC cpufreq driver");
	retval = cpufreq_register_driver(&mic_driver);
	printk("Returned from cpufreq register with %d\n", retval);
	return 0;
}

void mic_cpufreq_exit(void)
{
	cpufreq_unregister_driver(&mic_driver);
}
