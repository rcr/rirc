#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/ircv3.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/rirc.c"
#include "src/state.c"
#include "src/utils/utils.c"

#include "test/draw.mock.c"
#include "test/handlers/irc_recv.mock.c"
#include "test/handlers/irc_send.mock.c"
#include "test/io.mock.c"

static void
test_rirc_parse_args(void)
{
	/* TODO */
	(void)rirc_parse_args;
}

int
main(void)
{
	/* FIXME: */
	(void)rirc_pw_name;

	struct testcase tests[] = {
		TESTCASE(test_rirc_parse_args)
	};

	return run_tests(NULL, NULL, tests);
}
