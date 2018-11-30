#include "pcnlink_i.h"
#include "pcnlink_eventpoll.h"
#include <pcnporting.h>
#include <pcnlink-api.h>
#include <arch.h>

static struct pcnlink_context_t gctx;

int pcnlink_up(int pcnsrv_id, int pcnsrv_port,
	       int pcnlink_id, int pcnlink_port,
	       int qsize)
{
	struct pcn_conn_info_t ci, ci2;
	struct conn_endp_t pcnsrv = {.epd = SCIF_OPEN_FAILED};
	uint32_t ack;
	int link_num_ports;
	int rc = 0;

	pcn_dbg("(pcnsrv_id, %d) (pcnsrv_port, %d) "
		"(pcnlink_id, %d) (pcnlink_port, %d) "
		"(qsize, %d)\n",
		pcnsrv_id, pcnsrv_port,
		pcnlink_id, pcnlink_port, qsize);

	/* init context */
	memset(&gctx, 0, sizeof(gctx));
	gctx.pcnsrv.node           = pcnsrv_id;
	gctx.pcnsrv.port           = pcnsrv_port;
	gctx.pcnclnt.node          = pcnlink_id;
	gctx.pcnclnt.port          = pcnlink_port;
	gctx.per_channel_tx_q_size = qsize;

	/* scif connect to pcnsrv */
	pcnsrv.epd = scif_open();
	if (pcnsrv.epd == SCIF_OPEN_FAILED) {
		rc = -ENOMEM;
		goto err_out;
	}

	rc = scif_bind(pcnsrv.epd, gctx.pcnclnt.port);
	rc = __conv_scif_ret(rc);
	if (rc < 0)
		goto err_out;

	rc = scif_connect(pcnsrv.epd, &gctx.pcnsrv);
	rc = __conv_scif_ret(rc);
	if (rc < 0)
		goto err_out;

	/* receive the nubmer of channels */
	rc = scif_recv(pcnsrv.epd, &gctx.num_channel,
		       sizeof(gctx.num_channel),
		       SCIF_RECV_BLOCK);
	rc = __conv_scif_ret(rc);
	if (rc < 0)
		goto err_out;
	link_num_ports = gctx.num_channel * PCN_CHANNEL_NUM_PORTS;

	/* provide my connection information */
	ci.remote_id   = gctx.pcnsrv.node;
	ci.remote_port = gctx.pcnclnt.port + 1;
	ci.local_port  = ci.remote_port +
		((gctx.pcnclnt.node + 1) * link_num_ports);

	pcn_dbg("Local link init: "
		"(remote_id, %d) (remote_port, %d) (local_port, %d)\n",
		ci.remote_id, ci.remote_port, ci.local_port);

	ci2.remote_id   = gctx.pcnclnt.node;
	ci2.remote_port = ci.local_port;
	ci2.local_port  = ci.remote_port;
	rc = scif_send(pcnsrv.epd, &ci2, sizeof(ci2), SCIF_SEND_BLOCK);
	rc = __conv_scif_ret(rc);
	if (rc < 0)
		goto err_out;
	pcn_dbg("Remote link init: "
		"(remote_id, %d) (remote_port, %d) (local_port, %d)\n",
		ci2.remote_id, ci2.remote_port, ci2.local_port);

	/* create a link */
	rc = pcn_init_link(&ci, &pcnsrv,
			   gctx.num_channel,
			   gctx.per_channel_tx_q_size,
			   _pcnlink_reap_in_q,
			   &gctx.link);
	if (rc)
		goto err_out;

	/* create and start a rx worker */
#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
	rc = pthread_create(&gctx.rx_worker, NULL,
			    _pcnlink_rx_worker, &gctx);
	if (rc)
		goto err_out;
#else
	gctx.rx_worker = kthread_run(_pcnlink_rx_worker_kernel,
				     &gctx,
				     "pcnlink-rx-worker");
	if (IS_ERR(gctx.rx_worker)) {
		rc = PTR_ERR(gctx.rx_worker);
		goto err_out;
	}
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */


	/* get ack */
	rc = scif_recv(pcnsrv.epd, &ack, sizeof(ack), SCIF_RECV_BLOCK);
	rc = __conv_scif_ret(rc);
	if (rc < 0)
		goto err_out;

	/* close scif connection */
	scif_close(pcnsrv.epd);
	return 0;
err_out:
	pcn_err("rc = %d\n", rc);
	if (pcnsrv.epd != SCIF_OPEN_FAILED)
		scif_close(pcnsrv.epd);
	pcnlink_down();
	return rc;
}

void pcnlink_down(void)
{
	/* stop the rx_worker */
#ifdef PCIE_CLOUD_NETWORK_CONF_KERNEL
	kthread_stop(gctx.rx_worker);
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

	gctx.stop_rx_worker = 1;

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
	pthread_join(gctx.rx_worker, NULL);
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

	/* deinit lnik */
	pcn_deinit_link(&gctx.link);

	/* wipe out gctx */
	memset(&gctx, 0, sizeof(gctx));
}

#ifdef PCIE_CLOUD_NETWORK_CONF_KERNEL
static
int _pcnlink_rx_worker_kernel(void *p)
{
	_pcnlink_rx_worker(p);

	if (!kthread_should_stop())
		complete_and_exit(NULL, 0);
	return 0;
}
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

static
void *_pcnlink_rx_worker(void *p)
{
	struct pcnlink_context_t *ctx = p;
	int rc = 0;

	/* run until stopped */
	while (!gctx.stop_rx_worker
#ifdef PCIE_CLOUD_NETWORK_CONF_KERNEL
	       && !kthread_should_stop()
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */
		) {
		/* one sweep of rx channels */
		rc = _pcnlink_fetch_channels(ctx);
		if (rc < 0)
			goto err_out;

		/* if none are active, yield */
		if (rc == 0)
			pcn_yield();
	}
	rc = 0;
out:
	pcn_dbg("RX worker is terminated with %d\n", rc);
	return NULL;
err_out:
	pcn_err("rc = %d\n", rc);
	goto out;
}

static inline
int is_plsock_non_blocking(struct pcnlink_socket_t *plsock)
{
#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
	return plsock->__file_f_flags & O_NONBLOCK;
#else
	return plsock->sk.sk_socket->file->f_flags & O_NONBLOCK;
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */
}

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
void plsock_data_ready(struct pcnlink_socket_t *plsock)
{
	/* do nothing */
}
#else
/**
 * NOTE: a kernel module should provide this function.
 */
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

static
struct ring_buffer_t *get_plsock_rx_q(struct pcnlink_socket_t *plsock,
				      uint32_t command)
{
	volatile struct ring_buffer_t *rb = NULL;
	int i;

	for (i = 0; i < 2; ++i) {
		switch(command) {
		case PCN_NETMSG_E_ACCEPT:
		case PCN_NETMSG_E_RECVDATA:
			rb = ACCESS_ONCE(plsock->rx_e_q);
			break;
		default:
			rb = ACCESS_ONCE(plsock->rx_r_q);
			break;
		}
		if (rb)
			break;
		smp_mb();
	}

	if (!rb) {
		pcn_err("invalid per-socket rx_q: "
			"(command, %d) (plsock, %p, %x), (rb, %p), (e_q, %p), (r_q, %p)\n",
			command, plsock, plsock == NULL ? 0 : plsock->__deinit_status,
			rb, plsock->rx_e_q, plsock->rx_r_q);
		WARN_ON(1);
	}
	return (struct ring_buffer_t *)rb;
}

static
void *_pcnlink_oob_malloc(struct pcn_channel_t *channel, ssize_t size)
{
	void *ptr = pcn_malloc(size);
	if (ptr) {
		smp_faa(&channel->oob_size, -size);
	}
	return ptr;
}

static
void _pcnlink_oob_free(struct pcn_channel_t *channel, void *ptr, ssize_t size)
{
	if (ptr) {
		pcn_free(ptr);
		smp_faa(&channel->oob_size, size);
	}
}

static
size_t _pcnlink_need_reap_in_q(struct pcn_channel_t *channel)
{
	struct ring_buffer_scif_t *in_q = &channel->in_q;
	size_t free_size;

	/* check oob quota */
	if (ACCESS_ONCE(channel->oob_size) <= 0) {
		smp_rmb();
		if (ACCESS_ONCE(channel->oob_size) <= 0)
			return 0;
	}

	/* if there are some rooms to put, don't reap now */
	free_size = ring_buffer_scif_free_space(in_q);
	if (free_size > PCNLINK_IN_Q_REAP_THRESHOLD)
		return 0;

	/* if there are something to get out, don't reap now */
	if ( !ring_buffer_scif_is_empty(in_q) )
		return 0;

	/* reap the minimum at a time */
	return free_size + 1;
}

static
void _pcnlink_reap_in_q(void *_channel, void *_netmsg)
{
	/* TODO
	 * - can we check if tail2 and _netmsg are valid?
	 * - can we check if copied netmsg is valid?
	 */
	struct pcn_channel_t *channel = _channel;
	struct netmsg_header_t *netmsg = _netmsg;
	struct netmsg_header_t netmsg_copy;
	struct ring_buffer_scif_t *in_q = &channel->in_q;
	struct pcnlink_rx_netmsg_t *oob_rx_netmsg = NULL;
        struct netmsg_header_t *oob_netmsg;
	struct pcnlink_socket_t *plsock;
        uint32_t msize;
	int rc;

	/* check if the netmsg is already processed */
	netmsg_copy = *netmsg;
	if (!ring_buffer_scif_elm_valid(in_q, netmsg))
		return;

	/* copy the netmsg */
	/* - alloc a buffer */
	msize = netmsg_copy.size + sizeof(*oob_rx_netmsg);
	oob_rx_netmsg = _pcnlink_oob_malloc(channel, msize);
	if (oob_rx_netmsg == NULL)
		return;
	oob_netmsg = (struct netmsg_header_t *)&oob_rx_netmsg[1];

	/* - copy the orginal netmsg to the oob_netmsg */
	rc = copy_from_ring_buffer_scif(in_q, oob_netmsg, netmsg,
					netmsg_copy.size);
	if (rc)
		goto free_out;

	/* complete constructing oob_rx_netmsg */
	oob_rx_netmsg->channel = channel;
	oob_rx_netmsg->netmsg = oob_netmsg;
	oob_rx_netmsg->netmsg_hdr = *oob_netmsg;
	plsock = tag_to_plsock(oob_netmsg->tag);

	pcn_spin_lock(&channel->lock); {
		/* double check if the netmsg is processed in the meantime */
		if ( netmsg_copy.seqnum != netmsg->seqnum ||
		     !ring_buffer_scif_elm_valid(in_q, netmsg) ) {
			pcn_spin_unlock(&channel->lock);
			goto free_out;
		}

		/* publish this oob netmsg to the socket oob_e_q */
		spsc_q_put(&plsock->oob_e_q, &oob_rx_netmsg->oob_link);

		/* okay, let the original netmsg go */
		ring_buffer_scif_elm_set_done(in_q, netmsg);
		pcn_dbg("reap: (seqnum, %x) (cmd, %d) (size, %u) (plsock, %p) (oob_rx_netmsg, %p)\n",
			(unsigned int)oob_netmsg->seqnum, oob_netmsg->command,
			oob_netmsg->size, plsock, oob_rx_netmsg);
	} pcn_spin_unlock(&channel->lock);
	return;
free_out:
	_pcnlink_oob_free(channel, oob_rx_netmsg, msize);
}

static
int _pcnlink_fetch_channels(struct pcnlink_context_t *ctx)
{
	enum { MAX_BATCH = 32 };
	struct pcn_link_t *link = &gctx.link;
	int num_channel = link->num_channel;
	struct pcnlink_socket_t *plsock;
        int num_active_channels = 0;
	int i, j, rc;
#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
	void *ep = NULL;
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

	/* for each channel */
	for (i = 0; i < num_channel; ++i) {
		struct pcn_channel_t *channel = &link->channels[i];
		struct pcnlink_rx_netmsg_t rx_netmsg;
                struct ring_buffer_req_t q_req;

                /* if a channel is not established yet */
                if ( unlikely(!channel) )
                        goto next_channel;

		/* per-channel batching */
		for (j = 0; j < MAX_BATCH; ++j) {
			struct netmsg_header_t netmsg_hdr;
			struct ring_buffer_t *rx_q;

			/* get a netmsg from in_q of this channel */
			ring_buffer_get_req_init(&q_req, BLOCKING);
			rc = ring_buffer_scif_get_nolock(&channel->in_q,
							 &q_req);
			if (rc) {
				/* no new message in this channel */
				if (rc == -EAGAIN) {
#ifdef PCIE_CLOUD_NETWORK_CONF_OOB_FETCH
					size_t reap_size = _pcnlink_need_reap_in_q(channel);
					if (reap_size > 0) {
						size_t free_size;
						free_size = ring_buffer_scif_secure_free_space(
							&channel->in_q, reap_size);
						if (reap_size <= free_size) {
							pcn_wrn("force garbage collection: "
								"(free, %lu) (quota, %d) \n",
								free_size - reap_size,
								(int)ACCESS_ONCE(channel->oob_size));
						}
					}
#endif /* PCIE_CLOUD_NETWORK_CONF_OOB_FETCH */
					goto next_channel;
				}
				/* oops, something goes wrong */
				goto err_out;
			}
			rc = copy_from_ring_buffer_scif(&channel->in_q,
							&netmsg_hdr,
							q_req.data,
							sizeof(netmsg_hdr));
			if (rc)
				goto err_out;

			/* if plsock is being destructed, drop out */
			plsock = tag_to_plsock(netmsg_hdr.tag);
			rx_q = get_plsock_rx_q(plsock, netmsg_hdr.command);
			if (rx_q == NULL) {
				pcn_dbg("drop out packets of destructing socket "
					"(addr, %p) (cmd, %d)\n",
					plsock, netmsg_hdr.command);
				ring_buffer_scif_elm_set_done(&channel->in_q, q_req.data);
				continue;
			}

			/* early drop out zero-sized receive data */
			if (netmsg_hdr.command == PCN_NETMSG_E_RECVDATA) {
				struct netmsg_e_recvdata_t *e_recvdata = q_req.data;
				uint32_t datalen = e_recvdata->datalen;
				if (datalen == 0) {
					pcn_dbg("ignore zero-sized recvdata: %d\n", datalen);
					ring_buffer_scif_elm_set_done(&channel->in_q, q_req.data);
					continue;
				}
			}

			/* enqueue the netmsg to the rx_q
			 * of the corresponding socket */
			rx_netmsg.channel     = channel;
			rx_netmsg.netmsg_hdr  = netmsg_hdr;
			rx_netmsg.netmsg      = q_req.data;
			ring_buffer_put_req_init(&q_req,
						 BLOCKING,
						 sizeof(rx_netmsg));
			while ( (rc =
				 ring_buffer_put_nolock(
					 rx_q, &q_req)) == -EAGAIN) {
				pcn_yield();
			}
			if (rc)
				goto err_out;
			rc = copy_to_ring_buffer(rx_q,
						 q_req.data,
						 &rx_netmsg,
						 sizeof(rx_netmsg));
			if (rc)
				goto err_out;
			ring_buffer_elm_set_ready(rx_q, q_req.data);
			pcn_dbg2("put netmsg: (plsock, %p) (command, %d)\n",
				 plsock, rx_netmsg.netmsg_hdr.command);

			/* yeah, data is ready for the plsock */
			plsock_data_ready(plsock);

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
			if ((netmsg_hdr.command == PCN_NETMSG_E_ACCEPT) ||
			    (netmsg_hdr.command == PCN_NETMSG_E_RECVDATA)) {
				pcnlink_epoll_add_event(plsock);
			}
			ep = (void *) plsock->ep;
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

			/* increase the number of active channels */
			++num_active_channels;
		}
	next_channel: ;
        }

	/* enforce new netmsgs to be seen by waiters */
	if (num_active_channels)
		smp_wmb();

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
	/* now signal the epoll waiter to handle the events */
	pcnlink_epoll_signal(ep);
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

        return num_active_channels;
err_out:
	pcn_err("rc = %d\n", rc);
        return rc;
}

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
struct pcnlink_socket_t *plsock_alloc(void *private1, void *private2)
{
	return pcn_malloc(sizeof(struct pcnlink_socket_t));
}
#else
/**
 * NOTE: a kernel module should provide this function.
 */
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

static
int new_plsock(struct pcnlink_socket_t **pplsock,
	       void *private1, void *private2)
{
	struct pcnlink_socket_t *plsock = NULL;
	struct ring_buffer_t *rx_r_q;
	struct ring_buffer_t *rx_e_q;
	int rc = 0;

	/* alloc plsock */
	plsock = plsock_alloc(private1, private2);
	if (!plsock) {
		rc = -ENOMEM;
		goto err_out;
	}
	memset(plsock->__sos, 0, plsock_memset_size(plsock));
	INIT_LIST_HEAD(&plsock->list);

	rc = ring_buffer_create(PAGE_SIZE, sizeof(void *),
				0 /* non-blocking */,
				NULL, NULL, &rx_r_q);
	if (rc)
		goto err_out;
	ACCESS_ONCE(plsock->rx_r_q) = rx_r_q;

	rc = ring_buffer_create(PAGE_SIZE * 2, sizeof(void *),
				0 /* non-blocking */,
				NULL, NULL, &rx_e_q);
	if (rc)
		goto err_out;
	ACCESS_ONCE(plsock->rx_e_q) = rx_e_q;
	smp_mb();

	/* pass it out */
	*pplsock = plsock;
	return rc;
err_out:
	pcn_err("rc = %d\n", rc);
	delete_plsock(plsock);
	return rc;
}

static
void plsock_drain(struct ring_buffer_t *rx_qs[], int num)
{
      	struct ring_buffer_req_t q_req;
	struct ring_buffer_t *rx_q;
	int i, cont = 1;

	while (cont) {
		cont = 0;
		for (i = 0; i < num; ++i) {
			rx_q = rx_qs[i];
			if (rx_q == NULL)
				continue;
			ring_buffer_get_req_init(&q_req, BLOCKING);
			while (ring_buffer_get_nolock(rx_q, &q_req) != -EAGAIN) {
				ring_buffer_elm_set_done(rx_q, q_req.data);
				ring_buffer_get_req_init(&q_req, BLOCKING);
				++cont;
			}
		}
	}
}

void plsock_deinit(struct pcnlink_socket_t *plsock)
{
	struct ring_buffer_t *rx_qs[] = {plsock->rx_r_q, plsock->rx_e_q};
	const int num = sizeof(rx_qs)/sizeof(rx_qs[0]);
	struct pcnlink_last_recv_t *lastrcv = &plsock->lastrcv;
	struct pcnlink_rx_netmsg_t *rx_netmsg = &lastrcv->rx_netmsg;
	struct spsc_queue oob_e_q = plsock->oob_e_q;
	struct spsc_node *oob_node;
	int i;

	pcn_dbg("(plsock, %p)\n", plsock);

	/* make queues inaccessable */
	ACCESS_ONCE(plsock->rx_r_q) = NULL;
	ACCESS_ONCE(plsock->rx_e_q) = NULL;
	memset(&plsock->oob_e_q, 0, sizeof(plsock->oob_e_q));
	ACCESS_ONCE(plsock->__deinit_status) = 0xDEADBEEF;
	smp_mb();

	/* drain in/out queues */
	if (lastrcv->is_valid) {
		/* make this chunk done */
		ring_buffer_scif_elm_set_done(&rx_netmsg->channel->in_q,
					      rx_netmsg->netmsg);
		/* invalidate lastrcv */
		lastrcv->is_valid = 0;
		lastrcv->is_oob = 0;
	}
	plsock_drain(rx_qs, num);

	/* detroy oob elements */
	while ( (oob_node = spsc_q_get(&oob_e_q)) ) {
		struct pcnlink_rx_netmsg_t *oob_netmsg = container_of(
			oob_node, struct pcnlink_rx_netmsg_t, oob_link);
		pcn_free(oob_netmsg);
	}

	/* destroy in/out queues */
	for (i = 0; i < num; ++i) {
		if (rx_qs[i]) {
			ring_buffer_destroy(rx_qs[i]);
		}
	}

}

static
void delete_plsock(struct pcnlink_socket_t *plsock)
{
	/* sanity check */
	if (!plsock)
		return;

	/* deinit and free plsock */
	plsock_deinit(plsock);

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
	/* In kernel mode, alloc/dealloc plsock is done
	 * at the af_pcnlink layer. */
        pcn_free(plsock);
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */
}

static
struct pcn_channel_t *assign_channel(void *seed_ptr)
{
	unsigned long seed64;
	unsigned int  seed32;
	int channel_id;

	seed64 = (unsigned long)seed_ptr;
	seed32 = seed64 ^ (seed64 >> 6);
	channel_id = rand32(&seed32) % gctx.num_channel;
	return &gctx.link.channels[channel_id];
}

static
int is_plsock_ok(struct pcnlink_socket_t *plsock)
{
#ifdef PCIE_CLOUD_NETWORK_DEBUG
	if (ACCESS_ONCE(plsock->rx_e_q) && ACCESS_ONCE(plsock->rx_r_q))
		return 1; /* ok */

	smp_mb();
	if (ACCESS_ONCE(plsock->rx_e_q) && ACCESS_ONCE(plsock->rx_r_q))
		return 1; /* ok */

	pcn_err("plsock is not valid: "
		"(plsock, %p, %x) (channel, %p) (e_q, %p) (r_q, %p)\n",
		plsock, plsock == NULL ? 0 : plsock->__deinit_status,
		plsock->channel, plsock->rx_e_q, plsock->rx_r_q);
	return 0; /* nok */
#else
	return 1;
#endif /* PCIE_CLOUD_NETWORK_DEBUG */
}

#define pcnlink_rpc(__plsock, __t_type, __R_TYPE, __r_buff, __r_buff_len, ...) ({ \
	int __rc, __netmsg_size;			                \
      	struct ring_buffer_req_t __q_req;		                \
	struct ring_buffer_t *__rx_q;					\
	struct pcnlink_rx_netmsg_t __rx_netmsg = {			\
		.channel = NULL, .netmsg = NULL};			\
	/* put a request */						\
	pcn_here();							\
	if (!is_plsock_ok(__plsock))					\
		WARN_ON(1);						\
	__netmsg_size = netmsg_marshal_##__t_type(			\
		1, NULL, NETMSG_UNKNOWN_SEQNUM, __VA_ARGS__);		\
        ring_buffer_put_req_init(&__q_req,			        \
				 BLOCKING,				\
				 __netmsg_size);			\
	while ( (__rc = ring_buffer_scif_put(				\
			&(__plsock)->channel->out_q,			\
			&__q_req)) == -EAGAIN ) {			\
		pcn_yield();						\
	}								\
	if (__rc)							\
		goto __out;						\
	/* marsharl netmsg to the out_q and make it ready */		\
	netmsg_marshal_##__t_type(0, __q_req.data,			\
				  (__plsock)->channel->out_q_seqnum++,	\
				  __VA_ARGS__);				\
	ring_buffer_scif_elm_set_ready(&(__plsock)->channel->out_q,	\
				       __q_req.data);			\
	/* wait for its response */					\
	/* - get rx_netmsg */						\
	__rx_q = get_plsock_rx_q(__plsock, PCN_NETMSG_##__R_TYPE);	\
	ring_buffer_get_req_init(&__q_req, BLOCKING);			\
	while ( (__rc = ring_buffer_get_nolock(				\
			 __rx_q, &__q_req)) == -EAGAIN)	{		\
		pcn_yield();						\
	}								\
	if (__rc)							\
		goto __out;						\
	__rc = copy_from_ring_buffer(__rx_q, &__rx_netmsg,		\
				     __q_req.data, __q_req.size);	\
	if (__rc)							\
		goto __out;						\
	ring_buffer_elm_set_done(__rx_q, __q_req.data);			\
	/* - get r_socket message */					\
	__rc = copy_from_ring_buffer_scif(				\
		&__rx_netmsg.channel->in_q,				\
		__r_buff,						\
		__rx_netmsg.netmsg,					\
		min((int)__r_buff_len,					\
		    (int)__rx_netmsg.netmsg_hdr.size));			\
	if (__rc)							\
		goto __out;						\
	ring_buffer_scif_elm_set_done(&__rx_netmsg.channel->in_q,	\
				      __rx_netmsg.netmsg);		\
	(__plsock)->channel = __rx_netmsg.channel;			\
	__rc = 0;							\
__out:							                \
	pcn_dbg("rc = %d\n", __rc);					\
	__rc;								\
        })

struct pcnlink_socket_t *__pcnlink_socket(int domain, int type, int protocol)
{
	return __pcnlink_socket_ex(domain, type, protocol,
				   NULL, NULL, NULL);
}

struct pcnlink_socket_t *__pcnlink_socket_ex(int domain, int type, int protocol,
					     int *prc, void *private1, void *private2)
{
	struct pcnlink_socket_t *newpls = NULL;
	struct pcn_socktbl_t *socktbl;
	struct netmsg_r_socket_t r_socket;
	volatile struct pcn_channel_t **pchannel;
	int rc = 0;

	/* allocate newpls */
	rc = new_plsock(&newpls, private1, private2);
	if (rc)
		goto err_out;

	/* randomly select a channel */
	pchannel  = (volatile struct pcn_channel_t **)(&(newpls->channel));
	*pchannel = assign_channel(newpls);
	smp_wmb();

	// hack: type 3 is used for EPOLL. Don't send
	// any command to the host for EPOLL.
	if (type != 3) {

		/* do rpc */
		rc = pcnlink_rpc(newpls, t_socket, R_SOCKET, &r_socket,
				 sizeof(r_socket), plsock_to_tag(newpls),
				 domain, type, protocol);
		if (rc)
			goto err_out;
	}

	/* process t_socket */
	newpls->sockid = r_socket.sockid;
	newpls->type = PCN_SOCK_TYPE_NEW;
	socktbl = &gctx.link.socktbls[newpls->channel->channel_id];
	pcn_socktbl_add_lock(socktbl, &newpls->list, newpls->type);
	pcn_dbg("new sock: (plsock, %p), (sockid, %p)\n",
		newpls, (void *)newpls->sockid);

	if (prc)
		*prc = 0;
	return newpls;
err_out:
	pcn_err("rc = %d\n", rc);
	if (prc)
		*prc = rc;
	delete_plsock(newpls);
	return NULL;
}

int __pcnlink_bind(struct pcnlink_socket_t *plsock,
		   const struct sockaddr *addr, socklen_t addrlen)
{
	struct pcn_socktbl_t *socktbl;
	struct netmsg_r_bind_t r_bind;
	int rc;

	/* do rpc */
	rc = pcnlink_rpc(plsock, t_bind, R_BIND, &r_bind,
			 sizeof(r_bind), plsock_to_tag(plsock),
			 plsock->sockid, addrlen, (struct sockaddr_in *)addr);
	if (rc)
		goto err_out;

	/* process t_bind */
	socktbl = &gctx.link.socktbls[plsock->channel->channel_id];
	plsock->type = PCN_SOCK_TYPE_LISTEN;
	pcn_socktbl_move_lock(socktbl, &plsock->list, plsock->type);

	return (int)r_bind.re.rc;
err_out:
	pcn_err("rc = %d\n", rc);
	return rc;
}

int __pcnlink_connect(struct pcnlink_socket_t *plsock,
		      const struct sockaddr *addr, socklen_t addrlen)
{
	struct pcn_socktbl_t *socktbl;
	struct netmsg_r_connect_t r_connect;
	struct sockaddr_in *sin_addr = (struct sockaddr_in *)addr;
	int rc;

	/* do rpc */
	rc = pcnlink_rpc(plsock, t_connect, R_CONNECT, &r_connect,
			 sizeof(r_connect), plsock_to_tag(plsock),
			 plsock->sockid, addrlen, sin_addr);
	if (rc)
		goto err_out;

	/* process t_connect */
	plsock->sin = *sin_addr;
	socktbl = &gctx.link.socktbls[plsock->channel->channel_id];
	plsock->type = PCN_SOCK_TYPE_OUT;
	pcn_socktbl_move_lock(socktbl, &plsock->list, plsock->type);

	return (int)r_connect.re.rc;
err_out:
	pcn_err("rc = %d\n", rc);
	return rc;
}

int __pcnlink_listen(struct pcnlink_socket_t *plsock, int backlog)
{
	struct netmsg_r_listen_t r_listen;
	int rc;

	/* do rpc */
	rc = pcnlink_rpc(plsock, t_listen, R_LISTEN, &r_listen,
			 sizeof(r_listen), plsock_to_tag(plsock),
			 plsock->sockid, backlog);
	if (rc)
		goto err_out;

	return (int)r_listen.re.rc;
err_out:
	pcn_err("rc = %d\n", rc);
	return rc;
}

int __pcnlink_setsockopt(struct pcnlink_socket_t *plsock, int level,
			 int option_name, const void *option_value,
			 socklen_t option_len)
{
	struct netmsg_r_setsockopt_t r_setsockopt;
	int rc;

	/* do rpc */
	rc = pcnlink_rpc(plsock, t_setsockopt, R_SETSOCKOPT, &r_setsockopt,
			 sizeof(r_setsockopt), plsock_to_tag(plsock),
			 plsock->sockid, level, option_name,
			 option_len, option_value);
	if (rc)
		goto err_out;

	return (int)r_setsockopt.re.rc;
err_out:
	pcn_err("rc = %d\n", rc);
	return rc;
}

int __pcnlink_getsockopt(struct pcnlink_socket_t *plsock, int level,
			 int optname, void *optval, socklen_t *optlen)
{
	enum {MAX_OPTLEN = 1024};
	uint8_t buff[sizeof(struct netmsg_r_getsockopt_t) + MAX_OPTLEN];
	struct netmsg_r_getsockopt_t *r_getsockopt =
		(struct netmsg_r_getsockopt_t *)buff;
	uint32_t buff_optlen = min(MAX_OPTLEN, (int)*optlen);
	int rc;

	/* do rpc */
	rc = pcnlink_rpc(plsock, t_getsockopt, R_GETSOCKOPT, r_getsockopt,
			 sizeof(buff), plsock_to_tag(plsock),
			 plsock->sockid, level, optname, buff_optlen);
	if (rc)
		goto err_out;

	/* pass out optlen and optval */
	*optlen = buff_optlen;
	if ( copy_to_user(optval, r_getsockopt->optval, *optlen)) {
		rc = -EFAULT;
		goto err_out;
	}

	return (int)r_getsockopt->re.rc;
err_out:
	pcn_err("rc = %d\n", rc);
	return rc;
}

ssize_t __pcnlink_send(struct pcnlink_socket_t *plsock,
		       const void *buf, size_t len)
{
	struct netmsg_r_senddata_t r_senddata;
	int rc;

	/* do rpc */
	rc = pcnlink_rpc(plsock, t_senddata, R_SENDDATA, &r_senddata,
			 sizeof(r_senddata), plsock_to_tag(plsock),
			 plsock->sockid, len, buf);
	if (rc)
		goto err_out;

	return (int)r_senddata.re.rc;
err_out:
	pcn_err("rc = %d\n", rc);
	return rc;
}

int __pcnlink_shutdown(struct pcnlink_socket_t *plsock, int how)
{
	struct netmsg_r_shutdown_t r_shutdown;
	int rc;

	/* do rpc */
	rc = pcnlink_rpc(plsock, t_shutdown, R_SHUTDOWN, &r_shutdown,
			 sizeof(r_shutdown), plsock_to_tag(plsock),
			 plsock->sockid, how);
	if (rc)
		goto err_out;

	/* update poll_mask */
	if (!r_shutdown.re.rc) {
		/* XXX: is it precisely correct? */
		switch(how) {
		case SHUT_RD:
		case SHUT_WR:
			plsock->poll_mask |= POLLRDHUP;
			break;
		case SHUT_RDWR:
			plsock->poll_mask |= POLLHUP;
			break;
		}
	}

	return (int)r_shutdown.re.rc;
err_out:
	pcn_err("rc = %d\n", rc);
	return rc;
}

int __pcnlink_close(struct pcnlink_socket_t *plsock)
{
	struct pcn_socktbl_t *socktbl;
	struct netmsg_r_close_t r_close;
	int rc;

	/* do rpc */
	rc = pcnlink_rpc(plsock, t_close, R_CLOSE, &r_close,
			 sizeof(r_close), plsock_to_tag(plsock),
			 plsock->sockid);
	if (rc)
		goto err_out;

	/* process t_close */
	socktbl = &gctx.link.socktbls[plsock->channel->channel_id];
	pcn_socktbl_del_lock(socktbl, &plsock->list);
	delete_plsock(plsock);

	return (int)r_close.re.rc;
err_out:
	pcn_err("rc = %d\n", rc);
	return rc;
}

ssize_t __pcnlink_sendmsg(struct pcnlink_socket_t *plsock,
			  const struct msghdr *msg, ssize_t len)
{
	struct iovec *iov = msg->msg_iov;
	ssize_t io_size, total_size = 0;
	ssize_t i, rc;

	for (i = 0; len > 0 && i < msg->msg_iovlen; ++i, ++iov) {
                if (iov->iov_len) {
			io_size = min((ssize_t)iov->iov_len, len);

			rc = __pcnlink_send(plsock, iov->iov_base, io_size);
			if (rc < 0)
				goto err_out;

			total_size    += io_size;
			len           -= io_size;
                        iov->iov_base += io_size;
                        iov->iov_len  -= io_size;
		}
	}
	return total_size;
err_out:
	pcn_err("rc = %ld\n", rc);
	return rc;
}

ssize_t __pcnlink_recvmsg(struct pcnlink_socket_t *plsock,
			  struct msghdr *msg, ssize_t len)
{
	struct iovec *iov = msg->msg_iov;
	ssize_t io_size, total_size = 0;
	ssize_t i, rc;

	for (i = 0; len > 0 && i < msg->msg_iovlen; ++i, ++iov) {
                if (iov->iov_len) {
			io_size = min((ssize_t)iov->iov_len, len);
			rc = __pcnlink_recv(plsock, iov->iov_base, io_size);
			if (rc < 0)
				goto err_out;
			total_size    += rc;
			len           -= rc;
                        iov->iov_base += rc;
                        iov->iov_len  -= rc;
		}
	}
	pcn_dbg("total_size: %ld\n", total_size);
	return total_size;
err_out:
	pcn_err("rc = %ld\n", rc);
	return rc;
}

struct pcnlink_socket_t *__pcnlink_accept(
	struct pcnlink_socket_t *plsock,
	struct sockaddr *addr, socklen_t *addrlen)
{
	return __pcnlink_accept_ex(plsock, addr, addrlen,
				   NULL, NULL, NULL);
}

static
struct pcnlink_rx_netmsg_t *find_netmsg_in_oob_e_q(
	struct pcnlink_socket_t *plsock,
	struct netmsg_header_t *netmsg_hdr)
{
	uint64_t seqnum = netmsg_hdr->seqnum;
	struct pcnlink_rx_netmsg_t *oob_netmsg;
	struct spsc_node *oob_node;

	/* peek the head */
	smp_rmb();
	for (oob_node = spsc_q_peek(&plsock->oob_e_q);
	     oob_node != NULL;
	     oob_node = spsc_q_peek(&plsock->oob_e_q)) {
		oob_netmsg = container_of(oob_node,
					  struct pcnlink_rx_netmsg_t,
					  oob_link);
		pcn_dbg("oob_node: %p, oob_netmsg: %p seqnum: %lx\n",
			oob_node, oob_netmsg, oob_netmsg->netmsg_hdr.seqnum);
		/* peeked head is not ready to consume */
		if (seqnum < oob_netmsg->netmsg_hdr.seqnum) {
			return NULL;
		}
		/* consume the head */
		spsc_q_get(&plsock->oob_e_q);
		/* - bingo! */
		if (seqnum == oob_netmsg->netmsg_hdr.seqnum) {
			pcn_dbg2("find oob: (seqnum, %x) (cmd, %d) (size, %u) (plsock, %p)\n",
				 (unsigned int)seqnum, oob_netmsg->netmsg_hdr.command,
				 oob_netmsg->netmsg_hdr.size, plsock);
			return oob_netmsg;
		}
		/* - already processed */
		pcn_free(oob_netmsg);
	}
	return NULL;
}

static
struct pcnlink_rx_netmsg_t *peek_netmsg_in_oob_e_q(
	struct pcnlink_socket_t *plsock,
	struct netmsg_header_t *netmsg_hdr)
{
	uint64_t seqnum = netmsg_hdr->seqnum;
	struct pcnlink_rx_netmsg_t *oob_netmsg;
	struct spsc_node *oob_node;

	/* peek the head */
	smp_rmb();
	for (oob_node = spsc_q_peek(&plsock->oob_e_q);
	     oob_node != NULL;
	     oob_node = spsc_q_peek(&plsock->oob_e_q)) {
		oob_netmsg = container_of(oob_node,
					  struct pcnlink_rx_netmsg_t,
					  oob_link);
		/* peeked head is not ready to consume */
		if (seqnum < oob_netmsg->netmsg_hdr.seqnum)
			return NULL;
		/* - bingo! */
		if (seqnum == oob_netmsg->netmsg_hdr.seqnum) {
			pcn_dbg2("peek oob: (seqnum, %x) (cmd, %d) (size, %u) (plsock, %p)\n",
				 (unsigned int)seqnum, oob_netmsg->netmsg_hdr.command,
				 oob_netmsg->netmsg_hdr.size, plsock);
			return oob_netmsg;
		}
		/* - already processed: consume the head */
		spsc_q_get(&plsock->oob_e_q);
		pcn_free(oob_netmsg);
	}
	return NULL;
}

static
int get_rx_netmsg_e_accept(
	struct pcnlink_socket_t *plsock,
	struct pcnlink_rx_netmsg_t *rx_netmsg,
	struct netmsg_e_accept_t *e_accept)
{
	int is_non_blocking = is_plsock_non_blocking(plsock);
	struct pcnlink_rx_netmsg_t *oob_rx_netmsg;
      	struct ring_buffer_req_t q_req;
	struct ring_buffer_t *rx_q;
	int rc;

	/* - get rx_netmsg */
	if (!is_plsock_ok(plsock))
		WARN_ON(1);
	rx_q = get_plsock_rx_q(plsock, PCN_NETMSG_E_ACCEPT);
	ring_buffer_get_req_init(&q_req, BLOCKING);
	while ( (rc = ring_buffer_get_nolock(
			 rx_q, &q_req)) == -EAGAIN) {
		/* a socket is non-blocking
		 * and there is no pending connection */
		if (is_non_blocking) {
			rc = -EAGAIN;
			goto err_out;
		}

		pcn_yield();
	}
	if (rc)
		goto err_out;
	rc = copy_from_ring_buffer(rx_q, rx_netmsg,
				   q_req.data, q_req.size);
	if (rc)
		goto err_out;
	ring_buffer_elm_set_done(rx_q, q_req.data);

	/* - check if the rx_netmsg is the oob_e_q */
retry_oob:
	oob_rx_netmsg = find_netmsg_in_oob_e_q(plsock,
					       &rx_netmsg->netmsg_hdr);
	if (oob_rx_netmsg) {
		/* - get e_accept message from the oob_rx_netmsg */
		size_t msize = sizeof(*oob_rx_netmsg) + sizeof(*e_accept);
		*rx_netmsg = oob_rx_netmsg[0];
		*e_accept = *((struct netmsg_e_accept_t *)&oob_rx_netmsg[1]);
		_pcnlink_oob_free(plsock->channel, oob_rx_netmsg, msize);
	}
	else {
		/* - get e_accept message from the channel */
		rc = copy_from_ring_buffer_scif(
			&rx_netmsg->channel->in_q,
			e_accept,
			rx_netmsg->netmsg,
			sizeof(*e_accept));
		if (rc)
			goto err_out;
		/* - check if there is an oob element then retry */
		pcn_spin_lock(&plsock->channel->lock); {
			if ( peek_netmsg_in_oob_e_q(plsock,
						    &rx_netmsg->netmsg_hdr) ) {
				pcn_spin_unlock(&plsock->channel->lock);
				smp_mb();
				goto retry_oob;
			}
			ring_buffer_scif_elm_set_done(&rx_netmsg->channel->in_q,
						      rx_netmsg->netmsg);
		} pcn_spin_unlock(&plsock->channel->lock);
	}
	return 0;
err_out:
	pcn_err("rc = %d\n", rc);
	return rc;
}


struct pcnlink_socket_t *__pcnlink_accept_ex(
	struct pcnlink_socket_t *plsock,
	struct sockaddr *addr, socklen_t *addrlen, int *prc,
	void *private1, void *private2)
{
	struct pcnlink_rx_netmsg_t rx_netmsg;
	struct netmsg_e_accept_t e_accept;
      	struct ring_buffer_req_t q_req;
	struct pcnlink_socket_t *newpls = NULL;
	struct pcn_socktbl_t *socktbl;
	int rc, netmsg_size;

	/* get a netmsg */
	pcn_here();
	if (!is_plsock_ok(plsock))
		WARN_ON(1);
	rc = get_rx_netmsg_e_accept(plsock, &rx_netmsg, &e_accept);
	if (rc)
		goto err_out;

	/* allocate newpls and put it into the IN list */
	rc = new_plsock(&newpls, private1, private2);
	if (rc)
		goto err_out;
	newpls->channel = rx_netmsg.channel;
	newpls->sockid  = e_accept.sockid;
	newpls->type    = PCN_SOCK_TYPE_IN;
	newpls->sin     = e_accept.addr;
	socktbl = &gctx.link.socktbls[newpls->channel->channel_id];
	pcn_socktbl_add_lock(socktbl, &newpls->list, newpls->type);
	pcn_dbg("new sock: (plsock, %p), (sockid, %p)\n",
		newpls, (void *)newpls->sockid);

	/* notify my tag id to host */
	netmsg_size = netmsg_marshal_n_socket(1, NULL, NETMSG_UNKNOWN_SEQNUM,
					      0, 0);
        ring_buffer_put_req_init(&q_req, BLOCKING, netmsg_size);
	while ( (rc = ring_buffer_scif_put(
			 &newpls->channel->out_q, &q_req)) == -EAGAIN) {
		pcn_yield();
	}
	if (rc)
		goto err_out;
	netmsg_marshal_n_socket(0, q_req.data,
				newpls->channel->out_q_seqnum++,
				plsock_to_tag(newpls),
				newpls->sockid);
	ring_buffer_scif_elm_set_ready(&newpls->channel->out_q,
				       q_req.data);

	/* pass out addr and addrlen */
	if (addrlen)
		*addrlen = e_accept.addrlen;
	if (addr) {
		if ( copy_to_user(addr, &e_accept.addr, e_accept.addrlen)) {
			goto err_out;
		}
	}

	if (prc)
		*prc = 0;
	pcn_dbg("newpls = %p\n", newpls);
	return newpls;
err_out:
	delete_plsock(newpls);
	pcn_err("rc = %d\n", rc);
	if (prc)
		*prc = rc;
	return NULL;
}


#ifdef PCIE_CLOUD_NETWORK_CONF_KERNEL
struct _pin_hanlde {
	int nr_pages;
	struct page *pages[0];
};

static
int _get_nr_pages(char *data, int len)
{
	unsigned long start_page, end_page;
	start_page =  (unsigned long)data >> PAGE_SHIFT;
	end_page = ((unsigned long)data + len + PAGE_SIZE - 1) >> PAGE_SHIFT;
	return end_page - start_page;
}

static
int _get_pinned_pages(struct page **pages, char *data, int nr_pages)
{
	return get_user_pages_fast((unsigned long)data, nr_pages, 1, pages);
}

static
void _put_pinned_pages(struct page **pages, int nr_pages)
{
	int i;
	for (i = 0; i < nr_pages; ++i) {
		if (pages[i])
			put_page(pages[i]);
	}
}

static
void _unpin_user_mem(void *handle)
{
	struct _pin_hanlde *pin_handle = handle;

	/* sanity check */
	if (!pin_handle)
		return;

	/* put pinned pages */
	_put_pinned_pages(pin_handle->pages, pin_handle->nr_pages);

	/* free pin_handle */
	pcn_free(pin_handle);
}

static
void *_pin_user_mem(void *ubuf, size_t len)
{
	struct _pin_hanlde *pin_handle = NULL;
	int nr_pages, nr_pinned_pages;

	/* alloc pin_handle */
	nr_pages = _get_nr_pages(ubuf, len);
	pin_handle = pcn_calloc(1, sizeof(pin_handle) +
				(sizeof(struct page *) * nr_pages));
	if (!pin_handle)
		goto err_out;

	/* pin pages */
	pin_handle->nr_pages = nr_pages;
	nr_pinned_pages = _get_pinned_pages(pin_handle->pages,
					    ubuf,
					    pin_handle->nr_pages);
	if (nr_pinned_pages < 0)
		goto err_out;
	if (pin_handle->nr_pages != nr_pinned_pages)
		goto err_out;

	return pin_handle;
err_out:
	pcn_err("[%s:%d] ERROR \n", __func__, __LINE__);
	_unpin_user_mem(pin_handle);
	return NULL;
}

#else
static
void *_pin_user_mem(void *ubuf, size_t len)
{
	/* do nothing */
	return (void *)-1;
}

static
void _unpin_user_mem(void *handle)
{
	/* do nothing */
}
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

static
ssize_t _copy_recvdata_to_user(struct pcnlink_socket_t *plsock,
			       struct pcnlink_last_recv_t *lastrcv,
			       void *ubuf, size_t len)
{
	struct pcnlink_rx_netmsg_t *rx_netmsg = &lastrcv->rx_netmsg;
	struct pcnlink_rx_netmsg_t *oob_rx_netmsg = NULL;
	struct netmsg_e_recvdata_t *e_recvdata_in_rx_q;
	size_t copy_size = 0, left_size;
	void *data;
	int rc = 0;

	pcn_dbg2("channel = %p, netmsg = %p, seqnum = %x  cmd = %d\n",
		 rx_netmsg->channel,
		 rx_netmsg->netmsg,
		 rx_netmsg->netmsg_hdr.seqnum,
		 rx_netmsg->netmsg_hdr.command);

	/* is it valid? */
	if (!lastrcv->is_valid || len == 0) {
		return 0;
	}

	/* copy data from channel to user buffer */
	/* - calc. copy size */
	left_size = lastrcv->e_recvdata.datalen - lastrcv->data_start;
	copy_size = min(len, left_size);
	if (copy_size == 0) {
		goto out;
	}

	/* - check if the rx_netmsg is the oob_e_q */
retry_oob:
	if (!lastrcv->is_oob) {
		oob_rx_netmsg = find_netmsg_in_oob_e_q(
			plsock, &rx_netmsg->netmsg_hdr);
		if (oob_rx_netmsg) {
			/* - get e_recvdata message from the oob_rx_netmsg */
			*rx_netmsg = *oob_rx_netmsg;
			memcpy(&lastrcv->e_recvdata, rx_netmsg->netmsg,
			       sizeof(lastrcv->e_recvdata));
			lastrcv->is_oob = 1;
		}
	}

	/* - get start position in rx_q */
	e_recvdata_in_rx_q = (struct netmsg_e_recvdata_t *)rx_netmsg->netmsg;
	data = e_recvdata_in_rx_q->data  + lastrcv->data_start;

	/* - copy to user */
	if (!lastrcv->is_oob) {
		rc = copy_from_ring_buffer_scif(&rx_netmsg->channel->in_q,
						ubuf, data, copy_size);
		if (rc)
			goto err_out;
		/* - check if there is an oob element then retry */
		pcn_spin_lock(&plsock->channel->lock); {
			if ( peek_netmsg_in_oob_e_q(plsock,
						    &rx_netmsg->netmsg_hdr) ) {
				pcn_spin_unlock(&plsock->channel->lock);
				smp_mb();
				goto retry_oob;
			}
		} pcn_spin_unlock(&plsock->channel->lock);
	} else {
		memcpy(ubuf, data, copy_size);
		pcn_dbg("oob-copy: (seqnum, %x) (cmd, %d) (size, %u) (plsock, %p)\n",
			(unsigned int)e_recvdata_in_rx_q->hdr.seqnum,
			e_recvdata_in_rx_q->hdr.command,
			e_recvdata_in_rx_q->hdr.size,
			plsock);
	}
out:
	lastrcv->data_start += copy_size;

	/* is it completely consumed? */
	if (lastrcv->data_start == lastrcv->e_recvdata.datalen) {
		pcn_dbg2("(datalen, %d) (in_q, %p), (netmsg, %p)\n",
			 lastrcv->e_recvdata.datalen,
			 &rx_netmsg->channel->in_q, rx_netmsg->netmsg);

		/* make this chunk done */
		if (!lastrcv->is_oob) {
			pcn_spin_lock(&plsock->channel->lock); {
				if ( !peek_netmsg_in_oob_e_q(plsock,
							    &rx_netmsg->netmsg_hdr) ) {
					ring_buffer_scif_elm_set_done(
						&rx_netmsg->channel->in_q,
						rx_netmsg->netmsg);
				}
			} pcn_spin_unlock(&plsock->channel->lock);
		}
		else {
			size_t msize = sizeof(*oob_rx_netmsg) + e_recvdata_in_rx_q->hdr.size;
			oob_rx_netmsg = (void*)e_recvdata_in_rx_q - sizeof(*oob_rx_netmsg);
			_pcnlink_oob_free(plsock->channel, oob_rx_netmsg, msize);
		}
		/* invalidate lastrcv */
		lastrcv->is_valid = 0;
		lastrcv->is_oob = 0;
	}

	return copy_size;
err_out:
	pcn_err("rc = %d\n", rc);
	return rc;
}

ssize_t __pcnlink_recv(struct pcnlink_socket_t *plsock,
		       void *buf, size_t len)
{
	struct pcnlink_last_recv_t *lastrcv = &plsock->lastrcv;
	struct pcnlink_rx_netmsg_t *rx_netmsg = &lastrcv->rx_netmsg;
	struct netmsg_e_recvdata_t *e_recvdata = &lastrcv->e_recvdata;
	struct pcnlink_rx_netmsg_t *oob_rx_netmsg;
	struct ring_buffer_t *rx_q;
      	struct ring_buffer_req_t q_req;
	void *pin_handle = NULL;
	size_t total_copy = 0;
	ssize_t rc = 0;

	/* sanity check */
	if (!buf || len <= 0)
		goto out;

	/* pin user memory */
	pin_handle = _pin_user_mem(buf, len);
	if (!pin_handle)
		goto err_out;

	pcn_here();
retry:
	/* recv from the leftover of the last recv */
	if (lastrcv->is_valid) {
		rc = _copy_recvdata_to_user(plsock, lastrcv, buf, len);
		if (rc < 0)
			goto err_out;
		buf += rc;
		len -= rc;
		total_copy += rc;
	}
	if (!len)
		goto out;

	/* check if a socket is destructing */
	rx_q = get_plsock_rx_q(plsock, PCN_NETMSG_E_RECVDATA);
	if (rx_q == NULL) {
		rc = -ECONNRESET;
		pcn_dbg("drop out packets of destructing socket: %p\n",
			plsock);
		goto err_out;
	}

	/* fill buf from rx_q */
	ring_buffer_get_req_init(&q_req, BLOCKING);
	for (rc = ring_buffer_get_nolock(rx_q, &q_req);
	     rc == 0;
	     rc = ring_buffer_get_nolock(rx_q, &q_req)) {
		/* - get rx_netmsg */
		rc = copy_from_ring_buffer(rx_q, rx_netmsg,
					   q_req.data, q_req.size);
		if (rc)
			goto err_out;
		ring_buffer_elm_set_done(rx_q, q_req.data);

		/* - check if the rx_netmsg is the oob_e_q */
retry_oob:
		oob_rx_netmsg = find_netmsg_in_oob_e_q(plsock,
						       &rx_netmsg->netmsg_hdr);
		if (oob_rx_netmsg) {
			/* - get e_recvdata message from the oob_rx_netmsg */
			*rx_netmsg = *oob_rx_netmsg;
			memcpy(e_recvdata, rx_netmsg->netmsg,
			       sizeof(*e_recvdata));
			lastrcv->is_oob = 1;
		}
		else {
			/* - get e_recvdata message from the channel */
			rc = copy_from_ring_buffer_scif(
				&rx_netmsg->channel->in_q,
				e_recvdata,
				rx_netmsg->netmsg,
				sizeof(*e_recvdata));
			if (rc)
				goto err_out;
			/* - check if there is an oob element then retry */
			pcn_spin_lock(&plsock->channel->lock); {
				if ( peek_netmsg_in_oob_e_q(plsock,
							    &rx_netmsg->netmsg_hdr) ) {
					pcn_spin_unlock(&plsock->channel->lock);
					goto retry_oob;
				}
			} pcn_spin_unlock(&plsock->channel->lock);
			lastrcv->is_oob = 0;
		}

		/* - now, it is valid */
		lastrcv->data_start = 0;
		lastrcv->is_valid = 1;

		/* - copy recvdata to user buffer */
		rc = _copy_recvdata_to_user(plsock, lastrcv, buf, len);
		if (rc < 0)
			goto err_out;
		buf += rc;
		len -= rc;
		total_copy += rc;
		if (!len)
			goto out;

		/* prepare for next iteration */
		ring_buffer_get_req_init(&q_req, BLOCKING);
	}
	if (rc && rc != -EAGAIN)
		goto err_out;

	/* in the case of blocking mode, we have to get something. */
	if (!total_copy && !is_plsock_non_blocking(plsock)) {
		pcn_yield();
		goto retry;
	}
out:
	/* - unpin user memory */
	_unpin_user_mem(pin_handle);

	pcn_dbg("total_copy = %ld\n", total_copy);
	return total_copy;

err_out:
	/* - unpin user memory */
	_unpin_user_mem(pin_handle);
	pcn_err("rc = %ld\n", rc);
	return rc;
}

int __pcnlink_writable(struct pcnlink_socket_t *plsock)
{
	return !ring_buffer_scif_is_full(&plsock->channel->out_q);
}

int __pcnlink_readable(struct pcnlink_socket_t *plsock)
{
	return !ring_buffer_is_empty(plsock->rx_e_q);
}

unsigned int __pcnlink_poll(struct pcnlink_socket_t *plsock)
{
	unsigned int mask = plsock->poll_mask;

	pcn_here();
	switch(plsock->type) {
	case PCN_SOCK_TYPE_IN:
	case PCN_SOCK_TYPE_OUT:
		/* check if there are rooms in the outgoing queue
		 * of the associated channel */
		if (__pcnlink_writable(plsock))
			mask |= POLLOUT | POLLWRNORM | POLLWRBAND;

		/* NO BREAK intentionally */
	case PCN_SOCK_TYPE_LISTEN:
		/* check if there are pending events
		 * for accept() or recv() */
		if (__pcnlink_readable(plsock))
			mask |= POLLIN | POLLWRNORM;
		break;
	default:
	case PCN_SOCK_TYPE_EPOLL:
		mask |= POLLNVAL;
		break;
	}

	pcn_dbg("plsock = %p mask = 0x%x\n", plsock, mask);
	return mask;
}
