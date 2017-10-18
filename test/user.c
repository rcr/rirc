#include "test.h"

#include "../src/user.c"
#include "../src/utils.c"

static void
test_user_list(void)
{
	/* Test add/del/get/rpl users */

	struct user_list ulist;
	struct user *u1, *u2, *u3, *u4;

	memset(&ulist, 0, sizeof(ulist));

	/* Test adding users to list */
	assert_eq(user_list_add(&ulist, "aaa"), USER_ERR_NONE);
	assert_eq(user_list_add(&ulist, "bbb"), USER_ERR_NONE);
	assert_eq(user_list_add(&ulist, "ccc"), USER_ERR_NONE);

	if (ulist.count != 3)
		abort_test("Failed to add users to list");

	/* Test adding duplicates */
	assert_eq(user_list_add(&ulist, "aaa"), USER_ERR_DUPLICATE);

	/* Test retrieving by name, failure */
	assert_null(user_list_get(&ulist, "a", 0));
	assert_null(user_list_get(&ulist, "z", 0));

	/* Test retrieving by name, success */
	if ((u1 = user_list_get(&ulist, "aaa", 0)) == NULL)
		abort_test("Failed to retrieve u1");

	if ((u2 = user_list_get(&ulist, "bbb", 0)) == NULL)
		abort_test("Failed to retrieve u2");

	if ((u3 = user_list_get(&ulist, "ccc", 0)) == NULL)
		abort_test("Failed to retrieve u3");

	assert_strcmp(u1->nick, "aaa");
	assert_strcmp(u2->nick, "bbb");
	assert_strcmp(u3->nick, "ccc");

	/* Test retrieving by name prefix, failure */
	assert_null(user_list_get(&ulist, "z",  1));
	assert_null(user_list_get(&ulist, "ab", 2));

	if ((u1 = user_list_get(&ulist, "a", 1)) == NULL)
		abort_test("Failed to retrieve u1 by prefix");

	if ((u2 = user_list_get(&ulist, "bb", 2)) == NULL)
		abort_test("Failed to retrieve u2 by prefix");

	if ((u3 = user_list_get(&ulist, "ccc", 3)) == NULL)
		abort_test("Failed to retrieve u3 by prefix");

	assert_strcmp(u1->nick, "aaa");
	assert_strcmp(u2->nick, "bbb");
	assert_strcmp(u3->nick, "ccc");

	/* Test replacing user in list, failure */
	assert_eq(user_list_rpl(&ulist, "zzz", "yyy"), USER_ERR_NOT_FOUND);
	assert_eq(user_list_rpl(&ulist, "bbb", "ccc"), USER_ERR_DUPLICATE);

	/* Test replacing user in list, success */
	u3->prefix = '*';
	assert_eq(user_list_rpl(&ulist, "ccc", "ddd"), USER_ERR_NONE);

	if ((u4 = user_list_get(&ulist, "ddd", 0)) == NULL)
		abort_test("Failed to retrieve u4 by prefix");

	assert_eq(u4->prefix, '*');
	assert_strcmp(u4->nick, "ddd");

	assert_null(user_list_get(&ulist, "ccc",  0));

	/* Test removing users from list, failure */
	assert_eq(user_list_del(&ulist, "ccc"), USER_ERR_NOT_FOUND);

	/* Test removing users from list, success */
	assert_eq(user_list_del(&ulist, "aaa"), USER_ERR_NONE);
	assert_eq(user_list_del(&ulist, "bbb"), USER_ERR_NONE);
	assert_eq(user_list_del(&ulist, "ddd"), USER_ERR_NONE);

	assert_eq(ulist.count, 0);
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_user_list)
	};

	return run_tests(tests);
}
