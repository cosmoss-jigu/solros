#include <scif.h>
#include <link.h>
#include <pcnporting.h>

static int  _pcn_init_channel(struct pcn_link_t *link,
			      struct conn_endp_t *endp,
			      struct pcn_conn_info_t *conn_info,
			      int channel_id, size_t out_q_size,
			      ring_buffer_reap_cb_t in_q_cb,
			      struct pcn_channel_t *channel);
static void _pcn_deinit_channel(struct pcn_channel_t *channel);
static int  _pcn_new_socktbls(int n, struct pcn_socktbl_t **psocktbls);
static void _pcn_delete_socktbls(int n, struct pcn_socktbl_t *socktbls);

int pcn_init_link(struct pcn_conn_info_t *conn_info,
		  struct conn_endp_t *endp,
		  int num_channel, size_t out_q_size,
		  ring_buffer_reap_cb_t in_q_cb,
		  struct pcn_link_t *link)
{
	struct pcn_conn_info_t ch_cinfo = *conn_info;
	int rc = 0;
	int i;

	pcn_dbg("(remote_id, %d) (remote_port, %d) (local_port, %d)\n",
		conn_info->remote_id,
		conn_info->remote_port,
		conn_info->local_port);

	/* init. and alloc memories */
	memset(link, 0, sizeof(*link));
	link->remote_id   = conn_info->remote_id;
	link->num_channel = num_channel;
	link->channels    = pcn_malloc(link->num_channel *
				       sizeof(*link->channels));
	if (!link->channels) {
		rc = -ENOMEM;
		goto err_out;
	}

	/* init channels */
	for (i = 0; i < link->num_channel; ++i) {
		struct pcn_channel_t *channel = &link->channels[i];
		rc = _pcn_init_channel(link, endp, &ch_cinfo,
				       i, out_q_size,
				       in_q_cb, channel);
		if (rc)
			goto err_out;

		/* increase port numbers for next channel */
		ch_cinfo.remote_port += PCN_CHANNEL_NUM_PORTS;
		ch_cinfo.local_port  += PCN_CHANNEL_NUM_PORTS;

	}

	/* init per-channel socket tables if required */
	rc = _pcn_new_socktbls(link->num_channel,
			       &link->socktbls);
	if (rc)
		goto err_out;

	return 0;
err_out:
	pcn_deinit_link(link);
	return rc;
}

void pcn_deinit_link(struct pcn_link_t *link)
{
	/* sanity check */
	if (!link)
		return;

	/* deinit channels */
	if (link->channels) {
		int i;
		for (i = 0; i < link->num_channel; ++i) {
			struct pcn_channel_t *channel = &link->channels[i];
			_pcn_deinit_channel(channel);
		}
		pcn_free(link->channels);
	}

	/* delete socktbls */
	if (link->socktbls) {
		_pcn_delete_socktbls(link->num_channel, link->socktbls);
	}

	/* clear link */
	memset(link, 0, sizeof(*link));
}

static
int _pcn_init_channel(struct pcn_link_t *link,
		      struct conn_endp_t *endp,
		      struct pcn_conn_info_t *conn_info,
		      int channel_id, size_t out_q_size,
		      ring_buffer_reap_cb_t in_q_cb,
		      struct pcn_channel_t *channel)
{
	struct pcn_conn_info_t q_cinfo = *conn_info;
	struct ring_buffer_scif_t in_q;
	struct ring_buffer_scif_t out_q;
	int rc = 0, token = 0xCAFEBEBE;

	/* init */
	memset(channel, 0, sizeof(*channel));
	channel->link       = link;
	channel->channel_id = channel_id;
	channel->oob_size   = out_q_size;

	/* init spin lock */
	pcn_spin_init(&channel->lock);

	/* create an output ring buffer */
	rc = ring_buffer_scif_create_master(out_q_size,
					    L1D_CACHELINE_SIZE,
					    0, /* non-blocking queue */
                                            RING_BUFFER_SCIF_PRODUCER,
                                            NULL, NULL, /* no callbacks */
                                            &out_q);
	if (rc)
		goto err_out;
	channel->out_q = out_q;

	/* non-blocking waiting for shadow connection */
	rc = ring_buffer_scif_wait_for_shadow(&channel->out_q,
					      q_cinfo.local_port,
					      0);
	if (rc)
		goto err_out;
	q_cinfo.local_port  += PCN_QUEUE_NUM_PORTS;

	/* barrier before connecting to the corresponding remote shadow */
	rc = scif_send(endp->epd, &token, sizeof(token), SCIF_SEND_BLOCK);
	rc = __conv_scif_ret(rc);
	if (rc < 0)
		goto err_out;
	rc = scif_recv(endp->epd, &token, sizeof(token), SCIF_RECV_BLOCK);
	rc = __conv_scif_ret(rc);
	if (rc < 0)
		goto err_out;

	/* connect to a remote input ring buffer */
	rc = ring_buffer_scif_create_shadow(q_cinfo.local_port,
					    q_cinfo.remote_id,
					    q_cinfo.remote_port,
					    /* register a callback for slow consumers */
					    in_q_cb, channel,
					    &in_q);
	channel->in_q = in_q;
	if (rc)
		goto err_out;
	q_cinfo.remote_port += PCN_QUEUE_NUM_PORTS;
	smp_mb();
	return 0;
err_out:
	_pcn_deinit_channel(channel);
	return rc;
}

static
void _pcn_deinit_channel(struct pcn_channel_t *channel)
{
	/* sanity check */
	if (!channel)
		return;

	/* destroy the input ring buffer */
	ring_buffer_scif_destroy_shadow(&channel->in_q);

	/* destroy the output ring buffer */
	ring_buffer_scif_destroy_master(&channel->out_q);

	/* deinit spin lock */
	pcn_spin_deinit(&channel->lock);

	/* clear the channel */
	memset(channel, 0, sizeof(*channel));
}

static
int _pcn_new_socktbls(int n, struct pcn_socktbl_t **psocktbls)
{
	struct pcn_socktbl_t *socktbls = NULL;
	int i, j;
	int rc = 0;

	/* alloc socktbls */
	socktbls = pcn_calloc(n, sizeof(*socktbls));
	if (!socktbls) {
		rc = -ENOMEM;
		goto err_out;
	}

	/* init list head and spin lock */
	for (i = 0; i < n; ++i) {
		struct pcn_socktbl_t *st = &socktbls[i];
		for (j = 0; j < PCN_SOCK_TYPE_MAX; ++j) {
			struct list_head *sl = &st->types[j];
			INIT_LIST_HEAD(sl);
		}
		pcn_spin_init(&st->lock);
	}

	/* pass out socktbls */
	*psocktbls = socktbls;
	return 0;
err_out:
	if (socktbls)
		_pcn_delete_socktbls(n, socktbls);
	return rc;
}

static
void _pcn_delete_socktbls(int n, struct pcn_socktbl_t *socktbls)
{
	int i;

	/* !!! WARNING !!!
	 * It does not perform deep free.
	 * Sockets in the tables must be freed in advance. */

	/* sanity check */
	if (!socktbls)
		return;

	/* deinit spin lock */
	for (i = 0; i < n; ++i) {
		struct pcn_socktbl_t *st = &socktbls[i];
		pcn_spin_deinit(&st->lock);
	}

	/* free array of socktbls */
	pcn_free(socktbls);
}

void pcn_socktbl_add_lock(struct pcn_socktbl_t *st, struct list_head *sk_list, int type)
{
	smp_wmb();
	pcn_spin_lock(&st->lock); {
		list_add(sk_list, &st->types[type]);
	} pcn_spin_unlock(&st->lock);
}

void pcn_socktbl_del_lock(struct pcn_socktbl_t *st, struct list_head *sk_list)
{
	pcn_spin_lock(&st->lock); {
		list_del(sk_list);
	} pcn_spin_unlock(&st->lock);
}

void pcn_socktbl_move_lock(struct pcn_socktbl_t *st, struct list_head *sk_list, int type)
{
	smp_wmb();
	pcn_spin_lock(&st->lock); {
		list_move(sk_list, &st->types[type]);
	} pcn_spin_unlock(&st->lock);
}
