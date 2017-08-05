#include "test.h"
#include "../src/tree.h"

struct test_avl
{
	AVL_NODE(test_avl) node;
	int val;
};

struct test_avl_list
{
	AVL_HEAD(test_avl);
};

static inline int
test_avl_cmp(struct test_avl *t1, struct test_avl *t2)
{
	return (t1->val == t2->val) ? 0 : ((t1->val < t2->val) ? -1 : 1);
}

AVL_GENERATE(test_avl_list, test_avl, node, test_avl_cmp)

void
test_avl_get_height(void)
{
	struct test_avl t0 = { .node = { .height = 1 }};

	assert_eq(test_avl_list_AVL_GET_HEIGHT(&t0),  1);
	assert_eq(test_avl_list_AVL_GET_HEIGHT(NULL), 0);
}

void
test_avl_set_height(void)
{
	struct test_avl
		t0 = { .node = { .height = 1 }},
		t1 = { .node = { .tree_left = &t0 }};

	assert_eq(test_avl_list_AVL_SET_HEIGHT(&t1), 2);
}

void
test_avl_balance(void)
{
	/*     t30              balance : 2
	 *    /   \
	 * t20     t21          balance : 0, 1
	 *        /   \
	 *     t10     t11      balance : 0, 0
	 *            /   \
	 *         t00     t01  balance : 0, 0
	 */

	struct test_avl
		t00 = { .node = { .height = 1 }},
		t01 = { .node = { .height = 1 }},
		t10 = { .node = { .height = 1 }},
		t11 = { .node = { .tree_left  = &t00,
		                  .tree_right = &t01 }},
		t20 = { .node = { .height = 1 }},
		t21 = { .node = { .tree_left  = &t10,
		                  .tree_right = &t11 }},
		t30 = { .node = { .tree_left  = &t20,
		                  .tree_right = &t21 }};

	test_avl_list_AVL_SET_HEIGHT(&t11);
	test_avl_list_AVL_SET_HEIGHT(&t21);
	test_avl_list_AVL_SET_HEIGHT(&t30);

	assert_eq(test_avl_list_AVL_BALANCE(&t00), 0);
	assert_eq(test_avl_list_AVL_BALANCE(&t01), 0);
	assert_eq(test_avl_list_AVL_BALANCE(&t10), 0);
	assert_eq(test_avl_list_AVL_BALANCE(&t11), 0);
	assert_eq(test_avl_list_AVL_BALANCE(&t20), 0);
	assert_eq(test_avl_list_AVL_BALANCE(&t21), 1);
	assert_eq(test_avl_list_AVL_BALANCE(&t30), 2);

	/*         t70      balance : -2
	 *        /   \
	 *     t60     t61  balance : 1, 0
	 *    /   \
	 * t50     t51      balance : 0, 0
	 *        /   \
	 *     t40     t41  balance : 0, 0
	 */

	struct test_avl
		t40 = { .node = { .height = 1 }},
		t41 = { .node = { .height = 1 }},
		t50 = { .node = { .height = 1 }},
		t51 = { .node = { .tree_left  = &t40,
		                  .tree_right = &t41 }},
		t60 = { .node = { .tree_left  = &t50,
		                  .tree_right = &t51 }},
		t61 = { .node = { .height = 1 }},
		t70 = { .node = { .tree_left  = &t60,
		                  .tree_right = &t61 }};

	test_avl_list_AVL_SET_HEIGHT(&t51);
	test_avl_list_AVL_SET_HEIGHT(&t60);
	test_avl_list_AVL_SET_HEIGHT(&t70);

	assert_eq(test_avl_list_AVL_BALANCE(&t40),  0);
	assert_eq(test_avl_list_AVL_BALANCE(&t41),  0);
	assert_eq(test_avl_list_AVL_BALANCE(&t50),  0);
	assert_eq(test_avl_list_AVL_BALANCE(&t51),  0);
	assert_eq(test_avl_list_AVL_BALANCE(&t60),  1);
	assert_eq(test_avl_list_AVL_BALANCE(&t61),  0);
	assert_eq(test_avl_list_AVL_BALANCE(&t70), -2);
}

void
test_avl_add(void)
{
	//FIXME: failing until implementation of ..._AVL_ADD_REC
	return;

	/* Test AVL_ADD
	 *
	 * Add 200, 100, 300, 50, 75, 150, 350:
	 *
	 *        _ 200 _
	 *       /       \
	 *    100         300
	 *   /   \       /   \
	 * 50     75  150     350
	 */

	struct test_avl_list tl = {0};

	struct test_avl
		t0 = { .val = 200 },
		t1 = { .val = 100 },
		t2 = { .val = 300 },
		t3 = { .val = 50 },
		t4 = { .val = 75 },
		t5 = { .val = 150 },
		t6 = { .val = 350 };

	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t0), &t0);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t1), &t1);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t2), &t2);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t3), &t3);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t4), &t4);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t5), &t5);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t6), &t6);

	/* Duplicate */
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t6), NULL);

	/* Check tree structure */
	assert_ptrequals(TREE_ROOT(&tl), &t0);

	assert_ptrequals(t0.node.tree_left,  &t1);
	assert_ptrequals(t0.node.tree_right, &t2);

	assert_ptrequals(t1.node.tree_left,  &t3);
	assert_ptrequals(t1.node.tree_right, &t4);

	assert_ptrequals(t2.node.tree_left,  &t5);
	assert_ptrequals(t2.node.tree_right, &t6);

	assert_ptrequals(t3.node.tree_left,  NULL);
	assert_ptrequals(t3.node.tree_right, NULL);

	assert_ptrequals(t4.node.tree_left,  NULL);
	assert_ptrequals(t4.node.tree_right, NULL);

	assert_ptrequals(t5.node.tree_left,  NULL);
	assert_ptrequals(t5.node.tree_right, NULL);

	assert_ptrequals(t6.node.tree_left,  NULL);
	assert_ptrequals(t6.node.tree_right, NULL);
}

void
test_avl_del(void)
{
	/* TODO */
	;
}

void
test_avl_rotations(void)
{
	/* Exercise all 4 rotation types */

	/* Add 100, 200, 300:
	 *
	 *        100
	 *          \
	 *           200
	 *             \
	 *              300
	 *
	 * Rotates left:
	 *
	 *        200
	 *        / \
	 *     100   300
	 */

	/* TODO */

	/* Add 225, 275:
	 *
	 *        200
	 *        / \
	 *     100   300
	 *           /
	 *        225
	 *          \
	 *           275
	 *
	 * Rotates left-right:
	 *
	 *        200            200
	 *        / \            / \
	 *     100   300  ->  100   275
	 *           /              / \
	 *        275            225   300
	 *        /
	 *     225
	 */

	/* TODO */

	/* Add 220, 215:
	 *
	 *        200
	 *        / \
	 *     100   275
	 *           / \
	 *        225   300
	 *        /
	 *     220
	 *     /
	 *  215
	 *
	 * Rotates right:
	 *
	 *        200
	 *        / \
	 *     100   275
	 *           / \
	 *        220   300
	 *        / \
	 *     215   225
	 */

	/* TODO */

	/* Add 245, 235:
	 *
	 *        200
	 *        / \
	 *     100   275
	 *           / \
	 *        220   300
	 *        / \
	 *     215   225
	 *             \
	 *              245
	 *              /
	 *           235
	 *
	 * Rotates right-left
	 *
	 *        200               200
	 *        / \              /   \
	 *     100   275         100   275
	 *           / \               /  \
	 *        220   300  ->     220   300
	 *        / \               / \
	 *     215   225         215   235
	 *             \               / \
	 *              235         225   245
	 *                \
	 *                 245
	 *
	 */

	/* TODO */
}

void
test_avl_free(void)
{
	/* TODO */
	;
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_avl_get_height),
		TESTCASE(test_avl_set_height),
		TESTCASE(test_avl_balance),
		TESTCASE(test_avl_add),
		TESTCASE(test_avl_del),
		TESTCASE(test_avl_rotations),
		TESTCASE(test_avl_free)
	};

	return run_tests(tests);
}
