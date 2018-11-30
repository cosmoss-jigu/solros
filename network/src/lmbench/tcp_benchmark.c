/*
 * lat_tcp.c - simple TCP transaction latency test
 *
 * Three programs in one -
 *	server usage:	tcp_xact -s
 *	client usage:	tcp_xact [-m <message size>] [-P <parallelism>] [-W <warmup>] [-N <repetitions>] hostname
 *	shutdown:	tcp_xact -S hostname
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"

typedef struct _state {
	int	msize;
	int	sock;
	char	*server;
	char	*buf;
} state_t;

int	pps_test;

void	init(iter_t iterations, void* cookie);
void	cleanup(iter_t iterations, void* cookie);
void	doclient(iter_t iterations, void* cookie);
void	server_main();
void	doserver(int sock);

int
main(int ac, char **av)
{
	state_t state;
	int	parallel = 1;
	int	warmup = 0;
	int	repetitions = -1;
	int 	c;
	char	buf[256];
	char	*usage = "-s\n OR [-m <message size>] [-T] [-P <parallelism>] [-W <warmup>] [-N <repetitions>] server\n OR -S server\n";

	state.msize = 1;

	while (( c = getopt(ac, av, "sS:m:P:W:N:T")) != EOF) {
		switch(c) {
		case 's': /* Server */
			if (fork() == 0) {
				server_main();
			}
			exit(0);
		case 'S': /* shutdown serverhost */
			state.sock = tcp_connect(optarg,
						 TCP_XACT,
						 SOCKOPT_NONE);
			close(state.sock);
			fprintf(stderr, "Shutdown server done\n");
			exit(0);
		case 'm':
			state.msize = atoi(optarg);
			break;
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0)
				lmbench_usage(ac, av, usage);
			break;
		case 'W':
			warmup = atoi(optarg);
			break;
		case 'N':
			repetitions = atoi(optarg);
			break;
		case 'T': /* PPS test */ 
			pps_test = 0;
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}

	if (optind != ac - 1) {
		lmbench_usage(ac, av, usage);
	}

	state.server = av[optind];
	benchmp(init, doclient, cleanup, MEDIUM, parallel, 
		warmup, repetitions, &state);

	if (!pps_test) {
		sprintf(buf, "TCP latency using %s", state.server);
		micro(buf, get_n());
	}

	exit(0);
}

void
init(iter_t iterations, void* cookie)
{
	state_t *state = (state_t *) cookie;
	int	msize  = htonl(state->msize), rc;

	if (iterations) return;

	state->sock = tcp_connect(state->server, TCP_XACT, SOCKOPT_NONE);
	state->buf = malloc(state->msize);
	if (!state->buf) {
		perror("malloc");
		exit(1);
	}

	rc = write(state->sock, &msize, sizeof(int));
}

void
cleanup(iter_t iterations, void* cookie)
{
	state_t *state = (state_t *) cookie;

	if (iterations) return;

	close(state->sock);
	free(state->buf);
}

void
doclient(iter_t iterations, void* cookie)
{
	state_t *state = (state_t *) cookie;
	int 	sock   = state->sock, rc;

	while (iterations-- > 0) {
		rc = write(sock, state->buf, state->msize);
		if (!pps_test) {
			rc = read(sock, state->buf, state->msize);
		}
	}
}

void
server_main()
{
	int     newsock, sock;

	GO_AWAY;
	signal(SIGCHLD, sigchld_wait_handler);
	sock = tcp_server(TCP_XACT, SOCKOPT_REUSE);
	for (;;) {
		newsock = tcp_accept(sock, SOCKOPT_NONE);
		switch (fork()) {
		    case -1:
			perror("fork");
			break;
		    case 0:
			doserver(newsock);
			exit(0);
		    default:
			close(newsock);
			break;
		}
	}
	/* NOTREACHED */
}

void
doserver(int sock)
{
	int	n;

	if (read(sock, &n, sizeof(int)) == sizeof(int)) {
		int	msize = ntohl(n), rc;
		char*   buf = (char*)malloc(msize);

		if (!buf) {
			close(sock);
			perror("malloc");
			exit(4);
		}
		for (n = 0; read(sock, buf, msize) > 0; n++) {
			if (!pps_test) {
				rc = write(sock, buf, msize);
			}
		}
		close(sock);
		free(buf);
	} else {
		/*
		 * A connection with no data means shut down.
		 */
		close(sock);
		tcp_done(TCP_XACT);
		kill(getppid(), SIGTERM);
		exit(0);
	}
}
