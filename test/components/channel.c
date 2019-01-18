#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/mode.c"
#include "src/components/user.c"
#include "src/utils/utils.c"

static void
test_channel_list(void)
{
	/* Test add/del/get channels */

	struct channel_list clist;
	struct channel *c1 = NULL,
	               *c2 = NULL,
	               *c3 = NULL;

	memset(&clist, 0, sizeof(clist));

	c1 = channel("aaa", CHANNEL_T_OTHER);
	c2 = channel("bbb", CHANNEL_T_OTHER);
	c3 = channel("bbb", CHANNEL_T_OTHER);

	/* Test adding channels to list */
	assert_ptr_eq(channel_list_add(&clist, c1), NULL);
	assert_ptr_eq(channel_list_add(&clist, c2), NULL);

	/* Test adding duplicate */
	assert_ptr_eq(channel_list_add(&clist, c2), c2);

	/* Test adding duplicate by name */
	assert_ptr_eq(channel_list_add(&clist, c3), c2);

	/* Test retrieving by name */
	assert_ptr_eq(channel_list_get(&clist, "aaa"), c1);
	assert_ptr_eq(channel_list_get(&clist, "bbb"), c2);

	/* Test removing channels from list */
	assert_ptr_eq(channel_list_del(&clist, c1), c1);
	assert_ptr_eq(channel_list_del(&clist, c2), c2);

	/* Test removing channels not in list */
	assert_ptr_eq(channel_list_del(&clist, c1), NULL);
	assert_ptr_eq(channel_list_del(&clist, c2), NULL);
	assert_ptr_eq(channel_list_del(&clist, c3), NULL);

	channel_free(c1);
	channel_free(c2);
	channel_free(c3);
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_channel_list)
	};

	return run_tests(tests);
}
