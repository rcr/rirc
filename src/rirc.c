#include <getopt.h>
#include <pwd.h>
#include <unistd.h>

#include "config.h"
#include "src/io.h"
#include "src/state.h"

#define arg_error(...) \
	do { fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE); } while (0);

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

static const char* getpwuid_pw_name(struct passwd *passwd);
static void rirc_usage(void);

static const char*
getpwuid_pw_name(struct passwd *passwd)
{
	if (!(passwd = getpwuid(geteuid())))
		fatal("getpwuid", (errno ? errno : ENOENT));

	return passwd->pw_name;
}

static void
rirc_usage(void)
{
	puts(
	"\n"
	"rirc version "VERSION" ~ Richard C. Robbins <mail@rcr.io>\n"
	"\n"
	"Usage:\n"
	"  rirc [-hv] [-s server [-p port] [-w pass] [-n nicks] [-c chans] [-u user] [-r real]], ...]\n"
	"\n"
	"Help:\n"
	"  -h, --help            Print this message and exit\n"
	"  -v, --version         Print rirc version and exit\n"
	"\n"
	"Options:\n"
	"  -s, --server=SERVER      Connect to SERVER\n"
	"  -p, --port=PORT          Connect to SERVER using PORT\n"
	"  -w, --pass=PASS          Connect to SERVER using PASS\n"
	"  -u, --username=USERNAME  Connect to SERVER using USERNAME\n"
	"  -r, --realname=REALNAME  Connect to SERVER using REALNAME\n"
	"  -n, --nicks=NICKS        Comma separated list of nicks to use for SERVER\n"
	"  -c, --chans=CHANNELS     Comma separated list of channels to join for SERVER\n"
	);
}

int
main(int argc, char **argv)
{
	srand(time(NULL));

	int opt_c = 0,
	    opt_i = 0;

	size_t n_servers = 0;

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
	while (0 < (opt_c = getopt_long(argc, argv, "s:p:w:n:c:r:u:vh", long_opts, &opt_i))) {

		switch (opt_c) {

			case 's': /* Connect to server */

				if (*optarg == '-')
					arg_error("-s/--connect requires an argument");

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

			#define CHECK_SERVER_OPTARG(STR) \
				if (*optarg == '-') \
					arg_error(STR " requires an argument"); \
				if (n_servers == 0) \
					arg_error(STR " requires a server argument first");

			case 'p': /* Connect using port */
				CHECK_SERVER_OPTARG("-p/--port");
				cli_servers[n_servers - 1].port = optarg;
				break;

			case 'w': /* Connect using port */
				CHECK_SERVER_OPTARG("-w/--pass");
				cli_servers[n_servers - 1].pass = optarg;
				break;

			case 'n': /* Comma separated list of nicks to use */
				CHECK_SERVER_OPTARG("-n/--nick");
				cli_servers[n_servers - 1].nicks = optarg;
				break;

			case 'c': /* Comma separated list of channels to join */
				CHECK_SERVER_OPTARG("-c/--chans");
				cli_servers[n_servers - 1].chans = optarg;
				break;

			case 'u': /* Connect using username */
				CHECK_SERVER_OPTARG("-u/--username");
				cli_servers[n_servers - 1].username = optarg;
				break;

			case 'r': /* Connect using realname */
				CHECK_SERVER_OPTARG("-r/--realname");
				cli_servers[n_servers - 1].realname = optarg;
				break;

			#undef CHECK_SERVER_OPTARG

			case 'v': /* Print rirc version and exit */
				puts("rirc version " VERSION);
				exit(EXIT_SUCCESS);

			case 'h': /* Print rirc usage and exit */
				rirc_usage();
				exit(EXIT_SUCCESS);

			default:
				printf("%s --help for usage\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	// FIXME:
	init_state();

	struct passwd passwd;

	if (!default_nick_set || !default_nick_set[0])
		default_nick_set = getpwuid_pw_name(&passwd);

	if (!default_username || !default_username[0])
		default_username = getpwuid_pw_name(&passwd);

	if (!default_realname || !default_realname[0])
		default_realname = getpwuid_pw_name(&passwd);

	for (size_t i = 0; i < n_servers; i++) {

		if (cli_servers[i].nicks == NULL)
			cli_servers[i].nicks = default_nick_set;

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

		if (server_set_chans(s, cli_servers[i].chans))
			arg_error("invalid chans: '%s'", cli_servers[i].chans);

		if (server_set_nicks(s, cli_servers[i].nicks))
			arg_error("invalid nicks: '%s'", cli_servers[i].nicks);

		cli_servers[i].s = s;
	}

	for (size_t i = 0; i < n_servers; i++)
		io_cx(cli_servers[i].s->connection);

	io_loop();
}
