#include "test.h"
#include "../src/avl.c"

/*
 * Util functions for testing AVL properties
 * */

static int
_avl_count(avl_node *n)
{
	/* Count the number of nodes in a tree */

	if (n == NULL)
		return 0;

	return 1 + _avl_count(n->l) + _avl_count(n->r);
}

static int
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

static int
_avl_height(avl_node *n)
{
	if (n == NULL)
		return 0;

	return 1 + MAX(_avl_height(n->l), _avl_height(n->r));
}

/*
 * Tests
 * */

void
test_avl(void)
{
	/* Test AVL tree functions */

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

	/* Hardcode caluculated maximum heigh of avl tree, avoid importing math libs */
	double min_height; /* log_2(n + 1) */
	double max_height; /* log_2(n + 2) * 1.618 - 0.328 */

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
	assert_equals(count, 78); /* Required for hardcoded log2 calculations */
	min_height = 6.303;                /* log2(78 + 1) ~= 6.303 */
	max_height = 6.321 * 1.44 - 0.328; /* log2(78 + 2) ~= 6.321 */

	ret = _avl_height(root);

	if (ret < min_height)
		fail_testf("_avl_height() returned %d, expected greater than %f", ret, min_height);

	if (ret >= max_height)
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

	/* Check that the height of root stays within the mathematical bounds AVL trees allow */
	assert_equals(count, 39); /* Required for hardcoded log2 calculations */
	min_height = 5.321;                /* log2(39 + 1) ~= 5.321 */
	max_height = 5.357 * 1.44 - 0.328; /* log2(39 + 2) ~= 5.357 */

	ret = _avl_height(root);

	if (ret < min_height)
		fail_testf("_avl_height() returned %d, expected greater than %f", ret, min_height);

	if (ret >= max_height)
		fail_testf("_avl_height() returned %d, expected strictly less than %f", ret, max_height);

	if ((ret = _avl_height(root)) >= max_height)
		fail_testf("_avl_height() returned %d, expected strictly less than %f", ret, max_height);

	/* Test deleting string that was previously deleted */
	if (avl_del(&root, *strings))
		fail_testf("_avl_del() should have failed to delete %s", *strings);
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_avl)
	};

	return run_tests(tests);
}
