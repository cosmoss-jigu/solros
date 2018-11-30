#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <proxy.h>
#include <pcnconfig.h>

struct cmd_opt_t {
	int port;
	int ncpu;
	int qsize;
};

static int parse_option(int argc, char* argv[], struct cmd_opt_t *opt)
{
	static struct option options[] = {
		{"help",  no_argument,       0, 'h'},
		{"port",  required_argument, 0, 'p'},
		{"ncpu",  required_argument, 0, 'n'},
		{"qsize", required_argument, 0, 'z'},
		{0, 0, 0, 0},
	};
	int arg_cnt;

	for (arg_cnt = 0; 1; ++arg_cnt) {
		int c, len, unit, idx;
		c = getopt_long(argc, argv, "hp:n:z:", options, &idx);
		if (c == -1)
			break;

		switch (c) {
		case 'p':
			opt->port = strtol(optarg, NULL, 10);
			break;
		case 'n':
			opt->ncpu = strtol(optarg, NULL, 10);
			break;
		case 'z':
                        len = strlen(optarg);
                        unit = 1;
                        if (optarg[len - 1] == 'K') {
                                unit = 1024;
                                optarg[len - 1] = '\0';
                        }
                        else if (optarg[len - 1] == 'M') {
                                unit = 1024 * 1024;
                                optarg[len - 1] = '\0';
                        }
                        else if (optarg[len - 1] == 'G') {
                                unit = 1024 * 1024 * 1024;
                                optarg[len - 1] = '\0';
                        }
                        opt->qsize = strtod(optarg, NULL) * unit;
			break;
		default:
			return -EINVAL;
		}
	}
	return arg_cnt;
}

static void usage(FILE* out) {
	extern const char* __progname;
	fprintf(out, "Usage: %s\n", __progname);
	fprintf(out, "  --help  = show this message\n");
	fprintf(out, "  --port  = port number (%d)\n",
		PCN_PROXY_DEFAULT_PORT);
	fprintf(out, "  --ncpu  = number of CPUs (%d)\n",
		PCN_PROXY_DEFAULT_NCPU);
	fprintf(out, "  --qsize = RX queue size (%d)\n",
		PCN_PROXY_DEFAULT_RX_Q_SIZE);
}

int main(int argc, char *argv[])
{
	struct cmd_opt_t opt = {
		.port  = PCN_PROXY_DEFAULT_PORT,
		.ncpu  = PCN_PROXY_DEFAULT_NCPU,
		.qsize = PCN_PROXY_DEFAULT_RX_Q_SIZE,
	};
	struct pcn_proxy_t proxy;
	int rc;

	/* parse command line options */
	rc = parse_option(argc, argv, &opt);
	if (rc < 0) {
		usage(stderr);
		goto err_out;
	}

	/* init proxy */
	rc = pcn_init_proxy(opt.port, opt.ncpu,
			    opt.port + 1, opt.qsize,
			    &proxy);
	if (rc) {
		fprintf(stderr,
			"Fail to create a proxy with "
			"(port, %d), (ncpu, %d), and (qsize, %d): "
			"rc %d\n",
			opt.port, opt.ncpu, opt.qsize, rc);
		goto err_out;
	}
	fprintf(stdout,
		"Created a proxy with "
		"(port, %d), (ncpu, %d), and (qsize, %d)\n",
		opt.port, opt.ncpu, opt.qsize);

	/* wait for connections */
	fprintf(stdout, "Wait for connections...\n");
	do {
		rc = pcn_proxy_accept_link(&proxy);
		if (rc == 0) {
			fprintf(stdout,
				"A new link is connected\n");
		}
		else {
			fprintf(stderr,
				"Fail to establish a link: %d\n", rc);
		}
	} while (rc == 0);

	/* clean up */
	pcn_deinit_proxy(&proxy);
	return 0;
err_out:
	return rc;
}
