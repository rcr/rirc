#include "test/test.h"
#include "src/components/ircv3_cap.c"

static void
test_TODO(void)
{
	;
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_TODO),
	};

	return run_tests(tests);
}
