/*
 * File: micras.h
 *
 * Derivative of micras.h from the RAS module.
 * Stripped down to just what is needed for the PM/RAS interactions.
 *
 * -- begin legalese --
 * Copyright (c) Intel Corporation (2011).
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
 * -- end legalese --
 */

#ifndef _MICRAS_H_
#define _MICRAS_H_	1

#if defined(CONFIG_MK1OM)
/*
 * Power management registration exchange records.
 * The RAS module populates a 'params' record and pass it to
 * the PM module through the micpm_ras_register() function.
 * In return the PM module populate the passed 'callback' record.
 * The PM module is responsible for populating the lists of
 * supported core frequencies and core voltages. In contrast to
 * KnF, where the lists reflect the hardware capabilities, these
 * reflect the actual frequencies and voltages that the core-freq
 * module can use to lower power consumption.
 *
 * Call-out function API:
 *
 *   int mt_call(int opcode, void * opague_struct);
 *	opcode		Index for MT operation
 *      opague_struct	Structure associated with opcode
 *   Returns count of bytes returned or 0 on success.
 *   Returns negative number on failures (and no data returned).
 *   Opcodes and associated structs are in micras_api.h and micmca_api.h
 *
 *   void mt_ttl(int who, int what);
 *	who		Source of throttle event 0=power 1=thermal
 *	what		New throttle state 0=off 1=on
 *   Can be called with any combination at any time, even from interrupt.
 */  

struct micpm_params {
  uint32_t    * freq_lst;		/* Core frequency list */
  uint32_t    * freq_len;		/* Core freq count */
  uint32_t	freq_siz;		/* Space in core freq list */
  uint32_t    * volt_lst;		/* Core voltage list */
  uint32_t    * volt_len;		/* Core voltage count */
  uint32_t	volt_siz;		/* Space in core volt list */ 
  int	     (* mt_call)(uint16_t, void *);  /* Access MT function */
  void       (* mt_ttl)(int, int);	     /* Throttle notifier */
};

struct micpm_callbacks {
  int  (*micpm_get_turbo)(void);	/* Get PM turbo setting */
  void (*micpm_set_turbo)(int);		/* Notify PM of new turbo setting */
  void (*micpm_vf_refresh)(void);	/* Refresh core V/F lists */
  int  (*micpm_get_pmcfg)(void);	/* Get PM operating mode */
};


/*
 * Bit locations for micpm_get_turbo() and micpm_set_turbo()
 */

#define MR_PM_MODE	(1 << 0)	/* Turbo mode */
#define MR_PM_STATE	(1 << 1)	/* Current turbo state */
#define MR_PM_AVAIL	(1 << 2)	/* Turbo mode available */


/*
 * Bit positions for the different features turned on/off 
 * in the uOS PM configuration. 
 */
#define	PMCFG_PSTATES_BIT	0
#define PMCFG_COREC6_BIT	1
#define PMCFG_PC3_BIT		2
#define PMCFG_PC6_BIT		3


/*
 * Register/Unregister functions that RAS calls during module init/exit.
 * Pointers to exchanged data structures are passed at registration.
 * The RAS module guarantee that the pointers are valid until
 * the unregister function is called. That way the PM module can
 * modify the core frequency/voltage lists if they gets changed.
 * The callbacks must always either be a valid function pointer
 * or a null pointer. 
 */

extern int	micpm_ras_register(struct micpm_callbacks *, struct micpm_params *);
extern void	micpm_ras_unregister(void);

#endif

#endif /* Recursion block */
