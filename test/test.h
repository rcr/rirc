#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdlib.h>

typedef void (*testcase)(void);

static int _failures_, _failures_t_, _failure_printed_;

static int _assert_strcmp(char*, char*);

#define fail_test(M) \
	do { \
		if (!_failure_printed_) { \
			_failure_printed_ = 1; \
			printf("\n  %s:\n", __func__); \
		} \
		_failures_++; \
		printf("    %d: " M "\n", __LINE__); \
	} while (0)

#define fail_testf(...) \
	do { \
		if (!_failure_printed_) { \
			_failure_printed_ = 1; \
			printf("\n  %s:\n", __func__); \
		} \
		_failures_++; \
		printf("    %d: ", __LINE__); \
		printf(__VA_ARGS__); \
		printf("\n"); \
	} while (0)

#define assert_strcmp(X, Y) \
	do { \
		if (_assert_strcmp(X, Y)) \
			fail_testf(#X " expected '%s', got '%s'", (Y) == NULL ? "NULL" : (Y), (X) == NULL ? "NULL" : (X)); \
	} while (0)

#define assert_equals(X, Y) \
	do { \
		if ((X) != (Y)) \
			fail_testf(#X " expected '%d', got '%d'", (Y), (X)); \
	} while (0)

static int
_assert_strcmp(char *p1, char *p2)
{
	if (p1 == NULL || p2 == NULL)
		return p1 != p2;

	return strcmp(p1, p2);
}

static int
_run_tests_(const char *filename, testcase testcases[], size_t len)
{
	printf("%s... ", filename);
	fflush(stdout);

	testcase *tc;

	for (tc = testcases; len--; tc++) {
		_failures_ = 0;
		(*tc)();

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
