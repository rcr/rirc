#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/handlers/irc_ctcp.c"
#include "src/utils/utils.c"

#define CHECK_REQUEST(P, M, R, L, S) \
	do { \
	    send_buf[0] = 0; \
	    line_buf[0] = 0; \
	    (P).trailing = (M); \
	    assert_eq(recv_ctcp_request(s, &(P)), (R)); \
	    assert_strcmp(line_buf, (L)); \
	    assert_strcmp(send_buf, (S)); \
	} while (0)

#define X(cmd) static void test_recv_ctcp_request_##cmd(void);
CTCP_HANDLERS
#undef X

#define X(cmd) static void test_recv_ctcp_response_##cmd(void);
CTCP_HANDLERS
#undef X

static char chan_buf[1024];
static char line_buf[1024];
static char send_buf[1024];
static struct channel *c_chan;
static struct channel *c_priv;
static struct server *s;

/* Mock stat.c */
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

static void
test_recv_ctcp_request(void)
{
	struct parsed_mesg pm1 = { .from = "nick" };
	struct parsed_mesg pm2 = { 0 };

	char m1[] = "";
	char m2[] = "";
	char m3[] = " ";
	char m4[] = "\001";
	char m5[] = "\001\001";
	char m6[] = "\001 \001";
	char m7[] = "\001TEST1\001";
	char m8[] = "\001TEST2 arg1 arg2\001";

	CHECK_REQUEST(pm2, m1, 1, "Received CTCP message from unknown sender", "");
	CHECK_REQUEST(pm1, m2, 1, "Received malformed CTCP message from nick", "");
	CHECK_REQUEST(pm1, m3, 1, "Received malformed CTCP message from nick", "");
	CHECK_REQUEST(pm1, m4, 1, "Received malformed CTCP message from nick", "");
	CHECK_REQUEST(pm1, m5, 1, "Received empty CTCP message from nick", "");
	CHECK_REQUEST(pm1, m6, 1, "Received empty CTCP message from nick", "");

	CHECK_REQUEST(pm1, m7, 0,
		"Received unsupported CTCP command 'TEST1' from nick",
		"NOTICE nick :\001ERRMSG Unsupported CTCP command: 'TEST1'\001");

	CHECK_REQUEST(pm1, m8, 0,
		"Received unsupported CTCP command 'TEST2' from nick",
		"NOTICE nick :\001ERRMSG Unsupported CTCP command: 'TEST2'\001");
}


static void
test_recv_ctcp_response(void)
{

}

static void
test_recv_ctcp_request_action(void)
{
	char m1[] = "\001ACTION test action 1\001";
	char m2[] = "\001ACTION test action 2\001";
	char m3[] = "\001ACTION test action 3\001";
	char m4[] = "\001ACTION test action 4\001";
	char p1[] = "mynick";
	char p2[] = "mynick";
	char p3[] = "chan";
	char p4[] = "not_a_channel";

	/* Action message to me as existing private message */
	struct parsed_mesg pm1 = { .trailing = m1, .from = "nick", .params = p1 };

	/* Action message to me as new private message */
	struct parsed_mesg pm2 = { .trailing = m2, .from = "new_priv", .params = p2 };

	/* Action message to existing channel */
	struct parsed_mesg pm3 = { .trailing = m3, .from = "nick", .params = p3 };

	/* Action message to nonexistant channel */
	struct parsed_mesg pm4 = { .trailing = m4, .from = "nick", .params = p4 };

#define CHECK_ACTION_REQUEST(P, R, C, L) \
	do { \
	    chan_buf[0] = 0; \
	    line_buf[0] = 0; \
	    assert_eq(recv_ctcp_request(s, &(P)), (R)); \
	    assert_strcmp(chan_buf, (C)); \
	    assert_strcmp(line_buf, (L)); \
	} while (0)

	CHECK_ACTION_REQUEST(pm1, 0,
		"nick",
		"nick test action 1");

	CHECK_ACTION_REQUEST(pm2, 0,
		"new_priv",
		"new_priv test action 2");

	CHECK_ACTION_REQUEST(pm3, 0,
		"chan",
		"nick test action 3");

	CHECK_ACTION_REQUEST(pm4, 1,
		"h1",
		"CTCP ACTION: target 'not_a_channel' not found");

#undef CHECK_ACTION_REQUEST
}

static void
test_recv_ctcp_request_clientinfo(void)
{
	struct parsed_mesg pm = { .from = "nick" };
	char m1[] = "\001CLIENTINFO\001";
	char m2[] = "\001CLIENTINFO unused args\001";

	CHECK_REQUEST(pm, m1, 0,
		"CTCP CLIENTINFO from nick",
		"NOTICE nick :\001CLIENTINFO ACTION CLIENTINFO PING SOURCE TIME VERSION\001");

	CHECK_REQUEST(pm, m2, 0,
		"CTCP CLIENTINFO from nick (unused args)",
		"NOTICE nick :\001CLIENTINFO ACTION CLIENTINFO PING SOURCE TIME VERSION\001");
}

static void
test_recv_ctcp_request_ping(void)
{
	struct parsed_mesg pm = { .from = "nick" };
	char m1[] = "\001PING 0\001";
	char m2[] = "\001PING 1 123 abc 0\001";

	CHECK_REQUEST(pm, m1, 0,
		"CTCP PING from nick",
		"NOTICE nick :\001PING 0\001");

	CHECK_REQUEST(pm, m2, 0,
		"CTCP PING from nick",
		"NOTICE nick :\001PING 1 123 abc 0\001");
}

static void
test_recv_ctcp_request_source(void)
{
	struct parsed_mesg pm = { .from = "nick" };
	char m1[] = "\001SOURCE\001";
	char m2[] = "\001SOURCE unused args\001";

	CHECK_REQUEST(pm, m1, 0,
		"CTCP SOURCE from nick",
		"NOTICE nick :\001SOURCE rcr.io/rirc\001");

	CHECK_REQUEST(pm, m2, 0,
		"CTCP SOURCE from nick (unused args)",
		"NOTICE nick :\001SOURCE rcr.io/rirc\001");
}

static void
test_recv_ctcp_request_time(void)
{
	struct parsed_mesg pm = { .from = "nick" };
	char m1[] = "\001TIME\001";
	char m2[] = "\001TIME unused args\001";

	CHECK_REQUEST(pm, m1, 0,
		"CTCP TIME from nick",
		"NOTICE nick :\001TIME 1970-01-01T00:00:00Z+0000\001");

	CHECK_REQUEST(pm, m2, 0,
		"CTCP TIME from nick (unused args)",
		"NOTICE nick :\001TIME 1970-01-01T00:00:00Z+0000\001");
}

static void
test_recv_ctcp_request_version(void)
{
	struct parsed_mesg pm = { .from = "nick" };
	char m1[] = "\001VERSION\001";
	char m2[] = "\001VERSION unused args\001";

	CHECK_REQUEST(pm, m1, 0,
		"CTCP VERSION from nick",
		"NOTICE nick :\001VERSION rirc v"VERSION" ("__DATE__")\001");

	CHECK_REQUEST(pm, m2, 0,
		"CTCP VERSION from nick (unused args)",
		"NOTICE nick :\001VERSION rirc v"VERSION" ("__DATE__")\001");
}

static void
test_recv_ctcp_response_action(void)
{

}

static void
test_recv_ctcp_response_clientinfo(void)
{

}

static void
test_recv_ctcp_response_ping(void)
{

}

static void
test_recv_ctcp_response_source(void)
{

}

static void
test_recv_ctcp_response_time(void)
{

}

static void
test_recv_ctcp_response_version(void)
{

}

int
main(void)
{
	int ret;
	s = server("h1", "p1", NULL, "u1", "r1");
	c_chan = channel("chan", CHANNEL_T_CHANNEL);
	c_priv = channel("nick", CHANNEL_T_PRIVATE);
	channel_list_add(&s->clist, c_chan);
	channel_list_add(&s->clist, c_priv);
	server_nick_set(s, "mynick");

	testcase tests[] = {
		TESTCASE(test_recv_ctcp_request),
		TESTCASE(test_recv_ctcp_response),
#define X(cmd) TESTCASE(test_recv_ctcp_request_##cmd),
		CTCP_HANDLERS
#undef X
#define X(cmd) TESTCASE(test_recv_ctcp_response_##cmd),
		CTCP_HANDLERS
#undef X
	};

	ret = run_tests(tests);
	server_free(s);
	return ret;
}
