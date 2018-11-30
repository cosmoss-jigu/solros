#include <linux/capability.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/net.h>
#include <net/sock.h>
#include <net/tcp_states.h>
#include <pcnlink-api.h>

#define CREATE_TRACE_POINTS
#include <trace/events/pcnlink.h>

static int server_id   __read_mostly = PCNLINK_PCNSRV_ID;
static int server_port __read_mostly = PCNLINK_PCNSRV_PORT;
static int client_id   __read_mostly = PCNLINK_ID;
static int client_port __read_mostly = PCNLINK_PORT;
static int qsize       __read_mostly = PCNLINK_PER_CHN_TX_Q_SIZE;

static struct proto pcnlink_proto;
static const struct proto_ops pcnlink_ops;

static inline struct pcnlink_socket_t *
plsock_sk(struct sock *sk)
{
        return container_of(sk, struct pcnlink_socket_t, sk);
}

static int
pcnlink_release(struct socket *sock)
{
        struct sock *sk = sock->sk;
	int rc = 0;

	pcn_here();

	/* sanity check */
	if (!sk)
		return 0;

	/* close plsock */
	rc = __pcnlink_close( plsock_sk(sk));
	if (rc)
		goto err_out;

	sock_orphan(sk);
	sock->sk = NULL;
	release_sock(sk);

	/* purge queues */

	/* clean up resource */
	sock_put(sk);
	return 0;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	return rc;
}

static int
pcnlink_bind(struct socket *sock, struct sockaddr *uaddr,
	     int addr_len)
{
        int rc = 0;

	pcn_here();

	/* check if sockaddr is sane */
        if (addr_len < sizeof(struct sockaddr_in)) {
		rc = -EINVAL;
                goto err_out;
	}

	/* bind plsock */
	rc = __pcnlink_bind( plsock_sk(sock->sk),
			     uaddr, addr_len);
	if (rc)
		goto err_out;
	return 0;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	return rc;
}

static int
pcnlink_connect(struct socket *sock, struct sockaddr *addr,
		int alen, int flags)
{
        int rc = 0;

	pcn_here();

	/* check if sockaddr is sane */
        if (alen < sizeof(struct sockaddr_in)) {
		rc = -EINVAL;
                goto err_out;
	}

	/* connect plsock */
	rc = __pcnlink_connect( plsock_sk(sock->sk),
				addr, alen);
	if (rc)
		goto err_out;
	return 0;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	return rc;
}

static void
pcnlink_sock_destruct(struct sock *sk)
{
	pcn_here();
	plsock_deinit( plsock_sk(sk));
}

static void
pcnlink_write_space(struct sock *sk)
{
	struct pcnlink_socket_t *plsock = plsock_sk(sk);
	struct socket_wq *wq;

	rcu_read_lock();
	if (__pcnlink_writable(plsock)) {
		wq = rcu_dereference(sk->sk_wq);
		if (wq_has_sleeper(wq))
			wake_up_interruptible_sync_poll(&wq->wait,
				POLLOUT | POLLWRNORM | POLLWRBAND);
		sk_wake_async(sk, SOCK_WAKE_SPACE, POLL_OUT);
	}
	rcu_read_unlock();
}


static void
pcnlink_data_ready(struct sock *sk, int __dummy)
{
	struct socket_wq *wq;

	rcu_read_lock();
	wq = rcu_dereference(sk->sk_wq);
	if (wq_has_sleeper(wq))
		wake_up_interruptible_sync_poll(&wq->wait, POLLIN | POLLPRI |
						POLLRDNORM | POLLRDBAND);
	sk_wake_async(sk, SOCK_WAKE_WAITD, POLL_IN);
	rcu_read_unlock();
}

void plsock_data_ready(struct pcnlink_socket_t *plsock)
{
	struct sock *sk = &plsock->sk;
	sk->sk_data_ready(sk, -1);
}

struct pcnlink_socket_t *
plsock_alloc(void *__net, void *__sock)
{
	struct net *net = __net;
	struct socket *sock = __sock;
	struct sock *sk;
	struct pcnlink_socket_t *plsock;

	/* alloc */
	sk = sk_alloc(net, PF_PCNLINK, GFP_KERNEL, &pcnlink_proto);
	if (!sk)
		goto err_out;
	plsock = plsock_sk(sk);

	/* link with sock */
        sock_init_data(sock, sk);

	/* install destructor */
	sk->sk_destruct     = pcnlink_sock_destruct;

	/* install data availability notification
	 * callbacks for epoll() and select() */
	sk->sk_write_space  = pcnlink_write_space;
 	sk->sk_data_ready   = pcnlink_data_ready;

	pcn_dbg("NEW SOCKET: (sock, %p) (sk, %p) (plsock, %p)\n",
		sock, sk, plsock);
	return plsock;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] fail to alloc\n",
		__func__, __LINE__);
	return NULL;
}

static int
pcnlink_accept(struct socket *sock, struct socket *newsock, int flags)
{
        struct sock *sk = sock->sk;
	struct pcnlink_socket_t *newpls;
	struct sock *newsk;
        int rc;

	pcn_here();

	/* accept plsock so a newpls is created */
	newpls = __pcnlink_accept_ex( plsock_sk(sock->sk),
				      NULL, /* we don't need to get addr */
				      NULL, /* we don't need to get alen */
				      &rc, /* get rc of accept_ex() */
				      sk->sk_net, /* inherit net from sock */
				      newsock); /* it is about newsock */
	if (rc || !newpls)
		goto err_out;

	/* init sock and sk */
	newsk = newsock->sk;
	if (newsk == NULL) {
		rc = -EIO;
		goto err_out;
	}
	return 0;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	return rc;
}


static int
pcnlink_getname(struct socket *sock, struct sockaddr *uaddr,
		int *uaddr_len, int peer)
{
        struct pcnlink_socket_t *plsock  = plsock_sk(sock->sk);
        DECLARE_SOCKADDR(struct sockaddr_in *, sin, uaddr);

	pcn_here();
	*sin = plsock->sin;
        *uaddr_len = sizeof(*sin);
        return 0;
}

static unsigned int
pcnlink_poll(struct file *file, struct socket *sock, poll_table *wait)
{
	struct sock *sk = sock->sk;

	pcn_here();

	sock_poll_wait(file, sk_sleep(sk), wait);
	return __pcnlink_poll( plsock_sk(sock->sk));
}

static int
pcnlink_listen(struct socket *sock, int backlog)
{
	int rc;
	pcn_here();
	rc = __pcnlink_listen( plsock_sk(sock->sk), backlog);
	if (rc)
		goto err_out;
	return 0;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	return rc;
}

static int
pcnlink_shutdown(struct socket *sock, int how)
{
	int rc;

	pcn_here();
	rc = __pcnlink_shutdown( plsock_sk(sock->sk), how);
	if (rc)
		goto err_out;
	return 0;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	return rc;
}

static int
pcnlink_setsockopt(struct socket *sock, int level, int optname,
		   char __user *optval, unsigned int optlen)
{
        struct pcnlink_socket_t *plsock = plsock_sk(sock->sk);
	int rc;

	pcn_here();
	rc = __pcnlink_setsockopt(plsock, level, optname, optval, optlen);
	if (rc)
		goto err_out;
	return 0;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	return rc;
}

static int
pcnlink_getsockopt(struct socket *sock, int level, int optname,
		   char __user *optval, int __user *optlen)
{
        struct pcnlink_socket_t *plsock = plsock_sk(sock->sk);
	int rc;

	pcn_here();
	rc = __pcnlink_getsockopt(plsock, level, optname, optval, optlen);
	if (rc)
		goto err_out;
	return 0;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	return rc;
}

static int
pcnlink_sendmsg(struct kiocb *kiocb, struct socket *sock,
		struct msghdr *msg, size_t len)
{
        struct pcnlink_socket_t *plsock = plsock_sk(sock->sk);
	int rc;

	pcn_dbg("[PCNLINK:%s:%d] << len = %d\n", __func__, __LINE__, (int)len);
	trace_pcnlink_sendmsg_begin(sock, msg, len);
	rc = __pcnlink_sendmsg(plsock, msg, len);
	if (rc < 0)
		goto err_out;
	trace_pcnlink_sendmsg_end(sock, msg, len);
	pcn_dbg("[PCNLINK:%s:%d] >> len = %d   rc = %d\n", __func__, __LINE__, (int)len, (int)rc);
	return rc;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	return rc;
}

static int
pcnlink_recvmsg(struct kiocb *kiocb, struct socket *sock,
		struct msghdr *msg, size_t len,
		int flags)
{
        struct pcnlink_socket_t *plsock = plsock_sk(sock->sk);
	int rc;

	pcn_dbg("[PCNLINK:%s:%d] << len = %d\n", __func__, __LINE__, (int)len);
	trace_pcnlink_recvmsg_begin(sock, msg, len);
	rc = __pcnlink_recvmsg(plsock, msg, len);
	if (rc < 0)
		goto err_out;
	pcn_dbg("[PCNLINK:%s:%d] >> len = %d   rc = %d\n", __func__, __LINE__, (int)len, (int)rc);
	trace_pcnlink_recvmsg_end(sock, msg, len);
	return rc;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	return rc;
}

static int
pcnlink_create(struct net *net, struct socket *sock, int protocol,
	       int kern)
{
        struct sock *sk;
	struct pcnlink_socket_t *plsock;
	int rc = 0;

	pcn_here();

	/* check for valid socket type */
        if (sock->type != SOCK_STREAM) {
		rc = -ESOCKTNOSUPPORT;
		goto err_out;
	}

	/* create a plsock */
	plsock = __pcnlink_socket_ex(PF_INET, /* pcnlink is a proxy of inet */
				     sock->type, protocol,
				     &rc, net, sock);
	if (rc || !plsock)
		goto err_out;

	/* init */
        sock->state     = SS_UNCONNECTED;
	sock->ops       = &pcnlink_ops;
	sk->sk_family   = PF_PCNLINK;
	sk->sk_protocol = protocol;
	return 0;
err_out:
	pcn_dbg("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	return rc;
}

static struct proto pcnlink_proto = {
        .name     = "PCNLINK",
        .owner    = THIS_MODULE,
        .obj_size = sizeof(struct pcnlink_socket_t),
};

static const struct proto_ops pcnlink_ops = {
        .family =       PF_NETLINK,
        .owner =        THIS_MODULE,
        .release =      pcnlink_release,
        .bind =         pcnlink_bind,
        .connect =      pcnlink_connect,
        .socketpair =   sock_no_socketpair,
        .accept =       pcnlink_accept,
        .getname =      pcnlink_getname,
        .poll =         pcnlink_poll,
        .ioctl =        sock_no_ioctl,
        .listen =       pcnlink_listen,
        .shutdown =     pcnlink_shutdown,
        .setsockopt =   pcnlink_setsockopt,
        .getsockopt =   pcnlink_getsockopt,
        .sendmsg =      pcnlink_sendmsg,
        .recvmsg =      pcnlink_recvmsg,
        .mmap =         sock_no_mmap,
        .sendpage =     sock_no_sendpage,
};

static const struct net_proto_family pcnlink_family_ops = {
        .family = PF_PCNLINK,
        .create = pcnlink_create,
        .owner  = THIS_MODULE,
};

static int
__init pcnlink_init(void)
{
	int rc = 0;

	pcn_here();

	/* registers protos and sock */
	rc = proto_register(&pcnlink_proto, 1);
	if (rc)
		goto err_out;

	sock_register(&pcnlink_family_ops);

        /* connect to the server */
	rc = pcnlink_up(server_id, server_port,
			client_id, client_port, qsize);
	if (rc)
		goto err_out;

       	return 0;
err_out:
	pcn_err("[PCNLINK:%s:%d] error return: rc = %d\n",
		__func__, __LINE__, rc);
	pcnlink_down();
	return rc;
}

static void
__exit pcnlink_exit(void)
{
	pcnlink_down();
}

module_init(pcnlink_init);
module_exit(pcnlink_exit);

MODULE_AUTHOR("Changwoo Min <changwoo@gatech.edu>");
MODULE_DESCRIPTION("socket link layer for PCN (PCIe Cloud Network)");
MODULE_LICENSE("GPL");

module_param(server_id,   int, 0);
MODULE_PARM_DESC(server_id, "Host server node id");
module_param(server_port, int, 0);
MODULE_PARM_DESC(server_port, "Host server port number");
module_param(client_id, int, 0);
MODULE_PARM_DESC(client_id, "MIC client node id");
module_param(client_port, int, 0);
MODULE_PARM_DESC(client_port, "MIC client port number");
module_param(qsize, int, 0);
MODULE_PARM_DESC(client_port, "Per-channel queue size");


/**
 * TODO XXX
 * ??? AF_INET, PF_PCNLINK ???
 */
