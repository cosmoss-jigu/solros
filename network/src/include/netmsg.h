#ifndef _NETMSG_H
#define _NETMSG_H

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
# include <stdint.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
#else
# include <linux/kernel.h>
# include <linux/socket.h>
# include <linux/in.h>
# include <linux/string.h>

typedef uint32_t socklen_t;
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */


/**
 * command types
 */
enum {
	/*
	 * RPC request from Xeon Phi to host
	 */
	PCN_NETMSG_T_SOCKET = 0,
	PCN_NETMSG_T_BIND,
	PCN_NETMSG_T_CONNECT,
	PCN_NETMSG_T_LISTEN,
	PCN_NETMSG_T_SETSOCKOPT,
	PCN_NETMSG_T_GETSOCKOPT, /* 5 */
	PCN_NETMSG_T_SENDMSG,
	PCN_NETMSG_T_SENDDATA,
	PCN_NETMSG_T_SHUTDOWN,
	PCN_NETMSG_T_CLOSE,

	/*
	 * RPC response from host to Xeon Phi
	 */
	PCN_NETMSG_R_SOCKET,     /* 10 */
	PCN_NETMSG_R_BIND,
	PCN_NETMSG_R_CONNECT,
	PCN_NETMSG_R_LISTEN,
	PCN_NETMSG_R_SETSOCKOPT,
	PCN_NETMSG_R_GETSOCKOPT, /* 15 */
	PCN_NETMSG_R_SENDMSG,
	PCN_NETMSG_R_SENDDATA,
	PCN_NETMSG_R_SHUTDOWN,
	PCN_NETMSG_R_CLOSE,

	/*
	 * Event from host to Xeon Phi
	 */
	PCN_NETMSG_E_ACCEPT,     /* 20 */
	PCN_NETMSG_E_RECVDATA,

	/*
	 * Notification from Xeon Phi to host
	 */
	PCN_NETMSG_N_SOCKET,

	PCN_NETMSG_MAX,                     /* end marker */
};

const static char *netmsg_command_str[] = {
	"T_SOCKET",
	"T_BIND",
	"T_CONNECT",
	"T_LISTEN",
	"T_SETSOCKOPT",
	"T_GETSOCKOPT",
	"T_SENDMSG",
	"T_SENDDATA",
	"T_SHUTDOWN",
	"T_CLOSE",
	"R_SOCKET",
	"R_BIND",
	"R_CONNECT",
	"R_LISTEN",
	"R_SETSOCKOPT",
	"R_GETSOCKOPT",
	"R_SENDMSG",
	"R_SENDDATA",
	"R_SHUTDOWN",
	"R_CLOSE",
	"E_ACCEPT",
	"E_RECVDATA",
	"N_SOCKET",
	"\0",
};

/**
 * common netmsg header
 */
struct netmsg_header_t {
        uint64_t               seqnum;      /* sequence number */
        uint32_t               size;        /* netmsg size */
        uint32_t               command;     /* command type */
        uint64_t               tag;         /* tag id of this message */
	uint8_t                body[0];     /* start of message body */
} __attribute__((__packed__));


/**
 * common netmsg response header
 */
struct netmsg_re_t {
	struct netmsg_header_t hdr;         /* header */
	int32_t                rc;          /* return code */
} __attribute__((__packed__));


/**
 * RPC:SOCKET
 * int socket(int domain, int type, int protocol)
 * <- size[4] T_SOCKET tag[8] domain[4] type[4] protocol[4]
 * -> size[4] R_SOCKET tag[8] rc[4] sockid[8] */
struct netmsg_t_socket_t {
	struct netmsg_header_t hdr;         /* header */
	int32_t                domain;      /* domain */
	int32_t                type;        /* type */
	int32_t                protocol;    /* protocol */
} __attribute__((__packed__));

struct netmsg_r_socket_t {
        struct netmsg_re_t     re;          /* common response */
	uint64_t               sockid;      /* sock id */
} __attribute__((__packed__));


/**
 * RPC:BIND
 * int bind(int sockfd, const struct sockaddr *addr,
 *          socklen_t addrlen)
 * <- size[4] T_BIND tag[8] sockid[8] alen[4] [struct sockaddr_in]
 * -> size[4] R_BIND tag[8] rc[4] */
struct netmsg_t_bind_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
	uint32_t               addrlen;     /* addrlen */
	struct sockaddr_in     addr;        /* inet address */
} __attribute__((__packed__));

struct netmsg_r_bind_t {
        struct netmsg_re_t     re;          /* common response */
} __attribute__((__packed__));


/**
 * RPC:CONNECT
 * int connect(int sockfd, const stnruct sockaddr *addr,
 *             socklen_t addrlen);
 * <- size[4] T_CONNECT tag[8] sockid[8] alen[4] [struct sockaddr_in]
 * -> size[4] R_CONNECT tag[8] rc[4] */
struct netmsg_t_connect_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
	uint32_t               addrlen;     /* addrlen */
	struct sockaddr_in     addr;        /* inet address */
} __attribute__((__packed__));

struct netmsg_r_connect_t {
        struct netmsg_re_t     re;          /* common response */
} __attribute__((__packed__));


/**
 * RPC:LISTEN
 * int listen(int sockfd, int backlog)
 * <- size[4] T_LISTEN tag[8] sockid[8] backlog[4]
 * -> size[4] R_LISTEN tag[8] rc[4] */
struct netmsg_t_listen_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
	int32_t                backlog;     /* backlog */
} __attribute__((__packed__));

struct netmsg_r_listen_t {
        struct netmsg_re_t     re;          /* common response */
} __attribute__((__packed__));


/**
 * RPC:SETSOCKOPT
 * int setsockopt(int socket, int level, int option_name,
 * const void *option_value, socklen_t option_len);
 * <- size[4] T_SETSOCKOPT tag[8] sockid[8]
 *            level[4] optname[4] optlen[4] optval[N]
 * -> size[4] R_SETSOCKOPT tag[8] rc[4] */
struct netmsg_t_setsockopt_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
	int32_t                level;       /* level */
	int32_t                optname;     /* option name */
	uint32_t               optlen;      /* option length */
	uint8_t                optval[0];   /* option value */
} __attribute__((__packed__));

struct netmsg_r_setsockopt_t {
        struct netmsg_re_t     re;          /* common response */
} __attribute__((__packed__));


/**
 * RPC:GETSOCKOPT
 * int getsockopt(int sockfd, int level, int optname, void *optval,
 * socklen_t *optlen);
 * <- size[4] T_GETSOCKOPT tag[8] sockid[8] level[4] optname[4] optlen[4]
 * -> size[4] R_GETSOCKOPT tag[8] rc[4] optlen[4] optval[N] */
struct netmsg_t_getsockopt_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
	int32_t                level;       /* level */
	int32_t                optname;     /* option name */
	uint32_t               optlen;      /* option length */
} __attribute__((__packed__));

struct netmsg_r_getsockopt_t {
        struct netmsg_re_t     re;          /* common response */
	uint32_t               optlen;      /* option length */
	uint8_t                optval[0];   /* option value */
} __attribute__((__packed__));


/**
 * RPC:SENDMSG
 * ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
 * <- size[4] T_SENDMSG tag[8] sockid[8] flags[4]
 *    [struct msghdr]
 *    msg_name[msghdr.msg_namelen]
 *    msg_iov[msghdr.msg_iovlen]
 *    msg_control[msghdr.msg_controllen]
 * -> size[4] R_SENDMSG tag[8] rc[4] */
struct netmsg_t_sendmsg_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
	int32_t                flags;       /* flags */
	struct msghdr          msghdr;      /* msghdr */
	uint8_t                msghdr_x[0]; /* msghdr extended */
} __attribute__((__packed__));

struct netmsg_r_sendmsg_t {
        struct netmsg_re_t     re;          /* common response */
} __attribute__((__packed__));


/**
 * RPC:SENDDATA
 * ssize_t send(int sockfd, const void *buf, size_t len, int flags)
 * <- size[4] T_SENDDATA tag[8] sockid[8] datalen[4] data[N]
 * -> size[4] R_SENDDATA tag[8] rc[4] */
struct netmsg_t_senddata_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
	uint32_t               datalen;     /* data length */
	uint8_t                data[0];     /* data */
} __attribute__((__packed__));

struct netmsg_r_senddata_t {
        struct netmsg_re_t     re;          /* common response */
} __attribute__((__packed__));


/**
 * RPC:SHUTDOWN
 * int shutdown(int sockfd, int how);
 * <- size[4] T_SHUTDOWN tag[8] sockid[8] how[4]
 * -> size[4] R_SHUTDOWN tag[8] rc[4] */
struct netmsg_t_shutdown_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
	int32_t                how;         /* how */
} __attribute__((__packed__));

struct netmsg_r_shutdown_t {
        struct netmsg_re_t     re;          /* common response */
} __attribute__((__packed__));


/**
 * RPC:CLOSE
 * int close(int fd)
 * <- size[4] T_CLOSE tag[8] sockid[8]
 * -> size[4] R_CLOSE tag[8] rc[4] */
struct netmsg_t_close_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
} __attribute__((__packed__));

struct netmsg_r_close_t {
        struct netmsg_re_t     re;          /* common response */
} __attribute__((__packed__));


/**
 * EVENT:ACCEPT
 * int accept(int sockfd, struct sockaddr *addr,
 *            socklen_t *addrlen)
 * -> size[4] E_ACCEPT tag[8] sockid[8] alen[4] [struct sockaddr] */
struct netmsg_e_accept_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
	uint32_t               addrlen;     /* addrlen */
	struct sockaddr_in     addr;        /* inet address */
} __attribute__((__packed__));


/**
 * EVENT:RECVDATA
 * ssize_t recv(int sockfd, void *buf, size_t len, int flags);
 * ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
 * -> size[4] E_RECVDATA tag[8] sockid[8] len[4] data[N] */
struct netmsg_e_recvdata_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
	uint32_t               datalen;     /* data length */
	uint8_t                data[0];     /* data */
} __attribute__((__packed__));


/**
 * NOTIFICATION:SOCKET
 * -> size[4] N_SOCKET tag[8] sockid[8] */
struct netmsg_n_socket_t {
	struct netmsg_header_t hdr;         /* header */
	uint64_t               sockid;      /* sock id */
} __attribute__((__packed__));

/**
 * marshalling APIs
 */
#define NETMSG_UNKNOWN_SEQNUM 0xFFFFFFFFFFFFFFFF

int netmsg_marshal_header(int dryrun, void *buff, uint64_t seqnum,
			  uint32_t size, uint32_t command, uint64_t tag);
int netmsg_unmarshal_header(int dryrun, void *buff, struct netmsg_header_t *hdr);
int netmsg_marshal_re(int dryrun, void *buff, uint64_t seqnum, uint32_t size,
		      uint32_t command, uint64_t tag, int32_t rc);
int netmsg_unmarshal_re(int dryrun, void *buff, struct netmsg_re_t *re);
int netmsg_marshal_t_socket(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			    int32_t domain, int32_t type, int32_t protocol);
int netmsg_unmarshal_t_socket(int dryrun, void *buff, struct netmsg_t_socket_t *t_socket);
int netmsg_marshal_r_socket(int dryrun, void *buff, uint64_t seqnum,
			    uint64_t tag, int32_t rc, uint64_t sockid);
int netmsg_unmarshal_r_socket(int dryrun, void *buff, struct netmsg_r_socket_t *r_socket);
int netmsg_marshal_t_bind(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			  uint64_t sockid, uint32_t addrlen, struct sockaddr_in *addr);
int netmsg_unmarshal_t_bind(int dryrun, void *buff, struct netmsg_t_bind_t *t_bind);
int netmsg_marshal_r_bind(int dryrun, void *buff, uint64_t seqnum,
			  uint64_t tag, int32_t rc);
int netmsg_unmarshal_r_bind(int dryrun, void *buff, struct netmsg_r_bind_t *r_bind);
int netmsg_marshal_t_connect(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			     uint64_t sockid, uint32_t addrlen, struct sockaddr_in *addr);
int netmsg_unmarshal_t_connect(int dryrun, void *buff, struct netmsg_t_connect_t *t_connect);
int netmsg_marshal_r_connect(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, int32_t rc);
int netmsg_unmarshal_r_connect(int dryrun, void *buff, struct netmsg_r_connect_t *r_connect);
int netmsg_marshal_t_listen(int dryrun, void *buff, uint64_t seqnum,
			    uint64_t tag, uint64_t sockid, int32_t backlog);
int netmsg_unmarshal_t_listen(int dryrun, void *buff, struct netmsg_t_listen_t *t_listen);
int netmsg_marshal_r_listen(int dryrun, void *buff, uint64_t seqnum,
			    uint64_t tag, int32_t rc);
int netmsg_unmarshal_r_listen(int dryrun, void *buff, struct netmsg_r_listen_t *r_listen);
int netmsg_marshal_t_setsockopt(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
				uint64_t sockid, int32_t level, int32_t optname,
				uint32_t optlen, const void *optval);
int netmsg_unmarshal_t_setsockopt(int dryrun, void *buff,
				  struct netmsg_t_setsockopt_t *t_setsockopt);
int netmsg_marshal_r_setsockopt(int dryrun, void *buff, uint64_t seqnum,
				uint64_t tag, int32_t rc);
int netmsg_unmarshal_r_setsockopt(int dryrun, void *buff,
				  struct netmsg_r_setsockopt_t *r_setsockopt);
int netmsg_marshal_t_getsockopt(int dryrun, void *buff, uint64_t seqnum,
				uint64_t tag, uint64_t sockid, int32_t level,
				int32_t optname, uint32_t optlen);
int netmsg_unmarshal_t_getsockopt(int dryrun, void *buff,
				  struct netmsg_t_getsockopt_t *t_getsockopt);
int netmsg_marshal_r_getsockopt(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
				int32_t rc, uint32_t optlen, void *optval);
int netmsg_unmarshal_r_getsockopt(int dryrun, void *buff,
				  struct netmsg_r_getsockopt_t *r_getsockopt);
int netmsg_marshal_t_sendmsg(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			     uint64_t sockid, int32_t flags, struct msghdr *msghdr);
int netmsg_unmarshal_t_sendmsg(int dryrun, void *buff, struct netmsg_t_sendmsg_t *t_sendmsg);
int netmsg_marshal_r_sendmsg(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, int32_t rc);
int netmsg_unmarshal_r_sendmsg(int dryrun, void *buff,
			       struct netmsg_r_sendmsg_t *r_sendmsg);
int netmsg_marshal_t_senddata(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			      uint64_t sockid, uint32_t datalen, const void *data);
int netmsg_unmarshal_t_senddata(int dryrun, void *buff, struct netmsg_t_senddata_t *t_senddata);
int netmsg_marshal_r_senddata(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, int32_t rc);
int netmsg_unmarshal_r_senddata(int dryrun, void *buff,
				struct netmsg_r_senddata_t *r_senddata);
int netmsg_marshal_t_shutdown(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			      uint64_t sockid, int32_t how);
int netmsg_unmarshal_t_shutdown(int dryrun, void *buff, struct netmsg_t_shutdown_t *t_shutdown);
int netmsg_marshal_r_shutdown(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, int32_t rc);
int netmsg_unmarshal_r_shutdown(int dryrun, void *buff, struct netmsg_r_shutdown_t *r_shutdown);
int netmsg_marshal_t_close(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, uint64_t sockid);
int netmsg_unmarshal_t_close(int dryrun, void *buff, struct netmsg_t_close_t *t_close);
int netmsg_marshal_r_close(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, int32_t rc);
int netmsg_unmarshal_r_close(int dryrun, void *buff, struct netmsg_r_close_t *r_close);
int netmsg_marshal_e_accept(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			    uint64_t sockid, uint32_t addrlen, struct sockaddr_in *addr);
int netmsg_unmarshal_e_accept(int dryrun, void *buff, struct netmsg_e_accept_t *e_accept);
int netmsg_marshal_e_recvdata(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			      uint64_t sockid, uint32_t datalen, void *data);
int netmsg_unmarshal_e_recvdata(int dryrun, void *buff, struct netmsg_e_recvdata_t *e_recvdata);
int netmsg_marshal_n_socket(int dryrun, void *buff, uint64_t seqnum,
			    uint64_t tag, uint64_t sockid);
int netmsg_unmarshal_n_socket(int dryrun, void *buff, struct netmsg_n_socket_t *n_socket);
#endif /* _NETMSG_H */
