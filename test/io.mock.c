#include <stdarg.h>

#define MOCK_SEND_LEN 512
#define MOCK_SEND_N   10

static char mock_send[MOCK_SEND_N][MOCK_SEND_LEN];
static unsigned mock_send_i;
static unsigned mock_send_n;

void
mock_reset_io(void)
{
	mock_send_i = 0;
	mock_send_n = 0;
	memset(mock_send, 0, MOCK_SEND_LEN * MOCK_SEND_N);
}

int
io_sendf(struct connection *c, const char *fmt, ...)
{
	va_list ap;

	UNUSED(c);

	va_start(ap, fmt);
	assert_gt(vsnprintf(mock_send[mock_send_i], sizeof(mock_send[0]), fmt, ap), 0);
	va_end(ap);

	mock_send_n++;

	if (mock_send_i++ == MOCK_SEND_N)
		mock_send_i = 0;

	return 0;
}

struct connection*
connection(const void *o, const char *h, const char *p, uint8_t f)
{
	UNUSED(o);
	UNUSED(h);
	UNUSED(p);
	UNUSED(f);
	return NULL;
}

const char* io_err(int err) { UNUSED(err); return "err"; }
int io_cx(struct connection *c) { UNUSED(c); return 0; }
int io_dx(struct connection *c) { UNUSED(c); return 0; }
unsigned io_tty_cols(void) { return 0; }
unsigned io_tty_rows(void) { return 0; }
void connection_free(struct connection *c) { UNUSED(c); }
void io_init(void) { ; }
void io_start(void) { ; }
void io_stop(void) { ; }
