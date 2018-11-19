#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/state.c"
#include "src/mesg.c"
#include "src/utils/utils.c"

/* Mock rirc.c */
const char *default_username = "username";
const char *default_realname = "realname";

/* Mock draw.c */
void draw(union draw d) { UNUSED(d); }
void draw_bell(void) { ; }
void draw_term(void) { ; }
void
split_buffer_cols(
	struct buffer_line *l,
	unsigned int *h,
	unsigned int *t,
	unsigned int c,
	unsigned int p)
{
	UNUSED(l);
	UNUSED(h);
	UNUSED(t);
	UNUSED(c);
	UNUSED(p);
}

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

#define INP_S(S) io_cb_read_inp((S), strlen(S))
#define INP_C(C) io_cb_read_inp((char[]){(C)}, 1)
#define CURRENT_LINE (buffer_head(&current_channel()->buffer)->text)

static void
test_state(void)
{
	state_init();

	INP_S("/");
	INP_C(0x0A);
	assert_strcmp(CURRENT_LINE, "This is not a server");

	INP_S("//");
	INP_C(0x0A);
	assert_strcmp(CURRENT_LINE, "This is not a server");

	INP_S(":");
	INP_C(0x0A);
	assert_strcmp(CURRENT_LINE, "Messages beginning with ':' require a command");

	INP_S("::");
	INP_C(0x0A);
	assert_strcmp(CURRENT_LINE, "Messages beginning with ':' require a command");

	INP_S("test");
	INP_C(0x0A);
	assert_strcmp(CURRENT_LINE, "This is not a server");

	INP_C(CTRL('c'));
	INP_C(CTRL('p'));
	INP_C(CTRL('x'));
	assert_strcmp(CURRENT_LINE, "Type :quit to exit rirc");

	INP_C(CTRL('l'));
	assert_null(buffer_head(&current_channel()->buffer));

	struct server *s1 = server("h1", "p1", NULL, "u1", "r1");
	struct server *s2 = server("h2", "p2", NULL, "u2", "r2");
	struct server *s3 = server("h3", "p3", NULL, "u3", "r3");

	assert_ptrequals(server_list_add(state_server_list(), s1), NULL);
	assert_ptrequals(server_list_add(state_server_list(), s2), NULL);
	assert_ptrequals(server_list_add(state_server_list(), s3), NULL);

	state_term();
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_state),
	};

	return run_tests(tests);
}
