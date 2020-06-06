#include "test/test.h"

#define IRCV3_CAPS \
	X("cap-1", cap_1, IRCV3_CAP_AUTO) \
	X("cap-2", cap_2, 0)

#include "src/components/ircv3_cap.c"

static void
test_ircv3_caps(void)
{
	struct ircv3_caps caps;

	ircv3_caps(&caps);

	assert_eq(caps.cap_1.req, 0);
	assert_eq(caps.cap_2.req, 0);
	assert_eq(caps.cap_1.req_auto, 1);
	assert_eq(caps.cap_2.req_auto, 0);
	assert_eq(caps.cap_1.set, 0);
	assert_eq(caps.cap_2.set, 0);
}

static void
test_ircv3_caps_reset(void)
{
	/* TODO */
}

static void
test_ircv3_cap_ack(void)
{
	struct ircv3_caps caps;

	ircv3_caps(&caps);

	caps.cap_reqs = 2;

	assert_eq(caps.cap_1.set, 0);
	assert_eq(caps.cap_1.req, 0);

	caps.cap_1.req = 1;
	caps.cap_1.set = 0;
	assert_eq(ircv3_cap_ack(&caps, "cap-1"), 0);
	assert_eq(caps.cap_1.set, 1);
	assert_eq(caps.cap_1.req, 0);
	assert_eq(caps.cap_reqs, 1);

	caps.cap_1.req = 1;
	caps.cap_1.set = 1;
	assert_eq(ircv3_cap_ack(&caps, "-cap-1"), 0);
	assert_eq(caps.cap_1.set, 0);
	assert_eq(caps.cap_1.req, 0);
	assert_eq(caps.cap_reqs, 0);

	assert_eq(ircv3_cap_ack(&caps, "cap-unsupported"), IRCV3_CAP_ERR_UNSUPPORTED);

	caps.cap_1.req = 0;
	caps.cap_1.set = 0;
	assert_eq(ircv3_cap_ack(&caps, "cap-1"), IRCV3_CAP_ERR_NO_REQ);
	caps.cap_1.set = 0;

	caps.cap_1.req = 1;
	caps.cap_1.set = 1;
	assert_eq(ircv3_cap_ack(&caps, "cap-1"), IRCV3_CAP_ERR_WAS_SET);

	caps.cap_1.req = 1;
	caps.cap_1.set = 0;
	assert_eq(ircv3_cap_ack(&caps, "-cap-1"), IRCV3_CAP_ERR_WAS_UNSET);
}

static void
test_ircv3_cap_nak(void)
{
	struct ircv3_caps caps;

	ircv3_caps(&caps);

	caps.cap_reqs = 2;

	assert_eq(caps.cap_1.set, 0);
	assert_eq(caps.cap_1.req, 0);

	caps.cap_1.req = 1;
	caps.cap_1.set = 0;
	assert_eq(ircv3_cap_nak(&caps, "cap-1"), 0);
	assert_eq(caps.cap_1.set, 0);
	assert_eq(caps.cap_1.req, 0);
	assert_eq(caps.cap_reqs, 1);

	caps.cap_1.req = 1;
	caps.cap_1.set = 1;
	assert_eq(ircv3_cap_nak(&caps, "-cap-1"), 0);
	assert_eq(caps.cap_1.set, 1);
	assert_eq(caps.cap_1.req, 0);
	assert_eq(caps.cap_reqs, 0);

	assert_eq(ircv3_cap_nak(&caps, "cap-unsupported"), IRCV3_CAP_ERR_UNSUPPORTED);

	caps.cap_1.req = 0;
	caps.cap_1.set = 0;
	assert_eq(ircv3_cap_nak(&caps, "cap-1"), IRCV3_CAP_ERR_NO_REQ);
	caps.cap_1.set = 0;

	caps.cap_1.req = 1;
	caps.cap_1.set = 1;
	assert_eq(ircv3_cap_nak(&caps, "cap-1"), IRCV3_CAP_ERR_WAS_SET);

	caps.cap_1.req = 1;
	caps.cap_1.set = 0;
	assert_eq(ircv3_cap_nak(&caps, "-cap-1"), IRCV3_CAP_ERR_WAS_UNSET);
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_ircv3_caps),
		TESTCASE(test_ircv3_caps_reset),
		TESTCASE(test_ircv3_cap_ack),
		TESTCASE(test_ircv3_cap_nak),
	};

	/* Coverage */
	(void) ircv3_cap_err(IRCV3_CAP_ERR_NO_REQ);
	(void) ircv3_cap_err(IRCV3_CAP_ERR_UNSUPPORTED);
	(void) ircv3_cap_err(IRCV3_CAP_ERR_WAS_SET);
	(void) ircv3_cap_err(IRCV3_CAP_ERR_WAS_UNSET);
	(void) ircv3_cap_err(-1);

	return run_tests(tests);
}
