#include "test.h"

#include "../src/buffer.c"

#include <limits.h>

static void
test_buffer_f(void)
{
	/* Test retrieving the first line after pushing to a full buffer */

	int i;

	struct buffer b = {
		.head = 0,
		.tail = 0,
		.vals = {0}
	};

	assert_equals(buffer_size(&b), 0);

	for (i = 0; i < BUFFER_LINES_MAX + 1; i++)
		buffer_push(&b, i + 1);

	assert_equals(buffer_f(&b), BUFFER_LINES_MAX + 1);
	assert_equals(buffer_size(&b), BUFFER_LINES_MAX);
}

static void
test_buffer_l(void)
{
	/* Test retrieving the last line after pushing to a full buffer */

	int i;

	struct buffer b = {
		.head = 0,
		.tail = 0,
		.vals = {0}
	};

	assert_equals(buffer_size(&b), 0);

	for (i = 0; i < BUFFER_LINES_MAX; i++)
		buffer_push(&b, i + 1);


	assert_equals(buffer_l(&b), 1);
	assert_equals(buffer_size(&b), BUFFER_LINES_MAX);

	buffer_push(&b, i + 1);

	assert_equals(buffer_l(&b), 2);
	assert_equals(buffer_size(&b), BUFFER_LINES_MAX);
}

static void
test_buffer_index_overflow(void)
{
	/* Test masked indexing after unsigned integer overflow */

	struct buffer b = {
		.head = UINT_MAX,
		.tail = UINT_MAX - 1,
		.vals = {0}
	};

	assert_equals(buffer_size(&b), 1);
	assert_equals(MASK(b.head), (BUFFER_LINES_MAX - 1));

	buffer_push(&b, 0);

	assert_equals(buffer_size(&b), 2);
	assert_equals(MASK(b.head), 0);

	buffer_push(&b, -1);

	assert_equals(buffer_size(&b), 3);
	assert_equals(b.vals[0], -1);
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
