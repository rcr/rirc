#include "../src/mesg.c"
#include "../src/utils.c"

#include "test.h"

/* Mock stuff */

static server mock_s = {
	.host = "mock-host",
	.port = "mock-port",

	.connecting = NULL,
};

static channel mock_c = {
	.server = &mock_s
};

static channel *c = &mock_c;

static char err[MAX_ERROR];

/* TODO:
 * Move this stuff and find a cleaner way to reset these in tests
 * */
static int server_connect__called__;
static char *server_connect__host__;
static char *server_connect__port__;

void
server_connect(char *host, char *port)
{
	server_connect__called__ = 1;

	server_connect__host__ = host;
	server_connect__port__ = port;
}

/* Handler tests */

void
test_send_connect(void)
{
	/* No args, connected, should issue an error message */
	server_connect__called__ = 0;
	server_connect__host__ = NULL;
	server_connect__port__ = NULL;

	mock_s.soc = 1; /* Non-negative socket implies connected */

	char str1[] = "";
	send_connect(err, str1, c);

	assert_equals(server_connect__called__, 0);
	assert_strcmp(err, "Error: Already connected or reconnecting to server");


	/* No args, not connected, should attempt to reconnect on the current server */
	server_connect__called__ = 0;
	server_connect__host__ = NULL;
	server_connect__port__ = NULL;

	mock_s.soc = -1; /* -1 socket implies not connected */

	char str2[] = "";
	send_connect(err, str2, c);

	assert_equals(server_connect__called__, 1);
	assert_strcmp(server_connect__host__, "mock-host");
	assert_strcmp(server_connect__port__, "mock-port");


	/* <server> */
	server_connect__called__ = 0;
	server_connect__host__ = NULL;
	server_connect__port__ = NULL;

	char str3[] = "server.tld";
	send_connect(err, str3, c);

	assert_equals(server_connect__called__, 1);
	assert_strcmp(server_connect__host__, "server.tld");
	assert_strcmp(server_connect__port__, "6667");


	/* <server>:<port> */
	server_connect__called__ = 0;
	server_connect__host__ = NULL;
	server_connect__port__ = NULL;

	char str4[] = "server.tld:123";
	send_connect(err, str4, c);

	assert_equals(server_connect__called__, 1);
	assert_strcmp(server_connect__host__, "server.tld");
	assert_strcmp(server_connect__port__, "123");


	/* <server> <port> */
	server_connect__called__ = 0;
	server_connect__host__ = NULL;
	server_connect__port__ = NULL;

	char str5[] = "server.tld 123";
	send_connect(err, str5, c);

	assert_equals(server_connect__called__, 1);
	assert_strcmp(server_connect__host__, "server.tld");
	assert_strcmp(server_connect__port__, "123");
}

int
main(void)
{
	testcase tests[] = {
		&test_send_connect,
	};

	return run_tests(tests);
}

/* Stubbed out functions required by hanlders in send/recv methods
 *
 * TODO: The number of functions here is an indication that some refactoring is needed,
 * consider moving this stuff to state.c.mock */

void
newline(channel *c, line_t type, const char *from, const char *mesg)
{
	UNUSED(c);
	UNUSED(type);
	UNUSED(from);
	UNUSED(mesg);
}

void
newlinef(channel *c, line_t type, const char *from, const char *mesg, ...)
{
	UNUSED(c);
	UNUSED(type);
	UNUSED(from);
	UNUSED(mesg);
}

channel*
channel_get(char *chan, server *s)
{
	UNUSED(chan);
	UNUSED(s);

	return NULL;
}

int
sendf(char *err, server *s, const char *fmt, ...)
{
	UNUSED(err);
	UNUSED(s);
	UNUSED(fmt);

	return 0;
}

channel*
new_channel(char *name, server *server, channel *chanlist, buffer_t type)
{
	UNUSED(name);
	UNUSED(server);
	UNUSED(chanlist);
	UNUSED(type);

	return NULL;
}

void
server_disconnect(server *s, int err, int kill, char *mesg)
{
	UNUSED(s);
	UNUSED(err);
	UNUSED(kill);
	UNUSED(mesg);
}

void
auto_nick(char **autonick, char *nick)
{
	UNUSED(autonick);
	UNUSED(nick);
}

void
clear_channel(channel *c)
{
	UNUSED(c);
}

channel*
channel_close(channel *c)
{
	UNUSED(c);

	return NULL;
}

void
free_channel(channel *c)
{
	UNUSED(c);
}
