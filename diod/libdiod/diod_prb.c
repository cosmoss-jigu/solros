#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>
#include <syslog.h>
#include <sys/time.h>
#include <poll.h>
#include <pthread.h>
#include <scif.h>

#include "9p.h"
#include "npfs.h"
#include "list.h"

#include "diod_log.h"
#include "diod_prb.h"

#define PRB_DEFAULT_PORT      564
#define PRB_BACKLOG           64

static int g_prb_port = PRB_DEFAULT_PORT;
static int g_local_port = PRB_DEFAULT_PORT + 100;

struct diod_prb_struct {
    scif_epd_t server_epd;
};

struct prb_conn_info {
    int mic_id;   /* mic id */
    int req_port; /* request  port */
    int res_port; /* response port */
} __attribute__((__packed__));

diod_prb_t
diod_prb_create (void)
{
    diod_prb_t prb;

    /* alloc instance */
    prb = malloc (sizeof (*prb));
    if (!prb)
        msg_exit ("out of memory");
    prb->server_epd = SCIF_OPEN_FAILED;

    /* create a server scif socket */
    prb->server_epd = scif_open();
    if (prb->server_epd == SCIF_OPEN_FAILED)
        msg_exit ("fail to create an SCIF server socket");

    return prb;
}

int
diod_prb_listen (List l, diod_prb_t prb)
{
    ListIterator itr;
    char *s, *host, *port;
    int n, port_num;

    /* get port number from option */
    if (!(itr = list_iterator_create(l)))
        msg_exit ("out of memory");
    s = list_next(itr);
    if (s) {
	if (!(host = strdup (s)))
	    msg_exit ("out of memory");
	port = strchr (host, ':');
	NP_ASSERT (port != NULL);
	*port++ = '\0';
	port_num = strtol(port, NULL, 10);
	if (errno == ERANGE || errno == EINVAL)
	    msg_exit ("invalid port number");
	g_prb_port = port_num;
    }
    list_iterator_destroy(itr);

    /* bind */
    n = scif_bind(prb->server_epd, g_prb_port);
    if (n < 0)
       errn_exit (n, "scif_bind: listening port:%d conflict???", g_prb_port);

    /* listen */
    n = scif_listen(prb->server_epd, PRB_BACKLOG);
    if (n < 0)
	errn_exit (n, "scif_listen");
    g_local_port = g_prb_port + 100;
    msg ("scif_listen: %d port", g_prb_port);

    return 0;
}

void
diod_prb_accept_one (Npsrv *srv, diod_prb_t prb)
{
    Nptrans *trans;
    Npconn *conn;
    scif_epd_t client_epd = SCIF_OPEN_FAILED;
    struct scif_portID peer;
    struct prb_conn_info conn_info;
    int n;

    /* accept a connection */
    n = scif_accept(prb->server_epd, &peer, &client_epd,
		    SCIF_ACCEPT_SYNC);
    if (n < 0)
	errn_exit(n, "scif_accept");

    /* get a prb connection from client */
    n = scif_recv(client_epd, &conn_info, sizeof(conn_info), SCIF_RECV_BLOCK);
    if (n < 0)
	errn_exit(n, "scif_recv");

    /* create a prb connection */
    msg ("prb: connection request: mic_id: %d  req_port: %d  res_port: %d\n",
         conn_info.mic_id, conn_info.req_port, conn_info.res_port);
    trans = np_prbtrans_create(client_epd,
			       conn_info.mic_id, conn_info.req_port, conn_info.res_port,
			       &g_local_port);
    if (trans) {
	conn = np_conn_create(srv, trans, "prb", 0);
	np_srv_add_conn(srv, conn);
    } else
	errn (np_rerror (), "np_prbtrans_create failed");

    /* close a client connection to set up prb */
    if (client_epd != SCIF_OPEN_FAILED)
	scif_close(client_epd);
}

void
diod_prb_destroy (diod_prb_t prb)
{
    if (prb->server_epd != SCIF_OPEN_FAILED) {
	scif_close(prb->server_epd);
	prb->server_epd = SCIF_OPEN_FAILED;
    }
}

void
diod_prb_shutdown (diod_prb_t prb)
{
    diod_prb_destroy (prb);
    free(prb);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
