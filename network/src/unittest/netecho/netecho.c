#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <pcnporting.h>
#include <pcnlink-api.h>
#include <pcnlink-socket.h> /* PORTING HOWTO #1/2:
			     * include <pcnlink-socket.h>
			     * instead of <sys/socket.h>
			     * at the end of #include list */

/* #define SUPPORT_USER_PCNLINK */

#define FINGER_PRINT_START   'a'
#define FINGER_PRINT_GARBAGE '@'
#define MAX_ITER              3
#define IO_SIZE_MUL           2
#define IO_SIZE_INIT          2048
#define MAX_BUFF_SIZE         (128 * 1024)
static char buff[MAX_BUFF_SIZE];

enum {
	TRANS_TCP = 0,
	TRANS_PCN,
};

enum {
	MODE_SERVER = 0,
	MODE_CLIENT,
};

enum {
	BLOCK = 0,
	NON_BLOCK,
};

struct cmd_opt_t {
	int trans;
	int mode;
	char *ip;
	int port;
	int non_block;
};

static int parse_option(int argc, char* argv[], struct cmd_opt_t *opt)
{
	static struct option options[] = {
		{"help",  no_argument,       0, 'h'},
		{"trans", required_argument, 0, 't'},
		{"mode",  required_argument, 0, 'm'},
		{"ip",    required_argument, 0, 'i'},
		{"port",  required_argument, 0, 'p'},
		{"non-block",  required_argument, 0, 'n'},
		{0, 0, 0, 0},
	};
	int arg_cnt;

	for (arg_cnt = 0; 1; ++arg_cnt) {
		int c, len, unit, idx;
		c = getopt_long(argc, argv, "h:t:m:i:p:n", options, &idx);
		if (c == -1)
			break;

		switch (c) {
		case 't':
			if (strcmp(optarg, "tcp") == 0)
				opt->trans = TRANS_TCP;
#ifdef SUPPORT_USER_PCNLINK
			else if (strcmp(optarg, "pcn") == 0)
				opt->trans = TRANS_PCN;
#endif /* SUPPORT_USER_PCNLINK */

			else
				return -EINVAL;
			break;
		case 'm':
			if (strcmp(optarg, "server") == 0)
				opt->mode = MODE_SERVER;
			else if (strcmp(optarg, "client") == 0)
				opt->mode = MODE_CLIENT;
			else
				return -EINVAL;
			break;
		case 'i':
			opt->ip = strdup(optarg);
			break;
		case 'p':
			opt->port = strtol(optarg, NULL, 10);
			break;
		case 'n':
			opt->non_block = NON_BLOCK;
			break;
		default:
			return -EINVAL;
		}
	}
	return arg_cnt;
}

static
void __wipe_out_mem(void *p, size_t len)
{
	memset(p, FINGER_PRINT_GARBAGE, len);
}

void run_tcp_server(struct cmd_opt_t *opt)
{
	int server_sock, client_sock;
	struct sockaddr_in server_addr, client_addr;
	int rc, received, sent, io_size = IO_SIZE_INIT;
	int i, c, iter;

	__wipe_out_mem(buff, sizeof(buff));
	pcn_test_exit(1, "========= START TCP SERVER =========");

	/* create the TCP socket */
	/* PORTING HOWTO #2/2:
	 * replace all PF_INET to PF_PCNLINK */
	server_sock = socket(PF_PCNLINK, SOCK_STREAM, IPPROTO_TCP);
	pcn_test_exit(server_sock >= 0, "socket: sock: %d errno: %d",
		      server_sock, errno);

	/* construct the server sockaddr_in structure */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(opt->port);

	/* bind the server socket */
	rc = bind(server_sock, (struct sockaddr *) &server_addr,
		  sizeof(server_addr));
	pcn_test_exit(rc >= 0, "bind: rc: %d errno: %d", rc, errno);

	/* listen on the server socket */
	rc = listen(server_sock, 64);
	pcn_test_exit(rc >= 0, "listen: rc: %d errno: %d", rc, errno);

	/* run until cancelled */
	do {
		unsigned int client_len = sizeof(client_addr);

		/* wait for client connection */
		client_sock = accept(server_sock,
				     (struct sockaddr *)&client_addr,
				     &client_len);
		pcn_test_exit(client_sock >= 0, "accept: sock: %d errno: %d",
			      client_sock, errno);

		/* perform io */
		for (c = FINGER_PRINT_START, iter = 0;
		     iter < MAX_ITER && io_size < sizeof(buff);
		     io_size *= IO_SIZE_MUL, ++c, ++iter) {
			/* recv */
			received = recv(client_sock, buff, io_size, 0);
			pcn_test_exit(received >= 0, "recv: %d", received);

			/* check conents */
			for (i = 0; i < min(io_size, received); ++i) {
				if (buff[i] != c) {
					pcn_test_exit(0,
						      "recv: buff[%d](%d) != c(%d)",
						      i, buff[i], c);
				}
			}
			/* send */
			sent = send(client_sock, buff, received, 0);
			pcn_test_exit(sent == received,
				 "sent(%d) == received(%d)", sent, received);
		}
		pcn_test_exit(1, "==== end of IO");

		/* client socket */
		close(client_sock);
		pcn_test_exit(1, "==== close socket");
	} while (0);
}

static int
make_socket_non_blocking (int sfd)
{
	int flags, s;

	flags = fcntl (sfd, F_GETFL, 0);
	pcn_test_exit(flags >= 0, "fcntl error %d", errno);

	flags |= O_NONBLOCK;
	s = fcntl (sfd, F_SETFL, flags);
	pcn_test_exit(s >= 0, "fcntl error %d", errno);

	return 0;
}

void run_tcp_server_epoll(struct cmd_opt_t *opt)
{
        int server_sock, client_sock;
        struct sockaddr_in server_addr, client_addr;
        int rc, io_size = IO_SIZE_INIT;
	int received, sent;
        int i, c, n, efd, s;
	struct epoll_event event;
	struct epoll_event *events;

	__wipe_out_mem(buff, sizeof(buff));
        pcn_test_exit(1, "========= START TCP SERVER EPOLL =========");

        /* create the TCP socket */
        server_sock = socket(PF_PCNLINK, SOCK_STREAM, IPPROTO_TCP);
        pcn_test_exit(server_sock >= 0, "socket: sock: %d errno: %d",
                      server_sock, errno);

        /* construct the server sockaddr_in structure */
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(opt->port);

	make_socket_non_blocking(server_sock);
        /* bind the server socket */
        rc = bind(server_sock, (struct sockaddr *) &server_addr,
                  sizeof(server_addr));
        pcn_test_exit(rc >= 0, "bind: rc: %d errno: %d", rc, errno);

        /* listen on the server socket */
        rc = listen(server_sock, 64);
        pcn_test_exit(rc >= 0, "listen: rc: %d errno: %d", rc, errno);

	efd = epoll_create1 (0);
        pcn_test_exit(efd >= 0, "epoll: errno: %d", errno);

	event.data.fd = server_sock;
	event.events = EPOLLIN;
	s = epoll_ctl (efd, EPOLL_CTL_ADD, server_sock, &event);
        pcn_test_exit(s >= 0, "epoll: errno: %d", errno);

	/* Buffer where events are returned */
	events = calloc(64, sizeof(event));

	/* The event loop */
	while (1) {
		static int count = 0; /* XXX */

		n = epoll_wait (efd, events, 64, 0);

		/* XXX */
		if (count > 1000000) {
			pcn_dbg("epoll n = %d\n", n);
			count = 0;
		}
		count++;
		/* XXX */

		for (i = 0; i < n; i++) {
			pcn_dbg("epoll got event = %d\n", n);
			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				(!(events[i].events & EPOLLIN))) {
              			/* An error has occured on this fd, or the socket is not
                 		   ready for reading (why were we notified then?) */
	      			close (events[i].data.fd);
	      			continue;
	    		} else if (server_sock == events[i].data.fd) {
              			/* We have a notification on the listening socket, which
                 		means one or more incoming connections. */
              			while (1)
                		{
	                  		struct sockaddr in_addr;
                  			socklen_t in_len;
					int infd;

                  			in_len = sizeof(in_addr);
					pcn_dbg("before accept: %d\n", 0);
                  			infd = accept (server_sock, &in_addr, &in_len);
					pcn_dbg("after %d accept\n", infd);

                  			if (infd == -1)
                    			{
                      				if ((errno == EAGAIN) ||
                          			   (errno == EWOULDBLOCK))
                        			{
                          				/* We have processed all incoming
                             				connections. */
                          				break;
                        			}
                      				else
                          				break;
                    			}

	                  		/* Make the incoming socket non-blocking and add it to the
        	             		list of fds to monitor. */
                	 		s = make_socket_non_blocking (infd);
        				pcn_test_exit(s >= 0, "epoll: errno: %d", errno);

                  			event.data.fd = infd;
                  			event.events = EPOLLIN;
                  			s = epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event);
                    		}
              			continue;
            		} else {
              			/* We have data on the fd waiting to be read. Read and
                 		display it. We must read whatever data is available
                 		completely, as we are running in edge-triggered mode
                 		and won't get a notification again for the same
                 		data. */
              			int done = 0;

                  		received = recv(events[i].data.fd, buff, MAX_BUFF_SIZE, 0);
                  		if (received == -1) {
                      			/* If errno == EAGAIN, that means we have read all
                         		 data. So go back to the main loop. */
                      			if (errno != EAGAIN) {
                          			done = 1;
                        		}
                    		} else if (received == 0) {
                      			/* End of file. The remote has closed the
                         		connection. */
                      			done = 1;
                    		}

              			if (done)
                		{
                  			/* Closing the descriptor will make epoll remove it
                     			from the set of descriptors which are monitored. */
                  			close(events[i].data.fd);
					goto done;
                		}

                  		/* Write the buffer to standard output */
                  		sent = send(events[i].data.fd, buff, received, 0);
				pcn_test_exit(received == sent,
					      "sent(%d) == received(%d)", sent, received);

				io_size *= IO_SIZE_MUL;
				if (io_size >= sizeof(buff)) {
					pcn_test_exit(1, "==== end of IO");

					/* client socket */
					close(events[i].data.fd);
					pcn_test_exit(1, "==== close socket");
					goto done;
				}
            		}
        	}
	}

done:
  	free(events);
  	close(server_sock);
}

void run_tcp_client(struct cmd_opt_t *opt)
{
	int client_sock;
	struct sockaddr_in client_addr;
	int rc, received, sent, io_size = IO_SIZE_INIT;
	int i, c, iter;

	__wipe_out_mem(buff, sizeof(buff));
	pcn_test_exit(1, "========= START TCP CLIENT =========");

	/* create the TCP socket */
	client_sock = socket(PF_PCNLINK, SOCK_STREAM, IPPROTO_TCP);
	pcn_test_exit(client_sock >= 0, "socket: sock: %d errno: %d",
		      client_sock, errno);

	/* construct the server sockaddr_in structure */
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = inet_addr(opt->ip);
	client_addr.sin_port = htons(opt->port);

	/* connect */
	rc = connect(client_sock, (struct sockaddr *) &client_addr,
		     sizeof(client_addr));
	pcn_test_exit(rc >= 0, "connect: rc: %d errno: %d", rc, errno);

	/* run until cancelled */
	do {
		unsigned int client_len = sizeof(client_addr);

		/* perform io */
		for (c = FINGER_PRINT_START, iter = 0;
		     iter < MAX_ITER && io_size < sizeof(buff);
		     io_size *= IO_SIZE_MUL, ++c, ++iter) {
			/* set contents */
			for (i = 0; i < io_size; ++i) {
				buff[i] = c;
			}

			/* send */
			sent = send(client_sock, buff, io_size, 0);
			pcn_test_exit(sent >= 0, "sent: %d", sent);

			/* recv */
			received = recv(client_sock, buff, io_size, 0);
			pcn_test_exit(sent == received,
				 "sent(%d) == received(%d)", sent, received);

			/* check conents */
			for (i = 0; i < min(io_size, received); ++i) {
				if (buff[i] != c) {
					pcn_test_exit(0,
						      "recv: buff[%d](%d) != c(%d)",
						      i, buff[i], c);
				}
			}
		}
		pcn_test_exit(1, "==== end of IO");

		/* client socket */
		close(client_sock);
		pcn_test_exit(1, "==== close socket");
	} while(0);
}

#ifdef SUPPORT_USER_PCNLINK
void run_pcn_server(struct cmd_opt_t *opt)
{
	int server_sock, client_sock;
	struct sockaddr_in server_addr, client_addr;
	int rc, received, sent, io_size = IO_SIZE_INIT;
	int i, c, iter;

	__wipe_out_mem(buff, sizeof(buff));
	pcn_test_exit(1, "========= START PCN SERVER =========");

	/* create the TCP socket */
	server_sock = pcnlink_u_socket(PF_PCNLINK, SOCK_STREAM, IPPROTO_TCP);
	pcn_test_exit(server_sock >= 0, "socket: sock: %d errno: %d",
		      server_sock, errno);

	/* construct the server sockaddr_in structure */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(opt->port);

	/* bind the server socket */
	rc = pcnlink_u_bind(server_sock, (struct sockaddr *) &server_addr,
		  sizeof(server_addr));
	pcn_test_exit(rc >= 0, "bind: rc: %d errno: %d", rc, errno);

	/* listen on the server socket */
	rc = pcnlink_u_listen(server_sock, 64);
	pcn_test_exit(rc >= 0, "listen: rc: %d errno: %d", rc, errno);

	/* run until cancelled */
	do {
		unsigned int client_len = sizeof(client_addr);

		/* wait for client connection */
		client_sock = pcnlink_u_accept(server_sock,
					       (struct sockaddr *)&client_addr,
					       &client_len);
		pcn_test_exit(client_sock >= 0, "accept: sock: %d errno: %d",
			      client_sock, errno);

		/* perform io */
		for (c = FINGER_PRINT_START, iter = 0;
		     iter < MAX_ITER && io_size < sizeof(buff);
		     io_size *= IO_SIZE_MUL, ++c, ++iter) {
			/* recv */
			received = pcnlink_u_recv(client_sock, buff, io_size, 0);
			pcn_test_exit(received >= 0, "recv: %d", received);

			/* check conents */
			for (i = 0; i < min(io_size, received); ++i) {
				if (buff[i] != c) {
					pcn_test_exit(0,
						      "recv: buff[%d](%d) != c(%d)",
						      i, buff[i], c);
				}
			}
			/* send */
			sent = pcnlink_u_send(client_sock, buff, received, 0);
			pcn_test_exit(sent == received,
				 "sent(%d) == received(%d)", sent, received);
		}

		/* client socket */
		pcnlink_u_close(client_sock);
	} while (0);
}

void run_pcn_server_epoll(struct cmd_opt_t *opt)
{
        int server_sock, client_sock;
        struct sockaddr_in server_addr, client_addr;
        int rc, received, sent, io_size = IO_SIZE_INIT;
        int i, c, n, done;
	int efd, s;
	struct pcnlink_epoll_event event;
	struct pcnlink_epoll_event *events;

	__wipe_out_mem(buff, sizeof(buff));
        pcn_test_exit(1, "========= START TCP SERVER EPOLL =========");

        /* create the TCP socket */
	server_sock = pcnlink_u_socket(PF_PCNLINK, SOCK_STREAM, IPPROTO_TCP);
        pcn_test_exit(server_sock >= 0, "socket: sock: %d errno: %d",
                      server_sock, errno);

        /* construct the server sockaddr_in structure */
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(opt->port);

	//make_socket_non_blocking(server_sock);
        /* bind the server socket */
        rc = pcnlink_u_bind(server_sock, (struct sockaddr *) &server_addr,
                  	    sizeof(server_addr));
        pcn_test_exit(rc >= 0, "bind: rc: %d errno: %d", rc, errno);

        /* listen on the server socket */
        rc = pcnlink_u_listen(server_sock, 64);
        pcn_test_exit(rc >= 0, "listen: rc: %d errno: %d", rc, errno);

	efd = pcnlink_epoll_create(64);
        pcn_test_exit(efd >= 0, "epoll: errno: %d", errno);

	event.data.sockid = server_sock;
	event.events = EPOLLIN;
	s = pcnlink_epoll_ctl(efd, EPOLL_CTL_ADD, server_sock, &event);
        pcn_test_exit(s >= 0, "epoll: errno: %d", errno);

	/* Buffer where events are returned */
	events = calloc(64, sizeof(event));

	/* The event loop */
	while (1) {

		n = pcnlink_epoll_wait(efd, events, 64, -1);
		for (i = 0; i < n; i++) {

			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				(!(events[i].events & EPOLLIN))) {
              			/* An error has occured on this fd, or the socket is not
                 		   ready for reading (why were we notified then?) */
	      			pcnlink_u_close(events[i].data.sockid);
	      			continue;
	    		} else if (server_sock == events[i].data.sockid) {
              			/* We have a notification on the listening socket, which
                 		means one or more incoming connections. */
	                  	struct sockaddr in_addr;
                  		       socklen_t in_len;
				int infd;

                  		in_len = sizeof(in_addr);
                  		infd = pcnlink_u_accept (server_sock, &in_addr, &in_len);
                  		if (infd == -1)
                    		{
                      			if ((errno == EAGAIN) ||
                          		   (errno == EWOULDBLOCK))
                        		{
                          			/* We have processed all incoming
                             			connections. */
                          			break;
                        		}
                      			else
                          			break;
                    		}

	                  	/* Make the incoming socket non-blocking and add it to the
        	             	list of fds to monitor. */
                	 	//s = make_socket_non_blocking (infd);
        			//pcn_test_exit(s == -1, "epoll: errno: %d", errno);

                  		event.data.sockid = infd;
                  		event.events = EPOLLIN | EPOLLET;
                  		s = pcnlink_epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event);
        			pcn_test_exit(s >= 0, "epoll: errno: %d", errno);
              			continue;
            		} else {
              			/* We have data on the fd waiting to be read. Read and
                 		display it. We must read whatever data is available
                 		completely, as we are running in edge-triggered mode
                 		and won't get a notification again for the same
                 		data. */
				client_sock = events[i].data.sockid;

                  		ssize_t count;
				count = pcnlink_u_recv(client_sock, buff, io_size, 0);
        			pcn_test_exit(count >= 0, "epoll: errno: %d", errno);
					
                    		if (count == 0) {
                      			/* End of file. The remote has closed the
                         		connection. */
                      			done = 1;
                    		}

                  		/* Write the buffer to standard output */
				sent = pcnlink_u_send(client_sock, buff, count, 0);
        			pcn_test_exit(s >= 0, "epoll: errno: %d", errno);
				io_size = io_size * 2;

				// stop with buffsize
				if (io_size > sizeof(buff)/2)
					done = 1;

              			if (done)
                		{
                  			/* Closing the descriptor will make epoll remove it
                     			from the set of descriptors which are monitored. */
                  			pcnlink_u_close(client_sock);
					break;
                		}

            		}
        	}
		if (done)
			break;
	}
  	free(events);
  	pcnlink_u_close(server_sock);
	return;
}

void run_pcn_client(struct cmd_opt_t *opt)
{
	int client_sock;
	struct sockaddr_in client_addr;
	int rc, received, sent, io_size = IO_SIZE_INIT;
	int i, c, iter;

	__wipe_out_mem(buff, sizeof(buff));
	pcn_test_exit(1, "========= START PCN CLIENT =========");

	/* create the TCP socket */
	client_sock = pcnlink_u_socket(PF_PCNLINK, SOCK_STREAM, IPPROTO_TCP);
	pcn_test_exit(client_sock >= 0, "socket: sock: %d errno: %d",
		      client_sock, errno);

	/* construct the server sockaddr_in structure */
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = inet_addr(opt->ip);
	client_addr.sin_port = opt->port;

	/* connect */
	rc = pcnlink_u_connect(client_sock, (struct sockaddr *) &client_addr,
			       sizeof(client_addr));
	pcn_test_exit(rc >= 0, "connect: rc: %d errno: %d", rc, errno);

	/* run until cancelled */
	do {
		unsigned int client_len = sizeof(client_addr);

		/* perform io */
		for (c = FINGER_PRINT_START, iter = 0;
		     iter < MAX_ITER && io_size < sizeof(buff);
		     io_size *= IO_SIZE_MUL, ++c, ++iter) {
			/* set contents */
			for (i = 0; i < io_size; ++i) {
				buff[i] = c;
			}

			/* send */
			sent = pcnlink_u_send(client_sock, buff, io_size, 0);
			pcn_test_exit(sent >= 0, "sent: %d", sent);

			/* recv */
			received = pcnlink_u_recv(client_sock, buff, io_size, 0);
			pcn_test_exit(sent == received,
				 "sent(%d) == received(%d)", sent, received);

			/* check conents */
			for (i = 0; i < min(io_size, received); ++i) {
				if (buff[i] != c) {
					pcn_test_exit(0,
						      "recv: buff[%d](%d) != c(%d)",
						      i, buff[i], c);
				}
			}
		}

		/* client socket */
		pcnlink_u_close(client_sock);
	} while(0);
}
#endif /* SUPPORT_USER_PCNLINK */

static void usage(FILE* out) {
	extern const char* __progname;
	fprintf(out, "Usage: %s\n", __progname);
	fprintf(out, "  --help  = show this message\n");

#ifdef SUPPORT_USER_PCNLINK
	fprintf(out, "  --trans = {tcp | pcn}\n");
#else
	fprintf(out, "  --trans = tcp\n");
#endif /* SUPPORT_USER_PCNLINK */
	fprintf(out, "  --mode  = {server | client}\n");
	fprintf(out, "  --ip    = [0.0.0.0]\n");
	fprintf(out, "  --port  = [port number]\n");
	fprintf(out, "  --n     = for non-blocking mode\n");
}

int main(int argc, char *argv[])
{
	struct cmd_opt_t opt = {
		.trans = TRANS_TCP,
		.mode  = MODE_SERVER,
		.ip    = "127.0.0.1",
		.port  = 9999,
	};
	int rc;

	/* parse command line options */
	rc = parse_option(argc, argv, &opt);
	if (rc < 0) {
		usage(stderr);
		goto err_out;
	}

	/* run */
	if (opt.trans == TRANS_TCP) {
		if (opt.mode == MODE_SERVER) {
			if (opt.non_block)
				run_tcp_server_epoll(&opt);
			else
				run_tcp_server(&opt);
		}
		else if (opt.mode == MODE_CLIENT) {
			run_tcp_client(&opt);
		}
	}
#ifdef SUPPORT_USER_PCNLINK
	else if (opt.trans == TRANS_PCN) {
		rc = pcnlink_up(PCNLINK_PCNSRV_ID,
				PCNLINK_PCNSRV_PORT,
				0,
				PCNLINK_PORT,
				PCNLINK_PER_CHN_TX_Q_SIZE);
		pcn_test_exit(rc == 0, "pcnlink_up: %d", rc);

		if (opt.mode == MODE_SERVER) {
			if (opt.non_block)
				run_pcn_server_epoll(&opt);
			else
				run_pcn_server(&opt);
		}
		else if (opt.mode == MODE_CLIENT) {
			run_pcn_client(&opt);
		}

		pcnlink_down();
	}
#endif /* SUPPORT_USER_PCNLINK */

	return 0;
err_out:
	return rc;
}
