#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/ircv3.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/handlers/irc_send.c"
#include "src/utils/utils.c"

#include "test/io.mock.c"
#include "test/state.mock.c"

#define CHECK_SEND_PRIVMSG(C, M, RET, LINE_N, SEND_N, LINE, SEND) \
	do { \
		mock_reset_io(); \
		mock_reset_state(); \
		assert_eq(irc_send_message(s, (C), (M)), (RET)); \
		assert_eq(mock_line_n, (LINE_N)); \
		assert_eq(mock_send_n, (SEND_N)); \
		assert_strcmp(mock_line[0], (LINE)); \
		assert_strcmp(mock_send[0], (SEND)); \
	} while (0)

#define CHECK_SEND_COMMAND(C, M, RET, LINE_N, SEND_N, LINE, SEND) \
	do { \
		mock_reset_io(); \
		mock_reset_state(); \
		assert_eq(irc_send_command(s, (C), (M)), (RET)); \
		assert_eq(mock_line_n, (LINE_N)); \
		assert_eq(mock_send_n, (SEND_N)); \
		assert_strcmp(mock_line[0], (LINE)); \
		assert_strcmp(mock_send[0], (SEND)); \
	} while (0)

#define X(cmd) static void test_send_##cmd(void);
SEND_HANDLERS
#undef X

#define X(cmd) static void test_send_ctcp_##cmd(void);
SEND_CTCP_HANDLERS
#undef X

static struct channel *c_chan;
static struct channel *c_priv;
static struct channel *c_serv;
static struct server *s;

static void
test_irc_send_command(void)
{
	char m1[] = "";
	char m2[] = " test";
	char m3[] = "test";
	char m4[] = "test arg1 arg2 arg3";
	char m5[] = "privmsg targ test message";
	char m6[] = "privmsg not registered";

	mock_reset_io();
	mock_reset_state();
	assert_eq(irc_send_command(NULL, c_chan, ""), 1);
	assert_eq(mock_line_n, 1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "This is not a server");

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Messages beginning with '/' require a command", "");
	CHECK_SEND_COMMAND(c_chan, m2, 1, 1, 0, "Messages beginning with '/' require a command", "");
	CHECK_SEND_COMMAND(c_chan, m3, 0, 0, 1, "", "TEST");
	CHECK_SEND_COMMAND(c_chan, m4, 0, 0, 1, "", "TEST arg1 arg2 arg3");
	CHECK_SEND_COMMAND(c_chan, m5, 0, 1, 1, "test message", "PRIVMSG targ :test message");

	s->registered = 0;

	CHECK_SEND_COMMAND(c_chan, m6, 1, 1, 0, "Not registered with server", "");

	s->registered = 1;
}

static void
test_irc_send_message(void)
{
	char m1[] = "chan test 1";
	char m2[] = "serv test 2";
	char m3[] = "priv test 3";
	char m4[] = "chan test 4";
	char m5[] = "chan not registered";

	CHECK_SEND_PRIVMSG(c_chan, m1, 1, 1, 0, "Not on channel", "");
	CHECK_SEND_PRIVMSG(c_serv, m2, 1, 1, 0, "This is not a channel", "");

	c_chan->joined = 1;

	CHECK_SEND_PRIVMSG(c_priv, m3, 0, 1, 1, "priv test 3", "PRIVMSG priv :priv test 3");
	CHECK_SEND_PRIVMSG(c_chan, m4, 0, 1, 1, "chan test 4", "PRIVMSG chan :chan test 4");
	CHECK_SEND_PRIVMSG(c_chan, "", 1, 1, 0, "Message is empty", "");

	mock_reset_io();
	mock_reset_state();
	assert_eq(irc_send_message(NULL, c_chan, "test"), 1);
	assert_strcmp(mock_line[0], "This is not a server");
	assert_strcmp(mock_send[0], "");

	s->registered = 0;

	CHECK_SEND_PRIVMSG(c_chan, m5, 1, 1, 0, "Not registered with server", "");

	s->registered = 1;
}

static void
test_send_away(void)
{
	char m1[] = "away";
	char m2[] = "away ";
	char m3[] = "away testing away message";

	CHECK_SEND_COMMAND(c_chan, m1, 0, 0, 1, "", "AWAY");
	CHECK_SEND_COMMAND(c_chan, m2, 0, 0, 1, "", "AWAY");
	CHECK_SEND_COMMAND(c_chan, m3, 0, 0, 1, "", "AWAY :testing away message");
}

static void
test_send_names(void)
{
	char m1[] = "names";
	char m2[] = "names target";
	char m3[] = "names";
	char m4[] = "names target";
	char m5[] = "names";
	char m6[] = "names target";

	CHECK_SEND_COMMAND(c_chan, m1, 0, 0, 1, "", "NAMES chan");
	CHECK_SEND_COMMAND(c_chan, m2, 0, 0, 1, "", "NAMES target");

	CHECK_SEND_COMMAND(c_priv, m3, 0, 0, 1, "", "NAMES");
	CHECK_SEND_COMMAND(c_priv, m4, 0, 0, 1, "", "NAMES target");

	CHECK_SEND_COMMAND(c_serv, m5, 0, 0, 1, "", "NAMES");
	CHECK_SEND_COMMAND(c_serv, m6, 0, 0, 1, "", "NAMES target");
}

static void
test_send_notice(void)
{
	char m1[] = "notice";
	char m2[] = "notice test1";
	char m3[] = "notice test2 ";
	char m4[] = "notice test3  ";
	char m5[] = "notice test4 test notice message";

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /notice <target> <message>", "");
	CHECK_SEND_COMMAND(c_chan, m2, 1, 1, 0, "Usage: /notice <target> <message>", "");
	CHECK_SEND_COMMAND(c_chan, m3, 1, 1, 0, "Usage: /notice <target> <message>", "");
	CHECK_SEND_COMMAND(c_chan, m4, 0, 0, 1, "", "NOTICE test3 : ");
	CHECK_SEND_COMMAND(c_chan, m5, 0, 0, 1, "", "NOTICE test4 :test notice message");
}

static void
test_send_part(void)
{
	char m1[] = "part";
	char m2[] = "part";
	char m3[] = "part";
	char m4[] = "part test part message";

	CHECK_SEND_COMMAND(c_serv, m1, 1, 1, 0, "This is not a channel", "");
	CHECK_SEND_COMMAND(c_priv, m2, 1, 1, 0, "This is not a channel", "");
	CHECK_SEND_COMMAND(c_chan, m3, 0, 0, 1, "", "PART chan :" DEFAULT_PART_MESG);
	CHECK_SEND_COMMAND(c_chan, m4, 0, 0, 1, "", "PART chan :test part message");
}

static void
test_send_privmsg(void)
{
	char m1[] = "privmsg";
	char m2[] = "privmsg chan";
	char m3[] = "privmsg chan ";

	struct channel *c1;
	struct channel *c2;
	struct channel *c3;
	struct channel *c4;

	assert_eq(s->clist.count, 3);

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /privmsg <target> <message>", "");
	CHECK_SEND_COMMAND(c_chan, m2, 1, 1, 0, "Usage: /privmsg <target> <message>", "");
	CHECK_SEND_COMMAND(c_chan, m3, 1, 1, 0, "Usage: /privmsg <target> <message>", "");

	/* test sending to existing channel */
	char m4[] = "privmsg chan  ";
	char m5[] = "privmsg chan test 1";

	CHECK_SEND_COMMAND(c_chan, m4, 0, 1, 1, " ",      "PRIVMSG chan : ");
	CHECK_SEND_COMMAND(c_chan, m5, 0, 1, 1, "test 1", "PRIVMSG chan :test 1");

	assert_eq(s->clist.count, 3);

	/* test sending to single new target */
	char m6[] = "privmsg #new1 test 2";

	CHECK_SEND_COMMAND(c_chan, m6, 0, 1, 1, "test 2", "PRIVMSG #new1 :test 2");

	if (!(c1 = channel_list_get(&(s->clist), "#new1", s->casemapping)))
		test_abort("channel '#new1' not found");

	assert_eq(c1->type, CHANNEL_T_CHANNEL);
	assert_eq(s->clist.count, 4);

	/* test sending to multiple new targets */
	char m7[] = "privmsg #new2,priv1,#new3,priv2 test 3";

	CHECK_SEND_COMMAND(c_chan, m7, 0, 4, 1, "test 3", "PRIVMSG #new2,priv1,#new3,priv2 :test 3");

	if (!(c1 = channel_list_get(&(s->clist), "#new2", s->casemapping)))
		test_abort("channel '#new2' not found");

	if (!(c2 = channel_list_get(&(s->clist), "priv1", s->casemapping)))
		test_abort("channel 'priv1' not found");

	if (!(c3 = channel_list_get(&(s->clist), "#new3", s->casemapping)))
		test_abort("channel '#new3' not found");

	if (!(c4 = channel_list_get(&(s->clist), "priv2", s->casemapping)))
		test_abort("channel 'priv2' not found");

	assert_eq(c1->type, CHANNEL_T_CHANNEL);
	assert_eq(c2->type, CHANNEL_T_PRIVMSG);
	assert_eq(c3->type, CHANNEL_T_CHANNEL);
	assert_eq(c4->type, CHANNEL_T_PRIVMSG);
	assert_eq(s->clist.count, 8);

	/* test with some duplicates channels */
	char m8[] = "privmsg priv3,priv1,priv2 test 4";

	CHECK_SEND_COMMAND(c_chan, m8, 0, 3, 1, "test 4", "PRIVMSG priv3,priv1,priv2 :test 4");

	if (!(c1 = channel_list_get(&(s->clist), "priv3", s->casemapping)))
		test_abort("channel 'priv3' not found");

	assert_eq(c1->type, CHANNEL_T_PRIVMSG);
	assert_eq(s->clist.count, 9);
}

static void
test_send_quit(void)
{
	char m1[] = "quit";
	char m2[] = "quit";
	char m3[] = "quit";
	char m4[] = "quit test quit message";

	CHECK_SEND_COMMAND(c_serv, m1, 0, 0, 1, "", "QUIT :" DEFAULT_QUIT_MESG);
	CHECK_SEND_COMMAND(c_priv, m2, 0, 0, 1, "", "QUIT :" DEFAULT_QUIT_MESG);
	CHECK_SEND_COMMAND(c_chan, m3, 0, 0, 1, "", "QUIT :" DEFAULT_QUIT_MESG);
	CHECK_SEND_COMMAND(c_chan, m4, 0, 0, 1, "", "QUIT :test quit message");
}

static void
test_send_topic(void)
{
	char m1[] = "topic";
	char m2[] = "topic";
	char m3[] = "topic";
	char m4[] = "topic test new topic";

	CHECK_SEND_COMMAND(c_serv, m1, 1, 1, 0, "This is not a channel", "");
	CHECK_SEND_COMMAND(c_priv, m2, 1, 1, 0, "This is not a channel", "");
	CHECK_SEND_COMMAND(c_chan, m3, 0, 0, 1, "", "TOPIC chan");
	CHECK_SEND_COMMAND(c_chan, m4, 0, 0, 1, "", "TOPIC chan :test new topic");
}

static void
test_send_topic_unset(void)
{
	char m1[] = "topic-unset";
	char m2[] = "topic-unset";
	char m3[] = "topic-unset test";
	char m4[] = "topic-unset";
	char m5[] = "topic-unset ";

	CHECK_SEND_COMMAND(c_serv, m1, 1, 1, 0, "This is not a channel", "");
	CHECK_SEND_COMMAND(c_priv, m2, 1, 1, 0, "This is not a channel", "");
	CHECK_SEND_COMMAND(c_chan, m3, 1, 1, 0, "Usage: /topic-unset", "");
	CHECK_SEND_COMMAND(c_chan, m4, 0, 0, 1, "", "TOPIC chan :");
	CHECK_SEND_COMMAND(c_chan, m5, 0, 0, 1, "", "TOPIC chan :");
}

static void
test_send_who(void)
{
	char m1[] = "who";
	char m2[] = "who";
	char m3[] = "who";
	char m4[] = "who targ";

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /who <target>", "");
	CHECK_SEND_COMMAND(c_serv, m2, 1, 1, 0, "Usage: /who <target>", "");
	CHECK_SEND_COMMAND(c_priv, m3, 0, 0, 1, "", "WHO priv");
	CHECK_SEND_COMMAND(c_priv, m4, 0, 0, 1, "", "WHO targ");
}

static void
test_send_whois(void)
{
	char m1[] = "whois";
	char m2[] = "whois";
	char m3[] = "whois";
	char m4[] = "whois targ";

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /whois <target>", "");
	CHECK_SEND_COMMAND(c_serv, m2, 1, 1, 0, "Usage: /whois <target>", "");
	CHECK_SEND_COMMAND(c_priv, m3, 0, 0, 1, "", "WHOIS priv");
	CHECK_SEND_COMMAND(c_priv, m4, 0, 0, 1, "", "WHOIS targ");
}

static void
test_send_whowas(void)
{
	char m1[] = "whowas";
	char m2[] = "whowas";
	char m3[] = "whowas";
	char m4[] = "whowas targ";
	char m5[] = "whowas targ 5";

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /whowas <target> [count]", "");
	CHECK_SEND_COMMAND(c_serv, m2, 1, 1, 0, "Usage: /whowas <target> [count]", "");
	CHECK_SEND_COMMAND(c_priv, m3, 0, 0, 1, "", "WHOWAS priv");
	CHECK_SEND_COMMAND(c_priv, m4, 0, 0, 1, "", "WHOWAS targ");
	CHECK_SEND_COMMAND(c_priv, m5, 0, 0, 1, "", "WHOWAS targ 5");
}

static void
test_send_ctcp_action(void)
{
	char m1[] = "ctcp-action";
	char m2[] = "ctcp-action target";
	char m3[] = "ctcp-action target ";
	char m4[] = "ctcp-action target action message";

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /ctcp-action <target> <text>", "");
	CHECK_SEND_COMMAND(c_chan, m2, 1, 1, 0, "Usage: /ctcp-action <target> <text>", "");
	CHECK_SEND_COMMAND(c_chan, m3, 1, 1, 0, "Usage: /ctcp-action <target> <text>", "");
	CHECK_SEND_COMMAND(c_chan, m4, 0, 0, 1, "", "PRIVMSG target :\001ACTION action message\001");
}

static void
test_send_ctcp_clientinfo(void)
{
	char m1[] = "ctcp-clientinfo";
	char m2[] = "ctcp-clientinfo";
	char m3[] = "ctcp-clientinfo";
	char m4[] = "ctcp-clientinfo targ";

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /ctcp-clientinfo <target>", "");
	CHECK_SEND_COMMAND(c_serv, m2, 1, 1, 0, "Usage: /ctcp-clientinfo <target>", "");
	CHECK_SEND_COMMAND(c_priv, m3, 0, 0, 1, "", "PRIVMSG priv :\001CLIENTINFO\001");
	CHECK_SEND_COMMAND(c_priv, m4, 0, 0, 1, "", "PRIVMSG targ :\001CLIENTINFO\001");
}

static void
test_send_ctcp_finger(void)
{
	char m1[] = "ctcp-finger";
	char m2[] = "ctcp-finger";
	char m3[] = "ctcp-finger";
	char m4[] = "ctcp-finger targ";

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /ctcp-finger <target>", "");
	CHECK_SEND_COMMAND(c_serv, m2, 1, 1, 0, "Usage: /ctcp-finger <target>", "");
	CHECK_SEND_COMMAND(c_priv, m3, 0, 0, 1, "", "PRIVMSG priv :\001FINGER\001");
	CHECK_SEND_COMMAND(c_priv, m4, 0, 0, 1, "", "PRIVMSG targ :\001FINGER\001");
}

static void
test_send_ctcp_ping(void)
{
	char m1[] = "ctcp-ping";
	char m2[] = "ctcp-ping";
	char m3[] = "ctcp-ping";
	char m4[] = "ctcp-ping targ";

	char *p1;
	char *p2;
	const char *arg1;
	const char *arg2;
	const char *arg3;

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /ctcp-ping <target>", "");
	CHECK_SEND_COMMAND(c_serv, m2, 1, 1, 0, "Usage: /ctcp-ping <target>", "");

	/* test send to channel */
	errno = 0;
	mock_reset_io();
	mock_reset_state();

	assert_eq(irc_send_command(s, c_priv, m3), 0);

	p1 = strchr(mock_send[0], '\001');
	p2 = strchr(p1 + 1, '\001');
	assert_true(p1 != NULL);
	assert_true(p2 != NULL);

	*p1++ = 0;
	*p2++ = 0;

	assert_eq(mock_line_n, 0);
	assert_eq(mock_send_n, 1);
	/* truncated by ctcp delimeter */
	assert_strcmp(mock_send[0], "PRIVMSG priv :");

	assert_ptr_not_null((arg1 = irc_strsep(&p1)));
	assert_ptr_not_null((arg2 = irc_strsep(&p1)));
	assert_ptr_not_null((arg3 = irc_strsep(&p1)));

	assert_strcmp(arg1, "PING");
	assert_gt(strtoul(arg2, NULL, 10), 0); /* sec */
	assert_gt(strtoul(arg3, NULL, 10), 0); /* usec */
	assert_eq(errno, 0);

	/* test send to target */
	errno = 0;
	mock_reset_io();
	mock_reset_state();

	assert_eq(irc_send_command(s, c_priv, m4), 0);

	p1 = strchr(mock_send[0], '\001');
	p2 = strchr(p1 + 1, '\001');
	assert_true(p1 != NULL);
	assert_true(p2 != NULL);

	*p1++ = 0;
	*p2++ = 0;

	assert_eq(mock_line_n, 0);
	assert_eq(mock_send_n, 1);
	/* truncated by ctcp delimeter */
	assert_strcmp(mock_send[0], "PRIVMSG targ :");

	assert_ptr_not_null((arg1 = irc_strsep(&p1)));
	assert_ptr_not_null((arg2 = irc_strsep(&p1)));
	assert_ptr_not_null((arg3 = irc_strsep(&p1)));

	assert_strcmp(arg1, "PING");
	assert_gt(strtoul(arg2, NULL, 10), 0); /* sec */
	assert_gt(strtoul(arg3, NULL, 10), 0); /* usec */
	assert_eq(errno, 0);
}

static void
test_send_ctcp_source(void)
{
	char m1[] = "ctcp-source";
	char m2[] = "ctcp-source";
	char m3[] = "ctcp-source";
	char m4[] = "ctcp-source targ";

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /ctcp-source <target>", "");
	CHECK_SEND_COMMAND(c_serv, m2, 1, 1, 0, "Usage: /ctcp-source <target>", "");
	CHECK_SEND_COMMAND(c_priv, m3, 0, 0, 1, "", "PRIVMSG priv :\001SOURCE\001");
	CHECK_SEND_COMMAND(c_priv, m4, 0, 0, 1, "", "PRIVMSG targ :\001SOURCE\001");
}

static void
test_send_ctcp_time(void)
{
	char m1[] = "ctcp-time";
	char m2[] = "ctcp-time";
	char m3[] = "ctcp-time";
	char m4[] = "ctcp-time targ";

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /ctcp-time <target>", "");
	CHECK_SEND_COMMAND(c_serv, m2, 1, 1, 0, "Usage: /ctcp-time <target>", "");
	CHECK_SEND_COMMAND(c_priv, m3, 0, 0, 1, "", "PRIVMSG priv :\001TIME\001");
	CHECK_SEND_COMMAND(c_priv, m4, 0, 0, 1, "", "PRIVMSG targ :\001TIME\001");
}

static void
test_send_ctcp_userinfo(void)
{
	char m1[] = "ctcp-userinfo";
	char m2[] = "ctcp-userinfo";
	char m3[] = "ctcp-userinfo";
	char m4[] = "ctcp-userinfo targ";

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /ctcp-userinfo <target>", "");
	CHECK_SEND_COMMAND(c_serv, m2, 1, 1, 0, "Usage: /ctcp-userinfo <target>", "");
	CHECK_SEND_COMMAND(c_priv, m3, 0, 0, 1, "", "PRIVMSG priv :\001USERINFO\001");
	CHECK_SEND_COMMAND(c_priv, m4, 0, 0, 1, "", "PRIVMSG targ :\001USERINFO\001");
}

static void
test_send_ctcp_version(void)
{
	char m1[] = "ctcp-version";
	char m2[] = "ctcp-version";
	char m3[] = "ctcp-version";
	char m4[] = "ctcp-version targ";

	CHECK_SEND_COMMAND(c_chan, m1, 1, 1, 0, "Usage: /ctcp-version <target>", "");
	CHECK_SEND_COMMAND(c_serv, m2, 1, 1, 0, "Usage: /ctcp-version <target>", "");
	CHECK_SEND_COMMAND(c_priv, m3, 0, 0, 1, "", "PRIVMSG priv :\001VERSION\001");
	CHECK_SEND_COMMAND(c_priv, m4, 0, 0, 1, "", "PRIVMSG targ :\001VERSION\001");
}

static void
test_send_ircv3_cap_ls(void)
{
	char m1[] = "cap-ls";
	char m2[] = "cap-ls xxx";

	CHECK_SEND_COMMAND(c_chan, m1, 0, 0, 1, "", "CAP LS " IRCV3_CAP_VERSION);
	CHECK_SEND_COMMAND(c_chan, m2, 1, 1, 0, "Usage: /cap-ls", "");
}

static void
test_send_ircv3_cap_list(void)
{
	char m1[] = "cap-list";
	char m2[] = "cap-list xxx";

	CHECK_SEND_COMMAND(c_chan, m1, 0, 0, 1, "", "CAP LIST");
	CHECK_SEND_COMMAND(c_chan, m2, 1, 1, 0, "Usage: /cap-list", "");
}

static int
test_init(void)
{
	s = server("h1", "p1", NULL, "u1", "r1", NULL);

	c_serv = s->channel;

	c_chan = channel("chan", CHANNEL_T_CHANNEL);
	c_priv = channel("priv", CHANNEL_T_PRIVMSG);

	if (!s || !c_chan || !c_priv)
		return -1;

	channel_list_add(&s->clist, c_chan);
	channel_list_add(&s->clist, c_priv);

	s->registered = 1;

	return 0;
}

static int
test_term(void)
{
	server_free(s);

	return 0;
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_irc_send_command),
		TESTCASE(test_irc_send_message),
#define X(cmd) TESTCASE(test_send_##cmd),
		SEND_HANDLERS
#undef X
#define X(cmd) TESTCASE(test_send_ctcp_##cmd),
		SEND_CTCP_HANDLERS
#undef X
#define X(cmd) TESTCASE(test_send_ircv3_cap_##cmd),
		SEND_IRCV3_CAP_HANDLERS
#undef X
	};

	return run_tests(test_init, test_term, tests);
}
