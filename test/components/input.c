#include "test/test.h"

/* Preclude definitions for testing */
#define INPUT_LEN_MAX 16
#define INPUT_HIST_MAX 4

#include "src/components/input.c"

static char* t_input_write(struct input*, size_t);
static uint16_t completion_l(char*, uint16_t, uint16_t, int);
static uint16_t completion_m(char*, uint16_t, uint16_t, int);
static uint16_t completion_s(char*, uint16_t, uint16_t, int);
static uint16_t completion_rot1(char*, uint16_t, uint16_t, int);

static char*
t_input_write(struct input *inp, size_t len)
{
	static char buf[INPUT_LEN_MAX + 1];
	input_write(inp, buf, (len ? len : sizeof(buf)));
	return buf;
}

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
test_input_init(void)
{
	struct input inp;

	input_init(&inp);
	assert_eq(input_text_iszero(&inp), 1);
	input_free(&inp);
}

static void
test_input_reset(void)
{
	struct input inp;

	input_init(&inp);

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
	struct input inp;

	input_init(&inp);

	/* Valid */
	assert_eq(input_insert(&inp, "a", 1), 1);
	assert_strcmp(t_input_write(&inp, 0), "a");
	assert_eq(input_insert(&inp, "bc", 2), 1);
	assert_eq(input_insert(&inp, "de", 2), 1);
	assert_eq(input_insert(&inp, "fgh", 3), 1);
	assert_eq(input_insert(&inp, "i", 1), 1);
	assert_eq(input_insert(&inp, "j", 1), 1);
	assert_eq(input_insert(&inp, "klmnop", 6), 1);
	assert_strcmp(t_input_write(&inp, 0), "abcdefghijklmnop");
	assert_eq((int)input_text_size(&inp), INPUT_LEN_MAX);

	/* Full */
	assert_eq(input_insert(&inp, "z", 1), 0);
	assert_strcmp(t_input_write(&inp, 0), "abcdefghijklmnop");
	assert_eq((int)input_text_size(&inp), INPUT_LEN_MAX);

	input_free(&inp);
}

static void
test_input_del(void)
{
	struct input inp;

	input_init(&inp);

	/* Deleting back/forw on empty input */
	assert_strcmp(t_input_write(&inp, 0), "");
	assert_eq(input_delete_back(&inp), 0);
	assert_eq(input_delete_forw(&inp), 0);

	assert_eq(input_insert(&inp, "abcefg", 6), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);

	/* Delete left */
	assert_eq(input_delete_back(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "acefg");
	assert_eq(input_delete_back(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "cefg");
	assert_eq(input_delete_back(&inp), 0);

	/* Delete right */
	assert_eq(input_delete_forw(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "efg");
	assert_eq(input_delete_forw(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "fg");
	assert_eq(input_delete_forw(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "g");
	assert_eq(input_delete_forw(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "");
	assert_eq(input_delete_forw(&inp), 0);

	input_free(&inp);
}

static void
test_input_hist(void)
{
	struct input inp;

	input_init(&inp);

	assert_eq(input_hist_push(&inp), 0);

	/* Test scrolling input fails when no history */
	assert_eq(input_hist_back(&inp), 0);
	assert_eq(input_hist_forw(&inp), 0);

	/* Test pushing clears the working input */
	assert_eq(input_insert(&inp, "111", 3), 1);
	assert_strcmp(t_input_write(&inp, 0), "111");
	assert_eq(input_hist_push(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "");

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
	assert_strcmp(t_input_write(&inp, 0), "555");
	assert_eq(input_hist_back(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "444");
	assert_eq(input_hist_back(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "333");
	assert_eq(input_hist_back(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "222");
	assert_eq(input_hist_back(&inp), 0);
	assert_strcmp(inp.hist.save, "000");

	/* Test scrolling forw to head */
	assert_eq(input_hist_forw(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "333");
	assert_eq(input_hist_forw(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "444");
	assert_eq(input_hist_forw(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "555");
	assert_eq(input_hist_forw(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "000");
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
	struct input inp;

	input_init(&inp);

	/* Test move back */
	assert_eq(input_insert(&inp, "ab", 2), 1);
	assert_strcmp(t_input_write(&inp, 0), "ab");
	assert_eq(input_cursor_back(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "ab");
	assert_eq(input_insert(&inp, "c", 1), 1);
	assert_strcmp(t_input_write(&inp, 0), "acb");
	assert_eq(input_insert(&inp, "d", 1), 1);
	assert_strcmp(t_input_write(&inp, 0), "acdb");
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 0);
	assert_eq(input_insert(&inp, "e", 1), 1);
	assert_strcmp(t_input_write(&inp, 0), "eacdb");

	/* Test move forward */
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_insert(&inp, "f", 1), 1);
	assert_strcmp(t_input_write(&inp, 0), "eacdfb");
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 0);
	assert_eq(input_insert(&inp, "g", 1), 1);
	assert_strcmp(t_input_write(&inp, 0), "eacdfbg");

	input_free(&inp);
}

static void
test_input_frame(void)
{
	struct input inp;

	input_init(&inp);

	/* Test cursor fits */
	assert_eq(input_insert(&inp, "1234567890", 10), 1);
	assert_eq(input_frame(&inp, 11), 10);
	assert_strcmp(t_input_write(&inp, 0), "1234567890");

	/* Test cursor doesnt fit */
	assert_eq(input_frame(&inp, 10), 6);
	assert_strcmp(t_input_write(&inp, 0), "567890");

	/* Test cursor back keeps cursor in view */
	assert_eq(input_frame(&inp, 5), 3);
	assert_strcmp(t_input_write(&inp, 6), "890");
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_frame(&inp, 5), 3);
	assert_strcmp(t_input_write(&inp, 6), "56789");
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_frame(&inp, 5), 3);
	assert_strcmp(t_input_write(&inp, 6), "23456");
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_frame(&inp, 5), 0);
	assert_strcmp(t_input_write(&inp, 6), "12345");

	/* Test cursor forw keeps cursor in view */
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_frame(&inp, 5), 3);
	assert_strcmp(t_input_write(&inp, 6), "12345");
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_frame(&inp, 5), 3);
	assert_strcmp(t_input_write(&inp, 6), "45678");
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_cursor_forw(&inp), 1);
	assert_eq(input_frame(&inp, 5), 3);
	assert_strcmp(t_input_write(&inp, 6), "890");

	input_free(&inp);
}

static void
test_input_write(void)
{
	struct input inp;

	input_init(&inp);

	/* Test output is written correctly regardless of cursor position */
	assert_eq(input_insert(&inp, "abcde", 5), 1);
	assert_strcmp(t_input_write(&inp, 0), "abcde");
	assert_eq(input_cursor_back(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "abcde");
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_strcmp(t_input_write(&inp, 0), "abcde");
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 0);
	assert_strcmp(t_input_write(&inp, 0), "abcde");

	/* Test output is always null terminated */
	assert_eq(input_insert(&inp, "fghijklmno", 10), 1);
	assert_strcmp(t_input_write(&inp, 0), "fghijklmnoabcde");
	assert_strcmp(t_input_write(&inp, INPUT_LEN_MAX / 2), "fghijkl");

	input_free(&inp);
}

static void
test_input_complete(void)
{
	struct input inp;

	input_init(&inp);

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
	assert_strcmp(t_input_write(&inp, 0), " abc ");

	/* Test: ` abc `
	 *            ^ */
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_eq(input_complete(&inp, completion_rot1), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_strcmp(t_input_write(&inp, 0), " bcd ");

	/* Test: ` bcd `
	 *           ^ */
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], 'd');
	assert_eq(input_complete(&inp, completion_rot1), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_strcmp(t_input_write(&inp, 0), " cde ");

	/* Test: ` cde `
	 *          ^ */
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], 'd');
	assert_eq(input_complete(&inp, completion_rot1), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_strcmp(t_input_write(&inp, 0), " def ");

	/* Test: ` def `
	 *         ^ */
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], 'd');
	assert_eq(input_complete(&inp, completion_rot1), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_strcmp(t_input_write(&inp, 0), " efg ");

	/* Test: ` efg `
	 *        ^ */
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], ' ');
	assert_eq(input_complete(&inp, completion_rot1), 0);
	assert_eq(inp.text[inp.head], ' ');
	assert_strcmp(t_input_write(&inp, 0), " efg ");
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
	assert_strcmp(t_input_write(&inp, 0), "y!! abc ");
	assert_eq(input_reset(&inp), 1);

	/* Test replacement word longer */
	assert_eq(input_insert(&inp, " abc ab", 7), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(input_cursor_back(&inp), 1);
	assert_eq(inp.text[inp.head], 'c');
	assert_eq(input_complete(&inp, completion_l), 1);
	assert_strcmp(t_input_write(&inp, 0), " xyxyxy ab");
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
	assert_strcmp(t_input_write(&inp, 0), " z ab ");
	assert_eq(inp.text[inp.tail], ' ');
	assert_eq(input_reset(&inp), 1);

	/* Test writing up to max chars */
	assert_eq(input_insert(&inp, "a", 1), 1);
	assert_eq(input_complete(&inp, completion_m), 1);
	assert_strcmp(t_input_write(&inp, 0), "xxxxxxxxxxxxxxxx");
	assert_eq((int)input_text_size(&inp), INPUT_LEN_MAX);

	input_free(&inp);
}

static void
test_input_text_size(void)
{
	struct input inp;

	input_init(&inp);

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
		TESTCASE(test_input_init),
		TESTCASE(test_input_reset),
		TESTCASE(test_input_ins),
		TESTCASE(test_input_del),
		TESTCASE(test_input_hist),
		TESTCASE(test_input_move),
		TESTCASE(test_input_frame),
		TESTCASE(test_input_write),
		TESTCASE(test_input_complete),
		TESTCASE(test_input_text_size)
	};

	return run_tests(tests);
}
