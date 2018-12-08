#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/handlers/irc_send.c"
#include "src/utils/utils.c"

#define X(cmd) static void test_send_##cmd(void);
SEND_HANDLERS
#undef X

#define X(cmd) static void test_send_ctcp_##cmd(void);
SEND_CTCP_HANDLERS
#undef X

#define CHECK_SEND(C, M, R, F, S) \
	do { \
	    send_buf[0] = 0; \
	    fail_buf[0] = 0; \
		assert_eq(irc_send_command(s, (C), (M)), (R)); \
	    assert_strcmp(fail_buf, (F)); \
	    assert_strcmp(send_buf, (S)); \
	} while (0)

static char send_buf[1024];
static char fail_buf[1024];
static struct channel *c_chan;
static struct channel *c_priv;
static struct channel *c_serv;
static struct server *s;

/* Mock stat.c */
void
newlinef(struct channel *c, enum buffer_line_t t, const char *f, const char *fmt, ...)
{
	va_list ap;

	UNUSED(c);
	UNUSED(f);
	UNUSED(t);

	va_start(ap, fmt);
	assert_gt(vsnprintf(fail_buf, sizeof(fail_buf), fmt, ap), 0);
	va_end(ap);
}

void
newline(struct channel *c, enum buffer_line_t t, const char *f, const char *fmt)
{
	UNUSED(c);
	UNUSED(f);
	UNUSED(t);

	assert_gt(snprintf(fail_buf, sizeof(fail_buf), fmt, sizeof(fail_buf)), 0);
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
test_irc_send_command(void)
{
	;
}

static void
test_irc_send_privmsg(void)
{
	;
}

static void
test_send_ctcp_action(void)
{
	char m1[] = "ctcp-action test action";
	char m2[] = "ctcp-action test action";
	char m3[] = "ctcp-action test action";

	CHECK_SEND(c_chan, m1, 0, "", "PRIVMSG chan :\001ACTION test action\001");
	CHECK_SEND(c_priv, m2, 0, "", "PRIVMSG priv :\001ACTION test action\001");
	CHECK_SEND(c_serv, m3, 1, "This is not a channel", "");
}

static void
test_send_ctcp_clientinfo(void)
{
	;
}

static void
test_send_ctcp_finger(void)
{
	;
}

static void
test_send_ctcp_ping(void)
{
	;
}

static void
test_send_ctcp_source(void)
{
	;
}

static void
test_send_ctcp_time(void)
{
	;
}

static void
test_send_ctcp_userinfo(void)
{
	;
}

static void
test_send_ctcp_version(void)
{
	;
}

static void
test_send_join(void)
{
	;
}

static void
test_send_notice(void)
{
	;
}

static void
test_send_part(void)
{
	;
}

static void
test_send_privmsg(void)
{
	;
}

static void
test_send_quit(void)
{
	;
}

static void
test_send_topic(void)
{
	;
}

int
main(void)
{
	c_chan = channel("chan", CHANNEL_T_CHANNEL);
	c_priv = channel("priv", CHANNEL_T_PRIVATE);
	c_serv = channel("serv", CHANNEL_T_SERVER);
	s = server("h1", "p1", NULL, "u1", "r1");
	s->nick = "nick";

	testcase tests[] = {
		TESTCASE(test_irc_send_command),
		TESTCASE(test_irc_send_privmsg),
#define X(cmd) TESTCASE(test_send_##cmd),
		SEND_HANDLERS
#undef X
#define X(cmd) TESTCASE(test_send_ctcp_##cmd),
		SEND_CTCP_HANDLERS
#undef X
	};

	return run_tests(tests);
}
