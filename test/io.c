#include "test/test.h"

static void
test_dummy(void)
{
	test_pass();
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_dummy)
	};

	return run_tests(NULL, NULL, tests);
}
