#ifndef __PCN_LINK_EPOLL_H_
#define __PCN_LINK_EPOLL_H_

/*----------------------------------------------------------------------------*/
enum pcnlink_epoll_op
{
        PCNLINK_EPOLL_CTL_ADD = 1,
        PCNLINK_EPOLL_CTL_DEL = 2,
        PCNLINK_EPOLL_CTL_MOD = 3,
};
/*----------------------------------------------------------------------------*/
enum pcnlink_event_type
{
        PCNLINK_EPOLLNONE  = 0x000,
        PCNLINK_EPOLLIN    = 0x001,
        PCNLINK_EPOLLPRI   = 0x002,
        PCNLINK_EPOLLOUT   = 0x004,
        PCNLINK_EPOLLRDNORM        = 0x040,
        PCNLINK_EPOLLRDBAND        = 0x080,
        PCNLINK_EPOLLWRNORM        = 0x100,
        PCNLINK_EPOLLWRBAND        = 0x200,
        PCNLINK_EPOLLMSG           = 0x400,
        PCNLINK_EPOLLERR           = 0x008,
        PCNLINK_EPOLLHUP           = 0x010,
        PCNLINK_EPOLLRDHUP         = 0x2000,
        PCNLINK_EPOLLONESHOT       = (1 << 30),
        PCNLINK_EPOLLET            = (1 << 31)
};
/*----------------------------------------------------------------------------*/
typedef union pcnlink_epoll_data
{
        void *ptr;
        int sockid;
        uint32_t u32;
        uint64_t u64;
} pcnlink_epoll_data_t;
/*----------------------------------------------------------------------------*/
struct pcnlink_epoll_event
{
        uint32_t events;
        pcnlink_epoll_data_t data;
};
/*----------------------------------------------------------------------------*/
int
pcnlink_epoll_create(int size);
/*----------------------------------------------------------------------------*/
int
pcnlink_epoll_ctl(int epid, int op, int sockid,
		 struct pcnlink_epoll_event *event);
/*----------------------------------------------------------------------------*/
int
pcnlink_epoll_wait(int epid, struct pcnlink_epoll_event *events,
		int maxevents, int timeout);
/*----------------------------------------------------------------------------*/

#endif /* __PCNLINK_EPOLL_H_ */
