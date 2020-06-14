#include "test/test.h"

#define IRCV3_CAPS \
	X("cap-1", cap_1, IRCV3_CAP_AUTO) \
	X("cap-2", cap_2, 0) \
	X("cap-3", cap_3, (IRCV3_CAP_NO_DEL | IRCV3_CAP_NO_REQ))

#include "src/components/ircv3.c"

static void
test_ircv3_caps(void)
{
	struct ircv3_caps caps;

	ircv3_caps(&caps);

	assert_eq(caps.cap_1.req, 0);
	assert_eq(caps.cap_2.req, 0);
	assert_eq(caps.cap_3.req, 0);
	assert_eq(caps.cap_1.req_auto, 1);
	assert_eq(caps.cap_2.req_auto, 0);
	assert_eq(caps.cap_3.req_auto, 0);
	assert_eq(caps.cap_1.set, 0);
	assert_eq(caps.cap_2.set, 0);
	assert_eq(caps.cap_3.set, 0);
	assert_eq(caps.cap_1.supported, 0);
	assert_eq(caps.cap_2.supported, 0);
	assert_eq(caps.cap_3.supported, 0);
	assert_eq(caps.cap_1.supports_del, 1);
	assert_eq(caps.cap_2.supports_del, 1);
	assert_eq(caps.cap_3.supports_del, 0);
	assert_eq(caps.cap_1.supports_req, 1);
	assert_eq(caps.cap_2.supports_req, 1);
	assert_eq(caps.cap_3.supports_req, 0);
}

static void
test_ircv3_caps_reset(void)
{
	struct ircv3_caps caps;

	caps.cap_3.req = 1;
	caps.cap_3.req_auto = 1;
	caps.cap_3.set = 1;
	caps.cap_3.supported = 1;
	caps.cap_3.supports_del = 1;
	caps.cap_3.supports_req = 1;

	ircv3_caps_reset(&caps);

	assert_eq(caps.cap_3.req, 0);
	assert_eq(caps.cap_3.req_auto, 1);
	assert_eq(caps.cap_3.set, 0);
	assert_eq(caps.cap_3.supported, 0);
	assert_eq(caps.cap_3.supports_del, 1);
	assert_eq(caps.cap_3.supports_req, 1);
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_ircv3_caps),
		TESTCASE(test_ircv3_caps_reset),
	};

	return run_tests(tests);
}
