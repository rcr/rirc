#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../src/utils.c"

int _assert_strcmp(char*, char*);

#define fail_test(M) \
	do { \
		failures++; \
		printf("\t%s %d: " M "\n", __func__, __LINE__); \
	} while (0)

#define fail_testf(...) \
	do { \
		failures++; \
		printf("\t%s %d: ", __func__, __LINE__); \
		printf(__VA_ARGS__); \
		printf("\n"); \
	} while (0)

#define assert_strcmp(X, Y) \
	do { \
		if (_assert_strcmp(X, Y)) \
			fail_testf(#X " expected '%s', got '%s'", (Y) ? (Y) : "NULL", (X) ? (X) : "NULL"); \
	} while (0)

#define assert_equals(X, Y) \
	do { \
		if ((X) != (Y)) \
			fail_testf(#X " expected '%d', got '%d'", (Y), (X)); \
	} while (0)

int
_assert_strcmp(char *p1, char *p2)
{
	if (p1 == NULL || p2 == NULL)
		return p1 != p2;

	return strcmp(p1, p2);
}

/* Recusrive functions for testing AVL tree properties */

int
_avl_count(avl_node *n)
{
	/* Count the number of nodes in a tree */

	if (n == NULL)
		return 0;

	return 1 + _avl_count(n->l) + _avl_count(n->r);
}

int
_avl_is_binary(avl_node *n)
{
	if (n == NULL)
		return 1;

	if (n->l && (strcmp(n->key, n->l->key) <= 0))
		return 0;

	if (n->r && (strcmp(n->key, n->r->key) >= 0))
		return 0;

	return 1 & _avl_is_binary(n->l) & _avl_is_binary(n->r);
}

int
_avl_height(avl_node *n)
{
	if (n == NULL)
		return 0;

	return 1 + MAX(_avl_height(n->l), _avl_height(n->r));
}

/*
 * Tests
 * */

int test_avl(void);
int test_parse(void);
int test_check_pinged(void);
int test_word_wrap(void);
int test_count_line_rows(void);
int test_auto_nick(void);


int
test_avl(void)
{
	/* Test AVL tree functions */

	int failures = 0;

	avl_node *root = NULL;

	/* Insert strings a-z, zz-za, aa-az to hopefully excersize all combinations of rotations */
	const char **ptr, *strings[] = {
		"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
		"n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
		"zz", "zy", "zx", "zw", "zv", "zu", "zt", "zs", "zr", "zq", "zp", "zo", "zn",
		"zm", "zl", "zk", "zj", "zi", "zh", "zg", "zf", "ze", "zd", "zc", "zb", "za",
		"aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj", "ak", "al", "am",
		"an", "ao", "ap", "aq", "ar", "as", "at", "au", "av", "aw", "ax", "ay", "az",
		NULL
	};

	int ret, count = 0;

	/* Add all strings to the tree */
	for (ptr = strings; *ptr; ptr++) {
		if (!avl_add(&root, *ptr, NULL))
			fail_testf("avl_add() failed to add %s", *ptr);
		else
			count++;
	}

	/* Check that all were added correctly */
	if ((ret = _avl_count(root)) != count)
		fail_testf("_avl_count() returned %d, expected %d", ret, count);

	/* Check that the binary properties of the tree hold */
	if (!_avl_is_binary(root))
		fail_test("_avl_is_binary() failed");

	/* Check that the height of root stays within the mathematical bounds AVL trees allow */
	double max_height = 1.44 * log2(count + 2) - 0.328;

	if ((ret = _avl_height(root)) >= max_height)
		fail_testf("_avl_height() returned %d, expected strictly less than %f", ret, max_height);

	/* Test adding a duplicate and case sensitive duplicate */
	if (avl_add(&root, "aa", NULL) && count++)
		fail_test("avl_add() failed to detect duplicate 'aa'");

	if (avl_add(&root, "aA", NULL) && count++)
		fail_test("avl_add() failed to detect case sensitive duplicate 'aA'");

	/* Delete about half of the strings */
	int num_delete = count / 2;

	for (ptr = strings; *ptr && num_delete > 0; ptr++, num_delete--) {
		if (!avl_del(&root, *ptr))
			fail_testf("avl_del() failed to delete %s", *ptr);
		else
			count--;
	}

	/* Check that all were deleted correctly */
	if ((ret = _avl_count(root)) != count)
		fail_testf("_avl_count() returned %d, expected %d", ret, count);

	/* Check that the binary properties of the tree still hold */
	if (!_avl_is_binary(root))
		fail_test("_avl_is_binary() failed");

	/* Check that the height of root is still within the mathematical bounds AVL trees allow */
	max_height = 1.44 * log2(count + 2) - 0.328;

	if ((ret = _avl_height(root)) >= max_height)
		fail_testf("_avl_height() returned %d, expected strictly less than %f", ret, max_height);

	/* Test deleting string that was previously deleted */
	if (avl_del(&root, *strings))
		fail_testf("_avl_del() should have failed to delete %s", *strings);

	return failures;
}

int
test_getarg(void)
{
	/* Test string token parsing */

	int failures = 0;

	char *ptr;

	/* Test null pointer */
	assert_strcmp(getarg(NULL, ' '), NULL);

	/* Test empty string */
	char str1[] = "";

	ptr = str1;
	assert_strcmp(getarg(&ptr, ' '), NULL);

	/* Test only whitestapce */
	char str2[] = "   ";

	ptr = str2;
	assert_strcmp(getarg(&ptr, ' '), NULL);

	/* Test single token */
	char str3[] = "arg1";

	ptr = str3;
	assert_strcmp(getarg(&ptr, ' '), "arg1");
	assert_strcmp(getarg(&ptr, ' '), NULL);

	/* Test multiple tokens */
	char str4[] = "arg2 arg3 arg4";

	ptr = str4;
	assert_strcmp(getarg(&ptr, ' '), "arg2");
	assert_strcmp(getarg(&ptr, ' '), "arg3");
	assert_strcmp(getarg(&ptr, ' '), "arg4");
	assert_strcmp(getarg(&ptr, ' '), NULL);

	/* Test multiple tokens with extraneous whitespace */
	char str5[] = "   arg5   arg6   arg7   ";

	ptr = str5;
	assert_strcmp(getarg(&ptr, ' '), "arg5");
	assert_strcmp(getarg(&ptr, ' '), "arg6");
	assert_strcmp(getarg(&ptr, ' '), "arg7");
	assert_strcmp(getarg(&ptr, ' '), NULL);

	if (failures)
		printf("\t%d failure%c\n", failures, (failures > 1) ? 's' : 0);

	return failures;
}

int
test_parse(void)
{
	/* Test the IRC message parsing function */

	int failures = 0;

	parsed_mesg p;

	/* Test ordinary message */
	char mesg1[] = ":nick!user@hostname.domain CMD args :trailing";

	if ((parse(&p, mesg1)) == NULL)
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "args");
	assert_strcmp(p.trailing, "trailing");

	/* Test no nick/host */
	char mesg2[] = "CMD arg1 arg2 :  trailing message  ";

	if ((parse(&p, mesg2)) == NULL)
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     NULL);
	assert_strcmp(p.hostinfo, NULL);
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2");
	assert_strcmp(p.trailing, "  trailing message  ");

	/* Test the 15 arg limit */
	char mesg3[] = "CMD a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 :trailing message";

	if ((parse(&p, mesg3)) == NULL)
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     NULL);
	assert_strcmp(p.hostinfo, NULL);
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14");
	assert_strcmp(p.trailing, "a15 :trailing message");

	/* Test ':' can exist in args */
	char mesg4[] = ":nick!user@hostname.domain CMD arg:1:2:3 arg:4:5:6 :trailing message";

	if ((parse(&p, mesg4)) == NULL)
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg:1:2:3 arg:4:5:6");
	assert_strcmp(p.trailing, "trailing message");

	/* Test no args */
	char mesg5[] = ":nick!user@hostname.domain CMD :trailing message";

	if ((parse(&p, mesg5)) == NULL)
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   NULL);
	assert_strcmp(p.trailing, "trailing message");

	/* Test no trailing */
	char mesg6[] = ":nick!user@hostname.domain CMD arg1 arg2 arg3";

	if ((parse(&p, mesg6)) == NULL)
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2 arg3");
	assert_strcmp(p.trailing, NULL);

	/* Test no user */
	char mesg7[] = ":nick@hostname.domain CMD arg1 arg2 arg3";

	if ((parse(&p, mesg7)) == NULL)
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2 arg3");
	assert_strcmp(p.trailing, NULL);

	/* Error: empty message */
	char mesg8[] = "";

	if ((parse(&p, mesg8)) != NULL)
		fail_test("parse() was expected to fail");

	/* Error: no command */
	char mesg9[] = ":nick!user@hostname.domain";

	if ((parse(&p, mesg9)) != NULL)
		fail_test("parse() was expected to fail");

	if (failures)
		printf("\t%d failure%c\n", failures, (failures > 1) ? 's' : 0);

	return failures;
}

int
test_check_pinged(void)
{
	/* Test detecting user's nick in message */

	int failures = 0;

	char *nick = "testnick";

	/* Test message contains username */
	char *mesg1 = "testing testnick testing";
	assert_equals(check_pinged(mesg1, nick), 1);

	/* Test common way of addressing messages to users */
	char *mesg2 = "testnick: testing";
	assert_equals(check_pinged(mesg2, nick), 1);

	/* Test non-nick char prefix */
	char *mesg3 = "testing !@#testnick testing";
	assert_equals(check_pinged(mesg3, nick), 1);

	/* Test non-nick char suffix */
	char *mesg4 = "testing testnick!@#$ testing";
	assert_equals(check_pinged(mesg4, nick), 1);

	/* Test non-nick char prefix and suffix */
	char *mesg5 = "testing !testnick! testing";
	assert_equals(check_pinged(mesg5, nick), 1);

	/* Error: message doesn't contain username */
	char *mesg6 = "testing testing";
	assert_equals(check_pinged(mesg6, nick), 0);

	/* Error: message contains username prefix */
	char *mesg7 = "testing testnickshouldfail testing";
	assert_equals(check_pinged(mesg7, nick), 0);

	if (failures)
		printf("\t%d failure%c\n", failures, (failures > 1) ? 's' : 0);

	return failures;
}

int
test_word_wrap(void)
{
	/* TODO */
	return 0;
}

int
test_count_line_rows(void)
{
	/* TODO */
	return 0;
}

int
test_auto_nick(void)
{
	/* TODO */
	return 0;
}

int
main(void)
{
	printf(__FILE__":\n");

	int failures = 0;

	failures += test_avl();
	failures += test_parse();
	failures += test_getarg();
	failures += test_check_pinged();
	failures += test_word_wrap();
	failures += test_count_line_rows();
	failures += test_auto_nick();

	if (failures) {
		printf("%d failure%c total\n\n", failures, (failures > 1) ? 's' : 0);
		exit(EXIT_FAILURE);
	}

	printf("OK\n\n");

	return EXIT_SUCCESS;
}
