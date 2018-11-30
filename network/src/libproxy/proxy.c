#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pcnporting.h>
#include "proxy_i.h"

static pthread_mutex_t __bind_lock = PTHREAD_MUTEX_INITIALIZER;

int pcn_init_proxy(int listening_port, int num_netctx,
		   int base_link_port, size_t rx_q_size,
		   struct pcn_proxy_t *proxy)
{
	int i;
	int rc = 0;

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
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
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
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
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

	/* create and launch per-netctx thread */
	rc = pthread_create(&netctx->worker,
			    NULL,
			    _pcn_netctx_worker,
			    netctx);
	if (rc)
		goto err_out;

	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
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

	netctx->events = (struct epoll_event *) calloc(PCN_MAX_EVENTS,
						       sizeof(struct epoll_event));
	if (!netctx->events) {
		rc = -ENOMEM;
		goto err_out;
	}

	/* wait for a start signal */
	smp_rmb();
	while (netctx->status == PCN_NETCTX_STATUS_UNINIT) {
		pcn_yield();
	}

	/* create the epoll fd */
	netctx->epoll_fd = _pcn_epoll_create(1);
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
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
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
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
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
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return rc;
}

#define fetch_netmsg()  ({						\
			struct pcn_channel_t *channel =			\
				netctx->pchannels[remote_id];		\
			rc = copy_from_ring_buffer_scif(&channel->in_q,	\
							netctx->scratchpad, \
							in_msg->data,	\
							in_msg->size);	\
			if (rc)						\
				goto err_out;				\
			hdr = netctx->scratchpad + in_msg->size;	\
			netmsg_unmarshal_header(0, netctx->scratchpad, hdr); \
			hdr;						\
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
		pcn_dbg("execute_netmsg: %s(%d) -> (rc, %d)\n",		\
			netmsg_command_str[PCN_NETMSG_##__TYPE],	\
			PCN_NETMSG_##__TYPE,				\
			rc);						\
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
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
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
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
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
			struct pcn_channel_t *__channel =		\
				netctx->pchannels[remote_id];		\
			uint64_t __tag = __t_type->hdr.tag;		\
			struct ring_buffer_req_t __out_msg;		\
			int __size, __rc;				\
			__size = netmsg_marshal_##__r_type(1,		\
							   netctx->scratchpad, \
							   NETMSG_UNKNOWN_SEQNUM, \
							   __tag,	\
							   __VA_ARGS__); \
			ring_buffer_put_req_init(&__out_msg, BLOCKING, __size);	\
			while ( (__rc =					\
				 ring_buffer_scif_put_nolock(		\
					 &__channel->out_q,		\
					 &__out_msg)) == -EAGAIN ) {	\
				pcn_yield();				\
			}						\
			if (__rc)					\
				goto __out;				\
			netmsg_marshal_##__r_type(0,			\
						  netctx->scratchpad,	\
						  __channel->out_q_seqnum++, \
						  __tag,		\
						  __VA_ARGS__);		\
			__rc = copy_to_ring_buffer_scif(&__channel->out_q, \
							__out_msg.data,	\
							netctx->scratchpad, \
							__size);	\
			if (__rc)					\
				goto __out;				\
			ring_buffer_scif_elm_set_ready(&__channel->out_q, \
						       __out_msg.data);	\
			__rc = 0;					\
		__out:							\
			__rc;						\
		})

static
int _pcn_set_socket_non_blocking (int sfd)
{
	int rc;

	/* get current socket flags */
	rc = fcntl (sfd, F_GETFL, 0);

	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

	/* set non-blocking */
	rc |= O_NONBLOCK;
	rc = fcntl (sfd, F_SETFL, rc);

	if (rc== -1) {
		rc = -errno;
		goto err_out;
	}

	return 0;
err_out:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return rc;
}

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

	/* update the tag values */
	if (socket_t->type == PCN_SOCK_TYPE_IN) {
		socket_t->tag_available = 1;
		socket_t->tag = n_socket->hdr.tag;
	}

	/* No response, just notification */
	return 0;
}

static inline
int _pcn_set_remote_id_bits(int *prid_bits, int add_rid_bits)
{
	int rid_bits;
	int new_rid_bits;

	do {
		rid_bits = *prid_bits;
		new_rid_bits = rid_bits | add_rid_bits;
	} while( !smp_cas(prid_bits, rid_bits, new_rid_bits));
	return new_rid_bits;
}

static inline
int _pcn_unset_remote_id_bits(int *prid_bits, int rm_rid_bits)
{
	int rid_bits;
	int new_rid_bits;

	do {
		rid_bits = *prid_bits;
		new_rid_bits = rid_bits & ~rm_rid_bits;
	} while( !smp_cas(prid_bits, rid_bits, new_rid_bits));
	return new_rid_bits;
}

static inline
int _pcn_num_listening_ends(int remote_id_bits)
{
	return __builtin_popcount(remote_id_bits);
}

static
int _pcn_channel_process_t_socket(struct pcn_netctx_t *netctx,
				  int remote_id,
				  struct netmsg_t_socket_t *t_socket)
{
	struct pcn_socket_t *socket_t = NULL;
	struct pcn_socktbl_t *socktbl;
	int rc, val = 1;

	/* do socket operation */
	rc = _pcn_socket(t_socket->domain, t_socket->type,
			 t_socket->protocol);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

	/* By this load balancing between channels is handled by
	 * linux kernel TCP stack. need to handle this in mTCP.*/
	_pcn_setsockopt(rc, SOL_SOCKET, SO_REUSEADDR, (char *) &val,
			sizeof(val));

	/* alloc & init socket */
	socket_t = calloc(1, sizeof(*socket_t));
	if (!socket_t) {
		rc = -ENOMEM;
		goto err_out;
	}
	socket_t->netctx = netctx;
	socket_t->remote_id = remote_id;
	socket_t->type = PCN_SOCK_TYPE_NEW;
	socket_t->sockfd = rc;
	socket_t->remote_id_bits = 1 << remote_id;
	socket_t->lb_cnt = remote_id;
	socket_t->tag = t_socket->hdr.tag;

	/* add to the list */
	socktbl = netctx->psocktbls[remote_id];
	pcn_socktbl_add_lock(socktbl, &socket_t->list, socket_t->type);

	/* Add mapping from sockid to remote_id */
	netctx->sockid_remid[socket_t->sockfd].remote_id = remote_id;
	netctx->sockid_remid[socket_t->sockfd].sock      = socket_t;

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
struct pcn_socket_t *_pcn_find_listening_socket(
	struct pcn_proxy_t *proxy, struct sockaddr_in  *addr,
	socklen_t addrlen)
{
#ifdef PCIE_CLOUD_NETWORK_CONF_SHARED_LISTEN
	struct pcn_netctx_t *netctx;
	struct pcn_socktbl_t *socktbl;
	struct pcn_socket_t *socket_t;
	struct list_head *sk_list, *s;
	int i, j;

	/* TODO: I know this is not ideal. :( */
	for (i = 0; i < proxy->num_netctx; ++i) {
		netctx = &proxy->netctxs[i];
		for (j = 1; j < PCN_PROXY_MAX_LINKS; ++j) {
			socktbl = netctx->psocktbls[j];
			if (!socktbl)
				continue;
			sk_list = &socktbl->types[PCN_SOCK_TYPE_LISTEN];
			pcn_spin_lock(&socktbl->lock);
			list_for_each(s, sk_list) {
				socket_t = list_entry(s, struct pcn_socket_t, list);
				if (socket_t->addrlen == addrlen &&
				    !memcmp(&socket_t->addr, addr, addrlen)) {
					pcn_spin_unlock(&socktbl->lock);
					return socket_t;
				}
			}
			pcn_spin_unlock(&socktbl->lock);
		}
	}
#endif
	return NULL;
}

static
void _pcn_register_listening_socket(
	struct pcn_proxy_t *proxy, struct pcn_socket_t *rs)
{
#ifdef PCIE_CLOUD_NETWORK_CONF_SHARED_LISTEN
	struct sockaddr_in  *addr = &rs->addr;
	socklen_t addrlen = rs->addrlen;
	struct pcn_netctx_t *netctx;
	struct pcn_socktbl_t *socktbl;
	struct pcn_socket_t *socket_t;
	struct list_head *sk_list, *s;
	int i, j, rid_bits;

	/* TODO: I know this is not ideal. :( */
	for (i = 0; i < proxy->num_netctx; ++i) {
		netctx = &proxy->netctxs[i];
		for (j = 1; j < PCN_PROXY_MAX_LINKS; ++j) {
			socktbl = netctx->psocktbls[j];
			if (!socktbl)
				continue;
			sk_list = &socktbl->types[PCN_SOCK_TYPE_LISTEN];
			pcn_spin_lock(&socktbl->lock);
			list_for_each(s, sk_list) {
				socket_t = list_entry(s, struct pcn_socket_t, list);
				if (socket_t->addrlen == addrlen &&
				    !memcmp(&socket_t->addr, addr, addrlen)) {
					rs->remote_id_bits = _pcn_set_remote_id_bits(
						&socket_t->remote_id_bits, rs->remote_id_bits);
				}
			}
			pcn_spin_unlock(&socktbl->lock);
		}
	}
#endif
}

static
void _pcn_unregister_listening_socket(
	struct pcn_proxy_t *proxy, struct pcn_socket_t *us)
{
#ifdef PCIE_CLOUD_NETWORK_CONF_SHARED_LISTEN
	struct sockaddr_in  *addr = &us->addr;
	socklen_t addrlen = us->addrlen;
	int rm_rid_bits = 1 << us->remote_id;
	struct pcn_netctx_t *netctx;
	struct pcn_socktbl_t *socktbl;
	struct pcn_socket_t *socket_t;
	struct list_head *sk_list, *s;
	int i, j;

	/* TODO: I know this is not ideal. :( */
	for (i = 0; i < proxy->num_netctx; ++i) {
		netctx = &proxy->netctxs[i];
		for (j = 1; j < PCN_PROXY_MAX_LINKS; ++j) {
			socktbl = netctx->psocktbls[j];
			if (!socktbl)
				continue;
			sk_list = &socktbl->types[PCN_SOCK_TYPE_LISTEN];
			pcn_spin_lock(&socktbl->lock);
			list_for_each(s, sk_list) {
				socket_t = list_entry(s, struct pcn_socket_t, list);
				if (socket_t->addrlen == addrlen &&
				    !memcmp(&socket_t->addr, addr, addrlen)) {
					_pcn_unset_remote_id_bits(
						&socket_t->remote_id_bits, rm_rid_bits);
				}
			}
			pcn_spin_unlock(&socktbl->lock);
		}
	}
#endif
}

static
struct pcn_socket_t *_pcn_next_remote_socket(
	struct pcn_proxy_t *proxy, struct pcn_socket_t *ms)
{
#ifdef PCIE_CLOUD_NETWORK_CONF_SHARED_LISTEN
	struct sockaddr_in  *addr = &ms->addr;
	socklen_t addrlen = ms->addrlen;
	int remote_id_bits = ms->remote_id_bits;
	struct pcn_netctx_t *netctx;
	struct pcn_socktbl_t *socktbl;
	struct pcn_socket_t *socket_t;
	struct list_head *sk_list, *s;
	int i, j, remote_id;

	/* a listening socket is not shared */
	if (_pcn_num_listening_ends(remote_id_bits) <= 1) {
		return ms;
	}

	/* find next remote id using simple round robin */
	remote_id = ms->lb_cnt;
	for (i = 0; i < PCN_PROXY_MAX_LINKS; ++i, ++remote_id) {
		remote_id = remote_id % PCN_PROXY_MAX_LINKS;
		if (remote_id_bits & (1 << remote_id)) {
			ms->lb_cnt = remote_id + 1;
			break;
		}
	}

	/* find a socket instance with the addr and remote_id */
	/* TODO: I know this is not ideal. :( */
	for (i = 0; i < proxy->num_netctx; ++i) {
		netctx = &proxy->netctxs[i];
		for (j = 1; j < PCN_PROXY_MAX_LINKS; ++j) {
			socktbl = netctx->psocktbls[j];
			if (!socktbl)
				continue;
			pcn_spin_lock(&socktbl->lock); {
				sk_list = &socktbl->types[PCN_SOCK_TYPE_LISTEN];
				list_for_each(s, sk_list) {
					socket_t = list_entry(s, struct pcn_socket_t, list);
					if (socket_t->remote_id == remote_id &&
					    socket_t->addrlen == addrlen &&
					    !memcmp(&socket_t->addr, addr, addrlen)) {
						pcn_spin_unlock(&socktbl->lock);
						goto out;
					}
				}
			} pcn_spin_unlock(&socktbl->lock);
		}
	}
	pcn_assert(0, "Never be here!");
	socket_t = NULL;
out:
	;
	return socket_t;
#else
	return ms;
#endif
}

static
void pcn_bind_lock(void)
{
#ifdef PCIE_CLOUD_NETWORK_CONF_SHARED_LISTEN
	pcn_mutex_lock(&__bind_lock);
#endif
}

static
void pcn_bind_unlock(void)
{
#ifdef PCIE_CLOUD_NETWORK_CONF_SHARED_LISTEN
	pcn_mutex_unlock(&__bind_lock);
#endif
}

static
int _pcn_channel_process_t_bind(struct pcn_netctx_t *netctx,
				int remote_id,
				struct netmsg_t_bind_t *t_bind)
{
	struct pcn_socket_t *socket_t = sockid_to_socket_t(t_bind->sockid);
	struct pcn_socket_t *old_socket_t = NULL;
	struct pcn_socktbl_t *socktbl;
	int rc;

	/* set this socket as non-blocking */
	_pcn_set_socket_non_blocking(socket_t->sockfd);

	/* do socket operation */
	pcn_bind_lock(); {
		old_socket_t = _pcn_find_listening_socket(
			netctx->proxy, &t_bind->addr, t_bind->addrlen);
		if (old_socket_t == NULL) {
			rc = _pcn_bind(socket_t->sockfd,
				       (struct sockaddr *)&t_bind->addr,
				       t_bind->addrlen);
			pcn_dbg("This is the first listening socket: "
				"(socket, %p), (fd, %d), (rc, %d)\n",
				socket_t, socket_t->sockfd, rc);
		}
		else {
			rc = dup2(old_socket_t->sockfd, socket_t->sockfd);
			pcn_dbg("A server socket is already bound; "
				"try dup2: (old_socket, %p, %d) (new_socket, %p, %d) (rc, %d)\n",
				old_socket_t, old_socket_t->sockfd,
				socket_t, socket_t->sockfd,
				rc);
		}
		if (rc == -1) {
			rc = -errno;
			pcn_bind_unlock();
			goto err_out;
		}

		/* move the socket_t to PCN_SOCK_TYPE_LISTEN */
		socket_t->addr = t_bind->addr;
		socket_t->addrlen = t_bind->addrlen;
		socket_t->type = PCN_SOCK_TYPE_LISTEN;
		socktbl = netctx->psocktbls[remote_id];
		_pcn_register_listening_socket(netctx->proxy, socket_t);
		pcn_socktbl_move_lock(socktbl, &socket_t->list, socket_t->type);
	} pcn_bind_unlock();
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
	struct epoll_event ev;
	int epfd = netctx->epoll_fd, rc;

	/* sanity check */
	pcn_assert(socket_t->type == PCN_SOCK_TYPE_NEW,
		   "incorrect socket type");

	/* do socket operation */
	rc = _pcn_connect(socket_t->sockfd,
			  (struct sockaddr *)&t_connect->addr,
			  t_connect->addrlen);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

        /* move the socket_t to PCN_SOCK_TYPE_LISTEN */
	socktbl = netctx->psocktbls[remote_id];
	socket_t->type = PCN_SOCK_TYPE_OUT;
	pcn_socktbl_move_lock(socktbl, &socket_t->list, socket_t->type);

        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = socket_t->sockfd;

        rc = _pcn_epoll_ctl(epfd, EPOLL_CTL_ADD, socket_t->sockfd, &ev);
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

	/* close a zombie fd */
	_pcn_close(netctx, 0);
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
	struct epoll_event ev;
	int epfd = netctx->epoll_fd;
	int rc;

        /* sanity check */
	pcn_assert(socket_t->type == PCN_SOCK_TYPE_LISTEN,
		   "incorrect socket type");

        /* do socket operation */
	rc = _pcn_listen(socket_t->sockfd, t_listen->backlog);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

	/* Add these FD to the EPOLL context FD */
	ev.events = EPOLLIN;
	ev.data.fd = socket_t->sockfd;

	rc = _pcn_epoll_ctl(epfd, EPOLL_CTL_ADD, socket_t->sockfd, &ev);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}
	netctx->num_listen++; /* XXX: where to decrement? */
out:
        /* reply to the remote end */
	netctx->listen_recv[remote_id] = 1;
	rc = response_netmsg(t_listen, r_listen, rc);
	if (rc)
		goto err_rtn;

	/* close a zombie fd */
	_pcn_close(netctx, 0);
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
	rc = _pcn_setsockopt(socket_t->sockfd, t_setsockopt->level,
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
	rc = _pcn_getsockopt(socket_t->sockfd, t_getsockopt->level,
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
	struct epoll_event ev;

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

                /* we have enough buffer to store this */
                memcpy(socket_t->start, t_senddata->data,
		       remaining);
                socket_t->start = socket_t->start + remaining;
		socket_t->len  += remaining;

		/* since the data is copied to the buffer, notify
		 * xeon phi that all data is sent. */
		rc = remaining;
		goto out;
	}

	/* do socket operation */
	/* XXX: large data splitting */
	rc = _pcn_senddata(socket_t->sockfd, t_senddata->data,
			   t_senddata->datalen, 0);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

	/* Entire data can't be sent. Enable backlog for this socket */
	if (rc < t_senddata->datalen) {
		remaining = t_senddata->datalen - rc;
		buf_remaining = MAX_HOST_BUFFER -
			(socket_t->start - socket_t->buf);

		if (remaining > buf_remaining) {
			/* send rc to xeon phi */
			goto err_out;
		}

		/* we have enough buffer to store this */
		memcpy(socket_t->start, (t_senddata->data + rc),
		       remaining);
		socket_t->start = socket_t->start + remaining;
		socket_t->backlog = 1;
		socket_t->len += remaining;

		/* enable EPOLLOUT on the socket */
        	ev.events = EPOLLOUT;
        	ev.data.fd = socket_t->sockfd;

		/* since the data is copied to the buffer, notify
		 * xeon phi that all data is sent. */
		rc = t_senddata->datalen;

        	rc1 = _pcn_epoll_ctl(netctx->epoll_fd, EPOLL_CTL_MOD,
				     socket_t->sockfd, &ev);
        	if (rc1 == -1) {
                	rc1 = -errno;
			/*  we still need to send response to xeon-phi */
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
	struct pcn_socktbl_t *socktbl = NULL;
	struct epoll_event ev;
	int epfd = netctx->epoll_fd;
	int rc;

	/* stop polling a closing socket */
        rc = _pcn_epoll_ctl(epfd, EPOLL_CTL_DEL, socket_t->sockfd, &ev);
        if (rc == -1) {
                rc = -errno;
                goto err_out0;
        }

	/* remove the socket_t */
	socktbl = netctx->psocktbls[remote_id];
	pcn_socktbl_del_lock(socktbl, &socket_t->list);

	/* unregister if it is a shared listening socket */
	if (_pcn_num_listening_ends(socket_t->remote_id_bits) > 1) {
		_pcn_unregister_listening_socket(netctx->proxy, socket_t);
	}

	/* reply to the remote end */
	rc = response_netmsg(t_close, r_close, rc);
	if (rc)
		goto err_out1;

	/* close the fd */
	netctx->sockid_remid[socket_t->sockfd].sock = NULL;
	smp_wmb();
	rc = _pcn_close(netctx, socket_t->sockfd);
	if (rc == -1) {
		rc = -errno;
		goto err_out2;
	}
out:
	/* free the socket_t */
	free(socket_t);
	return rc;
err_out0:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
err_out1:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
err_out2:
	pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	goto out;
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

	/* Get the size of message with the header */
	netmsg_size = netmsg_marshal_e_recvdata(1, NULL, NETMSG_UNKNOWN_SEQNUM,
						0, 0, data_len, NULL);

	ring_buffer_put_req_init(&out_msg, BLOCKING, netmsg_size);

	while ((rc =
		ring_buffer_scif_put_nolock(
			&channel->out_q,
			&out_msg)) == -EAGAIN) {
		pcn_yield();
	}

	/* copy the entire message len */
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
					 channel->out_q_seqnum++,
					 s->tag, socket_t_to_sockid(s),
					 data_len, NULL);

	ring_buffer_scif_elm_set_ready(&channel->out_q, msg_header);
	return size;
}

static
int _pcn_handle_read_ev(struct pcn_netctx_t *netctx, struct pcn_socket_t *socket_t)
{
	char *buf;
	int rc, netmsg_size;

	if ((socket_t->type == PCN_SOCK_TYPE_IN) &&
	    (!socket_t->tag_available)) {
		/* we will get the read event again !! */
		return 0;
	}

	while (1) {
		/* allocate buffer from the ring buffer */
		buf = _pcn_get_read_buffer(netctx, socket_t,
					   MAX_READ_LEN,
					   &netmsg_size);

		rc = _pcn_recv(socket_t->sockfd, buf, MAX_READ_LEN, 0);
        	if (rc == -1) {
			/* send xeon-phi null data */
			_pcn_send_read_ev(netctx, socket_t, buf,
				  	  0, netmsg_size);
                	rc = -errno;
                	goto err_out;
        	}

		_pcn_send_read_ev(netctx, socket_t, buf,
				  rc, netmsg_size);

		if (rc < MAX_READ_LEN)
			break;
	}

	/* check black buck count */
	if (rc > 0)
		socket_t->black_duck_cnt = 0;
	else
		++socket_t->black_duck_cnt;

#if 0
	if (socket_t->black_duck_cnt > MAX_BLACK_DUCK_CNT) {
		/* stop polling a closing socket */
		struct epoll_event ev;
		_pcn_epoll_ctl(netctx->epoll_fd, EPOLL_CTL_DEL,
			       socket_t->sockfd, &ev);
		pcn_err("drop connection: read(fd: %d) -> (rc, %d) (black_duck_cnt, %d)\n",
			socket_t->sockfd, rc, socket_t->black_duck_cnt);
	}
	else {
		pcn_dbg("handle_event: read(fd: %d) -> (rc, %d) (black_duck_cnt, %d)\n",
			socket_t->sockfd, rc, socket_t->black_duck_cnt);
	}
#endif
	return rc;
err_out:
        pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return 0;
}

static
int _pcn_handle_write_ev(struct pcn_netctx_t *netctx, struct pcn_socket_t *socket_t)
{
	int rc, rc1;
	struct epoll_event ev;

        pcn_assert(socket_t->type == PCN_SOCK_TYPE_IN ||
                   socket_t->type == PCN_SOCK_TYPE_OUT,
                   "incorrect socket type");

	/* Handle backlog */
	if (socket_t->backlog) {
		rc = _pcn_senddata(socket_t->sockfd, socket_t->send,
				   socket_t->len, 0);

        	if (rc == -1) {
                	rc = -errno;
                	goto err_out;
		}

		if (rc == socket_t->len) {
			socket_t->backlog = 0;
			socket_t->start = socket_t->buf;
			socket_t->send  = socket_t->buf;
			socket_t->len = 0;
			/* send backlog reset to xeon-phi */
		}

		if (rc < socket_t->len) {
			socket_t->send  += rc;
			socket_t->len   -= rc;

			/* enable EPOLLOUT on the socket */
			ev.events = EPOLLOUT;
			ev.data.fd = socket_t->sockfd;

			rc1 = _pcn_epoll_ctl(netctx->epoll_fd, EPOLL_CTL_MOD,
					     socket_t->sockfd, &ev);
			if (rc1 == -1) {
				rc1 = -errno;
				/* we still need to send response to xeon-phi */
				goto err_out;
			}
		}
	}

	pcn_dbg("handle_event: write(fd: %d) -> (rc, %d)\n", socket_t->sockfd, rc);
	return rc;
err_out:
        pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
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

        netmsg_size = netmsg_marshal_e_accept(1, NULL, NETMSG_UNKNOWN_SEQNUM,
					      0, 0, 0, NULL);

	ring_buffer_put_req_init(&out_msg, BLOCKING, netmsg_size);

        while ((rc =
                ring_buffer_scif_put_nolock(
                        &channel->out_q,
                        &out_msg)) == -EAGAIN) {
                pcn_yield();
        }

	netmsg_marshal_e_accept(0, out_msg.data,
				channel->out_q_seqnum++,
				s->tag, socket_t_to_sockid(s),
				s->addrlen, &s->addr);

	ring_buffer_scif_elm_set_ready(&channel->out_q,
				       out_msg.data);
}

static
void _pcn_handle_accept_ev_x(struct pcn_netctx_t *netctx,
			     struct pcn_socket_t *listen,
			     int fd, struct sockaddr_in *addr,
			     socklen_t *addrlen)
{
	struct epoll_event ev;
        struct pcn_socket_t *socket_t = NULL, *listen2;
	struct pcn_socktbl_t *socktbl;
	int rc, remote_id;
	int epfd = netctx->epoll_fd;

	/* alloc & init socket */
	socket_t = calloc(1, sizeof(*socket_t));
	if (!socket_t) {
		rc = -ENOMEM;
		goto err_out;
	}

	listen = _pcn_next_remote_socket(netctx->proxy, listen);
	remote_id = listen->remote_id;
	socket_t->remote_id = remote_id;
	socket_t->netctx    = netctx;
	socket_t->type      = PCN_SOCK_TYPE_IN;
	socket_t->sockfd    = fd;
	socket_t->addr      = *addr;
	socket_t->addrlen   = *addrlen;
        socket_t->start     = socket_t->buf;
        socket_t->send      = socket_t->buf;
	socket_t->tag       = listen->tag;

	/* set this socket as non-blocking */
	_pcn_set_socket_non_blocking(fd);

        /* add to the list */
        socktbl = netctx->psocktbls[remote_id];
	pcn_socktbl_add_lock(socktbl, &socket_t->list, socket_t->type);

	/* Add these FD to the EPOLL context FD */
	ev.events = EPOLLIN;
	ev.data.fd = socket_t->sockfd;

	rc = _pcn_epoll_ctl(epfd, EPOLL_CTL_ADD, socket_t->sockfd, &ev);
	if (rc == -1) {
		rc = -errno;
		goto err_out;
	}

	/* Add mapping from sockid to remote_id */
	netctx->sockid_remid[fd].remote_id = remote_id;
	netctx->sockid_remid[fd].sock      = socket_t;

	/* maximize send buffer to available max */
	_pcn_maximize_sndbuf(fd);

	/* send accept command to the appropriate client */
	_pcn_send_accept_ev(netctx, socket_t);

	/* close a zombie fd */
	_pcn_close(netctx, 0);
	pcn_dbg("handle_event: accept() -> (socket:%p, remote_id: %d, fd:%d)\n",
		socket_t, remote_id, fd); /* for statistics of load balancing */
	return;
err_out:
        pcn_err("[%s:%d] rc = %d\n", __func__, __LINE__, rc);
	return;
}

static
void _pcn_handle_accept_ev(struct pcn_netctx_t *netctx, struct pcn_socket_t *socket_t)
{
	struct sockaddr_in addr;
	socklen_t addrlen;
	int i, new_fd;

	/* Get new incoming connections */
	addrlen = sizeof(addr);
	new_fd = accept(socket_t->sockfd, (struct sockaddr *) &addr,
			&addrlen);
	if (new_fd >= 0) {
		/* add new_fd to particular xeon-phi. */
		_pcn_handle_accept_ev_x(netctx, socket_t, new_fd,
					&addr, &addrlen);
	}
}

static
int _pcn_netctx_process_sockets(struct pcn_netctx_t *netctx)
{
	int i, nevents;

	nevents	= _pcn_epoll_wait(netctx->epoll_fd, netctx->events,
			          PCN_MAX_EVENTS, 0);
	for (i = 0; i < nevents; i++) {
		struct epoll_event *event = &netctx->events[i];
		int fd = event->data.fd;
		struct pcn_socket_t *socket_t = netctx->sockid_remid[fd].sock;

		if (socket_t == NULL)
			continue;

		if (socket_t->type == PCN_SOCK_TYPE_LISTEN) {
			_pcn_handle_accept_ev(netctx, socket_t);
		}
		else if (event->events & EPOLLIN) {
			_pcn_handle_read_ev(netctx, socket_t);
		}
		else if (event->events & EPOLLOUT) {
			_pcn_handle_write_ev(netctx, socket_t);
		}
	}
	return nevents;
}
