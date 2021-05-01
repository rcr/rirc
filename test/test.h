#ifndef TEST_H
#define TEST_H

/* test.h -- unit test framework for rirc
 *
 * Defines the following macros:
 *
 *   Signed, unsigned comparison:
 *    - assert_true(expr)
 *    - assert_false(expr)
 *    - assert_eq(expr, expected)   - signed equality
 *    - assert_lt(expr, expected)   - signed less than
 *    - assert_gt(expr, expected)   - signed greater than
 *    - assert_ueq(expr, expected)  - unsigned equality
 *
 *    String comparison:
 *    - assert_strcmp(expr, expected)
 *    - assert_strncmp(expr, expected, len)
 *
 *   Pointer comparison:
 *    - assert_ptr_eq(expr, expected)
 *    - assert_ptr_null(expr)
 *    - assert_ptr_not_null(expr)
 *
 *   Assert that the expression exits rirc fatally:
 *    - assert_fatal(expr)
 *
 *   Explicitly fail or abort a test [with formated] message
 *    - test_fail(M)
 *    - test_failf(M, ...)
 *    - test_abort(M)
 *    - test_abortf(M, ...)
 *    - test_abort_main(M)
 */

#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TESTCASE(X) { &(X), #X }
#define TESTING

#define TOKEN_PASTE(x, y) x##y
#define TOKEN(x, y) TOKEN_PASTE(x, y)

#define assert_true(X) \
	do { \
		if (!(X)) \
			test_fail(#X " expected true"); \
	} while (0)

#define assert_false(X) \
	do { \
		if ((X)) \
			test_fail(#X " expected false"); \
	} while (0)

#define assert_eq(X, Y) \
	do { \
		int __ret_x = (int)(X); \
		int __ret_y = (int)(Y); \
		if (__ret_x != __ret_y) \
			test_failf(#X " expected '%d', got '%d'", __ret_y, __ret_x); \
	} while (0)

#define assert_gt(X, Y) \
	do { \
		int __ret_x = (int)(X); \
		int __ret_y = (int)(Y); \
		if (!(__ret_x > __ret_y)) \
			test_failf(#X " expected '%d' to be greater than '%d'", __ret_y, __ret_x); \
	} while (0)

#define assert_lt(X, Y) \
	do { \
		int __ret_x = (int)(X); \
		int __ret_y = (int)(Y); \
		if (!(__ret_x < __ret_y)) \
			test_failf(#X " expected '%d' to be less than '%d'", __ret_y, __ret_x); \
	} while (0)

#define assert_ueq(X, Y) \
	do { \
		unsigned __ret_x = (unsigned)(X); \
		unsigned __ret_y = (unsigned)(Y); \
		if (__ret_x != __ret_y) \
			test_failf(#X " expected '%u', got '%u'", __ret_y, __ret_x); \
	} while (0)

#define assert_strcmp(X, Y) \
	do { \
		const char *__ret_x = (X); \
		const char *__ret_y = (Y); \
		if (_assert_strcmp(__ret_x, __ret_y)) \
			test_failf(#X " expected '%s', got '%s'", \
				__ret_y == NULL ? "NULL" : __ret_y, \
				__ret_x == NULL ? "NULL" : __ret_x); \
	} while (0)

#define assert_strncmp(X, Y, N) \
	do { \
		const char *__ret_x = (X); \
		const char *__ret_y = (Y); \
		if (_assert_strncmp(__ret_x, __ret_y, (N))) \
			test_failf(#X " expected '%.*s', got '%.*s'", \
				(int)(N), __ret_y == NULL ? "NULL" : __ret_y, \
				(int)(N), __ret_x == NULL ? "NULL" : __ret_x); \
	} while (0)

#define assert_ptr_eq(X, Y) \
	do { \
		uintptr_t __ret_x = (uintptr_t)(X); \
		uintptr_t __ret_y = (uintptr_t)(Y); \
		if (__ret_x != __ret_y) \
			test_failf(#X " expected '%016" PRIxPTR "', got '%016" PRIxPTR "'", __ret_y, __ret_x); \
	} while (0)

#define assert_ptr_null(X) \
	do { \
		uintptr_t __ret_x = (uintptr_t)(X); \
		if (__ret_x) \
			test_failf(#X " expected NULL, got '%016" PRIxPTR "'", __ret_x); \
	} while (0)

#define assert_ptr_not_null(X) \
	do { \
		uintptr_t __ret_x = (uintptr_t)(X); \
		if (!__ret_x) \
			test_fail(#X " expected not NULL"); \
	} while (0)

#define assert_fatal(X) \
	do { \
		if (setjmp(_tc_fatal_expected_)) { \
			_assert_fatal_ = 1; \
			(X); \
			if (_assert_fatal_) \
				test_fail("'"#X "' should have exited fatally"); \
			_assert_fatal_ = 0; \
		} \
	} while (0)

/* Precludes the definition in utils.h
 *  - in normal operation, fatal errors abort program execution by calling exit().
 *  - in testcases however, fatal errors are considered testcase failures and jump
 *    to one of two places:
 *     - the next line of the testcase if the fatal error was expected
 *     - the end of the testcase if the fatal error was not expected */
#ifdef fatal
#error "test.h" should be the first include within testcase files
#else
#define _fatal(...) \
	do { \
		if (_assert_fatal_) { \
			longjmp(_tc_fatal_expected_, 1); \
		} else { \
			snprintf(_tc_errbuf_1, sizeof(_tc_errbuf_1), "%s:%d:%s:", __FILE__, __LINE__, __func__); \
			snprintf(_tc_errbuf_2, sizeof(_tc_errbuf_2), __VA_ARGS__); \
			longjmp(_tc_fatal_unexpected_, 1); \
		} \
	} while (0)
#define fatal        _fatal
#define fatal_noexit _fatal
#endif

#define run_tests(X) \
	_run_tests_(__FILE__, X, sizeof(X) / sizeof(X[0]))

#define test_fail(M) \
	do { \
		_print_testcase_name_(__func__); \
		printf("    %d: " M "\n", __LINE__); \
		_failures_++; \
	} while (0)

#define test_failf(M, ...) \
	do { \
		_print_testcase_name_(__func__); \
		printf("    %d: ", __LINE__); \
		printf((M), __VA_ARGS__); \
		printf("\n"); \
		_failures_++; \
	} while (0)

#define test_abort(M) \
	do { \
		_print_testcase_name_(__func__); \
		printf("    %d: " M "\n", __LINE__); \
		printf("    ---Testcase aborted---\n"); \
		_failures_++; \
		return; \
	} while (0)

#define test_abortf(M, ...) \
	do { \
		_print_testcase_name_(__func__); \
		printf("    %d: ", __LINE__); \
		printf((M), __VA_ARGS__); \
		printf("\n"); \
		printf("    ---Testcase aborted---\n"); \
		_failures_++; \
		return; \
	} while (0)

#define test_abort_main(M) \
	do { \
		printf("    %d: " M "\n", __LINE__); \
		printf("    ---Testcase aborted---\n"); \
		return EXIT_FAILURE; \
	} while (0)

struct testcase
{
	void (*tc_ptr)(void);
	const char *tc_str;
};

static int _assert_strcmp(const char*, const char*);
static int _assert_strncmp(const char*, const char*, size_t);
static void _print_testcase_name_(const char*);

static char _tc_errbuf_1[512];
static char _tc_errbuf_2[512];
static jmp_buf _tc_fatal_expected_;
static jmp_buf _tc_fatal_unexpected_;
static unsigned _assert_fatal_;
static unsigned _failure_printed_;
static unsigned _failures_;
static unsigned _failures_t_;

static int
_assert_strcmp(const char *p1, const char *p2)
{
	if (!p1 || !p2)
		return p1 != p2;

	return strcmp(p1, p2);
}

static int
_assert_strncmp(const char *p1, const char *p2, size_t n)
{
	if (!p1 || !p2)
		return p1 != p2;

	return strncmp(p1, p2, n);
}

static void
_print_testcase_name_(const char *name)
{
	/* Print the testcase name once */
	if (!_failure_printed_) {
		_failure_printed_ = 1;

		if (!_failures_t_)
			puts("");

		printf("  %s:\n", name);
	}
}

static int
_run_tests_(const char *filename, struct testcase testcases[], size_t len)
{
	/* Silence compiler warnings for unused test functions/vars */
	((void)(_assert_strcmp));
	((void)(_assert_strncmp));
	((void)(_assert_fatal_));
	((void)(_failure_printed_));
	((void)(_tc_fatal_expected_));
	((void)(_tc_fatal_unexpected_));

	printf("%s... ", filename);
	fflush(stdout);

	struct testcase *tc;

	for (tc = testcases; len--; tc++) {

		_failures_ = 0;
		_failure_printed_ = 0;

		if (setjmp(_tc_fatal_unexpected_)) {
			_print_testcase_name_(tc->tc_str);
			printf("    Unexpected fatal error:\n");
			printf("    %s %s\n", _tc_errbuf_1, _tc_errbuf_2);
			printf("    -- aborting testcase --\n");
			_failures_++;
		} else {
			(*tc->tc_ptr)();
		}

		if (_failures_) {
			printf("      %d failure%s\n", _failures_, (_failures_ > 1) ? "s" : "");
			_failures_t_ += _failures_;
		}
	}

	if (_failures_t_) {
		printf("  %d failure%s total\n", _failures_t_, (_failures_t_ > 1) ? "s" : "");
		return EXIT_FAILURE;
	} else {
		printf(" OK\n");
		return EXIT_SUCCESS;
	}
}

#endif
