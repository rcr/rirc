#include "test.h"
#include "../src/utils.c"

void
test_getarg(void)
{
	/* Test string token parsing */

	char *ptr;

	/* Test null pointer */
	assert_strcmp(getarg(NULL, " "), NULL);

	/* Test empty string */
	char str1[] = "";

	ptr = str1;
	assert_strcmp(getarg(&ptr, " "), NULL);

	/* Test only whitestapce */
	char str2[] = "   ";

	ptr = str2;
	assert_strcmp(getarg(&ptr, " "), NULL);

	/* Test single token */
	char str3[] = "arg1";

	ptr = str3;
	assert_strcmp(getarg(&ptr, " "), "arg1");
	assert_strcmp(getarg(&ptr, " "), NULL);

	/* Test multiple tokens */
	char str4[] = "arg2 arg3 arg4";

	ptr = str4;
	assert_strcmp(getarg(&ptr, " "), "arg2");
	assert_strcmp(getarg(&ptr, " "), "arg3");
	assert_strcmp(getarg(&ptr, " "), "arg4");
	assert_strcmp(getarg(&ptr, " "), NULL);

	/* Test multiple tokens with extraneous whitespace */
	char str5[] = "   arg5   arg6   arg7   ";

	ptr = str5;
	assert_strcmp(getarg(&ptr, " "), "arg5");
	assert_strcmp(getarg(&ptr, " "), "arg6");
	assert_strcmp(getarg(&ptr, " "), "arg7");
	assert_strcmp(getarg(&ptr, " "), NULL);

	/* Test multiple separator characters */
	char str6[] = "!!!arg8:!@#$arg9   :   arg10!@#";

	ptr = str6;
	assert_strcmp(getarg(&ptr, "!:"), "arg8");
	assert_strcmp(getarg(&ptr, ":$#@! "), "arg9");
	assert_strcmp(getarg(&ptr, " :"), "arg10!@#");
	assert_strcmp(getarg(&ptr, " "), NULL);
}

void
test_irc_toupper(void)
{
	/* Test rfc 2812 2.2 */

	char *p, str[] = "*az{}|^[]\\~*";

	for (p = str; *p; p++)
		*p = irc_toupper(*p);

	assert_strcmp(str, "*AZ[]\\~[]\\~*");
}

void
test_parse_mesg(void)
{
	/* Test the IRC message parsing function */

	struct parsed_mesg p;

	/* Test ordinary message */
	char mesg1[] = ":nick!user@hostname.domain CMD args :trailing";

	if (!parse_mesg(&p, mesg1))
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.host,     "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "args");
	assert_strcmp(p.trailing, "trailing");

	/* Test no nick/host */
	char mesg2[] = "CMD arg1 arg2 :  trailing message  ";

	if (!parse_mesg(&p, mesg2))
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     NULL);
	assert_strcmp(p.host,     NULL);
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2");
	assert_strcmp(p.trailing, "  trailing message  ");

	/* Test the 15 arg limit */
	char mesg3[] = "CMD a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 :trailing message";

	if (!parse_mesg(&p, mesg3))
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     NULL);
	assert_strcmp(p.host,     NULL);
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14");
	assert_strcmp(p.trailing, "a15 :trailing message");

	/* Test ':' can exist in args */
	char mesg4[] = ":nick!user@hostname.domain CMD arg:1:2:3 arg:4:5:6 :trailing message";

	if (!parse_mesg(&p, mesg4))
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.host,     "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg:1:2:3 arg:4:5:6");
	assert_strcmp(p.trailing, "trailing message");

	/* Test no args */
	char mesg5[] = ":nick!user@hostname.domain CMD :trailing message";

	if (!parse_mesg(&p, mesg5))
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.host,     "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   NULL);
	assert_strcmp(p.trailing, "trailing message");

	/* Test no trailing */
	char mesg6[] = ":nick!user@hostname.domain CMD arg1 arg2 arg3";

	if (!parse_mesg(&p, mesg6))
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.host,     "user@hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2 arg3");
	assert_strcmp(p.trailing, NULL);

	/* Test no user */
	char mesg7[] = ":nick@hostname.domain CMD arg1 arg2 arg3";

	if (!parse_mesg(&p, mesg7))
		fail_test("Failed to parse message");
	assert_strcmp(p.from,     "nick");
	assert_strcmp(p.host,     "hostname.domain");
	assert_strcmp(p.command,  "CMD");
	assert_strcmp(p.params,   "arg1 arg2 arg3");
	assert_strcmp(p.trailing, NULL);

	/* Error: empty message */
	char mesg8[] = "";

	if ((parse_mesg(&p, mesg8)) != 0)
		fail_test("parse_mesg() was expected to fail");

	/* Error: no command */
	char mesg9[] = ":nick!user@hostname.domain";

	if ((parse_mesg(&p, mesg9)) != 0)
		fail_test("parse_mesg() was expected to fail");

	/* Error no command, ensure original string wasn't altered */
	assert_strcmp(mesg9, ":nick!user@hostname.domain");
}

void
test_check_pinged(void)
{
	/* Test detecting user's nick in message */

	char *nick = "testnick";

	/* Test message contains username */
	char *mesg1 = "testing testnick testing";
	assert_equals(check_pinged(mesg1, nick), 1);

	/* Test common way of addressing messages to users */
	char *mesg2 = "testnick: testing";
	assert_equals(check_pinged(mesg2, nick), 1);

	/* Test non-nick char prefix */
	char *mesg3 = "testing !@#testnick testing";
	assert_equals(check_pinged(mesg3, nick), 1);

	/* Test non-nick char suffix */
	char *mesg4 = "testing testnick!@#$ testing";
	assert_equals(check_pinged(mesg4, nick), 1);

	/* Test non-nick char prefix and suffix */
	char *mesg5 = "testing !testnick! testing";
	assert_equals(check_pinged(mesg5, nick), 1);

	/* Error: message doesn't contain username */
	char *mesg6 = "testing testing";
	assert_equals(check_pinged(mesg6, nick), 0);

	/* Error: message contains username prefix */
	char *mesg7 = "testing testnickshouldfail testing";
	assert_equals(check_pinged(mesg7, nick), 0);
}

void
test_word_wrap(void)
{
	/*	_test_word_wrap(
	 *		<string>,
	 *		<substring>
	 *	)
	 *
	 *	Where substring represents the available width, i.e. n = strlen(substring)
	 * */

	char *ret, *seg1, *seg2, *end, str[256] = {0};

#define _test_word_wrap(S, SS)               \
	strncpy(str, (S), sizeof(str) - 1);      \
	end = str + strlen(str);                 \
	seg1 = seg2 = str;                       \
	ret = word_wrap(strlen(SS), &seg2, end); \
	*ret = 0;

	/* Test fits */
	_test_word_wrap(
		"test1 test2 test3",
		"test1 test2 test3"
	);
	assert_strcmp(seg1, "test1 test2 test3");
	assert_strcmp(seg2, "");

	_test_word_wrap(
		"test1 test2 test3",
		"test1 test2 test3xxxxx"
	);
	assert_strcmp(seg1, "test1 test2 test3");
	assert_strcmp(seg2, "");

	/* Test wrap on word */
	_test_word_wrap(
		"test1 test2 test3",
		"test1 t"
	);
	assert_strcmp(seg1, "test1");
	assert_strcmp(seg2, "test2 test3");

	_test_word_wrap(
		"test1 test2 test3",
		"test1 test"
	);
	assert_strcmp(seg1, "test1");
	assert_strcmp(seg2, "test2 test3");

	_test_word_wrap(
		"test1 test2 test3",
		"test1 test2"
	);
	assert_strcmp(seg1, "test1 test2");
	assert_strcmp(seg2, "test3");

	/* Test wrap on whitespace */
	_test_word_wrap(
		"test1 test2 test3",
		"test1 test2 "
	);
	assert_strcmp(seg1, "test1 test2");
	assert_strcmp(seg2, "test3");

	_test_word_wrap(
		"test1 test2   test3",
		"test1 test2"
	);
	assert_strcmp(seg1, "test1 test2");
	assert_strcmp(seg2, "test3");

	_test_word_wrap(
		"test1 test2   test3",
		"test1 test2 "
	);
	assert_strcmp(seg1, "test1 test2");
	assert_strcmp(seg2, "test3");

	_test_word_wrap(
		"test1 test2   test3",
		"test1 test2   "
	);
	assert_strcmp(seg1, "test1 test2");
	assert_strcmp(seg2, "test3");

	/* Test edge case: nowhere to wrap */
	_test_word_wrap(
		"test1test2 test3",
		"test1"
	);
	*ret = '!';
	assert_strcmp(seg1, "test1!est2 test3");
	assert_strcmp(seg2, "!est2 test3");

	/* Test edge case: whitespace prefix */
	_test_word_wrap(
		" testing",
		"   "
	);
	*ret = '!';
	assert_strcmp(seg1, " te!ting");
	assert_strcmp(seg2, "!ting");

	/* Test edge case: single space */
	_test_word_wrap(
		" ",
		"   "
	);
	assert_strcmp(seg1, " ");
	assert_strcmp(seg2, "");

	/* Test edge case: empty string*/
	_test_word_wrap(
		"",
		"   "
	);
	assert_strcmp(seg1, "");
	assert_strcmp(seg2, "");

	if (seg1 != seg2 || seg2 != end)
		fail_test("seg1 should be advanced to end of string");
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_check_pinged),
		TESTCASE(test_getarg),
		TESTCASE(test_irc_toupper),
		TESTCASE(test_parse_mesg),
		TESTCASE(test_word_wrap)
	};

	return run_tests(tests);
}
