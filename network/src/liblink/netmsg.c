#include <netmsg.h>
#include <pcnporting.h>
#include <arch.h>

pcn_static_assert(sizeof(struct netmsg_re_t) <= L1D_CACHELINE_SIZE,
		  "it is larger than one cache line "
		  "so it can generate two or more PCIe transactions.");

int netmsg_marshal_header(int dryrun, void *buff, uint64_t seqnum,
			  uint32_t size, uint32_t command, uint64_t tag)
{
        struct netmsg_header_t *hdr = buff;
	if (!dryrun) {
		hdr->seqnum = seqnum;
		hdr->size = size;
		hdr->command = command;
		hdr->tag = tag;
	}
	return sizeof(*hdr);
}

int netmsg_unmarshal_header(int dryrun, void *buff, struct netmsg_header_t *hdr)
{
        struct netmsg_header_t *_hdr = buff;
	if (!dryrun) {
		*hdr = *_hdr;
	}
	return sizeof(*hdr);
}

int netmsg_marshal_re(int dryrun, void *buff, uint64_t seqnum, uint32_t size,
		      uint32_t command, uint64_t tag, int32_t rc)
{
	struct netmsg_re_t *re = buff;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum, size, command, tag);
		re->rc = rc;
	}
	return sizeof(*re);
}

int netmsg_unmarshal_re(int dryrun, void *buff, struct netmsg_re_t *re)
{
        struct netmsg_re_t *_re = buff;
	if (!dryrun) {
		*re = *_re;
	}
	return sizeof(*re);
}

int netmsg_marshal_t_socket(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			    int32_t domain, int32_t type, int32_t protocol)
{
	struct netmsg_t_socket_t *t_socket = buff;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum,
				      sizeof(*t_socket),
				      PCN_NETMSG_T_SOCKET, tag);
		t_socket->domain = domain;
		t_socket->type = type;
		t_socket->protocol = protocol;
	}
	return sizeof(*t_socket);
}

int netmsg_unmarshal_t_socket(int dryrun, void *buff, struct netmsg_t_socket_t *t_socket)
{
        struct netmsg_t_socket_t *_t_socket = buff;
	if (!dryrun) {
		*t_socket = *_t_socket;
	}
	return sizeof(*t_socket);
}

int netmsg_marshal_r_socket(int dryrun, void *buff, uint64_t seqnum,
			    uint64_t tag, int32_t rc, uint64_t sockid)
{
	struct netmsg_r_socket_t *r_socket = buff;
	if (!dryrun) {
		netmsg_marshal_re(dryrun, buff, seqnum,
				  sizeof(*r_socket),
				  PCN_NETMSG_R_SOCKET, tag, rc);
		r_socket->sockid = sockid;
	}
	return sizeof(*r_socket);

}

int netmsg_unmarshal_r_socket(int dryrun, void *buff, struct netmsg_r_socket_t *r_socket)
{
        struct netmsg_r_socket_t *_r_socket = buff;
	if (!dryrun) {
		*r_socket = *_r_socket;
	}
	return sizeof(*r_socket);
}

int netmsg_marshal_t_bind(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			  uint64_t sockid, uint32_t addrlen, struct sockaddr_in *addr)
{
	struct netmsg_t_bind_t *t_bind = buff;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum,
				      sizeof(*t_bind),
				      PCN_NETMSG_T_BIND, tag);
		t_bind->sockid = sockid;
		t_bind->addrlen = addrlen;
		t_bind->addr = *addr;
	}
	return sizeof(*t_bind);
}

int netmsg_unmarshal_t_bind(int dryrun, void *buff, struct netmsg_t_bind_t *t_bind)
{
        struct netmsg_t_bind_t *_t_bind = buff;
	if (!dryrun) {
		*t_bind = *_t_bind;
	}
	return sizeof(*t_bind);
}

int netmsg_marshal_r_bind(int dryrun, void *buff, uint64_t seqnum,
			  uint64_t tag, int32_t rc)
{
	return netmsg_marshal_re(dryrun, buff, seqnum,
				 sizeof(struct netmsg_r_bind_t),
				 PCN_NETMSG_R_BIND, tag, rc);
}

int netmsg_unmarshal_r_bind(int dryrun, void *buff, struct netmsg_r_bind_t *r_bind)
{
	return netmsg_unmarshal_re(dryrun, buff, &r_bind->re);
}

int netmsg_marshal_t_connect(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			     uint64_t sockid, uint32_t addrlen, struct sockaddr_in *addr)
{
	struct netmsg_t_connect_t *t_connect = buff;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum,
				      sizeof(*t_connect),
				      PCN_NETMSG_T_CONNECT, tag);
		t_connect->sockid = sockid;
		t_connect->addrlen = addrlen;
		t_connect->addr = *addr;
	}
	return sizeof(*t_connect);
}

int netmsg_unmarshal_t_connect(int dryrun, void *buff, struct netmsg_t_connect_t *t_connect)
{
        struct netmsg_t_connect_t *_t_connect = buff;
	if (!dryrun) {
		*t_connect = *_t_connect;
	}
	return sizeof(*t_connect);
}

int netmsg_marshal_r_connect(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, int32_t rc)
{
	return netmsg_marshal_re(dryrun, buff, seqnum,
				 sizeof(struct netmsg_r_connect_t),
				 PCN_NETMSG_R_CONNECT, tag, rc);
}

int netmsg_unmarshal_r_connect(int dryrun, void *buff, struct netmsg_r_connect_t *r_connect)
{
	return netmsg_unmarshal_re(dryrun, buff, &r_connect->re);
}

int netmsg_marshal_t_listen(int dryrun, void *buff, uint64_t seqnum,
			    uint64_t tag, uint64_t sockid, int32_t backlog)
{
	struct netmsg_t_listen_t *t_listen = buff;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum,
				      sizeof(*t_listen),
				      PCN_NETMSG_T_LISTEN, tag);
		t_listen->sockid = sockid;
		t_listen->backlog = backlog;
	}
	return sizeof(*t_listen);
}

int netmsg_unmarshal_t_listen(int dryrun, void *buff, struct netmsg_t_listen_t *t_listen)
{
        struct netmsg_t_listen_t *_t_listen = buff;
	if (!dryrun) {
		*t_listen = *_t_listen;
	}
	return sizeof(*t_listen);
}

int netmsg_marshal_r_listen(int dryrun, void *buff, uint64_t seqnum,
			    uint64_t tag, int32_t rc)
{
	return netmsg_marshal_re(dryrun, buff, seqnum,
				 sizeof(struct netmsg_r_listen_t),
				 PCN_NETMSG_R_LISTEN, tag, rc);
}

int netmsg_unmarshal_r_listen(int dryrun, void *buff, struct netmsg_r_listen_t *r_listen)
{
	return netmsg_unmarshal_re(dryrun, buff, &r_listen->re);
}

int netmsg_marshal_t_setsockopt(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
				uint64_t sockid, int32_t level, int32_t optname,
				uint32_t optlen, const void *optval)
{
	struct netmsg_t_setsockopt_t *t_setsockopt = buff;
	int size = sizeof(*t_setsockopt) + optlen;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum, size,
				      PCN_NETMSG_T_SETSOCKOPT, tag);
		t_setsockopt->sockid = sockid;
		t_setsockopt->level = level;
		t_setsockopt->optname = optname;
		t_setsockopt->optlen = optlen;
		if (optlen > 0 && optval)
			memcpy(t_setsockopt->optval, optval, optlen);
	}
	return size;
}

int netmsg_unmarshal_t_setsockopt(int dryrun, void *buff,
				  struct netmsg_t_setsockopt_t *t_setsockopt)
{
        struct netmsg_t_setsockopt_t *_t_setsockopt = buff;
	int size = _t_setsockopt->hdr.size;
	if (!dryrun) {
		memcpy(t_setsockopt, _t_setsockopt, size);
	}
	return size;
}

int netmsg_marshal_r_setsockopt(int dryrun, void *buff, uint64_t seqnum,
				uint64_t tag, int32_t rc)
{
	return netmsg_marshal_re(dryrun, buff, seqnum,
				 sizeof(struct netmsg_r_setsockopt_t),
				 PCN_NETMSG_R_SETSOCKOPT, tag, rc);
}

int netmsg_unmarshal_r_setsockopt(int dryrun, void *buff,
				  struct netmsg_r_setsockopt_t *r_setsockopt)
{
	return netmsg_unmarshal_re(dryrun, buff, &r_setsockopt->re);
}

int netmsg_marshal_t_getsockopt(int dryrun, void *buff, uint64_t seqnum,
				uint64_t tag, uint64_t sockid, int32_t level,
				int32_t optname, uint32_t optlen)
{
	struct netmsg_t_getsockopt_t *t_getsockopt = buff;
	int size = sizeof(*t_getsockopt);
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum, size,
				      PCN_NETMSG_T_GETSOCKOPT, tag);
		t_getsockopt->sockid = sockid;
		t_getsockopt->level = level;
		t_getsockopt->optname = optname;
		t_getsockopt->optlen = optlen;
	}
	return size;
}

int netmsg_unmarshal_t_getsockopt(int dryrun, void *buff,
				  struct netmsg_t_getsockopt_t *t_getsockopt)
{
        struct netmsg_t_getsockopt_t *_t_getsockopt = buff;
	if (!dryrun) {
		*t_getsockopt = *_t_getsockopt;
	}
	return sizeof(*t_getsockopt);
}

int netmsg_marshal_r_getsockopt(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
				int32_t rc, uint32_t optlen, void *optval)

{
	struct netmsg_r_getsockopt_t *r_getsockopt = buff;
	int size = sizeof(*r_getsockopt) + optlen;
	if (!dryrun) {
		netmsg_marshal_re(dryrun, buff, seqnum, size,
				  PCN_NETMSG_R_GETSOCKOPT, tag, rc);
		r_getsockopt->optlen = optlen;
		if (optlen > 0 && optval)
			memcpy(r_getsockopt->optval, optval, optlen);
	}
	return size;
}

int netmsg_unmarshal_r_getsockopt(int dryrun, void *buff,
				  struct netmsg_r_getsockopt_t *r_getsockopt)
{
        struct netmsg_r_getsockopt_t *_r_getsockopt = buff;
	int size = _r_getsockopt->re.hdr.size;
	if (!dryrun) {
		memcpy(r_getsockopt, _r_getsockopt, size);
	}
	return size;
}

int netmsg_marshal_t_sendmsg(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			     uint64_t sockid, int32_t flags, struct msghdr *msghdr)
{
	struct netmsg_t_sendmsg_t *t_sendmsg = buff;
	void *msghdr_x = t_sendmsg->msghdr_x;
	int size = sizeof(*t_sendmsg) +
		msghdr->msg_namelen +
		(sizeof(struct iovec) * msghdr->msg_iovlen) +
		msghdr->msg_controllen;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum, size,
				      PCN_NETMSG_T_SENDMSG, tag);
		t_sendmsg->sockid = sockid;
		t_sendmsg->flags  = flags;
		if (msghdr->msg_namelen > 0) {
			memcpy(msghdr_x, msghdr->msg_name, msghdr->msg_namelen);
			msghdr_x += msghdr->msg_namelen;
		}
		if (msghdr->msg_iovlen > 0) {
			memcpy(msghdr_x, msghdr->msg_iov,
			       sizeof(struct iovec) * msghdr->msg_iovlen);
			msghdr_x += sizeof(struct iovec) * msghdr->msg_iovlen;
		}
		if (msghdr->msg_controllen > 0) {
			memcpy(msghdr_x, msghdr->msg_control, msghdr->msg_controllen);
		}
	}
	return size;
}

int netmsg_unmarshal_t_sendmsg(int dryrun, void *buff, struct netmsg_t_sendmsg_t *t_sendmsg)
{
        struct netmsg_t_sendmsg_t *_t_sendmsg = buff;
	int size = _t_sendmsg->hdr.size;
	struct msghdr *msghdr;
	void *msghdr_x;
	if (!dryrun) {
		memcpy(t_sendmsg, _t_sendmsg, size);
		msghdr = &t_sendmsg->msghdr;
		msghdr_x = t_sendmsg->msghdr_x;
		if (msghdr->msg_namelen > 0) {
			msghdr->msg_name = msghdr_x;
			msghdr_x += msghdr->msg_namelen;
		}
		if (msghdr->msg_iovlen > 0) {
			msghdr->msg_iov = msghdr_x;
			msghdr_x += sizeof(struct iovec) * msghdr->msg_iovlen;
		}
		if (msghdr->msg_controllen > 0) {
			msghdr->msg_control = msghdr_x;
		}
	}
	return t_sendmsg->hdr.size;
}

int netmsg_marshal_r_sendmsg(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, int32_t rc)
{
	return netmsg_marshal_re(dryrun, buff, seqnum,
				 sizeof(struct netmsg_r_sendmsg_t),
				 PCN_NETMSG_R_SENDMSG, tag, rc);
}

int netmsg_unmarshal_r_sendmsg(int dryrun, void *buff,
			       struct netmsg_r_sendmsg_t *r_sendmsg)
{
	return netmsg_unmarshal_re(dryrun, buff, &r_sendmsg->re);
}

int netmsg_marshal_t_senddata(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			      uint64_t sockid, uint32_t datalen, const void *data)
{
	struct netmsg_t_senddata_t *t_senddata = buff;
	int size = sizeof(*t_senddata) + datalen;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum, size,
				      PCN_NETMSG_T_SENDDATA, tag);
		t_senddata->sockid = sockid;
		t_senddata->datalen = datalen;
		if (datalen > 0) {
			memcpy(t_senddata->data, data, datalen);
		}
	}
	return size;
}

int netmsg_unmarshal_t_senddata(int dryrun, void *buff, struct netmsg_t_senddata_t *t_senddata)
{
        struct netmsg_t_senddata_t *_t_senddata = buff;
	int size = _t_senddata->hdr.size;
	if (!dryrun) {
		memcpy(t_senddata, _t_senddata, size);
	}
	return size;
}

int netmsg_marshal_r_senddata(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, int32_t rc)
{
	return netmsg_marshal_re(dryrun, buff, seqnum,
				 sizeof(struct netmsg_r_senddata_t),
				 PCN_NETMSG_R_SENDDATA, tag, rc);
}

int netmsg_unmarshal_r_senddata(int dryrun, void *buff,
			       struct netmsg_r_senddata_t *r_senddata)
{
	return netmsg_unmarshal_re(dryrun, buff, &r_senddata->re);
}

int netmsg_marshal_t_shutdown(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			      uint64_t sockid, int32_t how)
{
	struct netmsg_t_shutdown_t *t_shutdown = buff;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum,
				      sizeof(*t_shutdown),
				      PCN_NETMSG_T_SHUTDOWN, tag);
		t_shutdown->sockid = sockid;
		t_shutdown->how = how;
	}
	return sizeof(*t_shutdown);
}

int netmsg_unmarshal_t_shutdown(int dryrun, void *buff, struct netmsg_t_shutdown_t *t_shutdown)
{
        struct netmsg_t_shutdown_t *_t_shutdown = buff;
	if (!dryrun) {
		*t_shutdown = *_t_shutdown;
	}
	return sizeof(*t_shutdown);
}

int netmsg_marshal_r_shutdown(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, int32_t rc)
{
	return netmsg_marshal_re(dryrun, buff, seqnum,
				 sizeof(struct netmsg_r_shutdown_t),
				 PCN_NETMSG_R_SHUTDOWN, tag, rc);
}

int netmsg_unmarshal_r_shutdown(int dryrun, void *buff, struct netmsg_r_shutdown_t *r_shutdown)
{
	return netmsg_unmarshal_re(dryrun, buff, &r_shutdown->re);
}

int netmsg_marshal_t_close(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, uint64_t sockid)
{
	struct netmsg_t_close_t *t_close = buff;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum,
				      sizeof(*t_close),
				      PCN_NETMSG_T_CLOSE, tag);
		t_close->sockid = sockid;
	}
	return sizeof(*t_close);
}

int netmsg_unmarshal_t_close(int dryrun, void *buff, struct netmsg_t_close_t *t_close)
{
        struct netmsg_t_close_t *_t_close = buff;
	if (!dryrun) {
		*t_close = *_t_close;
	}
	return sizeof(*t_close);
}

int netmsg_marshal_r_close(int dryrun, void *buff, uint64_t seqnum, uint64_t tag, int32_t rc)
{
	return netmsg_marshal_re(dryrun, buff, seqnum,
				 sizeof(struct netmsg_r_close_t),
				 PCN_NETMSG_R_CLOSE, tag, rc);
}

int netmsg_unmarshal_r_close(int dryrun, void *buff, struct netmsg_r_close_t *r_close)
{
	return netmsg_unmarshal_re(dryrun, buff, &r_close->re);
}

int netmsg_marshal_e_accept(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			    uint64_t sockid, uint32_t addrlen, struct sockaddr_in *addr)
{
	struct netmsg_e_accept_t *e_accept = buff;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum,
				      sizeof(*e_accept),
				      PCN_NETMSG_E_ACCEPT, tag);
		e_accept->sockid = sockid;
		e_accept->addrlen = addrlen;
		e_accept->addr = *addr;
	}
	return sizeof(*e_accept);
}

int netmsg_unmarshal_e_accept(int dryrun, void *buff, struct netmsg_e_accept_t *e_accept)
{
        struct netmsg_e_accept_t *_e_accept = buff;
	if (!dryrun) {
		*e_accept = *_e_accept;
	}
	return sizeof(*e_accept);
}

int netmsg_marshal_e_recvdata(int dryrun, void *buff, uint64_t seqnum, uint64_t tag,
			      uint64_t sockid, uint32_t datalen, void *data)
{
	struct netmsg_e_recvdata_t *e_recvdata = buff;
	int size = sizeof(*e_recvdata) + datalen;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum, size,
				      PCN_NETMSG_E_RECVDATA, tag);
		e_recvdata->sockid = sockid;
		e_recvdata->datalen= datalen;
		if (data && datalen > 0) {
			memcpy(e_recvdata->data, data, datalen);
		}
	}
	return size;
}

int netmsg_unmarshal_e_recvdata(int dryrun, void *buff, struct netmsg_e_recvdata_t *e_recvdata)
{
        struct netmsg_e_recvdata_t *_e_recvdata = buff;
	int size = _e_recvdata->hdr.size;
	if (!dryrun) {
		memcpy(e_recvdata, _e_recvdata, size);
	}
	return size;
}

int netmsg_marshal_n_socket(int dryrun, void *buff, uint64_t seqnum,
			    uint64_t tag, uint64_t sockid)
{
	struct netmsg_n_socket_t *n_socket = buff;
	if (!dryrun) {
		netmsg_marshal_header(dryrun, buff, seqnum,
				      sizeof(*n_socket),
				      PCN_NETMSG_N_SOCKET, tag);
		n_socket->sockid = sockid;
	}
	return sizeof(*n_socket);
}

int netmsg_unmarshal_n_socket(int dryrun, void *buff, struct netmsg_n_socket_t *n_socket)
{
        struct netmsg_n_socket_t *_n_socket = buff;
	if (!dryrun) {
		*n_socket = *_n_socket;
	}
	return sizeof(*n_socket);
}
