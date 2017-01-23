#include "test.h"

#include "../src/utils.c" /* FIXME: word_wrap */
#include "../src/buffer.c"

#include <limits.h>

static char*
_fmt_int(int i)
{
	static char buff[1024];

	if ((snprintf(buff, sizeof(buff) - 1, "%d", i)) < 0)
		fail_test("snprintf");

	return buff;
}

static void
_buffer_newline(struct buffer *b, const char *t)
{
	/* Abstract newline with default values */

	buffer_newline(b, BUFFER_LINE_OTHER, "", t);
}

static void
test_buffer(void)
{
	/* Test retrieving values from an initialized buffer and resetting it */

	struct buffer b = buffer(BUFFER_T_SIZE);

	assert_equals(b.type, BUFFER_T_SIZE);

	assert_equals(buffer_size(&b), 0);
	assert_null(buffer_head(&b));
	assert_null(buffer_tail(&b));
	assert_null(buffer_line(&b, b.scrollback));

	/* Reset the buffer, check values again */
	b = buffer(BUFFER_T_SIZE);

	assert_equals(b.type, BUFFER_T_SIZE);

	assert_equals(buffer_size(&b), 0);
	assert_null(buffer_head(&b));
	assert_null(buffer_tail(&b));
	assert_null(buffer_line(&b, b.scrollback));
}

static void
test_buffer_head(void)
{
	/* Test retrieving the first line after pushing to a full buffer */

	int i;

	struct buffer b = buffer(BUFFER_OTHER);

	assert_null(buffer_head(&b));

	for (i = 0; i < BUFFER_LINES_MAX + 1; i++)
		_buffer_newline(&b, _fmt_int(i + 1));

	assert_strcmp(buffer_head(&b)->text, _fmt_int(BUFFER_LINES_MAX + 1));
	assert_equals(buffer_size(&b), BUFFER_LINES_MAX);
}

static void
test_buffer_tail(void)
{
	/* Test retrieving the last line after pushing to a full buffer */

	int i;

	struct buffer b = buffer(BUFFER_OTHER);

	assert_null(buffer_tail(&b));

	for (i = 0; i < BUFFER_LINES_MAX; i++)
		_buffer_newline(&b, _fmt_int(i + 1));

	assert_strcmp(buffer_tail(&b)->text, _fmt_int(1));
	assert_equals(buffer_size(&b), BUFFER_LINES_MAX);

	_buffer_newline(&b, _fmt_int(i + 1));

	assert_strcmp(buffer_tail(&b)->text, _fmt_int(2));
	assert_equals(buffer_size(&b), BUFFER_LINES_MAX);
}

static void
test_buffer_line(void)
{
	/* Test that retrieving a buffer line fails when i != [tail, head) */

	struct buffer b = buffer(BUFFER_OTHER);

	/* Should retrieve null for an empty buffer */
	assert_equals(buffer_size(&b), 0);
	assert_null(buffer_line(&b, b.head));
	assert_null(buffer_line(&b, b.tail));
	assert_null(buffer_line(&b, b.scrollback));

	/* For any buffer line retrieval, these conditions should always hold */
	#define CHECK_BUFFER(B) \
	    assert_fatal(buffer_line(&(B), (B).tail - 1)); \
	    assert_fatal(buffer_line(&(B), (B).head));     \
	    (void)buffer_line(&(B), (B).tail);             \
	    (void)buffer_line(&(B), (B).tail + 1);         \
	    (void)buffer_line(&(B), (B).head - 1);         \

	/* Normal case:
	 * |-----T-----H-----|
	 *    a     b     c
	 *
	 * b    : valid
	 * a, c : invalid */

	b.tail = 1;
	b.head = 1 + BUFFER_LINES_MAX;
	CHECK_BUFFER(b);

	/* Inverted case:
	 * |-----H-----T-----|
	 *    a     b     c
	 *
	 * b    : invalid
	 * a, c : valid */

	b.tail = UINT_MAX - 1;
	b.head = UINT_MAX - 1 + BUFFER_LINES_MAX;
	CHECK_BUFFER(b);

	/* Edge case, head is 0
	 * |H-----------T---|
	 *        a       b
	 * a : invalid
	 * b : valid */

	b.tail = 0 - BUFFER_LINES_MAX;
	b.head = 0;
	CHECK_BUFFER(b);

	/* Edge case, head is UINT_MAX
	 * |---------T-----H|
	 *      a       b
	 * a : invalid
	 * b : valid */

	b.tail = UINT_MAX - BUFFER_LINES_MAX;
	b.head = UINT_MAX;
	CHECK_BUFFER(b);

	/* Edge case, tail is 0:
	 * |T---H-----------|
	 *    a       b
	 * a : valid
	 * b : invalid */

	b.tail = 0;
	b.head = 0 + BUFFER_LINES_MAX;
	CHECK_BUFFER(b);

	/* Edge case, tail is UINT_MAX
	 * |---H-----------T|
	 *   a      c      b
	 * a, b : valid
	 * c    : invalid */

	b.tail = UINT_MAX;
	b.head = UINT_MAX + BUFFER_LINES_MAX;
	CHECK_BUFFER(b);

	#undef CHECK_BUFFER
}

static void
test_buffer_scrollback(void)
{
	/* Test features of the buffer scrollback:
	 *   Empty buffer returns NULL for scrollback
	 *   Buffer scrollback stays locked to the head when incrementing
	 *   Buffer scrollback stays between [tail, head) when scrolled back
	 *   Buffer scrollback stays locked to the tail when incrementing
	 * */

	struct buffer b = buffer(BUFFER_OTHER);

	/* Empty buffer returns NULL */
	assert_null(buffer_line(&b, b.scrollback));

	/* Buffer scrollback stays locked to the buffer head when incrementing */
	_buffer_newline(&b, "a");
	assert_strcmp(buffer_line(&b, b.scrollback)->text, "a");

	_buffer_newline(&b, "b");
	assert_strcmp(buffer_line(&b, b.scrollback)->text, "b");

	_buffer_newline(&b, "c");
	assert_strcmp(buffer_line(&b, b.scrollback)->text, "c");

	/* Buffer scrollback stays between [tail, head) when scrolled back */
	b.scrollback = b.tail + 1;
	assert_strcmp(buffer_line(&b, b.scrollback)->text, "b");

	_buffer_newline(&b, "d");
	assert_strcmp(buffer_line(&b, b.scrollback)->text, "b");

	/* Buffer scrollback stays locked to the buffer tail when incrementing */
	b.head = b.tail + BUFFER_LINES_MAX;
	assert_true(buffer_full(&b));

	_buffer_newline(&b, "e");
	assert_strcmp(buffer_line(&b, b.scrollback)->text, "b");

	_buffer_newline(&b, "f");
	assert_strcmp(buffer_line(&b, b.scrollback)->text, "c");
}

static void
test_buffer_scrollback_status(void)
{
	/* Test retrieving buffer scrollback status */

	struct buffer b = buffer(BUFFER_OTHER);

	b.head = (BUFFER_LINES_MAX / 2) - 1;
	b.tail = UINT_MAX - (BUFFER_LINES_MAX / 2);
	b.scrollback = b.tail;

	assert_true(buffer_full(&b));

	b.scrollback = b.tail;
	assert_equals((int)(100 * buffer_scrollback_status(&b)), 100);

	b.scrollback = b.tail + (BUFFER_LINES_MAX / 2);
	assert_equals((int)(100 * buffer_scrollback_status(&b)), 50);

	b.scrollback = b.head - 1;
	assert_equals((int)(100 * buffer_scrollback_status(&b)), 0);
}

static void
test_buffer_index_overflow(void)
{
	/* Test masked indexing after unsigned integer overflow */

	struct buffer b = buffer(BUFFER_OTHER);

	b.head = UINT_MAX;
	b.tail = UINT_MAX - 1;
	b.scrollback = b.tail;

	assert_equals(buffer_size(&b), 1);
	assert_equals(MASK(b.head), (BUFFER_LINES_MAX - 1));

	_buffer_newline(&b, _fmt_int(0));

	assert_equals(buffer_size(&b), 2);
	assert_equals(MASK(b.head), 0);

	_buffer_newline(&b, _fmt_int(-1));

	assert_equals(buffer_size(&b), 3);
	assert_strcmp(b.buffer_lines[0].text, _fmt_int(-1));
}

static void
test_buffer_line_overlength(void)
{
	/* Test that lines over the maximum length are recursively split and added separately */

	struct buffer b = buffer(BUFFER_OTHER);

	/* Indices to first and last positions of lines, total length = 2.5 times the maximum */
	unsigned int f1 = 0,
	             l1 = TEXT_LENGTH_MAX - 1,
	             f2 = TEXT_LENGTH_MAX,
	             l2 = TEXT_LENGTH_MAX * 2 - 1,
	             f3 = TEXT_LENGTH_MAX * 2,
	             l3 = TEXT_LENGTH_MAX * 2 + TEXT_LENGTH_MAX / 2 - 1;

	/* Add a line that's 2.5 times the maximum length */
	char text[l3 + 1];

	memset(&text, ' ', sizeof(text) - 1);

	text[f1] = 'a';
	text[l1] = 'A';
	text[f2] = 'b';
	text[l2] = 'B';
	text[f3] = 'c';
	text[l3] = 'C';

	text[sizeof(text)] = 0;

	_buffer_newline(&b, text);

	assert_equals((int)b.buffer_lines[0].text_len, TEXT_LENGTH_MAX);
	assert_equals((int)b.buffer_lines[2].text_len, TEXT_LENGTH_MAX / 2);

	assert_equals(buffer_size(&b), 3);

	assert_equals(b.buffer_lines[0].text[0], 'a');
	assert_equals(b.buffer_lines[0].text[TEXT_LENGTH_MAX - 1], 'A');

	assert_equals(b.buffer_lines[1].text[0], 'b');
	assert_equals(b.buffer_lines[1].text[TEXT_LENGTH_MAX - 1], 'B');

	assert_equals(b.buffer_lines[2].text[0], 'c');
	assert_equals(b.buffer_lines[2].text[TEXT_LENGTH_MAX / 2 - 1], 'C');
}

static void
test_buffer_line_rows(void)
{
	/* Test calculating the number of rows a buffer line occupies */

	struct buffer b = buffer(BUFFER_OTHER);

	_buffer_newline(&b, "aa bb cc");

	/* 1 column: 6 rows. word wrap skips whitespace prefix in line continuations:
	 * a
	 * a
	 * b
	 * b
	 * c
	 * c
	 * */
	assert_equals(buffer_line_rows(buffer_head(&b), 1), 6);

	/* 4 columns: 3 rows:
	 * 'aa b' -> wraps to
	 *   'aa'
	 *   'bb c'
	 * 'bb c' -> wraps to
	 *   'bb'
	 *   'cc'
	 * */
	assert_equals(buffer_line_rows(buffer_head(&b), 4), 3);

	/* Greater columns than length should always return one row */
	assert_equals(buffer_line_rows(buffer_head(&b), buffer_head(&b)->text_len + 1), 1);

	/* Test empty line should return at least 1 row */
	_buffer_newline(&b, "");

	assert_equals(buffer_line_rows(buffer_head(&b), 1), 1);
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_buffer),
		TESTCASE(test_buffer_head),
		TESTCASE(test_buffer_tail),
		TESTCASE(test_buffer_line),
		TESTCASE(test_buffer_scrollback),
		TESTCASE(test_buffer_scrollback_status),
		TESTCASE(test_buffer_index_overflow),
		TESTCASE(test_buffer_line_overlength),
		TESTCASE(test_buffer_line_rows),
	};

	return run_tests(tests);
}
