#ifndef _LINK_H
#define _LINK_H
#include <list.h>
#include <ring_buffer_scif.h>
#include <pcnporting.h>

struct pcn_channel_t;
struct pcn_socktbl_t;

/**
 * network link between a host and a Xeon Phi
 * a network proxy maintains per-channel socket table
 */
struct pcn_link_t {
	int remote_id;                      /* host = 0, mic0 = 1, ... */
	int num_channel;                    /* number of channels */
	struct pcn_channel_t *channels;     /* communications channels */
	struct pcn_socktbl_t *socktbls;     /* per-channel socket table */
};

/**
 * a communication channel of a link,
 * which is associated with a network context
 * and composed of two ring buffers (i.e., in_q and out_q)
 */
struct pcn_channel_t {
	struct pcn_link_t *link;            /* owner link */
	int channel_id;                     /* channel id: [0, num_netctx) */
	uint64_t out_q_seqnum;              /* out_q sequence number */
	pcn_spinlock_t lock;                /* lock for in_q reaper */
	struct ring_buffer_scif_t in_q;     /* shadow ring buffer */
	struct ring_buffer_scif_t out_q;    /* master ring buffer */

	ssize_t oob_size ____cacheline_aligned; /* memory size of oob */
} ____cacheline_aligned;

#define PCN_QUEUE_NUM_PORTS     RING_BUFFER_SCIF_NUM_PORTS
#define PCN_CHANNEL_NUM_PORTS   (PCN_QUEUE_NUM_PORTS  * 2)

/**
 * socket type
 */
enum {
	PCN_SOCK_TYPE_NEW = 0,              /* just created by socket() */
	PCN_SOCK_TYPE_LISTEN,               /* listening socket: listen() */
	PCN_SOCK_TYPE_EPOLL,                /* epoll socket: epoll() */
	PCN_SOCK_TYPE_IN,                   /* inbound socket: accept() */
	PCN_SOCK_TYPE_OUT,                  /* outbound socket: connect() */

	PCN_SOCK_TYPE_MAX,                  /* end marker */
};

/**
 * a set of sockets which belong to a channel
 */
struct pcn_socktbl_t {
	pcn_spinlock_t lock;                /* lock to access socktbl */
	struct list_head types[             /* lists of sockets per type */
		PCN_SOCK_TYPE_MAX];
};

struct pcn_conn_info_t {
	int remote_id;                      /* host = 0, mic0 = 1, ... */
	int remote_port;                    /* remote port number */
	int local_port;                     /* local port number */
} __attribute__((__packed__));              /* it will be sent via scif */


/**
 * External APIs
 */
int  pcn_init_link(struct pcn_conn_info_t *conn_info,
		   struct conn_endp_t *endp,
		   int num_channel, size_t out_q_size,
		   ring_buffer_reap_cb_t in_q_cb,
		   struct pcn_link_t *link);
void pcn_deinit_link(struct pcn_link_t *link);

void pcn_socktbl_add_lock(struct pcn_socktbl_t *st, struct list_head *sk_list, int type);
void pcn_socktbl_del_lock(struct pcn_socktbl_t *st, struct list_head *sk_list);
void pcn_socktbl_move_lock(struct pcn_socktbl_t *st, struct list_head *sk_list, int type);
#endif /* _LINK_H */
