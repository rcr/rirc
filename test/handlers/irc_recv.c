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

static struct irc_message m;

static void
test_353(void)
{
	/* 353 <nick> <type> <channel> 1*(<modes><nick>) */

	struct channel *c = channel("#chan", CHANNEL_T_CHANNEL);
	struct server *s = server("host", "post", NULL, "user", "real");
	struct user *u1;
	struct user *u2;
	struct user *u3;
	struct user *u4;

	channel_list_add(&s->clist, c);
	server_nick_set(s, "me");

	/* test errors */
	channel_reset(c);
	CHECK_REQUEST("353 me", 1, 1, 0,
		"RPL_NAMEREPLY: type is null", "");

	channel_reset(c);
	CHECK_REQUEST("353 me =", 1, 1, 0,
		"RPL_NAMEREPLY: channel is null", "");

	channel_reset(c);
	CHECK_REQUEST("353 me = #chan", 1, 1, 0,
		"RPL_NAMEREPLY: nicks is null", "");

	channel_reset(c);
	CHECK_REQUEST("353 me = #x :n1", 1, 1, 0,
		"RPL_NAMEREPLY: channel '#x' not found", "");

	channel_reset(c);
	CHECK_REQUEST("353 me x #chan :n1", 1, 1, 0,
		"RPL_NAMEREPLY: invalid channel flag: 'x'", "");

	channel_reset(c);
	CHECK_REQUEST("353 me = #chan :!n1", 1, 1, 0,
		"RPL_NAMEREPLY: invalid user prefix: '!'", "");

	channel_reset(c);
	CHECK_REQUEST("353 me = #chan :+@n1", 1, 1, 0,
		"RPL_NAMEREPLY: invalid nick: '@n1'", "");

	channel_reset(c);
	CHECK_REQUEST("353 me = #chan :n1 n2 n1", 1, 1, 0,
		"RPL_NAMEREPLY: duplicate nick: 'n1'", "");

	/* test single nick */
	channel_reset(c);
	CHECK_REQUEST("353 me = #chan n1", 0, 0, 0, "", "");

	if (user_list_get(&(c->users), s->casemapping, "n1", 0) == NULL)
		test_fail("Failed to retrieve user n1");

	channel_reset(c);
	CHECK_REQUEST("353 me = #chan :@n1", 0, 0, 0, "", "");

	if (user_list_get(&(c->users), s->casemapping, "n1", 0) == NULL)
		test_fail("Failed to retrieve user n1");

	/* test multiple nicks */
	channel_reset(c);
	CHECK_REQUEST("353 me = #chan :@n1 +n2 n3", 0, 0, 0, "", "");

	if (!(u1 = user_list_get(&(c->users), CASEMAPPING_RFC1459, "n1", 0))
	 || !(u2 = user_list_get(&(c->users), CASEMAPPING_RFC1459, "n2", 0))
	 || !(u3 = user_list_get(&(c->users), CASEMAPPING_RFC1459, "n3", 0)))
		test_abort("Failed to retrieve users");

	assert_eq(u1->prfxmodes.lower, (flag_bit('o')));
	assert_eq(u2->prfxmodes.lower, (flag_bit('v')));
	assert_eq(u3->prfxmodes.lower, 0);

	/* test multiple nicks, multiprefix enabled */
	s->ircv3_caps.multi_prefix.set = 1;
	channel_reset(c);
	CHECK_REQUEST("353 me = #chan :@n1 +n2 @+n3 +@n4", 0, 0, 0, "", "");

	if (!(u1 = user_list_get(&(c->users), CASEMAPPING_RFC1459, "n1", 0))
	 || !(u2 = user_list_get(&(c->users), CASEMAPPING_RFC1459, "n2", 0))
	 || !(u3 = user_list_get(&(c->users), CASEMAPPING_RFC1459, "n3", 0))
	 || !(u4 = user_list_get(&(c->users), CASEMAPPING_RFC1459, "n4", 0)))
		test_abort("Failed to retrieve users");

	assert_eq(u1->prfxmodes.prefix, '@');
	assert_eq(u2->prfxmodes.prefix, '+');
	assert_eq(u3->prfxmodes.prefix, '@');
	assert_eq(u4->prfxmodes.prefix, '@');
	assert_eq(u1->prfxmodes.lower, (flag_bit('o')));
	assert_eq(u2->prfxmodes.lower, (flag_bit('v')));
	assert_eq(u3->prfxmodes.lower, (flag_bit('o') | flag_bit('v')));
	assert_eq(u4->prfxmodes.lower, (flag_bit('o') | flag_bit('v')));

	server_free(s);
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_353)
	};

	return run_tests(tests);
}
