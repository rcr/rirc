/* TODO: input2 -> input */

#include "test/test.h"

/* Preclude definition for testing */
#define INPUT_LEN_MAX 10

#include "src/components/input2.c"

static void
test_input(void)
{
	char buf[INPUT_LEN_MAX + 1];
	struct input2 inp;

	input2(&inp);

	assert_eq(input2_empty(&inp), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "");
}

static void
test_input_ins(void)
{
	char buf[INPUT_LEN_MAX + 1];
	struct input2 inp;

	input2(&inp);

	/* Valid */
	assert_eq(input2_ins(&inp, "a", 1), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "a");
	assert_eq(input2_ins(&inp, "bc", 2), 1);
	assert_eq(input2_ins(&inp, "de", 2), 1);
	assert_eq(input2_ins(&inp, "fgh", 3), 1);
	assert_eq(input2_ins(&inp, "i", 1), 1);
	assert_eq(input2_ins(&inp, "j", 1), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "abcdefghij");

	/* Full */
	assert_eq(input2_ins(&inp, "z", 1), 0);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "abcdefghij");
}

static void
test_input_del(void)
{
	char buf[INPUT_LEN_MAX + 1];
	struct input2 inp;

	input2(&inp);

	/* Deleting back/forw on empty input */
	assert_eq(input2_del(&inp, 0), 0);
	assert_eq(input2_del(&inp, 1), 0);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "");

	assert_eq(input2_ins(&inp, "abcef", 5), 1);
	assert_eq(input2_move(&inp, 0), 1);
	assert_eq(input2_move(&inp, 0), 1);

	/* Delete left */
	assert_eq(input2_del(&inp, 0), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "abef");

	/* Delete right */
	assert_eq(input2_del(&inp, 1), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "abf");
}

static void
test_input_move(void)
{
	char buf[INPUT_LEN_MAX + 1];
	struct input2 inp;

	input2(&inp);

	/* Test move back */
	assert_eq(input2_ins(&inp, "ab", 2), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "ab");
	assert_eq(input2_move(&inp, 0), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "ab");
	assert_eq(input2_ins(&inp, "c", 1), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "acb");
	assert_eq(input2_ins(&inp, "d", 1), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "acdb");
	assert_eq(input2_move(&inp, 0), 1);
	assert_eq(input2_move(&inp, 0), 1);
	assert_eq(input2_move(&inp, 0), 1);
	assert_eq(input2_move(&inp, 0), 0);
	assert_eq(input2_ins(&inp, "e", 1), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "eacdb");

	/* Test move forward */
	assert_eq(input2_move(&inp, 1), 1);
	assert_eq(input2_move(&inp, 1), 1);
	assert_eq(input2_move(&inp, 1), 1);
	assert_eq(input2_ins(&inp, "f", 1), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "eacdfb");
	assert_eq(input2_move(&inp, 1), 1);
	assert_eq(input2_move(&inp, 1), 0);
	assert_eq(input2_ins(&inp, "g", 1), 1);
	assert_strcmp(input2_write(&inp, buf, sizeof(buf)), "eacdfbg");
}

static void
test_input_write(void)
{
	char buf1[INPUT_LEN_MAX + 1];
	char buf2[INPUT_LEN_MAX / 2];
	struct input2 inp;

	input2(&inp);

	/* Test output is written correctly regardless of cursor position */
	assert_eq(input2_ins(&inp, "abcde", 5), 1);
	assert_strcmp(input2_write(&inp, buf1, sizeof(buf1)), "abcde");
	assert_eq(input2_move(&inp, 0), 1);
	assert_strcmp(input2_write(&inp, buf1, sizeof(buf1)), "abcde");
	assert_eq(input2_move(&inp, 0), 1);
	assert_eq(input2_move(&inp, 0), 1);
	assert_eq(input2_move(&inp, 0), 1);
	assert_strcmp(input2_write(&inp, buf1, sizeof(buf1)), "abcde");
	assert_eq(input2_move(&inp, 0), 1);
	assert_eq(input2_move(&inp, 0), 0);
	assert_strcmp(input2_write(&inp, buf1, sizeof(buf1)), "abcde");

	/* Test output is always null terminated */
	assert_eq(input2_ins(&inp, "fghij", 5), 1);
	assert_strcmp(input2_write(&inp, buf1, sizeof(buf1)), "fghijabcde");
	assert_strcmp(input2_write(&inp, buf1, sizeof(buf2)), "fghi");
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_input),
		TESTCASE(test_input_ins),
		TESTCASE(test_input_del),
		TESTCASE(test_input_move),
		TESTCASE(test_input_write)
	};

	return run_tests(tests);
}
