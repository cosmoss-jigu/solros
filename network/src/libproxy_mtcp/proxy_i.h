#ifndef _PROXY_I_H
#define _PROXY_I_H
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <arch.h>
#include <netmsg.h>
#include <proxy.h>
#include <mtcp_api.h>
#include <mtcp_epoll.h>

/**
 * socket API porting layer for BSD socket and mTCP socket
 */
#define _pcn_socket(__c,__d,__t,__p)               mtcp_socket(__c,__d,__t,__p)
#define _pcn_bind(__c,__s,__a,__l)                 mtcp_bind(__c,__s,__a,__l)
#define _pcn_connect(__c,__s,__a,__l)              mtcp_connect(__c,__s,__a,__l)
#define _pcn_listen(__c,__s,__b)                   mtcp_listen(__c,__s,__b)
#define _pcn_accept(__c,__s,__a,__l)               mtcp_accept(__c,__s,__a,__l)
#define _pcn_setsockopt(__c,__s,__l,__n,__v,__e)   mtcp_setsockopt(__c,__s,__l,__n,__v,__e)
#define _pcn_getsockopt(__c,__s,__l,__n,__v,__e)   mtcp_getsockopt(__c,__s,__l,__n,__v,__e)
// mtcp doesn't have sendmsg interface.
#define _pcn_sendmsg(__s,__m,__f)                  sendmsg(__s,__m,__f)
#define _pcn_write(__c,__s,__b,__l)                mtcp_write(__c,__s,__b,__l)
#define _pcn_read(__c,__s,__b,__l)	       	   mtcp_read(__c,__s,__b,__l)
#define _pcn_epoll_create(__c,__n)		   mtcp_epoll_create(__c,__n)
#define _pcn_epoll_wait(__c,__s,__e,__m,__t)       mtcp_epoll_wait(__c,__s,__e,__m,__t)
#define _pcn_epoll_ctl(__c,__s,__o,__f,__e)	   mtcp_epoll_ctl(__c,__s,__o,__f,__e)
// mtcp doesn't have shutdown interface.
#define _pcn_shutdown(__s,__f)                     shutdown(__s,__f)
#define _pcn_close(__c,__s)                        mtcp_close((__c)->mctx,__s)

/**
 * internal functions
 */
static int   _pcn_init_netctx(struct pcn_proxy_t *proxy, int netctx_id,
			      struct pcn_netctx_t *netctx);
static void  _pcn_deinit_netctx(struct pcn_netctx_t *netctx);
static void *_pcn_netctx_worker(void *_p);
static int   _pcn_netctx_process(struct pcn_netctx_t *netctx);
static int   _pcn_netctx_process_channels(struct pcn_netctx_t *netctx);
static int   _pcn_netctx_process_sockets(struct pcn_netctx_t *netctx);
static int   _pcn_channel_process_netmsg(struct pcn_netctx_t *netctx,
					 int remote_id,
					 struct ring_buffer_req_t *in_msg);
static int   _pcn_secure_scratchpad(struct pcn_netctx_t *netctx, int size);
static int   _pcn_channel_process_t_socket(struct pcn_netctx_t *netctx,
					   int remote_id,
					   struct netmsg_t_socket_t *t_socket);
static int   _pcn_channel_process_t_bind(struct pcn_netctx_t *netctx,
					 int remote_id,
					 struct netmsg_t_bind_t *t_bind);
static int   _pcn_channel_process_t_connect(struct pcn_netctx_t *netctx,
					    int remote_id,
					    struct netmsg_t_connect_t *t_connect);
static int   _pcn_channel_process_t_listen(struct pcn_netctx_t *netctx,
					   int remote_id,
					   struct netmsg_t_listen_t *t_listen);
static int   _pcn_channel_process_t_setsockopt(struct pcn_netctx_t *netctx,
					       int remote_id,
					       struct netmsg_t_setsockopt_t *t_setsockopt);
static int   _pcn_channel_process_t_getsockopt(struct pcn_netctx_t *netctx,
					       int remote_id,
					       struct netmsg_t_getsockopt_t *t_getsockopt);
static int   _pcn_channel_process_t_sendmsg(struct pcn_netctx_t *netctx,
					    int remote_id,
					    struct netmsg_t_sendmsg_t *t_sendmsg);
static int   _pcn_channel_process_t_senddata(struct pcn_netctx_t *netctx,
					    int remote_id,
					    struct netmsg_t_senddata_t *t_senddata);
static int   _pcn_channel_process_t_shutdown(struct pcn_netctx_t *netctx,
					     int remote_id,
					     struct netmsg_t_shutdown_t *t_shutdown);
static int   _pcn_channel_process_t_close(struct pcn_netctx_t *netctx,
					  int remote_id,
					  struct netmsg_t_close_t *t_close);

/**
 * Util Functions
 */
int proxy_core_affinitize(int cpu);
#endif /* _PROXY_I_H */
