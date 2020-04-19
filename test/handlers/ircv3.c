#include "test/test.h"

/* Precludes the definition in server.h */
#define IRCV3_CAPS \
	X("cap-1", cap_1) \
	X("cap-2", cap_2) \
	X("cap-3", cap_3) \
	X("cap-4", cap_4) \
	X("cap-5", cap_5)

struct ircv3_caps
{
	int cap_1;
	int cap_2;
	int cap_3;
	int cap_4;
	int cap_5;
};

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
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
#define TEST_SEND_BUF_MAX 5
#define TEST_LINE_BUF_MAX 5

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
	assert_eq(ircv3_CAP(s, &m), 1);
	assert_strcmp(line_buf[0], "CAP: target is null");

	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ");
	assert_eq(ircv3_CAP(s, &m), 1);
	assert_strcmp(line_buf[0], "CAP: command is null");

	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ ack");
	assert_eq(ircv3_CAP(s, &m), 1);
	assert_strcmp(line_buf[0], "CAP: unrecognized subcommand 'ack'");

	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ xxx");
	assert_eq(ircv3_CAP(s, &m), 1);
	assert_strcmp(line_buf[0], "CAP: unrecognized subcommand 'xxx'");
}

static void
test_ircv3_CAP_LS(void)
{
	struct server *s = server("host", "post", NULL, "user", "real");
	struct irc_message m;

	/* test empty LS, no parameter */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LS");
	assert_eq(ircv3_CAP(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_strcmp(line_buf[0], "CAP: parameter is null");

	/* test empty LS, no parameter, multiline */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LS *");
	assert_eq(ircv3_CAP(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_strcmp(line_buf[0], "CAP: parameter is null");

	/* test multiple parameters, no '*' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LS cap-1 cap-2");
	assert_eq(ircv3_CAP(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_strcmp(line_buf[0], "CAP: invalid parameters");

	/* test empty LS, with leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LS :");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 0);

	/* test no leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LS cap-1");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 1);
	assert_strcmp(send_buf[0], "CAP REQ :cap-1");

	/* test with leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LS :cap-1");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 1);
	assert_strcmp(send_buf[0], "CAP REQ :cap-1");

	/* test multiple caps */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LS :cap-1 cap-2 cap-3");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 3);
	assert_strcmp(send_buf[0], "CAP REQ :cap-1");
	assert_strcmp(send_buf[1], "CAP REQ :cap-2");
	assert_strcmp(send_buf[2], "CAP REQ :cap-3");

	/* test multiple caps, with unsupported */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LS :cap-1 foo cap-2 bar cap-3");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 3);
	assert_strcmp(send_buf[0], "CAP REQ :cap-1");
	assert_strcmp(send_buf[1], "CAP REQ :cap-2");
	assert_strcmp(send_buf[2], "CAP REQ :cap-3");

	/* test multiline */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LS * cap-1");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 0);
	IRC_MESSAGE_PARSE("CAP targ LS * :cap-2 cap-3");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 0);
	IRC_MESSAGE_PARSE("CAP targ LS cap-4");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 4);
	assert_strcmp(send_buf[0], "CAP REQ :cap-1");
	assert_strcmp(send_buf[1], "CAP REQ :cap-2");
	assert_strcmp(send_buf[2], "CAP REQ :cap-3");
	assert_strcmp(send_buf[3], "CAP REQ :cap-4");

	server_free(s);
}

static void
test_ircv3_CAP_LIST(void)
{
	struct server *s = server("host", "post", NULL, "user", "real");
	struct irc_message m;

	/* test empty LIST, no parameter */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LIST");
	assert_eq(ircv3_CAP(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP: parameter is null");

	/* test empty LIST, no parameter, multiline */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LIST *");
	assert_eq(ircv3_CAP(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP: parameter is null");

	/* test multiple parameters, no '*' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LIST cap-1 cap-2");
	assert_eq(ircv3_CAP(s, &m), 1);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP: invalid parameters");

	/* test empty LIST, with leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LIST :");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP LIST: (no caps set)");

	/* test no leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LIST cap-1");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP LIST: cap-1");

	/* test with leading ':' */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LIST :cap-1");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 0);

	/* test multiple caps */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LIST :cap-1 cap-2 cap-3");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 1);
	assert_strcmp(line_buf[0], "CAP LIST: cap-1 cap-2 cap-3");

	/* test multiline */
	test_buf_reset();
	IRC_MESSAGE_PARSE("CAP targ LIST * cap-1");
	assert_eq(ircv3_CAP(s, &m), 0);
	IRC_MESSAGE_PARSE("CAP targ LIST * :cap-2 cap-3");
	assert_eq(ircv3_CAP(s, &m), 0);
	IRC_MESSAGE_PARSE("CAP targ LIST cap-4");
	assert_eq(ircv3_CAP(s, &m), 0);
	assert_eq(send_buf_n, 0);
	assert_eq(line_buf_n, 3);
	assert_strcmp(line_buf[0], "CAP LIST: cap-1");
	assert_strcmp(line_buf[1], "CAP LIST: cap-2 cap-3");
	assert_strcmp(line_buf[2], "CAP LIST: cap-4");

	server_free(s);
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_ircv3_CAP),
		TESTCASE(test_ircv3_CAP_LS),
		TESTCASE(test_ircv3_CAP_LIST)
	};

	return run_tests(tests);
}
