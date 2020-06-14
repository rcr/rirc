#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/ircv3.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/draw.c"
#include "src/state.c"
#include "src/utils/utils.c"

#include "test/handlers/irc_recv.mock.c"
#include "test/handlers/irc_send.mock.c"
#include "test/io.mock.c"
#include "test/rirc.mock.c"

static void
test_STUB(void)
{
	; /* TODO */
}


int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_STUB)
	};

	return run_tests(tests);
}
