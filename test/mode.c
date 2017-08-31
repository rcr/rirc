#include "test.h"

#include "../src/mode.c"

static void
test_char_mode(void)
{
	/* Test mode->char, char->mode */
	assert_eq(char_mode('!'), -1);

	assert_eq(char_mode('a'), 0);
	assert_eq(char_mode('z'), MODE_LEN / 2 - 1);
	assert_eq(char_mode('A'), MODE_LEN / 2);
	assert_eq(char_mode('Z'), MODE_LEN - 1);

	assert_eq(mode_char(0),                'a');
	assert_eq(mode_char(MODE_LEN / 2 - 1), 'z');
	assert_eq(mode_char(MODE_LEN / 2),     'A');
	assert_eq(mode_char(MODE_LEN - 1),     'Z');
}

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

static void
test_usermode_set(void)
{
	struct usermode usermode = {0};

	/* Test invalid mode flag */
	assert_eq(usermode_set(&usermode, '!'), 1);

	/* Test valid mode flags */
	assert_eq(usermode_set(&usermode, 'Z'), 0);
	assert_strcmp(usermode.str, "Z");

	assert_eq(usermode_set(&usermode, 'a'), 0);
	assert_strcmp(usermode.str, "aZ");

	assert_eq(usermode_set(&usermode, 'z'), 0);
	assert_strcmp(usermode.str, "azZ");

	assert_eq(usermode_set(&usermode, 'A'), 0);
	assert_strcmp(usermode.str, "azAZ");
}

static void
test_usermode_unset(void)
{
	struct usermode usermode = {0};

	/* Test invalid mode flag */
	assert_eq(usermode_unset(&usermode, '!'), 1);

	/* Test valid mode flags */
	assert_eq(usermode_set(&usermode, 'Z'), 0);
	assert_eq(usermode_set(&usermode, 'a'), 0);
	assert_eq(usermode_set(&usermode, 'z'), 0);
	assert_eq(usermode_set(&usermode, 'A'), 0);

	assert_eq(usermode_unset(&usermode, 'A'), 0);
	assert_strcmp(usermode.str, "azZ");

	assert_eq(usermode_unset(&usermode, 'z'), 0);
	assert_strcmp(usermode.str, "aZ");

	assert_eq(usermode_unset(&usermode, 'a'), 0);
	assert_strcmp(usermode.str, "Z");

	assert_eq(usermode_unset(&usermode, 'Z'), 0);
	assert_strcmp(usermode.str, "");
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_char_mode),
		TESTCASE(test_mode_get_prefix),
		TESTCASE(test_usermode_set),
		TESTCASE(test_usermode_unset),
	};

	return run_tests(tests);
}
