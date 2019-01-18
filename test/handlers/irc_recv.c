#include "test/test.h"

#include "src/components/buffer.c"
#include "src/components/channel.c"
#include "src/components/input.c"
#include "src/components/mode.c"
#include "src/components/server.c"
#include "src/components/user.c"
#include "src/handlers/irc_recv.c"
#include "src/utils/utils.c"

static char chan_buf[1024];
static char line_buf[1024];
static char send_buf[1024];

/* Mock stat.c */
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
