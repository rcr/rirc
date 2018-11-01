#include <errno.h>
#include <getopt.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "src/io.h"
#include "src/state.h"

#define arg_error(...) \
	do { fprintf(stderr, "%s: ", runtime_name); \
	     fprintf(stderr, __VA_ARGS__); \
	     fprintf(stderr, "\n"); \
	     fprintf(stderr, "%s --help for usage\n", runtime_name); \
	     exit(EXIT_FAILURE); \
	} while (0)

static const char* opt_arg_str(char);
static const char* getpwuid_pw_name(void);
static void parse_args(int, char**);

#ifndef DEBUG
const char *runtime_name = "rirc";
#else
const char *runtime_name = "rirc.debug";
#endif

#ifndef DEFAULT_NICK_SET
const char *default_nick_set = DEFAULT_NICK_SET;
#else
const char *default_nick_set;
#endif

#ifndef DEFAULT_USERNAME
const char *default_username = DEFAULT_USERNAME;
#else
const char *default_username;
#endif

#ifndef DEFAULT_REALNAME
const char *default_realname = DEFAULT_REALNAME;
#else
const char *default_realname;
#endif

static const char *const rirc_usage =
"\nrirc v"VERSION" ~ Richard C. Robbins <mail@rcr.io>"
"\n"
"\nUsage:"
"\n  rirc [-hv] [-s server [-p port] [-w pass] [-n nicks] [-c chans] [-u user] [-r real]], ...]"
"\n"
"\nHelp:"
"\n  -h, --help            Print this message and exit"
"\n  -v, --version         Print rirc version and exit"
"\n"
"\nOptions:"
"\n  -s, --server=SERVER      Connect to SERVER"
"\n  -p, --port=PORT          Connect to SERVER using PORT"
"\n  -w, --pass=PASS          Connect to SERVER using PASS"
"\n  -u, --username=USERNAME  Connect to SERVER using USERNAME"
"\n  -r, --realname=REALNAME  Connect to SERVER using REALNAME"
"\n  -n, --nicks=NICKS        Comma separated list of nicks to use for SERVER"
"\n  -c, --chans=CHANNELS     Comma separated list of channels to join for SERVER"
"\n";

static const char *const rirc_version =
#ifdef DEBUG
"rirc v"VERSION" (debug build)";
#else
"rirc v"VERSION;
#endif

static const char*
opt_arg_str(char c)
{
	switch (c) {
		case 's': return "-s/--server";
		case 'p': return "-p/--port";
		case 'w': return "-w/--pass";
		case 'n': return "-n/--nicks";
		case 'c': return "-c/--chans";
		case 'u': return "-u/--username";
		case 'r': return "-r/--realname";
		default:
			fatal("unknown option flag '%c'", c);
	}
	return NULL;
}

static const char*
getpwuid_pw_name(void)
{
	static struct passwd *passwd;

	errno = 0;

	if (!passwd && !(passwd = getpwuid(geteuid())))
		fatal("getpwuid: %s", strerror((errno ? errno : ENOENT)));

	return passwd->pw_name;
}

static void
parse_args(int argc, char **argv)
{
	int opt_c = 0,
	    opt_i = 0;

	size_t n_servers = 0;

	if (argc > 0)
		runtime_name = argv[0];

	srand(time(NULL));

	opterr = 0;

	struct option long_opts[] = {
		{"server",   required_argument, 0, 's'},
		{"port",     required_argument, 0, 'p'},
		{"pass",     required_argument, 0, 'w'},
		{"nicks",    required_argument, 0, 'n'},
		{"chans",    required_argument, 0, 'c'},
		{"username", required_argument, 0, 'u'},
		{"realname", required_argument, 0, 'r'},
		{"version",  no_argument,       0, 'v'},
		{"help",     no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	struct cli_server {
		const char *host;
		const char *port;
		const char *pass;
		const char *nicks;
		const char *chans;
		const char *username;
		const char *realname;
		struct server *s;
	} cli_servers[IO_MAX_CONNECTIONS];

	/* FIXME: getopt_long is a GNU extension */
	while (0 < (opt_c = getopt_long(argc, argv, ":s:p:w:n:c:r:u:vh", long_opts, &opt_i))) {

		switch (opt_c) {

			case 's': /* Connect to server */

				if (*optarg == '-')
					arg_error("-s/--server requires an argument");

				if (++n_servers == IO_MAX_CONNECTIONS)
					arg_error("exceeded maximum number of servers (%d)", IO_MAX_CONNECTIONS);

				cli_servers[n_servers - 1].host = optarg;
				cli_servers[n_servers - 1].port = "6667";
				cli_servers[n_servers - 1].pass = NULL;
				cli_servers[n_servers - 1].nicks = NULL;
				cli_servers[n_servers - 1].chans = NULL;
				cli_servers[n_servers - 1].username = NULL;
				cli_servers[n_servers - 1].realname = NULL;
				break;

			#define CHECK_SERVER_OPTARG(OPT_C) \
				if (*optarg == '-') \
					arg_error("option '%s' requires an argument", opt_arg_str((OPT_C))); \
				if (n_servers == 0) \
					arg_error("option '%s' requires a server argument first", opt_arg_str((OPT_C)));

			case 'p': /* Connect using port */
				CHECK_SERVER_OPTARG(opt_c);
				cli_servers[n_servers - 1].port = optarg;
				break;

			case 'w': /* Connect using port */
				CHECK_SERVER_OPTARG(opt_c);
				cli_servers[n_servers - 1].pass = optarg;
				break;

			case 'n': /* Comma separated list of nicks to use */
				CHECK_SERVER_OPTARG(opt_c);
				cli_servers[n_servers - 1].nicks = optarg;
				break;

			case 'c': /* Comma separated list of channels to join */
				CHECK_SERVER_OPTARG(opt_c);
				cli_servers[n_servers - 1].chans = optarg;
				break;

			case 'u': /* Connect using username */
				CHECK_SERVER_OPTARG(opt_c);
				cli_servers[n_servers - 1].username = optarg;
				break;

			case 'r': /* Connect using realname */
				CHECK_SERVER_OPTARG(opt_c);
				cli_servers[n_servers - 1].realname = optarg;
				break;

			#undef CHECK_SERVER_OPTARG

			case 'v':
				puts(rirc_version);
				exit(EXIT_SUCCESS);

			case 'h':
				puts(rirc_usage);
				exit(EXIT_SUCCESS);

			case '?':
				arg_error("unknown options '%s'", argv[optind - 1]);

			case ':':
				arg_error("option '%s' requires an argument", opt_arg_str(optopt));

			default:
				arg_error("unknown opt error");
		}
	}

	if (optind < argc)
		arg_error("unused option '%s'", argv[optind]);

	if (!default_nick_set || !default_nick_set[0])
		default_nick_set = getpwuid_pw_name();

	if (!default_username || !default_username[0])
		default_username = getpwuid_pw_name();

	if (!default_realname || !default_realname[0])
		default_realname = getpwuid_pw_name();

	state_init();

	for (size_t i = 0; i < n_servers; i++) {

		struct server *s = server(
			cli_servers[i].host,
			cli_servers[i].port,
			cli_servers[i].pass,
			(cli_servers[i].username ? cli_servers[i].username : default_username),
			(cli_servers[i].realname ? cli_servers[i].realname : default_realname)
		);

		if (s == NULL)
			arg_error("failed to create: %s:%s", cli_servers[i].host, cli_servers[i].port);

		if (server_list_add(state_server_list(), s))
			arg_error("duplicate server: %s:%s", cli_servers[i].host, cli_servers[i].port);

		if (cli_servers[i].chans && state_server_set_chans(s, cli_servers[i].chans))
			arg_error("invalid chans: '%s'", cli_servers[i].chans);

		if (server_set_nicks(s, (cli_servers[i].nicks ? cli_servers[i].nicks : default_nick_set)))
			arg_error("invalid nicks: '%s'", cli_servers[i].nicks);

		cli_servers[i].s = s;
	}

	for (size_t i = 0; i < n_servers; i++)
		io_cx(cli_servers[i].s->connection);
}

int
main(int argc, char **argv)
{
	parse_args(argc, argv);
	io_loop();
}
