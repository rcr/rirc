#include "test/test.h"
#include "src/components/user.c"
#include "src/utils/utils.c"

static void
test_user_list(void)
{
	/* Test add/del/get/rpl users */

	struct user *u1, *u2, *u3, *u4;
	struct user_list ulist;

	memset(&ulist, 0, sizeof(ulist));

	/* Test adding users to list */
	assert_eq(user_list_add(&ulist, CASEMAPPING_RFC1459, "aaa", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&ulist, CASEMAPPING_RFC1459, "bbb", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&ulist, CASEMAPPING_RFC1459, "ccc", MODE_EMPTY), USER_ERR_NONE);

	if (ulist.count != 3)
		test_abort("Failed to add users to list");

	/* Test adding duplicates */
	assert_eq(user_list_add(&ulist, CASEMAPPING_RFC1459, "aaa", MODE_EMPTY), USER_ERR_DUPLICATE);

	/* Test retrieving by name, failure */
	assert_ptr_null(user_list_get(&ulist, CASEMAPPING_RFC1459, "a", 0));
	assert_ptr_null(user_list_get(&ulist, CASEMAPPING_RFC1459, "z", 0));

	/* Test retrieving by name, success */
	if ((u1 = user_list_get(&ulist, CASEMAPPING_RFC1459, "aaa", 0)) == NULL)
		test_abort("Failed to retrieve u1");

	if ((u2 = user_list_get(&ulist, CASEMAPPING_RFC1459, "bbb", 0)) == NULL)
		test_abort("Failed to retrieve u2");

	if ((u3 = user_list_get(&ulist, CASEMAPPING_RFC1459, "ccc", 0)) == NULL)
		test_abort("Failed to retrieve u3");

	assert_strcmp(u1->nick, "aaa");
	assert_strcmp(u2->nick, "bbb");
	assert_strcmp(u3->nick, "ccc");

	/* Test retrieving by name prefix, failure */
	assert_ptr_null(user_list_get(&ulist, CASEMAPPING_RFC1459, "z",  1));
	assert_ptr_null(user_list_get(&ulist, CASEMAPPING_RFC1459, "ab", 2));

	if ((u1 = user_list_get(&ulist, CASEMAPPING_RFC1459, "a", 1)) == NULL)
		test_abort("Failed to retrieve u1 by prefix");

	if ((u2 = user_list_get(&ulist, CASEMAPPING_RFC1459, "bb", 2)) == NULL)
		test_abort("Failed to retrieve u2 by prefix");

	if ((u3 = user_list_get(&ulist, CASEMAPPING_RFC1459, "ccc", 3)) == NULL)
		test_abort("Failed to retrieve u3 by prefix");

	assert_strcmp(u1->nick, "aaa");
	assert_strcmp(u2->nick, "bbb");
	assert_strcmp(u3->nick, "ccc");

	/* Test replacing user in list, failure */
	assert_eq(user_list_rpl(&ulist, CASEMAPPING_RFC1459, "zzz", "yyy"), USER_ERR_NOT_FOUND);
	assert_eq(user_list_rpl(&ulist, CASEMAPPING_RFC1459, "bbb", "ccc"), USER_ERR_DUPLICATE);

	/* Test replacing user in list, success */
	u3->prfxmodes.lower = 0x123;
	u3->prfxmodes.upper = 0x456;
	u3->prfxmodes.prefix = '*';

	assert_eq(user_list_rpl(&ulist, CASEMAPPING_RFC1459, "ccc", "ddd"), USER_ERR_NONE);

	if ((u4 = user_list_get(&ulist, CASEMAPPING_RFC1459, "ddd", 0)) == NULL)
		test_abort("Failed to retrieve u4");

	assert_eq(u4->prfxmodes.lower, 0x123);
	assert_eq(u4->prfxmodes.upper, 0x456);
	assert_eq(u4->prfxmodes.prefix, '*');

	assert_strcmp(u4->nick, "ddd");

	assert_ptr_null(user_list_get(&ulist, CASEMAPPING_RFC1459, "ccc",  0));

	/* Test removing users from list, failure */
	assert_eq(user_list_del(&ulist, CASEMAPPING_RFC1459, "ccc"), USER_ERR_NOT_FOUND);

	/* Test removing users from list, success */
	assert_eq(user_list_del(&ulist, CASEMAPPING_RFC1459, "aaa"), USER_ERR_NONE);
	assert_eq(user_list_del(&ulist, CASEMAPPING_RFC1459, "bbb"), USER_ERR_NONE);
	assert_eq(user_list_del(&ulist, CASEMAPPING_RFC1459, "ddd"), USER_ERR_NONE);

	assert_eq(ulist.count, 0);
}

static void
test_user_list_casemapping(void)
{
	/* Test add/del/get/rpl casemapping */

	struct user *u;
	struct user_list ulist;

	memset(&ulist, 0, sizeof(ulist));

	assert_eq(user_list_add(&ulist, CASEMAPPING_RFC1459, "aaa", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&ulist, CASEMAPPING_RFC1459, "AaA", MODE_EMPTY), USER_ERR_DUPLICATE);
	assert_eq(user_list_add(&ulist, CASEMAPPING_RFC1459, "{}^", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&ulist, CASEMAPPING_RFC1459, "[}~", MODE_EMPTY), USER_ERR_DUPLICATE);
	assert_eq(user_list_add(&ulist, CASEMAPPING_RFC1459, "zzz", MODE_EMPTY), USER_ERR_NONE);

	assert_eq(ulist.count, 3);

	if ((u = user_list_get(&ulist, CASEMAPPING_RFC1459, "AAA", 0)) == NULL)
		test_abort("Failed to retrieve u");

	assert_ptr_eq(user_list_get(&ulist, CASEMAPPING_RFC1459, "aAa", 3), u);
	assert_ptr_eq(user_list_get(&ulist, CASEMAPPING_RFC1459, "A",   1), u);

	assert_eq(user_list_rpl(&ulist, CASEMAPPING_RFC1459, "AaA", "bbb"), USER_ERR_NONE);
	assert_eq(user_list_del(&ulist, CASEMAPPING_RFC1459, "aaa"),        USER_ERR_NOT_FOUND);
	assert_eq(user_list_del(&ulist, CASEMAPPING_RFC1459, "BBB"),        USER_ERR_NONE);

	assert_eq(ulist.count, 2);

	assert_eq(user_list_rpl(&ulist, CASEMAPPING_RFC1459, "{}^", "[}~"), USER_ERR_NONE);
	assert_eq(user_list_rpl(&ulist, CASEMAPPING_RFC1459, "zzz", "ZzZ"), USER_ERR_NONE);

	assert_eq(ulist.count, 2);

	assert_ptr_not_null(user_list_get(&ulist, CASEMAPPING_RFC1459, "zZz", 0));
	assert_ptr_not_null(user_list_get(&ulist, CASEMAPPING_RFC1459, "{]^", 0));

	user_list_free(&ulist);
}

static void
test_user_list_free(void)
{
	/* Test userlist can be freed and used again */

	struct user_list ulist;

	memset(&ulist, 0, sizeof(ulist));

	const char **p, *users[] = {
		"aaa", "bbb", "ccc", "ddd", "eee", "fff",
		"ggg", "hhh", "iii", "jjj", "kkk", "lll",
		"mmm", "nnn", "ooo", "ppp", "qqq", "rrr",
		"sss", "ttt", "uuu", "vvv", "www", "xxx",
		"yyy", "zzz", NULL
	};

	for (p = users; *p; p++) {
		if (user_list_add(&ulist, CASEMAPPING_RFC1459, *p, MODE_EMPTY) != USER_ERR_NONE)
			test_failf("Failed to add user to list: %s", *p);
	}

	user_list_free(&ulist);

	for (p = users; *p; p++) {
		if (user_list_add(&ulist, CASEMAPPING_RFC1459, *p, MODE_EMPTY) != USER_ERR_NONE)
			test_failf("Failed to remove user from list: %s", *p);
	}

	user_list_free(&ulist);
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_user_list),
		TESTCASE(test_user_list_casemapping),
		TESTCASE(test_user_list_free)
	};

	return run_tests(NULL, NULL, tests);
}
