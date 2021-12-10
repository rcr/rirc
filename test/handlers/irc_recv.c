#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/channel.h"
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
static struct channel *p1;
static struct channel *p2;
static struct channel *p3;
static struct server *s;

static void
test_irc_generic(void)
{
	#define CHECK_IRC_GENERIC(M, C, F, RET, LINE_N) \
	do { \
		mock_reset_io(); \
		mock_reset_state(); \
		IRC_MESSAGE_PARSE(M); \
		assert_eq(irc_generic(s, &m, (C), (F)), (RET)); \
		assert_eq(mock_line_n, (LINE_N)); \
		assert_eq(mock_send_n, 0); \
	} while (0)

	CHECK_IRC_GENERIC("COMMAND", NULL, NULL, 1, 0);

	/* test no command, only args */
	CHECK_IRC_GENERIC("COMMAND arg1", NULL, NULL, 0, 1);
	assert_strcmp(mock_line[0], "[arg1]");

	/* test no command, only trailing */
	CHECK_IRC_GENERIC("COMMAND :trailing arg", NULL, NULL, 0, 1);
	assert_strcmp(mock_line[0], "trailing arg");

	/* test no command, args and trailing */
	CHECK_IRC_GENERIC("COMMAND arg1 arg2 :trailing arg", NULL, NULL, 0, 1);
	assert_strcmp(mock_line[0], "[arg1 arg2] ~ trailing arg");

	/* test command, no args or trailing */
	CHECK_IRC_GENERIC("COMMAND", "COMMAND", NULL, 0, 1);
	assert_strcmp(mock_line[0], "[COMMAND]");

	/* test command, only args */
	CHECK_IRC_GENERIC("COMMAND arg1", "COMMAND", NULL, 0, 1);
	assert_strcmp(mock_line[0], "[COMMAND] [arg1]");

	/* test command, only trailing */
	CHECK_IRC_GENERIC("COMMAND :trailing arg", "COMMAND", NULL, 0, 1);
	assert_strcmp(mock_line[0], "[COMMAND] ~ trailing arg");

	/* test command, args and trailing */
	CHECK_IRC_GENERIC("COMMAND arg1 arg2 :trailing arg", "COMMAND", NULL, 0, 1);
	assert_strcmp(mock_line[0], "[COMMAND] [arg1 arg2] ~ trailing arg");

	#undef CHECK_IRC_GENERIC
}

static void
test_irc_generic_error(void)
{
	char m1[] = "COMMAND arg1 arg2 :trailing arg";

	mock_reset_io();
	mock_reset_state();
	assert_eq(irc_message_parse(&m, m1), 0);
	assert_eq(irc_generic_error(s, &m), 0);
	assert_eq(mock_line_n, 1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "[arg1 arg2] ~ trailing arg");
}

static void
test_irc_generic_ignore(void)
{
	char m1[] = "COMMAND arg1 arg2 :trailing arg";

	mock_reset_io();
	mock_reset_state();
	assert_eq(irc_message_parse(&m, m1), 0);
	assert_eq(irc_generic_ignore(s, &m), 0);
	assert_eq(mock_line_n, 0);
	assert_eq(mock_send_n, 0);
}

static void
test_irc_generic_info(void)
{
	char m1[] = "COMMAND arg1 arg2 :trailing arg";

	mock_reset_io();
	mock_reset_state();
	assert_eq(irc_message_parse(&m, m1), 0);
	assert_eq(irc_generic_info(s, &m), 0);
	assert_eq(mock_line_n, 1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "[arg1 arg2] ~ trailing arg");
}

static void
test_irc_generic_unknown(void)
{
	char m1[] = "COMMAND arg1 arg2 :trailing arg";

	mock_reset_io();
	mock_reset_state();
	assert_eq(irc_message_parse(&m, m1), 0);
	assert_eq(irc_generic_unknown(s, &m), 0);
	assert_eq(mock_line_n, 1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "[COMMAND] [arg1 arg2] ~ trailing arg");
}

static void
test_irc_numeric_001(void)
{
	/* 001 :<Welcome message> */

	/* test registered */
	mock_reset_io();
	mock_reset_state();

	assert_eq(s->registered, 0);

	CHECK_RECV("001 me", 0, 1, 3);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "You are known as me");
	assert_strcmp(mock_send[0], "JOIN #c1");
	assert_strcmp(mock_send[1], "JOIN #c2");
	assert_strcmp(mock_send[2], "JOIN #c3");

	assert_eq(s->registered, 1);

	/* test welcome message */
	mock_reset_io();
	mock_reset_state();

	CHECK_RECV("001 me :welcome message", 0, 2, 3);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "welcome message");
	assert_strcmp(mock_line[1], "You are known as me");
	assert_strcmp(mock_send[0], "JOIN #c1");
	assert_strcmp(mock_send[1], "JOIN #c2");
	assert_strcmp(mock_send[2], "JOIN #c3");

	/* test channel keys */
	mock_reset_io();
	mock_reset_state();

	channel_key_add(c1, "foo");
	channel_key_add(c3, "bar");

	CHECK_RECV("001 me :welcome message", 0, 2, 3);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "welcome message");
	assert_strcmp(mock_line[1], "You are known as me");
	assert_strcmp(mock_send[0], "JOIN #c1 foo");
	assert_strcmp(mock_send[1], "JOIN #c2");
	assert_strcmp(mock_send[2], "JOIN #c3 bar");

	channel_key_del(c1);
	channel_key_del(c3);

	/* test user modes */
	mock_reset_io();
	mock_reset_state();

	s->mode = strdup("abc");

	CHECK_RECV("001 me", 0, 1, 4);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "You are known as me");
	assert_strcmp(mock_send[0], "MODE me +abc");
	assert_strcmp(mock_send[1], "JOIN #c1");
	assert_strcmp(mock_send[2], "JOIN #c2");
	assert_strcmp(mock_send[3], "JOIN #c3");

	/* test parted channels aren't auto joined */
	mock_reset_io();
	mock_reset_state();

	c2->parted = 1;

	CHECK_RECV("001 me", 0, 1, 3);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "You are known as me");
	assert_strcmp(mock_send[0], "MODE me +abc");
	assert_strcmp(mock_send[1], "JOIN #c1");
	assert_strcmp(mock_send[2], "JOIN #c3");
}

static void
test_irc_numeric_353(void)
{
	/* 353 <nick> <type> <channel> 1*(<modes><nick>) */

	struct user *u1;
	struct user *u2;
	struct user *u3;
	struct user *u4;

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
	assert_strcmp(mock_line[0], "RPL_NAMEREPLY: invalid channel type: 'x'");

	CHECK_RECV("353 me = #c1 :+@", 1, 1, 0);
	assert_strcmp(mock_line[0], "RPL_NAMEREPLY: invalid nick: '+@'");

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

	assert_eq(u1->prfxmodes.lower, (mode_bit('o')));
	assert_eq(u2->prfxmodes.lower, (mode_bit('v')));
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
	assert_eq(u1->prfxmodes.lower, (mode_bit('o')));
	assert_eq(u2->prfxmodes.lower, (mode_bit('v')));
	assert_eq(u3->prfxmodes.lower, (mode_bit('o') | mode_bit('v')));
	assert_eq(u4->prfxmodes.lower, (mode_bit('o') | mode_bit('v')));
}

static void
test_irc_numeric_401(void)
{
	/* <nick> :No such nick/channel */

	/* test errors */
	CHECK_RECV("401 me", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "ERR_NOSUCHNICK: nick is null");

	/* test channel buffer not found */
	CHECK_RECV("401 me #notfound", 0, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "[#notfound] No such nick/channel");

	/* test privmsg buffer not found */
	CHECK_RECV("401 me notfound", 0, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "[notfound] No such nick/channel");

	/* test channel buffer found */
	CHECK_RECV("401 me #c1", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "[#c1] No such nick/channel");

	/* test privmsg buffer found */
	CHECK_RECV("401 me p1", 0, 1, 0);
	assert_strcmp(mock_chan[0], "p1");
	assert_strcmp(mock_line[0], "[p1] No such nick/channel");

	/* test with message */
	CHECK_RECV("401 me p1 :401 message", 0, 1, 0);
	assert_strcmp(mock_chan[0], "p1");
	assert_strcmp(mock_line[0], "[p1] 401 message");
}

static void
test_irc_numeric_403(void)
{
	/* <chan> :No such channel */

	/* test errors */
	CHECK_RECV("403 me", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "ERR_NOSUCHCHANNEL: chan is null");

	/* test channel buffer not found */
	CHECK_RECV("403 me #notfound", 0, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "[#notfound] No such channel");

	/* test privmsg buffer not found */
	CHECK_RECV("403 me notfound", 0, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "[notfound] No such channel");

	/* test channel buffer found */
	CHECK_RECV("403 me #c1", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "[#c1] No such channel");

	/* test privmsg buffer found */
	CHECK_RECV("403 me p1", 0, 1, 0);
	assert_strcmp(mock_chan[0], "p1");
	assert_strcmp(mock_line[0], "[p1] No such channel");

	/* test with message */
	CHECK_RECV("403 me p1 :403 message", 0, 1, 0);
	assert_strcmp(mock_chan[0], "p1");
	assert_strcmp(mock_line[0], "[p1] 403 message");
}

static void
test_recv(void)
{
	/* test unhandled command type */
	CHECK_RECV("UNHANDLED", 0, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "[UNHANDLED]");
}

static void
test_recv_error(void)
{
	/* ERROR :<message> */

	CHECK_RECV("ERROR", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "ERROR: message is null");

	CHECK_RECV("ERROR error", 0, 1, 0);
	assert_strcmp(mock_line[0], "error");

	s->quitting = 0;
	CHECK_RECV("ERROR :error message", 0, 1, 0);
	assert_strcmp(mock_line[0], "error message");

	s->quitting = 1;
	CHECK_RECV("ERROR :error message", 0, 1, 0);
	assert_strcmp(mock_line[0], "error message");
}

static void
test_recv_invite(void)
{
	/* :nick!user@host INVITE <nick> <channel> */

	CHECK_RECV("INVITE nick channel", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "INVITE: sender's nick is null");

	CHECK_RECV(":nick1!user@host INVITE", 1, 1, 0);
	assert_strcmp(mock_line[0], "INVITE: nick is null");

	CHECK_RECV(":nick1!user@host INVITE nick", 1, 1, 0);
	assert_strcmp(mock_line[0], "INVITE: channel is null");

	CHECK_RECV(":nick1!user@host INVITE nick #notfound", 1, 1, 0);
	assert_strcmp(mock_line[0], "INVITE: channel '#notfound' not found");

	CHECK_RECV(":nick1!user@host INVITE me #c1", 0, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "nick1 invited you to #c1");

	CHECK_RECV(":nick1!user@host INVITE invitee #c1", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick1 invited invitee to #c1");
}

static void
test_recv_join(void)
{
	/* :nick!user@host JOIN <channel>
	 * :nick!user@host JOIN <channel> <account> :<realname> */

	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c2->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);

	threshold_join = 0;

	CHECK_RECV("JOIN #c1", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "JOIN: sender's nick is null");

	CHECK_RECV(":nick!user@host JOIN", 1, 1, 0);
	assert_strcmp(mock_line[0], "JOIN: channel is null");

	CHECK_RECV(":nick!user@host JOIN #notfound", 1, 1, 0);
	assert_strcmp(mock_line[0], "JOIN: channel '#notfound' not found");

	CHECK_RECV(":nick1!user@host JOIN #c1", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "JOIN: user 'nick1' already on channel '#c1'");

	CHECK_RECV(":nick2!user@host JOIN #c1", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick2!user@host has joined");

	/* test IRCv3 extended-join disabled */
	s->ircv3_caps.extended_join.set = 0;

	CHECK_RECV(":nick3!user@host JOIN #c1 account :real name", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick3!user@host has joined");

	/* test IRCv3 extended-join enabled */
	s->ircv3_caps.extended_join.set = 1;

	CHECK_RECV(":nick4!user@host JOIN #c1", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "JOIN: account is null");

	CHECK_RECV(":nick5!user@host JOIN #c1 account", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "JOIN: realname is null");

	CHECK_RECV(":nick6!user@host JOIN #c1 account :real name", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick6!user@host has joined [account - real name]");

	threshold_join = 2;

	CHECK_RECV(":nick2!user@host JOIN #c2", 0, 0, 0);

	/* test user joining new channel */
	CHECK_RECV(":me!user@host JOIN #new", 0, 1, 1);
	assert_strcmp(mock_chan[0], "#new");
	assert_strcmp(mock_line[0], "Joined #new");
	assert_strcmp(mock_send[0], "MODE #new");
	assert_ptr_not_null(channel_list_get(&s->clist, "#new", s->casemapping));
}

static void
test_recv_kick(void)
{
	/* :nick!user@host KICK <channel> <user> [:message] */

	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick3", MODE_EMPTY), USER_ERR_NONE);

	CHECK_RECV("KICK #c1", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "KICK: sender's nick is null");

	CHECK_RECV(":nick!user@host KICK", 1, 1, 0);
	assert_strcmp(mock_line[0], "KICK: channel is null");

	CHECK_RECV(":nick!user@host KICK #c1", 1, 1, 0);
	assert_strcmp(mock_line[0], "KICK: user is null");

	CHECK_RECV(":nick!user@host KICK #notfound nick1", 1, 1, 0);
	assert_strcmp(mock_line[0], "KICK: channel '#notfound' not found");

	/* no message */
	CHECK_RECV(":nick!user@host KICK #c1 nick1", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick has kicked nick1");
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick1", 0));

	/* empty message */
	CHECK_RECV(":nick!user@host KICK #c1 nick2 :", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick has kicked nick2");
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick2", 0));

	CHECK_RECV(":nick!user@host KICK #c1 nick3 :kick message", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick has kicked nick3 (kick message)");
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick3", 0));

	/* no message */
	CHECK_RECV(":nick!user@host KICK #c1 me", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "Kicked by nick");
	assert_eq(c1->parted, 1);

	/* empty message */
	CHECK_RECV(":nick!user@host KICK #c2 me :", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c2");
	assert_strcmp(mock_line[0], "Kicked by nick");
	assert_eq(c2->parted, 1);

	CHECK_RECV(":nick!user@host KICK #c3 me :kick message", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c3");
	assert_strcmp(mock_line[0], "Kicked by nick (kick message)");
	assert_eq(c3->parted, 1);
}

static void
test_recv_mode(void)
{
	/* TODO */
}

static void
test_recv_mode_chanmodes(void)
{
	/* TODO */
}

static void
test_recv_mode_usermodes(void)
{
	/* TODO */
}

static void
test_recv_nick(void)
{
	/* :nick!user@host NICK <nick> */

	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c3->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c3->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);

	CHECK_RECV("NICK new_nick", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "NICK: old nick is null");

	CHECK_RECV(":nick1!user@host NICK", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "NICK: new nick is null");

	/* user not on channels */
	CHECK_RECV(":nick3!user@host NICK new_nick", 0, 0, 0);

	CHECK_RECV(":nick1!user@host NICK new_nick", 0, 2, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick1  >>  new_nick");
	assert_strcmp(mock_chan[1], "#c3");
	assert_strcmp(mock_line[1], "nick1  >>  new_nick");
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick1", 0));
	assert_ptr_null(user_list_get(&(c3->users), s->casemapping, "nick1", 0));

	CHECK_RECV(":nick2!user@host NICK new_nick", 0, 2, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "NICK: user 'new_nick' already on channel '#c1'");
	assert_strcmp(mock_chan[1], "host");
	assert_strcmp(mock_line[1], "NICK: user 'new_nick' already on channel '#c3'");

	CHECK_RECV(":me!user@host NICK new_me", 0, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "Your nick is now 'new_me'");

	/* user can change own nick case */
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "abc{}|^", MODE_EMPTY), USER_ERR_NONE);

	CHECK_RECV(":abc{}|^!user@host NICK AbC{]|~", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "abc{}|^  >>  AbC{]|~");

	server_nick_set(s, "me");
}

static void
test_recv_notice(void)
{
	/* TODO */
}

static void
test_recv_numeric(void)
{
	CHECK_RECV(":hostname 0 * arg :trailing arg", 1, 1, 0);
	assert_strcmp(mock_line[0], "NUMERIC: '0' invalid");

	CHECK_RECV(":hostname 00 * arg :trailing arg", 1, 1, 0);
	assert_strcmp(mock_line[0], "NUMERIC: '00' invalid");

	CHECK_RECV(":hostname 0a * arg :trailing arg", 1, 1, 0);
	assert_strcmp(mock_line[0], "NUMERIC: '0a' invalid");

	CHECK_RECV(":hostname 000 * arg :trailing arg", 1, 1, 0);
	assert_strcmp(mock_line[0], "NUMERIC: '000' invalid");

	CHECK_RECV(":hostname 00a * arg :trailing arg", 1, 1, 0);
	assert_strcmp(mock_line[0], "NUMERIC: '00a' invalid");

	CHECK_RECV(":hostname 0000 * arg :trailing arg", 1, 1, 0);
	assert_strcmp(mock_line[0], "NUMERIC: '0000' invalid");

	CHECK_RECV(":hostname 1000 * arg :trailing arg", 1, 1, 0);
	assert_strcmp(mock_line[0], "NUMERIC: '1000' invalid");

	CHECK_RECV(":hostname 001", 1, 1, 0);
	assert_strcmp(mock_line[0], "NUMERIC: target is null");

	CHECK_RECV(":hostname 001 test", 1, 1, 0);
	assert_strcmp(mock_line[0], "NUMERIC: target 'test' is invalid");

	/* test numeric unhandled */
	CHECK_RECV(":hostname 666 me arg1 arg2 :trailing arg", 0, 1, 0);
	assert_strcmp(mock_line[0], "[666] [arg1 arg2] ~ trailing arg");
	assert_ptr_null(irc_numerics[666]);
}

static void
test_recv_part(void)
{
	/* :nick!user@host PART <channel> [:message] */

	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick3", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick4", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick5", MODE_EMPTY), USER_ERR_NONE);

	threshold_part = 0;

	CHECK_RECV("PART #c1 :part message", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "PART: sender's nick is null");

	CHECK_RECV(":nick1!user@host PART", 1, 1, 0);
	assert_strcmp(mock_line[0], "PART: channel is null");

	CHECK_RECV(":nick1!user@host PART #notfound :quit message", 1, 1, 0);
	assert_strcmp(mock_line[0], "PART: channel '#notfound' not found");

	CHECK_RECV(":nick6!user@host PART #c1 :part message", 1, 1, 0);
	assert_strcmp(mock_line[0], "PART: nick 'nick6' not found in '#c1'");

	CHECK_RECV(":nick1!user@host PART #c1 :part message", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick1!user@host has parted (part message)");
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick1", 0));

	/* no message */
	CHECK_RECV(":nick2!user@host PART #c1", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick2!user@host has parted");
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick2", 0));

	/* empty message */
	CHECK_RECV(":nick3!user@host PART #c1 :", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick3!user@host has parted");
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick3", 0));

	threshold_part = 1;

	CHECK_RECV(":nick4!user@host PART #c1", 0, 0, 0);
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick4", 0));

	/* channel not found, assume closed */
	CHECK_RECV(":me!user@host PART #notfound", 0, 0, 0);

	/* no message */
	CHECK_RECV(":me!user@host PART #c1", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "you have parted");
	assert_eq(c1->parted, 1);

	/* empty message */
	CHECK_RECV(":me!user@host PART #c2 :", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c2");
	assert_strcmp(mock_line[0], "you have parted");
	assert_eq(c2->parted, 1);

	CHECK_RECV(":me!user@host PART #c3 message", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c3");
	assert_strcmp(mock_line[0], "you have parted (message)");
	assert_eq(c3->parted, 1);
}

static void
test_recv_ping(void)
{
	/* PING <server> */

	CHECK_RECV("PING", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "PING: server is null");

	CHECK_RECV("PING server", 0, 0, 1);
	assert_strcmp(mock_send[0], "PONG server");
}

static void
test_recv_pong(void)
{
	/* PONG <server> [<server2>] */

	CHECK_RECV("PONG", 0, 0, 0);
	CHECK_RECV("PONG s1", 0, 0, 0);
	CHECK_RECV("PONG s1 s2", 0, 0, 0);
}

static void
test_recv_privmsg(void)
{
	/* TODO */
}

static void
test_recv_quit(void)
{
	/* :nick!user@host QUIT [:message] */

	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick3", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick4", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick5", MODE_EMPTY), USER_ERR_NONE);

	assert_eq(user_list_add(&(c3->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c3->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);

	threshold_quit = 0;

	CHECK_RECV("QUIT message", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "QUIT: sender's nick is null");

	/* user not on channels */
	CHECK_RECV(":nick6!user@host QUIT :quit message", 0, 0, 0);

	CHECK_RECV(":nick2!user@host QUIT :quit message", 0, 2, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick2!user@host has quit (quit message)");
	assert_strcmp(mock_chan[1], "#c3");
	assert_strcmp(mock_line[1], "nick2!user@host has quit (quit message)");
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick2", 0));
	assert_ptr_null(user_list_get(&(c3->users), s->casemapping, "nick2", 0));

	/* no message */
	CHECK_RECV(":nick3!user@host QUIT", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick3!user@host has quit");
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick3", 0));

	/* empty message */
	CHECK_RECV(":nick4!user@host QUIT :", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick4!user@host has quit");
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick4", 0));

	threshold_quit = 1;

	/* c1 = {nick1, nick5}, c3 = {nick1} */
	CHECK_RECV(":nick1!user@host QUIT", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c3");
	assert_strcmp(mock_line[0], "nick1!user@host has quit");
	assert_ptr_null(user_list_get(&(c1->users), s->casemapping, "nick1", 0));
	assert_ptr_null(user_list_get(&(c3->users), s->casemapping, "nick1", 0));
}

static void
test_recv_topic(void)
{
	/* :nick!user@host TOPIC <channel> [:topic] */

	CHECK_RECV("TOPIC #c1 message", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "TOPIC: sender's nick is null");

	CHECK_RECV(":nick1!user@host TOPIC", 1, 1, 0);
	assert_strcmp(mock_line[0], "TOPIC: channel is null");

	CHECK_RECV(":nick1!user@host TOPIC #c1", 1, 1, 0);
	assert_strcmp(mock_line[0], "TOPIC: topic is null");

	CHECK_RECV(":nick1!user@host TOPIC #notfound message", 1, 1, 0);
	assert_strcmp(mock_line[0], "TOPIC: channel '#notfound' not found");

	CHECK_RECV(":nick1!user@host TOPIC #c1 message", 0, 2, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_chan[1], "#c1");
	assert_strcmp(mock_line[0], "nick1 has set the topic:");
	assert_strcmp(mock_line[1], "\"message\"");

	CHECK_RECV(":nick1!user@host TOPIC #c1 :topic message", 0, 2, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_chan[1], "#c1");
	assert_strcmp(mock_line[0], "nick1 has set the topic:");
	assert_strcmp(mock_line[1], "\"topic message\"");

	CHECK_RECV(":nick1!user@host TOPIC #c1 :", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick1 has unset the topic");
}

static void
test_recv_ircv3_cap(void)
{
	/* Full IRCv3 CAP coverage in test/handlers/ircv3.c */

	s->registered = 1;

	CHECK_RECV("CAP * LS :cap-1 cap-2 cap-3", 0, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "CAP LS: cap-1 cap-2 cap-3");
}

static void
test_recv_ircv3_account(void)
{
	/* :nick!user@host ACCOUNT <account> */

	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c3->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);

	threshold_account = 0;

	CHECK_RECV("ACCOUNT *", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "ACCOUNT: sender's nick is null");

	CHECK_RECV(":nick1!user@host ACCOUNT", 1, 1, 0);
	assert_strcmp(mock_line[0], "ACCOUNT: account is null");

	/* user not on channels */
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

	threshold_account = 2;

	CHECK_RECV(":nick1!user@host ACCOUNT *", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c3");
}

static void
test_recv_ircv3_away(void)
{
	/* :nick!user@host AWAY [:message] */

	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c3->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);

	threshold_away = 0;

	CHECK_RECV("AWAY *", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "AWAY: sender's nick is null");

	/* user not on channels */
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

	threshold_away = 2;

	CHECK_RECV(":nick1!user@host AWAY", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c3");
}

static void
test_recv_ircv3_chghost(void)
{
	/* :nick!user@host CHGHOST new_user new_host */

	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c1->users), CASEMAPPING_RFC1459, "nick2", MODE_EMPTY), USER_ERR_NONE);
	assert_eq(user_list_add(&(c3->users), CASEMAPPING_RFC1459, "nick1", MODE_EMPTY), USER_ERR_NONE);

	threshold_chghost = 0;

	CHECK_RECV("CHGHOST new_user new_host", 1, 1, 0);
	assert_strcmp(mock_chan[0], "host");
	assert_strcmp(mock_line[0], "CHGHOST: sender's nick is null");

	CHECK_RECV(":nick1!user@host CHGHOST", 1, 1, 0);
	assert_strcmp(mock_line[0], "CHGHOST: user is null");

	CHECK_RECV(":nick1!user@host CHGHOST new_user", 1, 1, 0);
	assert_strcmp(mock_line[0], "CHGHOST: host is null");

	/* user not on channels */
	CHECK_RECV(":nick3!user@host CHGHOST new_user new_host", 0, 0, 0);

	CHECK_RECV(":nick1!user@host CHGHOST new_user new_host", 0, 2, 0);
	assert_strcmp(mock_chan[0], "#c1");
	assert_strcmp(mock_line[0], "nick1 has changed user/host: new_user/new_host");
	assert_strcmp(mock_chan[1], "#c3");
	assert_strcmp(mock_line[1], "nick1 has changed user/host: new_user/new_host");

	threshold_chghost = 2;

	CHECK_RECV(":nick1!user@host CHGHOST new_user new_host", 0, 1, 0);
	assert_strcmp(mock_chan[0], "#c3");
	assert_strcmp(mock_line[0], "nick1 has changed user/host: new_user/new_host");
}

static int
test_init(void)
{
	s = server("host", "port", NULL, "user", "real", NULL);

	c1 = channel("#c1", CHANNEL_T_CHANNEL);
	c2 = channel("#c2", CHANNEL_T_CHANNEL);
	c3 = channel("#c3", CHANNEL_T_CHANNEL);

	p1 = channel("p1", CHANNEL_T_PRIVMSG);
	p2 = channel("p2", CHANNEL_T_PRIVMSG);
	p3 = channel("p3", CHANNEL_T_PRIVMSG);

	if (!s || !c1 || !c2 || !c3 || !p1 || !p2 || !p3)
		return -1;

	channel_list_add(&s->clist, c1);
	channel_list_add(&s->clist, c2);
	channel_list_add(&s->clist, c3);
	channel_list_add(&s->clist, p1);
	channel_list_add(&s->clist, p2);
	channel_list_add(&s->clist, p3);

	server_nick_set(s, "me");

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
		TESTCASE(test_irc_generic),
		TESTCASE(test_irc_generic_error),
		TESTCASE(test_irc_generic_ignore),
		TESTCASE(test_irc_generic_info),
		TESTCASE(test_irc_generic_unknown),
		TESTCASE(test_irc_numeric_001),
		TESTCASE(test_irc_numeric_353),
		TESTCASE(test_irc_numeric_401),
		TESTCASE(test_irc_numeric_403),
		TESTCASE(test_recv),
		TESTCASE(test_recv_error),
		TESTCASE(test_recv_invite),
		TESTCASE(test_recv_join),
		TESTCASE(test_recv_kick),
		TESTCASE(test_recv_mode),
		TESTCASE(test_recv_mode_chanmodes),
		TESTCASE(test_recv_mode_usermodes),
		TESTCASE(test_recv_nick),
		TESTCASE(test_recv_notice),
		TESTCASE(test_recv_numeric),
		TESTCASE(test_recv_part),
		TESTCASE(test_recv_ping),
		TESTCASE(test_recv_pong),
		TESTCASE(test_recv_privmsg),
		TESTCASE(test_recv_quit),
		TESTCASE(test_recv_topic),
		TESTCASE(test_recv_ircv3_cap),
		TESTCASE(test_recv_ircv3_account),
		TESTCASE(test_recv_ircv3_away),
		TESTCASE(test_recv_ircv3_chghost)
	};

	return run_tests(test_init, test_term, tests);
}
