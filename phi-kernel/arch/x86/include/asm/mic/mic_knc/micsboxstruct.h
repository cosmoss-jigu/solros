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
/******************************************************************************
File Name: micsboxstruct.h
Description:  "Raw" register offsets & bit specifications for Intel MIC (KNC)
Notes:
    Auto gen sbox register header
*******************************************************************************/
#ifndef _MIC_SBOXSTRUCT_REGISTERS_H_
#define _MIC_SBOXSTRUCT_REGISTERS_H_

#include "../micreghelper.h"

/************************************************************************* 
    DMA Configuration Register 
*************************************************************************/
typedef union _sboxDcrReg
{
    U32 value;
    struct
    {
	U32 co0						   : 1; // bit 0 DMA Channel 0 Owner
	U32 ce0						   : 1; // bit 1 DMA Channel 0 Enable
	U32 co1						   : 1; // bit 2 DMA Channel 1 Owner
	U32 ce1						   : 1; // bit 3 DMA Channel 1 Enable
	U32 co2						   : 1; // bit 4 DMA Channel 2 Owner
	U32 ce2						   : 1; // bit 5 DMA Channel 2 Enable
	U32 co3						   : 1; // bit 6 DMA Channel 3 Owner
	U32 ce3						   : 1; // bit 7 DMA Channel 3 Enable
	U32 co4						   : 1; // bit 8 DMA Channel 4 Owner
	U32 ce4						   : 1; // bit 9 DMA Channel 4 Enable
	U32 co5						   : 1; // bit 10 DMA Channel 5 Owner
	U32 ce5						   : 1; // bit 11 DMA Channel 5 Enable
	U32 co6						   : 1; // bit 12 DMA Channel 6 Owner
	U32 ce6						   : 1; // bit 13 DMA Channel 6 Enable
	U32 co7						   : 1; // bit 14 DMA AES Endianess - 0b1 = Big Endian. 0b0 = Little Endian
	U32 ce7						   : 1; // bit 15 DMA Channel 7 Enable
	U32 arb_h					   : 8; // bit 16-23 Arb H
	U32 arb_l					   : 7; // bit 24-30 Arb L
	U32 p						   : 1; // bit 31 Priority EN
    } bits;
} sboxDcrReg;

STATIC_ASSERT(sizeof(sboxDcrReg) == sizeof(U32));

/************************************************************************* 
    Register that controls the interrupt response to thermal events 
*************************************************************************/
typedef union _sboxThermalInterruptEnableReg
{
    U32 value;
    struct
    {
	U32 high_temp_interrupt_enable			   : 1; // bit 0 This bit enables/disables the s/w programmed HIGH temp interrupt 
	U32 low_temp_interrupt_enable			   : 1; // bit 1 This bit enables/disables the s/w programmed  LOW temp interrupt 
	U32 out_of_spec_interrupt_enable		   : 1; // bit 2 This bit enables/disables the Out of Spec Temp interrupt 
	U32 fan_monitor_interrupt_enable		   : 1; // bit 3 This bit enables/disables the interrupt from Fan Control Unit	  
	U32 system_monitor_interrupt_enable		   : 1; // bit 4 This bit enables/disables the system hot interrupts 
	U32 mclk_ratio_interrupt_enable			   : 1; // bit 5 This bit enables/disables the mclk ratio interrupts 
	U32 alert_interrupt_enable			   : 1; // bit 6 This bit enables/disables the ALERT# interrupts 
	U32 gpuhot_interrupt_enable			   : 1; // bit 7 This bit enables/disables the GPUHOT# interrupts 
	U32 pwralert_interrupt_enable			   : 1; // bit 8 This bit enables/disables the PWRALERT# interrupt 
	U32 rsvd0					   : 1; // bit 9
	U32 sw_threshold1_temp				   :10; // bit 10-19 Software Programmable Thermal Threshold #1 
	U32 sw_threshold1_enable			   : 1; // bit 20 This bit enables/disables the sw threshold #1 interrupts 
	U32 sw_threshold2_temp				   :10; // bit 21-30 Software Programmable Thermal Threshold #2 
	U32 sw_threshold2_enable			   : 1; // bit 31 This bit enables/disables the sw threshold #2 interrupts 
    } bits;
} sboxThermalInterruptEnableReg;

STATIC_ASSERT(sizeof(sboxThermalInterruptEnableReg) == sizeof(U32));


/************************************************************************* 
    Status and Log info for all the thermal interrupts 
*************************************************************************/
typedef union _sboxThermalStatusReg
{
    U32 value;
    struct
    {
        U32 thermal_monitor_status                         : 1; // bit 0 This bit is set whenever the current die temp exceeds the thermal monitor control temperature 
        U32 thermal_monitor_log                            : 1; // bit 1 This bit is a sticky version of Thermal_Monitor_Status, cleared by s/w or by reset 
        U32 out_of_spec_status                             : 1; // bit 2 This bit is set whenever the current die temp exceeds the Out of Spec temperature; is sticky 
        U32 out_of_spec_log                                : 1; // bit 3 This bit is also a sticky version of Out_Of_Spec_Status 
        U32 thermal_threshold1_status                      : 1; // bit 4 This bit is set whenever the current die temp exceeds the software programmed thermal threshold
        U32 thermal_threshold1_log                         : 1; // bit 5 This bit is a sticky version of Thermal_Threshold1_Status, cleared by s/w or by reset 
        U32 thermal_threshold2_status                      : 1; // bit 6 This bit is set whenever the current die temp exceeds the software programmed thermal threshold
        U32 thermal_threshold2_log                         : 1; // bit 7 This bit is a sticky version of Thermal_Threshold2_Status, cleared by s/w or by reset 
        U32 fan_monitor_status                             : 1; // bit 8 This bit is an indication if there was an error/fail condition with the Off-chip Fan  
        U32 fan_monitor_log                                : 1; // bit 9 This bit is a sticky version of Fan_Monitor_Status, cleared by s/w or by reset 
        U32 system_hot_status                              : 1; // bit 10 This bit is an indication if any other device on the MIC Card is hot 
        U32 system_hot_log                                 : 1; // bit 11 This bit is a sticky version of System_Hot_Status, cleared by s/w or by reset 
        U32 rsvd0                                          :20; // bit 12-31
    } bits;
} sboxThermalStatusReg;

STATIC_ASSERT(sizeof(sboxThermalStatusReg) == sizeof(U32));


/************************************************************************* 
    "Elapsed Time Clock" Timer - lower 32 bits 
*************************************************************************/
typedef union _sboxElapsedTimeLowReg
{
    U32 value;
    struct
    {
	U32 elapsed_time_low				   :32; // bit 0-31 "Elapsed Time Clock" Timer - lower 32 bits 
    } bits;
} sboxElapsedTimeLowReg;

STATIC_ASSERT(sizeof(sboxElapsedTimeLowReg) == sizeof(U32));
/************************************************************************* 
    "Elapsed Time Clock" Timer - higher 32 bits 
*************************************************************************/
typedef union _sboxElapsedTimeHighReg
{
    U32 value;
    struct
    {
	U32 elapsed_time_high				   :32; // bit 0-31 "Elapsed Time Clock" Timer - higher 32 bits 
    } bits;
} sboxElapsedTimeHighReg;

STATIC_ASSERT(sizeof(sboxElapsedTimeHighReg) == sizeof(U32));
/************************************************************************* 
    Status and Log info for KNC new thermal interrupts
*************************************************************************/
typedef union _sboxThermalStatusInterruptReg
{
    U32 value;
    struct
    {
	U32 mclk_ratio_status				   : 1; // bit 0 This bit is set whenever MCLK Ratio Changes. Cleared by SW writing. 
	U32 mclk_ratio_log				   : 1; // bit 1 This bit is also a sticky version of MCLK_Ratio_Status 
	U32 alert_status				   : 1; // bit 2 This bit is set whenever ALERT# pin is asserted. Cleared by SW writing. 
	U32 alert_log					   : 1; // bit 3 This bit is a sticky version of Alert_Status, cleared by s/w or by reset 
	U32 gpuhot_status				   : 1; // bit 4 This bit reflects the real-time value of the GPUHOT# pin (synchronized to SCLK domain). 
	U32 gpuhot_log					   : 1; // bit 5 This bit is set on the assertion edge of GPUHOT# and remains set until software clears it by doing a write. 
	U32 pwralert_status				   : 1; // bit 6 This bit reflects the real-time value of the PWRALERT# pin (synchronized to SCLK domain). 
	U32 pwralert_log				   : 1; // bit 7 This bit is set on the assertion edge of PWRALERT# and remains set until software clears it by doing a write. 
	U32 rsvd0					   :23; // bit 8-30
	U32 etc_freeze					   : 1; // bit 31 This bit freeze the increment of elapsed-time counter 
    } bits;
} sboxThermalStatusInterruptReg;

STATIC_ASSERT(sizeof(sboxThermalStatusInterruptReg) == sizeof(U32));

/************************************************************************* 
    Consists of Current Die Temperatures of sensors 0 thru 2 
*************************************************************************/
typedef union _sboxCurrentDieTemp0Reg
{
    U32 value;
    struct
    {
        U32 sensor0_temp                                   :10; // bit 0-9 current Temperature of Sensor0
        U32 sensor1_temp                                   :10; // bit 10-19 current Temperature of Sensor1
        U32 sensor2_temp                                   :10; // bit 20-29 current Temperature of Sensor2
        U32 rsvd0                                          : 2; // bit 30-31
    } bits;
} sboxCurrentDieTemp0Reg;

STATIC_ASSERT(sizeof(sboxCurrentDieTemp0Reg) == sizeof(U32));

/************************************************************************* 
    Configuration CSR for GPU HOT 
*************************************************************************/
typedef union _sboxGpuHotConfigReg
{
    U32 value;
    struct
    {
        U32 enable_freq_throttle                           : 1; // bit 0 Enables Frequency Throttling initiated by someone outside MIC 
        U32 xxgpuhot_enable                                : 1; // bit 1 Enable MIC Assertion of XXGPUHOT 
        U32 rsvd0                                          :30; // bit 2-31
    } bits;
} sboxGpuHotConfigReg;

STATIC_ASSERT(sizeof(sboxGpuHotConfigReg) == sizeof(U32));

/************************************************************************* 
    System Interrupt Cause Read Register 0 
*************************************************************************/
typedef union _sboxSicr0Reg
{
    U32 value;
    struct
    {
	U32 dbr						   : 5; // bit 0-4 This bit is set whenever the uOS requests an interrupt service from the host driver thru one of the four doorbells 
	U32 rsvd0					   : 3; // bit 5-7
	U32 dma						   : 8; // bit 8-15 DMA Channels 7:0 - This bit is set whenever the uOS or the host driver configures the DMA engine to issue an interrupt upon a DMA Transfer completion on one of the channels
	U32 rsvd1					   : 8; // bit 16-23
	U32 disprra					   : 1; // bit 24 Alloocation bit for Display Rendering Event Interrupt A
	U32 disprrb					   : 1; // bit 25 Alloocation bit for Display Rendering Event Interrupt B
	U32 dispnrr					   : 1; // bit 26 Display Non-Rendering Event Interrupt - This bit is set whenever the Dbox detects a non-rendering interrupt condition, which are display pipeline errors i.e. overrun and underruns and hot-plug detect. 
	U32 rsvd2					   : 3; // bit 27-29
	U32 sboxerr					   : 1; // bit 30 LEP Error interrupts like the PCU
	U32 spidone					   : 1; // bit 31 SPI Done Interrupt - This bit is set when the Sbox SPI controller is done with it's programmming operaiton
    } bits;
} sboxSicr0Reg;

STATIC_ASSERT(sizeof(sboxSicr0Reg) == sizeof(U32));


/************************************************************************* 
    The expected MCLK Ratio that is sent to the corepll 
*************************************************************************/
typedef union _sboxCurrentratioReg
{
    U32 value;
    struct
    {
	U32 mclkratio					   :12; // bit 0-11 This field contrains the actual MCLK ratio that the core is currently running at.  This may differ from the value programmed into the COREFREQ register due to thermal throttling or other events which force the MCLK ratio to a fixed value.
	U32 rsvd0					   : 4; // bit 12-15
	U32 goalratio					   :12; // bit 16-27 This field contrains the goal for the MCLK ratio..	 This may differ from the value programmed into the COREFREQ register due to thermal throttling or other events which force the MCLK ratio to a fixed value.  reset value needs to match corefreq.ratio RTL will use the corefreq.ratio reset value
	U32 rsvd1					   : 4; // bit 28-31
    } bits;
} sboxCurrentratioReg;

STATIC_ASSERT(sizeof(sboxCurrentratioReg) == sizeof(U32));




/************************************************************************* 
    Core Frequency 
*************************************************************************/
typedef union _sboxCorefreqReg
{
    U32 value;
    struct
    {
	U32 ratio					   :12; // bit 0-11 Ratio
	U32 rsvd0					   : 3; // bit 12-14
	U32 fuseratio					   : 1; // bit 15 If overclocking is enabled, setting this bit will default the goal ratio to the fuse value.
	U32 asyncmode					   : 1; // bit 16 Async Mode Bit 16, Reserved Bits 20:17 used to be ExtClkFreq, 
	U32 rsvd1					   : 9; // bit 17-25
	U32 ratiostep					   : 4; // bit 26-29 Power throttle ratio-step
	U32 jumpratio					   : 1; // bit 30 Power throttle jump at once
	U32 booted					   : 1; // bit 31 Booted: This bit selects between the default MCLK Ratio (600MHz) and the programmable MCLK ratio. 0=default 1=programmable.
    } bits;
} sboxCorefreqReg;

STATIC_ASSERT(sizeof(sboxCorefreqReg) == sizeof(U32));
/************************************************************************* 
    Core Voltage 
*************************************************************************/
typedef union _sboxCorevoltReg
{
    U32 value;
    struct
    {
	U32 vid						   : 8; // bit 0-7 VID
	U32 rsvd0					   :24; // bit 8-31
    } bits;
} sboxCorevoltReg;

STATIC_ASSERT(sizeof(sboxCorevoltReg) == sizeof(U32));


/************************************************************************* 
    SVID VR12/MVP7 Control Interace Register 
*************************************************************************/
typedef union _sboxSvidcontrolReg
{
    U32 value;
    struct
    {
	U32 svid_dout					   : 9; // bit 0-8 SVID DATA OUT Field
	U32 svid_cmd					   : 9; // bit 9-17 SVID Command Field
	U32 svid_din					   :11; // bit 18-28 SVID DATA IN Field
	U32 svid_error					   : 1; // bit 29 error indicator bit
	U32 svid_idle					   : 1; // bit 30 svid idle
	U32 cmd_start					   : 1; // bit 31 cmd start
    } bits;
} sboxSvidcontrolReg;

STATIC_ASSERT(sizeof(sboxSvidcontrolReg) == sizeof(U32));
/************************************************************************* 
    Power Control Unit Register 
*************************************************************************/
typedef union _sboxPcucontrolReg
{
    U32 value;
    struct
    {
	U32 enablemclkpllshutdown			   : 1; // bit 0 This bit is set by Host OS to allow disabling of MCLK PLL to enter C3-state.  It is cleared by the Host OS to prevent the MCLK PLL from being shutdown by the PCU.  MCLK PLL will only be disabled if this bit is set AND all internal idle requirements have been met. 0=Disable MCLK Shutdown, 1-Allow MCLK Shutdown
	U32 mclk_enabled				   : 1; // bit 1 (read-only)This bit reflects the state of the PCU MCLK-Enable FSM.  It can be used as explicit feedback by SW to verify that successfully shutdown the MCLK PLL. 0=FSM not in MCLK_ENABLED state and MCLK is off.  1=FSM is in MCLK_ENABLED state and MCLK is running.
	U32 ring_active					   : 1; // bit 2 (read-only)This bit reflects the state of the ring-idle indicator coming from the Ring Box.  0=All Agents Idle, 1=At least one agent active
	U32 preventautoc3exit				   : 1; // bit 3 This bit is set by Host SW whenever it sets the VID value lower than it was when KNC entered Pkg-C3.	When set, any ring-bound PCIe transactions received by KNC will be completed without actually being sent to KNC ring, and an interrupt can be generated to the Host indicating this as an error condition.  This bit must be cleared for KNC to re-enable MCLK and exit C3. 0=Allow C3 Exit, 1=Prevent C3 Exit, Generate Error
	U32 ghost_active				   : 1; // bit 4 (read-only)This bit reflects the state of the Ghost Downstream Arbiter.  0=Idle, 1=At least one agent active
	U32 tcu_active					   : 1; // bit 5 (read-only) This bit reflects the state of the TCU(SBQ or DMA).  0=Idle, 1=At least one agent active
	U32 itp_sclkgatedisable				   : 1; // bit 6 (read-only)Status of the ITP sClk clock-gate disable, 1=disabled 
	U32 itp_pkgc3disable				   : 1; // bit 7 (read-only) Status of the ITP Pkg-C3 clock-gate disable, 1=disabled
	U32 c3mclkgateen				   : 1; // bit 8 This bit, when set, will cause the MCLK to be gated at each individual Ring Agent when KNC is in C3.	It should be set only when the EnableMclkPllShutdown (bit 0) is not set.  Disabling MCLK when in C3 can only occur when the correct idle conditions are true and this or EnableMclkPllShutdown fields are set.	1=Gate Mclk, 0=Do Not Gate MCLK
	U32 c3waketimer_en				   : 1; // bit 9 This bit, when set, loads the C3WakeUp timer with the 16-bit C3WakeTime field and enables the C3WakeUp timer to begin decrementing  while in AutoC3.  Clearing the disables the timer.	 HW will automatically clear this bit when the timer reaches zero or if AutoC3 exits. Writing a value of 0xFFFF to the C3WakeTimer will also clear this bit.
	U32 sysint_active				   : 1; // bit 10 (read-only) This bit reflects the state of the SYSINT.  0=Idle, 1=At least one pending interrupt
	U32 sclk_grid_off_disable			   : 1; // bit 11 Setting this bit will prevent the PCU logic from instructing the sclk PLL from gating the clock.
	U32 icc_dvo_ssc_cg_enable			   : 1; // bit 12 Clock gate:: ICC logic related to DVO SSC buffer for ssc clock for d2d 1=clock gated
	U32 icc_core_ref_clock_cg_enable		   : 1; // bit 13 Clock gate: ICC logic related to Core Reference clock:  1=clock gated 
	U32 icc_gddr_ssc_cg_enable			   : 1; // bit 14 Clock Gate: ICC Logic relted to GDDR SSC buffer 1=clock gated
	U32 icc_pll_disable				   : 1; // bit 15 This bit can be set by Host SW to shutdown the ICC PLL.  This should only be done once MCLK PLL is already shutdown.	This bit must be cleared at least 50us before reenabling MCLK.
	U32 mclk_pll_lock				   : 1; // bit 16 This bit contains the value of hte actual MCLK PLL lock bit. Primarily for debug.  1=locked
	U32 groupb_pwrgood_mask				   : 1; // bit 17 Clock Gate: ICC Logic relted to GDDR SSC buffer 1=clock gated
	U32 rsvd0					   :14; // bit 18-31
    } bits;
} sboxPcucontrolReg;

STATIC_ASSERT(sizeof(sboxPcucontrolReg) == sizeof(U32));
/************************************************************************* 
    Host PM scratch registers 
*************************************************************************/
typedef union _sboxHostpmstateReg
{
    U32 value;
    struct
    {
	U32 host_pm_status				   : 7; // bit 0-6 This 7-bit value is writeable/readable by Host to communicate any PM information that the uOS might need to know.
	U32 abort_not_processed				   : 1;	// bit 8. This bit is used by the Host to tell the card that it has processed the last abort message it received.
	U32 minvid					   : 8; // bit 8-15 This 8-bit value is readable by Host to communicate what the fuse-programmed minimum VID that indicates the minimum VID for reliable operation at min MCLK Ratio.
	U32 tdp_vid					   : 8; // bit 16-23 This 8-bit value is readable by SW and represents the fuse-programmed nominal VID for TDP operating point.	 Will be used by Host SW for restoring VID to a good point for C6-Exit.
	U32 rsvd0					   : 8; // bit 24-31
    } bits;
} sboxHostpmstateReg;

STATIC_ASSERT(sizeof(sboxHostpmstateReg) == sizeof(U32));
/************************************************************************* 
    uOS PM Scratch registers 
*************************************************************************/
typedef union _sboxUospmstateReg
{
    U32 value;
    struct
    {
	U32 uos_pm_status				   : 8; // bit 0-7 This 7-bit value is writeable/readable by uOS to communicate any PM information that the Host might need to know.
	U32 rsvd0					   :24; // bit 8-31
    } bits;
} sboxUospmstateReg;

STATIC_ASSERT(sizeof(sboxUospmstateReg) == sizeof(U32));
/************************************************************************* 
    C3 WakeUp Timer Control for autoC3 
*************************************************************************/
typedef union _sboxC3wakeupTimerReg
{
    U32 value;
    struct
    {
	U32 c3waketime					   :16; // bit 0-15 This 16-bit field represents the load value, in ~31.25us granularity, of the C3WakeUp timer.  This value must be non-zero, and the enable bit must be set(1) for the timer to begin counting down. Values of 0xFFFF will essentially disable the timer along with C3WakeTimer_En.
	U32 rsvd0					   : 1; // bit 16
	U32 c3wake_timeout				   : 1; // bit 17 When this read-only bit is set, it means that the timer has counted down and reached 0.  Once this bit is set, it can only be cleared by reloading and restarting the timer.	While this bit is set, KNC cannot enter C3.  If PcuControl.PreventAutoC3 bit is set, for example Deeper C3, this field will never assert. 
	U32 rsvd1					   :14; // bit 18-31
    } bits;
} sboxC3wakeupTimerReg;

STATIC_ASSERT(sizeof(sboxC3wakeupTimerReg) == sizeof(U32));

/************************************************************************* 
    C3 Entry and Exit Timers 
*************************************************************************/
typedef union _sboxC3TimersReg
{
    U32 value;
    struct
    {
	U32 c3_entry_timer				   : 8; // bit 0-7 Programmable 8-bit timer with a 1us granularity which blocks C3 entry.  Value of 0=1us
	U32 rsvd0					   : 8; // bit 8-15
	U32 c3_exit_timer				   : 3; // bit 16-18 Programmable 8-bit timer with a 4us granularity which blocks C3 entry.  Value of 0=4us
	U32 rsvd1					   :13; // bit 19-31
    } bits;
} sboxC3TimersReg;

STATIC_ASSERT(sizeof(sboxC3TimersReg) == sizeof(U32));
/************************************************************************* 
    uOS PCU Control CSR.. i.e. not for host consumption 
*************************************************************************/
typedef union _sboxUosPcucontrolReg
{
    U32 value;
    struct
    {
	U32 rsvd0					   : 2; // bit 0-1
	U32 spi_clk_disable				   : 1; // bit 2 Clock gate: This bit shutsdown the SPI Clock output to Flash component. 1=SPI Flash Clock Off
	U32 rsvd1					   :29; // bit 3-31
    } bits;
} sboxUosPcucontrolReg;

STATIC_ASSERT(sizeof(sboxUosPcucontrolReg) == sizeof(U32));

/************************************************************************* 
    EMON Counter 0 
*************************************************************************/
typedef union _sboxEmonCounter0Reg
{
    U32 value;
    struct
    {
	U32 emon_counter0				   :32; // bit 0-31 EMON counter 0
    } bits;
} sboxEmonCounter0Reg;

STATIC_ASSERT(sizeof(sboxEmonCounter0Reg) == sizeof(U32));

/************************************************************************* 
    Scratch Pad registers for package-C6 
*************************************************************************/
typedef union _sboxC6ScratchReg
{
    U32 value;
    struct
    {
	U32 pad						   :32; // bit 0-31 Scratch Pad bits
    } bits;
} sboxC6ScratchReg;

/************************************************************************* 
     
*************************************************************************/
typedef union _sboxPcieVendorIdDeviceIdReg
{
    U32 value;
    struct
    {
        U32 vendor_id                                      :16; // bit 0-15 
        U32 device_id                                      :16; // bit 16-31 
    } bits;
} sboxPcieVendorIdDeviceIdReg;

STATIC_ASSERT(sizeof(sboxPcieVendorIdDeviceIdReg) == sizeof(U32));

#endif
