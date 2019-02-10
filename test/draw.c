#include "test/test.h"

/* FIXME: decouple draw.c and state.c, irc_send.c, irc_recv.c */

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/draw.c"
#include "src/state.c"
#include "src/utils/utils.c"

/* Mock rirc.c */
const char *default_username = "username";
const char *default_realname = "realname";

/* Mock io.c */
struct connection*
connection(const void *o, const char *h, const char *p)
{
	UNUSED(o);
	UNUSED(h);
	UNUSED(p);
	return NULL;
}
const char* io_err(int err) { UNUSED(err); return "err"; }
int io_cx(struct connection *c) { UNUSED(c); return 0; }
int io_dx(struct connection *c) { UNUSED(c); return 0; }
int io_sendf(struct connection *c, const char *f, ...) { UNUSED(c); UNUSED(f); return 0; }
unsigned io_tty_cols(void) { return 0; }
unsigned io_tty_rows(void) { return 0; }
void io_free(struct connection *c) { UNUSED(c); }
void io_term(void) { ; }

/* Mock handlers/irc_send.c */
int irc_send_command(struct server *s, struct channel *c, char *m) { UNUSED(s); UNUSED(c); UNUSED(m); return 0; }
int irc_send_privmsg(struct server *s, struct channel *c, char *m) { UNUSED(s); UNUSED(c); UNUSED(m); return 0; }

/* Mock handlers/irc_recv.c */
int irc_recv(struct server *s, struct irc_message *m) { UNUSED(s); UNUSED(m); return 0; }

static void
test_STUB(void)
{
	; /* TODO */
}


int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_STUB)
	};

	return run_tests(tests);
}