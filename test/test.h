#ifndef RIRC_TEST_H
#define RIRC_TEST_H

/* test.h -- unit test framework for rirc, output in TAP format
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
 */

#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TESTING

#define TESTCASE(X) { &(X) }

#define TOKEN_PASTE(x, y) x##y
#define TOKEN(x, y) TOKEN_PASTE(x, y)

#define assert_true(X) \
	do { \
		if (!(X)) \
			test_fail(#X " expected true"); \
		else \
			test_pass(); \
	} while (0)

#define assert_false(X) \
	do { \
		if ((X)) \
			test_fail(#X " expected false"); \
		else \
			test_pass(); \
	} while (0)

#define assert_eq(X, Y) \
	do { \
		int t__ret_x = (X); \
		int t__ret_y = (Y); \
		if (t__ret_x != t__ret_y) \
			test_failf(#X " expected '%d', got '%d'", t__ret_y, t__ret_x); \
		else \
			test_pass(); \
	} while (0)

#define assert_gt(X, Y) \
	do { \
		int t__ret_x = (X); \
		int t__ret_y = (Y); \
		if (!(t__ret_x > t__ret_y)) \
			test_failf(#X " expected '%d' to be greater than '%d'", t__ret_y, t__ret_x); \
		else \
			test_pass(); \
	} while (0)

#define assert_lt(X, Y) \
	do { \
		int t__ret_x = (X); \
		int t__ret_y = (Y); \
		if (!(t__ret_x < t__ret_y)) \
			test_failf(#X " expected '%d' to be less than '%d'", t__ret_y, t__ret_x); \
		else \
			test_pass(); \
	} while (0)

#define assert_ueq(X, Y) \
	do { \
		unsigned t__ret_x = (X); \
		unsigned t__ret_y = (Y); \
		if (t__ret_x != t__ret_y) \
			test_failf(#X " expected '%u', got '%u'", t__ret_y, t__ret_x); \
		else \
			test_pass(); \
	} while (0)

#define assert_strcmp(X, Y) \
	do { \
		const char *t__ret_x = (X); \
		const char *t__ret_y = (Y); \
		if (t__assert_strcmp_(t__ret_x, t__ret_y)) \
			test_failf(#X " expected '%s', got '%s'", \
				t__ret_y == NULL ? "NULL" : t__ret_y, \
				t__ret_x == NULL ? "NULL" : t__ret_x); \
		else \
			test_pass(); \
	} while (0)

#define assert_strncmp(X, Y, N) \
	do { \
		const char *t__ret_x = (X); \
		const char *t__ret_y = (Y); \
		if (t__assert_strncmp_(t__ret_x, t__ret_y, (N))) \
			test_failf(#X " expected '%.*s', got '%.*s'", \
				(int)(N), t__ret_y == NULL ? "NULL" : t__ret_y, \
				(int)(N), t__ret_x == NULL ? "NULL" : t__ret_x); \
		else \
			test_pass(); \
	} while (0)

#define assert_ptr_eq(X, Y) \
	do { \
		uintptr_t t__ret_x = (uintptr_t)(X); \
		uintptr_t t__ret_y = (uintptr_t)(Y); \
		if (t__ret_x != t__ret_y) \
			test_failf(#X " expected '%016" PRIxPTR "', got '%016" PRIxPTR "'", t__ret_y, t__ret_x); \
		else \
			test_pass(); \
	} while (0)

#define assert_ptr_null(X) \
	do { \
		uintptr_t t__ret_x = (uintptr_t)(X); \
		if (t__ret_x) \
			test_failf(#X " expected NULL, got '%016" PRIxPTR "'", t__ret_x); \
		else \
			test_pass(); \
	} while (0)

#define assert_ptr_not_null(X) \
	do { \
		uintptr_t t__ret_x = (uintptr_t)(X); \
		if (!t__ret_x) \
			test_fail(#X " expected not NULL"); \
		else \
			test_pass(); \
	} while (0)

#define assert_fatal(X) \
	do { \
		t__tc_assert_fatal_ = 1; \
		if (setjmp(t__tc_fatal_expected_)) { \
			t__tc_assert_fatal_ = 0; \
		} else { \
			(X); \
		} \
		if (t__tc_assert_fatal_) \
			test_fail("'" #X "' should have exited fatally"); \
		else \
			test_pass(); \
		t__tc_assert_fatal_ = 0; \
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
#define t__fatal(...) \
	do { \
		if (t__tc_assert_fatal_) { \
			longjmp(t__tc_fatal_expected_, 1); \
		} else { \
			snprintf(t__tc_errbuf_1_, sizeof(t__tc_errbuf_1_), "%s:%u:%s:", __FILE__, __LINE__, __func__); \
			snprintf(t__tc_errbuf_2_, sizeof(t__tc_errbuf_2_), __VA_ARGS__); \
			longjmp(t__tc_fatal_unexpected_, 1); \
		} \
	} while (0)
#define fatal        t__fatal
#define fatal_noexit t__fatal
#endif

#define test_pass() \
	do { \
		t__tc_n++; \
		printf("ok\n"); \
	} while (0)

#define test_fail(M) \
	do { \
		t__tc_n++; \
		printf("not ok - %s:%u: " M "\n", __func__, __LINE__); \
	} while (0)

#define test_failf(M, ...) \
	do { \
		t__tc_n++; \
		printf("not ok - %s:%u: " M "\n", __func__, __LINE__, __VA_ARGS__); \
	} while (0)

#define test_abort(M) \
	do { \
		printf("Bail out! %u: " M "\n", __LINE__); \
		return; \
	} while (0)

#define test_abortf(M, ...) \
	do { \
		printf("Bail out! %u: " M "\n", __LINE__, __VA_ARGS__); \
		return; \
	} while (0)

#define run_tests(INIT, TERM, TESTS) \
	t__run_tests_(INIT, TERM, TESTS, (sizeof(TESTS) / sizeof(TESTS[0])))

struct testcase
{
	void (*tc_ptr)(void);
};

static unsigned t__tc_n;

[[ maybe_unused ]] static char t__tc_errbuf_1_[512];
[[ maybe_unused ]] static char t__tc_errbuf_2_[512];
[[ maybe_unused ]] static jmp_buf t__tc_fatal_expected_;
[[ maybe_unused ]] static jmp_buf t__tc_fatal_unexpected_;
[[ maybe_unused ]] static unsigned t__tc_assert_fatal_;

[[ maybe_unused ]] static inline int
t__assert_strcmp_(const char *p1, const char *p2)
{
	if (!p1 || !p2)
		return p1 != p2;

	return strcmp(p1, p2);
}

[[ maybe_unused ]] static inline int
t__assert_strncmp_(const char *p1, const char *p2, size_t n)
{
	if (!p1 || !p2)
		return p1 != p2;

	return strncmp(p1, p2, n);
}

static int
t__run_tests_(
	int (*tc_init)(void),
	int (*tc_term)(void),
	const struct testcase testcases[],
	const size_t len)
{
	for (volatile size_t i = 0; i < len; i++) {

		if (setjmp(t__tc_fatal_unexpected_)) {
			printf("Bail out! Unexpected fatal: %s %s\n", t__tc_errbuf_1_, t__tc_errbuf_2_);
			break;
		}

		if (tc_init && (*tc_init)()) {
			printf("Bail out! Testcase init failed\n");
			break;
		}

		(*testcases[i].tc_ptr)();

		if (tc_term && (*tc_term)()) {
			printf("Bail out! Testcase term failed\n");
			break;
		}
	}

	printf("1..%u\n", t__tc_n);

	return EXIT_SUCCESS;
}

#endif
