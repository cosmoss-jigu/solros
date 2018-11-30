#ifndef __PCNLINK_EVENTPOLL_H_
#define __PCNLINK_EVENTPOLL_H_

#include <pcnlink-api.h>
#include <pcnlink-epoll.h>

/*----------------------------------------------------------------------------*/
struct pcnlink_epoll_stat
{
        uint64_t calls;
        uint64_t waits;
        uint64_t wakes;

        uint64_t issued;
        uint64_t registered;
        uint64_t invalidated;
        uint64_t handled;
};
/*----------------------------------------------------------------------------*/
struct pcnlink_epoll_event_int
{
        struct pcnlink_epoll_event ev;
        int sockid;
};
/*----------------------------------------------------------------------------*/
enum event_queue_type
{
        USR_EVENT_QUEUE = 0,
        USR_SHADOW_EVENT_QUEUE = 1,
        PCNLINK_EVENT_QUEUE = 2
};
/*----------------------------------------------------------------------------*/
struct event_queue
{
        struct pcnlink_epoll_event_int *events;
        int start;                      // starting index
        int end;                        // ending index

        int size;                       // max size
        int num_events;                 // number of events
};
/*----------------------------------------------------------------------------*/
struct pcnlink_epoll
{
        struct event_queue *usr_queue;
        struct event_queue *usr_shadow_queue;
        struct event_queue *pcnlink_queue;

        uint8_t waiting;
        struct pcnlink_epoll_stat stat;
	int sockfd;

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
        pthread_cond_t    epoll_cond;
        pthread_mutex_t   epoll_lock;
#else
        wait_queue_head_t epoll_cond;
        struct mutex      epoll_lock;
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */
};

/*----------------------------------------------------------------------------*/
int
close_epoll_socket(int epid);

void
pcnlink_epoll_add_event(struct pcnlink_socket_t *plsock);

void
pcnlink_epoll_signal(void *ep);

#endif /* __PCNLINK_EVENTPOLL_H_ */

