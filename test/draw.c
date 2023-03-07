#include <limits.h>

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
t__buffer_newline(struct buffer *b, const char *t)
{
	/* Abstract newline with default values */

	buffer_newline(b, BUFFER_LINE_OTHER, "", t, 0, strlen(t), 0);
}

static void
test_draw_buffer_line_rows(void)
{
	/* Test calculating the number of rows a buffer line occupies */

	struct buffer b;

	buffer(&b);

	t__buffer_newline(&b, "aa bb cc");

	/* 1 column: 6 rows. word wrap skips whitespace prefix in line continuations:
	 * a
	 * a
	 * b
	 * b
	 * c
	 * c
	 * */
	assert_eq(draw_buffer_line_rows(buffer_head(&b), 1), 6);

	/* 4 columns: 3 rows:
	 * 'aa b' -> wraps to
	 *   'aa'
	 *   'bb c'
	 * 'bb c' -> wraps to
	 *   'bb'
	 *   'cc'
	 * */
	assert_eq(draw_buffer_line_rows(buffer_head(&b), 4), 3);

	/* Greater columns than length should always return one row */
	assert_eq(draw_buffer_line_rows(buffer_head(&b), buffer_head(&b)->text_len + 1), 1);

	/* Test empty line should return at least 1 row */
	t__buffer_newline(&b, "");

	assert_eq(draw_buffer_line_rows(buffer_head(&b), 1), 1);
}

static void
test_draw_buffer_scrollback_status(void)
{
	/* Test retrieving buffer scrollback status */

	char buf[4];
	struct buffer b;

	buffer(&b);

	b.head = 100;
	b.tail = 0;
	assert_ueq(buffer_size(&b), 100);

	/* test scrollback head in view */
	b.buffer_i_bot = (b.head - 1);
	assert_strcmp((draw_buffer_scrollback_status(&b, buf, sizeof(buf))), NULL);

	/* test scrollback tail in view */
	b.buffer_i_bot = b.tail;
	b.buffer_i_top = b.tail;
	assert_strcmp((draw_buffer_scrollback_status(&b, buf, sizeof(buf))), "100");

	/* test scrollback at half */
	b.buffer_i_bot = (b.head / 2);
	b.buffer_i_top = (b.head / 2) - 2;
	assert_strcmp((draw_buffer_scrollback_status(&b, buf, sizeof(buf))), "50");

	/* test buffer index wrapping */
	b.head = 999;
	b.tail = UINT_MAX - 1000;
	assert_ueq(buffer_size(&b), 2000);

	/* test scrollback head in view */
	b.scrollback = (b.head - 1);
	assert_strcmp((draw_buffer_scrollback_status(&b, buf, sizeof(buf))), NULL);

	/* test scrollback tail in view */
	b.buffer_i_bot = b.tail;
	b.buffer_i_top = b.tail;
	b.scrollback = (b.head - 2);
	assert_strcmp((draw_buffer_scrollback_status(&b, buf, sizeof(buf))), "100");

	/* test scrollback at half */
	b.buffer_i_bot = UINT_MAX;
	b.buffer_i_top = UINT_MAX - 2;
	assert_strcmp((draw_buffer_scrollback_status(&b, buf, sizeof(buf))), "50");

	b.head++;
	b.tail++;
	b.buffer_i_bot++;
	b.buffer_i_top++;
	assert_ueq(b.buffer_i_bot, 0);
	assert_ueq(b.buffer_i_top, UINT_MAX - 1);
	assert_strcmp((draw_buffer_scrollback_status(&b, buf, sizeof(buf))), "50");

	b.head++;
	b.tail++;
	b.buffer_i_bot++;
	b.buffer_i_top++;
	assert_ueq(b.buffer_i_bot, 1);
	assert_ueq(b.buffer_i_top, UINT_MAX);
	assert_strcmp((draw_buffer_scrollback_status(&b, buf, sizeof(buf))), "50");
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_draw_buffer_line_rows),
		TESTCASE(test_draw_buffer_scrollback_status),
	};

	return run_tests(NULL, NULL, tests);
}
