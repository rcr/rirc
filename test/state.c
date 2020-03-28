#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/handlers/irc_send.c"
#include "src/state.c"
#include "src/utils/utils.c"

#include "test/draw.c.mock"
#include "test/io.c.mock"
#include "test/rirc.c.mock"
#include "test/handlers/irc_recv.c.mock"

#define INP_S(S) io_cb_read_inp((S), strlen(S))
#define INP_C(C) io_cb_read_inp((char[]){(C)}, 1)
#define CURRENT_LINE (buffer_head(&current_channel()->buffer)->text)

// TODO: tests for
// sending certain /commands to private buffer, server buffer

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

	/* Test empty command */
	INP_S(":");
	INP_C(0x0A);
	assert_strcmp(CURRENT_LINE, "Messages beginning with ':' require a command");

	/* Test control characters */
	INP_C(CTRL('c'));
	INP_C(CTRL('p'));
	INP_C(CTRL('x'));
	assert_strcmp(CURRENT_LINE, "Type :quit to exit rirc");

	INP_C(CTRL('l'));
	assert_ptr_null(buffer_head(&current_channel()->buffer));

	/* Test adding servers */
	struct server *s1 = server("h1", "p1", NULL, "u1", "r1");
	struct server *s2 = server("h2", "p2", NULL, "u2", "r2");
	struct server *s3 = server("h3", "p3", NULL, "u3", "r3");

	assert_ptr_eq(server_list_add(state_server_list(), s1), NULL);
	assert_ptr_eq(server_list_add(state_server_list(), s2), NULL);
	assert_ptr_eq(server_list_add(state_server_list(), s3), NULL);
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_state),
	};

	return run_tests(tests);
}
