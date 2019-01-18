#include "test/test.h"

/* Preclude definition for testing */
#define IO_MESG_LEN 10

#include "src/io.c"

/* Stubbed state callbacks */
void io_cb(enum io_cb_t t, const void *obj, ...) { UNUSED(t); UNUSED(obj); }
void io_cb_read_inp(char *buf, size_t n) { UNUSED(buf); UNUSED(n); }

static int cb_count;
static int cb_size;
static char soc_buf[IO_MESG_LEN + 1];

void io_cb_read_soc(char *buf, size_t n, const void *obj)
{
	UNUSED(obj);
	UNUSED(n);
	cb_count++;
	cb_size = (int)n;
	snprintf(soc_buf, sizeof(soc_buf), "%s", buf);
}

static void
test_io_recv(void)
{
	struct connection c;
	memset(&c, 0, sizeof(c));

#define IO_RECV(S) \
	io_recv(&c, (S), sizeof((S)) - 1);

	/* Test complete message received */
	soc_buf[0] = 0;

	IO_RECV("foo\r\nbar\r\n");
	assert_eq((signed) c.read.i, 0);
	assert_eq(cb_count, 2);
	assert_eq(cb_size, 3);
	assert_strcmp(soc_buf, "bar");

	/* Test empty messages */
	IO_RECV("\r\n\r\n");
	IO_RECV("\r");
	IO_RECV("\n");
	assert_eq(cb_count, 2);
	
	/* Test message received in multiple parts */
	IO_RECV("a");
	IO_RECV("b");
	IO_RECV("c\r");
	IO_RECV("\nx");
	assert_eq(cb_count, 3);
	assert_eq(cb_size, 3);
	assert_strcmp(soc_buf, "abc");
	IO_RECV("yz\r\n");
	assert_eq(cb_count, 4);
	assert_eq(cb_size, 3);
	assert_strcmp(soc_buf, "xyz");

	/* Test non-delimiter, non-CTCP control character are skiped */
	const char str1[] = {'a', 0x00, 0x01, 0x02, '\r', 'b', '\n', 'c', 0x01, '\r', '\n', 0};
	const char str2[] = {'a', 0x01, 'b', 'c', 0x01, 0};
	IO_RECV(str1);
	assert_eq(cb_count, 5);
	assert_eq(cb_size, 5);
	assert_strcmp(soc_buf, str2);

	/* Test buffer overrun */
	IO_RECV("abcdefghijklmnopqrstuvwxyz");
	IO_RECV("\r\n");
	assert_eq(cb_count, 6);
	assert_eq(cb_size, 10);
	assert_strcmp(soc_buf, "abcdefghij");

#undef IO_RECV
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_io_recv),
	};

	return run_tests(tests);
}
