#undef TRACE_SYSTEM
#define TRACE_SYSTEM pcnlink

#if !defined(_TRACE_EVENT_PCNLINK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_PCNLINK_H

#include <linux/socket.h>
#include <linux/tracepoint.h>

/*trace event for pcnlink recvmsg*/
TRACE_EVENT(pcnlink_recvmsg_begin,

		TP_PROTO(struct socket *sock,
			struct msghdr *msg,
			size_t len),

		TP_ARGS(sock, msg, len),

		TP_STRUCT__entry(
			__field(	void *, sock)
			__field(	void *, msg)
			__field(	size_t, len)
			),

		TP_fast_assign(
			__entry->sock = sock;
			__entry->msg = msg;
			__entry->len = len;
			),

		TP_printk("begin_recvmsg sock=%p, msg=%p, size=%lu",
			__entry->sock, __entry->msg, __entry->len)
);

/*trace event for pcnlink recvmsg*/
TRACE_EVENT(pcnlink_recvmsg_end,

		TP_PROTO(struct socket *sock,
			struct msghdr *msg,
			size_t len),

		TP_ARGS(sock, msg, len),

		TP_STRUCT__entry(
			__field(	void *, sock)
			__field(	void *, msg)
			__field(	size_t, len)
			),

		TP_fast_assign(
			__entry->sock = sock;
			__entry->msg = msg;
			__entry->len = len;
			),

		TP_printk("end_recvmsg sock=%p, msg=%p, size=%lu",
			__entry->sock, __entry->msg, __entry->len)
);

/*trace event for pcnlink sendmsg*/
TRACE_EVENT(pcnlink_sendmsg_begin,

		TP_PROTO(struct socket *sock,
			struct msghdr *msg,
			size_t len),

		TP_ARGS(sock, msg, len),

		TP_STRUCT__entry(
			__field(	void *, sock)
			__field(	void *, msg)
			__field(	size_t, len)
			),

		TP_fast_assign(
			__entry->sock = sock;
			__entry->msg = msg;
			__entry->len = len;
			),

		TP_printk("begin_sendmsg sock=%p, msg=%p, size=%lu",
			__entry->sock, __entry->msg, __entry->len)
);

/*trace event for pcnlink sendmsg*/
TRACE_EVENT(pcnlink_sendmsg_end,

		TP_PROTO(struct socket *sock,
			struct msghdr *msg,
			size_t len),

		TP_ARGS(sock, msg, len),

		TP_STRUCT__entry(
			__field(	void *, sock)
			__field(	void *, msg)
			__field(	size_t, len)
			),

		TP_fast_assign(
			__entry->sock = sock;
			__entry->msg = msg;
			__entry->len = len;
			),

		TP_printk("end_sendmsg sock=%p, msg=%p, size=%lu",
			__entry->sock, __entry->msg, __entry->len)
);

#endif

/*default include,
 this should be outside*/
#include <trace/define_trace.h> 
