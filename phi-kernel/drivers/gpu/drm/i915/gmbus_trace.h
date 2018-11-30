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

#if !defined(_GMBUS_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _GMBUS_TRACE_H_

#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM gmbus
#define TRACE_INCLUDE_FILE gmbus_trace

/* object tracking */

TRACE_EVENT(gmbus_reg_rw,
           TP_PROTO(bool write, u32 reg, u64 val, int len),

           TP_ARGS(write, reg, val, len),

           TP_STRUCT__entry(
                   __field(u64, val)
                   __field(u32, reg)
                   __field(u16, write)
                   __field(u16, len)
                   ),

           TP_fast_assign(
                   __entry->val = (u64)val;
                   __entry->reg = reg;
                   __entry->write = write;
                   __entry->len = len;
                   ),

           TP_printk("%s reg=0x%x, len=%d, val=(0x%x, 0x%x)",
                     __entry->write ? "write" : "read",
		     __entry->reg, __entry->len,
		     (u32)(__entry->val & 0xffffffff),
		     (u32)(__entry->val >> 32))
);

#endif /* _GMBUS_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/gpu/drm/i915
#include <trace/define_trace.h>
