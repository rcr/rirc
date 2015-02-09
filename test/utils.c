#include <stdio.h>
#include <string.h>

#include "../src/utils.c"

int __assert_strcmp(char*, char*);

#define fail_test(M, ...) \
	do { \
		failures++; \
		printf("\t%s %d: " M "\n", __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define assert_strcmp(X, Y) \
	do { \
		if (__assert_strcmp(X, Y)) \
			fail_test(#X " expected '%s', got '%s'", (Y) ? (Y) : "NULL", (X) ? (X) : "NULL"); \
	} while (0)

int
__assert_strcmp(char *p1, char *p2)
{
	if (p1 == NULL || p2 == NULL)
		return p1 != p2;

	return strcmp(p1, p2);
}

int test_parse(void);

int
test_parse(void)
{
	/* Test the IRC message parsing function */

	int ret, failures = 0;

	parsed_mesg p;

	/* Test empty message */
	char mesg0[] = "";

	/* Should fail due to empty command */
	if ((ret = parse(&p, mesg0)) != 0)
		fail_test("parse() returned %d, expected 0", ret);
	assert_strcmp(p.from,     NULL);
	assert_strcmp(p.hostinfo, NULL);
	assert_strcmp(p.command,  NULL);
	assert_strcmp(p.params,   NULL);
	assert_strcmp(p.trailing, NULL);

	/* Test ordinary message */
	char mesg1[] = ":nick!user@hostname.domain CMD args :trailing";

	parse(&p, mesg1);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "args ");
	assert_strcmp(p.trailing, "trailing");

	/* Test no nick/host */
	char mesg2[] = "CMD arg1 arg2 :  trailing message  ";

	parse(&p, mesg2);
	assert_strcmp(p.from,     NULL);
	assert_strcmp(p.hostinfo, NULL);
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2 ");
	assert_strcmp(p.trailing, "  trailing message  ");

	/* Test the 15 arg limit */
	char mesg3[] = "CMD a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 :trailing message";

	parse(&p, mesg3);
	assert_strcmp(p.from,     NULL);
	assert_strcmp(p.hostinfo, NULL);
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 ");
	assert_strcmp(p.trailing, "a15 :trailing message");

	/* Test ':' can exist in args */
	char mesg4[] = ":nick!user@hostname.domain CMD arg:1:2:3 arg:4:5:6 :trailing message";

	parse(&p, mesg4);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg:1:2:3 arg:4:5:6 ");
	assert_strcmp(p.trailing, "trailing message");

	/* Test no args */
	char mesg5[] = ":nick!user@hostname.domain CMD :trailing message";

	parse(&p, mesg5);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   NULL);
	assert_strcmp(p.trailing, "trailing message");

	/* Test no trailing */
	char mesg6[] = ":nick!user@hostname.domain CMD arg1 arg2 arg3";

	parse(&p, mesg6);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2 arg3");
	assert_strcmp(p.trailing, NULL);

	/* Test no user */
	char mesg7[] = ":nick@hostname.domain CMD arg1 arg2 arg3";

	parse(&p, mesg7);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2 arg3");
	assert_strcmp(p.trailing, NULL);

	/* Test no command */
	char mesg8[] = ":nick!user@hostname.domain";

	/* Should fail due to empty command */
	if ((ret = parse(&p, mesg8)) != 0)
		fail_test("parse() returned %d, expected 0", ret);
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.hostinfo, "user@hostname.domain");
	assert_strcmp(p.command,  NULL);
	assert_strcmp(p.params,   NULL);
	assert_strcmp(p.trailing, NULL);

	if (failures)
		printf("\t%d failure%c\n", failures, (failures > 1) ? 's' : 0);

	return failures;
}


int
main(void)
{
	printf(__FILE__":\n");

	int failures = 0;

	failures += test_parse();

	if (failures) {
		printf("%d failure%c total\n\n", failures, (failures > 1) ? 's' : 0);
		exit(EXIT_FAILURE);
	}

	printf("OK\n\n");

	return EXIT_SUCCESS;
}
