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
/* ************************************************************************* *\
File Name: mic_pm_card.h
Description: contains common power management specific header defines for
			 host and card.
TODO: This file is a truncated version of the mic_pm.h file used by the host PM driver
and the pm_scif kernel module that loads after SCIF. Need to split the mic_pm.h file 
used by the host driver into this file and another file that includes all other host
specific stuff.  

\* ************************************************************************* */

#if !defined(__MIC_PM_CARD_H)
#define __MIC_PM_CARD_H

#define PM_MAJOR_VERSION 1
#define PM_MINOR_VERSION 0

typedef enum _PM_MESSAGE {
        PM_MESSAGE_PC3READY,    
        PM_MESSAGE_OPEN,        
        PM_MESSAGE_OPEN_ACK,    
        PM_MESSAGE_CLOSE,                       
        PM_MESSAGE_CLOSE_ACK,
        PM_MESSAGE_TEST,
        PM_MESSAGE_MAX, 
} PM_MESSAGE;

typedef enum _PM_IDLE_STATE {
        PM_IDLE_STATE_PC0,
        PM_IDLE_STATE_PC3_READY, 
        PM_IDLE_STATE_PC3,
        PM_IDLE_STATE_PC6,
        PM_IDLE_STATE_LOST,
        PM_IDLE_STATE_MAX,
} PM_IDLE_STATE;

//Generic PM Header. Has message type and length of message.
typedef struct _pm_msg_header {
	PM_MESSAGE opcode;
	uint32_t len;
} pm_msg_header;

typedef struct _pm_msg_pc3Ready
{
	//pm_msg_header header;
	uint64_t wake_up_time;
} pm_msg_pc3Ready;

typedef struct _pm_msg_latency_response
{
	//pm_msg_header header;
	uint64_t pc3_latency;
	uint64_t dpc3_latency;
	uint64_t pc6_latency;
} pm_msg_latency_response;

typedef struct _pm_msg_unit_test
{
	pm_msg_header header;
	void * buf;
} pm_msg_unit_test;

typedef struct _pm_version
{
	uint16_t major_version;
	uint16_t minor_version;
} pm_version;

typedef struct _pm_msg_pm_options
{
	uint8_t pc3_enabled;
	uint8_t pc6_enabled;
	pm_version version;
} pm_msg_pm_options;

typedef struct _pm_pc6_latency {
	uint64_t pc6_entry_latency;
	uint64_t pc6_residency;
	uint64_t pc6_exit_latency;
} pm_pc6_latency;

#endif //__MIC_PM_CARD_H
