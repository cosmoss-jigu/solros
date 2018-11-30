#ifndef _PCNLINK_KAPI_H
#define _PCNLINK_KAPI_H
#include <pcnporting.h>
#include <list.h>
#include <scif.h>
#include <link.h>
#include <netmsg.h>
#include <ring_buffer.h>
#include <spsc_queue.h>
#include <pcnlink-epoll.h>

#ifdef PCIE_CLOUD_NETWORK_CONF_KERNEL
# include <net/sock.h>
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

/**
 * pcnlink context
 */
struct pcnlink_context_t {
	struct scif_portID pcnsrv;          /* pcnsrv node id and port */
	struct scif_portID pcnclnt;         /* pcnclient node id and port */
	int per_channel_tx_q_size;          /* per channel TX Q size */
	uint32_t num_channel;               /* number of channels */
	struct pcn_link_t link;             /* link to pcnsrv */
	int stop_rx_worker;                 /* stop rx worker */
#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
	pthread_t           rx_worker;      /* RX worker thread */
#else
	struct task_struct *rx_worker;      /* RX worker thread */
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */
};

/**
 * buffered rx q message from a chennel
 */
struct pcnlink_rx_netmsg_t {
	struct pcn_channel_t *channel;      /* channel of a netmsg */
	struct netmsg_header_t *netmsg;     /* netmsg in a channel */
	struct netmsg_header_t netmsg_hdr;  /* copy of netmsg header */
	struct spsc_node oob_link;          /* link for out-of-band list */
};

/**
 * internal socket
 */
struct pcnlink_last_recv_t {
	struct pcnlink_rx_netmsg_t rx_netmsg;/* last recv netmsg */
	struct netmsg_e_recvdata_t e_recvdata;
	uint32_t data_start;                /* data start index */
	int is_valid;                       /* is it valid? */
	int is_oob;                         /* is it oob? */
};

struct pcnlink_socket_t {
#ifdef PCIE_CLOUD_NETWORK_CONF_KERNEL
	/* !!! WARNING !!!
	 * 'sk' has to be the first of pcnlink_socket_t
	 * because some part of linux kernel assumes
	 * that sk is at the beginning. */
	struct sock sk;                     /* in-kernel sock structure */
#endif
	int __sos[0];                       /* {{ start of structure */
	struct pcn_channel_t *channel;      /* owner channel */
	uint64_t sockfd;		    /* Local sockfd */
        uint64_t sockid;                    /* sockid: socket of pcnsrv
					     * tag:    socket of pcnlink */
	int type;                           /* socket type */
	struct list_head list;              /* socket list for socktbls */
	struct ring_buffer_t *rx_r_q;       /* rpc response q (blocking) */
	struct ring_buffer_t *rx_e_q;       /* net event q (non-blocking) */
	struct spsc_queue oob_e_q;          /* out-of-band event qeueue */
	struct pcnlink_last_recv_t lastrcv; /* last E_RECVDATA information */

	unsigned int poll_mask;             /* mask for poll() */
	struct sockaddr_in sin;             /* socke address for getname */

	struct pcnlink_epoll *ep;	    /* the epoll data structure */

	uint32_t epoll;                     /* registered events */
	uint32_t events;                    /* available events */
	pcnlink_epoll_data_t ep_data;
	uint32_t __deinit_status;           /* for debugging */
#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
	int __file_f_flags;                 /* blocking or non-blocking */
#endif
	int __eos[0];                       /* }} end of structure */
};

#define plsock_memset_size(__plsock)  \
	((void *)((__plsock)->__eos) - (void *)((__plsock)->__sos))

/**
 * functions should be provided by integrator.
 */
struct pcnlink_socket_t *plsock_alloc(void *private1, void *private2);
void plsock_deinit(struct pcnlink_socket_t *plsock);
void plsock_data_ready(struct pcnlink_socket_t *plsock);

/**
 * pcnlink APIs
 */
struct pcnlink_socket_t *__pcnlink_socket(int domain, int type, int protocol);
struct pcnlink_socket_t *__pcnlink_socket_ex(int domain, int type, int protocol, int *prc, void *private1, void *private2);
int __pcnlink_bind(struct pcnlink_socket_t *plsock,
		   const struct sockaddr *addr, socklen_t addrlen);
int __pcnlink_connect(struct pcnlink_socket_t *plsock,
		      const struct sockaddr *addr, socklen_t addrlen);
int __pcnlink_listen(struct pcnlink_socket_t *plsock, int backlog);
int __pcnlink_setsockopt(struct pcnlink_socket_t *plsock, int level,
			 int option_name, const void *option_value,
			 socklen_t option_len);
int __pcnlink_getsockopt(struct pcnlink_socket_t *plsock, int level,
			 int optname, void *optval, socklen_t *optlen);
ssize_t __pcnlink_sendmsg(struct pcnlink_socket_t *plsock,
			  const struct msghdr *msg, ssize_t len);
ssize_t pcnlink_send(struct pcnlink_socket_t *plsock,
		     const void *buf, size_t len);
int __pcnlink_shutdown(struct pcnlink_socket_t *plsock, int how);
int __pcnlink_close(struct pcnlink_socket_t *plsock);
struct pcnlink_socket_t *__pcnlink_accept(
	struct pcnlink_socket_t *plsock,
	struct sockaddr *addr, socklen_t *addrlen);
struct pcnlink_socket_t *__pcnlink_accept_ex(
	struct pcnlink_socket_t *plsock,
	struct sockaddr *addr, socklen_t *addrlen, int *prc,
	void *private1, void *private2);
ssize_t __pcnlink_recv(struct pcnlink_socket_t *plsock,
		       void *buf, size_t len);
ssize_t __pcnlink_recvmsg(struct pcnlink_socket_t *plsock,
			  struct msghdr *msg, ssize_t len);
unsigned int __pcnlink_poll(struct pcnlink_socket_t *plsock);
int __pcnlink_writable(struct pcnlink_socket_t *plsock);
int __pcnlink_readable(struct pcnlink_socket_t *plsock);
#endif /* _PCNLINK_KAPI_H */
