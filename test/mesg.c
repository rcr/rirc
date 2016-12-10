#include "test.h"

#if 0
/* FIXME: these tests are all broken and should be rewriten after
 * refactoring and decoupling the includes */

#include "../src/mesg.c"
#include "../src/utils.c"
#include "../src/state.c.mock"

/* Mock stuff */

static server mock_s = {
	.host = "mock-host",
	.port = "mock-port",

	.nick = "mock-nick",

	.connecting = NULL,
};

static channel mock_c = {
	.server = &mock_s,
	.name = "mock-channel",
};

static channel *c = &mock_c;

static char err[MAX_ERROR];

/* send handler tests */

/* Ensure all send handlers have a testcase */
#define X(cmd) static void test_send_##cmd(void);
HANDLED_SEND_CMDS
#undef X

static void
test_send_connect(void)
{
	/* /connect [(host) | (host:port) | (host port)] */

	/* No args, connected, should issue an error message */
	server_connect__called__ = 0;
	server_connect__host__ = NULL;
	server_connect__port__ = NULL;
	*err = 0;

	/* `/connect` no serverless buffer should return an error */

	c->server = NULL;

	char str1[] = "";
	send_connect(err, str1, c);

	assert_equals(server_connect__called__, 0);
	assert_strcmp(err, "Error: /connect <host | host:port | host port>");

	c->server = &mock_s;

	/* No args, connected, should issue an error message */
	server_connect__called__ = 0;
	server_connect__host__ = NULL;
	server_connect__port__ = NULL;
	*err = 0;

	mock_s.soc = 1; /* Non-negative socket implies connected */

	char str2[] = "";
	send_connect(err, str2, c);

	assert_equals(server_connect__called__, 0);
	assert_strcmp(err, "Error: Already connected or reconnecting to server");


	/* No args, not connected, should attempt to reconnect on the current server */
	server_connect__called__ = 0;
	server_connect__host__ = NULL;
	server_connect__port__ = NULL;

	mock_s.soc = -1; /* -1 socket implies not connected */

	char str3[] = "";
	send_connect(err, str3, c);

	assert_equals(server_connect__called__, 1);
	assert_strcmp(server_connect__host__, "mock-host");
	assert_strcmp(server_connect__port__, "mock-port");


	/* <server> */
	server_connect__called__ = 0;
	server_connect__host__ = NULL;
	server_connect__port__ = NULL;

	char str4[] = "server.tld";
	send_connect(err, str4, c);

	assert_equals(server_connect__called__, 1);
	assert_strcmp(server_connect__host__, "server.tld");
	assert_strcmp(server_connect__port__, "6667");


	/* <server>:<port> */
	server_connect__called__ = 0;
	server_connect__host__ = NULL;
	server_connect__port__ = NULL;

	char str5[] = "server.tld:123";
	send_connect(err, str5, c);

	assert_equals(server_connect__called__, 1);
	assert_strcmp(server_connect__host__, "server.tld");
	assert_strcmp(server_connect__port__, "123");


	/* <server> <port> */
	server_connect__called__ = 0;
	server_connect__host__ = NULL;
	server_connect__port__ = NULL;

	char str6[] = "server.tld 123";
	send_connect(err, str6, c);

	assert_equals(server_connect__called__, 1);
	assert_strcmp(server_connect__host__, "server.tld");
	assert_strcmp(server_connect__port__, "123");
}

static void
test_send_ctcp(void)
{
	/* /ctcp <target> <message> */

	*err = 0;

	char str1[] = "";
	send_ctcp(err, str1, c);

	assert_strcmp(err, "Error: /ctcp <target> <command> [arguments]");


	*err = 0;

	char str2[] = "target";
	send_ctcp(err, str2, c);

	assert_strcmp(err, "Error: /ctcp <target> <command> [arguments]");


	server_connect__called__ = 0;
	*sendf__buff__ = 0;

	char str3[] = "target command";
	send_ctcp(err, str3, c);

	assert_equals(sendf__called__, 1);
	assert_strcmp(sendf__buff__, "PRIVMSG target :""\x01""COMMAND\x01");


	server_connect__called__ = 0;
	*sendf__buff__ = 0;

	char str4[] = "target coMMand arg1 arg2 arg3";
	send_ctcp(err, str4, c);

	assert_equals(sendf__called__, 1);
	assert_strcmp(sendf__buff__, "PRIVMSG target :""\x01""COMMAND arg1 arg2 arg3\x01");
}

static void
test_send_ignore(void)
{
	/* /ignore [nick] */

	nicklist_print__called__ = 0;

	char str1[] = "";
	send_ignore(err, str1, c);

	assert_equals(nicklist_print__called__, 1);


	newlinef__called__ = 0;
	*newlinef__buff__ = 0;

	char str2[] = "ignore_test";
	send_ignore(err, str2, c);

	assert_equals(newlinef__called__, 1);
	assert_strcmp(newlinef__buff__, "Ignoring 'ignore_test'");


	newlinef__called__ = 0;
	*err = 0;

	char str3[] = "ignore_test";
	send_ignore(err, str3, c);

	assert_strcmp(err, "Error: Already ignoring 'ignore_test'");
}

static void
test_send_nick(void)
{
	/* /nick [nick] */

	newlinef__called__ = 0;
	*newlinef__buff__ = 0;

	char str1[] = "";
	send_nick(err, str1, c);

	assert_equals(newlinef__called__, 1);
	assert_strcmp(newlinef__buff__, "Your nick is mock-nick");


	server_connect__called__ = 0;
	*sendf__buff__ = 0;

	char str2[] = "nick_test";
	send_nick(err, str2, c);

	assert_equals(sendf__called__, 1);
	assert_strcmp(sendf__buff__, "NICK nick_test");
}

int
main(void)
{
	testcase tests[] = {
		/* Test all send handlers */
		#define X(cmd) &test_send_##cmd,
		HANDLED_SEND_CMDS
		#undef X

		/* TODO: all the other recv commands */
		&test_recv_join,
	};

	return run_tests(tests);
}

#endif

static void
test_dummy(void)
{
	;
}


int
main(void)
{
	testcase tests[] = {
		&test_dummy
	};

	return run_tests(tests);
}
