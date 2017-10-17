#include "test.h"

#include "../src/user.c"
#include "../src/utils.c"

	/*
	int user_list_add(struct user_list*, char*, char);
	int user_list_del(struct user_list*, char*);
	int user_list_rpl(struct user_list*, char*, char*);
	struct user* user_list_get(struct user_list*, char*, size_t);
	*/

static void
test_user_list(void)
{
	/* Test add/del/get/rpl users */

	struct user_list ulist;
	struct user *u1 = NULL,
	            *u2 = NULL,
	            *u3 = NULL;

	memset(&ulist, 0, sizeof(ulist));

	/* Test adding users to list */
	u1 = user_list_add(&ulist, "aaa", 0);
	u2 = user_list_add(&ulist, "bbb", 0);
	u3 = user_list_add(&ulist, "ccc", 0);

	if (ulist.count != 3)
		abort_test("Failed to add users to list");

	/* Test adding duplicates */
	assert_null(user_list_add(&ulist, "aaa",   0));
	assert_null(user_list_add(&ulist, "aaa", '@'));

	/* Test retrieving by name, success */
	assert_ptrequals(user_list_get(&ulist, "aaa", 0), u1);
	assert_ptrequals(user_list_get(&ulist, "bbb", 0), u2);
	assert_ptrequals(user_list_get(&ulist, "ccc", 0), u3);

	/* Test retrieving by name, failure */
	assert_null(user_list_get(&ulist, "a", 0));
	assert_null(user_list_get(&ulist, "z", 0));

	/* Test retrieving by name prefix, success */
	assert_ptrequals(user_list_get(&ulist, "a",  1), u1);
	assert_ptrequals(user_list_get(&ulist, "bb", 2), u2);

	/* Test retrieving by name prefix, failure */
	assert_null(user_list_get(&ulist, "z", 1));

	/* Test removing users from list, success */
	assert_ptrequals(user_list_del(&ulist, "aaa"), u1);
	assert_ptrequals(user_list_del(&ulist, "bbb"), u2);
	assert_ptrequals(user_list_del(&ulist, "ccc"), u3);

	/* Test removing users from list, failure */
	assert_null(user_list_del(&ulist, "aaa"));
	assert_null(user_list_del(&ulist, "zzz"));

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
