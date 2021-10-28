#include "test/test.h"

/* Extends the definition in server.h */
#define IRCV3_CAPS_TEST \
	X("cap-1", cap_1, IRCV3_CAP_AUTO) \
	X("cap-2", cap_2, 0) \
	X("cap-3", cap_3, IRCV3_CAP_AUTO) \
	X("cap-4", cap_4, IRCV3_CAP_AUTO) \
	X("cap-5", cap_5, 0) \
	X("cap-6", cap_6, IRCV3_CAP_AUTO) \
	X("cap-7", cap_7, (IRCV3_CAP_AUTO | IRCV3_CAP_NO_DEL))

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

static struct irc_message m;
static struct server *s;

static void
mock_reset(void)
{
	mock_reset_io();
	mock_reset_state();
	server_reset(s);
}

static void
test_ircv3_recv_CAP(void)
{
	mock_reset();
	IRC_MESSAGE_PARSE("CAP");
	assert_eq(irc_recv(s, &m), -1);
	assert_strcmp(mock_line[0], "CAP: target is null");

	mock_reset();
	IRC_MESSAGE_PARSE("CAP *");
	assert_eq(irc_recv(s, &m), -1);
	assert_strcmp(mock_line[0], "CAP: command is null");

	mock_reset();
	IRC_MESSAGE_PARSE("CAP * ack");
	assert_eq(irc_recv(s, &m), -1);
	assert_strcmp(mock_line[0], "CAP: unrecognized subcommand 'ack'");

	mock_reset();
	IRC_MESSAGE_PARSE("CAP * xxx");
	assert_eq(irc_recv(s, &m), -1);
	assert_strcmp(mock_line[0], "CAP: unrecognized subcommand 'xxx'");
}

static void
test_ircv3_recv_CAP_LS(void)
{
	/* test empty LS, no parameter */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LS");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "CAP LS: parameter is null");

	/* test empty LS, no parameter, multiline */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LS *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "CAP LS: parameter is null");

	/* test multiple parameters, no '*' */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LS cap-1 cap-2");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_strcmp(mock_line[0], "CAP LS: invalid parameters");

	/* test empty LS, with leading ':' */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LS :");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_strcmp(mock_send[0], "CAP END");

	/* test no leading ':' */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LS cap-1");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_strcmp(mock_send[0], "CAP REQ :cap-1");

	/* test with leading ':' */
	mock_reset();
	ircv3_caps_reset(&(s->ircv3_caps));
	IRC_MESSAGE_PARSE("CAP * LS :cap-1");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_strcmp(mock_send[0], "CAP REQ :cap-1");
	assert_eq(s->ircv3_caps.cap_1.supported, 1);
	assert_eq(s->ircv3_caps.cap_2.supported, 0);

	/* test multiple caps, cap_2 is non-auto */
	mock_reset();
	ircv3_caps_reset(&(s->ircv3_caps));
	IRC_MESSAGE_PARSE("CAP * LS :cap-1 cap-2 cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_strcmp(mock_send[0], "CAP REQ :cap-1 cap-3");
	assert_eq(s->ircv3_caps.cap_1.supported, 1);
	assert_eq(s->ircv3_caps.cap_2.supported, 1);
	assert_eq(s->ircv3_caps.cap_3.supported, 1);

	/* test multiple caps, with unsupported */
	mock_reset();
	ircv3_caps_reset(&(s->ircv3_caps));
	IRC_MESSAGE_PARSE("CAP * LS :cap-1 foo cap-2 bar cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_strcmp(mock_send[0], "CAP REQ :cap-1 cap-3");
	assert_eq(s->ircv3_caps.cap_1.supported, 1);
	assert_eq(s->ircv3_caps.cap_2.supported, 1);
	assert_eq(s->ircv3_caps.cap_3.supported, 1);

	/* test cap key=val */
	mock_reset();
	ircv3_caps_reset(&(s->ircv3_caps));
	IRC_MESSAGE_PARSE("CAP * LS :cap-1=foo cap-3 cap-4=bar");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_strcmp(mock_send[0], "CAP REQ :cap-1 cap-3 cap-4");
	assert_eq(s->ircv3_caps.cap_1.supported, 1);
	assert_eq(s->ircv3_caps.cap_3.supported, 1);
	assert_eq(s->ircv3_caps.cap_4.supported, 1);
	assert_strcmp(s->ircv3_caps.cap_1.val, "foo");
	assert_strcmp(s->ircv3_caps.cap_3.val, NULL);
	assert_strcmp(s->ircv3_caps.cap_4.val, "bar");

	/* test multiline */
	mock_reset();
	ircv3_caps_reset(&(s->ircv3_caps));
	IRC_MESSAGE_PARSE("CAP * LS * cap-1");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	IRC_MESSAGE_PARSE("CAP * LS * :cap-2 cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	IRC_MESSAGE_PARSE("CAP * LS cap-4");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_strcmp(mock_send[0], "CAP REQ :cap-1 cap-3 cap-4");
	assert_eq(s->ircv3_caps.cap_1.supported, 1);
	assert_eq(s->ircv3_caps.cap_2.supported, 1);
	assert_eq(s->ircv3_caps.cap_3.supported, 1);
	assert_eq(s->ircv3_caps.cap_4.supported, 1);

	/* test registered */
	mock_reset();
	s->registered = 1;
	IRC_MESSAGE_PARSE("CAP * LS :cap-1 cap-2 cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP LS: cap-1 cap-2 cap-3");
}

static void
test_ircv3_recv_CAP_LIST(void)
{
	/* test empty LIST, no parameter */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LIST");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP LIST: parameter is null");

	/* test empty LIST, no parameter, multiline */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LIST *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP LIST: parameter is null");

	/* test multiple parameters, no '*' */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LIST cap-1 cap-2");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP LIST: invalid parameters");

	/* test empty LIST, with leading ':' */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LIST :");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP LIST: (no capabilities)");

	/* test no leading ':' */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LIST cap-1");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP LIST: cap-1");

	/* test with leading ':' */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LIST :cap-1");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);

	/* test multiple caps */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LIST :cap-1 cap-2 cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP LIST: cap-1 cap-2 cap-3");

	/* test multiline */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * LIST * cap-1");
	assert_eq(irc_recv(s, &m), 0);
	IRC_MESSAGE_PARSE("CAP * LIST * :cap-2 cap-3");
	assert_eq(irc_recv(s, &m), 0);
	IRC_MESSAGE_PARSE("CAP * LIST cap-4");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_line[0], "CAP LIST: cap-1");
	assert_strcmp(mock_line[1], "CAP LIST: cap-2 cap-3");
	assert_strcmp(mock_line[2], "CAP LIST: cap-4");
}

static void
test_ircv3_recv_CAP_ACK(void)
{
	/* test empty ACK */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * ACK");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP ACK: parameter is null");

	/* test empty ACK, with leading ':' */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * ACK :");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP ACK: parameter is empty");

	/* test ack key=val */
	mock_reset();
	s->ircv3_caps.cap_1.req = 1;
	s->ircv3_caps.cap_2.req = 1;
	s->ircv3_caps.cap_3.req = 1;
	s->ircv3_caps.cap_1.val = strdup("foo");
	s->ircv3_caps.cap_3.val = strdup("bar");
	IRC_MESSAGE_PARSE("CAP * ACK :cap-1 cap-2 cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_line[0], "capability change accepted: cap-1=foo");
	assert_strcmp(mock_line[1], "capability change accepted: cap-2");
	assert_strcmp(mock_line[2], "capability change accepted: cap-3=bar");
	assert_strcmp(mock_send[0], "CAP END");

	/* test unregisterd, error */
	mock_reset();
	s->ircv3_caps.cap_1.set = 0;
	s->ircv3_caps.cap_2.set = 0;
	s->ircv3_caps.cap_1.req = 1;
	s->ircv3_caps.cap_2.req = 1;
	IRC_MESSAGE_PARSE("CAP * ACK :cap-1 cap-aaa cap-2 cap-bbb");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 5);
	assert_strcmp(mock_line[0], "capability change accepted: cap-1");
	assert_strcmp(mock_line[1], "CAP ACK: 'cap-aaa' not supported");
	assert_strcmp(mock_line[2], "capability change accepted: cap-2");
	assert_strcmp(mock_line[3], "CAP ACK: 'cap-bbb' not supported");
	assert_strcmp(mock_line[4], "CAP ACK: parameter errors");

	/* test unregistered, no error */
	mock_reset();
	s->ircv3_caps.cap_1.set = 0;
	s->ircv3_caps.cap_2.set = 1;
	s->ircv3_caps.cap_3.set = 1;
	s->ircv3_caps.cap_1.req = 1;
	s->ircv3_caps.cap_2.req = 1;
	s->ircv3_caps.cap_3.req = 1;
	IRC_MESSAGE_PARSE("CAP * ACK :cap-1 -cap-2");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[0], "capability change accepted: cap-1");
	assert_strcmp(mock_line[1], "capability change accepted: -cap-2");
	assert_eq(s->ircv3_caps.cap_1.set, 1);
	assert_eq(s->ircv3_caps.cap_2.set, 0);
	IRC_MESSAGE_PARSE("CAP * ACK :-cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_line[2], "capability change accepted: -cap-3");
	assert_strcmp(mock_send[0], "CAP END");

	/* test registered, error */
	mock_reset();
	s->registered = 1;
	s->ircv3_caps.cap_1.set = 0;
	s->ircv3_caps.cap_2.set = 1;
	s->ircv3_caps.cap_3.set = 0;
	s->ircv3_caps.cap_4.set = 1;
	s->ircv3_caps.cap_1.req = 1;
	s->ircv3_caps.cap_2.req = 1;
	s->ircv3_caps.cap_3.req = 1;
	s->ircv3_caps.cap_4.req = 0;
	IRC_MESSAGE_PARSE("CAP * ACK :cap-1 cap-2 -cap-3 -cap-4");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 5);
	assert_strcmp(mock_line[0], "capability change accepted: cap-1");
	assert_strcmp(mock_line[1], "CAP ACK: 'cap-2' was set");
	assert_strcmp(mock_line[2], "CAP ACK: 'cap-3' was not set");
	assert_strcmp(mock_line[3], "CAP ACK: '-cap-4' was not requested");
	assert_strcmp(mock_line[4], "CAP ACK: parameter errors");

	/* test registered, no error */
	mock_reset();
	s->registered = 1;
	s->ircv3_caps.cap_1.set = 0;
	s->ircv3_caps.cap_2.set = 1;
	s->ircv3_caps.cap_1.req = 1;
	s->ircv3_caps.cap_2.req = 1;
	IRC_MESSAGE_PARSE("CAP * ACK :cap-1 -cap-2");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[0], "capability change accepted: cap-1");
	assert_strcmp(mock_line[1], "capability change accepted: -cap-2");
	assert_eq(s->ircv3_caps.cap_1.set, 1);
	assert_eq(s->ircv3_caps.cap_2.set, 0);
}

static void
test_ircv3_recv_CAP_NAK(void)
{
	/* test empty NAK */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * NAK");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP NAK: parameter is null");

	/* test empty NAK, with leading ':' */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * NAK :");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP NAK: parameter is empty");

	/* test unsupported caps */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * NAK :cap-aaa cap-bbb cap-ccc");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_line[0], "capability change rejected: cap-aaa");
	assert_strcmp(mock_line[1], "capability change rejected: cap-bbb");
	assert_strcmp(mock_line[2], "capability change rejected: cap-ccc");
	assert_strcmp(mock_send[0], "CAP END");

	/* test supported caps */
	mock_reset();
	s->ircv3_caps.cap_1.req = 0;
	s->ircv3_caps.cap_2.req = 1;
	s->ircv3_caps.cap_3.req = 1;
	s->ircv3_caps.cap_1.set = 0;
	s->ircv3_caps.cap_2.set = 1;
	s->ircv3_caps.cap_3.set = 1;
	IRC_MESSAGE_PARSE("CAP * NAK :cap-1 cap-2");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[0], "capability change rejected: cap-1");
	assert_strcmp(mock_line[1], "capability change rejected: cap-2");
	assert_eq(s->ircv3_caps.cap_1.req, 0);
	assert_eq(s->ircv3_caps.cap_2.req, 0);
	assert_eq(s->ircv3_caps.cap_1.set, 0);
	assert_eq(s->ircv3_caps.cap_2.set, 1);
	IRC_MESSAGE_PARSE("CAP * NAK :cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_line[2], "capability change rejected: cap-3");
	assert_strcmp(mock_send[0], "CAP END");

	/* test registered - don't send END */
	mock_reset();
	s->registered = 1;
	s->ircv3_caps.cap_1.req = 0;
	s->ircv3_caps.cap_2.req = 1;
	s->ircv3_caps.cap_1.set = 0;
	s->ircv3_caps.cap_2.set = 1;
	IRC_MESSAGE_PARSE("CAP * NAK :cap-1 cap-2");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[0], "capability change rejected: cap-1");
	assert_strcmp(mock_line[1], "capability change rejected: cap-2");
	assert_eq(s->ircv3_caps.cap_1.req, 0);
	assert_eq(s->ircv3_caps.cap_2.req, 0);
	assert_eq(s->ircv3_caps.cap_1.set, 0);
	assert_eq(s->ircv3_caps.cap_2.set, 1);
}

static void
test_ircv3_recv_CAP_DEL(void)
{
	/* test empty DEL */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * DEL");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP DEL: parameter is null");

	/* test empty DEL, with leading ':' */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * DEL :");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP DEL: parameter is empty");

	/* test cap-7 doesn't support DEL */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * DEL :cap-7");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP DEL: 'cap-7' doesn't support DEL");

	/* test unsupported caps */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * DEL :cap-aaa cap-bbb cap-ccc");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 0);

	/* test supported caps */
	mock_reset();
	s->ircv3_caps.cap_1.req = 0;
	s->ircv3_caps.cap_2.req = 1;
	s->ircv3_caps.cap_1.set = 1;
	s->ircv3_caps.cap_2.set = 0;
	s->ircv3_caps.cap_1.supported = 1;
	s->ircv3_caps.cap_2.supported = 1;
	IRC_MESSAGE_PARSE("CAP * DEL :cap-1 cap-2");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[0], "capability lost: cap-1");
	assert_strcmp(mock_line[1], "capability lost: cap-2");
	assert_eq(s->ircv3_caps.cap_1.req, 0);
	assert_eq(s->ircv3_caps.cap_2.req, 0);
	assert_eq(s->ircv3_caps.cap_1.set, 0);
	assert_eq(s->ircv3_caps.cap_2.set, 0);
	assert_eq(s->ircv3_caps.cap_1.supported, 0);
	assert_eq(s->ircv3_caps.cap_2.supported, 0);
}

static void
test_ircv3_recv_CAP_NEW(void)
{
	/* test empty NEW */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * NEW");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP NEW: parameter is null");

	/* test empty DEL, with leading ':' */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * NEW :");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "CAP NEW: parameter is empty");

	/* test unsupported caps */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * NEW :cap-aaa cap-bbb cap-ccc");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 0);

	/* test supported caps */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * NEW :cap-1 cap-2 cap-3 cap-4 cap-5 cap-6 cap-7");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 7);
	assert_strcmp(mock_line[0], "new capability: cap-1 (auto-req)");
	assert_strcmp(mock_line[1], "new capability: cap-2"); /* no REQ (IRCV3_CAP_AUTO disasabled) */
	assert_strcmp(mock_line[2], "new capability: cap-3 (auto-req)");
	assert_strcmp(mock_line[3], "new capability: cap-4 (auto-req)");
	assert_strcmp(mock_line[4], "new capability: cap-5"); /* no REQ (IRCV3_CAP_AUTO disasabled) */
	assert_strcmp(mock_line[5], "new capability: cap-6 (auto-req)");
	assert_strcmp(mock_line[6], "new capability: cap-7 (auto-req)");
	assert_strcmp(mock_send[0], "CAP REQ :cap-1 cap-3 cap-4 cap-6 cap-7");
	assert_eq(s->ircv3_caps.cap_1.supported, 1);
	assert_eq(s->ircv3_caps.cap_2.supported, 1);
	assert_eq(s->ircv3_caps.cap_3.supported, 1);
	assert_eq(s->ircv3_caps.cap_4.supported, 1);
	assert_eq(s->ircv3_caps.cap_5.supported, 1);
	assert_eq(s->ircv3_caps.cap_6.supported, 1);
	assert_eq(s->ircv3_caps.cap_7.supported, 1);

	/* test supported caps with cap.req, cap.set */
	mock_reset();
	s->ircv3_caps.cap_1.req = 1; /* test cap.req */
	s->ircv3_caps.cap_4.set = 1; /* test cap.set */
	IRC_MESSAGE_PARSE("CAP * NEW :cap-1 cap-2 cap-3 cap-4 cap-5 cap-6 cap-7");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 7);
	assert_strcmp(mock_line[0], "new capability: cap-1"); /* no REQ (.req = 1) */
	assert_strcmp(mock_line[1], "new capability: cap-2"); /* no REQ (IRCV3_CAP_AUTO disasabled) */
	assert_strcmp(mock_line[2], "new capability: cap-3 (auto-req)");
	assert_strcmp(mock_line[3], "new capability: cap-4"); /* no REQ (.set = 1) */
	assert_strcmp(mock_line[4], "new capability: cap-5"); /* no REQ (IRCV3_CAP_AUTO disasabled) */
	assert_strcmp(mock_line[5], "new capability: cap-6 (auto-req)");
	assert_strcmp(mock_line[6], "new capability: cap-7 (auto-req)");
	assert_strcmp(mock_send[0], "CAP REQ :cap-3 cap-6 cap-7");
	assert_eq(s->ircv3_caps.cap_1.supported, 1);
	assert_eq(s->ircv3_caps.cap_2.supported, 1);
	assert_eq(s->ircv3_caps.cap_3.supported, 1);
	assert_eq(s->ircv3_caps.cap_4.supported, 1);
	assert_eq(s->ircv3_caps.cap_5.supported, 1);
	assert_eq(s->ircv3_caps.cap_6.supported, 1);
	assert_eq(s->ircv3_caps.cap_7.supported, 1);

	/* test supported caps, only non-auto */
	mock_reset();
	IRC_MESSAGE_PARSE("CAP * NEW :cap-2 cap-5");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[0], "new capability: cap-2"); /* no REQ (IRCV3_CAP_AUTO disasabled) */
	assert_strcmp(mock_line[1], "new capability: cap-5"); /* no REQ (IRCV3_CAP_AUTO disasabled) */
	assert_eq(s->ircv3_caps.cap_1.supported, 0);
	assert_eq(s->ircv3_caps.cap_2.supported, 1);
	assert_eq(s->ircv3_caps.cap_3.supported, 0);
	assert_eq(s->ircv3_caps.cap_4.supported, 0);
	assert_eq(s->ircv3_caps.cap_5.supported, 1);
	assert_eq(s->ircv3_caps.cap_6.supported, 0);
	assert_eq(s->ircv3_caps.cap_7.supported, 0);
}


static void
test_ircv3_recv_AUTHENTICATE(void)
{
	IRC_MESSAGE_PARSE("AUTHENTICATE *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "AUTHENTICATE: no SASL mechanism");

	s->ircv3_sasl.mech = -1;

	IRC_MESSAGE_PARSE("AUTHENTICATE *");
	assert_fatal(irc_recv(s, &m));
}

static void
test_ircv3_recv_AUTHENTICATE_EXTERNAL(void)
{
	server_set_sasl(s, "external", NULL, NULL);

	IRC_MESSAGE_PARSE("AUTHENTICATE *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "Invalid SASL state for mechanism EXTERNAL: 0");

	assert_eq(ircv3_sasl_init(s), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_send[0], "AUTHENTICATE EXTERNAL");

	IRC_MESSAGE_PARSE("AUTHENTICATE");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[1], "Invalid SASL response for mechanism EXTERNAL: response is null");

	IRC_MESSAGE_PARSE("AUTHENTICATE *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_line[2], "Invalid SASL response for mechanism EXTERNAL: '*'");

	IRC_MESSAGE_PARSE("AUTHENTICATE +");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 2);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_send[1], "AUTHENTICATE +");
}

static void
test_ircv3_recv_AUTHENTICATE_PLAIN(void)
{
	server_set_sasl(s, "plain", NULL, "pass");

	IRC_MESSAGE_PARSE("AUTHENTICATE *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "SASL mechanism PLAIN requires a username");

	server_set_sasl(s, "plain", "user", NULL);

	IRC_MESSAGE_PARSE("AUTHENTICATE *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[1], "SASL mechanism PLAIN requires a password");

	server_set_sasl(s, "plain", "user", "pass");

	IRC_MESSAGE_PARSE("AUTHENTICATE *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_line[2], "Invalid SASL state for mechanism PLAIN: 0");

	assert_eq(ircv3_sasl_init(s), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_send[0], "AUTHENTICATE PLAIN");

	IRC_MESSAGE_PARSE("AUTHENTICATE");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 4);
	assert_strcmp(mock_line[3], "Invalid SASL response for mechanism PLAIN: response is null");

	IRC_MESSAGE_PARSE("AUTHENTICATE *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 5);
	assert_strcmp(mock_line[4], "Invalid SASL response for mechanism PLAIN: '*'");

	IRC_MESSAGE_PARSE("AUTHENTICATE +");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 2);
	assert_eq(mock_line_n, 5);
	assert_strcmp(mock_send[1], "AUTHENTICATE dXNlcgB1c2VyAHBhc3M=");

	char user1[150] = {0};
	char user2[147] = {0};

	memset(user1, 'x', sizeof(user1) - 1);
	memset(user2, 'x', sizeof(user2) - 1);

	server_set_sasl(s, "plain", user1, "pass");

	IRC_MESSAGE_PARSE("AUTHENTICATE +");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 2);
	assert_eq(mock_line_n, 6);
	assert_strcmp(mock_line[5], "SASL decoded auth message too long");

	server_set_sasl(s, "plain", user2, "pass");

	IRC_MESSAGE_PARSE("AUTHENTICATE +");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 2);
	assert_eq(mock_line_n, 7);
	assert_strcmp(mock_line[6], "SASL encoded auth message too long");
}

static void
test_ircv3_numeric_900(void)
{
	IRC_MESSAGE_PARSE("900 *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "RPL_LOGGEDIN: missing nick");

	IRC_MESSAGE_PARSE("900 * nick!ident@host");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[1], "RPL_LOGGEDIN: missing account");

	IRC_MESSAGE_PARSE("900 * nick!ident@host account");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_line[2], "SASL success: you are logged in as account");

	IRC_MESSAGE_PARSE("900 * nick!ident@host account :test 900 message");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 4);
	assert_strcmp(mock_line[3], "SASL success: test 900 message");
}

static void
test_ircv3_numeric_901(void)
{
	IRC_MESSAGE_PARSE("901 *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "RPL_LOGGEDOUT: missing nick");

	IRC_MESSAGE_PARSE("901 * nick!ident@host");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[1], "You are now logged out");

	IRC_MESSAGE_PARSE("901 * nick!ident@host :test 901 message");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_line[2], "test 901 message");
}

static void
test_ircv3_numeric_902(void)
{
	IRC_MESSAGE_PARSE("902 *");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "You must use a nick assigned to you");

	IRC_MESSAGE_PARSE("902 * :test 902 message");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[1], "test 902 message");

	assert_eq(s->ircv3_sasl.state, IRCV3_SASL_STATE_NONE);
}

static void
test_ircv3_numeric_903(void)
{
	IRC_MESSAGE_PARSE("903 *");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_send[0], "CAP END");
	assert_strcmp(mock_line[0], "SASL authentication successful");

	IRC_MESSAGE_PARSE("903 * :test 903 message");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 2);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_send[1], "CAP END");
	assert_strcmp(mock_line[1], "test 903 message");

	assert_eq(s->ircv3_sasl.state, IRCV3_SASL_STATE_AUTHENTICATED);
}

static void
test_ircv3_numeric_904(void)
{
	IRC_MESSAGE_PARSE("904 *");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "SASL authentication failed");

	IRC_MESSAGE_PARSE("904 * :test 904 message");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[1], "test 904 message");

	assert_eq(s->ircv3_sasl.state, IRCV3_SASL_STATE_NONE);
}

static void
test_ircv3_numeric_905(void)
{
	IRC_MESSAGE_PARSE("905 *");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "SASL message too long");

	IRC_MESSAGE_PARSE("905 * :test 905 message");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[1], "test 905 message");

	assert_eq(s->ircv3_sasl.state, IRCV3_SASL_STATE_NONE);
}

static void
test_ircv3_numeric_906(void)
{
	IRC_MESSAGE_PARSE("906 *");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "SASL authentication aborted");

	IRC_MESSAGE_PARSE("906 * :test 906 message");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[1], "test 906 message");

	assert_eq(s->ircv3_sasl.state, IRCV3_SASL_STATE_NONE);
}

static void
test_ircv3_numeric_907(void)
{
	IRC_MESSAGE_PARSE("907 *");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "You have already authenticated using SASL");

	IRC_MESSAGE_PARSE("907 * :test 907 message");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[1], "test 907 message");

	assert_eq(s->ircv3_sasl.state, IRCV3_SASL_STATE_NONE);
}

static void
test_ircv3_numeric_908(void)
{
	IRC_MESSAGE_PARSE("908 *");
	assert_eq(irc_recv(s, &m), -1);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 1);
	assert_strcmp(mock_line[0], "RPL_SASLMECHS: missing mechanisms");

	IRC_MESSAGE_PARSE("908 * m1,m2,m3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 2);
	assert_strcmp(mock_line[1], "m1,m2,m3 are available SASL mechanisms");

	IRC_MESSAGE_PARSE("908 * m1,m2,m3 :test 908 message");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 3);
	assert_strcmp(mock_line[2], "m1,m2,m3 test 908 message");
}

static void
test_ircv3_cap_req_count(void)
{
	mock_reset();
	assert_eq(ircv3_cap_req_count(&(s->ircv3_caps)), 0);

	mock_reset();
	s->ircv3_caps.cap_1.req = 1;
	assert_eq(ircv3_cap_req_count(&(s->ircv3_caps)), 1);

	mock_reset();
	s->ircv3_caps.cap_1.req = 1;
	s->ircv3_caps.cap_2.req = 1;
	s->ircv3_caps.cap_3.req = 1;
	assert_eq(ircv3_cap_req_count(&(s->ircv3_caps)), 3);
}

static void
test_ircv3_cap_req_send(void)
{
	mock_reset();
	s->ircv3_caps.cap_1.req = 1;
	assert_eq(ircv3_cap_req_send(&(s->ircv3_caps), s), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 0);
	assert_strcmp(mock_send[0], "CAP REQ :cap-1");

	mock_reset();
	s->ircv3_caps.cap_1.req = 1;
	s->ircv3_caps.cap_3.req = 1;
	assert_eq(ircv3_cap_req_send(&(s->ircv3_caps), s), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 0);
	assert_strcmp(mock_send[0], "CAP REQ :cap-1 cap-3");

	mock_reset();
	s->ircv3_caps.cap_1.req = 1;
	s->ircv3_caps.cap_3.req = 1;
	s->ircv3_caps.cap_5.req = 1;
	assert_eq(ircv3_cap_req_send(&(s->ircv3_caps), s), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 0);
	assert_strcmp(mock_send[0], "CAP REQ :cap-1 cap-3 cap-5");
}

static void
test_ircv3_cap_end(void)
{
	/* test previously registered or doesn't support IRCv3 */
	s->registered = 1;

	assert_eq(ircv3_cap_end(s), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 0);

	/* test IRCv3 CAP negotiation in progress */
	server_reset(s);
	s->ircv3_caps.cap_1.req = 1;

	assert_eq(ircv3_cap_end(s), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 0);

	/* test IRCv3 SASL authentication in progress */
	server_reset(s);
	s->ircv3_sasl.state = IRCV3_SASL_STATE_REQ_MECH;

	assert_eq(ircv3_cap_end(s), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 0);

	/* test send CAP END */
	server_reset(s);

	assert_eq(ircv3_cap_end(s), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 0);
	assert_strcmp(mock_send[0], "CAP END");
}

static void
test_ircv3_sasl_init(void)
{
	/* test no sasl auth mechanism */
	assert_eq(ircv3_sasl_init(s), 0);
	assert_eq(mock_send_n, 0);
	assert_eq(mock_line_n, 0);

	/* test start authentication process, external */
	s->ircv3_sasl.mech = IRCV3_SASL_MECH_EXTERNAL;
	s->ircv3_sasl.state = IRCV3_SASL_STATE_NONE;

	assert_eq(ircv3_sasl_init(s), 0);
	assert_eq(mock_send_n, 1);
	assert_eq(mock_line_n, 0);
	assert_strcmp(mock_send[0], "AUTHENTICATE EXTERNAL");
	assert_eq(s->ircv3_sasl.state, IRCV3_SASL_STATE_REQ_MECH);

	/* test start authentication process, plain */
	s->ircv3_sasl.mech = IRCV3_SASL_MECH_PLAIN;
	s->ircv3_sasl.state = IRCV3_SASL_STATE_NONE;

	assert_eq(ircv3_sasl_init(s), 0);
	assert_eq(mock_send_n, 2);
	assert_eq(mock_line_n, 0);
	assert_strcmp(mock_send[1], "AUTHENTICATE PLAIN");
	assert_eq(s->ircv3_sasl.state, IRCV3_SASL_STATE_REQ_MECH);

	/* test authentication in progress */
	s->ircv3_sasl.state = IRCV3_SASL_STATE_REQ_MECH;

	assert_eq(ircv3_sasl_init(s), 0);
	assert_eq(mock_send_n, 2);
	assert_eq(mock_line_n, 0);

	/* test previously authenticated */
	s->ircv3_sasl.state = IRCV3_SASL_STATE_AUTHENTICATED;

	assert_eq(ircv3_sasl_init(s), 0);
	assert_eq(mock_send_n, 2);
	assert_eq(mock_line_n, 0);

	/* test invalid mechanism */
	s->ircv3_sasl.mech = -1;
	s->ircv3_sasl.state = IRCV3_SASL_STATE_NONE;

	assert_fatal(ircv3_sasl_init(s));

	/* test invalid state */
	s->ircv3_sasl.mech = IRCV3_SASL_MECH_PLAIN;
	s->ircv3_sasl.state = -1;

	assert_fatal(ircv3_sasl_init(s));
}

static int
test_init(void)
{
	mock_reset_io();
	mock_reset_state();

	if (!(s = server("host", "post", NULL, "user", "real", NULL)))
		return -1;

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
		TESTCASE(test_ircv3_recv_CAP),
		TESTCASE(test_ircv3_recv_CAP_LS),
		TESTCASE(test_ircv3_recv_CAP_LIST),
		TESTCASE(test_ircv3_recv_CAP_ACK),
		TESTCASE(test_ircv3_recv_CAP_NAK),
		TESTCASE(test_ircv3_recv_CAP_DEL),
		TESTCASE(test_ircv3_recv_CAP_NEW),
		TESTCASE(test_ircv3_recv_AUTHENTICATE),
		TESTCASE(test_ircv3_recv_AUTHENTICATE_EXTERNAL),
		TESTCASE(test_ircv3_recv_AUTHENTICATE_PLAIN),
		TESTCASE(test_ircv3_numeric_900),
		TESTCASE(test_ircv3_numeric_901),
		TESTCASE(test_ircv3_numeric_902),
		TESTCASE(test_ircv3_numeric_903),
		TESTCASE(test_ircv3_numeric_904),
		TESTCASE(test_ircv3_numeric_905),
		TESTCASE(test_ircv3_numeric_906),
		TESTCASE(test_ircv3_numeric_907),
		TESTCASE(test_ircv3_numeric_908),
		TESTCASE(test_ircv3_cap_req_count),
		TESTCASE(test_ircv3_cap_req_send),
		TESTCASE(test_ircv3_cap_end),
		TESTCASE(test_ircv3_sasl_init),
	};

	return run_tests(test_init, test_term, tests);
}
