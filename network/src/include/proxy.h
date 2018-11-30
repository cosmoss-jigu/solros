#ifndef _PROXY_H
#define _PROXY_H
#include <pthread.h>
#include <list.h>
#include <link.h>
#include <limits.h>
#include <netinet/in.h>
#include <mtcp_api.h>
#include <mtcp_epoll.h>

struct pcn_netctx_t;
struct pcn_socktbl_t;
struct pcn_socket_t;

/**
 * network proxy running on a host and serving for multiple Xeon Phis
 */
#define PCN_PROXY_MAX_LINKS     9           /* max installable Phis + 1 */

#define MAX_FLOW  (10000)                   /* max flow number per netctx */
#define PCN_MAX_EVENTS (MAX_FLOW * 3) 	    /* max epoll events per netctx */
#define MAX_SENDBUF_SIZE (256 * 1024 * 1024) /* Max snd buffer size per socket */
#define SOCKID_MAX 4096                     /* XXX: please check */
#define MAX_BLACK_DUCK_CNT 3                /* XXX: maybe bug? */

struct pcn_proxy_t {
        struct conn_endp_t server;          /* listening port
                                             * to establish links */

	int num_netctx;                     /* number of network contexts */
	struct pcn_netctx_t *netctxs;       /* array of network contexts
					     * indexed by channel_id */

	int base_link_port;                 /* starting port for links */
	size_t rx_q_size;                   /* RX queue size of a link */
	int num_link;                       /* established number of links */
	struct pcn_link_t links[            /* array of links */
		PCN_PROXY_MAX_LINKS];       /* indexed by remote_id */
};

struct pcn_sockfd_map_t {
	int remote_id;
	struct pcn_socket_t *sock;
};

/**
 * network context, which is per-core and maintained by a proxy
 */
#define PCN_NETCTX_SCRATCHPAD_INIT_SIZE  (4 * 1024 * 1024)
#define MAX_HOST_BUFFER			 (4 * 1024)
#define MAX_READ_LEN			 (32 * 1024) /* let's make it large enough for DMA */

struct pcn_netctx_t {
	struct pcn_proxy_t *proxy;          /* owner proxy */
	int netctx_id;                      /* network context id */
	int status;                         /* current status */
	void *scratchpad;                   /* scratchpad memory */
	int scratchpad_size;                /* size of scratchpad size */
	int num_link;                       /* established number of links */
	struct pcn_channel_t *pchannels[    /* array of channels */
		PCN_PROXY_MAX_LINKS];       /* indexed by remote_id */
	struct pcn_socktbl_t *psocktbls[    /* array of socktbls */
		PCN_PROXY_MAX_LINKS];       /* indexed by remote_id */
	pthread_t worker;                   /* worker thread */
	int core;                           /* Core netctx runs on */
	/* XXX: mtcp_context, etc */
	mctx_t	 mctx;                      /* mtcp context       */
	int num_listen;
	int epoll_fd;			    /* the epoll fd for all sockets */
	struct epoll_event *events; 	    /* epoll events */
	struct mtcp_epoll_event *mevents;   /* mtcp epoll events */
	struct pcn_sockfd_map_t
		sockid_remid[SOCKID_MAX];   /* IN socket - remote-id mapping */
	int listen_recv[                    /* used for load balancing */
            PCN_PROXY_MAX_LINKS];           /* list of remote-ids */
	int zombie_fd;                      /* zombie fd */
};

/**
 * netctx status
 */
enum {
	PCN_NETCTX_STATUS_UNINIT = 0,       /* uninitialized */
	PCN_NETCTX_STATUS_RUNNABLE,         /* ready-to-run, running */
	PCN_NETCTX_STATUS_STOPPED,          /* stopped */
	PCN_NETCTX_STATUS_MAX,
};


/**
 * pcnsrv socket associated with TCP/IP stack
 */
struct pcn_socket_t {
	struct pcn_netctx_t *netctx;        /* owner netctx */
	int remote_id;                      /* owner node id */
	int type;                           /* socket type */
	struct list_head list;              /* socket list */
	union {
		int sockfd;                 /* socket fd for user space */
		                            /* XXX: mtcp_socket, ... */
	};
	struct sockaddr_in  addr;	    /* socket address */
	socklen_t    addrlen;	    	    /* length of the addr */

	int remote_id_bits;                 /* remote ids of this shared listen socket */
	int lb_cnt;                         /* load balancing counter */

        int  backlog;                       /* Handle back log - if present */
        char *start;                        /* Which point in the buffer to copy */
        char *send;                         /* Which point in the buffer to send */
        size_t len;                         /* Length of the remaining data to send */
	uint64_t tag;			    /* Xeon Phi sock id */
	int  tag_available;                 /* Tag available to proceed for IN Socket */
	int  black_duck_cnt;                /* number of ill-behaving count XXX: maybe bug? */
        char buf[MAX_HOST_BUFFER];          /* use this buffer to handle NO_MEM */
};

/**
 * External APIs
 */
int  pcn_init_proxy(int listening_port, int num_netctx,
		     int base_link_port, size_t rx_q_size,
		     struct pcn_proxy_t *proxy);
void pcn_deinit_proxy(struct pcn_proxy_t *proxy);
int  pcn_proxy_accept_link(struct pcn_proxy_t *proxy);
#endif /* _PROXY_H */
