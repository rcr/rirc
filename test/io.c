#include "test/test.h"
#include "src/io.c"

/* Stubbed state callbacks */
void io_cb_cxng(const void *obj, const char *fmt, ...) { UNUSED(obj); UNUSED(fmt); }
void io_cb_cxed(const void *obj, const char *fmt, ...) { UNUSED(obj); UNUSED(fmt); }
void io_cb_fail(const void *obj, const char *fmt, ...) { UNUSED(obj); UNUSED(fmt); }
void io_cb_lost(const void *obj, const char *fmt, ...) { UNUSED(obj); UNUSED(fmt); }
void io_cb_ping(const void *obj, unsigned int ping) { UNUSED(obj); UNUSED(ping); }
void io_cb_signal(int sig) { UNUSED(sig); }
void io_cb_read_inp(char *buf, size_t n) { UNUSED(buf); UNUSED(n); }

static int cb_count;
static char soc_buf[IO_MESG_LEN];

void io_cb_read_soc(char *buf, size_t n, const void *obj)
{
	UNUSED(obj);
	UNUSED(n);
	cb_count++;
	snprintf(soc_buf, sizeof(soc_buf), "%s", buf);
}

static void
test_io_recv(void)
{
	struct connection c = {0};

#define IO_RECV(C, S) \
	io_recv(&(C), (S), sizeof((S))-1);

	/* Test complete message received */
	soc_buf[0] = 0;

	IO_RECV(c, "foo\r\n");
	assert_eq((signed) c.read.i, 0);
	assert_eq(cb_count, 1);
	assert_strcmp(soc_buf, "foo");

	/* Test message received in multiple parts */
	soc_buf[0] = 0;

	IO_RECV(c, "testing");
	assert_eq((signed) c.read.i, 7);
	assert_eq(cb_count, 1);
	assert_strcmp(soc_buf, "");

	IO_RECV(c, "\rfoo");
	assert_eq((signed) c.read.i, 11);
	assert_eq(cb_count, 1);
	assert_strcmp(soc_buf, "");

	IO_RECV(c, "\nbar\r");
	assert_eq((signed) c.read.i, 16);
	assert_eq(cb_count, 1);
	assert_strcmp(soc_buf, "");

	IO_RECV(c, "\n");
	assert_eq((signed) c.read.i, 0);
	assert_eq(cb_count, 2);
	assert_strcmp(soc_buf, "testing\rfoo\nbar");

	/* Test empty message is discarded */
	soc_buf[0] = 0;

	IO_RECV(c, "\r");
	IO_RECV(c, "\n");
	assert_eq((signed) c.read.i, 0);
	assert_eq(cb_count, 2);
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_io_recv),
	};

	return run_tests(tests);
}
