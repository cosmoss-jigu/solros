#ifndef _PCNLINK_I_H
#define _PCNLINK_I_H
#include <pcnlink-kapi.h>

/**
 * internal functions
 */
#ifdef PCIE_CLOUD_NETWORK_CONF_KERNEL
static int  _pcnlink_rx_worker_kernel(void *p);
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

static void _pcnlink_reap_in_q(void *_channel, void *_netmsg);
static void *_pcnlink_rx_worker(void *p);
static int   _pcnlink_fetch_channels(struct pcnlink_context_t *ctx);
static int  new_plsock(struct pcnlink_socket_t **pplsock, void *private1, void *private2);
static void delete_plsock(struct pcnlink_socket_t *plsock);
static struct ring_buffer_t *get_plsock_rx_q(struct pcnlink_socket_t *plsock,
					     uint32_t command);
static struct pcn_channel_t *assign_channel(void *seed_ptr);
static int is_plsock_non_blocking(struct pcnlink_socket_t *plsock);

static inline
struct pcnlink_socket_t *tag_to_plsock(uint64_t tag)
{
	return (struct pcnlink_socket_t *)tag;
}

static inline
uint64_t plsock_to_tag(struct pcnlink_socket_t *plsock)
{
	return (uint64_t)plsock;
}
#endif /* _PCNLINK_I_H */
