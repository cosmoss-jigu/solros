#include <linux/in.h>
#include <linux/module.h>
#include <linux/net.h>
#include <linux/ipv6.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/un.h>
#include <linux/uaccess.h>
#include <linux/inet.h>
#include <linux/idr.h>
#include <linux/file.h>
#include <linux/parser.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>

#include <net/9p/9p.h>
#include <net/9p/client.h>
#include <net/9p/transport.h>

#define CREATE_TRACE_POINTS
#include <trace/events/9p.h>

#include "protocol.h"
#include "trans_common.h"
#include "ring_buffer_scif.h"

#define P9_PRB_NODE_ID     0                   /* host */
#define P9_PRB_SERVER_PORT 564                 /* server port */
#define P9_PRB_CLIENT_PORT 3000                /* client port */
#define P9_PRB_MAXSIZE     P9_ZC_DEFAULT_MSIZE /* (64*1024) */
#define P9_PRB_SQ_DEPTH    64
#define P9_PRB_RQ_DEPTH    64

struct p9_trans_prb {
        struct scif_portID        addr;
	int                       sq_depth;
	int                       rq_depth;
	struct ring_buffer_scif_t sq; /* request  (out) */
	struct ring_buffer_scif_t rq; /* response (in)  */
	struct task_struct        *rq_reader;
};

struct p9_prb_opts {
	int srv_port;
	int cln_id;
	int cln_port;
	int sq_depth;
	int rq_depth;
};

struct p9_prb_conn_info {
	int mic_id;   /* mic id */
	int req_port; /* request  port */
	int res_port; /* response port */
} __attribute__((__packed__));

enum {
	Opt_srv_port,
	Opt_cln_id,
	Opt_cln_port,
	Opt_sq_depth,
	Opt_rq_depth,
	Opt_err,
};

static match_table_t tokens = {
	{Opt_srv_port, "port=%u"},
	{Opt_cln_id,   "client-id=%u"},
	{Opt_cln_port, "client-port=%u"},
	{Opt_sq_depth, "sq=%u"},
	{Opt_rq_depth, "rq=%u"},
	{Opt_err, NULL},
};

static int rq_reader_main(void *p);

static int
parse_opts(char *params, struct p9_prb_opts *opts)
{
	char *p;
	substring_t args[MAX_OPT_ARGS];
	int option;
	char *options, *tmp_options;

	opts->srv_port = P9_PRB_SERVER_PORT;
	opts->cln_id   = -1;
	opts->cln_port = P9_PRB_CLIENT_PORT;
	opts->sq_depth = P9_PRB_SQ_DEPTH;
	opts->rq_depth = P9_PRB_RQ_DEPTH;

	if (!params)
		return 0;
	p9_debug(P9_DEBUG_TRANS, "params = %s\n", params);

	tmp_options = kstrdup(params, GFP_KERNEL);
	if (!tmp_options) {
		p9_debug(P9_DEBUG_ERROR,
			 "failed to allocate copy of option string\n");
		return -ENOMEM;
	}
	options = tmp_options;

	while ((p = strsep(&options, ",")) != NULL) {
		int token;
		int r;
		if (!*p)
			continue;
		token = match_token(p, tokens, args);

		switch (token) {
		case Opt_srv_port:
		case Opt_cln_id:
		case Opt_cln_port:
		case Opt_sq_depth:
		case Opt_rq_depth:
			r = match_int(&args[0], &option);
			if (r < 0) {
				p9_debug(P9_DEBUG_ERROR,
					 "integer field, but no integer?\n");
				continue;
			}
			break;
		default:
			continue;
		}

		switch (token) {
		case Opt_srv_port:
			opts->srv_port = option;
			break;
		case Opt_cln_id:
			opts->cln_id   = option;
			break;
		case Opt_cln_port:
			opts->cln_port = option;
			break;
		case Opt_sq_depth:
			opts->sq_depth = option;
			break;
		case Opt_rq_depth:
			opts->rq_depth = option;
			break;
		default:
			continue;
		}
	}
	/* RQ must be at least as large as the SQ */
	opts->rq_depth = max(opts->rq_depth, opts->sq_depth);
	kfree(tmp_options);
	return 0;
}

static struct p9_trans_prb *
alloc_prb(struct p9_prb_opts *opts)
{
        struct p9_trans_prb *prb;

        prb = kzalloc(sizeof(*prb), GFP_KERNEL);
        if (!prb)
                return NULL;

        prb->addr.node = P9_PRB_NODE_ID;
        prb->addr.port = opts->srv_port;
        prb->sq_depth  = opts->sq_depth;
        prb->rq_depth  = opts->rq_depth;
        return prb;
}


static void
prb_destroy_trans(struct p9_trans_prb *prb)
{
	p9_debug(P9_DEBUG_TRANS, ":%d prb = %p\n",
		 __LINE__, prb);

	if (!prb)
		return;

	/* stop the rq_reader thread */
	if (prb->rq_reader)
		prb->rq_reader = NULL;

	/* destroy ring buffers */
	ring_buffer_scif_destroy_master(&prb->sq);
	ring_buffer_scif_destroy_master(&prb->rq);

	/* free prb */
	kfree(prb);
}

static int
prb_create_trans(struct p9_client *client, const char *addr, char *args)
{
	struct p9_prb_opts opts;
	struct p9_trans_prb *prb;
	scif_epd_t epd = NULL;
	struct p9_prb_conn_info conn_info;
	int port, rc;
	size_t q_size;
	int token = 0xCAFEBEBE;

	p9_debug(P9_DEBUG_TRANS, ":%d client = %p addr: %s\n",
		 __LINE__, client, addr);

	/* parse options */
	rc = parse_opts(args, &opts);
	if (rc < 0)
		return rc;

	/* alloc prb */
	prb = alloc_prb(&opts);
	if (!prb)
		return -ENOMEM;
	prb->addr.node = simple_strtol(addr, NULL, 10);
	prb->addr.port = opts.srv_port;

	/* connect to the server */
	epd = scif_open();
	if (epd == NULL)
		goto err_out;

	port = opts.cln_port;
	rc = scif_bind(epd, port);
	if (rc < 0)
		goto err_out;
	port++;

	p9_debug(P9_DEBUG_TRANS,
		 "srv_addr: %d   srv_port: %d   cln_port: %d\n",
		 prb->addr.node, prb->addr.port, opts.cln_port);
	rc = scif_connect(epd, &prb->addr);
	if (rc < 0)
		goto err_out;

	/* create a send queue (out) */
	q_size = prb->sq_depth * P9_PRB_MAXSIZE;
        rc = ring_buffer_scif_create_master(q_size,
                                            L1_CACHE_BYTES,
                                            RING_BUFFER_BLOCKING,
                                            RING_BUFFER_SCIF_PRODUCER,
                                            NULL, NULL,
                                            &prb->sq);
	if (rc)
		goto err_out;

	/* create a receive queue (in) */
	q_size = prb->rq_depth * P9_PRB_MAXSIZE;
        rc = ring_buffer_scif_create_master(q_size,
                                            L1_CACHE_BYTES,
                                            RING_BUFFER_BLOCKING,
                                            RING_BUFFER_SCIF_CONSUMER,
                                            NULL, NULL,
                                            &prb->rq);
	if (rc)
		goto err_out;

	/* send the master connection information to the server */
	conn_info.mic_id   = opts.cln_id;
	conn_info.req_port = port;
	conn_info.res_port = port + 10;
	rc = scif_send(epd,
		       &conn_info, sizeof(conn_info),
		       SCIF_SEND_BLOCK);
	if (rc < 0)
		goto err_out;
	p9_debug(P9_DEBUG_TRANS,
		 "mic_id: %d   req_port: %d   res_port: %d\n",
		 conn_info.mic_id, conn_info.req_port, conn_info.res_port);

	/* wait until the server to connect a send queue (out) */
	rc = scif_send(epd, &token, sizeof(token), 0);
	if (rc < 0)
		goto err_out;
	rc = ring_buffer_scif_wait_for_shadow(&prb->sq,
					      conn_info.req_port,
					      1);
	if (rc)
		goto err_out;

	/* wait until the server to connect a receive queue (in) */
	rc = scif_send(epd, &token, sizeof(token), 0);
	if (rc < 0)
		goto err_out;
	rc = ring_buffer_scif_wait_for_shadow(&prb->rq,
					      conn_info.res_port,
					      1);
	if (rc)
		goto err_out;

	/* update client info. */
	client->trans  = prb;
	client->status = Connected;

	/* recv ack */
	rc = scif_recv(epd, &token, sizeof(token), SCIF_RECV_BLOCK);
	if (rc < 0)
		goto err_out;
	scif_close(epd);
	epd = NULL;

	/* create a rq_reader thread */
	smp_mb();
	prb->rq_reader = kthread_run(rq_reader_main,
				     client,
				     "9p-prb-rq-reader");
	if (IS_ERR(prb->rq_reader))
		goto err_out;
	return 0;
err_out:
	p9_debug(P9_DEBUG_ERROR,
		 ":%d client = %p addr: %s prb = %p rc = %d\n",
		 __LINE__, client, addr, prb, rc);
	if (epd) {
		scif_close(epd);
		epd = NULL;
	}
	prb_destroy_trans(prb);
	return -ENOTCONN;
}

static void
prb_close(struct p9_client *client)
{
        struct p9_trans_prb *prb;

	p9_debug(P9_DEBUG_TRANS, ":%d client = %p\n",
		 __LINE__, client);

        if (!client)
                return;

        prb = client->trans;
        if (!prb)
                return;

        client->status = Disconnected;
        prb_destroy_trans(prb);
}

static int
prb_cancel(struct p9_client *client, struct p9_req_t *req)
{
	p9_debug(P9_DEBUG_TRANS, ":%d client = %p p9_req = %p\n",
		 __LINE__, client, req);
        return 1;
}

static int
prb_process_response(struct p9_client *client)
{
        struct p9_trans_prb *prb = client->trans;
	struct ring_buffer_req_t rq_req;
	struct p9_req_t *p9_req = NULL;
	uint16_t tag;
	int rc;

	/* get an element from rq */
	ring_buffer_get_req_init(&rq_req, BLOCKING);
	rc = ring_buffer_scif_get_nolock(&prb->rq, &rq_req);
	if (rc) {
		p9_debug(P9_DEBUG_ERROR,
			 "Fail to get response from rq: rc = %d\n",
			 rc);
		goto err_out;
	}

	/* get a tag from the element */
	tag = le16_to_cpu(*(__le16 *) ((char *)rq_req.data + 5));

	/* get a corresponding p9_req of the tag */
	p9_req = p9_tag_lookup(client, tag);
	if (!p9_req || (p9_req->status != REQ_STATUS_SENT)) {
		p9_debug(P9_DEBUG_ERROR, "Unexpected packet tag %d\n",
			 tag);
		rc = -EIO;
		goto err_out;
	}
	if (p9_req->rc == NULL) {
		p9_debug(P9_DEBUG_ERROR,
			 "No recv fcall for tag %d (req %p), disconnecting!\n",
			 tag, p9_req);
		rc = -EIO;
		goto err_out;
	}
	p9_req->rc->sdata = (char *)p9_req->rc + sizeof(struct p9_fcall);

	/*tracepoint*/
	trace_prb_process_response(client, p9_req);

	/* copy data of the element to the p9_req */
	rc = copy_from_ring_buffer_scif(&prb->rq,
					p9_req->rc->sdata,
					rq_req.data,
					rq_req.size);
	if (rc)
		goto err_out;
	p9_req->rc->capacity = rq_req.size;

	/* wrap up: set_done, STATUS_RCVD */
	ring_buffer_scif_elm_set_done(&prb->rq, rq_req.data);
	p9_req->status = REQ_STATUS_RCVD;

	/* run a callback function */
	p9_client_cb(client, p9_req);
	return 0;

err_out:
        p9_debug(P9_DEBUG_ERROR,
		 "IO error: disconnect: p9_req %p rc %d\n",
		 p9_req, rc);
        client->status = Disconnected;
	return rc;
}

static int
rq_reader_main(void *p)
{
	struct p9_client *client = p;
	int rc;

        p9_debug(P9_DEBUG_TRANS,
		 "rq_reader thread is started \n");

	/* keep processing as long as there is no error */
	smp_mb();
	do {
		/* process a response */
		rc = prb_process_response(client);
		if (rc)
			break;
	} while (client->status == Connected && !kthread_should_stop());

	if (!kthread_should_stop())
		complete_and_exit(NULL, rc);

        p9_debug(P9_DEBUG_TRANS,
		 "rq_reader thread is terminated with (rc, %d)\n",
		 rc);
	return rc;
}

static int
prb_request(struct p9_client *client, struct p9_req_t *p9_req)
{
        struct p9_trans_prb *prb = client->trans;
	struct ring_buffer_req_t sq_req;
	int rc;

        p9_debug(P9_DEBUG_TRANS, ":%d task %p p9_req: %p tcall %p id %d\n",
                 __LINE__, current, p9_req, p9_req->tc, p9_req->tc->id);
	trace_prb_request(client, p9_req);

	/* initial status is UNSENT */
        p9_req->status = REQ_STATUS_UNSENT;

	/* put an element to sg */
	ring_buffer_put_req_init(&sq_req, BLOCKING, p9_req->tc->size);
	rc = ring_buffer_scif_put(&prb->sq, &sq_req);
	if (rc)
		goto err_out;

	/* copy data to the element */
	rc = copy_to_ring_buffer_scif(&prb->sq,
				      sq_req.data,
				      p9_req->tc->sdata,
				      p9_req->tc->size);
	if (rc)
		goto err_out;

	/* final successful status is SENT */
	ring_buffer_scif_elm_set_ready(&prb->sq, sq_req.data);
        p9_req->status = REQ_STATUS_SENT;

	return 0;
err_out:
        p9_debug(P9_DEBUG_ERROR,
		 ":%d IO error: disconnect: task %p p9_req: %p tcall %p id %d\n",
                 __LINE__, current, p9_req, p9_req->tc, p9_req->tc->id);
        client->status = Disconnected;
	return rc;
}

static unsigned int
rest_of_page(void *data)
{
        return PAGE_SIZE - ((unsigned long)data % PAGE_SIZE);
}

static int
prb_get_pinned_pages(struct page **pages, char *data,
		     int nr_pages, int write, int kern_buf)
{
	int err;

	p9_debug(P9_DEBUG_TRANS,
		 ":%d data = %p nr_pages = %d write = %d kern_buf = %d\n",
		 __LINE__, data, nr_pages, write, kern_buf);

	if (!kern_buf) {
		err = p9_payload_gup(data, &nr_pages, pages, write);
		if (err < 0)
			return err;
	} else {
		/* kernel buffer, no need to pin pages */
		int s, index = 0;
		int count = nr_pages;
		while (nr_pages) {
			s = rest_of_page(data);
			pages[index++] = kmap_to_page(data);
			data += s;
			nr_pages--;
		}
		nr_pages = count;
	}
	return nr_pages;
}

static int
prb_zc_write_io(struct p9_client *client, struct p9_req_t *req,
		__u64 rg_addr, __u32 rg_len)
{
	int rc = p9pdu_writef(req->tc, client->proto_version,
			      "qd", rg_addr, rg_len);
	p9_debug(P9_DEBUG_TRANS,
		 ":%d rg_addr: 0x%llx  len: %d  rc: %d\n",
		 __LINE__, rg_addr, rg_len, rc);
	return rc;
}


static int
prb_zc_write_iov(struct p9_client *client, struct p9_req_t *req,
		 struct page **pages, int nr_pages,
		 char *udata, int ulen)
{
	int i, err;
	__u64 prev_pg_addr, pg_addr, rg_addr;
	__u32 pg_len, rg_len;

	p9_debug(P9_DEBUG_TRANS,
		 ":%d  client = %p  req = %p\n",
		 __LINE__, client, req);

	/* init */
	pg_addr      = (__u64)page_to_phys(pages[0]);
	prev_pg_addr = pg_addr - PAGE_SIZE;
	rg_addr      = pg_addr + offset_in_page(udata);
	rg_len       = 0;

	/* scan each page to find physically contiguous regions */
	for (i = 0; i < nr_pages; ++i) {
		/* get physcical addresses */
		pg_addr = (__u64)page_to_phys(pages[i]);

		/* emmit iov for a physically non-contiguous region */
		if (pg_addr != (prev_pg_addr + PAGE_SIZE)) {
			err = prb_zc_write_io(client, req, rg_addr, rg_len);
			if (unlikely(err))
				goto err_out;

			/* reset region info. */
			rg_len = 0;
			rg_addr = (__u64)page_to_phys(pages[i]);
		}

		/* expand this region */
		pg_len = rest_of_page(udata);
		if (pg_len > ulen)
			pg_len = ulen;

		rg_len += pg_len;
		udata  += pg_len;
		ulen   -= pg_len;
		prev_pg_addr = pg_addr;
	}
	BUG_ON(ulen != 0);

	/* emit the last region */
	err = prb_zc_write_io(client, req, rg_addr, rg_len);
	if (unlikely(err))
		goto err_out;

	/* update pdu size at the beginning */
	p9pdu_finalize(client, req->tc);
	return 0;
  err_out:
	p9_debug(P9_DEBUG_ERROR,
		 ":%d IO error: disconnect: client = %p  req = %p  err = %d\n",
		 __LINE__, client, req, err);
        client->status = Disconnected;
	return err;
}

static int
prb_zc_rsize(struct p9_client *client, u32 count)
{
        const static int iov_size = sizeof(__u64) + sizeof(__u32);
        unsigned int n_iov, zc_size;

        /* simply assume the worst case such that every page is fragmented */
	/* XXX: it assumes client->msize is the P9_ZC_HDR_SZ. */
        n_iov = (min(client->msize, P9_ZC_HDR_SZ) - P9_IOHDRSZ) / iov_size;
        zc_size = n_iov * PAGE_SIZE;

        /* zc_size is the conservative maximum transferable size */
        return min(zc_size, count);
}

static int
prb_zc_request(struct p9_client *client, struct p9_req_t *req,
	       char *uidata, char *uodata, int inlen,
	       int outlen, int in_hdr_len, int kern_buf)
{
	char *udata;
	int write, ulen, nr_pages, nr_pinned_pages;
	struct page **pages = NULL;
	int err;

	p9_debug(P9_DEBUG_TRANS,
		 ":%d client = %p  req = %p\n",
		 __LINE__, client, req);

	/* check io direction */
	write = uodata != NULL;
	if (write) {
		udata = uodata;
		ulen  = outlen;
	}
	else {
		udata = uidata;
		ulen  = inlen;
	}
	p9_debug(P9_DEBUG_TRANS, "task %p tcall %p id %d udata %p ulen %d\n",
		 current, req->tc, req->tc->id, udata, ulen);
	BUG_ON(uidata != NULL && uodata != NULL);

	/* get pinned pages */
	nr_pages = p9_nr_pages(udata, ulen);
	pages = kmalloc(sizeof(struct page *) * nr_pages, GFP_NOFS);
	if (!pages) {
		err = -ENOMEM;
		goto err_out;

	}
	nr_pinned_pages = prb_get_pinned_pages(pages, udata, nr_pages,
					       !write, kern_buf);
	if (nr_pinned_pages < 0) {
		err = nr_pinned_pages;
		goto free_out;
	}
	if (nr_pinned_pages != nr_pages) {
		nr_pages = nr_pinned_pages;
		err = -ENOMEM;
		goto unpin_out;
	}
	p9_debug(P9_DEBUG_TRANS,
		 ":%d %d pages are pinned (kern_buf: %d)\n",
		 __LINE__, nr_pages, kern_buf);

	/* generate io vector */
	err = prb_zc_write_iov(client, req, pages, nr_pages, udata, ulen);
	if (unlikely(err))
		goto unpin_out;

	/* send a request */
	err = prb_request(client, req);

unpin_out:
	/*
	 * Non kernel buffers are pinned, unpin them
	 */
	if (!kern_buf) {
		p9_release_pages(pages, nr_pages);
		p9_debug(P9_DEBUG_TRANS,
			 ":%d %d pages are released (err: %d)\n",
			 __LINE__, nr_pages, err);
	}
free_out:
	kfree(pages);
err_out:
	if (err) {
		p9_debug(P9_DEBUG_ERROR,
			 ":%d io error: disconnect: client = %p  req = %p err = %d\n",
			 __LINE__, client, req, err);
		client->status = Disconnected;
	}
	return err;
}

#ifdef CONFIG_9P_FS_BULK_PAGE_IO
static int
prb_zc_bulk_write_iov(struct p9_client *client, struct p9_req_t *req,
                        struct page **pages, u32 nr_page, int len)
{
	int i, err;
	__u64 prev_pg_addr, pg_addr, rg_addr;
	__u32 rg_len;

	pg_addr = (__u64)page_to_phys(pages[0]);
	prev_pg_addr = pg_addr - PAGE_SIZE;
	rg_addr = pg_addr;
	rg_len = 0;

	p9_debug(P9_DEBUG_TRANS,
		 "nr_page = %d len = %d\n", nr_page, len);

	for (i = 0; i < nr_page; ++i) {
		pg_addr = (__u64)page_to_phys(pages[i]);

		if (pg_addr != (prev_pg_addr + PAGE_SIZE)) {
			err = prb_zc_write_io(client, req, rg_addr, rg_len);
			if (unlikely(err))
				goto err_out;

			rg_len = 0;
			rg_addr = (__u64)page_to_phys(pages[i]);
		}

		rg_len += PAGE_SIZE;
		len -= PAGE_SIZE;
		prev_pg_addr = pg_addr;
	}
	BUG_ON(len != 0);

	err = prb_zc_write_io(client, req, rg_addr, rg_len);
	if (unlikely(err))
		goto err_out;

	p9pdu_finalize(client, req->tc);
        return 0;
err_out:
	p9_debug(P9_DEBUG_ERROR,
		 ":%d IO error: disconnect: client = %p  req = %p  err = %d\n",
		 __LINE__, client, req, err);
        client->status = Disconnected;
	return err;
}

static int
prb_zc_bulk_request(struct p9_client *client, struct p9_req_t *req,
                      struct page **pages, u32 pgcount,
                      int len, int in_hdr_len)
{
	int err;

	p9_debug(P9_DEBUG_TRANS,
		 ":%d  pgcount = %d len = %d in_hdr_len = %d\n",
		 __LINE__, pgcount, len, in_hdr_len);

	err = prb_zc_bulk_write_iov(client, req, pages, pgcount, len);
	if (unlikely(err))
		goto err_out;

	err = prb_request(client, req);
	if (err) {
		p9_debug(P9_DEBUG_ERROR,
		":%d io error: disconnect: client = %p  req = %p err = %d\n",
		__LINE__, client, req, err);
		client->status = Disconnected;
	}
err_out:
	return err;
}
#endif



static struct p9_trans_module p9_prb_trans = {
	.name = "prb",
	.maxsize = P9_PRB_MAXSIZE,
	.def = 0,
	.owner = THIS_MODULE,
	.create = prb_create_trans,
	.close = prb_close,
	.request = prb_request,
	.cancel = prb_cancel,
#ifdef CONFIG_NET_9P_ZERO_COPY
	.zc_rsize = prb_zc_rsize,
	.zc_request = prb_zc_request,
# ifdef CONFIG_9P_FS_BULK_PAGE_IO
        .zc_bulk_request = prb_zc_bulk_request,
# endif
#endif
};

/**
 * p9_trans_prb_init - Register the 9P PRB transport driver
 */
static int __init p9_trans_prb_init(void)
{
	v9fs_register_trans(&p9_prb_trans);
	return 0;
}

static void __exit p9_trans_prb_exit(void)
{
	v9fs_unregister_trans(&p9_prb_trans);
}

module_init(p9_trans_prb_init);
module_exit(p9_trans_prb_exit);

MODULE_AUTHOR("Changwoo Min <changwoo@gatech.edu>");
MODULE_DESCRIPTION("PCI Ring Buffer (PRB) Transport for 9P");
MODULE_LICENSE("GPL");
