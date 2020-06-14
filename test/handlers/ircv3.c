#include "test/test.h"

/* Precludes the definition in server.h */
#define IRCV3_CAPS \
	X("cap-1", cap_1, IRCV3_CAP_AUTO) \
	X("cap-2", cap_2, 0) \
	X("cap-3", cap_3, IRCV3_CAP_AUTO) \
	X("cap-4", cap_4, IRCV3_CAP_AUTO) \
	X("cap-5", cap_5, (IRCV3_CAP_AUTO | IRCV3_CAP_NO_DEL))

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/ircv3.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/handlers/irc_recv.c"
#include "src/handlers/ircv3.c"
#include "src/utils/utils.c"

#include "test/draw.mock.c"

#define IRC_MESSAGE_PARSE(S) \
	char TOKEN(buf, __LINE__)[] = S; \
	assert_eq(irc_message_parse(&m, TOKEN(buf, __LINE__)), 0);

#define TEST_BUF_SIZE 512
#define TEST_SEND_BUF_MAX 10
#define TEST_LINE_BUF_MAX 10

static void test_buf_reset(void);

static int line_buf_i;
static int line_buf_n;
static int send_buf_i;
static int send_buf_n;
static char chan_buf[TEST_BUF_SIZE];
static char line_buf[TEST_LINE_BUF_MAX][TEST_BUF_SIZE];
static char send_buf[TEST_SEND_BUF_MAX][TEST_BUF_SIZE];

static void
test_buf_reset(void)
{
	line_buf_i = 0;
	line_buf_n = 0;
	send_buf_i = 0;
	send_buf_n = 0;
	chan_buf[0] = 0;
	memset(line_buf, 0, TEST_LINE_BUF_MAX * TEST_BUF_SIZE);
	memset(send_buf, 0, TEST_SEND_BUF_MAX * TEST_BUF_SIZE);
}

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
	r2 = vsnprintf(line_buf[line_buf_i], sizeof(line_buf[line_buf_i]), fmt, ap);
	va_end(ap);

	line_buf_n++;

	if (line_buf_i++ == TEST_LINE_BUF_MAX)
		line_buf_i = 0;

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
	r2 = snprintf(line_buf[line_buf_i], sizeof(line_buf[line_buf_i]), "%s", fmt);

	line_buf_n++;

	if (line_buf_i++ == TEST_LINE_BUF_MAX)
		line_buf_i = 0;

	assert_gt(r1, 0);
	assert_gt(r2, 0);
}

struct channel* current_channel(void) { return NULL; }
void channel_set_current(struct channel *c) { UNUSED(c); }

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
	assert_gt(vsnprintf(send_buf[send_buf_i], sizeof(send_buf[0]), fmt, ap), 0);
	va_end(ap);

	send_buf_n++;

	if (send_buf_i++ == TEST_SEND_BUF_MAX)
		send_buf_i = 0;

	return 0;
}

int
io_dx(struct connection *c)
{
	UNUSED(c);

	return 0;
}

/* Mock irc_ctcp.c */
int
ctcp_request(struct server *s, const char *f, const char *t, char *m)
{
	UNUSED(s);
	UNUSED(f);
	UNUSED(t);
	UNUSED(m);
	return 0;
}

int
ctcp_response(struct server *s, const char *f, const char *t, char *m)
{
	UNUSED(s);
	UNUSED(f);
	UNUSED(t);
	UNUSED(m);
	return 0;
}

static void
test_ircv3_CAP(void)
{
	struct server *s = server("host", "post", NULL, "user", "real");
	struct irc_message m;

	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP");
	assert_eq(irc_recv(s, &m), 1);
	assert_strcmp(line_buf[0], "CAP: target is null");

	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP *");
	assert_eq(irc_recv(s, &m), 1);
	assert_strcmp(line_buf[0], "CAP: command is null");

	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * ack");
	assert_eq(irc_recv(s, &m), 1);
	assert_strcmp(line_buf[0], "CAP: unrecognized subcommand 'ack'");

	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * xxx");
	assert_eq(irc_recv(s, &m), 1);
	assert_strcmp(line_buf[0], "CAP: unrecognized subcommand 'xxx'");
}

static void
test_ircv3_CAP_LS(void)
{
	struct server *s = server("host", "post", NULL, "user", "real");
	struct irc_message m;

	/* test empty LS, no parameter */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LS");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_strcmp(line_buf[0], "CAP LS: parameter is null");

	/* test empty LS, no parameter, multiline */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LS *");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_strcmp(line_buf[0], "CAP LS: parameter is null");

	/* test multiple parameters, no '*' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LS cap-1 cap-2");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_strcmp(line_buf[0], "CAP LS: invalid parameters");

	/* test empty LS, with leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LS :");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 1);
	assert_strcmp(send_buf[0], "CAP END");

	/* test no leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LS cap-1");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 1);
	assert_strcmp(send_buf[0], "CAP REQ :cap-1");

	/* test with leading ':' */
	test_buf_reset();
	ircv3_caps_reset(&(s->ircv3_caps));
	IRC_MESSAGE_PARSE("CAP * LS :cap-1");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 1);
	assert_strcmp(send_buf[0], "CAP REQ :cap-1");
	assert_eq(s->ircv3_caps.cap_1.supported, 1);
	assert_eq(s->ircv3_caps.cap_2.supported, 0);

	/* test multiple caps, cap_2 is non-auto */
	test_buf_reset();
	ircv3_caps_reset(&(s->ircv3_caps));
	IRC_MESSAGE_PARSE("CAP * LS :cap-1 cap-2 cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 2);
	assert_strcmp(send_buf[0], "CAP REQ :cap-1");
	assert_strcmp(send_buf[1], "CAP REQ :cap-3");
	assert_eq(s->ircv3_caps.cap_1.supported, 1);
	assert_eq(s->ircv3_caps.cap_2.supported, 1);
	assert_eq(s->ircv3_caps.cap_3.supported, 1);

	/* test multiple caps, with unsupported */
	test_buf_reset();
	ircv3_caps_reset(&(s->ircv3_caps));
	IRC_MESSAGE_PARSE("CAP * LS :cap-1 foo cap-2 bar cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 2);
	assert_strcmp(send_buf[0], "CAP REQ :cap-1");
	assert_strcmp(send_buf[1], "CAP REQ :cap-3");
	assert_eq(s->ircv3_caps.cap_1.supported, 1);
	assert_eq(s->ircv3_caps.cap_2.supported, 1);
	assert_eq(s->ircv3_caps.cap_3.supported, 1);

	/* test multiline */
	test_buf_reset();
	ircv3_caps_reset(&(s->ircv3_caps));
	IRC_MESSAGE_PARSE("CAP * LS * cap-1");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	IRC_MESSAGE_PARSE("CAP * LS * :cap-2 cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	IRC_MESSAGE_PARSE("CAP * LS cap-4");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 3);
	assert_strcmp(send_buf[0], "CAP REQ :cap-1");
	assert_strcmp(send_buf[1], "CAP REQ :cap-3");
	assert_strcmp(send_buf[2], "CAP REQ :cap-4");
	assert_eq(s->ircv3_caps.cap_1.supported, 1);
	assert_eq(s->ircv3_caps.cap_2.supported, 1);
	assert_eq(s->ircv3_caps.cap_3.supported, 1);
	assert_eq(s->ircv3_caps.cap_4.supported, 1);

	/* test registered */
	test_buf_reset();
	s->registered = 1;
	IRC_MESSAGE_PARSE("CAP * LS :cap-1 cap-2 cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP LS: cap-1 cap-2 cap-3");

	server_free(s);
}

static void
test_ircv3_CAP_LIST(void)
{
	struct server *s = server("host", "post", NULL, "user", "real");
	struct irc_message m;

	/* test empty LIST, no parameter */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LIST");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP LIST: parameter is null");

	/* test empty LIST, no parameter, multiline */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LIST *");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP LIST: parameter is null");

	/* test multiple parameters, no '*' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LIST cap-1 cap-2");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP LIST: invalid parameters");

	/* test empty LIST, with leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LIST :");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP LIST: (no capabilities)");

	/* test no leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LIST cap-1");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP LIST: cap-1");

	/* test with leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LIST :cap-1");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);

	/* test multiple caps */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LIST :cap-1 cap-2 cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP LIST: cap-1 cap-2 cap-3");

	/* test multiline */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * LIST * cap-1");
	assert_eq(irc_recv(s, &m), 0);
	IRC_MESSAGE_PARSE("CAP * LIST * :cap-2 cap-3");
	assert_eq(irc_recv(s, &m), 0);
	IRC_MESSAGE_PARSE("CAP * LIST cap-4");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 3);
	assert_strcmp(line_buf[0], "CAP LIST: cap-1");
	assert_strcmp(line_buf[1], "CAP LIST: cap-2 cap-3");
	assert_strcmp(line_buf[2], "CAP LIST: cap-4");

	server_free(s);
}

static void
test_ircv3_CAP_ACK(void)
{
	struct server *s = server("host", "post", NULL, "user", "real");
	struct irc_message m;

	s->registered = 0;

	/* test empty ACK */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * ACK");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP ACK: parameter is null");

	/* test empty ACK, with leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * ACK :");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP ACK: parameter is empty");

	/* unregisterd, error */
	test_buf_reset();
	s->registered = 0;
	s->ircv3_caps.cap_1.set = 0;
	s->ircv3_caps.cap_2.set = 0;
	s->ircv3_caps.cap_1.req = 1;
	s->ircv3_caps.cap_2.req = 1;
	IRC_MESSAGE_PARSE("CAP * ACK :cap-1 cap-aaa cap-2 cap-bbb");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 5);
	assert_strcmp(line_buf[0], "capability change accepted: cap-1");
	assert_strcmp(line_buf[1], "CAP ACK: 'cap-aaa' not supported");
	assert_strcmp(line_buf[2], "capability change accepted: cap-2");
	assert_strcmp(line_buf[3], "CAP ACK: 'cap-bbb' not supported");
	assert_strcmp(line_buf[4], "CAP ACK: parameter errors");

	/* unregistered, no error */
	test_buf_reset();
	s->registered = 0;
	s->ircv3_caps.cap_1.set = 0;
	s->ircv3_caps.cap_2.set = 1;
	s->ircv3_caps.cap_3.set = 1;
	s->ircv3_caps.cap_1.req = 1;
	s->ircv3_caps.cap_2.req = 1;
	s->ircv3_caps.cap_3.req = 1;
	IRC_MESSAGE_PARSE("CAP * ACK :cap-1 -cap-2");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 2);
	assert_strcmp(line_buf[0], "capability change accepted: cap-1");
	assert_strcmp(line_buf[1], "capability change accepted: -cap-2");
	assert_eq(s->ircv3_caps.cap_1.set, 1);
	assert_eq(s->ircv3_caps.cap_2.set, 0);
	IRC_MESSAGE_PARSE("CAP * ACK :-cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 1);
	assert_eq(line_buf_n, 3);
	assert_strcmp(line_buf[2], "capability change accepted: -cap-3");
	assert_strcmp(send_buf[0], "CAP END");

	/* registered, error */
	test_buf_reset();
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
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 5);
	assert_strcmp(line_buf[0], "capability change accepted: cap-1");
	assert_strcmp(line_buf[1], "CAP ACK: 'cap-2' was set");
	assert_strcmp(line_buf[2], "CAP ACK: 'cap-3' was not set");
	assert_strcmp(line_buf[3], "CAP ACK: '-cap-4' was not requested");
	assert_strcmp(line_buf[4], "CAP ACK: parameter errors");

	/* registered, no error */
	test_buf_reset();
	s->registered = 1;
	s->ircv3_caps.cap_1.set = 0;
	s->ircv3_caps.cap_2.set = 1;
	s->ircv3_caps.cap_1.req = 1;
	s->ircv3_caps.cap_2.req = 1;
	IRC_MESSAGE_PARSE("CAP * ACK :cap-1 -cap-2");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 2);
	assert_strcmp(line_buf[0], "capability change accepted: cap-1");
	assert_strcmp(line_buf[1], "capability change accepted: -cap-2");
	assert_eq(s->ircv3_caps.cap_1.set, 1);
	assert_eq(s->ircv3_caps.cap_2.set, 0);

	server_free(s);
}

static void
test_ircv3_CAP_NAK(void)
{
	struct server *s = server("host", "post", NULL, "user", "real");
	struct irc_message m;

	/* test empty NAK */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * NAK");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP NAK: parameter is null");

	/* test empty NAK, with leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * NAK :");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP NAK: parameter is empty");

	/* test unsupported caps */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * NAK :cap-aaa cap-bbb cap-ccc");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 1);
	assert_eq(line_buf_n, 3);
	assert_strcmp(line_buf[0], "capability change rejected: cap-aaa");
	assert_strcmp(line_buf[1], "capability change rejected: cap-bbb");
	assert_strcmp(line_buf[2], "capability change rejected: cap-ccc");
	assert_strcmp(send_buf[0], "CAP END");

	/* test supported caps */
	test_buf_reset();
	s->registered = 0;
	s->ircv3_caps.cap_1.req = 0;
	s->ircv3_caps.cap_2.req = 1;
	s->ircv3_caps.cap_3.req = 1;
	s->ircv3_caps.cap_1.set = 0;
	s->ircv3_caps.cap_2.set = 1;
	s->ircv3_caps.cap_3.set = 1;
	IRC_MESSAGE_PARSE("CAP * NAK :cap-1 cap-2");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 2);
	assert_strcmp(line_buf[0], "capability change rejected: cap-1");
	assert_strcmp(line_buf[1], "capability change rejected: cap-2");
	assert_eq(s->ircv3_caps.cap_1.req, 0);
	assert_eq(s->ircv3_caps.cap_2.req, 0);
	assert_eq(s->ircv3_caps.cap_1.set, 0);
	assert_eq(s->ircv3_caps.cap_2.set, 1);
	IRC_MESSAGE_PARSE("CAP * NAK :cap-3");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 1);
	assert_eq(line_buf_n, 3);
	assert_strcmp(line_buf[2], "capability change rejected: cap-3");
	assert_strcmp(send_buf[0], "CAP END");

	/* test registered - don't send END */
	test_buf_reset();
	s->registered = 1;
	s->ircv3_caps.cap_1.req = 0;
	s->ircv3_caps.cap_2.req = 1;
	s->ircv3_caps.cap_1.set = 0;
	s->ircv3_caps.cap_2.set = 1;
	IRC_MESSAGE_PARSE("CAP * NAK :cap-1 cap-2");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 2);
	assert_strcmp(line_buf[0], "capability change rejected: cap-1");
	assert_strcmp(line_buf[1], "capability change rejected: cap-2");
	assert_eq(s->ircv3_caps.cap_1.req, 0);
	assert_eq(s->ircv3_caps.cap_2.req, 0);
	assert_eq(s->ircv3_caps.cap_1.set, 0);
	assert_eq(s->ircv3_caps.cap_2.set, 1);

	server_free(s);
}

static void
test_ircv3_CAP_DEL(void)
{
	struct server *s = server("host", "post", NULL, "user", "real");
	struct irc_message m;

	/* test empty DEL */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * DEL");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP DEL: parameter is null");

	/* test empty DEL, with leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * DEL :");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP DEL: parameter is empty");

	/* test cap-5 doesn't support DEL */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * DEL :cap-5");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP DEL: 'cap-5' doesn't support DEL");

	/* test unsupported caps */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * DEL :cap-aaa cap-bbb cap-ccc");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 0);

	/* test supported caps */
	test_buf_reset();
	s->ircv3_caps.cap_1.req = 0;
	s->ircv3_caps.cap_2.req = 1;
	s->ircv3_caps.cap_1.set = 1;
	s->ircv3_caps.cap_2.set = 0;
	s->ircv3_caps.cap_1.supported = 1;
	s->ircv3_caps.cap_2.supported = 1;
	IRC_MESSAGE_PARSE("CAP * DEL :cap-1 cap-2");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 2);
	assert_strcmp(line_buf[0], "capability lost: cap-1");
	assert_strcmp(line_buf[1], "capability lost: cap-2");
	assert_eq(s->ircv3_caps.cap_1.req, 0);
	assert_eq(s->ircv3_caps.cap_2.req, 0);
	assert_eq(s->ircv3_caps.cap_1.set, 0);
	assert_eq(s->ircv3_caps.cap_2.set, 0);
	assert_eq(s->ircv3_caps.cap_1.supported, 0);
	assert_eq(s->ircv3_caps.cap_2.supported, 0);

	server_free(s);
}

static void
test_ircv3_CAP_NEW(void)
{
	struct server *s = server("host", "post", NULL, "user", "real");
	struct irc_message m;

	/* test empty NEW */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * NEW");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP NEW: parameter is null");

	/* test empty DEL, with leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * NEW :");
	assert_eq(irc_recv(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP NEW: parameter is empty");

	/* test unsupported caps */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP * NEW :cap-aaa cap-bbb cap-ccc");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 0);

	/* test supported caps */
	test_buf_reset();
	s->ircv3_caps.cap_3.req = 0;
	s->ircv3_caps.cap_4.req = 0;
	s->ircv3_caps.cap_5.req = 1; /* cap req - don't send REQ */
	s->ircv3_caps.cap_3.set = 1; /* cap set - don't send REQ */
	s->ircv3_caps.cap_4.set = 0;
	s->ircv3_caps.cap_5.set = 0;
	s->ircv3_caps.cap_3.supported = 0;
	s->ircv3_caps.cap_4.supported = 0;
	s->ircv3_caps.cap_5.supported = 0;
	IRC_MESSAGE_PARSE("CAP * NEW :cap-3 cap-4 cap-5");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 1);
	assert_eq(line_buf_n, 3);
	assert_strcmp(line_buf[0], "new capability: cap-3");
	assert_strcmp(line_buf[1], "new capability: cap-4");
	assert_strcmp(line_buf[2], "new capability: cap-5");
	assert_strcmp(send_buf[0], "CAP REQ :cap-4");
	assert_eq(s->ircv3_caps.cap_3.supported, 1);
	assert_eq(s->ircv3_caps.cap_4.supported, 1);
	assert_eq(s->ircv3_caps.cap_5.supported, 1);

	/* test supported caps, cap_2 is non-auto */
	test_buf_reset();
	s->ircv3_caps.cap_2.req = 0;
	s->ircv3_caps.cap_2.set = 1;
	s->ircv3_caps.cap_2.supported = 0;
	IRC_MESSAGE_PARSE("CAP * NEW :cap-2");
	assert_eq(irc_recv(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "new capability: cap-2");
	assert_eq(s->ircv3_caps.cap_2.supported, 1);

	server_free(s);
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_ircv3_CAP),
		TESTCASE(test_ircv3_CAP_LS),
		TESTCASE(test_ircv3_CAP_LIST),
		TESTCASE(test_ircv3_CAP_ACK),
		TESTCASE(test_ircv3_CAP_NAK),
		TESTCASE(test_ircv3_CAP_DEL),
		TESTCASE(test_ircv3_CAP_NEW),
	};

	return run_tests(tests);
}
