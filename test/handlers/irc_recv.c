#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/ircv3_cap.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/handlers/irc_recv.c"
#include "src/handlers/ircv3.c"
#include "src/utils/utils.c"

#include "test/draw.mock.c"

static char chan_buf[1024];
static char line_buf[1024];
static char send_buf[1024];

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
	assert_gt(vsnprintf(send_buf, sizeof(send_buf), fmt, ap), 0);
	va_end(ap);

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
test_353(void)
{
	struct channel *c1 = channel("c1", CHANNEL_T_CHANNEL);
	struct channel *c2 = channel("c2", CHANNEL_T_CHANNEL);
	struct channel *c3 = channel("c3", CHANNEL_T_CHANNEL);
	struct server *s = server("host", "post", NULL, "user", "real");
	struct irc_message m;
	struct user *u1;
	struct user *u2;
	struct user *u3;
	struct user *u4;

	channel_list_add(&s->clist, c1);
	channel_list_add(&s->clist, c2);
	channel_list_add(&s->clist, c3);

	#define IRC_MESSAGE_PARSE(S) \
		char TOKEN(buf, __LINE__)[] = S; \
		assert_eq(irc_message_parse(&m, TOKEN(buf, __LINE__)), 0);

	// TODO: line_buf can be used to check the error message

	server_nick_set(s, "mynick");

	IRC_MESSAGE_PARSE("353 c1");
	assert_eq(irc_353(s, &m), 1);

	IRC_MESSAGE_PARSE("353 c1 =");
	assert_eq(irc_353(s, &m), 1);

	IRC_MESSAGE_PARSE("353 = c1 :n1");
	assert_eq(irc_353(s, &m), 0);

	if (user_list_get(&(c1->users), CASEMAPPING_RFC1459, "n1", 0) == NULL)
		test_abort("Failed to retrieve u");

	IRC_MESSAGE_PARSE("353 = c2 :n1 n2 n3");
	assert_eq(irc_353(s, &m), 0);

	if ((user_list_get(&(c2->users), CASEMAPPING_RFC1459, "n1", 0) == NULL)
	 || (user_list_get(&(c2->users), CASEMAPPING_RFC1459, "n2", 0) == NULL)
	 || (user_list_get(&(c2->users), CASEMAPPING_RFC1459, "n3", 0) == NULL))
		test_abort("Failed to retrieve users");

	IRC_MESSAGE_PARSE("353 = c3 :@n1 +n2 @+n3 +@n4");
	assert_eq(irc_353(s, &m), 0);

	if (((u1 = user_list_get(&(c3->users), CASEMAPPING_RFC1459, "n1", 0)) == NULL)
	 || ((u2 = user_list_get(&(c3->users), CASEMAPPING_RFC1459, "n2", 0)) == NULL)
	 || ((u3 = user_list_get(&(c3->users), CASEMAPPING_RFC1459, "n3", 0)) == NULL)
	 || ((u4 = user_list_get(&(c3->users), CASEMAPPING_RFC1459, "n4", 0)) == NULL))
		test_abort("Failed to retrieve users");

	assert_eq(u1->prfxmodes.prefix, '@');
	assert_eq(u2->prfxmodes.prefix, '+');
	assert_eq(u3->prfxmodes.prefix, '@');
	assert_eq(u4->prfxmodes.prefix, '@');

	IRC_MESSAGE_PARSE("MODE c3 -o n1");
	assert_eq(recv_mode(s, &m), 0);

	IRC_MESSAGE_PARSE("MODE c3 -v n2");
	assert_eq(recv_mode(s, &m), 0);

	IRC_MESSAGE_PARSE("MODE c3 -o n3");
	assert_eq(recv_mode(s, &m), 0);

	IRC_MESSAGE_PARSE("MODE c3 -v n4");
	assert_eq(recv_mode(s, &m), 0);

	assert_eq(u1->prfxmodes.prefix, 0);
	assert_eq(u2->prfxmodes.prefix, 0);
	assert_eq(u3->prfxmodes.prefix, '+');
	assert_eq(u4->prfxmodes.prefix, '@');

	#undef IRC_MESSAGE_PARSE

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
