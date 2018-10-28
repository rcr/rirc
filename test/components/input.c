#include "test/test.h"

/* Preclude definitions for testing */
#define INPUT_LEN_MAX 16
#define INPUT_HIST_MAX 4

#include "src/components/input.c"

static uint16_t completion_l(char*, uint16_t, uint16_t, int);
static uint16_t completion_m(char*, uint16_t, uint16_t, int);
static uint16_t completion_s(char*, uint16_t, uint16_t, int);
static uint16_t completion_rot1(char*, uint16_t, uint16_t, int);

static uint16_t
completion_l(char *str, uint16_t len, uint16_t max, int first)
{
	/* Completes to word longer than len */

	(void)len;
	(void)first;

	const char longer[] = "xyxyxy";

	if (max < sizeof(longer) - 1)
		return 0;

	memcpy(str, longer, sizeof(longer) - 1);

	return sizeof(longer) - 1;
}

static uint16_t
completion_m(char *str, uint16_t len, uint16_t max, int first)
{
	/* Writes up to max chars */

	(void)first;

	for (uint16_t i = 0; i < (len + max); i++)
		str[i] = 'x';

	return (len + max);
}

static uint16_t
completion_s(char *str, uint16_t len, uint16_t max, int first)
{
	/* Completes to word shorter than len */

	(void)len;
	(void)first;

	const char shorter[] = "z";

	if (max < sizeof(shorter) - 1)
		return 0;

	memcpy(str, shorter, sizeof(shorter) - 1);

	return sizeof(shorter) - 1;
}

static uint16_t
completion_rot1(char *str, uint16_t len, uint16_t max, int first)
{
	/* Completetion function, increments all characters */

	uint16_t i = 0;

	while (i < len && i < max)
		str[i++] += 1;

	if (first) {
		str[i++] = '!';
		str[i++] = '!';
	}

	return i;
}

static void
test_input(void)
{
	char buf[INPUT_LEN_MAX + 1];
	struct input inp;

	input(&inp);

	assert_eq(input_text_iszero(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "");

	input_free(&inp);
}

static void
test_input_reset(void)
{
	struct input inp;

	input(&inp);

	/* Test clearing empty input */
	assert_eq(input_reset(&inp), 0);
	assert_eq(input_text_iszero(&inp), 1);

	/* Test clearing non-empty input */
	assert_eq(input_insert(&inp, "abc", 3), 1);
	assert_eq(input_reset(&inp), 1);
	assert_eq(input_text_iszero(&inp), 1);

	/* Test clearing non-empty input, cursor at start */
	assert_eq(input_insert(&inp, "abc", 3), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 0);
	assert_eq(input_reset(&inp), 1);
	assert_eq(input_text_iszero(&inp), 1);

	input_free(&inp);
}

static void
test_input_ins(void)
{
	char buf[INPUT_LEN_MAX + 1];
	struct input inp;

	input(&inp);

	/* Valid */
	assert_eq(input_insert(&inp, "a", 1), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "a");
	assert_eq(input_insert(&inp, "bc", 2), 1);
	assert_eq(input_insert(&inp, "de", 2), 1);
	assert_eq(input_insert(&inp, "fgh", 3), 1);
	assert_eq(input_insert(&inp, "i", 1), 1);
	assert_eq(input_insert(&inp, "j", 1), 1);
	assert_eq(input_insert(&inp, "klmnop", 6), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "abcdefghijklmnop");
	assert_eq((int)input_text_size(&inp), INPUT_LEN_MAX);

	/* Full */
	assert_eq(input_insert(&inp, "z", 1), 0);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "abcdefghijklmnop");
	assert_eq((int)input_text_size(&inp), INPUT_LEN_MAX);

	input_free(&inp);
}

static void
test_input_del(void)
{
	char buf[INPUT_LEN_MAX + 1];
	struct input inp;

	input(&inp);

	/* Deleting back/forw on empty input */
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "");
	assert_eq(input_delete_back(&inp), 0);
	assert_eq(input_delete_forw(&inp), 0);

	assert_eq(input_insert(&inp, "abcefg", 6), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);

	/* Delete left */
	assert_eq(input_delete_back(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "acefg");
	assert_eq(input_delete_back(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "cefg");
	assert_eq(input_delete_back(&inp), 0);

	/* Delete right */
	assert_eq(input_delete_forw(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "efg");
	assert_eq(input_delete_forw(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "fg");
	assert_eq(input_delete_forw(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "g");
	assert_eq(input_delete_forw(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "");
	assert_eq(input_delete_forw(&inp), 0);

	input_free(&inp);
}

static void
test_input_hist(void)
{
	char buf[INPUT_LEN_MAX + 1];
	struct input inp;

	input(&inp);

	assert_eq(input_hist_push(&inp), 0);

	/* Test scrolling input fails when no history */
	assert_eq(input_hist_back(&inp), 0);
	assert_eq(input_hist_forw(&inp), 0);

	/* Test pushing clears the working input */
	assert_eq(input_insert(&inp, "111", 3), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "111");
	assert_eq(input_hist_push(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "");

	/* Test pushing up to INPUT_HIST_MAX */
	assert_eq(input_insert(&inp, "222", 3), 1);
	assert_eq(input_hist_push(&inp), 1);
	assert_eq(input_insert(&inp, "333", 3), 1);
	assert_eq(input_hist_push(&inp), 1);
	assert_eq(input_insert(&inp, "444", 3), 1);
	assert_eq(input_hist_push(&inp), 1);

#define INP_HIST_CURR(I) ((I).hist.ptrs[INPUT_MASK((I).hist.current)])
#define INP_HIST_HEAD(I) ((I).hist.ptrs[INPUT_MASK((I).hist.head - 1)])
#define INP_HIST_TAIL(I) ((I).hist.ptrs[INPUT_MASK((I).hist.tail)])

	assert_strcmp(INP_HIST_HEAD(inp), "444");
	assert_strcmp(INP_HIST_TAIL(inp), "111");

	/* Test pushing after INPUT_HIST_MAX frees the tail */
	assert_eq(input_insert(&inp, "555", 3), 1);
	assert_eq(input_hist_push(&inp), 1);

	assert_strcmp(INP_HIST_HEAD(inp), "555");
	assert_strcmp(INP_HIST_TAIL(inp), "222");

	/* Test scrolling back saves the current working input */
	assert_eq(input_insert(&inp, "000", 3), 1);

	/* Test scrolling back to tail */
	assert_eq(input_hist_back(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "555");
	assert_eq(input_hist_back(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "444");
	assert_eq(input_hist_back(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "333");
	assert_eq(input_hist_back(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "222");
	assert_eq(input_hist_back(&inp), 0);
	assert_strcmp(inp.hist.save, "000");

	/* Test scrolling forw to head */
	assert_eq(input_hist_forw(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "333");
	assert_eq(input_hist_forw(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "444");
	assert_eq(input_hist_forw(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "555");
	assert_eq(input_hist_forw(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "000");
	assert_eq(input_hist_forw(&inp), 0);

	assert_strcmp(INP_HIST_HEAD(inp), "555");
	assert_strcmp(INP_HIST_TAIL(inp), "222");

	/* Test replaying hist head */
	assert_eq(input_hist_back(&inp), 1);
	assert_eq(input_hist_push(&inp), 1);
	assert_strcmp(INP_HIST_HEAD(inp), "555");
	assert_strcmp(INP_HIST_TAIL(inp), "222");

	/* Test replaying hist middle */
	assert_eq(input_hist_back(&inp), 1);
	assert_eq(input_hist_back(&inp), 1);
	assert_eq(input_hist_push(&inp), 1);
	assert_strcmp(INP_HIST_HEAD(inp), "444");
	assert_strcmp(INP_HIST_TAIL(inp), "222");

	/* Test replaying hist tail */
	assert_eq(input_hist_back(&inp), 1);
	assert_eq(input_hist_back(&inp), 1);
	assert_eq(input_hist_back(&inp), 1);
	assert_eq(input_hist_back(&inp), 1);
	assert_eq(input_hist_back(&inp), 0);
	assert_eq(input_hist_push(&inp), 1);
	assert_strcmp(INP_HIST_HEAD(inp), "222");
	assert_strcmp(INP_HIST_TAIL(inp), "333");

#undef INP_HIST_CURR
#undef INP_HIST_HEAD
#undef INP_HIST_TAIL

	input_free(&inp);
}

static void
test_input_move(void)
{
	char buf[INPUT_LEN_MAX + 1];
	struct input inp;

	input(&inp);

	/* Test move back */
	assert_eq(input_insert(&inp, "ab", 2), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "ab");
	assert_eq(input_cursor_back(&inp), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "ab");
	assert_eq(input_insert(&inp, "c", 1), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "acb");
	assert_eq(input_insert(&inp, "d", 1), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "acdb");
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 0);
	assert_eq(input_insert(&inp, "e", 1), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "eacdb");

	/* Test move forward */
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_insert(&inp, "f", 1), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "eacdfb");
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 0);
	assert_eq(input_insert(&inp, "g", 1), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "eacdfbg");

	input_free(&inp);
}

static void
test_input_write(void)
{
	char buf1[INPUT_LEN_MAX + 1];
	char buf2[INPUT_LEN_MAX / 2];
	struct input inp;

	input(&inp);

	/* Test output is written correctly regardless of cursor position */
	assert_eq(input_insert(&inp, "abcde", 5), 1);
	assert_strcmp(input_write(&inp, buf1, sizeof(buf1)), "abcde");
	assert_eq(input_cursor_back(&inp), 1);
	assert_strcmp(input_write(&inp, buf1, sizeof(buf1)), "abcde");
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_strcmp(input_write(&inp, buf1, sizeof(buf1)), "abcde");
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 0);
	assert_strcmp(input_write(&inp, buf1, sizeof(buf1)), "abcde");

	/* Test output is always null terminated */
	assert_eq(input_insert(&inp, "fghijklmno", 10), 1);
	assert_strcmp(input_write(&inp, buf1, sizeof(buf1)), "fghijklmnoabcde");
	assert_strcmp(input_write(&inp, buf2, sizeof(buf2)), "fghijkl");

	input_free(&inp);
}

static void
test_input_complete(void)
{
	char buf[INPUT_LEN_MAX + 1];
	struct input inp;

	input(&inp);

	/* Test empty */
	assert_eq(input_complete(&inp, completion_rot1), 0);
	assert_eq(input_reset(&inp), 0);

	/* Test only space */
	assert_eq(input_insert(&inp, " ", 1), 1);
	assert_eq(input_complete(&inp, completion_rot1), 0);
	assert_eq(input_reset(&inp), 1);

	/* Test: ` abc `
	 *             ^ */
	assert_eq(input_insert(&inp, " abc ", 5), 1);
	assert_eq(input_complete(&inp, completion_rot1), 0);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), " abc ");

	/* Test: ` abc `
	 *            ^ */
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_eq(input_complete(&inp, completion_rot1), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), " bcd ");

	/* Test: ` bcd `
	 *           ^ */
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], 'd');
	assert_eq(input_complete(&inp, completion_rot1), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), " cde ");

	/* Test: ` cde `
	 *          ^ */
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], 'd');
	assert_eq(input_complete(&inp, completion_rot1), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), " def ");

	/* Test: ` def `
	 *         ^ */
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], 'd');
	assert_eq(input_complete(&inp, completion_rot1), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), " efg ");

	/* Test: ` efg `
	 *        ^ */
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_eq(input_complete(&inp, completion_rot1), 0);
	assert_eq(inp.text[inp.head], ' ');
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), " efg ");
	assert_eq(input_reset(&inp), 1);

	/* Test start of line */
	assert_eq(input_insert(&inp, "x abc ", 6), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], 'x');
	assert_eq(input_complete(&inp, completion_rot1), 1);
	assert_eq(inp.text[inp.tail], ' ');
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "y!! abc ");
	assert_eq(input_reset(&inp), 1);

	/* Test replacement word longer */
	assert_eq(input_insert(&inp, " abc ab", 7), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], 'c');
	assert_eq(input_complete(&inp, completion_l), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), " xyxyxy ab");
	assert_eq(inp.text[inp.tail], ' '); /* points to 'c' */
	assert_eq(input_reset(&inp), 1);

	/* Test replacement word shorter */
	assert_eq(input_insert(&inp, " abc ab ", 8), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], 'c');
	assert_eq(input_complete(&inp, completion_s), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), " z ab ");
	assert_eq(inp.text[inp.tail], ' ');
	assert_eq(input_reset(&inp), 1);

	/* Test writing up to max chars */
	assert_eq(input_insert(&inp, "a", 1), 1);
	assert_eq(input_complete(&inp, completion_m), 1);
	assert_strcmp(input_write(&inp, buf, sizeof(buf)), "xxxxxxxxxxxxxxxx");
	assert_eq((int)input_text_size(&inp), INPUT_LEN_MAX);

	input_free(&inp);
}

static void
test_input_text_size(void)
{
	struct input inp;

	input(&inp);

	/* Test size is correct from 0 -> max */
	for (int i = 0; i < INPUT_LEN_MAX; i++) {
		assert_eq((int)input_text_size(&inp), i);
		assert_eq(input_insert(&inp, "a", 1), 1);
	}

	assert_eq((int)input_text_size(&inp), INPUT_LEN_MAX);

	/* Test size is correct regardless of cursor position */
	for (int i = 0; i < INPUT_LEN_MAX; i++) {
		assert_eq(input_cursor_back(&inp), 1);
		assert_eq((int)input_text_size(&inp), INPUT_LEN_MAX);
	}

	input_free(&inp);
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_input),
		TESTCASE(test_input_reset),
		TESTCASE(test_input_ins),
		TESTCASE(test_input_del),
		TESTCASE(test_input_hist),
		TESTCASE(test_input_move),
		TESTCASE(test_input_write),
		TESTCASE(test_input_complete),
		TESTCASE(test_input_text_size)
	};

	return run_tests(tests);
}
