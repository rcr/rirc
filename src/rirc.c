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

#include "src/net2.h"
#include "src/state.h"

#define opt_error(MESG) \
	do { puts((MESG)); exit(EXIT_FAILURE); } while (0);

static void cleanup(void);
static void startup(int, char**);
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

int
main(int argc, char **argv)
{
	startup(argc, argv);

	main_loop();

	return EXIT_SUCCESS;
}

static void
usage(void)
{
	puts(
	"\n"
	"rirc version " VERSION " ~ Richard C. Robbins <mail@rcr.io>\n"
	"\n"
	"Usage:\n"
	"  rirc [-c server [OPTIONS]]*\n"
	"\n"
	"Help:\n"
	"  -h, --help             Print this message\n"
	"\n"
	"Options:\n"
	"  -c, --connect=SERVER   Connect to SERVER\n"
	"  -p, --port=PORT        Connect using PORT\n"
	"  -j, --join=CHANNELS    Comma separated list of channels to join\n"
	"  -n, --nicks=NICKS      Comma and/or space separated list of nicks to use\n"
	"  -v, --version          Print rirc version and exit\n"
	"\n"
	"Examples:\n"
	"  rirc -c server -j '#chan'\n"
	"  rirc -c server -j '#chan' -c server2 -j '#chan2'\n"
	"  rirc -c server -p 1234 -j '#chan1,#chan2' -n 'nick, nick_, nick__'\n"
	);
}

static void
startup(int argc, char **argv)
{
	int c, i, opt_i = 0, server_i = -1;

	struct option long_opts[] =
	{
		{"connect", required_argument, 0, 'c'},
		{"port",    required_argument, 0, 'p'},
		{"join",    required_argument, 0, 'j'},
		{"nick",    required_argument, 0, 'n'},
		{"version", no_argument,       0, 'v'},
		{"help",    no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	struct auto_server {
		char *host;
		char *port;
		char *join;
		char *nicks;
	} auto_servers[MAX_SERVERS] = {{0, 0, 0, 0}};

	while ((c = getopt_long(argc, argv, "c:p:n:j:vh", long_opts, &opt_i))) {

		if (c == -1)
			break;

		switch (c) {

			/* Connect to server */
			case 'c':
				if (*optarg == '-')
					opt_error("-c/--connect requires an argument");

				if (++server_i == MAX_SERVERS)
					opt_error("exceeded maximum number of servers (" STR(MAX_SERVERS) ")");

				auto_servers[server_i].host = optarg;
				break;

			/* Connect using port */
			case 'p':
				if (*optarg == '-')
					opt_error("-p/--port requires an argument");

				if (server_i < 0)
					opt_error("-p/--port requires a server argument first");

				auto_servers[server_i].port = optarg;
				break;

			/* Comma and/or space separated list of nicks to use */
			case 'n':
				if (*optarg == '-')
					opt_error("-n/--nick requires an argument");

				if (server_i < 0)
					opt_error("-n/--nick requires a server argument first");

				auto_servers[server_i].nicks = optarg;
				break;

			/* Comma separated list of channels to join */
			case 'j':
				if (*optarg == '-')
					opt_error("-j/--join requires an argument");

				if (server_i < 0)
					opt_error("-j/--join requires a server argument first");

				auto_servers[server_i].join = optarg;
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

	/* atexit doesn't set errno */
	if (atexit(cleanup) != 0)
		fatal("atexit", 0);

	init_state();

	config.default_nick = getenv("USER");

	for (i = 0; i <= server_i; i++) {

		//TODO: - split server.c / net.c
		//      - add servers to server list
		//      - add channels per server to server's channel list
		//      - initiate connection
		server_connect(
			auto_servers[i].host,
			auto_servers[i].port ? auto_servers[i].port : "6667",
			auto_servers[i].nicks,
			auto_servers[i].join
		);
	}
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
