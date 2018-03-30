/* For sigaction */
#define _DARWIN_C_SOURCE 200112L
/* For SIGWINCH on FreeBSD */
#ifndef __BSD_VISIBLE
#define __BSD_VISIBLE 1
#endif

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include "src/net.h"
#include "src/state.h"

#define arg_error(...) \
	do { fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE); } while (0);

static void cleanup(void);
static void main_loop(void);
static void usage(void);
static void signal_sigwinch(int);

static struct termios oterm;
static struct sigaction sa_sigwinch;

static volatile sig_atomic_t flag_sigwinch;

/* Global configuration */
struct config config =
{
	.username = "rirc_v" VERSION,
	.realname = "rirc v" VERSION,
	.join_part_quit_threshold = 100
};

static void
usage(void)
{
	puts(
	"\n"
	"rirc version " VERSION " ~ Richard C. Robbins <mail@rcr.io>\n"
	"\n"
	"Usage:\n"
	"  rirc [-hv] [-s server [-p port] [-w pass] [-n nicks] [-c chans], ...]\n"
	"\n"
	"Help:\n"
	"  -h, --help            Print this message and exit\n"
	"  -v, --version         Print rirc version and exit\n"
	"\n"
	"Options:\n"
	"  -s, --server=SERVER   Connect to SERVER\n"
	"  -p, --port=PORT       Connect to SERVER using PORT\n"
	"  -w, --pass=PASS       Connect to SERVER using PASS\n"
	"  -n, --nicks=NICKS     Comma separated list of nicks to use for SERVER\n"
	"  -c, --chans=CHANNELS  Comma separated list of channels to join for SERVER\n"
	);
}

int
main(int argc, char **argv)
{
	int c, i, opt_i = 0, server_i = -1;

	struct option long_opts[] =
	{
		{"server",  required_argument, 0, 's'},
		{"port",    required_argument, 0, 'p'},
		{"pass",    required_argument, 0, 'w'},
		{"nicks",   required_argument, 0, 'n'},
		{"chans",   required_argument, 0, 'c'},
		{"version", no_argument,       0, 'v'},
		{"help",    no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	struct auto_server {
		char *host;
		char *port;
		char *pass;
		char *nicks;
		char *chans;
		struct server *s;
	} auto_servers[NET_MAX_CONNECTIONS];

	/* FIXME: getopt_long is a GNU extension */
	while ((c = getopt_long(argc, argv, "s:p:w:n:c:vh", long_opts, &opt_i))) {

		if (c == -1)
			break;

		switch (c) {

			/* Connect to server */
			case 's':
				if (*optarg == '-')
					arg_error("-s/--connect requires an argument");

				if (++server_i == NET_MAX_CONNECTIONS)
					arg_error("exceeded maximum number of servers (%d)", NET_MAX_CONNECTIONS);

				auto_servers[server_i].host = optarg;
				auto_servers[server_i].port = "6667",
				auto_servers[server_i].pass = NULL;
				auto_servers[server_i].nicks = NULL;
				auto_servers[server_i].chans = NULL;
				break;

			/* Connect using port */
			case 'p':
				if (*optarg == '-')
					arg_error("-p/--port requires an argument");

				if (server_i < 0)
					arg_error("-p/--port requires a server argument first");

				auto_servers[server_i].port = optarg;
				break;

			/* Connect using port */
			case 'w':
				if (*optarg == '-')
					arg_error("-w/--pass requires an argument");

				if (server_i < 0)
					arg_error("-w/--pass requires a server argument first");

				auto_servers[server_i].pass = optarg;
				break;

			/* Comma separated list of nicks to use */
			case 'n':
				if (*optarg == '-')
					arg_error("-n/--nick requires an argument");

				if (server_i < 0)
					arg_error("-n/--nick requires a server argument first");

				auto_servers[server_i].nicks = optarg;
				break;

			/* Comma separated list of channels to join */
			case 'c':
				if (*optarg == '-')
					arg_error("-c/--chans requires an argument");

				if (server_i < 0)
					arg_error("-c/--chans requires a server argument first");

				auto_servers[server_i].chans = optarg;
				break;

			/* Print rirc version and exit */
			case 'v':
				puts("rirc version " VERSION);
				exit(EXIT_SUCCESS);

			/* Print rirc usage and exit */
			case 'h':
				usage();
				exit(EXIT_SUCCESS);

			default:
				printf("%s --help for usage\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	/* stdout is fflush()'ed on every redraw */
	errno = 0; /* "may set errno" */
	if (setvbuf(stdout, NULL, _IOFBF, 0) != 0)
		fatal("setvbuf", errno);

	/* Set terminal to raw mode */
	if (tcgetattr(0, &oterm) < 0)
		fatal("tcgetattr", errno);

	struct termios nterm = oterm;

	nterm.c_lflag &= ~(ECHO | ICANON | ISIG);
	nterm.c_cc[VMIN]  = 0;
	nterm.c_cc[VTIME] = 0;

	if (tcsetattr(0, TCSADRAIN, &nterm) < 0)
		fatal("tcsetattr", errno);

	srand(time(NULL));

	/* Set up signal handlers */
	sa_sigwinch.sa_handler = signal_sigwinch;
	if (sigaction(SIGWINCH, &sa_sigwinch, NULL) == -1)
		fatal("sigaction - SIGWINCH", errno);

	init_state();

	config.default_nick = getenv("USER");

	for (i = 0; i <= server_i; i++) {

		struct server *s = server(
			auto_servers[i].host,
			auto_servers[i].port,
			auto_servers[i].pass);

		if (s == NULL)
			arg_error("failed to create: %s:%s", auto_servers[i].host, auto_servers[i].port);

		if (server_list_add(state_server_list(), s))
			arg_error("duplicate server: %s:%s", auto_servers[i].host, auto_servers[i].port);

		if (server_set_chans(s, auto_servers[i].chans))
			arg_error("invalid chans: '%s'", auto_servers[i].chans);

		if (server_set_nicks(s, auto_servers[i].nicks))
			arg_error("invalid nicks: '%s'", auto_servers[i].nicks);

		auto_servers[i].s = s;
	}

	for (i = 0; i <= server_i; i++)
		net_cx(auto_servers[i].s->connection);

	/* atexit doesn't set errno */
	if (atexit(cleanup) != 0)
		fatal("atexit", 0);

	main_loop();

	return EXIT_SUCCESS;
}

static void
cleanup(void)
{
	/* Exit handler; must return normally */

	/* Reset terminal colours */
	printf("\x1b[38;0;m");
	printf("\x1b[48;0;m");

#ifndef DEBUG
	/* Clear screen */
	if (!fatal_exit)
		printf("\x1b[H\x1b[J");
#endif

	/* Reset terminal modes */
	tcsetattr(0, TCSADRAIN, &oterm);
}

/* TODO: install sig handlers for cleanly exiting in debug mode */ 
static void
signal_sigwinch(int signum)
{
	UNUSED(signum);

	flag_sigwinch = 1;
}

static void
main_loop(void)
{
	for (;;) {

		/* For each server, check connection status, and input */
		check_servers();

		/* Window has changed size */
		if (flag_sigwinch) {
			flag_sigwinch = 0;
			resize();
		}

		redraw();

		net_poll();
	}
}
