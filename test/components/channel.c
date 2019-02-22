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
	c3 = channel("ccc", CHANNEL_T_OTHER);

	channel_list_add(&clist, c1);
	channel_list_add(&clist, c2);
	channel_list_add(&clist, c3);

	assert_ptr_eq(channel_list_get(&clist, "aaa", CASEMAPPING_ASCII), c1);
	assert_ptr_eq(channel_list_get(&clist, "bbb", CASEMAPPING_ASCII), c2);
	assert_ptr_eq(channel_list_get(&clist, "ccc", CASEMAPPING_ASCII), c3);

	channel_list_del(&clist, c2);
	channel_list_del(&clist, c1);
	channel_list_del(&clist, c3);

	assert_ptr_eq(channel_list_get(&clist, "aaa", CASEMAPPING_ASCII), NULL);
	assert_ptr_eq(channel_list_get(&clist, "bbb", CASEMAPPING_ASCII), NULL);
	assert_ptr_eq(channel_list_get(&clist, "ccc", CASEMAPPING_ASCII), NULL);

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
