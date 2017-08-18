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
	return (t1->val == t2->val) ? 0 : ((t1->val > t2->val) ? 1 : -1);
}

static inline int
test_avl_cmp_n(struct test_avl *t1, struct test_avl *t2, size_t n)
{
	int tmp = t1->val * n;

	return (tmp == t2->val) ? 0 : ((tmp > t2->val) ? 1 : -1);
}

static void
foreach_f(struct test_avl *t)
{
	t->val = 0;
	t->node.tree_left = NULL;
	t->node.tree_right = NULL;
}

AVL_GENERATE(test_avl_list, test_avl, node, test_avl_cmp, test_avl_cmp_n)

static void
test_avl_get_height(void)
{
	struct test_avl t0 = { .node = { .height = 1 }};

	assert_eq(test_avl_list_AVL_GET_HEIGHT(&t0),  1);
	assert_eq(test_avl_list_AVL_GET_HEIGHT(NULL), 0);
}

static void
test_avl_set_height(void)
{
	struct test_avl
		t0 = { .node = { .height = 1 }},
		t1 = { .node = { .tree_left = &t0 }};

	assert_eq(test_avl_list_AVL_SET_HEIGHT(&t1), 2);
}

static void
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

	assert_eq(test_avl_list_AVL_BALANCE(&t40),  0);
	assert_eq(test_avl_list_AVL_BALANCE(&t41),  0);
	assert_eq(test_avl_list_AVL_BALANCE(&t50),  0);
	assert_eq(test_avl_list_AVL_BALANCE(&t51),  0);
	assert_eq(test_avl_list_AVL_BALANCE(&t60),  1);
	assert_eq(test_avl_list_AVL_BALANCE(&t61),  0);
	assert_eq(test_avl_list_AVL_BALANCE(&t70), -2);
}

static void
test_avl_add(void)
{
	/* Test AVL_ADD
	 *
	 * Add 200, 100, 300, 50, 150, 250, 350:
	 *
	 *        _ 200 _
	 *       /       \
	 *    100         300
	 *   /   \       /   \
	 * 50     150 250     350
	 */

	struct test_avl_list tl = {0};

	struct test_avl
		t0 = { .val = 200 },
		t1 = { .val = 100 },
		t2 = { .val = 300 },
		t3 = { .val = 50 },
		t4 = { .val = 150 },
		t5 = { .val = 250 },
		t6 = { .val = 350 };

	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t0), &t0);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t1), &t1);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t2), &t2);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t3), &t3);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t4), &t4);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t5), &t5);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t6), &t6);

	/* Duplicate */
	assert_null(test_avl_list_AVL_ADD(&tl, &t6));

	/* Check tree structure */
	assert_ptrequals(TREE_ROOT(&tl), &t0);

	assert_ptrequals(t0.node.tree_left,  &t1);
	assert_ptrequals(t0.node.tree_right, &t2);

	assert_ptrequals(t1.node.tree_left,  &t3);
	assert_ptrequals(t1.node.tree_right, &t4);

	assert_ptrequals(t2.node.tree_left,  &t5);
	assert_ptrequals(t2.node.tree_right, &t6);

	assert_null(t3.node.tree_left);
	assert_null(t3.node.tree_right);

	assert_null(t4.node.tree_left);
	assert_null(t4.node.tree_right);

	assert_null(t5.node.tree_left);
	assert_null(t5.node.tree_right);

	assert_null(t6.node.tree_left);
	assert_null(t6.node.tree_right);

	/* Retrieve the nodes */
	assert_ptrequals(test_avl_list_AVL_GET(&tl, &t0), &t0);
	assert_ptrequals(test_avl_list_AVL_GET(&tl, &t1), &t1);
	assert_ptrequals(test_avl_list_AVL_GET(&tl, &t2), &t2);
	assert_ptrequals(test_avl_list_AVL_GET(&tl, &t3), &t3);
	assert_ptrequals(test_avl_list_AVL_GET(&tl, &t4), &t4);
	assert_ptrequals(test_avl_list_AVL_GET(&tl, &t5), &t5);
	assert_ptrequals(test_avl_list_AVL_GET(&tl, &t6), &t6);

	struct test_avl t7 = { .val = -1 };

	assert_null(test_avl_list_AVL_GET(&tl, &t7));
}

static void
test_avl_del(void)
{
	/* Test AVL_DEL
	 *
	 * Add 200, 100, 300, 50, 150, 250, 350:
	 *
	 *        _ 200 _
	 *       /       \
	 *    100         300
	 *   /   \       /   \
	 * 50     150 250     350
	 */

	struct test_avl_list tl = {0};

	struct test_avl
		t200 = { .val = 200 },
		t100 = { .val = 100 },
		t300 = { .val = 300 },
		t050 = { .val = 50 },
		t150 = { .val = 150 },
		t250 = { .val = 250 },
		t350 = { .val = 350 },
		t0 = { .val = 0 };

	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t200), &t200);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t100), &t100);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t300), &t300);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t050), &t050);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t150), &t150);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t250), &t250);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t350), &t350);

	/* Test deleting node not found in tree */
	assert_ptrequals(test_avl_list_AVL_DEL(&tl, &t0), NULL);

	/* Delete 200; In-order successor is substituted from leaf
	 *
	 *        _ 250 _
	 *       /       \
	 *    100         300
	 *   /   \           \
	 * 50     150        350
	 */

	assert_ptrequals(test_avl_list_AVL_DEL(&tl, &t200), &t200);

	/* Check tree structure */
	assert_ptrequals(TREE_ROOT(&tl), &t250);

	assert_ptrequals(t250.node.tree_left, &t100);
	assert_ptrequals(t250.node.tree_right, &t300);

	assert_ptrequals(t100.node.tree_left, &t050);
	assert_ptrequals(t100.node.tree_right, &t150);

	assert_null(t300.node.tree_left);
	assert_ptrequals(t300.node.tree_right, &t350);

	assert_null(t050.node.tree_left);
	assert_null(t050.node.tree_right);

	assert_null(t150.node.tree_left);
	assert_null(t150.node.tree_right);

	assert_null(t350.node.tree_left);
	assert_null(t350.node.tree_right);

	/* Delete 250; In-order successor with no left-subtree is substituted
	 *
	 *        _ 300 _
	 *       /       \
	 *    100         350
	 *   /   \
	 * 50     150
	 *
	 */

	assert_ptrequals(test_avl_list_AVL_DEL(&tl, &t250), &t250);

	/* Check tree structure */
	assert_ptrequals(TREE_ROOT(&tl), &t300);

	assert_ptrequals(t300.node.tree_left, &t100);
	assert_ptrequals(t300.node.tree_right, &t350);

	assert_ptrequals(t100.node.tree_left, &t050);
	assert_ptrequals(t100.node.tree_right, &t150);

	assert_null(t050.node.tree_left);
	assert_null(t050.node.tree_right);

	assert_null(t150.node.tree_left);
	assert_null(t150.node.tree_right);

	assert_null(t350.node.tree_left);
	assert_null(t350.node.tree_right);

	/* Delete 300; No successor, tree is rotated
	 *
	 *        350
	 *       /
	 *    100
	 *   /   \
	 * 50     150
	 *
	 * ->
	 *        100
	 *       /   \
	 *     50     350
	 *           /
	 *        150
	 */

	assert_ptrequals(test_avl_list_AVL_DEL(&tl, &t300), &t300);

	/* Check tree structure */
	assert_ptrequals(TREE_ROOT(&tl), &t100);

	assert_ptrequals(t100.node.tree_left, &t050);
	assert_ptrequals(t100.node.tree_right, &t350);

	assert_null(t050.node.tree_left);
	assert_null(t050.node.tree_right);

	assert_ptrequals(t350.node.tree_left, &t150);
	assert_null(t350.node.tree_right);

	assert_null(t150.node.tree_left);
	assert_null(t150.node.tree_right);

	/* Delete 50; tree is rotated
	 *
	 * 100
	 *    \
	 *     350
	 *    /
	 * 150
	 *
	 * ->
	 *
	 *     150
	 *    /   \
	 * 100     350
	 */

	assert_ptrequals(test_avl_list_AVL_DEL(&tl, &t050), &t050);

	/* Check tree structure */
	assert_ptrequals(TREE_ROOT(&tl), &t150);

	assert_ptrequals(t150.node.tree_left, &t100);
	assert_ptrequals(t150.node.tree_right, &t350);

	assert_null(t100.node.tree_left);
	assert_null(t100.node.tree_right);

	assert_null(t350.node.tree_left);
	assert_null(t350.node.tree_right);

	/* Test same-key based delete returns pointer to the deleted object */

	struct test_avl key_test = { .val = t100.val };

	assert_ptrequals(test_avl_list_AVL_DEL(&tl, &key_test), &t100);

	/* Delete remaining nodes */
	assert_ptrequals(test_avl_list_AVL_DEL(&tl, &t150), &t150);
	assert_ptrequals(test_avl_list_AVL_DEL(&tl, &t350), &t350);

	assert_null(TREE_ROOT(&tl));
}

static void
test_avl_get_n(void)
{
	/* Test parameterized matching */

	struct test_avl_list tl = {0};

	struct test_avl
		t0 = { .val =   0, },
		t1 = { .val =  10, },
		t2 = { .val = -15, },
		t3 = { .val =   5, };

	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t0), &t0);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t1), &t1);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t2), &t2);

	assert_ptrequals(test_avl_list_AVL_NGET(&tl, &t3,  2), &t1);
	assert_ptrequals(test_avl_list_AVL_NGET(&tl, &t3, -3), &t2);
}

static void
test_avl_rotations(void)
{
	/* Exercise all 4 rotation types */

	struct test_avl_list tl = {0};

	/* Add 100, 200, 300:
	 *
	 *       100
	 *         \
	 *          200
	 *            \
	 *             300
	 *
	 * Rotates left:
	 *
	 *       200
	 *      /   \
	 *   100     300
	 */

	struct test_avl
		t0 = { .val = 100 },
		t1 = { .val = 200 },
		t2 = { .val = 300 };

	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t0), &t0);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t1), &t1);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t2), &t2);

	assert_ptrequals(TREE_ROOT(&tl), &t1);

	/* 100 */
	assert_null(t0.node.tree_left);
	assert_null(t0.node.tree_right);

	/* 200 */
	assert_ptrequals(t1.node.tree_left,  &t0);
	assert_ptrequals(t1.node.tree_right, &t2);

	/* 300 */
	assert_null(t2.node.tree_left);
	assert_null(t2.node.tree_right);

	/* Add 225, 275:
	 *
	 *       200
	 *      /   \
	 *   100     300
	 *          /
	 *       225
	 *          \
	 *           275
	 *
	 * Rotates left-right:
	 *
	 *       200              200
	 *      /   \            /   \
	 *   100     300  ->  100     275
	 *          /                /   \
	 *       275              225     300
	 *      /
	 *   225
	 */

	struct test_avl
		t3 = { .val = 225 },
		t4 = { .val = 275 };

	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t3), &t3);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t4), &t4);

	assert_ptrequals(TREE_ROOT(&tl), &t1);

	/* 100 */
	assert_null(t0.node.tree_left);
	assert_null(t0.node.tree_right);

	/* 200 */
	assert_ptrequals(t1.node.tree_left,  &t0);
	assert_ptrequals(t1.node.tree_right, &t4);

	/* 300 */
	assert_null(t2.node.tree_left);
	assert_null(t2.node.tree_right);

	/* 225 */
	assert_null(t3.node.tree_left);
	assert_null(t3.node.tree_right);

	/* 275 */
	assert_ptrequals(t4.node.tree_left,  &t3);
	assert_ptrequals(t4.node.tree_right, &t2);

	/* Add 50, 40, 30:
	 *
	 *             200
	 *            /   \
	 *         100     275
	 *        /       /   \
	 *      50     225     300
	 *     /
	 *   40
	 *
	 * Rotates right:
	 *
	 *         _ 200 _
	 *        /       \
	 *      50         275
	 *     /  \       /   \
	 *   40    100 225     300
	 */

	struct test_avl
		t5 = { .val = 50 },
		t6 = { .val = 40 };

	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t5), &t5);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t6), &t6);

	assert_ptrequals(TREE_ROOT(&tl), &t1);

	/* 100 */
	assert_null(t0.node.tree_left);
	assert_null(t0.node.tree_right);

	/* 200 */
	assert_ptrequals(t1.node.tree_left,  &t5);
	assert_ptrequals(t1.node.tree_right, &t4);

	/* 300 */
	assert_null(t2.node.tree_left);
	assert_null(t2.node.tree_right);

	/* 225 */
	assert_null(t3.node.tree_left);
	assert_null(t3.node.tree_right);

	/* 275 */
	assert_ptrequals(t4.node.tree_left,  &t3);
	assert_ptrequals(t4.node.tree_right, &t2);

	/* 50 */
	assert_ptrequals(t5.node.tree_left,  &t6);
	assert_ptrequals(t5.node.tree_right, &t0);

	/* 40 */
	assert_null(t6.node.tree_left);
	assert_null(t6.node.tree_right);

	/* Add 45, 42:
	 *
	 *         _ 200 _
	 *        /       \
	 *      50         275
	 *     /  \       /   \
	 *   40    100 225     300
	 *     \
	 *      45
	 *     /
	 *   42
	 *
	 *
	 * Rotates right-left
	 *
	 *         _ 200 _                    _ 200 _
	 *        /       \                  /       \
	 *      50         275             50         275
	 *     /  \       /   \           /  \       /   \
	 *   40    100 225     300  ->  42    100 225     300
	 *     \                       /  \
	 *      42                   40    45
	 *        \
	 *         45
	 *
	 */

	struct test_avl
		t7 = { .val = 45 },
		t8 = { .val = 42 };

	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t7), &t7);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t8), &t8);

	assert_ptrequals(TREE_ROOT(&tl), &t1);

	/* 100 */
	assert_null(t0.node.tree_left);
	assert_null(t0.node.tree_right);

	/* 200 */
	assert_ptrequals(t1.node.tree_left,  &t5);
	assert_ptrequals(t1.node.tree_right, &t4);

	/* 300 */
	assert_null(t2.node.tree_left);
	assert_null(t2.node.tree_right);

	/* 225 */
	assert_null(t3.node.tree_left);
	assert_null(t3.node.tree_right);

	/* 275 */
	assert_ptrequals(t4.node.tree_left,  &t3);
	assert_ptrequals(t4.node.tree_right, &t2);

	/* 50 */
	assert_ptrequals(t5.node.tree_left,  &t8);
	assert_ptrequals(t5.node.tree_right, &t0);

	/* 40 */
	assert_null(t6.node.tree_left);
	assert_null(t6.node.tree_right);

	/* 45 */
	assert_null(t7.node.tree_left);
	assert_null(t7.node.tree_right);

	/* 42 */
	assert_ptrequals(t8.node.tree_left,  &t6);
	assert_ptrequals(t8.node.tree_right, &t7);
}

static void
test_avl_foreach(void)
{
	/* Test that each node can be reached and altered safely (e.g. freed) */

	struct test_avl_list tl = {0};

	struct test_avl
		t200 = { .val = 200 },
		t100 = { .val = 100 },
		t300 = { .val = 300 },
		t050 = { .val = 50 },
		t150 = { .val = 150 },
		t250 = { .val = 250 },
		t350 = { .val = 350 };

	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t200), &t200);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t100), &t100);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t300), &t300);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t050), &t050);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t150), &t150);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t250), &t250);
	assert_ptrequals(test_avl_list_AVL_ADD(&tl, &t350), &t350);

	test_avl_list_AVL_FOREACH(&tl, foreach_f);

	assert_eq(t200.val, 0);
	assert_null(t200.node.tree_left);
	assert_null(t200.node.tree_right);

	assert_eq(t100.val, 0);
	assert_null(t100.node.tree_left);
	assert_null(t100.node.tree_right);

	assert_eq(t300.val, 0);
	assert_null(t300.node.tree_left);
	assert_null(t300.node.tree_right);

	assert_eq(t050.val, 0);
	assert_null(t050.node.tree_left);
	assert_null(t050.node.tree_right);

	assert_eq(t150.val, 0);
	assert_null(t150.node.tree_left);
	assert_null(t150.node.tree_right);

	assert_eq(t250.val, 0);
	assert_null(t250.node.tree_left);
	assert_null(t250.node.tree_right);

	assert_eq(t350.val, 0);
	assert_null(t350.node.tree_left);
	assert_null(t350.node.tree_right);
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
		TESTCASE(test_avl_get_n),
		TESTCASE(test_avl_rotations),
		TESTCASE(test_avl_foreach)
	};

	return run_tests(tests);
}
