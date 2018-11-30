#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pcnporting.h>
#include "proxy_i.h"
#include <mtcp_api.h>
#include <signal.h>

void proxy_signal_handler (int signum)
{
        int i;

	// do nothing now
	return;
}

int pcn_init_proxy(int listening_port, int num_netctx,
		   int base_link_port, size_t rx_q_size,
		   struct pcn_proxy_t *proxy)
{
	int i;
	int rc = 0;
	struct mtcp_conf mcfg;

	/* init */
	memset(proxy, 0, sizeof(*proxy));
	proxy->num_netctx     = num_netctx;
	proxy->base_link_port = base_link_port;
	proxy->rx_q_size      = rx_q_size;

	/* prepare a listening endpoint */
	proxy->server.epd = scif_open();
	if (proxy->server.epd == SCIF_OPEN_FAILED) {
		rc = -errno;
		goto err_out;
	}
	rc = scif_bind(proxy->server.epd, listening_port);
	if (rc < 0) {
		rc = -errno;
		goto err_out;
	}
	rc = scif_listen(proxy->server.epd, PCN_PROXY_MAX_LINKS);
	if (rc < 0) {
		rc = -errno;
		goto err_out;
	}

	/* init network contexts */
	proxy->netctxs = calloc(proxy->num_netctx,
				sizeof(*proxy->netctxs));
	if (!proxy->netctxs) {
		rc = -ENOMEM;
		goto err_out;
	}

	/* set the core limit in MTCP */
	mtcp_getconf(&mcfg);
	mcfg.num_cores = proxy->num_netctx;
	mtcp_setconf(&mcfg);

        /* initialize mtcp */
        rc = mtcp_init("epserver.conf");
        if (rc) {
                rc = -ENOMEM;
                goto err_out;
        }

        /* register signal handler to mtcp */
        mtcp_register_signal(SIGINT, proxy_signal_handler);

	smp_wmb();
	for (i = 0; i < proxy->num_netctx; ++i) {
		rc = _pcn_init_netctx(proxy, i, &proxy->netctxs[i]);
		if (rc)
			goto err_out;
	}

	/* kick all network context workers */
	for (i = 0; i < proxy->num_netctx; ++i) {
		proxy->netctxs[i].status =
			PCN_NETCTX_STATUS_RUNNABLE;
	}
	smp_wmb();

	return 0;
err_out:
	pcn_deinit_proxy(proxy);
	return rc;
}

void pcn_deinit_proxy(struct pcn_proxy_t *proxy)
{
	int i;

	/* sanity check */
	if (!proxy)
		return;

	/* close the listening endpoint */
	if (proxy->server.epd != SCIF_OPEN_FAILED) {
		scif_close(proxy->server.epd);
		proxy->server.epd = SCIF_OPEN_FAILED;
	}

	/* deinit network contexts */
	if (proxy->netctxs) {
		/* ask network context workers to stop */
		for (i = 0; i < proxy->num_netctx; ++i)
			proxy->netctxs[i].status = PCN_NETCTX_STATUS_STOPPED;
		smp_wmb();

		/* deinit network contexts */
		for (i = 0; i < proxy->num_netctx; ++i)
			_pcn_deinit_netctx(&proxy->netctxs[i]);

		/* clean up network contexts */
		free(proxy->netctxs);
		proxy->netctxs = NULL;
	}

	/* deinit links */
	if (proxy->num_link > 0) {
		for (i = 0; i < PCN_PROXY_MAX_LINKS; ++i)
			pcn_deinit_link(&proxy->links[i]);
		proxy->num_link = 0;
	}

	/* clean proxy */
	memset(proxy, 0, sizeof(*proxy));
}

int pcn_proxy_accept_link(struct pcn_proxy_t *proxy)
{
	uint32_t num_channel = proxy->num_netctx;
	int link_num_ports = proxy->num_netctx * PCN_CHANNEL_NUM_PORTS;
	struct conn_endp_t client = {.__dummy = NULL};
	struct pcn_conn_info_t ci;
	struct scif_portID peer;
	struct pcn_link_t *link;
	uint32_t ack = 0;
	int i, j;
	int rc = 0;

	/* accept a client scif connection */
	rc = scif_accept(proxy->server.epd, &peer,
			 &client.epd, SCIF_ACCEPT_SYNC);
	if (rc < 0) {
		rc = -errno;
		goto err_out;
	}

	/* send my number of channels (i.e., # of netctx) */
	rc = scif_send(client.epd, &num_channel,
		       sizeof(num_channel), SCIF_SEND_BLOCK);
	if (rc < 0) {
		rc = -errno;
		goto err_out;
	}

	/* receive connection information */
	rc = scif_recv(client.epd, &ci, sizeof(ci), SCIF_RECV_BLOCK);
	if (rc < 0) {
		rc = -errno;
		goto err_out;
	}

	/* init a link */
	link = &proxy->links[ci.remote_id];
	rc = pcn_init_link(&ci, &client, proxy->num_netctx,
			   proxy->rx_q_size, NULL, link);
	if (rc)
		goto err_out;
	++proxy->num_link;

	/* make the new link public to network contexts */
	for (i = 0; i < proxy->num_netctx; ++i) {
		struct pcn_netctx_t *nx = &proxy->netctxs[i];
		nx->pchannels[ci.remote_id] = &link->channels[i];
		nx->psocktbls[ci.remote_id] = &link->socktbls[i];
		smp_wmb_tso();
		++nx->num_link;
	}
	smp_wmb();

	/* send ack */
	rc = scif_send(client.epd, &ack, sizeof(ack), SCIF_SEND_BLOCK);
	if (rc < 0) {
		rc = -errno;
		goto err_out;
	}

	/* close a client scif connection */
	scif_close(client.epd);
	client.__dummy = NULL;
	return 0;
err_out:
	if (client.__dummy)
		scif_close(client.epd);
	return rc;
}

static
int _pcn_init_netctx(struct pcn_proxy_t *proxy, int netctx_id,
		     struct pcn_netctx_t *netctx)
{
	int rc = 0;

	/* init */
	memset(netctx, 0, sizeof(*netctx));
	netctx->proxy = proxy;
	netctx->netctx_id = netctx_id;

	/* assume core-ids are sequential */
	netctx->core = netctx_id;

	/* XXX: allocate resources (e.g., mtcp_context) */

	/* create and launch per-netctx thread */
	rc = pthread_create(&netctx->worker,
			    NULL,
			    _pcn_netctx_worker,
			    netctx);
	if (rc)
		goto err_out;

	return 0;
err_out:
	_pcn_deinit_netctx(netctx);
	return rc;
}

static
void _pcn_deinit_netctx(struct pcn_netctx_t *netctx)
{
	/* sanity check */
	if (!netctx)
		return;

	/* stop the worker */
	if (netctx->worker) {
		netctx->status = PCN_NETCTX_STATUS_STOPPED;
		pthread_join(netctx->worker, NULL);
		memset(&netctx->worker, 0, sizeof(netctx->worker));
	}

	/* release the scractchpad */
	free(netctx->scratchpad);
	netctx->scratchpad = NULL;

	/* clean netctx */
	memset(netctx, 0, sizeof(*netctx));
}

static
void *_pcn_netctx_worker(void *_p)
{
	struct pcn_netctx_t *netctx = _p;
	int rc = 0;

	/* pinning the thread */
	proxy_core_affinitize(netctx->core);

	/* Create mtcp_context */
	netctx->mctx = mtcp_create_context(netctx->core);
	if (!netctx->mctx) {
		rc = -ENOMEM;
		goto err_out;
	}

	/* to put scratchpad memory to thread's local memory,
	 * we allocate it here. */
	if (netctx->scratchpad) {
		netctx->scratchpad_size = PCN_NETCTX_SCRATCHPAD_INIT_SIZE;
		netctx->scratchpad = malloc(netctx->scratchpad_size);
		if (!netctx->scratchpad) {
			rc = -ENOMEM;
			goto err_out;
		}
	}

	netctx->mevents = (struct mtcp_epoll_event *) calloc(PCN_MAX_EVENTS,
                                   sizeof(struct mtcp_epoll_event));
	if (!netctx->mevents) {
		rc = -ENOMEM;
		goto err_out;
	}

	/* wait for a start signal */
	smp_rmb();
	while (netctx->status == PCN_NETCTX_STATUS_UNINIT) {
		pcn_yield();
	}

	/* create the epoll fd */
	netctx->epoll_fd = _pcn_epoll_create(netctx->mctx, PCN_MAX_EVENTS);
	if (netctx->epoll_fd == -1) {
		/* exit if we can't get epoll_fd */
		rc = -errno;
		goto err_out;
	}

	/* run until stopped */
	while (netctx->status == PCN_NETCTX_STATUS_RUNNABLE) {
		/* one sweep of channels and sockets */
		rc = _pcn_netctx_process(netctx);
		if (rc < 0)
			goto err_out;

		/* if none are active, yield */
		if (rc == 0)
			pcn_yield();
	}
	return (void *)0;
err_out:
	return (void *)rc;
}

static
int _pcn_netctx_process(struct pcn_netctx_t *netctx)
{
	int num_active;
	int rc = 0;

	/* sweep channels to process
	 * incoming requests from Xeon Phi */
	rc = _pcn_netctx_process_channels(netctx);
	if (rc < 0)
		goto err_out;
	num_active = rc;

	if (netctx->num_listen) {
		/* sweep host sockets to process
	 	 * incoming network event from outside  */
		rc = _pcn_netctx_process_sockets(netctx);
		if (rc < 0)
			goto err_out;
		num_active += rc;
	}

	return num_active;
err_out:
	return rc;
}

static
int _pcn_netctx_process_channels(struct pcn_netctx_t *netctx)
{
	struct pcn_channel_t **pchannels = netctx->pchannels;
	int num_link = netctx->num_link;
	int i, num_active_channels = 0;
	int rc = 0;

	/* process a network message from each channel */
	for (i = 0; i <= num_link; ++i) {
		struct pcn_channel_t *channel = pchannels[i];
		struct ring_buffer_req_t in_msg;

		/* if a channel is not established yet */
		if ( unlikely(!channel) )
			continue;

		/* get a netmsg from in_q of this channel */
		ring_buffer_get_req_init(&in_msg, BLOCKING);
		rc = ring_buffer_scif_get_nolock(&channel->in_q, &in_msg);
		if (rc) {
			/* no new message in this channel */
			if (rc == -EAGAIN)
				continue;
			/* oops, something goes wrong */
			goto err_out;
		}

		/* process a netmsg */
		rc = _pcn_channel_process_netmsg(netctx, i, &in_msg);
		if (rc)
			goto err_out;

		/* increast the number of active channels */
		++num_active_channels;
	}

	return num_active_channels;
err_out:
	return rc;
}

#define fetch_netmsg()  ({						\
	struct pcn_channel_t *channel =			                \
		netctx->pchannels[remote_id];				\
	rc = copy_from_ring_buffer_scif(&channel->in_q,			\
					netctx->scratchpad,		\
					in_msg->data,			\
					in_msg->size);			\
	if (rc)								\
		goto err_out;						\
	hdr = netctx->scratchpad + in_msg->size;			\
	netmsg_unmarshal_header(0, netctx->scratchpad, hdr);		\
	hdr;								\
	})

#define decode_netmsg(__hdr)  rc = -EINVAL;	\
	switch((__hdr)->command)

#define execute_netmsg(__TYPE, __type)					\
	case PCN_NETMSG_##__TYPE: {					\
		struct netmsg_##__type##_t *__type = (void *)hdr;	\
		struct pcn_channel_t *channel =			        \
			netctx->pchannels[remote_id];			\
		netmsg_unmarshal_##__type(0,				\
					  netctx->scratchpad,		\
					  __type);			\
		ring_buffer_scif_elm_set_done(&channel->in_q,	        \
					      in_msg->data);		\
		rc =  _pcn_channel_process_##__type(netctx,		\
						    remote_id,		\
						    __type);		\
		break;						        \
	}

static
int _pcn_channel_process_netmsg(struct pcn_netctx_t *netctx,
				int remote_id,
				struct ring_buffer_req_t *in_msg)
{
	struct netmsg_header_t *hdr;
	int rc = 0;

	/* secure the scratchpad memory */
	rc = _pcn_secure_scratchpad(netctx, in_msg->size * 2);
	if (rc)
		goto err_out;

	/* fetch, decode, and execute a netmsg */
	hdr = fetch_netmsg();
	decode_netmsg(hdr) {
		execute_netmsg(T_SOCKET,     t_socket);
		execute_netmsg(N_SOCKET,     n_socket);
		execute_netmsg(T_BIND,       t_bind);
		execute_netmsg(T_CONNECT,    t_connect);
		execute_netmsg(T_LISTEN,     t_listen);
		execute_netmsg(T_SETSOCKOPT, t_setsockopt);
		execute_netmsg(T_GETSOCKOPT, t_getsockopt);
		execute_netmsg(T_SENDMSG,    t_sendmsg);
		execute_netmsg(T_SENDDATA,   t_senddata);
		execute_netmsg(T_SHUTDOWN,   t_shutdown);
		execute_netmsg(T_CLOSE,      t_close);
	}
	if (rc)
		goto err_out;
	return 0;
err_out:
	return rc;
}

static
int _pcn_secure_scratchpad(struct pcn_netctx_t *netctx, int size)
{
	int rc = 0;

	/* if needed, enlarge the scratchpad */
	if ( unlikely(size > netctx->scratchpad_size) ) {
		free(netctx->scratchpad);
		netctx->scratchpad_size = (size + ~PAGE_MASK) & PAGE_MASK;
		netctx->scratchpad = malloc(netctx->scratchpad_size);
		if (!netctx->scratchpad) {
			rc = -ENOMEM;
			goto err_out;
		}
	}

	return 0;
err_out:
	return rc;
}

static inline
struct pcn_socket_t * sockid_to_socket_t(uint64_t sockid)
{
	return (struct pcn_socket_t *)sockid;
}

static inline
uint64_t socket_t_to_sockid(struct pcn_socket_t *socket_t)
{
	return (uint64_t)socket_t;
}

#define response_netmsg(__t_type, __r_type, ...) ({			\
		struct pcn_channel_t *__channel =			\
			netctx->pchannels[remote_id];			\
		uint64_t __tag = __t_type->hdr.tag;			\
		struct ring_buffer_req_t __out_msg;			\
		int __size, __rc;					\
		__size = netmsg_marshal_##__r_type(1,			\
						   netctx->scratchpad,	\
						   __tag,		\
						   __VA_ARGS__);	\
		ring_buffer_put_req_init(&__out_msg, BLOCKING, __size);	\
	        while ( (__rc =					        \
			 ring_buffer_scif_put_nolock(			\
				 &__channel->out_q,			\
				 &__out_msg)) == -EAGAIN ) {		\
			pcn_yield();					\
		}							\
		if (__rc)						\
			goto __out;					\
		netmsg_marshal_##__r_type(0,				\
					  netctx->scratchpad,		\
					  __tag,			\
					  __VA_ARGS__);			\
		__rc = copy_to_ring_buffer_scif(&__channel->out_q,      \
						__out_msg.data,		\
						netctx->scratchpad,	\
						__size);		\
		if (__rc)						\
			goto __out;					\
		ring_buffer_scif_elm_set_ready(&__channel->out_q,	\
					       __out_msg.data);		\
		__rc = 0;						\
	__out:							        \
		__rc;							\
	})

/*
 * Sets a socket's send buffer size to the maximum allowed by the system.
 */
static
void _pcn_maximize_sndbuf(const int sfd)
{
	socklen_t intsize = sizeof(int);
	int last_good = 0;
	int min, max, avg;
	int old_size, rc;

	/* Start with the default size. */
	rc = getsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &old_size, &intsize); 
	if (rc != 0) {
		goto err_out;
	}

	/* Binary-search for the real maximum. */
	min = old_size;
	max = MAX_SENDBUF_SIZE;

	while (min <= max) {
		avg = ((unsigned int)(min + max)) / 2;
		if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, 
			       (void *)&avg, intsize) == 0) {
			last_good = avg;
			min = avg + 1;
		} else {
			max = avg - 1;
		}
	}

	return;
err_out:
        pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
        return;
}

static
int _pcn_channel_process_n_socket(struct pcn_netctx_t *netctx,
                                  int remote_id,
                                  struct netmsg_n_socket_t *n_socket)
{
        struct pcn_socket_t *socket_t =
                sockid_to_socket_t(n_socket->sockid);
        int rc, val = 1;

	// update the tag values
	if (socket_t->type == PCN_SOCK_TYPE_IN) {
		socket_t->tag_available = 1;
		socket_t->tag = n_socket->hdr.tag;
	}

	// No response, just notification

	return 1;
}

static
int _pcn_channel_process_t_socket(struct pcn_netctx_t *netctx,
				  int remote_id,
				  struct netmsg_t_socket_t *t_socket)
{
	struct pcn_socket_t *socket_t = NULL;
	struct pcn_socktbl_t *socktbl;
	int rc, val = 1;

	if (!netctx->listen_fd) {
		/* do socket operation */
		rc = _pcn_socket(netctx->mctx, t_socket->domain, 
				 t_socket->type, 0);
		if (rc == -1) {
			rc = -errno;
			goto err_out;
		}

		/* By this load balancing between channels is handled by
	 	 * linux kernel TCP stack. need to handle this in mTCP.*/
		//_pcn_setsockopt(netctx->mctx, rc, SOL_SOCKET, SO_REUSEADDR, (char *) &val,
		//	sizeof(val));

	} else {
		rc = netctx->listen_fd->sockfd;
	}

	/* alloc & init socket */
	socket_t = calloc(1, sizeof(*socket_t));
	if (!socket_t) {
		rc = -ENOMEM;
		goto err_out;
	}
	socket_t->netctx = netctx;
	socket_t->remote_id = remote_id;
	if (netctx->listen_fd) {
		socket_t->type = netctx->listen_fd->type;
	} else {
		socket_t->type = PCN_SOCK_TYPE_NEW;
	}
	socket_t->sockfd = rc;
	socket_t->tag    = t_socket->hdr.tag;

	/* add to the list */
	socktbl = netctx->psocktbls[remote_id];
	list_add(&socket_t->list, &socktbl->types[socket_t->type]);

	netctx->listen_fd = socket_t;

	/* success: rc = 0 */
	rc = 0;
out:
	/* reply to the remote end */
	rc = response_netmsg(t_socket, r_socket, rc,
			     socket_t_to_sockid(socket_t));
	if (rc)
		goto err_rtn;
	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	free(socket_t);
	socket_t = NULL;
	goto out;
err_rtn:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	free(socket_t);
	socket_t = NULL;
	return rc;
}

static
int _pcn_channel_process_t_bind(struct pcn_netctx_t *netctx,
				int remote_id,
				struct netmsg_t_bind_t *t_bind)
{
	struct pcn_socket_t *socket_t =
		sockid_to_socket_t(t_bind->sockid);
	struct pcn_socktbl_t *socktbl;
	int rc;

        /* sanity check - listenfd should be present */
        pcn_assert(netctx->listen_fd != NULL,
                   "listenfd not available");

	if (netctx->listen_fd->type == PCN_SOCK_TYPE_LISTEN) {
		/* move the socket_t to PCN_SOCK_TYPE_LISTEN */
		socktbl = netctx->psocktbls[remote_id];
		socket_t->type = PCN_SOCK_TYPE_LISTEN;
		list_move(&socket_t->list, &socktbl->types[socket_t->type]);
		rc = 0;
		goto out;
	}

	/* sanity check */
	pcn_assert(socket_t->type == PCN_SOCK_TYPE_NEW,
		   "incorrect socket type");

	/* set this socket as non-blocking */
	mtcp_setsock_nonblock(netctx->mctx, socket_t->sockfd);

	/* do socket operation */
	rc = _pcn_bind(netctx->mctx, socket_t->sockfd, (struct sockaddr *)&t_bind->addr,
		       t_bind->addrlen);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

	/* move the socket_t to PCN_SOCK_TYPE_LISTEN */
	socktbl = netctx->psocktbls[remote_id];
	socket_t->type = PCN_SOCK_TYPE_LISTEN;
	list_move(&socket_t->list, &socktbl->types[socket_t->type]);

	/* Move the listen_fd to Listen state */
	if (socket_t != netctx->listen_fd) {
		if (netctx->listen_fd->type != PCN_SOCK_TYPE_LISTEN) {
			netctx->listen_fd->type = PCN_SOCK_TYPE_LISTEN;
			socktbl = netctx->psocktbls[netctx->listen_fd->remote_id];
			list_move(&netctx->listen_fd->list,
				  &socktbl->types[netctx->listen_fd->type]);
		}
	}

out:
	/* reply to the remote end */
	rc = response_netmsg(t_bind, r_bind, rc);
	if (rc)
		goto err_rtn;
	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
err_rtn:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return rc;
}

static
int _pcn_channel_process_t_connect(struct pcn_netctx_t *netctx,
				   int remote_id,
				   struct netmsg_t_connect_t *t_connect)
{
	struct pcn_socket_t *socket_t =
		sockid_to_socket_t(t_connect->sockid);
	struct pcn_socktbl_t *socktbl;
	struct mtcp_epoll_event ev;
	int epfd = netctx->epoll_fd, rc;

	/* sanity check */
	pcn_assert(socket_t->type == PCN_SOCK_TYPE_NEW,
		   "incorrect socket type");

	// handling client - assume no listening port.
	netctx->listen_fd = NULL;

	/* do socket operation */
	rc = _pcn_connect(netctx->mctx, socket_t->sockfd,
			  (struct sockaddr *)&t_connect->addr,
			  t_connect->addrlen);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

        /* move the socket_t to PCN_SOCK_TYPE_LISTEN */
	socktbl = netctx->psocktbls[remote_id];
	socket_t->type = PCN_SOCK_TYPE_OUT;
	list_move(&socket_t->list, &socktbl->types[socket_t->type]);

        ev.events = MTCP_EPOLLIN | MTCP_EPOLLET;
        ev.data.sockid = socket_t->sockfd;

        rc = _pcn_epoll_ctl(netctx->mctx, epfd, MTCP_EPOLL_CTL_ADD, socket_t->sockfd, &ev);
        if (rc == -1) {
                rc = -errno;
                goto err_out;
        }
        netctx->num_listen++;

        /* Add mapping from sockid to remote_id */
        netctx->sockid_remid[socket_t->sockfd].remote_id = 
						socket_t->remote_id;
        netctx->sockid_remid[socket_t->sockfd].sock  = socket_t;

out:
        /* reply to the remote end */
	rc = response_netmsg(t_connect, r_connect, rc);
	if (rc)
		goto err_rtn;
	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
err_rtn:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return rc;
}

static
int _pcn_channel_process_t_listen(struct pcn_netctx_t *netctx,
				  int remote_id,
				  struct netmsg_t_listen_t *t_listen)
{
	struct pcn_socket_t *socket_t =
		sockid_to_socket_t(t_listen->sockid);
	struct pcn_socktbl_t *socktbl;
	struct mtcp_epoll_event ev;
	int epfd = netctx->epoll_fd;
	int rc;

	if (netctx->num_listen) {
		rc = 0;
		goto out;
	}

        /* sanity check */
	pcn_assert(socket_t->type == PCN_SOCK_TYPE_LISTEN,
		   "incorrect socket type");

        /* do socket operation */
	rc = _pcn_listen(netctx->mctx, socket_t->sockfd, t_listen->backlog);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

	/* Add these FD to the EPOLL context FD */
	ev.events = MTCP_EPOLLIN;
	ev.data.sockid = socket_t->sockfd;

	rc = _pcn_epoll_ctl(netctx->mctx, epfd, MTCP_EPOLL_CTL_ADD, socket_t->sockfd, &ev);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}
	netctx->num_listen++;
out:
        /* reply to the remote end */
	netctx->listen_recv[remote_id] = 1;
	rc = response_netmsg(t_listen, r_listen, rc);
	if (rc)
		goto err_rtn;
	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
err_rtn:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return rc;

}

static
int _pcn_channel_process_t_setsockopt(struct pcn_netctx_t *netctx,
				      int remote_id,
				      struct netmsg_t_setsockopt_t *t_setsockopt)
{
	struct pcn_socket_t *socket_t =
		sockid_to_socket_t(t_setsockopt->sockid);
	struct pcn_socktbl_t *socktbl;
	int rc;

        /* do socket operation */
	rc = _pcn_setsockopt(netctx->mctx, socket_t->sockfd, t_setsockopt->level,
			     t_setsockopt->optname, t_setsockopt->optval,
			     t_setsockopt->optlen);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}
out:
        /* reply to the remote end */
	rc = response_netmsg(t_setsockopt, r_setsockopt, rc);
	if (rc)
		goto err_rtn;
	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
err_rtn:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return rc;
}

static
int _pcn_channel_process_t_getsockopt(struct pcn_netctx_t *netctx,
					      int remote_id,
					      struct netmsg_t_getsockopt_t *t_getsockopt)
{
	struct pcn_socket_t *socket_t =
		sockid_to_socket_t(t_getsockopt->sockid);
	struct pcn_socktbl_t *socktbl;
	uint8_t optval[1024];
	uint32_t optlen = min(sizeof(optval), t_getsockopt->optlen);
	int rc;

	/* do socket operation */
	rc = _pcn_getsockopt(netctx->mctx, socket_t->sockfd, t_getsockopt->level,
			     t_getsockopt->optname, optval, &optlen);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}
out:
	/* reply to the remote end */
	rc = response_netmsg(t_getsockopt, r_getsockopt, rc, optlen, optval);
	if (rc)
		goto err_rtn;
	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
err_rtn:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return rc;
}

static
int _pcn_channel_process_t_sendmsg(struct pcn_netctx_t *netctx,
				   int remote_id,
				   struct netmsg_t_sendmsg_t *t_sendmsg)
{
	struct pcn_socket_t *socket_t =
		sockid_to_socket_t(t_sendmsg->sockid);
	struct pcn_socktbl_t *socktbl;
	int rc;

	/* sanity check */
	pcn_assert(socket_t->type == PCN_SOCK_TYPE_IN ||
		   socket_t->type == PCN_SOCK_TYPE_OUT,
		   "incorrect socket type");

	/* do socket operation */
	/* XXX: need to translate the address of iov */
	rc = _pcn_sendmsg(socket_t->sockfd, &t_sendmsg->msghdr,
			t_sendmsg->flags);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}
out:
	/* reply to the remote end */
	rc = response_netmsg(t_sendmsg, r_sendmsg, rc);
	if (rc)
		goto err_rtn;
	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
err_rtn:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return rc;
}

static int   _pcn_channel_process_t_senddata(struct pcn_netctx_t *netctx,
					     int remote_id,
					     struct netmsg_t_senddata_t *t_senddata)
{
	struct pcn_socket_t *socket_t =
		sockid_to_socket_t(t_senddata->sockid);
	struct pcn_socktbl_t *socktbl;
	int rc, remaining, buf_remaining, rc1;
	struct mtcp_epoll_event ev;

	/* sanity check */
	pcn_assert(socket_t->type == PCN_SOCK_TYPE_IN ||
		   socket_t->type == PCN_SOCK_TYPE_OUT,
		   "incorrect socket type");

	/* If there is already backlog, then just append the data
	 * to the backlog */

	if (socket_t->backlog) {
		remaining = t_senddata->datalen;
		buf_remaining = MAX_HOST_BUFFER -
			(socket_t->start - socket_t->buf);

                if (remaining > buf_remaining) {
                        rc = -errno;
                        goto err_out;
                }
		
                // we have enough buffer to store this
                memcpy(socket_t->start, t_senddata->data,
                                        remaining);
                socket_t->start = socket_t->start + remaining;
		socket_t->len  += remaining;
		// since the data is copied to the buffer, notify
                // xeon phi that all data is sent.
                rc = remaining;
		goto out;
	}

	/* do socket operation */
	/* XXX: large data splitting */
	rc = _pcn_write(netctx->mctx, socket_t->sockfd, (char *) t_senddata->data,
			   t_senddata->datalen);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

	// Entire data can't be sent. Enable backlog for this
	// socket 

	if (rc < t_senddata->datalen) {
		remaining = t_senddata->datalen - rc;
		buf_remaining = MAX_HOST_BUFFER -
			(socket_t->start - socket_t->buf);

		if (remaining > buf_remaining) {
			// send rc to xeon phi	
			goto err_out;
		}

		// we have enough buffer to store this
		memcpy(socket_t->start, (t_senddata->data + rc),
					remaining);
		socket_t->start = socket_t->start + remaining;
		socket_t->backlog = 1;
		socket_t->len += remaining;

		// enable EPOLLOUT on the socket
        	ev.events = MTCP_EPOLLOUT;
        	ev.data.sockid = socket_t->sockfd;

		// since the data is copied to the buffer, notify
		// xeon phi that all data is sent.
		rc = t_senddata->datalen;

        	rc1 = _pcn_epoll_ctl(netctx->mctx, netctx->epoll_fd, MTCP_EPOLL_CTL_MOD, 
				  socket_t->sockfd, &ev);
        	if (rc1 == -1) {
                	rc1 = -errno;
			// we still need to send response to xeon-phi
                	goto err_out;
        	}
	}
out:
	/* reply to the remote end */
	rc = response_netmsg(t_senddata, r_senddata, rc);
	if (rc)
		goto err_rtn;
	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
err_rtn:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return rc;
}

static
int _pcn_channel_process_t_shutdown(struct pcn_netctx_t *netctx,
				    int remote_id,
				    struct netmsg_t_shutdown_t *t_shutdown)
{
	struct pcn_socket_t *socket_t =
		sockid_to_socket_t(t_shutdown->sockid);
	int rc;

	/* do socket operation */
	rc = _pcn_shutdown(socket_t->sockfd, t_shutdown->how);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}
out:
	/* reply to the remote end */
	rc = response_netmsg(t_shutdown, r_shutdown, rc);
	if (rc)
		goto err_rtn;
	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
err_rtn:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return rc;
}

static
int _pcn_channel_process_t_close(struct pcn_netctx_t *netctx,
				 int remote_id,
				 struct netmsg_t_close_t *t_close)
{
	struct pcn_socket_t *socket_t =
		sockid_to_socket_t(t_close->sockid);
	int rc;

	/* do socket operation */
	rc = _pcn_close(netctx, socket_t->sockfd);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

out:
	/* remove the socket_t */
	list_del(&socket_t->list);

	/* free the socket_t */
	free(socket_t);

	/* reply to the remote end */
	rc = response_netmsg(t_close, r_close, rc);
	if (rc)
		goto err_rtn;
	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
err_rtn:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return rc;
}

static 
char* _pcn_get_read_buffer(struct pcn_netctx_t *netctx, 
			   struct pcn_socket_t *s, 
			   int data_len,
			   int *netmsg_len)
{
	int netmsg_size, rc;
	struct ring_buffer_req_t out_msg;
	struct pcn_channel_t *channel =
				netctx->pchannels[s->remote_id];
	struct netmsg_e_recvdata_t *e_recvdata;

	// Get the size of message with the header
	netmsg_size = netmsg_marshal_e_recvdata(1, NULL, 0,
	  				  0, data_len, NULL);
	
	ring_buffer_put_req_init(&out_msg, BLOCKING, netmsg_size);

	while ((rc =
		ring_buffer_scif_put_nolock(
			&channel->out_q,
			&out_msg)) == -EAGAIN) {
		pcn_yield();
	}

	// copy the entire message len
	*netmsg_len = netmsg_size;
	e_recvdata = out_msg.data;
	return (char *) e_recvdata->data;
}

static
int _pcn_send_read_ev(struct pcn_netctx_t *netctx, struct pcn_socket_t *s,
		      char *data, int data_len, int netmsg_size)
{
	int size, rc;
	struct pcn_channel_t *channel =
				netctx->pchannels[s->remote_id];
	char *msg_header = data - (netmsg_size - MAX_READ_LEN);

	size = netmsg_marshal_e_recvdata(0, msg_header,
					 s->tag, socket_t_to_sockid(s),
					 data_len, NULL);

	ring_buffer_scif_elm_set_ready(&channel->out_q, msg_header);
	return size;
}

static
int _pcn_handle_read_ev(struct pcn_netctx_t *netctx, int fd)
{
	struct pcn_sockfd_map_t map = netctx->sockid_remid[fd];
	char *buf;
	int rc, netmsg_size;

	if ((map.sock->type == PCN_SOCK_TYPE_IN) &&
	    (!map.sock->tag_available)) {
		// we will get the read event again !!
		return 0;
	}

	while (1) {
		// allocate buffer from the ring buffer
		buf = _pcn_get_read_buffer(netctx, map.sock,
					   MAX_READ_LEN, 
					   &netmsg_size);

		rc = _pcn_read(netctx->mctx, fd, buf, MAX_READ_LEN);

        	if (rc == -1) {
			_pcn_send_read_ev(netctx, map.sock, buf,
				  	  0, netmsg_size);
                	rc = -errno;
                	goto err_out;
        	}

		_pcn_send_read_ev(netctx, map.sock, buf,
				  rc, netmsg_size);

		if (rc < MAX_READ_LEN)
			break;
	}

	return rc;
err_out:
        pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return 0;
}

static
int _pcn_handle_write_ev(struct pcn_netctx_t *netctx, int fd)
{
	struct pcn_sockfd_map_t map = netctx->sockid_remid[fd];
	struct pcn_socket_t *socket_t = map.sock;
	int rc;

        pcn_assert(socket_t->type == PCN_SOCK_TYPE_IN ||
                   socket_t->type == PCN_SOCK_TYPE_OUT,
                   "incorrect socket type");

	// Handle backlog
	if (socket_t->backlog) {
		rc = _pcn_write(netctx->mctx, fd, socket_t->send,
				   socket_t->len);

        	if (rc == -1) {
                	rc = -errno;
                	goto err_out;
		}

		if (rc == socket_t->len) {
			socket_t->backlog = 0;
			socket_t->start = socket_t->buf;
			socket_t->send  = socket_t->buf;
			socket_t->len = 0;
			
			// send backlog reset to xeon-phi
		}

		if (rc < socket_t->len) {
			socket_t->send  += rc;
			socket_t->len   -= rc;
		}
	}

err_out:
        pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
        return 0;
}

static
int _pcn_map_remote_id(struct pcn_netctx_t *netctx)
{
	int i = 0, id;

	// choose next remote_id by round-robin method
	while (i<PCN_PROXY_MAX_LINKS) {
		id = (netctx->current_remid + i) %
		      PCN_PROXY_MAX_LINKS;
		if (netctx->listen_recv[id]) {
			netctx->current_remid = id + 1;
			return id;
		}
		i++;
	}
	return 0;
}

static
void _pcn_send_accept_ev(struct pcn_netctx_t *netctx,
			 struct pcn_socket_t *s)
{
	struct pcn_channel_t *channel =
				netctx->pchannels[s->remote_id];
	int netmsg_size, rc;
	struct ring_buffer_req_t out_msg;

        netmsg_size = netmsg_marshal_e_accept(1, NULL, 0,
                                              0, 0, NULL);

	ring_buffer_put_req_init(&out_msg, BLOCKING, netmsg_size);

        while ((rc =
                ring_buffer_scif_put_nolock(
                        &channel->out_q,
                        &out_msg)) == -EAGAIN) {
                pcn_yield();
        }

	netmsg_marshal_e_accept(0, out_msg.data, s->tag,
				socket_t_to_sockid(s),
				s->addrlen, &s->addr);

	ring_buffer_scif_elm_set_ready(&channel->out_q,
				out_msg.data);
}

static
void _pcn_handle_err_ev(struct pcn_netctx_t *netctx,
		      int fd)
{
        struct pcn_sockfd_map_t map = netctx->sockid_remid[fd];
        char *buf;
        int rc, netmsg_size;

	_pcn_close(netctx, fd);

	// free socket entries
	netctx->sockid_remid[fd].sock = NULL;
	netctx->sockid_remid[fd].remote_id = 0;
	free(map.sock);

	// TODO We need to send close event to Xeon-Phi 
}

static
void _pcn_handle_accept_ev(struct pcn_netctx_t *netctx,
			   int fd, struct sockaddr_in *addr,
			   socklen_t *addrlen)
{
	struct mtcp_epoll_event ev;
        struct pcn_socket_t *socket_t = NULL;
	struct pcn_socktbl_t *socktbl;
	int rc, remote_id;
	int epfd = netctx->epoll_fd;

	/* alloc & init socket */
	socket_t = calloc(1, sizeof(*socket_t));
	if (!socket_t) {
		rc = -ENOMEM;
		goto err_out;
	}

	socket_t->netctx = netctx;
	// XXX:TODO Load_balance and assign it a remote_id
	remote_id = _pcn_map_remote_id(netctx);
	socket_t->remote_id = remote_id;
	socket_t->type = PCN_SOCK_TYPE_IN;
	socket_t->sockfd   = fd;
	socket_t->addr     = *addr;
	socket_t->addrlen = *addrlen;
        socket_t->start = socket_t->buf;
        socket_t->send  = socket_t->buf;
	socket_t->tag    = netctx->listen_fd->tag;

	// set this socket as non-blocking
	mtcp_setsock_nonblock(netctx->mctx, fd);

        /* add to the list */
        socktbl = netctx->psocktbls[remote_id];
        list_add(&socket_t->list, &socktbl->types[socket_t->type]);

	/* Add these FD to the EPOLL context FD */
	ev.events = MTCP_EPOLLIN;
	ev.data.sockid = socket_t->sockfd;

	rc = _pcn_epoll_ctl(netctx->mctx, epfd, MTCP_EPOLL_CTL_ADD, socket_t->sockfd, &ev);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

	/* Add mapping from sockid to remote_id */
	netctx->sockid_remid[fd].remote_id = remote_id;
	netctx->sockid_remid[fd].sock      = socket_t;

	/* maximize send buffer to available max */
	//_pcn_maximize_sndbuf(fd);

	/* send accept command to the appropriate client */
	_pcn_send_accept_ev(netctx, socket_t);
	return;
err_out:
        pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return;
}

static
int _pcn_accept_connection(struct pcn_netctx_t *netctx)
{
	int i, new_fd;
	struct pcn_socket_t *listen = netctx->listen_fd;
	struct sockaddr_in addr;
	socklen_t addrlen;

	/* Get new incoming connections */
	while (1) {
		addrlen = sizeof(addr);
		new_fd = _pcn_accept(netctx->mctx, listen->sockfd, NULL, NULL);
		if (new_fd >= 0) {
			// add new_fd to particular xeon-phi.
			_pcn_handle_accept_ev(netctx, new_fd, &addr,
					      &addrlen);
		} else {
			// Accepted waiting connections
			break;
		}
	}
	
	return 0;
}

static
int _pcn_listen_fd(struct pcn_netctx_t *netctx, int fd)
{
	int rc = -1;

	// If client, then there is no listening port.	
	if (!netctx->listen_fd)
		return rc;

	// look at the listen socket on each remote-id and 
        // check if this match
	rc = (netctx->listen_fd->sockfd == fd) ? fd : -1;

	return rc;
}

static
int _pcn_netctx_process_sockets(struct pcn_netctx_t *netctx)
{
	struct pcn_channel_t **pchannels = netctx->pchannels;
	struct pcn_socktbl_t **psocktbls = netctx->psocktbls;
	int num_link = netctx->num_link;
	int epfd = netctx->epoll_fd;
	int i, nevents, num_active_sockets = 0;
	int rc = 0, do_accept = 0;
	struct mtcp_epoll_event *events = netctx->mevents;

	/* XXX: create epoll fd array */
	// for (i = 0; i < num_link; ++i) {
	//	struct pcn_socktbl_t *socktbl = psocktbls[i];
	// }

	/* XXX: epoll for 0 msec
	 * - http://linux.die.net/man/2/epoll_wait
	 * - The timeout argument specifies the minimum number of
	 *   milliseconds that epoll_wait() will block. (This interval
	 *   will be rounded up to the system clock granularity, and
	 *   kernel scheduling delays mean that the blocking interval
	 *   may overrun by a small amount.) Specifying a timeout of -1
	 *   causes epoll_wait() to block indefinitely, while specifying
	 *                                              ^^^^^^^^^^^^^^^^
	 *   a timeout equal to zero cause epoll_wait() to return
	 *   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	 *   immediately, even if no events are available.
	 *   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	 */

	/* XXX: process sockets for LISTEN
	 * - accept()
	 * - add new fd to socktbl
	 * - send a response to out_q of the channel
	 *   -> PCN_NETMSG_E_ACCEPT */

	/* XXX: process sockets for IN
	 * - recv()
	 * - send a response to out_q of the channel
	 *   -> PCN_NETMSG_E_RECVDATA */
	/* XXX: contents-based packet steering like FlexNIC */

	nevents	= _pcn_epoll_wait(netctx->mctx, epfd, netctx->mevents,
			          PCN_MAX_EVENTS, 0);

	do_accept = 0;
	for (i = 0; i < nevents; i++) {
		if (_pcn_listen_fd(netctx, events[i].data.sockid) >= 0) {
			/* accept connections after 
			 * handling data. */
			do_accept = 1;
		} else if (events[i].events & MTCP_EPOLLERR) {
			_pcn_handle_err_ev(netctx, events[i].data.sockid);
		} else if (events[i].events & MTCP_EPOLLIN) {
			/* handle read event */
			_pcn_handle_read_ev(netctx, events[i].data.sockid);
		} else if (events[i].events & MTCP_EPOLLOUT) {
			_pcn_handle_write_ev(netctx, events[i].data.sockid);
		} else {
			pcn_assert(0, "Invalid IN socket event");
		}
	}

	if (do_accept) {
		// accept connections and add to epoll
		_pcn_accept_connection(netctx);
	}

	return netctx->num_listen;
	goto err_out; /* XXX: make compiler happy */
err_out:
	return rc;
}
