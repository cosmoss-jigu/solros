#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <scif.h>
#include "9p.h"
#include "npfs.h"
#include "npfsimpl.h"
#include "ring_buffer_scif.h"

typedef struct Prbtrans Prbtrans;

struct Prbtrans {
    struct Nptrans*           trans;
    struct ring_buffer_scif_t rbs_in;
    struct ring_buffer_scif_t rbs_out;
};

static void prb_trans_destroy(void *a);
static int  prb_trans_recv(Npfcall **fcp, u32 msize, void *a);
static int  prb_trans_send(Npfcall *fc, void *a);

/**
 * \brief Create a PCI ring buffer (prb) transport server
 *
 * \param epd        client epd
 * \param mic_id     mic id
 * \param req_port   port number of request buffer
 * \param res_port   port number of response buffer
 * \param local_port local port number
 * \returns A pointer to the newly allocated transport
 */
Nptrans *
np_prbtrans_create(scif_epd_t epd, int mic_id,
		   int req_port, int res_port, int *local_port)
{
    enum {PORT_SPACING_ONE_WAY = 50};
    Nptrans *trans;
    Prbtrans *prb;
    int rc, token;

    /* alloc prbtrans */
    prb = calloc(1, sizeof(*prb));
    if (!prb)
	goto err_out;

    /* connect to the master request ring buffer */
    rc = scif_recv(epd, &token, sizeof(token), SCIF_RECV_BLOCK);
    if (rc < 0)
	goto err_out;
    rc = ring_buffer_scif_create_shadow(*local_port,
					mic_id,
					req_port,
					NULL,
					NULL,
					&prb->rbs_in);
    if (rc) {
	fprintf(stderr,
		"[%s:%d] ring_buffer_scif_create_shadow: "
		"local_port = %d, mic_id = %d, req_port = %d, rc = %d\n",
		__func__, __LINE__,
		*local_port, mic_id, req_port, rc);
	goto err_out;
    }
    *local_port += PORT_SPACING_ONE_WAY;

    /* connect to the master response ring buffer */
    rc = scif_recv(epd, &token, sizeof(token), SCIF_RECV_BLOCK);
    if (rc < 0)
	goto err_out;
    rc = ring_buffer_scif_create_shadow(*local_port,
					mic_id,
					res_port,
					NULL,
					NULL,
					&prb->rbs_out);
    if (rc) {
	fprintf(stderr,
		"[%s:%d] ring_buffer_scif_create_shadow: "
		"local_port = %d, mic_id = %d, req_port = %d, rc = %d\n",
		__func__, __LINE__,
		*local_port, mic_id, req_port, rc);

	goto err_out;
    }
    *local_port += PORT_SPACING_ONE_WAY;

    /* create an nptrans */
    trans = np_trans_create(prb, prb_trans_recv,
			    prb_trans_send,
			    prb_trans_destroy);
    if (!trans)
	goto err_out;
    prb->trans = trans;

    /* send ack */
    rc = scif_send(epd, &token, sizeof(token), SCIF_SEND_BLOCK);
    if (rc < 0)
	goto err_out;
    return trans;
 err_out:
    fprintf(stderr, "[%s:%d] error\n", __func__, __LINE__);
    if (prb)
	prb_trans_destroy(prb);
    return NULL;
}

static void
prb_trans_destroy(void *a)
{
    Prbtrans *prb = (Prbtrans *)a;

    ring_buffer_scif_destroy_shadow(&prb->rbs_in);
    ring_buffer_scif_destroy_shadow(&prb->rbs_out);

    free(prb);
}

static int
prb_trans_recv(Npfcall **fcp, u32 msize, void *a)
{
    Prbtrans *prb = (Prbtrans *)a;
    struct ring_buffer_req_t req = {.data = NULL};
    Npfcall *fc = NULL;
    u32 alloc_size;
    int rc;

    /* get an element from rbs_in */
    ring_buffer_get_req_init(&req, BLOCKING);
    rc = ring_buffer_scif_get(&prb->rbs_in, &req);
    if (rc) {
	np_uerror(-rc);
	goto err_out;
    }

    /* alloc fcall */
    alloc_size = msize > req.size ? msize : req.size;
    if (!(fc = np_alloc_fcall(alloc_size))) {
	np_uerror(ENOMEM);
	goto err_out;
    }

    /* copy data to fcall */
    rc = copy_from_ring_buffer_scif(&prb->rbs_in,
				    fc->pkt,
				    req.data,
				    req.size);
    if (rc) {
	np_uerror(-rc);
	goto err_out;
    }
    ring_buffer_scif_elm_set_done(&prb->rbs_in, req.data);

    /* ok, pass out the fc */
    *fcp = fc;
    return 0;

 err_out:
    fprintf(stderr, "[%s:%d] error\n", __func__, __LINE__);
    if (req.data)
	ring_buffer_scif_elm_set_done(&prb->rbs_in, req.data);
    if (fc)
	free(fc);
    return -1;
}

static int
prb_trans_send(Npfcall *fc, void *a)
{
    Prbtrans *prb = (Prbtrans *)a;
    struct ring_buffer_req_t req = {.data = NULL};
    int rc;

    /* put an element to rbs_out */
    ring_buffer_put_req_init(&req, BLOCKING, fc->size);
    rc = ring_buffer_scif_put(&prb->rbs_out, &req);
    if (rc) {
	np_uerror(-rc);
	goto err_out;
    }

    /* copy from fcall to ring buffer */
    rc = copy_to_ring_buffer_scif(&prb->rbs_out, req.data,
				  fc->pkt, fc->size);
    if (rc) {
	np_uerror(-rc);
	goto err_out;
    }

    /* set ready */
    ring_buffer_scif_elm_set_ready(&prb->rbs_out, req.data);
    return fc->size;

 err_out:
    fprintf(stderr, "[%s:%d] error\n", __func__, __LINE__);
    return -1;
}
