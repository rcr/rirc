#include "test.h"

#include "../src/buffer.c"

#include <limits.h>

static char*
_fmt_int(int i)
{
	static char buff[1024] = {0};

	if ((snprintf(buff, sizeof(buff) - 1, "%d", i)) < 0)
		fail_test("snprintf");

	return buff;
}

static void
test_buffer_f(void)
{
	/* Test retrieving the first line after pushing to a full buffer */

	int i;

	struct buffer b = {0};

	assert_equals(buffer_size(&b), 0);

	for (i = 0; i < BUFFER_LINES_MAX + 1; i++)
		newline(&b, _fmt_int(i + 1));

	assert_strcmp(buffer_f(&b)->text, _fmt_int(BUFFER_LINES_MAX + 1));
	assert_equals(buffer_size(&b), BUFFER_LINES_MAX);
}

static void
test_buffer_l(void)
{
	/* Test retrieving the last line after pushing to a full buffer */

	int i;

	struct buffer b = {0};

	assert_equals(buffer_size(&b), 0);

	for (i = 0; i < BUFFER_LINES_MAX; i++)
		newline(&b, _fmt_int(i + 1));

	assert_strcmp(buffer_l(&b)->text, _fmt_int(1));
	assert_equals(buffer_size(&b), BUFFER_LINES_MAX);

	newline(&b, _fmt_int(i + 1));

	assert_strcmp(buffer_l(&b)->text, _fmt_int(2));
	assert_equals(buffer_size(&b), BUFFER_LINES_MAX);
}

static void
test_buffer_index_overflow(void)
{
	/* Test masked indexing after unsigned integer overflow */

	struct buffer b = {
		.head = UINT_MAX,
		.tail = UINT_MAX - 1
	};

	assert_equals(buffer_size(&b), 1);
	assert_equals(MASK(b.head), (BUFFER_LINES_MAX - 1));

	newline(&b, _fmt_int(0));

	assert_equals(buffer_size(&b), 2);
	assert_equals(MASK(b.head), 0);

	newline(&b, _fmt_int(-1));

	assert_equals(buffer_size(&b), 3);
	assert_strcmp(b.buffer_lines[0].text, _fmt_int(-1));
}

int
main(void)
{
	testcase tests[] = {
		&test_buffer_f,
		&test_buffer_l,
		&test_buffer_index_overflow
	};

	return run_tests(tests);
}
