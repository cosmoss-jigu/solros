#include <pcnporting.h>
#include "pcnlink_eventpoll.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

#define SPIN_BEFORE_SLEEP 0
#define SPIN_THRESH 10000000

/*----------------------------------------------------------------------------*/
char *event_str[] = {"NONE", "IN", "PRI", "OUT", "ERR", "HUP", "RDHUP"};
/*----------------------------------------------------------------------------*/
char *
event_to_string(uint32_t event)
{
        switch (event) {
                case PCNLINK_EPOLLNONE:
                        return event_str[0];
                        break;
                case PCNLINK_EPOLLIN:
                        return event_str[1];
                        break;
                case PCNLINK_EPOLLPRI:
                        return event_str[2];
                        break;
                case PCNLINK_EPOLLOUT:
                        return event_str[3];
                        break;
                case PCNLINK_EPOLLERR:
                        return event_str[4];
                        break;
                case PCNLINK_EPOLLHUP:
                        return event_str[5];
                        break;
                case PCNLINK_EPOLLRDHUP:
                        return event_str[6];
                        break;
                default:
			break;
        }

	pcn_dbg("Invalid Event to String %d\n", event);
        return NULL;
}

/*----------------------------------------------------------------------------*/
struct event_queue *
create_event_queue(int size)
{
        struct event_queue *eq;
	int rc;

        eq = (struct event_queue *)pcn_calloc(1, sizeof(struct event_queue));
        if (!eq) {
		rc = -ENOMEM;
                goto err_out;
	}

        eq->start = 0;
        eq->end = 0;
        eq->size = size;
        eq->events = (struct pcnlink_epoll_event_int *)
                        pcn_calloc(size, sizeof(struct pcnlink_epoll_event_int));
        if (!eq->events) {
                pcn_free(eq);
                eq = NULL;
		rc = -ENOMEM;
		goto err_out;
        }
        eq->num_events = 0;

out:
	return eq;
err_out:
	pcn_dbg("event queue creation failed %d \n", rc);
        goto out;
}
/*----------------------------------------------------------------------------*/
void
destroy_event_queue(struct event_queue *eq)
{
        if (eq->events)
                pcn_free(eq->events);

        pcn_free(eq);
}
/*----------------------------------------------------------------------------*/
int
pcnlink_epoll_create(int size)
{
        struct pcnlink_epoll *ep = NULL;
        struct pcnlink_socket_t *epsocket = NULL;
	int sockfd;

        if (size <= 0) {
                errno = EINVAL;
                goto err_out;
        }

        epsocket = pcnlink_u_epollsocket(&sockfd);
        if (!epsocket) {
                errno = ENFILE;
                goto err_out;
        }
	epsocket->type = PCN_SOCK_TYPE_EPOLL;

        ep = (struct pcnlink_epoll *)pcn_calloc(1, sizeof(struct pcnlink_epoll));
        if (!ep) {
                errno = ENFILE;
		goto err_out;
        }
	ep->sockfd = sockfd;

        /* create event queues */
        ep->usr_queue = create_event_queue(size);
        if (!ep->usr_queue) {
                errno = ENFILE;
		goto err_out;
	}

        ep->usr_shadow_queue = create_event_queue(size);
        if (!ep->usr_shadow_queue) {
                errno = ENFILE;
		goto err_out;
        }

        ep->pcnlink_queue = create_event_queue(size);
        if (!ep->pcnlink_queue) {
                errno = ENFILE;
		goto err_out;
        }

        epsocket->ep = ep;
        return ep->sockfd;
err_out:
	pcn_dbg("EPOLL socket creation error %d\n", errno);
	/* release ep */
	if (ep && ep->pcnlink_queue) {
                destroy_event_queue(ep->pcnlink_queue);
		ep->pcnlink_queue = NULL;
	}
        if (ep && ep->usr_shadow_queue) {
                destroy_event_queue(ep->usr_shadow_queue);
		ep->usr_shadow_queue = NULL;
        }
        if (ep && ep->usr_queue) {
                destroy_event_queue(ep->usr_queue);
		ep->usr_queue = NULL;
        }
        if (ep && ep->sockfd) {
		pcnlink_u_close(ep->sockfd);
		ep->sockfd = -1;
	}
	pcn_free(ep);

	/* release epsocket */
	if (epsocket) {
		pcnlink_u_close(sockfd);
		epsocket = NULL;
	}
	return -1;
}
/*----------------------------------------------------------------------------*/
int
close_epoll_socket(int epid)
{
        struct pcnlink_epoll *ep = NULL;
        struct pcnlink_socket_t* epsocket = NULL;
	int rc = 0;

	epsocket = sockfd_to_plsock(epid);
	if (!epsocket) {
                errno = EINVAL;
		goto err_out;
	}

        ep = epsocket->ep;
        if (!ep) {
                errno = EINVAL;
		goto err_out;
        }

        destroy_event_queue(ep->usr_queue);
        destroy_event_queue(ep->usr_shadow_queue);
        destroy_event_queue(ep->pcnlink_queue);
        pcn_free(ep);

        pcn_mutex_lock(&ep->epoll_lock);
        pcn_wait_wake_up(&ep->epoll_cond);
        pcn_mutex_unlock(&ep->epoll_lock);

        pcn_wait_destroy(&ep->epoll_cond);
        pcn_mutex_destroy(&ep->epoll_lock);

out:
        return rc;
err_out:
	pcn_dbg("epoll socket close failed %d\n", errno);
	rc = -errno;
	goto out;
}
/*----------------------------------------------------------------------------*/
static int
raise_pending_stream_events(struct pcnlink_epoll *ep, struct pcnlink_socket_t *socket)
{
	int rc = 0;

	// This has to be handled based on the data received in ring buffer

        /* if there are payloads already read before epoll registration */
        /* generate read event */
        /*if (socket->epoll & PCNLINK_EPOLLIN) {
        	add_epoll_event(ep, USR_SHADOW_EVENT_QUEUE, socket, PCNLINK_EPOLLIN);
        }*/

        /* same thing to the write event */
        /*if (socket->epoll & PCNLINK_EPOLLOUT) {
        	add_epoll_event(ep, USR_SHADOW_EVENT_QUEUE, socket, PCNLINK_EPOLLOUT);
        }*/

	return 0;
}
/*----------------------------------------------------------------------------*/
int
pcnlink_epoll_ctl(int epid, int op, int sockid, struct pcnlink_epoll_event *event)
{
        struct pcnlink_epoll *ep = NULL;
        struct pcnlink_socket_t *socket = NULL;
        struct pcnlink_socket_t *epsocket = NULL;
        uint32_t events;

        if (epid < 0) {
                errno = EBADF;
		goto err_out;
        }

        if (sockid < 0) {
                errno = EBADF;
		goto err_out;
        }

	epsocket = sockfd_to_plsock(epid);
	if (!epsocket) {
                errno = EINVAL;
		goto err_out;
	}

        if (epsocket->type != PCN_SOCK_TYPE_EPOLL) {
                errno = EINVAL;
		goto err_out;
        }

        ep = epsocket->ep;
	
        if (!ep || (!event && op != PCNLINK_EPOLL_CTL_DEL)) {
                errno = EINVAL;
		goto err_out;
        }

	socket = sockfd_to_plsock(sockid);
        if (!socket) {
                errno = EINVAL;
                goto err_out;
        }

        if (op == PCNLINK_EPOLL_CTL_ADD) {
                if (socket->epoll) {
                        errno = EEXIST;
			goto err_out;
                }

                /* EPOLLERR and EPOLLHUP are registered as default */
                events = event->events;
                events |= (PCNLINK_EPOLLERR | PCNLINK_EPOLLHUP);
                socket->ep_data = event->data;
                socket->epoll = events;

		raise_pending_stream_events(ep, socket);
		socket->ep = epsocket->ep;

        } else if (op == PCNLINK_EPOLL_CTL_MOD) {
                if (!socket->epoll) {
                        pcn_mutex_unlock(&ep->epoll_lock);
                        errno = ENOENT;
			goto err_out;
                }

                events = event->events;
                events |= (PCNLINK_EPOLLERR | PCNLINK_EPOLLHUP);
                socket->ep_data = event->data;
                socket->epoll = events;

		raise_pending_stream_events(ep, socket);

        } else if (op == PCNLINK_EPOLL_CTL_DEL) {
                if (!socket->epoll) {
                        errno = ENOENT;
			goto err_out;
                }

                socket->epoll = PCNLINK_EPOLLNONE;
		socket->ep = NULL;
        }

        return 0;
err_out:
	pcn_dbg("EPOLL CTL failed with  %d\n", errno);
	return -1;
	
}
/*----------------------------------------------------------------------------*/
int
pcnlink_epoll_wait(int epid, struct pcnlink_epoll_event *events, int maxevents, int timeout)
{
        struct pcnlink_epoll *ep;
        struct event_queue *eq;
        struct event_queue *eq_shadow;
        struct pcnlink_socket_t *event_socket;
	struct pcnlink_socket_t *epsocket = NULL;
        int validity;
        int i, cnt, ret;
        int num_events;

        if (epid < 0)  {
                errno = EBADF;
		goto err_out;
        }

        epsocket = sockfd_to_plsock(epid);
        if (!epsocket) {
                errno = EINVAL;
                goto err_out;
        }

        if (epsocket->type != PCN_SOCK_TYPE_EPOLL) {
                errno = EINVAL;
                goto err_out;
        }

        ep = epsocket->ep;

        if (!ep || (!events)) {
                errno = EINVAL;
                goto err_out;
        }

        ep->stat.calls++;

#if SPIN_BEFORE_SLEEP
        int spin = 0;
        while (ep->num_events == 0 && spin < SPIN_THRESH) {
                spin++;
        }
#endif /* SPIN_BEFORE_SLEEP */

        if (pcn_mutex_lock(&ep->epoll_lock)) {
                if (errno == EDEADLK)
			goto err_out;
        }

wait:
        eq = ep->usr_queue;
        eq_shadow = ep->usr_shadow_queue;

        /* wait until event occurs */
        while (eq->num_events == 0 && eq_shadow->num_events == 0 && timeout != 0) {

/*#if INTR_SLEEPING_PCNLINK
                if (pcnlink->wakeup_flag && pcnlink->is_sleeping) {
                        pthread_kill(pcnlink->ctx->thread, SIGUSR1);
                }
#endif*/
                ep->stat.waits++;
                ep->waiting = 1;
                if (timeout > 0) {
                        struct timespec deadline;

                        clock_gettime(CLOCK_REALTIME, &deadline);
                        if (timeout > 1000) {
                                int sec;
                                sec = timeout / 1000;
                                deadline.tv_sec += sec;
                                timeout -= sec * 1000;
                        }

                        if (deadline.tv_nsec >= 1000000000) {
                                deadline.tv_sec++;
                                deadline.tv_nsec -= 1000000000;
                        }

                        //deadline.tv_sec = pcnlink->cur_tv.tv_sec;
                        //deadline.tv_nsec = (pcnlink->cur_tv.tv_usec + timeout * 1000) * 1000;
                        ret = pcn_wait_sleep_on_timeout(&ep->epoll_cond,
							&ep->epoll_lock, &deadline);
                        if (ret && ret != ETIMEDOUT) {
                                /* errno set by pcn_cond_timedwait() */
                                pcn_mutex_unlock(&ep->epoll_lock);
				goto err_out;
                        }
                        timeout = 0;
                } else if (timeout < 0) {
                        ret = pcn_wait_sleep_on(&ep->epoll_cond, &ep->epoll_lock);
                        if (ret) {
                                /* errno set by pcn_cond_wait() */
                                pcn_mutex_unlock(&ep->epoll_lock);
				goto err_out;
                        }
        	}
		ep->waiting = 0;
	}

        /* fetch events from the user event queue */
        cnt = 0;
        num_events = eq->num_events;
        for (i = 0; i < num_events && cnt < maxevents; i++) {
                event_socket = sockfd_to_plsock(eq->events[eq->start].sockid);
                validity = 1;
                /*if (event_socket->socktype == PCNLINK_SOCK_UNUSED)
                        validity = 0;*/
                if (!(event_socket->epoll & eq->events[eq->start].ev.events))
                        validity = 0;
                if (!(event_socket->events & eq->events[eq->start].ev.events))
                        validity = 0;

                if (validity) {
                        events[cnt++] = eq->events[eq->start].ev;
                        assert(eq->events[eq->start].sockid >= 0);
                        ep->stat.handled++;
                } else {
                        ep->stat.invalidated++;
                }
                event_socket->events &= (~eq->events[eq->start].ev.events);

                eq->start++;
                eq->num_events--;
                if (eq->start >= eq->size) {
                        eq->start = 0;
                }
        }

        /* fetch eventes from user shadow event queue */
        eq = ep->usr_shadow_queue;
        num_events = eq->num_events;
        for (i = 0; i < num_events && cnt < maxevents; i++) {
                event_socket = sockfd_to_plsock(eq->events[eq->start].sockid);
                validity = 1;
                /*if (event_socket->socktype == PCNLINK_SOCK_UNUSED)
                        validity = 0;*/
                if (!(event_socket->epoll & eq->events[eq->start].ev.events))
                        validity = 0;
                if (!(event_socket->events & eq->events[eq->start].ev.events))
                        validity = 0;

                if (validity) {
                        events[cnt++] = eq->events[eq->start].ev;
                        assert(eq->events[eq->start].sockid >= 0);
                        ep->stat.handled++;
                } else {
                        ep->stat.invalidated++;
                }
                event_socket->events &= (~eq->events[eq->start].ev.events);

                eq->start++;
                eq->num_events--;
                if (eq->start >= eq->size) {
                        eq->start = 0;
                }
        }

        if (cnt == 0 && timeout != 0)
                goto wait;

        pcn_mutex_unlock(&ep->epoll_lock);

        return cnt;
err_out:
	pcn_dbg("EPOLL wait failed with  %d\n", errno);
	return -1;
}
/*----------------------------------------------------------------------------*/
inline int
add_epoll_event(struct pcnlink_epoll *ep,
                int queue_type, struct pcnlink_socket_t *socket, uint32_t event)
{
        struct event_queue *eq;
        int index;

        if (!ep || !socket || !event) {
		errno = EBADF;
		goto err_out;
	}

        ep->stat.issued++;

        if (socket->events & event) {
		errno = EBADF;
		goto err_out;
        }

        if (queue_type == PCNLINK_EVENT_QUEUE) {
                eq = ep->pcnlink_queue;
        } else if (queue_type == USR_EVENT_QUEUE) {
                eq = ep->usr_queue;
                pcn_mutex_lock(&ep->epoll_lock);
        } else if (queue_type == USR_SHADOW_EVENT_QUEUE) {
                eq = ep->usr_shadow_queue;
        } else {
		goto err_out;
        }

        if (eq->num_events >= eq->size) {
                if (queue_type == USR_EVENT_QUEUE)
                        pcn_mutex_unlock(&ep->epoll_lock);
		goto err_out;
        }

        index = eq->end++;

        socket->events |= event;
        eq->events[index].sockid = socket->sockfd;
        eq->events[index].ev.events = event;
        eq->events[index].ev.data = socket->ep_data;

        if (eq->end >= eq->size) {
                eq->end = 0;
        }
        eq->num_events++;

#if 0
        TRACE_EPOLL("Socket %d New event: %s, start: %u, end: %u, num: %u\n",
                        ep->events[index].sockid,
                        event_to_string(ep->events[index].ev.events),
                        ep->start, ep->end, ep->num_events);
#endif

        if (queue_type == USR_EVENT_QUEUE)
                pcn_mutex_unlock(&ep->epoll_lock);

        ep->stat.registered++;
        return 0;

err_out:
	pcn_dbg("add EPOLL event failed with %d\n", errno);
	return -1;
}

void
pcnlink_epoll_add_event(struct pcnlink_socket_t *plsock)
{
	if (plsock->type == PCN_SOCK_TYPE_LISTEN) {
		if (plsock->epoll != PCNLINK_EPOLLNONE) {
			add_epoll_event(plsock->ep, USR_EVENT_QUEUE,
					plsock, PCNLINK_EPOLLIN);
		}
		return;
	}

	if (plsock->epoll & PCNLINK_EPOLLIN) {
		if (plsock->epoll != PCNLINK_EPOLLNONE) {
			add_epoll_event(plsock->ep, USR_EVENT_QUEUE,
					plsock, PCNLINK_EPOLLIN);
		}
	}
	return;
}

void
pcnlink_epoll_signal(void *ep)
{
	struct pcnlink_epoll *e = (struct pcnlink_epoll *) ep;

        if (ep) {
                pcn_mutex_lock(&e->epoll_lock);
                if (e->waiting) {
                        pcn_wait_wake_up(&e->epoll_cond);
                }
                pcn_mutex_unlock(&e->epoll_lock);
        }
}
