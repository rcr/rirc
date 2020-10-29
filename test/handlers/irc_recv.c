#include "test/test.h"

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

#define CHECK_RECV(M, RET, LINE_N, SEND_N) \
	do { \
		mock_reset_io(); \
		mock_reset_state(); \
		IRC_MESSAGE_PARSE(M); \
		assert_eq(irc_recv(s, &m), (RET)); \
		assert_eq(mock_line_n, (LINE_N)); \
		assert_eq(mock_send_n, (SEND_N)); \
	} while (0)

static struct irc_message m;

static struct channel *c1;
static struct channel *c2;
static struct channel *c3;
static struct server *s;

static void
test_353(void)
{
	/* 353 <nick> <type> <channel> 1*(<modes><nick>) */

	struct user *u1;
	struct user *u2;
	struct user *u3;
	struct user *u4;

	channel_reset(c1);
	server_reset(s);

	/* test errors */
	CHECK_RECV("353 me", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "RPL_NAMEREPLY: type is null");
	assert_strcmp(mock_send[0], "");

	CHECK_RECV("353 me =", 1, 1, 0);
	assert_strcmp(mock_line[0], "RPL_NAMEREPLY: channel is null");

	CHECK_RECV("353 me = #c1", 1, 1, 0);
	assert_strcmp(mock_line[0], "RPL_NAMEREPLY: nicks is null");

	CHECK_RECV("353 me = #x :n1", 1, 1, 0);
	assert_strcmp(mock_line[0], "RPL_NAMEREPLY: channel '#x' not found");

	CHECK_RECV("353 me x #c1 :n1", 1, 1, 0);
	assert_strcmp(mock_line[0], "RPL_NAMEREPLY: invalid channel flag: 'x'");

	CHECK_RECV("353 me = #c1 :!n1", 1, 1, 0);
	assert_strcmp(mock_line[0], "RPL_NAMEREPLY: invalid user prefix: '!'");

	CHECK_RECV("353 me = #c1 :+@n1", 1, 1, 0);
	assert_strcmp(mock_line[0], "RPL_NAMEREPLY: invalid nick: '@n1'");

	CHECK_RECV("353 me = #c1 :n1 n2 n1", 1, 1, 0);
	assert_strcmp(mock_line[0], "RPL_NAMEREPLY: duplicate nick: 'n1'");

	/* test single nick */
	channel_reset(c1);
	CHECK_RECV("353 me = #c1 n1", 0, 0, 0);

	if (user_list_get(&(c1->users), s->casemapping, "n1", 0) == NULL)
		test_fail("Failed to retrieve user n1");

	channel_reset(c1);
	CHECK_RECV("353 me = #c1 :@n1", 0, 0, 0);

	if (user_list_get(&(c1->users), s->casemapping, "n1", 0) == NULL)
		test_fail("Failed to retrieve user n1");

	/* test multiple nicks */
	channel_reset(c1);
	CHECK_RECV("353 me = #c1 :@n1 +n2 n3", 0, 0, 0);

	if (!(u1 = user_list_get(&(c1->users), CASEMAPPING_RFC1459, "n1", 0))
	 || !(u2 = user_list_get(&(c1->users), CASEMAPPING_RFC1459, "n2", 0))
	 || !(u3 = user_list_get(&(c1->users), CASEMAPPING_RFC1459, "n3", 0)))
		test_abort("Failed to retrieve users");

	assert_eq(u1->prfxmodes.lower, (flag_bit('o')));
	assert_eq(u2->prfxmodes.lower, (flag_bit('v')));
	assert_eq(u3->prfxmodes.lower, 0);

	/* test multiple nicks, multiprefix enabled */
	s->ircv3_caps.multi_prefix.set = 1;

	channel_reset(c1);
	CHECK_RECV("353 me = #c1 :@n1 +n2 @+n3 +@n4", 0, 0, 0);

	if (!(u1 = user_list_get(&(c1->users), CASEMAPPING_RFC1459, "n1", 0))
	 || !(u2 = user_list_get(&(c1->users), CASEMAPPING_RFC1459, "n2", 0))
	 || !(u3 = user_list_get(&(c1->users), CASEMAPPING_RFC1459, "n3", 0))
	 || !(u4 = user_list_get(&(c1->users), CASEMAPPING_RFC1459, "n4", 0)))
		test_abort("Failed to retrieve users");

	assert_eq(u1->prfxmodes.prefix, '@');
	assert_eq(u2->prfxmodes.prefix, '+');
	assert_eq(u3->prfxmodes.prefix, '@');
	assert_eq(u4->prfxmodes.prefix, '@');
	assert_eq(u1->prfxmodes.lower, (flag_bit('o')));
	assert_eq(u2->prfxmodes.lower, (flag_bit('v')));
	assert_eq(u3->prfxmodes.lower, (flag_bit('o') | flag_bit('v')));
	assert_eq(u4->prfxmodes.lower, (flag_bit('o') | flag_bit('v')));
}

static void
test_recv_ircv3_account(void)
{
	/* :nick!user@host ACCOUNT <account> */

	channel_reset(c1);
	channel_reset(c2);
	channel_reset(c3);
	server_reset(s);

	/* c1 = {nick1, nick2}, c3 = {nick1} */
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c3->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);

	account_threshold = 0;

	CHECK_RECV("ACCOUNT *", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "ACCOUNT: sender's nick is null");

	CHECK_RECV(":nick1!user@host ACCOUNT", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "ACCOUNT: account is null");

	/* check user not on channels */
	CHECK_RECV(":nick3!user@host ACCOUNT account", 0, 0, 0);

	/* logging in */
	CHECK_RECV(":nick1!user@host ACCOUNT account", 0, 2, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick1 has logged in as account");
	assert_strcmp(mock_chan[1], "#c3");
	assert_strcmp(mock_line[1], "nick1 has logged in as account");

	/* logging out */
	CHECK_RECV(":nick1!user@host ACCOUNT *", 0, 2, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick1 has logged out");
	assert_strcmp(mock_chan[1], "#c3");
	assert_strcmp(mock_line[1], "nick1 has logged out");

	/* account threshold */
	account_threshold = 2;

	CHECK_RECV(":nick1!user@host ACCOUNT *", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c3");
}

static void
test_recv_ircv3_away(void)
{
	/* :nick!user@host AWAY [:message] */

	channel_reset(c1);
	channel_reset(c2);
	channel_reset(c3);
	server_reset(s);

	/* c1 = {nick1, nick2}, c3 = {nick1} */
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c3->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);

	away_threshold = 0;

	CHECK_RECV("AWAY *", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "AWAY: sender's nick is null");

	/* check user not on channels */
	CHECK_RECV(":nick3!user@host AWAY :away message", 0, 0, 0);

	/* away set */
	CHECK_RECV(":nick1!user@host AWAY :away message", 0, 2, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick1 is now away: away message");
	assert_strcmp(mock_chan[1], "#c3");
	assert_strcmp(mock_line[1], "nick1 is now away: away message");

	/* away unset */
	CHECK_RECV(":nick1!user@host AWAY", 0, 2, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick1 is no longer away");
	assert_strcmp(mock_chan[1], "#c3");
	assert_strcmp(mock_line[1], "nick1 is no longer away");

	/* away threshold */
	away_threshold = 2;

	CHECK_RECV(":nick1!user@host AWAY", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c3");
}

static void
test_recv_ircv3_chghost(void)
{
	/* :nick!user@host CHGHOST new_user new_host */

	channel_reset(c1);
	channel_reset(c2);
	channel_reset(c3);
	server_reset(s);

	/* c1 = {nick1, nick2}, c3 = {nick1} */
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c3->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);

	chghost_threshold = 0;

	CHECK_RECV("CHGHOST new_user new_host", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "CHGHOST: sender's nick is null");

	CHECK_RECV(":nick1!user@host CHGHOST", 1, 1, 0);
	assert_strcmp(mock_line[0], "CHGHOST: user is null");

	CHECK_RECV(":nick1!user@host CHGHOST new_user", 1, 1, 0);
	assert_strcmp(mock_line[0], "CHGHOST: host is null");

	/* check user not on channels */
	CHECK_RECV(":nick3!user@host CHGHOST new_user new_host", 0, 0, 0);

	CHECK_RECV(":nick1!user@host CHGHOST new_user new_host", 0, 2, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick1 has changed user/host: new_user/new_host");
	assert_strcmp(mock_chan[1], "#c3");
	assert_strcmp(mock_line[1], "nick1 has changed user/host: new_user/new_host");

	/* chghost threshold */
	chghost_threshold = 2;

	CHECK_RECV(":nick1!user@host CHGHOST new_user new_host", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c3");
	assert_strcmp(mock_line[0], "nick1 has changed user/host: new_user/new_host");
}

int
main(void)
{
	c1 = channel("#c1", CHANNEL_T_CHANNEL);
	c2 = channel("#c2", CHANNEL_T_CHANNEL);
	c3 = channel("#c3", CHANNEL_T_CHANNEL);
	s = server("host", "port", NULL, "user", "real");

	if (!s || !c1 || !c2 || !c3)
		test_abort_main("Failed test setup");

	channel_list_add(&s->clist, c1);
	channel_list_add(&s->clist, c2);
	channel_list_add(&s->clist, c3);

	server_nick_set(s, "me");

	struct testcase tests[] = {
		TESTCASE(test_353),
		TESTCASE(test_recv_ircv3_account),
		TESTCASE(test_recv_ircv3_away),
		TESTCASE(test_recv_ircv3_chghost)
	};

	int ret = run_tests(tests);

	server_free(s);

	return ret;
}
