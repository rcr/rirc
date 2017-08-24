#include "test.h"

#include "../src/mode.c"

static void
test_mode_get_prefix(void)
{
	struct mode_config m = {
		.PREFIX.F = "abc",
		.PREFIX.T = "123"
	};

	/* Test lower mode flag doesn't take prescedence */
	assert_eq(mode_get_prefix(&m, '1', 'c'), '1');

	/* Test higher mode flag takes prescedence */
	assert_eq(mode_get_prefix(&m, '3', 'b'), '2');

	/* Test abscent prefix */
	assert_eq(mode_get_prefix(&m, 0, 'b'), '2');

	/* Test new mode not in PREFIX mode_config */
	assert_eq(mode_get_prefix(&m, '3', 'd'), '3');

	/* Test abscent prefix and new mode not in PREFIX mode_config */
	assert_eq(mode_get_prefix(&m, 0, 'd'), 0);
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_mode_get_prefix),
	};

	return run_tests(tests);
}
