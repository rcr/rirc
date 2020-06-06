#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/ircv3_cap.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/handlers/irc_ctcp.c"
#include "src/utils/utils.c"

#define CHECK_REQUEST(F, T, M, R, L, S) \
	do { \
	    line_buf[0] = 0; \
	    send_buf[0] = 0; \
	    assert_eq(ctcp_request(s, (F), (T), (M)), (R)); \
	    assert_strcmp(line_buf, (L)); \
	    assert_strcmp(send_buf, (S)); \
	} while (0)

#define CHECK_RESPONSE(F, T, M, R, L) \
	do { \
	    line_buf[0] = 0; \
	    assert_eq(ctcp_response(s, (F), (T), (M)), (R)); \
	    assert_strcmp(line_buf, (L)); \
	} while (0)

static char chan_buf[1024];
static char line_buf[1024];
static char send_buf[1024];
static struct channel *c_chan;
static struct channel *c_priv;
static struct server *s;

/* Mock state.c */
void
newlinef(struct channel *c, enum buffer_line_t t, const char *f, const char *fmt, ...)
{
	va_list ap;
	int r1;
	int r2;

	UNUSED(f);
	UNUSED(t);

	va_start(ap, fmt);
	r1 = snprintf(chan_buf, sizeof(chan_buf), "%s", c->name);
	r2 = vsnprintf(line_buf, sizeof(line_buf), fmt, ap);
	va_end(ap);

	assert_gt(r1, 0);
	assert_gt(r2, 0);
}

void
newline(struct channel *c, enum buffer_line_t t, const char *f, const char *fmt)
{
	int r1;
	int r2;

	UNUSED(f);
	UNUSED(t);

	r1 = snprintf(chan_buf, sizeof(chan_buf), "%s", c->name);
	r2 = snprintf(line_buf, sizeof(line_buf), "%s", fmt);

	assert_gt(r1, 0);
	assert_gt(r2, 0);
}

/* Mock io.c */
const char*
io_err(int err)
{
	UNUSED(err);

	return "err";
}

int
io_sendf(struct connection *c, const char *fmt, ...)
{
	va_list ap;

	UNUSED(c);

	va_start(ap, fmt);
	assert_gt(vsnprintf(send_buf, sizeof(send_buf), fmt, ap), 0);
	va_end(ap);

	return 0;
}

#define X(cmd) static void test_recv_ctcp_request_##cmd(void);
CTCP_EXTENDED_FORMATTING
CTCP_EXTENDED_QUERY
CTCP_METADATA_QUERY
#undef X

#define X(cmd) static void test_recv_ctcp_response_##cmd(void);
CTCP_EXTENDED_QUERY
CTCP_METADATA_QUERY
#undef X

static void
test_recv_ctcp_request(void)
{
	char m1[] = "";
	char m2[] = " ";
	char m3[] = "\001";
	char m4[] = "\001\001";
	char m5[] = "\001 \001";
	char m6[] = "\001TEST1";
	char m7[] = "\001TEST1\001";
	char m8[] = "\001TEST2 arg1 arg2\001";
	char m9[] = "\001TEST1\001";

	CHECK_REQUEST("nick", "targ", m1, 1, "Received malformed CTCP from nick", "");
	CHECK_REQUEST("nick", "targ", m2, 1, "Received malformed CTCP from nick", "");
	CHECK_REQUEST("nick", "targ", m3, 1, "Received empty CTCP from nick", "");
	CHECK_REQUEST("nick", "targ", m4, 1, "Received empty CTCP from nick", "");
	CHECK_REQUEST("nick", "targ", m5, 1, "Received empty CTCP from nick", "");
	CHECK_REQUEST("nick", "targ", m6, 1, "Received unsupported CTCP request 'TEST1' from nick", "");
	CHECK_REQUEST("nick", "targ", m7, 1, "Received unsupported CTCP request 'TEST1' from nick", "");
	CHECK_REQUEST("nick", "targ", m8, 1, "Received unsupported CTCP request 'TEST2' from nick", "");
	CHECK_REQUEST(NULL, "targ", m9, 1, "Received CTCP from unknown sender", "");
}

static void
test_recv_ctcp_response(void)
{
	char m1[] = "";
	char m2[] = " ";
	char m3[] = "\001";
	char m4[] = "\001\001";
	char m5[] = "\001 \001";
	char m6[] = "\001TEST1";
	char m7[] = "\001TEST1\001";
	char m8[] = "\001TEST2 arg1 arg2\001";
	char m9[] = "\001TEST1\001";

	CHECK_RESPONSE("nick", "targ", m1, 1, "Received malformed CTCP from nick");
	CHECK_RESPONSE("nick", "targ", m2, 1, "Received malformed CTCP from nick");
	CHECK_RESPONSE("nick", "targ", m3, 1, "Received empty CTCP from nick");
	CHECK_RESPONSE("nick", "targ", m4, 1, "Received empty CTCP from nick");
	CHECK_RESPONSE("nick", "targ", m5, 1, "Received empty CTCP from nick");
	CHECK_RESPONSE("nick", "targ", m6, 1, "Received unsupported CTCP response 'TEST1' from nick");
	CHECK_RESPONSE("nick", "targ", m7, 1, "Received unsupported CTCP response 'TEST1' from nick");
	CHECK_RESPONSE("nick", "targ", m8, 1, "Received unsupported CTCP response 'TEST2' from nick");
	CHECK_RESPONSE(NULL, "targ", m9, 1, "Received CTCP from unknown sender");
}

static void
test_recv_ctcp_request_action(void)
{
	char m1[] = "\001ACTION test action 1";
	char m2[] = "\001ACTION test action 2\001";
	char m3[] = "\001ACTION test action 3";
	char m4[] = "\001ACTION \001";
	char m5[] = "\001ACTION\001";
	char m6[] = "\001ACTION";
	char m7[] = "\001ACTION test action 4\001";
	char m8[] = "\001ACTION test action 4\001";

#define CHECK_ACTION_REQUEST(F, T, M, R, C, L) \
	do { \
	    chan_buf[0] = 0; \
	    line_buf[0] = 0; \
	    assert_eq(ctcp_request(s, (F), (T), (M)), (R)); \
	    assert_strcmp(chan_buf, (C)); \
	    assert_strcmp(line_buf, (L)); \
	} while (0)

	/* Action message to me as existing private message */
	CHECK_ACTION_REQUEST("nick", "mynick", m1, 0, "nick", "nick test action 1");

	/* Action message to me as new private message */
	CHECK_ACTION_REQUEST("new_priv", "mynick", m2, 0, "new_priv", "new_priv test action 2");

	/* Action message to existing channel */
	CHECK_ACTION_REQUEST("nick", "chan", m3, 0, "chan", "nick test action 3");

	/* Empty action messages */
	CHECK_ACTION_REQUEST("nick", "mynick", m4, 0, "nick", "nick");
	CHECK_ACTION_REQUEST("nick", "mynick", m5, 0, "nick", "nick");
	CHECK_ACTION_REQUEST("nick", "mynick", m6, 0, "nick", "nick");

	/* Action message to nonexistant channel */
	CHECK_ACTION_REQUEST("nick", "not_a_chan", m7, 1, "h1", "CTCP ACTION: target 'not_a_chan' not found");

	/* Action message with no target */
	CHECK_ACTION_REQUEST("nick", NULL, m8, 1, "h1", "CTCP ACTION: target is NULL");

#undef CHECK_ACTION_REQUEST
}

static void
test_recv_ctcp_request_clientinfo(void)
{
	char m1[] = "\001CLIENTINFO";
	char m2[] = "\001CLIENTINFO\001";
	char m3[] = "\001CLIENTINFO unused args\001";

	CHECK_REQUEST("nick", "targ", m1, 0,
		"CTCP CLIENTINFO from nick",
		"NOTICE nick :\001CLIENTINFO ACTION CLIENTINFO PING SOURCE TIME VERSION\001");

	CHECK_REQUEST("nick", "targ", m2, 0,
		"CTCP CLIENTINFO from nick",
		"NOTICE nick :\001CLIENTINFO ACTION CLIENTINFO PING SOURCE TIME VERSION\001");

	CHECK_REQUEST("nick", "targ", m3, 0,
		"CTCP CLIENTINFO from nick (unused args)",
		"NOTICE nick :\001CLIENTINFO ACTION CLIENTINFO PING SOURCE TIME VERSION\001");
}

static void
test_recv_ctcp_request_finger(void)
{
	char m1[] = "\001FINGER";
	char m2[] = "\001FINGER\001";
	char m3[] = "\001FINGER unused args\001";

	CHECK_REQUEST("nick", "targ", m1, 0,
		"CTCP FINGER from nick",
		"NOTICE nick :\001FINGER rirc v"VERSION" ("__DATE__")\001");

	CHECK_REQUEST("nick", "targ", m2, 0,
		"CTCP FINGER from nick",
		"NOTICE nick :\001FINGER rirc v"VERSION" ("__DATE__")\001");

	CHECK_REQUEST("nick", "targ", m3, 0,
		"CTCP FINGER from nick (unused args)",
		"NOTICE nick :\001FINGER rirc v"VERSION" ("__DATE__")\001");
}

static void
test_recv_ctcp_request_ping(void)
{
	char m1[] = "\001PING";
	char m2[] = "\001PING 0\001";
	char m3[] = "\001PING 1 123 abc 0\001";

	/* empty PING message, do nothing */
	CHECK_REQUEST("nick", "targ", m1, 0, "", "");

	CHECK_REQUEST("nick", "targ", m2, 0,
		"CTCP PING from nick",
		"NOTICE nick :\001PING 0\001");

	CHECK_REQUEST("nick", "targ", m3, 0,
		"CTCP PING from nick",
		"NOTICE nick :\001PING 1 123 abc 0\001");
}

static void
test_recv_ctcp_request_source(void)
{
	char m1[] = "\001SOURCE";
	char m2[] = "\001SOURCE\001";
	char m3[] = "\001SOURCE unused args\001";

	CHECK_REQUEST("nick", "targ", m1, 0,
		"CTCP SOURCE from nick",
		"NOTICE nick :\001SOURCE rcr.io/rirc\001");

	CHECK_REQUEST("nick", "targ", m2, 0,
		"CTCP SOURCE from nick",
		"NOTICE nick :\001SOURCE rcr.io/rirc\001");

	CHECK_REQUEST("nick", "targ", m3, 0,
		"CTCP SOURCE from nick (unused args)",
		"NOTICE nick :\001SOURCE rcr.io/rirc\001");
}

static void
test_recv_ctcp_request_time(void)
{
	char m1[] = "\001TIME";
	char m2[] = "\001TIME\001";
	char m3[] = "\001TIME unused args\001";

	CHECK_REQUEST("nick", "targ", m1, 0,
		"CTCP TIME from nick",
		"NOTICE nick :\001TIME 1970-01-01T00:00:00\001");

	CHECK_REQUEST("nick", "targ", m2, 0,
		"CTCP TIME from nick",
		"NOTICE nick :\001TIME 1970-01-01T00:00:00\001");

	CHECK_REQUEST("nick", "targ", m3, 0,
		"CTCP TIME from nick (unused args)",
		"NOTICE nick :\001TIME 1970-01-01T00:00:00\001");
}

static void
test_recv_ctcp_request_userinfo(void)
{
	char m1[] = "\001USERINFO";
	char m2[] = "\001USERINFO\001";
	char m3[] = "\001USERINFO unused args\001";

	CHECK_REQUEST("nick", "targ", m1, 0,
		"CTCP USERINFO from nick",
		"NOTICE nick :\001USERINFO mynick (r1)\001");

	CHECK_REQUEST("nick", "targ", m2, 0,
		"CTCP USERINFO from nick",
		"NOTICE nick :\001USERINFO mynick (r1)\001");

	CHECK_REQUEST("nick", "targ", m3, 0,
		"CTCP USERINFO from nick (unused args)",
		"NOTICE nick :\001USERINFO mynick (r1)\001");
}

static void
test_recv_ctcp_request_version(void)
{
	char m1[] = "\001VERSION";
	char m2[] = "\001VERSION\001";
	char m3[] = "\001VERSION unused args\001";

	CHECK_REQUEST("nick", "targ", m1, 0,
		"CTCP VERSION from nick",
		"NOTICE nick :\001VERSION rirc v"VERSION" ("__DATE__")\001");

	CHECK_REQUEST("nick", "targ", m2, 0,
		"CTCP VERSION from nick",
		"NOTICE nick :\001VERSION rirc v"VERSION" ("__DATE__")\001");

	CHECK_REQUEST("nick", "targ", m3, 0,
		"CTCP VERSION from nick (unused args)",
		"NOTICE nick :\001VERSION rirc v"VERSION" ("__DATE__")\001");
}

static void
test_recv_ctcp_response_clientinfo(void)
{
	char m1[] = "\001CLIENTINFO";
	char m2[] = "\001CLIENTINFO\001";
	char m3[] = "\001CLIENTINFO FOO BAR BAZ";
	char m4[] = "\001CLIENTINFO 123 456 789\001";

	CHECK_RESPONSE("nick", "targ", m1, 1, "CTCP CLIENTINFO response from nick: empty message");
	CHECK_RESPONSE("nick", "targ", m2, 1, "CTCP CLIENTINFO response from nick: empty message");
	CHECK_RESPONSE("nick", "targ", m3, 0, "CTCP CLIENTINFO response from nick: FOO BAR BAZ");
	CHECK_RESPONSE("nick", "targ", m4, 0, "CTCP CLIENTINFO response from nick: 123 456 789");
}

static void
test_recv_ctcp_response_finger(void)
{
	char m1[] = "\001FINGER";
	char m2[] = "\001FINGER\001";
	char m3[] = "\001FINGER FOO BAR BAZ";
	char m4[] = "\001FINGER 123 456 789\001";

	CHECK_RESPONSE("nick", "targ", m1, 1, "CTCP FINGER response from nick: empty message");
	CHECK_RESPONSE("nick", "targ", m2, 1, "CTCP FINGER response from nick: empty message");
	CHECK_RESPONSE("nick", "targ", m3, 0, "CTCP FINGER response from nick: FOO BAR BAZ");
	CHECK_RESPONSE("nick", "targ", m4, 0, "CTCP FINGER response from nick: 123 456 789");
}

static void
test_recv_ctcp_response_ping(void)
{
	char m1[] = "\001PING";
	char m2[] = "\001PING 123";
	char m3[] = "\001PING 1a3 345\001";
	char m4[] = "\001PING 123 345a\001";
	char m5[] = "\001PING 125 456789\001";
	char m6[] = "\001PING 123 567890\001";
	char m7[] = "\001PING 120 456789\001";
	char m8[] = "\001PING 120 111111\001";
	char m9[] = "\001PING 120 000000";

	CHECK_RESPONSE("nick", "targ", m1, 1, "CTCP PING response from nick: sec is NULL");
	CHECK_RESPONSE("nick", "targ", m2, 1, "CTCP PING response from nick: usec is NULL");
	CHECK_RESPONSE("nick", "targ", m3, 1, "CTCP PING response from nick: sec is invalid");
	CHECK_RESPONSE("nick", "targ", m4, 1, "CTCP PING response from nick: usec is invalid");
	CHECK_RESPONSE("nick", "targ", m5, 1, "CTCP PING response from nick: invalid timestamp");
	CHECK_RESPONSE("nick", "targ", m6, 1, "CTCP PING response from nick: invalid timestamp");
	CHECK_RESPONSE("nick", "targ", m7, 0, "CTCP PING response from nick: 3.0s");
	CHECK_RESPONSE("nick", "targ", m8, 0, "CTCP PING response from nick: 3.345678s");
	CHECK_RESPONSE("nick", "targ", m9, 0, "CTCP PING response from nick: 3.456789s");
}

static void
test_recv_ctcp_response_source(void)
{
	char m1[] = "\001SOURCE";
	char m2[] = "\001SOURCE\001";
	char m3[] = "\001SOURCE FOO BAR BAZ";
	char m4[] = "\001SOURCE 123 456 789\001";

	CHECK_RESPONSE("nick", "targ", m1, 1, "CTCP SOURCE response from nick: empty message");
	CHECK_RESPONSE("nick", "targ", m2, 1, "CTCP SOURCE response from nick: empty message");
	CHECK_RESPONSE("nick", "targ", m3, 0, "CTCP SOURCE response from nick: FOO BAR BAZ");
	CHECK_RESPONSE("nick", "targ", m4, 0, "CTCP SOURCE response from nick: 123 456 789");
}

static void
test_recv_ctcp_response_time(void)
{
	char m1[] = "\001TIME";
	char m2[] = "\001TIME\001";
	char m3[] = "\001TIME FOO BAR BAZ";
	char m4[] = "\001TIME 123 456 789\001";

	CHECK_RESPONSE("nick", "targ", m1, 1, "CTCP TIME response from nick: empty message");
	CHECK_RESPONSE("nick", "targ", m2, 1, "CTCP TIME response from nick: empty message");
	CHECK_RESPONSE("nick", "targ", m3, 0, "CTCP TIME response from nick: FOO BAR BAZ");
	CHECK_RESPONSE("nick", "targ", m4, 0, "CTCP TIME response from nick: 123 456 789");
}

static void
test_recv_ctcp_response_userinfo(void)
{
	char m1[] = "\001USERINFO";
	char m2[] = "\001USERINFO\001";
	char m3[] = "\001USERINFO FOO BAR BAZ";
	char m4[] = "\001USERINFO 123 456 789\001";

	CHECK_RESPONSE("nick", "targs", m1, 1, "CTCP USERINFO response from nick: empty message");
	CHECK_RESPONSE("nick", "targs", m2, 1, "CTCP USERINFO response from nick: empty message");
	CHECK_RESPONSE("nick", "targs", m3, 0, "CTCP USERINFO response from nick: FOO BAR BAZ");
	CHECK_RESPONSE("nick", "targs", m4, 0, "CTCP USERINFO response from nick: 123 456 789");
}

static void
test_recv_ctcp_response_version(void)
{
	char m1[] = "\001VERSION";
	char m2[] = "\001VERSION\001";
	char m3[] = "\001VERSION FOO BAR BAZ";
	char m4[] = "\001VERSION 123 456 789\001";

	CHECK_RESPONSE("nick", "targs", m1, 1, "CTCP VERSION response from nick: empty message");
	CHECK_RESPONSE("nick", "targs", m2, 1, "CTCP VERSION response from nick: empty message");
	CHECK_RESPONSE("nick", "targs", m3, 0, "CTCP VERSION response from nick: FOO BAR BAZ");
	CHECK_RESPONSE("nick", "targs", m4, 0, "CTCP VERSION response from nick: 123 456 789");
}

static void
test_case_insensitive(void)
{
	/* Requests */
	char m1[] = "\001clientinfo";
	char m2[] = "\001clientinfo\001";

	/* Response */
	char m3[] = "\001clientinfo FOO BAR BAZ";
	char m4[] = "\001clientinfo 123 456 789\001";

	CHECK_REQUEST("nick", "targ", m1, 0,
		"CTCP CLIENTINFO from nick",
		"NOTICE nick :\001CLIENTINFO ACTION CLIENTINFO PING SOURCE TIME VERSION\001");

	CHECK_REQUEST("nick", "targ", m2, 0,
		"CTCP CLIENTINFO from nick",
		"NOTICE nick :\001CLIENTINFO ACTION CLIENTINFO PING SOURCE TIME VERSION\001");

	CHECK_RESPONSE("nick", "targ", m3, 0, "CTCP CLIENTINFO response from nick: FOO BAR BAZ");
	CHECK_RESPONSE("nick", "targ", m4, 0, "CTCP CLIENTINFO response from nick: 123 456 789");
}

int
main(void)
{
	s = server("h1", "p1", NULL, "u1", "r1");
	c_chan = channel("chan", CHANNEL_T_CHANNEL);
	c_priv = channel("nick", CHANNEL_T_PRIVATE);
	channel_list_add(&s->clist, c_chan);
	channel_list_add(&s->clist, c_priv);
	server_nick_set(s, "mynick");

	struct testcase tests[] = {
		TESTCASE(test_recv_ctcp_request),
		TESTCASE(test_recv_ctcp_response),
#define X(cmd) TESTCASE(test_recv_ctcp_request_##cmd),
		CTCP_EXTENDED_FORMATTING
		CTCP_EXTENDED_QUERY
		CTCP_METADATA_QUERY
#undef X
#define X(cmd) TESTCASE(test_recv_ctcp_response_##cmd),
		CTCP_EXTENDED_QUERY
		CTCP_METADATA_QUERY
#undef X
		TESTCASE(test_case_insensitive)
	};

	int ret = run_tests(tests);
	server_free(s);
	return ret;
}
