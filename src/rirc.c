/* For sigaction */
#define _POSIX_C_SOURCE 200112L
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

#include "common.h"
#include "state.h"

static void cleanup(void);
static void configure(void);
static void getopts(int, char**);
static void main_loop(void);
static void startup(void);
static void usage(void);
static void signal_sigwinch(int);

static struct sigaction sa_sigwinch;
static volatile sig_atomic_t flag_sigwinch;

/* Values parsed from getopts */
static struct
{
	char *connect;
	char *port;
	char *join;
	char *nicks;
} opts;

static struct termios oterm, nterm;

int
main(int argc, char **argv)
{
	getopts(argc, argv);
	configure();
	startup();
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
	"  rirc [-c server [OPTIONS]]\n"
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
	"  rirc -c server.tld -j '#chan'\n"
	"  rirc -c server.tld -p 1234 -j '#chan1,#chan2' -n 'nick, nick_, nick__'\n"
	);
}

static void
getopts(int argc, char **argv)
{
	opts.connect = NULL;
	opts.port    = NULL;
	opts.join    = NULL;
	opts.nicks   = NULL;

	int c, opt_i = 0;

	static struct option long_opts[] =
	{
		{"connect", required_argument, 0, 'c'},
		{"port",    required_argument, 0, 'p'},
		{"join",    required_argument, 0, 'j'},
		{"nick",    required_argument, 0, 'n'},
		{"version", no_argument,       0, 'v'},
		{"help",    no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, "c:p:n:j:vh", long_opts, &opt_i))) {

		if (c == -1)
			break;

		switch(c) {

			/* Connect to server */
			case 'c':
				if (*optarg == '-') {
					puts("-c/--connect requires an argument");
					exit(EXIT_FAILURE);
				}
				opts.connect = optarg;
				break;

			/* Connect using port */
			case 'p':
				if (*optarg == '-') {
					puts("-p/--port requires an argument");
					exit(EXIT_FAILURE);
				}
				opts.port = optarg;
				break;

			/* Comma and/or space separated list of nicks to use */
			case 'n':
				if (*optarg == '-') {
					puts("-n/--nick requires an argument");
					exit(EXIT_FAILURE);
				}
				opts.nicks = optarg;
				break;

			/* Comma separated list of channels to join */
			case 'j':
				if (*optarg == '-') {
					puts("-j/--join requires an argument");
					exit(EXIT_FAILURE);
				}
				opts.join = optarg;
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
}

static void
configure(void)
{
	/* TODO: this should create a series of server objects, and begin a connection on them
	 * things like autojoin for these servers will be a one-time property of the server
	 * instead of an awkward global config. in this sense, it will be required for a -c
	 * configuration to be present before any -j/-p/-n etc */
	/* Build a linked list of server objects, and delete the linked list as they're sent
	 * off for connection */

	if (opts.connect) {
		config.auto_connect = opts.connect;
		config.auto_port = opts.port ? opts.port : "6667";
		config.auto_join = opts.join;
		config.nicks = opts.nicks ? opts.nicks : getenv("USER");
	} else {
		config.auto_connect = NULL;
		config.auto_port = NULL;
		config.auto_join = NULL;
		config.nicks = getenv("USER");
	}

	//FIXME: these would become global_config as oppose to each s.config
	//options in this struct can be /set, or :set

	config.username = "rirc_v" VERSION;
	config.realname = "rirc v" VERSION;
	config.join_part_quit_threshold = 100;
}

static void
signal_sigwinch(int signum)
{
	UNUSED(signum);

	flag_sigwinch = 1;
}

static void
startup(void)
{
	/* stdout is fflush()'ed on every redraw */
	setvbuf(stdout, NULL, _IOFBF, 0);

	/* Set terminal to raw mode */
	tcgetattr(0, &oterm);
	nterm = oterm;
	nterm.c_lflag &= ~(ECHO | ICANON | ISIG);
	nterm.c_cc[VMIN] = 1;
	nterm.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSADRAIN, &nterm) < 0)
		fatal("tcsetattr");

	srand(time(NULL));

	/* Initialize submodules */
	init_mesg();
	init_state();

	/* Set up signal handlers */
	sa_sigwinch.sa_handler = signal_sigwinch;
	if (sigaction(SIGWINCH, &sa_sigwinch, NULL) == -1)
		fatal("sigaction - SIGWINCH");

	/* Register cleanup() for exit() */
	atexit(cleanup);

	/* FIXME: if the initial connect fails the autojoin channels
	 * will be used by whatever server next connects */
	/* TODO: for multiple server on cli, it will be a linked list
	 * and for each, we will send a server autoconnect and then free it */
	if (config.auto_connect)
		server_connect(config.auto_connect, config.auto_port);
}

static void
cleanup(void)
{
	/* Reset terminal modes */
	tcsetattr(0, TCSADRAIN, &oterm);

	/* Free submodules */
	free_mesg();
	free_state();

	/* Reset terminal colours */
	printf("\x1b[38;0;m");
	printf("\x1b[48;0;m");

#ifndef DEBUG
	/* Clear screen */
	printf("\x1b[H\x1b[J");
#endif
}

static void
main_loop(void)
{
	for (;;) {

		/* Check for input on stdin, sleep 200ms */
		poll_input();

		/* For each server, check connection status, and input */
		check_servers();

		/* Window has changed size */
		if (flag_sigwinch)
			flag_sigwinch = 0, draw(D_RESIZE);

		/* Redraw the ui (skipped if nothing has changed) */
		redraw(ccur);
	}
}
