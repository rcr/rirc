#include <stdarg.h>

#define MOCK_CHAN_LEN 512
#define MOCK_LINE_LEN 512
#define MOCK_LINE_N   10

static char mock_chan[MOCK_LINE_N][MOCK_CHAN_LEN];
static char mock_line[MOCK_LINE_N][MOCK_LINE_LEN];
static unsigned mock_line_i;
static unsigned mock_line_n;
static struct channel *mock_current_chan;

void
mock_reset_state(void)
{
	mock_current_chan = NULL;
	mock_line_i = 0;
	mock_line_n = 0;
	memset(mock_chan, 0, MOCK_LINE_N * MOCK_CHAN_LEN);
	memset(mock_line, 0, MOCK_LINE_N * MOCK_LINE_LEN);
}

void
newlinef(struct channel *c, enum buffer_line_type t, const char *f, const char *fmt, ...)
{
	va_list ap;
	int r1;
	int r2;

	UNUSED(f);
	UNUSED(t);

	va_start(ap, fmt);
	r1 = snprintf(mock_chan[mock_line_i], sizeof(mock_chan[mock_line_i]), "%s", c->name);
	r2 = vsnprintf(mock_line[mock_line_i], sizeof(mock_line[mock_line_i]), fmt, ap);
	va_end(ap);

	mock_line_n++;

	if (mock_line_i++ == MOCK_LINE_N)
		mock_line_i = 0;

	assert_gt(r1, 0);
	assert_gt(r2, 0);
}

void
newline(struct channel *c, enum buffer_line_type t, const char *f, const char *fmt)
{
	int r1;
	int r2;

	UNUSED(f);
	UNUSED(t);

	r1 = snprintf(mock_chan[mock_line_i], sizeof(mock_chan[mock_line_i]), "%s", c->name);
	r2 = snprintf(mock_line[mock_line_i], sizeof(mock_line[mock_line_i]), "%s", fmt);

	mock_line_n++;

	if (mock_line_i++ == MOCK_LINE_N)
		mock_line_i = 0;

	assert_gt(r1, 0);
	assert_gt(r2, 0);
}

struct channel* current_channel(void) { return mock_current_chan; }
void channel_set_current(struct channel *c) { mock_current_chan = c; }
