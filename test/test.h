#ifndef TEST_H
#define TEST_H

#define TESTING

/* TODO: list the test types at the top of file with usage */

#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TESTCASE(X) { &(X), #X }

typedef struct testcase
{
	void (*tc_ptr)(void);
	const char *tc_str;
} testcase;

/* Fatal errors normally abort program execution by calling exit().
 * In the testcases however, fatal errors jump to one of two places:
 *   - the next line of the testcase if the fatal error was expected
 *   - the end of the testcase if the fatal error was not expected */
static jmp_buf _tc_fatal_expected_,
               _tc_fatal_unexpected_;

static int _assert_fatal_, _failures_, _failures_t_, _failure_printed_;
static char _tc_errbuf_[512];

static int _assert_strcmp(const char*, const char*);

static void _print_testcase_name_(const char*);

#define fail_test(M) \
	do { \
		_print_testcase_name_(__func__); \
		printf("    %d: " M "\n", __LINE__); \
		_failures_++; \
	} while (0)

#define fail_testf(...) \
	do { \
		_print_testcase_name_(__func__); \
		printf("    %d: ", __LINE__); \
		printf(__VA_ARGS__); \
		printf("\n"); \
		_failures_++; \
	} while (0)

#define abort_test(M) \
	do { \
		_print_testcase_name_(__func__); \
		printf("    %d: " M "\n", __LINE__); \
		printf("    ---Testcase aborted---\n"); \
		_failures_++; \
		return; \
	} while (0)

#define abort_testf(...) \
	do { \
		_print_testcase_name_(__func__); \
		printf("    %d: ", __LINE__); \
		printf(__VA_ARGS__ "\n"); \
		printf("    ---Testcase aborted---\n"); \
		_failures_++; \
		return; \
	} while (0)

/* Precludes the definition in utils.h
 *   in normal operation should fatally exit the program
 *   in testing should be considered a failure but should NOT continue running the testcase */
#ifdef fatal
	#error "test.h" should be the first include within testcase files
#else
	#define fatal(M, E) \
	do { \
		if (_assert_fatal_) { \
			longjmp(_tc_fatal_expected_, 1); \
		} else { \
			snprintf(_tc_errbuf_, sizeof(_tc_errbuf_) - 1, "FATAL in "__FILE__" - %s : '%s'", __func__, M); \
			longjmp(_tc_fatal_unexpected_, 1); \
		} \
	} while (0)
#endif

#define assert_fatal(X) \
	do { \
		if (setjmp(_tc_fatal_expected_)) { \
			_assert_fatal_ = 1; \
			(X); \
			if (_assert_fatal_) \
				fail_test("'"#X "' should have exited fatally"); \
			_assert_fatal_ = 0; \
		} \
	} while (0)

#define assert_strcmp(X, Y) \
	do { \
		if (_assert_strcmp(X, Y)) \
			fail_testf(#X " expected '%s', got '%s'", (Y) == NULL ? "NULL" : (Y), (X) == NULL ? "NULL" : (X)); \
	} while (0)

#define assert_lt(X, Y) \
	do { \
		if (!((X) < (Y))) \
			fail_testf(#X " expected '%d' to be less than '%d'", (X), (Y)); \
	} while (0)

#define assert_gt(X, Y) \
	do { \
		if (!((X) > (Y))) \
			fail_testf(#X " expected '%d' to be greater than '%d'", (X), (Y)); \
	} while (0)

#define assert_eq(X, Y) \
	do { \
		if ((X) != (Y)) \
			fail_testf(#X " expected '%d', got '%d'", (Y), (X)); \
	} while (0)

#define assert_true(X) \
	do { \
		if ((X) != 1) \
			fail_test(#X " expected true"); \
	} while (0)

#define assert_false(X) \
	do { \
		if ((X) != 0) \
			fail_test(#X " expected false"); \
	} while (0)

#define assert_ptrequals(X, Y) \
	do { \
		if ((X) != (Y)) \
			fail_testf(#X " expected '%016" PRIxPTR "', got '%016" PRIxPTR "'", (uintptr_t)(Y), (uintptr_t)(X)); \
	} while (0)

#define assert_null(X) \
	do { \
		if ((X) != NULL) \
			fail_testf(#X " expected NULL, got '%016" PRIxPTR "'", (uintptr_t)(X)); \
	} while (0)

static int
_assert_strcmp(const char *p1, const char *p2)
{
	if (p1 == NULL || p2 == NULL)
		return p1 != p2;

	return strcmp(p1, p2);
}

static void
_print_testcase_name_(const char *name)
{
	/* Print the testcase name only one */
	if (!_failure_printed_) {
		_failure_printed_ = 1;

		if (!_failures_t_)
			puts("");

		printf("  %s:\n", name);
	}
}

static int
_run_tests_(const char *filename, testcase testcases[], size_t len)
{
	/* Silence compiler warnings for test functions/vars that are included but not used */
	((void)(_assert_strcmp));
	((void)(_assert_fatal_));
	((void)(_failure_printed_));
	((void)(_tc_fatal_expected_));
	((void)(_tc_fatal_unexpected_));

	printf("%s... ", filename);
	fflush(stdout);

	testcase *tc;

	for (tc = testcases; len--; tc++) {
		_failures_ = 0;
		_failure_printed_ = 0;

		if (setjmp(_tc_fatal_unexpected_)) {
			_print_testcase_name_(tc->tc_str);
			printf("    %s - aborting testcase\n", _tc_errbuf_);
			_failures_++;
		} else
			(*tc->tc_ptr)();

		if (_failures_) {
			printf("      %d failure%c\n", _failures_, (_failures_ > 1) ? 's' : 0);
			_failures_t_ += _failures_;
		}
	}

	if (_failures_t_) {
		printf("  %d failure%c total\n", _failures_t_, (_failures_t_ > 1) ? 's' : 0);
		return EXIT_FAILURE;
	} else {
		printf(" OK\n");
		return EXIT_SUCCESS;
	}
}

/* Macro so the proper filename is printed */
#define run_tests(X) \
	_run_tests_(__FILE__, X, sizeof(X) / sizeof(X[0]))

#endif
