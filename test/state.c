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

#define CURRENT_LINE \
	(buffer_head(&current_channel()->buffer) ? \
	 buffer_head(&current_channel()->buffer)->text : NULL)

static void
test_command(void)
{
	state_init();

	INP_S(":unknown command with args");
	INP_C(0x0A);

	assert_strcmp(action_message(), "Unknown command 'unknown'");

	/* clear error */
	INP_C(0x0A);

	state_term();
}

static void
test_command_clear(void)
{
	state_init();

	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");

	INP_S(":clear with args");
	INP_C(0x0A);

	assert_strcmp(action_message(), "clear: Unknown arg 'with'");

	/* clear error */
	INP_C(0x0A);

	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");

	INP_S(":clear");
	INP_C(0x0A);

	assert_ptr_null(CURRENT_LINE);

	state_term();
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

	state_init();

	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");

	INP_S(":close");
	INP_C(0x0A);

	assert_strcmp(action_message(), "Type :quit to exit rirc");

	/* clear error */
	INP_C(0x0A);

	/* host1 #c1 #c2, host2 #c3 #c4 */
	c1 = channel("#c1", CHANNEL_T_CHANNEL);
	c2 = channel("#c2", CHANNEL_T_CHANNEL);
	c3 = channel("#c3", CHANNEL_T_CHANNEL);
	c4 = channel("#c4", CHANNEL_T_CHANNEL);
	c5 = channel("#c5", CHANNEL_T_CHANNEL);
	s1 = server("host1", "port1", NULL, "user1", "real1");
	s2 = server("host2", "port2", NULL, "user2", "real2");

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

	INP_S(":close with args");
	INP_C(0x0A);

	assert_strcmp(action_message(), "close: Unknown arg 'with'");

	/* clear error */
	INP_C(0x0A);

	/* test closing last channel on server */
	channel_set_current(c2);

	INP_S(":close");
	INP_C(0x0A);

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_strcmp(current_channel()->name, "host2");

	/* test closing server channel */
	channel_set_current(s1->channel);

	INP_S(":close");
	INP_C(0x0A);

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_strcmp(current_channel()->name, "host2");

	/* test closing middle channel*/
	channel_set_current(c3);

	INP_S(":close");
	INP_C(0x0A);

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_strcmp(current_channel()->name, "#c4");

	/* test closing last channel*/
	channel_set_current(c5);

	INP_S(":close");
	INP_C(0x0A);

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_strcmp(current_channel()->name, "#c4");

	state_term();
}

static void
test_command_connect(void)
{
	struct server *s;

	mock_reset_io();

	state_init();
	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");

	INP_S(":connect");
	INP_C(0x0A);

	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");
	assert_strcmp(action_message(), "connect: This is not a server");

	/* clear error */
	INP_C(0x0A);

	if (!(s = server("host", "port", NULL, "user", "real")))
		test_abort("Failed test setup");

	if (server_list_add(state_server_list(), s))
		test_abort("Failed to add server");

	channel_set_current(s->channel);

	INP_S(":connect");
	INP_C(0x0A);

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_ptr_null(CURRENT_LINE);

	INP_S(":connect with args");
	INP_C(0x0A);

	assert_strcmp(action_message(), "connect: Unknown arg 'with'");

	/* clear error */
	INP_C(0x0A);

	INP_S(":connect");
	INP_C(0x0A);

	assert_strcmp(action_message(), "connect: cxed");

	/* clear error */
	INP_C(0x0A);

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_ptr_null(CURRENT_LINE);

	state_term();
}

static void
test_command_disconnect(void)
{
	struct server *s;

	mock_reset_io();

	state_init();
	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");

	INP_S(":disconnect");
	INP_C(0x0A);

	assert_strcmp(CURRENT_LINE, " - compiled with DEBUG flags");
	assert_strcmp(action_message(), "disconnect: This is not a server");

	/* clear error */
	INP_C(0x0A);

	if (!(s = server("host", "port", NULL, "user", "real")))
		test_abort("Failed test setup");

	if (server_list_add(state_server_list(), s))
		test_abort("Failed to add server");

	channel_set_current(s->channel);

	io_cx(NULL);

	INP_S(":disconnect");
	INP_C(0x0A);

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_ptr_null(CURRENT_LINE);

	INP_S(":disconnect with args");
	INP_C(0x0A);

	assert_strcmp(action_message(), "disconnect: Unknown arg 'with'");

	/* clear error */
	INP_C(0x0A);

	INP_S(":disconnect");
	INP_C(0x0A);

	assert_strcmp(action_message(), "disconnect: dxed");

	/* clear error */
	INP_C(0x0A);

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());
	assert_ptr_null(CURRENT_LINE);

	state_term();
}

static void
test_command_quit(void)
{
	state_init();

	INP_S(":quit with args");
	INP_C(0x0A);

	assert_strcmp(action_message(), "quit: Unknown arg 'with'");

	/* clear error */
	INP_C(0x0A);

	INP_S(":quit");
	INP_C(0x0A);

	assert_ptr_null(action_handler);
	assert_ptr_null(action_message());

	state_term();
}

static void
test_state(void)
{
	state_init();

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
	struct server *s1 = server("h1", "p1", NULL, "u1", "r1");
	struct server *s2 = server("h2", "p2", NULL, "u2", "r2");
	struct server *s3 = server("h3", "p3", NULL, "u3", "r3");

	assert_ptr_eq(server_list_add(state_server_list(), s1), NULL);
	assert_ptr_eq(server_list_add(state_server_list(), s2), NULL);
	assert_ptr_eq(server_list_add(state_server_list(), s3), NULL);

	state_term();
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

	return run_tests(NULL, NULL, tests);
}
