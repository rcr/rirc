#include "test/test.h"

#include "src/handlers/irc_recv.c"

static void
test_STUB(void)
{
	; /* TODO */
}


int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_STUB)
	};

	return run_tests(tests);
}
