#undef TRACE_SYSTEM
#define TRACE_SYSTEM micveth_dma 

#if !defined(_TRACE_EVENT_MICVETH_DMA_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_MICVETH_DMA_H

#include <linux/tracepoint.h>

/*trace event for micveth recv complete */
TRACE_EVENT(recv_dma_complete,
		TP_PROTO(unsigned len),

		TP_ARGS(len),

		TP_STRUCT__entry(__field(	unsigned, len)),

		TP_fast_assign(__entry->len = len;),
		TP_printk("recv_dma_complete %u",
			__entry->len)
);

/*trace event for micveth send complete */
TRACE_EVENT(send_dma_complete,
		TP_PROTO(unsigned len),

		TP_ARGS(len),

		TP_STRUCT__entry(__field(	unsigned, len)),

		TP_fast_assign(__entry->len = len;),
		TP_printk("send_dma_complete %u",
			__entry->len)
);

#endif

/*default include,
 this should be outside*/
#include <trace/define_trace.h> 
