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

	/* Test empty line should return at least 1 row */
	t__buffer_newline(&b, "");

	assert_eq(draw_buffer_line_rows(buffer_head(&b), 1), 1);

	/* Test wraps are calculated */
	t__buffer_newline(&b, "aa bb cc");

	/* 1 column: 8 rows
	 * a
	 * a
	 *
	 * b
	 * b
	 *
	 * c
	 * c
	 * */
	assert_eq(draw_buffer_line_rows(buffer_head(&b), 1), 8);

	/* 4 columns: 3 rows:
	 * 'aa b' -> wraps to
	 *   'aa'
	 *   'bb cc'
	 * 'bb cc' -> wraps to
	 *   'bb'
	 *   'cc'
	 * */
	assert_eq(draw_buffer_line_rows(buffer_head(&b), 4), 3);

	/* Greater columns than length should always return one row */
	assert_eq(draw_buffer_line_rows(buffer_head(&b), buffer_head(&b)->text_len + 1), 1);
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

static void
test_draw_buffer_wrap(void)
{
	#define CHECK_WRAP(S1, COLS, S2) \
		assert_strcmp(((S1) + draw_buffer_wrap((S1), strlen((S1)), (COLS))), (S2));

	/* test empty string */
	assert_ueq(draw_buffer_wrap("", 0, 0), 0);
	assert_ueq(draw_buffer_wrap("", 0, 1), 0);
	assert_ueq(draw_buffer_wrap("", 0, 2), 0);

	/* test zero cols */
	assert_ueq(draw_buffer_wrap("",   0, 0), 0);
	assert_ueq(draw_buffer_wrap("a",  1, 0), 0);
	assert_ueq(draw_buffer_wrap("ab", 2, 0), 0);

	/* test all print */
	CHECK_WRAP("abcde", 1, "bcde");
	CHECK_WRAP("abcde", 2, "cde");
	CHECK_WRAP("abcde", 3, "de");
	CHECK_WRAP("abcde", 4, "e");
	CHECK_WRAP("abcde", 5, "");
	CHECK_WRAP("abcde", 6, "");

	/* test all space */
	CHECK_WRAP("     ", 1, "    ");
	CHECK_WRAP("     ", 2, "   ");
	CHECK_WRAP("     ", 3, "  ");
	CHECK_WRAP("     ", 4, " ");
	CHECK_WRAP("     ", 5, "");
	CHECK_WRAP("     ", 6, "");

	/* test all combinations for 'a bb  ccc' */
	CHECK_WRAP("a bb  ccc", 1,  " bb  ccc");
	CHECK_WRAP("a bb  ccc", 2,  "bb  ccc");
	CHECK_WRAP("a bb  ccc", 3,  "bb  ccc");
	CHECK_WRAP("a bb  ccc", 4,  "bb  ccc");
	CHECK_WRAP("a bb  ccc", 5,  "bb  ccc");
	CHECK_WRAP("a bb  ccc", 6,  "bb  ccc");
	CHECK_WRAP("a bb  ccc", 7,  "ccc");
	CHECK_WRAP("a bb  ccc", 8,  "ccc");
	CHECK_WRAP("a bb  ccc", 9,  "");
	CHECK_WRAP("a bb  ccc", 10, "");

	/* test leading space */
	CHECK_WRAP(" a bb", 1, "a bb");
	CHECK_WRAP(" a bb", 2, "a bb");
	CHECK_WRAP(" a bb", 3, "a bb");
	CHECK_WRAP(" a bb", 4, "bb");
	CHECK_WRAP(" a bb", 5, "");
	CHECK_WRAP(" a bb", 6, "");

	CHECK_WRAP("  a bb", 1, " a bb");
	CHECK_WRAP("  a bb", 2, "a bb");
	CHECK_WRAP("  a bb", 3, "a bb");
	CHECK_WRAP("  a bb", 4, "a bb");
	CHECK_WRAP("  a bb", 5, "bb");
	CHECK_WRAP("  a bb", 6, "");
	CHECK_WRAP("  a bb", 7, "");

	/* test trailing space */
	CHECK_WRAP("a bb ", 1, " bb ");
	CHECK_WRAP("a bb ", 2, "bb ");
	CHECK_WRAP("a bb ", 3, "bb ");
	CHECK_WRAP("a bb ", 4, "bb ");
	CHECK_WRAP("a bb ", 5, "");
	CHECK_WRAP("a bb ", 6, "");

	CHECK_WRAP("a bb  ", 1, " bb  ");
	CHECK_WRAP("a bb  ", 2, "bb  ");
	CHECK_WRAP("a bb  ", 3, "bb  ");
	CHECK_WRAP("a bb  ", 4, "bb  ");
	CHECK_WRAP("a bb  ", 5, "bb  ");
	CHECK_WRAP("a bb  ", 6, "");
	CHECK_WRAP("a bb  ", 7, "");

	#define ATTRS_ALL "\x02\x03""11,22\x1D\x11\x0F\x16\x1E\x1F"

	/* test all attrs, test empty string */
	assert_ueq(draw_buffer_wrap(ATTRS_ALL "", 0, 0), 0);
	assert_ueq(draw_buffer_wrap("" ATTRS_ALL, 0, 1), 0);
	assert_ueq(draw_buffer_wrap(ATTRS_ALL "", 0, 2), 0);

	/* test all attrs, test zero cols */
	assert_ueq(draw_buffer_wrap(ATTRS_ALL "",      0, 0), 0);
	assert_ueq(draw_buffer_wrap("a" ATTRS_ALL,     1, 0), 0);
	assert_ueq(draw_buffer_wrap("a" ATTRS_ALL "b", 2, 0), 0);

	/* test all attrs, test all print */
	CHECK_WRAP(ATTRS_ALL "abcde", 1, "bcde");
	CHECK_WRAP(ATTRS_ALL "abcde", 2, "cde");
	CHECK_WRAP(ATTRS_ALL "abcde", 3, "de");
	CHECK_WRAP(ATTRS_ALL "abcde", 4, "e");
	CHECK_WRAP(ATTRS_ALL "abcde", 5, "");
	CHECK_WRAP(ATTRS_ALL "abcde", 6, "");

	/* test all attrs, test all space */
	CHECK_WRAP(ATTRS_ALL "     ", 1, "    ");
	CHECK_WRAP(ATTRS_ALL "     ", 2, "   ");
	CHECK_WRAP(ATTRS_ALL "     ", 3, "  ");
	CHECK_WRAP(ATTRS_ALL "     ", 4, " ");
	CHECK_WRAP(ATTRS_ALL "     ", 5, "");
	CHECK_WRAP(ATTRS_ALL "     ", 6, "");

	/* test all attrs, test all combinations for 'a bb  ccc' */
	CHECK_WRAP(            ATTRS_ALL "a bb  ccc", 1,  " bb  ccc");
	CHECK_WRAP("a"         ATTRS_ALL " bb  ccc",  2,  "bb  ccc");
	CHECK_WRAP("a "        ATTRS_ALL "bb  ccc",   3,  "bb  ccc");
	CHECK_WRAP("a b"       ATTRS_ALL "b  ccc",    4,  "b" ATTRS_ALL "b  ccc");
	CHECK_WRAP("a bb"      ATTRS_ALL "  ccc",     5,  "bb" ATTRS_ALL "  ccc");
	CHECK_WRAP("a bb "     ATTRS_ALL " ccc",      6,  "bb " ATTRS_ALL " ccc");
	CHECK_WRAP("a bb  "    ATTRS_ALL "ccc",       7,  "ccc");
	CHECK_WRAP("a bb  c"   ATTRS_ALL "cc",        8,  "c" ATTRS_ALL "cc");
	CHECK_WRAP("a bb  cc"  ATTRS_ALL "c",         9,  "");
	CHECK_WRAP("a bb  ccc" ATTRS_ALL,             10, "");

	#undef CHECK_WRAP
}

static void
test_draw_irc_colour(void)
{
	int fg;
	int bg;

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("xxx", &fg, &bg), 0);
	assert_eq(fg, -2);
	assert_eq(bg, -2);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03", &fg, &bg), 1);
	assert_eq(fg, -1);
	assert_eq(bg, -1);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03xxx", &fg, &bg), 1);
	assert_eq(fg, -1);
	assert_eq(bg, -1);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03,", &fg, &bg), 1);
	assert_eq(fg, -1);
	assert_eq(bg, -1);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03,1", &fg, &bg), 3);
	assert_eq(fg, -2);
	assert_eq(bg, irc_to_ansi_colour[1]);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03,11", &fg, &bg), 4);
	assert_eq(fg, -2);
	assert_eq(bg, irc_to_ansi_colour[11]);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "2", &fg, &bg), 2);
	assert_eq(fg, irc_to_ansi_colour[2]);
	assert_eq(bg, -2);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "3,", &fg, &bg), 2);
	assert_eq(fg, irc_to_ansi_colour[3]);
	assert_eq(bg, -2);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "4,5", &fg, &bg), 4);
	assert_eq(fg, irc_to_ansi_colour[4]);
	assert_eq(bg, irc_to_ansi_colour[5]);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "6,77", &fg, &bg), 5);
	assert_eq(fg, irc_to_ansi_colour[6]);
	assert_eq(bg, irc_to_ansi_colour[77]);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "11", &fg, &bg), 3);
	assert_eq(fg, irc_to_ansi_colour[11]);
	assert_eq(bg, -2);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "22,", &fg, &bg), 3);
	assert_eq(fg, irc_to_ansi_colour[22]);
	assert_eq(bg, -2);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "33,4", &fg, &bg), 5);
	assert_eq(fg, irc_to_ansi_colour[33]);
	assert_eq(bg, irc_to_ansi_colour[4]);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "55,66", &fg, &bg), 6);
	assert_eq(fg, irc_to_ansi_colour[55]);
	assert_eq(bg, irc_to_ansi_colour[66]);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "111", &fg, &bg), 3);
	assert_eq(fg, irc_to_ansi_colour[11]);
	assert_eq(bg, -2);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "222,", &fg, &bg), 3);
	assert_eq(fg, irc_to_ansi_colour[22]);
	assert_eq(bg, -2);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "333,4,", &fg, &bg), 3);
	assert_eq(fg, irc_to_ansi_colour[33]);
	assert_eq(bg, -2);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "1,222", &fg, &bg), 5);
	assert_eq(fg, irc_to_ansi_colour[1]);
	assert_eq(bg, irc_to_ansi_colour[22]);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "22,333", &fg, &bg), 6);
	assert_eq(fg, irc_to_ansi_colour[22]);
	assert_eq(bg, irc_to_ansi_colour[33]);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "11,22", &fg, NULL), 6);
	assert_eq(fg, irc_to_ansi_colour[11]);
	assert_eq(bg, -2);

	fg = -2;
	bg = -2;
	assert_eq(draw_parse_irc_colour("\x03" "11,22", NULL, &bg), 6);
	assert_eq(fg, -2);
	assert_eq(bg, irc_to_ansi_colour[22]);

	assert_eq(draw_parse_irc_colour("\x03",         NULL, NULL), 1);
	assert_eq(draw_parse_irc_colour("\x03" "11",    NULL, NULL), 3);
	assert_eq(draw_parse_irc_colour("\x03" ",22",   NULL, NULL), 4);
	assert_eq(draw_parse_irc_colour("\x03" "11,22", NULL, NULL), 6);
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_draw_buffer_line_rows),
		TESTCASE(test_draw_buffer_scrollback_status),
		TESTCASE(test_draw_buffer_wrap),
		TESTCASE(test_draw_irc_colour),
	};

	return run_tests(NULL, NULL, tests);
}
