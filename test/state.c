#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/ircv3.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/handlers/irc_send.c"
#include "src/state.c"
#include "src/utils/utils.c"

#include "test/draw.mock.c"
#include "test/handlers/irc_recv.mock.c"
#include "test/io.mock.c"
#include "test/rirc.mock.c"

#define INP_S(S) io_cb_read_inp((S), strlen(S))
#define INP_C(C) io_cb_read_inp((char[]){(C)}, 1)

#define INP_COMMAND(C) \
	do { \
		io_cb_read_inp((C), strlen(C)); \
		io_cb_read_inp("\012", 1); \
	} while (0)

#define CURRENT_LINE \
	(buffer_head(&current_channel()->buffer) ? \
	 buffer_head(&current_channel()->buffer)->text : NULL)

static void
test_command(void)
{
	INP_COMMAND(":unknown command with args");

	assert_strcmp(action_message(), "Unknown command 'unknown'");
}

static void
test_command_clear(void)
{
	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");

	INP_COMMAND(":clear");

	assert_ptr_null(CURRENT_LINE);

	INP_COMMAND(":clear with args");

	assert_strcmp(action_message(), "clear: Unknown arg 'with'");
}

static void
test_command_close(void)
{
	static struct channel *c1;
	static struct channel *c2;
	static struct channel *c3;
	static struct channel *c4;
	static struct channel *c5;
	struct server *s1;
	struct server *s2;

	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");

	INP_COMMAND(":close");

	assert_strcmp(action_message(), "Type :quit to exit rirc");

	/* clear error */
	INP_C(0x0A);

	/* host1 #c1 #c2, host2 #c3 #c4 */
	c1 = channel("#c1", CHANNEL_T_CHANNEL);
	c2 = channel("#c2", CHANNEL_T_CHANNEL);
	c3 = channel("#c3", CHANNEL_T_CHANNEL);
	c4 = channel("#c4", CHANNEL_T_CHANNEL);
	c5 = channel("#c5", CHANNEL_T_CHANNEL);
	s1 = server("host1", "port1", NULL, "user1", "real1", NULL);
	s2 = server("host2", "port2", NULL, "user2", "real2", NULL);

	if (!s1 || !s2 || !c1 || !c2 || !c3 || !c4 || !c5)
		test_abort("Failed to create servers and channels");

	c1->server = s1;
	c2->server = s1;
	channel_list_add(&(s1->clist), c1);
	channel_list_add(&(s1->clist), c2);

	c3->server = s2;
	c4->server = s2;
	c5->server = s2;
	channel_list_add(&(s2->clist), c3);
	channel_list_add(&(s2->clist), c4);
	channel_list_add(&(s2->clist), c5);

	if (server_list_add(state_server_list(), s1))
		test_abort("Failed to add server");

	if (server_list_add(state_server_list(), s2))
		test_abort("Failed to add server");

	channel_set_current(c1);

	INP_COMMAND(":close with args");

	assert_strcmp(action_message(), "close: Unknown arg 'with'");

	/* clear error */
	INP_C(0x0A);

	/* test closing last channel on server */
	channel_set_current(c2);

	INP_COMMAND(":close");

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_strcmp(current_channel()->name, "host2");

	/* test closing server channel */
	channel_set_current(s1->channel);

	INP_COMMAND(":close");

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_strcmp(current_channel()->name, "host2");

	/* test closing middle channel*/
	channel_set_current(c3);

	INP_COMMAND(":close");

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_strcmp(current_channel()->name, "#c4");

	/* test closing last channel*/
	channel_set_current(c5);

	INP_COMMAND(":close");

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_strcmp(current_channel()->name, "#c4");
}

static void
test_command_connect(void)
{
	struct server *s;

	INP_COMMAND(":connect");

	assert_strcmp(action_message(), "connect: This is not a server");
	INP_C(0x0A);

	INP_COMMAND(":connect host-1");

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_strcmp(current_channel()->name, "host-1");

	current_channel()->server->connected = 1;

	INP_COMMAND(":connect");

	assert_strcmp(action_message(), "connect: cxed");
	INP_C(0x0A);

	/* Test empty arguments */
	INP_COMMAND(":connect host -p");
	assert_strcmp(action_message(), "connect: '-p/--port' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --port");
	assert_strcmp(action_message(), "connect: '-p/--port' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host -w");
	assert_strcmp(action_message(), "connect: '-w/--pass' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --pass");
	assert_strcmp(action_message(), "connect: '-w/--pass' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host -u");
	assert_strcmp(action_message(), "connect: '-u/--username' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --username");
	assert_strcmp(action_message(), "connect: '-u/--username' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host -r");
	assert_strcmp(action_message(), "connect: '-r/--realname' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --realname");
	assert_strcmp(action_message(), "connect: '-r/--realname' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host -m");
	assert_strcmp(action_message(), "connect: '-m/--mode' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --mode");
	assert_strcmp(action_message(), "connect: '-m/--mode' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host -n");
	assert_strcmp(action_message(), "connect: '-n/--nicks' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --nicks");
	assert_strcmp(action_message(), "connect: '-n/--nicks' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host -c");
	assert_strcmp(action_message(), "connect: '-c/--chans' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --chans");
	assert_strcmp(action_message(), "connect: '-c/--chans' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --tls-verify");
	assert_strcmp(action_message(), "connect: '--tls-verify' requires an argument");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	/* Test invalid arguments */
	INP_COMMAND(":connect host xyz");
	assert_strcmp(action_message(), ":connect [hostname [options]]");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --xyz");
	assert_strcmp(action_message(), "connect: unknown option '--xyz'");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --tls-verify xyz");
	assert_strcmp(action_message(), "connect: invalid option for '--tls-verify' 'xyz'");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host-1");
	assert_strcmp(action_message(), "connect: duplicate server: host-1:6697");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --nicks 0,1,2");
	assert_strcmp(action_message(), "connect: invalid -n/--nicks: '0,1,2'");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	INP_COMMAND(":connect host --chans 0,1,2");
	assert_strcmp(action_message(), "connect: invalid -c/--chans: '0,1,2'");
	assert_strcmp(current_channel()->name, "host-1");
	INP_C(0x0A);

	/* Test default values */
	INP_COMMAND(":connect host-2");

	s = current_channel()->server;

	assert_strcmp(s->host, "host-2");
	assert_strcmp(s->port, "6697");
	assert_ptr_null(s->pass);
	assert_strcmp(s->username, "username");
	assert_strcmp(s->realname, "realname");
	assert_ptr_null(s->mode);
	assert_strcmp(s->nicks.set[0], "n1");
	assert_strcmp(s->nicks.set[1], "n2");
	assert_strcmp(s->nicks.set[2], "n3");

	/* Test all values */
	INP_COMMAND(":connect host-3"
		" --port 1234"
		" --pass abcdef"
		" --username xxx"
		" --realname yyy"
		" --mode zzz"
		" --nicks x0,y1,z2"
		" --chans #a1,b2,#c3"
		" --ipv4"
		" --ipv6"
		" --tls-disable"
		" --tls-verify 0"
		" --tls-verify 1"
		" --tls-verify 2"
		" --tls-verify disabled"
		" --tls-verify optional"
		" --tls-verify required"
	);

	s = current_channel()->server;

	assert_strcmp(s->host, "host-3");
	assert_strcmp(s->port, "1234");
	assert_strcmp(s->pass, "abcdef");
	assert_strcmp(s->username, "xxx");
	assert_strcmp(s->realname, "yyy");
	assert_strcmp(s->mode, "zzz");
	assert_strcmp(s->nicks.set[0], "x0");
	assert_strcmp(s->nicks.set[1], "y1");
	assert_strcmp(s->nicks.set[2], "z2");
	assert_ptr_not_null(channel_list_get(&(s->clist), "#a1", s->casemapping));
	assert_ptr_not_null(channel_list_get(&(s->clist), "b2", s->casemapping));
	assert_ptr_not_null(channel_list_get(&(s->clist), "#c3", s->casemapping));
}

static void
test_command_disconnect(void)
{
	struct server *s;

	mock_reset_io();

	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");

	INP_COMMAND(":disconnect");

	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");
	assert_strcmp(action_message(), "disconnect: This is not a server");

	/* clear error */
	INP_C(0x0A);

	if (!(s = server("host", "port", NULL, "user", "real", NULL)))
		test_abort("Failed test setup");

	if (server_list_add(state_server_list(), s))
		test_abort("Failed to add server");

	channel_set_current(s->channel);

	io_cx(NULL);

	INP_COMMAND(":disconnect");

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_ptr_null(CURRENT_LINE);

	INP_COMMAND(":disconnect with args");

	assert_strcmp(action_message(), "disconnect: Unknown arg 'with'");

	/* clear error */
	INP_C(0x0A);

	INP_COMMAND(":disconnect");

	assert_strcmp(action_message(), "disconnect: dxed");

	/* clear error */
	INP_C(0x0A);

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_ptr_null(CURRENT_LINE);
}

static void
test_command_quit(void)
{
	INP_COMMAND(":quit with args");

	assert_strcmp(action_message(), "quit: Unknown arg 'with'");

	/* clear error */
	INP_C(0x0A);

	INP_COMMAND(":quit");

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
}

static void
test_state(void)
{
	/* Test splash message */
	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");

	/* Test messages sent to the default rirc buffer */
	INP_S("/");
	INP_C(0x0A);
	assert_strcmp(CURRENT_LINE, "This is not a server");

	INP_S("//");
	INP_C(0x0A);
	assert_strcmp(CURRENT_LINE, "This is not a server");

	INP_S("::");
	INP_C(0x0A);
	assert_strcmp(CURRENT_LINE, "This is not a server");

	INP_S("test");
	INP_C(0x0A);
	assert_strcmp(CURRENT_LINE, "This is not a server");

	/* Test control characters */
	INP_C(CTRL('c'));
	INP_C(CTRL('p'));
	INP_C(CTRL('x'));
	assert_strcmp(action_message(), "Type :quit to exit rirc");
	INP_C('\n');

	INP_C(CTRL('l'));
	assert_strcmp(action_message(), "Clear buffer 'rirc'?   [y/n]");
	INP_C('y');
	assert_ptr_null(buffer_head(&current_channel()->buffer));

	/* Test empty command */
	INP_S(":");
	INP_C(0x0A);
	assert_ptr_null(buffer_head(&current_channel()->buffer));

	/* Test adding servers */
	struct server *s1 = server("h1", "p1", NULL, "u1", "r1", NULL);
	struct server *s2 = server("h2", "p2", NULL, "u2", "r2", NULL);
	struct server *s3 = server("h3", "p3", NULL, "u3", "r3", NULL);

	assert_ptr_eq(server_list_add(state_server_list(), s1), NULL);
	assert_ptr_eq(server_list_add(state_server_list(), s2), NULL);
	assert_ptr_eq(server_list_add(state_server_list(), s3), NULL);
}

static int
test_init(void)
{
	state_init();

	return 0;
}

static int
test_term(void)
{
	state_term();

	return 0;
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_command),
		TESTCASE(test_command_clear),
		TESTCASE(test_command_close),
		TESTCASE(test_command_connect),
		TESTCASE(test_command_disconnect),
		TESTCASE(test_command_quit),
		TESTCASE(test_state),
	};

	return run_tests(test_init, test_term, tests);
}
