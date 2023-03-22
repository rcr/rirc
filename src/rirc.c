#include "src/rirc.h"

#include "config.h"
#include "src/draw.h"
#include "src/io.h"
#include "src/state.h"

#include <errno.h>
#include <getopt.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define MAX_CLI_SERVERS 64

#define arg_error(...) \
	do { fprintf(stderr, "%s ", runtime_name); \
	     fprintf(stderr, __VA_ARGS__); \
	     fprintf(stderr, "\n%s --help for usage\n", runtime_name); \
	} while (0)

static const char* rirc_opt_str(char);
static const char* rirc_pw_name(void);
static int rirc_parse_args(int, char**);

#ifdef CA_CERT_FILE
const char *default_ca_file = CA_CERT_FILE;
#else
const char *default_ca_file;
#endif

#ifdef CA_CERT_PATH
const char *default_ca_path = CA_CERT_PATH;
#else
const char *default_ca_path;
#endif

#ifdef DEFAULT_NICKS
const char *default_nicks = DEFAULT_NICKS;
#else
const char *default_nicks;
#endif

#ifdef DEFAULT_USERNAME
const char *default_username = DEFAULT_USERNAME;
#else
const char *default_username;
#endif

#ifdef DEFAULT_REALNAME
const char *default_realname = DEFAULT_REALNAME;
#else
const char *default_realname;
#endif

#ifndef NDEBUG
const char *runtime_name = "rirc.debug";
#else
const char *runtime_name = "rirc";
#endif

static const char *const rirc_help =
"\nrirc v" STR(VERSION) " ~ Richard C. Robbins <mail@rcr.io>"
"\n"
"\nUsage:"
"\n  rirc [-hv] [-s host [options] ...]"
"\n"
"\nInfo:"
"\n  -h, --help      Print help message and exit"
"\n  -v, --version   Print rirc version and exit"
"\n"
"\nOptions:"
"\n  -s, --server=HOST         Set connection hostname"
"\n  -p, --port=PORT           Set connection port"
"\n  -w, --pass=PASS           Set IRC password"
"\n  -u, --username=USERNAME   Set IRC username"
"\n  -r, --realname=REALNAME   Set IRC realname"
"\n  -m, --mode=MODE           Set IRC user modes"
"\n  -n, --nicks=NICKS         Set comma separated list of nicks to use"
"\n  -c, --chans=CHANNELS      Set comma separated list of channels to join"
"\n      --tls-cert=PATH       Set TLS client certificate file path"
"\n      --tls-ca-file=PATH    Set TLS peer certificate file path"
"\n      --tls-ca-path=PATH    Set TLS peer certificate directory path"
"\n      --tls-verify=MODE     Set TLS peer certificate verification mode"
"\n      --tls-disable         Set TLS disabled"
"\n      --sasl=MECHANISM      Authenticate with SASL mechanism"
"\n      --sasl-user=USER      Authenticate with SASL username"
"\n      --sasl-pass=PASS      Authenticate with SASL password"
"\n      --ipv4                Use IPv4 addresses only"
"\n      --ipv6                Use IPv6 addresses only"
"\n";

static const char *const rirc_version =
#ifndef NDEBUG
"rirc v" STR(VERSION) " (debug " STR(GITHASH) ")";
#else
"rirc v" STR(VERSION);
#endif

static const char*
rirc_opt_str(char c)
{
	switch (c) {
		case 's': return "-s/--server";
		case 'p': return "-p/--port";
		case 'w': return "-w/--pass";
		case 'u': return "-u/--username";
		case 'r': return "-r/--realname";
		case 'm': return "-m/--mode";
		case 'n': return "-n/--nicks";
		case 'c': return "-c/--chans";
		case '0': return "--tls-cert";
		case '1': return "--tls-ca-file";
		case '2': return "--tls-ca-path";
		case '3': return "--tls-verify";
		case '4': return "--tls-disable";
		case '5': return "--sasl";
		case '6': return "--sasl-user";
		case '7': return "--sasl-pass";
		case '8': return "--ipv4";
		case '9': return "--ipv6";
		default:
			fatal("unknown option flag '%c'", c);
	}
	return NULL;
}

static const char*
rirc_pw_name(void)
{
	static const struct passwd *passwd;

	errno = 0;

	if (!passwd && !(passwd = getpwuid(geteuid())))
		fatal("getpwuid: %s", strerror((errno ? errno : ENOENT)));

	return passwd->pw_name;
}

static int
rirc_parse_args(int argc, char **argv)
{
	int opt_c = 0;
	int opt_i = 0;

	size_t n_servers = 0;

	struct cli_server {
		const char *host;
		const char *port;
		const char *pass;
		const char *username;
		const char *realname;
		const char *mode;
		const char *nicks;
		const char *chans;
		const char *tls_ca_file;
		const char *tls_ca_path;
		const char *tls_cert;
		const char *sasl;
		const char *sasl_user;
		const char *sasl_pass;
		int ipv;
		int tls;
		int tls_vrfy;
		struct server *s;
	} cli_servers[MAX_CLI_SERVERS];

	struct option long_opts[] = {
		{"server",      required_argument, 0, 's'},
		{"port",        required_argument, 0, 'p'},
		{"pass",        required_argument, 0, 'w'},
		{"username",    required_argument, 0, 'u'},
		{"realname",    required_argument, 0, 'r'},
		{"mode",        required_argument, 0, 'm'},
		{"nicks",       required_argument, 0, 'n'},
		{"chans",       required_argument, 0, 'c'},
		{"help",        no_argument,       0, 'h'},
		{"version",     no_argument,       0, 'v'},
		{"tls-cert",    required_argument, 0, '0'},
		{"tls-ca-file", required_argument, 0, '1'},
		{"tls-ca-path", required_argument, 0, '2'},
		{"tls-verify",  required_argument, 0, '3'},
		{"tls-disable", no_argument,       0, '4'},
		{"sasl",        required_argument, 0, '5'},
		{"sasl-user",   required_argument, 0, '6'},
		{"sasl-pass",   required_argument, 0, '7'},
		{"ipv4",        no_argument,       0, '8'},
		{"ipv6",        no_argument,       0, '9'},
		{0, 0, 0, 0}
	};

	opterr = 0;

	while (0 < (opt_c = getopt_long(argc, argv, ":s:p:w:u:r:m:n:c:hv", long_opts, &opt_i))) {

		switch (opt_c) {

			case 's': /* Connect to server */

				if (*optarg == '-') {
					arg_error("-s/--server requires an argument");
					return -1;
				}

				if (++n_servers == MAX_CLI_SERVERS) {
					arg_error("exceeded maximum number of servers (%d)", MAX_CLI_SERVERS);
					return -1;
				}

				cli_servers[n_servers - 1].host        = optarg;
				cli_servers[n_servers - 1].port        = NULL;
				cli_servers[n_servers - 1].pass        = NULL;
				cli_servers[n_servers - 1].username    = default_username;
				cli_servers[n_servers - 1].realname    = default_realname;
				cli_servers[n_servers - 1].mode        = NULL;
				cli_servers[n_servers - 1].nicks       = default_nicks;
				cli_servers[n_servers - 1].chans       = NULL;
				cli_servers[n_servers - 1].tls_ca_file = NULL;
				cli_servers[n_servers - 1].tls_ca_path = NULL;
				cli_servers[n_servers - 1].tls_cert    = NULL;
				cli_servers[n_servers - 1].sasl        = NULL;
				cli_servers[n_servers - 1].sasl_user   = NULL;
				cli_servers[n_servers - 1].sasl_pass   = NULL;
				cli_servers[n_servers - 1].ipv         = IO_IPV_UNSPEC;
				cli_servers[n_servers - 1].tls         = IO_TLS_ENABLED;
				cli_servers[n_servers - 1].tls_vrfy    = IO_TLS_VRFY_REQUIRED;
				break;

			#define CHECK_SERVER_OPTARG(OPT_C, REQ) \
				if ((REQ) && *optarg == '-') { \
					arg_error("option '%s' requires an argument", rirc_opt_str((OPT_C))); \
					return -1; \
				} \
				if (n_servers == 0) { \
					arg_error("option '%s' requires a server argument first", rirc_opt_str((OPT_C))); \
					return -1; \
				}

			case 'p': /* Set connection port */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].port = optarg;
				break;

			case 'w': /* Set IRC password */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].pass = optarg;
				break;

			case 'u': /* Set IRC username */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].username = optarg;
				break;

			case 'r': /* Set IRC realname */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].realname = optarg;
				break;

			case 'm': /* Set IRC user modes */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].mode = optarg;
				break;

			case 'n': /* Set comma separated list of nicks to use */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].nicks = optarg;
				break;

			case 'c': /* Set comma separated list of channels to join */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].chans = optarg;
				break;

			case '0': /* Set TLS client certificate file path */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].tls_cert = optarg;
				break;

			case '1': /* Set TLS peer certificate file path */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].tls_ca_file = optarg;
				break;

			case '2': /* Set TLS peer certificate directory path */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].tls_ca_path = optarg;
				break;

			case '3': /* Set TLS peer certificate verification mode */
				CHECK_SERVER_OPTARG(opt_c, 1);
				if (!strcmp(optarg, "0") || !strcasecmp(optarg, "DISABLED")) {
					cli_servers[n_servers -1].tls_vrfy = IO_TLS_VRFY_DISABLED;
					break;
				}
				if (!strcmp(optarg, "1") || !strcasecmp(optarg, "OPTIONAL")) {
					cli_servers[n_servers -1].tls_vrfy = IO_TLS_VRFY_OPTIONAL;
					break;
				}
				if (!strcmp(optarg, "2") || !strcasecmp(optarg, "REQUIRED")) {
					cli_servers[n_servers -1].tls_vrfy = IO_TLS_VRFY_REQUIRED;
					break;
				}
				arg_error("invalid option for '--tls-verify' '%s'", optarg);
				return -1;

			case '4': /* Set TLS disabled */
				CHECK_SERVER_OPTARG(opt_c, 0);
				cli_servers[n_servers -1].tls = IO_TLS_DISABLED;
				break;

			case '5': /* Authenticate with SASL mechanism */
				CHECK_SERVER_OPTARG(opt_c, 1);
				if (!strcasecmp(optarg, "EXTERNAL")) {
					cli_servers[n_servers - 1].sasl = optarg;
					break;
				}
				if (!strcasecmp(optarg, "PLAIN")) {
					cli_servers[n_servers - 1].sasl = optarg;
					break;
				}
				arg_error("invalid option for '--sasl' '%s'", optarg);
				return -1;

			case '6': /* Authenticate with SASL user */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].sasl_user = optarg;
				break;

			case '7': /* Authenticate with SASL pass */
				CHECK_SERVER_OPTARG(opt_c, 1);
				cli_servers[n_servers - 1].sasl_pass = optarg;
				break;

			case '8': /* Use IPv4 only */
				CHECK_SERVER_OPTARG(opt_c, 0);
				cli_servers[n_servers -1].ipv = IO_IPV_4;
				break;

			case '9': /* Use IPv6 only */
				CHECK_SERVER_OPTARG(opt_c, 0);
				cli_servers[n_servers -1].ipv = IO_IPV_6;
				break;

			#undef CHECK_SERVER_OPTARG

			case 'h':
				puts(rirc_help);
				exit(EXIT_SUCCESS);

			case 'v':
				puts(rirc_version);
				exit(EXIT_SUCCESS);

			case '?':
				arg_error("unknown option '%s'", argv[optind - 1]);
				return -1;

			case ':':
				arg_error("option '%s' requires an argument", rirc_opt_str(optopt));
				return -1;

			default:
				arg_error("unknown opt error");
				return -1;
		}
	}

	if (optind < argc) {
		arg_error("unused option '%s'", argv[optind]);
		return -1;
	}

	for (size_t i = 0; i < n_servers; i++) {

		if (cli_servers[i].port == NULL)
			cli_servers[i].port = (cli_servers[i].tls == IO_TLS_ENABLED) ? "6697" : "6667";

		cli_servers[i].s = server(
			cli_servers[i].host,
			cli_servers[i].port,
			cli_servers[i].pass,
			cli_servers[i].username,
			cli_servers[i].realname,
			cli_servers[i].mode
		);

		cli_servers[i].s->connection = connection(
			cli_servers[i].s,
			cli_servers[i].host,
			cli_servers[i].port,
			cli_servers[i].tls_ca_file,
			cli_servers[i].tls_ca_path,
			cli_servers[i].tls_cert,
			(cli_servers[i].ipv |
			 cli_servers[i].tls |
			 cli_servers[i].tls_vrfy));

		if (server_list_add(state_server_list(), cli_servers[i].s)) {
			arg_error("duplicate server: %s:%s", cli_servers[i].host, cli_servers[i].port);
			return -1;
		}

		if (cli_servers[i].nicks && server_set_nicks(cli_servers[i].s, cli_servers[i].nicks)) {
			arg_error("invalid %s: '%s'", rirc_opt_str('n'), cli_servers[i].nicks);
			return -1;
		}

		if (cli_servers[i].chans && server_set_chans(cli_servers[i].s, cli_servers[i].chans)) {
			arg_error("invalid %s: '%s'", rirc_opt_str('c'), cli_servers[i].chans);
			return -1;
		}

		if (cli_servers[i].sasl) {
			server_set_sasl(
				cli_servers[i].s,
				cli_servers[i].sasl,
				cli_servers[i].sasl_user,
				cli_servers[i].sasl_pass);
		}

		channel_set_current(cli_servers[i].s->channel);
	}

	for (size_t i = 0; i < n_servers; i++) {

		int ret;

		if ((ret = io_cx(cli_servers[i].s->connection)))
			server_error(cli_servers[i].s, "failed to connect: %s", io_err(ret));
	}

	return 0;
}

#ifndef TESTING
int
main(int argc, char **argv)
{
	if (argc)
		runtime_name = argv[0];

	if (!default_username || !default_username[0])
		default_username = rirc_pw_name();

	if (!default_realname || !default_realname[0])
		default_realname = rirc_pw_name();

	if (!default_nicks || !default_nicks[0])
		default_nicks = rirc_pw_name();

	srand(time(NULL));

	state_init();
	io_init();

	if (rirc_parse_args(argc, argv)) {
		state_term();
		return EXIT_FAILURE;
	}

	draw_init();
	io_start();
	draw_term();
	state_term();

	return EXIT_SUCCESS;
}
#endif
