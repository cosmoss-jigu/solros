#include <pthread.h>
#include <pcnlink-api.h>
#include "pcnlink_i.h"

#define PCNLINK_MAX_SOCKFD         1024
struct pcnlink_socket_t *sockfd_tbl[PCNLINK_MAX_SOCKFD];
pthread_mutex_t sockfd_tbl_mutex = PTHREAD_MUTEX_INITIALIZER;

struct pcnlink_socket_t *sockfd_to_plsock(int sockfd)
{
	smp_rmb();
	return sockfd_tbl[sockfd];
}

static
void sockfd_release(int sockfd)
{
	sockfd_tbl[sockfd] = NULL;
	smp_wmb();
}

static
int sockfd_new(struct pcnlink_socket_t *plsock)
{
	int sockfd = -1;
	pthread_mutex_lock(&sockfd_tbl_mutex); {
		int i;
		smp_rmb();
		for (i = 0; i < PCNLINK_MAX_SOCKFD; ++i) {
			if (sockfd_tbl[i] == NULL) {
				smp_rmb();
				if (sockfd_tbl[i] == NULL) {
					sockfd_tbl[i] = plsock;
					sockfd = i;
					smp_wmb();
					break;
				}
			}
		}
	} pthread_mutex_unlock(&sockfd_tbl_mutex);
	plsock->sockfd = sockfd;
	return sockfd;
}

int pcnlink_u_socket(int domain, int type, int protocol)
{
	struct pcnlink_socket_t *plsock;
	int rc;

	plsock = __pcnlink_socket_ex(domain, type, protocol,
				     &rc, NULL, NULL);
	if (!plsock) {
		errno = rc;
		return -1;
	}
	return sockfd_new(plsock);
}

struct pcnlink_socket_t* pcnlink_u_epollsocket(int *ret)
{
	struct pcnlink_socket_t *plsock;
	int rc;

	// create this as RAW socket to differentiate. We need to change this.
	// this is a hack - PF_INET = 2. SOCK_RAW = 3, IPPROTO_RAW =3.
	plsock = __pcnlink_socket_ex(2, 3, 255,
				     &rc, NULL, NULL);
	if (!plsock) {
		errno = rc;
		return NULL;
	}
	// create this as an epoll socket
	*ret = sockfd_new(plsock);
	return plsock;
}

int pcnlink_u_bind(int sockfd, const struct sockaddr *addr,
		 socklen_t addrlen)
{
	return __pcnlink_bind( sockfd_to_plsock(sockfd),
			       addr, addrlen);
}

int pcnlink_u_connect(int sockfd, const struct sockaddr *addr,
		    socklen_t addrlen)
{
	return __pcnlink_connect( sockfd_to_plsock(sockfd),
				 addr, addrlen);
}

int pcnlink_u_listen(int sockfd, int backlog)
{
	return __pcnlink_listen( sockfd_to_plsock(sockfd),
				backlog);
}

int pcnlink_u_setsockopt(int sockfd, int level, int option_name,
		       const void *option_value, socklen_t option_len)
{
	return __pcnlink_setsockopt( sockfd_to_plsock(sockfd),
				    level, option_name,
				    option_value, option_len);
}

int pcnlink_u_getsockopt(int sockfd, int level, int optname, void *optval,
		       socklen_t *optlen)
{
	return __pcnlink_getsockopt( sockfd_to_plsock(sockfd),
				    level, optname, optval, optlen);
}

static ssize_t get_msghdr_len(const struct msghdr *msg)
{
	struct iovec *iov = msg->msg_iov;
	ssize_t i, len = 0;

	for (i = 0; i < msg->msg_iovlen; ++i, ++iov) {
                if (iov->iov_len)
                        len += iov->iov_len;
	}

	return len;
}

ssize_t pcnlink_u_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	return __pcnlink_sendmsg( sockfd_to_plsock(sockfd),
				  msg,
				  get_msghdr_len(msg));
}

ssize_t pcnlink_u_send(int sockfd, const void *buf, size_t len, int flags)
{
	return __pcnlink_send( sockfd_to_plsock(sockfd), buf, len);
}

int pcnlink_u_shutdown(int sockfd, int how)
{
	return __pcnlink_shutdown( sockfd_to_plsock(sockfd),
				  how);
}

int pcnlink_u_close(int sockfd)
{
	int rc = __pcnlink_close( sockfd_to_plsock(sockfd));
	sockfd_release(sockfd);
	return rc;
}

int pcnlink_u_accept(int sockfd, struct sockaddr *addr,
		   socklen_t *addrlen)
{
	struct pcnlink_socket_t *plsock;
	int rc;

	plsock = __pcnlink_accept_ex( sockfd_to_plsock(sockfd),
				      addr, addrlen,
				      &rc, NULL, NULL);
	if (!plsock) {
		errno = rc;
		return -1;
	}
	return sockfd_new(plsock);
}

ssize_t pcnlink_u_recv(int sockfd, void *buf, size_t len, int flags)
{
	return __pcnlink_recv( sockfd_to_plsock(sockfd), buf, len);
}

ssize_t pcnlink_u_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
	return __pcnlink_recvmsg( sockfd_to_plsock(sockfd),
				  msg,
				  get_msghdr_len(msg));
}

int pcnlink_u_fcntl(int sockfd, int cmd, int flags)
{
	struct pcnlink_socket_t *plsock = sockfd_to_plsock(sockfd);

	switch(cmd) {
	case F_SETFL:
		plsock->__file_f_flags = flags;
		return 0;
	case F_GETFL:
		return plsock->__file_f_flags;
	}
	return -1;
}

unsigned int pcnlink_u_poll(int sockfd)
{
	return __pcnlink_poll( sockfd_to_plsock(sockfd));
}
