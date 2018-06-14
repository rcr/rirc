#include "test/test.h"
#include "src/io.c"


void io_cb_cxng(const void *obj, const char *fmt, ...) { UNUSED(obj); UNUSED(fmt); }
void io_cb_cxed(const void *obj, const char *fmt, ...) { UNUSED(obj); UNUSED(fmt); }
void io_cb_fail(const void *obj, const char *fmt, ...) { UNUSED(obj); UNUSED(fmt); }
void io_cb_lost(const void *obj, const char *fmt, ...) { UNUSED(obj); UNUSED(fmt); }
void io_cb_ping(const void *obj, unsigned int ping) { UNUSED(obj); UNUSED(ping); }
void io_cb_signal(int sig) { UNUSED(sig); }
void io_cb_read_inp(char *buf, size_t n) { UNUSED(buf); UNUSED(n); }
void io_cb_read_soc(char *buf, size_t n, const void *obj) { UNUSED(buf); UNUSED(n); UNUSED(obj); }

static void
test_(void)
{
	
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_),
	};

	return run_tests(tests);
}
