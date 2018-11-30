#undef TRACE_SYSTEM
#define TRACE_SYSTEM 9p

#if !defined(_TRACE_EVENT_9P_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_9P_H

#include <net/9p/client.h> /*for p9 client*/
#include <linux/tracepoint.h>

/*trace event for prb_process response*/
TRACE_EVENT(prb_request,

		TP_PROTO(struct p9_client *client,
			struct p9_req_t *req),

		TP_ARGS(client, req),

		TP_STRUCT__entry(
			__field(	void *, clientaddr)
			__field(	void *, req)
			),

		TP_fast_assign(
			__entry->clientaddr = client;
			__entry->req= req;
			),

		TP_printk("req clientaddr=%p, p9_req=%p",
			__entry->clientaddr, __entry->req)
);

/*trace event for prb_process response*/
TRACE_EVENT(prb_process_response,

		TP_PROTO(struct p9_client *client,
			struct p9_req_t *req),

		TP_ARGS(client, req),

		TP_STRUCT__entry(
			__field(	void *, clientaddr)
			__field(	void *, req)
			),

		TP_fast_assign(
			__entry->clientaddr = client;
			__entry->req= req;
			),

		TP_printk("response clientaddr=%p, p9_req=%p",
			__entry->clientaddr, __entry->req)
);

#define trace_9p_protocol_dump(...)
#define trace_9p_client_req(...)
#define trace_9p_client_res(...)

#endif

/*default include,
 this should be outside*/
#include <trace/define_trace.h> 
