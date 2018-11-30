#ifndef _PCNLINK_UAPI_H
#define _PCNLINK_UAPI_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * pcnlink socket APIs
 */
// need this API for epoll.
struct pcnlink_socket_t *sockfd_to_plsock(int sockfd);
struct pcnlink_socket_t* pcnlink_u_epollsocket(int *ret);

int pcnlink_u_socket(int domain, int type, int protocol);
int pcnlink_u_bind(int sockfd, const struct sockaddr *addr,
		 socklen_t addrlen);
int pcnlink_u_connect(int sockfd, const struct sockaddr *addr,
		    socklen_t addrlen);
int pcnlink_u_listen(int sockfd, int backlog);
int pcnlink_u_setsockopt(int sockfd, int level, int option_name,
		       const void *option_value, socklen_t option_len);
int pcnlink_u_getsockopt(int sockfd, int level, int optname, void *optval,
		       socklen_t *optlen);
ssize_t pcnlink_u_sendmsg(int sockfd, const struct msghdr *msg, int flags);
ssize_t pcnlink_u_send(int sockfd, const void *buf, size_t len, int flags);
int pcnlink_u_shutdown(int sockfd, int how);
int pcnlink_u_close(int sockfd);
int pcnlink_u_accept(int sockfd, struct sockaddr *addr,
		   socklen_t *addrlen);
ssize_t pcnlink_u_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t pcnlink_u_recvmsg(int sockfd, struct msghdr *msg, int flags);
int pcnlink_u_fcntl(int sockfd, int cmd, int flags);
unsigned int pcnlink_u_poll(int sockfd);
#endif /* _PCNLINK_UAPI_H */
