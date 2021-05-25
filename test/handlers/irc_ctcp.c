#include "test/test.h"

#include <ctype.h>

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/ircv3.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/handlers/irc_ctcp.c"
#include "src/handlers/irc_recv.c"
#include "src/handlers/ircv3.c"
#include "src/utils/utils.c"
#include "test/draw.mock.c"

#include "test/io.mock.c"
#include "test/state.mock.c"

#define IRC_MESSAGE_PARSE(S) \
	char TOKEN(buf, __LINE__)[] = S; \
	assert_eq(irc_message_parse(&m, TOKEN(buf, __LINE__)), 0);

#define CHECK_REQUEST(M, RET, LINE_N, SEND_N, LINE, SEND) \
	do { \
		mock_reset_io(); \
		mock_reset_state(); \
		IRC_MESSAGE_PARSE(M); \
		assert_eq(irc_recv(s, &m), (RET)); \
		assert_eq(mock_line_n, (LINE_N)); \
		assert_eq(mock_send_n, (SEND_N)); \
		assert_strcmp(mock_line[0], (LINE)); \
		assert_strcmp(mock_send[0], (SEND)); \
	} while (0)

#define CHECK_RESPONSE(M, RET, LINE) \
	do { \
		mock_reset_io(); \
		mock_reset_state(); \
		IRC_MESSAGE_PARSE(M); \
		assert_eq(irc_recv(s, &m), (RET)); \
		assert_eq(mock_line_n, 1); \
		assert_eq(mock_send_n, 0); \
		assert_strcmp(mock_line[0], (LINE)); \
	} while (0)

#define X(cmd) static void test_recv_ctcp_request_##cmd(void);
CTCP_EXTENDED_FORMATTING
CTCP_EXTENDED_QUERY
CTCP_METADATA_QUERY
#undef X

#define X(cmd) static void test_recv_ctcp_response_##cmd(void);
CTCP_EXTENDED_QUERY
CTCP_METADATA_QUERY
#undef X

static struct channel *c_chan;
static struct channel *c_priv;
static struct irc_message m;
static struct server *s;

static void
test_recv_ctcp_request(void)
{
	/* test malformed */
	mock_reset_io();
	mock_reset_state();
	assert_eq(ctcp_request(s, NULL, "me", "\001TEST"), 1);
	assert_eq(mock_line_n, 1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "Received CTCP from unknown sender");

	mock_reset_io();
	mock_reset_state();
	assert_eq(ctcp_request(s, "nick", "me", ""), 1);
	assert_eq(mock_line_n, 1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "Received malformed CTCP from nick");

	mock_reset_io();
	mock_reset_state();
	assert_eq(ctcp_request(s, "nick", "me", " "), 1);
	assert_eq(mock_line_n, 1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "Received malformed CTCP from nick");

	/* test empty */
	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001", 1, 1, 0,
		"Received empty CTCP from nick", "");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001\001", 1, 1, 0,
		"Received empty CTCP from nick", "");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001 \001", 1, 1, 0,
		"Received empty CTCP from nick", "");

	/* test unsupported */
	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001TEST1", 1, 1, 0,
		"Received unsupported CTCP request 'TEST1' from nick", "");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001TEST2\001", 1, 1, 0,
		"Received unsupported CTCP request 'TEST2' from nick", "");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001TEST3 arg1 arg2\001", 1, 1, 0,
		"Received unsupported CTCP request 'TEST3' from nick", "");

	/* test case insensitive */
	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001clientinfo", 0, 1, 1,
		"CTCP CLIENTINFO from nick",
		"NOTICE nick :\001CLIENTINFO " CTCP_CLIENTINFO "\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001clientinfo\001", 0, 1, 1,
		"CTCP CLIENTINFO from nick",
		"NOTICE nick :\001CLIENTINFO " CTCP_CLIENTINFO "\001");
}

static void
test_recv_ctcp_response(void)
{
	/* test malformed */
	mock_reset_io();
	mock_reset_state();
	assert_eq(ctcp_response(s, NULL, "me", "\001TEST"), 1);
	assert_eq(mock_line_n, 1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "Received CTCP from unknown sender");

	mock_reset_io();
	mock_reset_state();
	assert_eq(ctcp_response(s, "nick", "me", ""), 1);
	assert_eq(mock_line_n, 1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "Received malformed CTCP from nick");

	mock_reset_io();
	mock_reset_state();
	assert_eq(ctcp_response(s, "nick", "me", " "), 1);
	assert_eq(mock_line_n, 1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "Received malformed CTCP from nick");

	/* test empty */
	CHECK_RESPONSE(":nick!user@host NOTICE me :\001", 1,
		"Received empty CTCP from nick");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001\001", 1,
		"Received empty CTCP from nick");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001 \001", 1,
		"Received empty CTCP from nick");

	/* test unsupported */
	CHECK_RESPONSE(":nick!user@host NOTICE me :\001TEST1", 1,
		"Received unsupported CTCP response 'TEST1' from nick");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001TEST2\001", 1,
		"Received unsupported CTCP response 'TEST2' from nick");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001TEST3 arg1 arg2\001", 1,
		"Received unsupported CTCP response 'TEST3' from nick");

	/* test case insensitive */
	CHECK_RESPONSE(":nick!user@host NOTICE me :\001clientinfo FOO BAR BAZ", 0,
		"CTCP CLIENTINFO response from nick: FOO BAR BAZ");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001clientinfo 123 456 789\001", 0,
		"CTCP CLIENTINFO response from nick: 123 456 789");
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
		mock_reset_io(); \
		mock_reset_state(); \
		assert_eq(ctcp_request(s, (F), (T), (M)), (R)); \
		assert_eq(mock_line_n, 1); \
		assert_eq(mock_send_n, 0); \
		assert_strcmp(mock_chan[0], (C)); \
		assert_strcmp(mock_line[0], (L)); \
	} while (0)

	/* Action message to me as existing private message */
	CHECK_ACTION_REQUEST("nick", "me", m1, 0, "nick", "nick test action 1");

	/* Action message to me as new private message */
	CHECK_ACTION_REQUEST("new_priv", "me", m2, 0, "new_priv", "new_priv test action 2");

	/* Action message to existing channel */
	CHECK_ACTION_REQUEST("nick", "chan", m3, 0, "chan", "nick test action 3");

	/* Empty action messages */
	CHECK_ACTION_REQUEST("nick", "me", m4, 0, "nick", "nick");
	CHECK_ACTION_REQUEST("nick", "me", m5, 0, "nick", "nick");
	CHECK_ACTION_REQUEST("nick", "me", m6, 0, "nick", "nick");

	/* Action message to nonexistant channel */
	CHECK_ACTION_REQUEST("nick", "not_a_chan", m7, 1, "h1", "CTCP ACTION: target 'not_a_chan' not found");

	/* Action message with no target */
	CHECK_ACTION_REQUEST("nick", NULL, m8, 1, "h1", "CTCP ACTION: target is NULL");

#undef CHECK_ACTION_REQUEST
}

static void
test_recv_ctcp_request_clientinfo(void)
{
	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001CLIENTINFO", 0, 1, 1,
		"CTCP CLIENTINFO from nick",
		"NOTICE nick :\001CLIENTINFO " CTCP_CLIENTINFO "\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001CLIENTINFO\001", 0, 1, 1,
		"CTCP CLIENTINFO from nick",
		"NOTICE nick :\001CLIENTINFO " CTCP_CLIENTINFO "\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001CLIENTINFO unused args\001", 0, 1, 1,
		"CTCP CLIENTINFO from nick (unused args)",
		"NOTICE nick :\001CLIENTINFO " CTCP_CLIENTINFO "\001");

	char *p, clientinfo[] = CTCP_CLIENTINFO;

	for (p = clientinfo; *p; p++)
		*p = tolower(*p);

	#define X(cmd) assert_ptr_not_null(strstr(clientinfo, #cmd));
		CTCP_EXTENDED_FORMATTING
		CTCP_EXTENDED_QUERY
		CTCP_METADATA_QUERY
	#undef X
}

static void
test_recv_ctcp_request_finger(void)
{
	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001FINGER", 0, 1, 1,
		"CTCP FINGER from nick",
		"NOTICE nick :\001FINGER rirc v"VERSION" ("__DATE__")\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001FINGER\001", 0, 1, 1,
		"CTCP FINGER from nick",
		"NOTICE nick :\001FINGER rirc v"VERSION" ("__DATE__")\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001FINGER unused args\001", 0, 1, 1,
		"CTCP FINGER from nick (unused args)",
		"NOTICE nick :\001FINGER rirc v"VERSION" ("__DATE__")\001");
}

static void
test_recv_ctcp_request_ping(void)
{
	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001PING", 0, 1, 1,
		"CTCP PING from nick",
		"NOTICE nick :\001PING\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001PING 0\001", 0, 1, 1,
		"CTCP PING from nick (0)",
		"NOTICE nick :\001PING 0\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001PING 1 123 abc\001", 0, 1, 1,
		"CTCP PING from nick (1 123 abc)",
		"NOTICE nick :\001PING 1 123 abc\001");
}

static void
test_recv_ctcp_request_source(void)
{
	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001SOURCE", 0, 1, 1,
		"CTCP SOURCE from nick",
		"NOTICE nick :\001SOURCE https://rcr.io/rirc\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001SOURCE\001", 0, 1, 1,
		"CTCP SOURCE from nick",
		"NOTICE nick :\001SOURCE https://rcr.io/rirc\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001SOURCE unused args\001", 0, 1, 1,
		"CTCP SOURCE from nick (unused args)",
		"NOTICE nick :\001SOURCE https://rcr.io/rirc\001");
}

static void
test_recv_ctcp_request_time(void)
{
	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001TIME", 0, 1, 1,
		"CTCP TIME from nick",
		"NOTICE nick :\001TIME 1970-01-01T00:00:00\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001TIME\001", 0, 1, 1,
		"CTCP TIME from nick",
		"NOTICE nick :\001TIME 1970-01-01T00:00:00\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001TIME unused args\001", 0, 1, 1,
		"CTCP TIME from nick (unused args)",
		"NOTICE nick :\001TIME 1970-01-01T00:00:00\001");
}

static void
test_recv_ctcp_request_userinfo(void)
{
	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001USERINFO", 0, 1, 1,
		"CTCP USERINFO from nick",
		"NOTICE nick :\001USERINFO me (r1)\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001USERINFO\001", 0, 1, 1,
		"CTCP USERINFO from nick",
		"NOTICE nick :\001USERINFO me (r1)\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001USERINFO unused args\001", 0, 1, 1,
		"CTCP USERINFO from nick (unused args)",
		"NOTICE nick :\001USERINFO me (r1)\001");
}

static void
test_recv_ctcp_request_version(void)
{
	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001VERSION", 0, 1, 1,
		"CTCP VERSION from nick",
		"NOTICE nick :\001VERSION rirc v"VERSION" ("__DATE__")\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001VERSION\001", 0, 1, 1,
		"CTCP VERSION from nick",
		"NOTICE nick :\001VERSION rirc v"VERSION" ("__DATE__")\001");

	CHECK_REQUEST(":nick!user@host PRIVMSG me :\001VERSION unused args\001", 0, 1, 1,
		"CTCP VERSION from nick (unused args)",
		"NOTICE nick :\001VERSION rirc v"VERSION" ("__DATE__")\001");
}

static void
test_recv_ctcp_response_action(void)
{
	/* CTCP `extended formatting` messages generate no response */

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001ACTION", 1,
		"Received unsupported CTCP response 'ACTION' from nick");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001ACTION\001", 1,
		"Received unsupported CTCP response 'ACTION' from nick");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001ACTION foo bar baz\001", 1,
		"Received unsupported CTCP response 'ACTION' from nick");
}

static void
test_recv_ctcp_response_clientinfo(void)
{
	CHECK_RESPONSE(":nick!user@host NOTICE me :\001CLIENTINFO", 1,
		"CTCP CLIENTINFO response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001CLIENTINFO\001", 1,
		"CTCP CLIENTINFO response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001CLIENTINFO FOO BAR BAZ", 0,
		"CTCP CLIENTINFO response from nick: FOO BAR BAZ");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001CLIENTINFO 123 456 789\001", 0,
		"CTCP CLIENTINFO response from nick: 123 456 789");
}

static void
test_recv_ctcp_response_finger(void)
{
	CHECK_RESPONSE(":nick!user@host NOTICE me :\001FINGER", 1,
		"CTCP FINGER response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001FINGER\001", 1,
		"CTCP FINGER response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001FINGER FOO BAR BAZ", 0,
		"CTCP FINGER response from nick: FOO BAR BAZ");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001FINGER 123 456 789\001", 0,
		"CTCP FINGER response from nick: 123 456 789");
}

static void
test_recv_ctcp_response_ping(void)
{
	CHECK_RESPONSE(":nick!user@host NOTICE me :\001PING", 1,
		"CTCP PING response from nick: sec is NULL");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001PING 123", 1,
		"CTCP PING response from nick: usec is NULL");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001PING 1a3 345\001", 1,
		"CTCP PING response from nick: sec is invalid");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001PING 123 345a\001", 1,
		"CTCP PING response from nick: usec is invalid");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001PING 125 456789\001", 1,
		"CTCP PING response from nick: invalid timestamp");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001PING 123 567890\001", 1,
		"CTCP PING response from nick: invalid timestamp");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001PING 120 456789\001", 0,
		"CTCP PING response from nick: 3.0s");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001PING 120 111111\001", 0,
		"CTCP PING response from nick: 3.345678s");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001PING 120 000000", 0,
		"CTCP PING response from nick: 3.456789s");
}

static void
test_recv_ctcp_response_source(void)
{
	CHECK_RESPONSE(":nick!user@host NOTICE me :\001SOURCE", 1,
		"CTCP SOURCE response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001SOURCE\001", 1,
		"CTCP SOURCE response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001SOURCE FOO BAR BAZ", 0,
		"CTCP SOURCE response from nick: FOO BAR BAZ");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001SOURCE 123 456 789\001", 0,
		"CTCP SOURCE response from nick: 123 456 789");
}

static void
test_recv_ctcp_response_time(void)
{
	CHECK_RESPONSE(":nick!user@host NOTICE me :\001TIME", 1,
		"CTCP TIME response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001TIME\001", 1,
		"CTCP TIME response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001TIME FOO BAR BAZ", 0,
		"CTCP TIME response from nick: FOO BAR BAZ");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001TIME 123 456 789\001", 0,
		"CTCP TIME response from nick: 123 456 789");
}

static void
test_recv_ctcp_response_userinfo(void)
{
	CHECK_RESPONSE(":nick!user@host NOTICE me :\001USERINFO", 1,
		"CTCP USERINFO response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001USERINFO\001", 1,
		"CTCP USERINFO response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001USERINFO FOO BAR BAZ", 0,
		"CTCP USERINFO response from nick: FOO BAR BAZ");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001USERINFO 123 456 789\001", 0,
		"CTCP USERINFO response from nick: 123 456 789");
}

static void
test_recv_ctcp_response_version(void)
{
	CHECK_RESPONSE(":nick!user@host NOTICE me :\001VERSION", 1,
		"CTCP VERSION response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001VERSION\001", 1,
		"CTCP VERSION response from nick: empty message");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001VERSION FOO BAR BAZ", 0,
		"CTCP VERSION response from nick: FOO BAR BAZ");

	CHECK_RESPONSE(":nick!user@host NOTICE me :\001VERSION 123 456 789\001", 0,
		"CTCP VERSION response from nick: 123 456 789");
}

int
main(void)
{
	c_chan = channel("chan", CHANNEL_T_CHANNEL);
	c_priv = channel("nick", CHANNEL_T_PRIVMSG);

	s = server("h1", "p1", NULL, "u1", "r1");

	if (!s || !c_chan || !c_priv)
		test_abort_main("Failed test setup");

	channel_list_add(&s->clist, c_chan);
	channel_list_add(&s->clist, c_priv);

	server_nick_set(s, "me");

	struct testcase tests[] = {
		TESTCASE(test_recv_ctcp_request),
		TESTCASE(test_recv_ctcp_response),
#define X(cmd) TESTCASE(test_recv_ctcp_request_##cmd),
		CTCP_EXTENDED_FORMATTING
		CTCP_EXTENDED_QUERY
		CTCP_METADATA_QUERY
#undef X
#define X(cmd) TESTCASE(test_recv_ctcp_response_##cmd),
		CTCP_EXTENDED_FORMATTING
		CTCP_EXTENDED_QUERY
		CTCP_METADATA_QUERY
#undef X
	};

	int ret = run_tests(tests);

	server_free(s);

	return ret;
}
