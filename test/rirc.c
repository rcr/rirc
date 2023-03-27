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
test_dummy(void)
{
	test_pass();
}

int
main(void)
{
	/* FIXME: */
	(void)rirc_pw_name;
	(void)rirc_parse_args;

	struct testcase tests[] = {
		TESTCASE(test_dummy)
	};

	return run_tests(NULL, NULL, tests);
}
