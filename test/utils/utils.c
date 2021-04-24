#include "test/test.h"
#include "src/utils/utils.c"

static void
test_irc_ischan(void)
{
	/* TODO */
}

static void
test_irc_isnick(void)
{
	/* TODO */
}

static void
test_irc_message_param(void)
{
	char *param;
	struct irc_message m;

#define CHECK_IRC_MESSAGE_PARAM(R, S) \
	assert_eq(irc_message_param(&m, &param), (R)); \
	assert_strcmp(param, (S));

#define CHECK_IRC_MESSAGE_PARSE(M, R) \
	assert_eq(irc_message_parse(&m, (M)), (R));

	/* Test ordinary args */
	char mesg1[] = ":nick!user@host.domain.tld CMD arg1 arg2 arg3 :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg1, 0);
	CHECK_IRC_MESSAGE_PARAM(1, "arg1");
	CHECK_IRC_MESSAGE_PARAM(1, "arg2");
	CHECK_IRC_MESSAGE_PARAM(1, "arg3");
	CHECK_IRC_MESSAGE_PARAM(1, "trailing arg");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test no trailing arg */
	char mesg2[] = ":nick!user@host.domain.tld CMD arg1 arg2 arg3";

	CHECK_IRC_MESSAGE_PARSE(mesg2, 0);
	CHECK_IRC_MESSAGE_PARAM(1, "arg1");
	CHECK_IRC_MESSAGE_PARAM(1, "arg2");
	CHECK_IRC_MESSAGE_PARAM(1, "arg3");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test only trailing arg */
	char mesg3[] = ":nick!user@host.domain.tld CMD :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg3, 0);
	CHECK_IRC_MESSAGE_PARAM(1, "trailing arg");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test ':' can exist in args */
	char mesg4[] = ":nick!user@host.domain.tld CMD arg:1:2:3 arg:4:5:6 :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg4, 0);
	CHECK_IRC_MESSAGE_PARAM(1, "arg:1:2:3");
	CHECK_IRC_MESSAGE_PARAM(1, "arg:4:5:6");
	CHECK_IRC_MESSAGE_PARAM(1, "trailing arg");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test 15 arg limit */
	char mesg5[] = "CMD a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg5, 0);
	CHECK_IRC_MESSAGE_PARAM(1, "a1");
	CHECK_IRC_MESSAGE_PARAM(1, "a2");
	CHECK_IRC_MESSAGE_PARAM(1, "a3");
	CHECK_IRC_MESSAGE_PARAM(1, "a4");
	CHECK_IRC_MESSAGE_PARAM(1, "a5");
	CHECK_IRC_MESSAGE_PARAM(1, "a6");
	CHECK_IRC_MESSAGE_PARAM(1, "a7");
	CHECK_IRC_MESSAGE_PARAM(1, "a8");
	CHECK_IRC_MESSAGE_PARAM(1, "a9");
	CHECK_IRC_MESSAGE_PARAM(1, "a10");
	CHECK_IRC_MESSAGE_PARAM(1, "a11");
	CHECK_IRC_MESSAGE_PARAM(1, "a12");
	CHECK_IRC_MESSAGE_PARAM(1, "a13");
	CHECK_IRC_MESSAGE_PARAM(1, "a14");
	CHECK_IRC_MESSAGE_PARAM(1, "a15 :trailing arg");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test no args */
	char mesg6[] = ":nick!user@host.domain.tld CMD";

	CHECK_IRC_MESSAGE_PARSE(mesg6, 0);
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

#undef CHECK_IRC_MESSAGE_PARAM
#undef CHECK_IRC_MESSAGE_PARSE
}

static void
test_irc_message_parse(void)
{
	struct irc_message m;

#define CHECK_IRC_MESSAGE_PARSE(M, R) \
	assert_eq(irc_message_parse(&m, (M)), (R));

	/* Test ordinary message */
	char mesg1[] = ":nick!user@host.domain.tld CMD args :trailing";

	CHECK_IRC_MESSAGE_PARSE(mesg1, 0);
	assert_strcmp(m.command, "CMD");
	assert_strcmp(m.from,    "nick");
	assert_strcmp(m.host,    "user@host.domain.tld");
	assert_strcmp(m.params,  "args :trailing");
	assert_ueq(m.len_command, 3);
	assert_ueq(m.len_from,    4);
	assert_ueq(m.len_host,    20);

	/* Test no nick/host */
	char mesg2[] = "CMD arg1 arg2 :  trailing message  ";

	CHECK_IRC_MESSAGE_PARSE(mesg2, 0);
	assert_strcmp(m.command, "CMD");
	assert_strcmp(m.from,    NULL);
	assert_strcmp(m.host,    NULL);
	assert_strcmp(m.params,  "arg1 arg2 :  trailing message  ");
	assert_ueq(m.len_command, 3);
	assert_ueq(m.len_from,    0);
	assert_ueq(m.len_host,    0);

	/* Test no user */
	char mesg3[] = ":nick@host.domain.tld CMD arg1 arg2 arg3";

	CHECK_IRC_MESSAGE_PARSE(mesg3, 0);
	assert_strcmp(m.command, "CMD");
	assert_strcmp(m.from,    "nick");
	assert_strcmp(m.host,    "host.domain.tld");
	assert_strcmp(m.params,  "arg1 arg2 arg3");
	assert_ueq(m.len_command, 3);
	assert_ueq(m.len_from,    4);
	assert_ueq(m.len_host,    15);

	/* Test no host */
	char mesg4[] = ":nick CMD arg1 arg2 arg3";

	CHECK_IRC_MESSAGE_PARSE(mesg4, 0);
	assert_strcmp(m.command, "CMD");
	assert_strcmp(m.from,    "nick");
	assert_strcmp(m.host,    NULL);
	assert_strcmp(m.params,  "arg1 arg2 arg3");
	assert_ueq(m.len_command, 3);
	assert_ueq(m.len_from,    4);
	assert_ueq(m.len_host,    0);

	/* Test no args */
	char mesg5[] = ":nick!user@host.domain.tld CMD";

	CHECK_IRC_MESSAGE_PARSE(mesg5, 0);
	assert_strcmp(m.command, "CMD");
	assert_strcmp(m.from,    "nick");
	assert_strcmp(m.host,    "user@host.domain.tld");
	assert_strcmp(m.params,  NULL);
	assert_ueq(m.len_command, 3);
	assert_ueq(m.len_from,    4);
	assert_ueq(m.len_host,    20);

	/* Test extraneous space */
	char mesg6[] = "  :nick!user@host.domain.tld   CMD   arg1  arg2  arg3   : trailing";

	CHECK_IRC_MESSAGE_PARSE(mesg6, 0);
	assert_strcmp(m.command, "CMD");
	assert_strcmp(m.from,    "nick");
	assert_strcmp(m.host,    "user@host.domain.tld");
	assert_strcmp(m.params,  "arg1  arg2  arg3   : trailing");
	assert_ueq(m.len_command, 3);
	assert_ueq(m.len_from,    4);
	assert_ueq(m.len_host,    20);

	/* Error: empty message */
	char mesg7[] = "";
	CHECK_IRC_MESSAGE_PARSE(mesg7, -1);

	/* Error: no command */
	char mesg8[] = ":nick!user@host.domain.tld";
	CHECK_IRC_MESSAGE_PARSE(mesg8, -1);

	/* Error: malformed name/host */
	char mesg9[] = ": CMD arg1 arg2 arg3";
	CHECK_IRC_MESSAGE_PARSE(mesg9, -1);

#undef CHECK_IRC_MESSAGE_PARSE
}

static void
test_irc_message_split(void)
{
	char *param;
	const char *params;
	const char *trailing;
	struct irc_message m;

#define CHECK_IRC_MESSAGE_PARAM(R, S) \
	assert_eq(irc_message_param(&m, &param), (R)); \
	assert_strcmp(param, (S));

#define CHECK_IRC_MESSAGE_PARSE(M, R) \
	assert_eq(irc_message_parse(&m, (M)), (R));

#define CHECK_IRC_MESSAGE_SPLIT(R, P, T) \
	assert_eq(irc_message_split(&m, &params, &trailing), (R)); \
	assert_strcmp(params,   (P)); \
	assert_strcmp(trailing, (T));

	/* Test empty params */
	char mesg1[] = "CMD";

	CHECK_IRC_MESSAGE_PARSE(mesg1, 0);
	CHECK_IRC_MESSAGE_SPLIT(0, NULL, NULL);

	/* Test ordinary args */
	char mesg2[] = "CMD a1 a2 a3 :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg2, 0);
	CHECK_IRC_MESSAGE_SPLIT(1, "a1 a2 a3", "trailing arg");
	CHECK_IRC_MESSAGE_PARAM(1, "a1");
	CHECK_IRC_MESSAGE_PARAM(1, "a2");
	CHECK_IRC_MESSAGE_PARAM(1, "a3");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test no trailing arg */
	char mesg3[] = "CMD a1 a2 a3";

	CHECK_IRC_MESSAGE_PARSE(mesg3, 0);
	CHECK_IRC_MESSAGE_SPLIT(0, "a1 a2 a3", NULL);
	CHECK_IRC_MESSAGE_PARAM(1, "a1");
	CHECK_IRC_MESSAGE_PARAM(1, "a2");
	CHECK_IRC_MESSAGE_PARAM(1, "a3");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test only trailing arg */
	char mesg4[] = "CMD :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg4, 0);
	CHECK_IRC_MESSAGE_SPLIT(1, NULL, "trailing arg");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test ':' can exist in args */
	char mesg5[] = "CMD arg:1:2:3 arg:4:5:6 :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg5, 0);
	CHECK_IRC_MESSAGE_SPLIT(1, "arg:1:2:3 arg:4:5:6", "trailing arg");
	CHECK_IRC_MESSAGE_PARAM(1, "arg:1:2:3");
	CHECK_IRC_MESSAGE_PARAM(1, "arg:4:5:6");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test 15 arg limit */
	char mesg6[] = "CMD a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg6, 0);
	CHECK_IRC_MESSAGE_SPLIT(
		1, "a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14", "a15 :trailing arg");
	CHECK_IRC_MESSAGE_PARAM(1, "a1");
	CHECK_IRC_MESSAGE_PARAM(1, "a2");
	CHECK_IRC_MESSAGE_PARAM(1, "a3");
	CHECK_IRC_MESSAGE_PARAM(1, "a4");
	CHECK_IRC_MESSAGE_PARAM(1, "a5");
	CHECK_IRC_MESSAGE_PARAM(1, "a6");
	CHECK_IRC_MESSAGE_PARAM(1, "a7");
	CHECK_IRC_MESSAGE_PARAM(1, "a8");
	CHECK_IRC_MESSAGE_PARAM(1, "a9");
	CHECK_IRC_MESSAGE_PARAM(1, "a10");
	CHECK_IRC_MESSAGE_PARAM(1, "a11");
	CHECK_IRC_MESSAGE_PARAM(1, "a12");
	CHECK_IRC_MESSAGE_PARAM(1, "a13");
	CHECK_IRC_MESSAGE_PARAM(1, "a14");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test 15 arg limit - 1 previously parsed */
	char mesg7[] = "CMD a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg7, 0);
	CHECK_IRC_MESSAGE_PARAM(1, "a1");
	CHECK_IRC_MESSAGE_SPLIT(
		1, "a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14", "a15 :trailing arg");
	CHECK_IRC_MESSAGE_PARAM(1, "a2");
	CHECK_IRC_MESSAGE_PARAM(1, "a3");
	CHECK_IRC_MESSAGE_PARAM(1, "a4");
	CHECK_IRC_MESSAGE_PARAM(1, "a5");
	CHECK_IRC_MESSAGE_PARAM(1, "a6");
	CHECK_IRC_MESSAGE_PARAM(1, "a7");
	CHECK_IRC_MESSAGE_PARAM(1, "a8");
	CHECK_IRC_MESSAGE_PARAM(1, "a9");
	CHECK_IRC_MESSAGE_PARAM(1, "a10");
	CHECK_IRC_MESSAGE_PARAM(1, "a11");
	CHECK_IRC_MESSAGE_PARAM(1, "a12");
	CHECK_IRC_MESSAGE_PARAM(1, "a13");
	CHECK_IRC_MESSAGE_PARAM(1, "a14");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test 15 arg limit - 14 previously parsed */
	char mesg8[] = "CMD a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg8, 0);
	CHECK_IRC_MESSAGE_PARAM(1, "a1");
	CHECK_IRC_MESSAGE_PARAM(1, "a2");
	CHECK_IRC_MESSAGE_PARAM(1, "a3");
	CHECK_IRC_MESSAGE_PARAM(1, "a4");
	CHECK_IRC_MESSAGE_PARAM(1, "a5");
	CHECK_IRC_MESSAGE_PARAM(1, "a6");
	CHECK_IRC_MESSAGE_PARAM(1, "a7");
	CHECK_IRC_MESSAGE_PARAM(1, "a8");
	CHECK_IRC_MESSAGE_PARAM(1, "a9");
	CHECK_IRC_MESSAGE_PARAM(1, "a10");
	CHECK_IRC_MESSAGE_PARAM(1, "a11");
	CHECK_IRC_MESSAGE_PARAM(1, "a12");
	CHECK_IRC_MESSAGE_PARAM(1, "a13");
	CHECK_IRC_MESSAGE_PARAM(1, "a14");
	CHECK_IRC_MESSAGE_SPLIT(1, NULL, "a15 :trailing arg");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test 15 arg limit - all previously parsed */
	char mesg9[] = "CMD a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg9, 0);
	CHECK_IRC_MESSAGE_PARAM(1, "a1");
	CHECK_IRC_MESSAGE_PARAM(1, "a2");
	CHECK_IRC_MESSAGE_PARAM(1, "a3");
	CHECK_IRC_MESSAGE_PARAM(1, "a4");
	CHECK_IRC_MESSAGE_PARAM(1, "a5");
	CHECK_IRC_MESSAGE_PARAM(1, "a6");
	CHECK_IRC_MESSAGE_PARAM(1, "a7");
	CHECK_IRC_MESSAGE_PARAM(1, "a8");
	CHECK_IRC_MESSAGE_PARAM(1, "a9");
	CHECK_IRC_MESSAGE_PARAM(1, "a10");
	CHECK_IRC_MESSAGE_PARAM(1, "a11");
	CHECK_IRC_MESSAGE_PARAM(1, "a12");
	CHECK_IRC_MESSAGE_PARAM(1, "a13");
	CHECK_IRC_MESSAGE_PARAM(1, "a14");
	CHECK_IRC_MESSAGE_PARAM(1, "a15 :trailing arg");
	CHECK_IRC_MESSAGE_SPLIT(0, NULL, NULL);
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

	/* Test trimming args - 14 previously parsed */
	char mesg10[] = "CMD a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14    a15 :trailing arg";

	CHECK_IRC_MESSAGE_PARSE(mesg10, 0);
	CHECK_IRC_MESSAGE_PARAM(1, "a1");
	CHECK_IRC_MESSAGE_PARAM(1, "a2");
	CHECK_IRC_MESSAGE_PARAM(1, "a3");
	CHECK_IRC_MESSAGE_PARAM(1, "a4");
	CHECK_IRC_MESSAGE_PARAM(1, "a5");
	CHECK_IRC_MESSAGE_PARAM(1, "a6");
	CHECK_IRC_MESSAGE_PARAM(1, "a7");
	CHECK_IRC_MESSAGE_PARAM(1, "a8");
	CHECK_IRC_MESSAGE_PARAM(1, "a9");
	CHECK_IRC_MESSAGE_PARAM(1, "a10");
	CHECK_IRC_MESSAGE_PARAM(1, "a11");
	CHECK_IRC_MESSAGE_PARAM(1, "a12");
	CHECK_IRC_MESSAGE_PARAM(1, "a13");
	CHECK_IRC_MESSAGE_PARAM(1, "a14");
	CHECK_IRC_MESSAGE_SPLIT(1, NULL, "a15 :trailing arg");
	CHECK_IRC_MESSAGE_PARAM(0, NULL);

#undef CHECK_IRC_MESSAGE_PARAM
#undef CHECK_IRC_MESSAGE_PARSE
#undef CHECK_IRC_MESSAGE_SPLIT
}

static void
test_irc_pinged(void)
{
	/* Test detecting user's nick in message */

	const char *nick;

#define CHECK_IRC_PINGED(M, R) \
	assert_eq(irc_pinged(CASEMAPPING_RFC1459, (M), nick), (R));

	nick = "nick";

	CHECK_IRC_PINGED("nick", 1);
	CHECK_IRC_PINGED("nick ", 1);
	CHECK_IRC_PINGED("nick:", 1);
	CHECK_IRC_PINGED("nick: ", 1);
	CHECK_IRC_PINGED(" nick", 1);
	CHECK_IRC_PINGED(" nick ", 1);
	CHECK_IRC_PINGED(" nick:", 1);
	CHECK_IRC_PINGED(" nick: ", 1);
	CHECK_IRC_PINGED("xxx 'nick'! ", 1);
	CHECK_IRC_PINGED("xxx @nick?! xxx", 1);
	CHECK_IRC_PINGED("xxx @NICK?! xxx", 1);

	CHECK_IRC_PINGED("", 0);
	CHECK_IRC_PINGED(" ", 0);
	CHECK_IRC_PINGED("xxx", 0);
	CHECK_IRC_PINGED("xnick", 0);
	CHECK_IRC_PINGED("xnick:", 0);
	CHECK_IRC_PINGED("xnick: ", 0);
	CHECK_IRC_PINGED(" xnick", 0);
	CHECK_IRC_PINGED(" xnick:", 0);

	/* Test server assigns a non standard nick */
	nick = "000nick";

	CHECK_IRC_PINGED("000nick", 1);
	CHECK_IRC_PINGED("000nick ", 1);
	CHECK_IRC_PINGED("000nick:", 1);
	CHECK_IRC_PINGED("000nick: ", 1);
	CHECK_IRC_PINGED(" 000nick", 1);
	CHECK_IRC_PINGED(" 000nick ", 1);
	CHECK_IRC_PINGED(" 000nick:", 1);
	CHECK_IRC_PINGED(" 000nick: ", 1);
	CHECK_IRC_PINGED("xxx '000nick'! ", 1);
	CHECK_IRC_PINGED("xxx @000nick?! xxx", 1);
	CHECK_IRC_PINGED("xxx @000NICK?! xxx", 1);

	CHECK_IRC_PINGED("x000nick", 0);
	CHECK_IRC_PINGED("x000nick:", 0);
	CHECK_IRC_PINGED("x000nick: ", 0);
	CHECK_IRC_PINGED(" x000nick", 0);
	CHECK_IRC_PINGED(" x000nick:", 0);

#undef CHECK_IRC_PINGED
}

static void
test_irc_strcmp(void)
{
	/* Test lexicographic order
	 *
	 * The character '`' is permitted along with '{', but are disjoint
	 * in ascii, with lowercase letters between them. Ensure that in
	 * lexicographic order, irc_strmp ranks:
	 *  numeric > alpha > special */

	assert_gt(irc_strcmp(CASEMAPPING_RFC1459, "0", "a"), 0);
	assert_gt(irc_strcmp(CASEMAPPING_RFC1459, "a", "`"), 0);
	assert_gt(irc_strcmp(CASEMAPPING_RFC1459, "a", "{"), 0);
	assert_gt(irc_strcmp(CASEMAPPING_RFC1459, "z", "{"), 0);
	assert_gt(irc_strcmp(CASEMAPPING_RFC1459, "Z", "`"), 0);
	assert_gt(irc_strcmp(CASEMAPPING_RFC1459, "a", "Z"), 0);
	assert_gt(irc_strcmp(CASEMAPPING_RFC1459, "A", "z"), 0);

	/* Test case insensitive */

	/* Passes for CASEMAPPING_RFC1459 */
	assert_eq(irc_strcmp(CASEMAPPING_RFC1459, "abc123[]\\~`_", "ABC123{}|^`_"), 0);
	assert_lt(irc_strcmp(CASEMAPPING_STRICT_RFC1459, "abc123[]\\~`_", "ABC123{}|^`_"), 0);
	assert_gt(irc_strcmp(CASEMAPPING_ASCII, "abc123[]\\~`_", "ABC123{}|^`_"), 0);

	/* Passes for CASEMAPPING_RFC1459, CASEMAPPING_STRICT_RFC1459 */
	assert_eq(irc_strcmp(CASEMAPPING_RFC1459, "abc123[]\\`_", "ABC123{}|`_"), 0);
	assert_eq(irc_strcmp(CASEMAPPING_STRICT_RFC1459, "abc123[]\\`_", "ABC123{}|`_"), 0);
	assert_gt(irc_strcmp(CASEMAPPING_ASCII, "abc123[]\\`_", "ABC123{}|`_"), 0);

	/* Passes for CASEMAPPING_RFC1459, CASEMAPPING_STRICT_RFC1459, CASEMAPPING_ASCII */
	assert_eq(irc_strcmp(CASEMAPPING_RFC1459, "abc123", "ABC123"), 0);
	assert_eq(irc_strcmp(CASEMAPPING_STRICT_RFC1459, "abc123", "ABC123"), 0);
	assert_eq(irc_strcmp(CASEMAPPING_ASCII, "abc123", "ABC123"), 0);
}

static void
test_irc_strncmp(void)
{
	/* Test case insensitive */
	assert_eq(irc_strncmp(CASEMAPPING_RFC1459, "abc123[]\\~`_", "ABC123{}|^`_", 100), 0);

	/* Test n */
	assert_eq(irc_strncmp(CASEMAPPING_RFC1459, "abcA", "abcZ", 3), 0);
	assert_gt(irc_strncmp(CASEMAPPING_RFC1459, "abcA", "abcZ", 4), 0);
}

static void
test_irc_toupper(void)
{
	/* Test rfc 2812 2.2 */

	char *p, str[] = "*az{}|^[]\\~*";

	for (p = str; *p; p++)
		*p = irc_toupper(CASEMAPPING_RFC1459, *p);

	assert_strcmp(str, "*AZ[]\\~[]\\~*");
}

static void
test_irc_strsep(void)
{
	char *p;

	char mesg1[] = "";
	char mesg2[] = " ";
	char mesg3[] = "  ";

	assert_ptr_null(irc_strsep(NULL));

	p = mesg1;
	assert_ptr_null(irc_strsep(&p));

	p = mesg2;
	assert_ptr_null(irc_strsep(&p));

	p = mesg3;
	assert_ptr_null(irc_strsep(&p));

	char mesg4[] = "test1";
	p = mesg4;
	assert_strcmp(irc_strsep(&p), "test1");
	assert_ptr_null(irc_strsep(&p));

	char mesg5[] = "test2 test3";
	p = mesg5;
	assert_strcmp(irc_strsep(&p), "test2");
	assert_strcmp(irc_strsep(&p), "test3");
	assert_ptr_null(irc_strsep(&p));

	char mesg6[] = "  test4   test5  test6   ";
	p = mesg6;
	assert_strcmp(irc_strsep(&p), "test4");
	assert_strcmp(irc_strsep(&p), "test5");
	assert_strcmp(irc_strsep(&p), "test6");
	assert_ptr_null(irc_strsep(&p));
}

static void
test_irc_strtrim(void)
{
	/* Test skipping space at the begging of a string pointer
	 * and returning 0 when no non-space character is found */

	char *mesg1 = "testing";
	assert_ptr_eq(irc_strtrim(&mesg1), mesg1);
	assert_strcmp(mesg1, "testing");

	char *mesg2 = " testing ";
	assert_ptr_eq(irc_strtrim(&mesg2), mesg2);
	assert_strcmp(mesg2, "testing ");

	char *mesg3 = "";
	assert_ptr_null(irc_strtrim(&mesg3));
	assert_strcmp(mesg3, "");

	char *mesg4 = " ";
	assert_ptr_null(irc_strtrim(&mesg4));
	assert_strcmp(mesg4, "");
}

static void
test_irc_strwrap(void)
{
	char *ret, *seg1, *seg2, *end, str[256] = {0};

#define CHECK_IRC_STRWRAP(S, SS, L1, L2)  \
	strncpy(str, (S), sizeof(str) - 1); \
	end = str + strlen(str); \
	seg1 = seg2 = str; \
	ret = irc_strwrap(strlen(SS), &seg2, end); \
	*ret = 0; \
	assert_strcmp(seg1, (L1)); \
	assert_strcmp(seg2, (L2));

	/* Test fits */
	CHECK_IRC_STRWRAP(
		"test1 test2 test3",
		"test1 test2 test3",
		"test1 test2 test3",
		"");

	CHECK_IRC_STRWRAP(
		"test1 test2 test3",
		"test1 test2 test3xxxxx",
		"test1 test2 test3",
		"");

	/* Test wrap on word */
	CHECK_IRC_STRWRAP(
		"test1 test2 test3",
		"test1 t",
		"test1",
		"test2 test3");

	CHECK_IRC_STRWRAP(
		"test1 test2 test3",
		"test1 test",
		"test1",
		"test2 test3");

	CHECK_IRC_STRWRAP(
		"test1 test2 test3",
		"test1 test2",
		"test1 test2",
		"test3");

	/* Test wrap on whitespace */
	CHECK_IRC_STRWRAP(
		"test1 test2 test3",
		"test1 test2 ",
		"test1 test2",
		"test3");

	CHECK_IRC_STRWRAP(
		"test1 test2   test3",
		"test1 test2",
		"test1 test2",
		"test3");

	CHECK_IRC_STRWRAP(
		"test1 test2   test3",
		"test1 test2 ",
		"test1 test2",
		"test3");

	CHECK_IRC_STRWRAP(
		"test1 test2   test3",
		"test1 test2   ",
		"test1 test2",
		"test3");

	/* Test edge case: single space */
	CHECK_IRC_STRWRAP(
		" ",
		"   ",
		" ",
		"");

	/* Test edge case: empty string*/
	CHECK_IRC_STRWRAP(
		"",
		"   ",
		"",
		"");

#undef CHECK_IRC_STRWRAP

	if (seg1 != seg2 || seg2 != end)
		test_fail("seg1 should be advanced to end of string");

	/* Test edge case: nowhere to wrap */
	char m1[] = "test1test2 test3";
	char m2[] = "test1";

	end = m1 + strlen(m1);
	seg1 = seg2 = m1;
	ret = irc_strwrap(strlen(m2), &seg2, end);
	*ret = '!';
	assert_strcmp(seg1, "test1!est2 test3");
	assert_strcmp(seg2, "!est2 test3");

	/* Test edge case: whitespace prefix */
	char m3[] = " testing";
	char m4[] = "   ";

	end = m3 + strlen(m3);
	seg1 = seg2 = m3;
	ret = irc_strwrap(strlen(m4), &seg2, end);
	*ret = '!';
	assert_strcmp(seg1, " te!ting");
	assert_strcmp(seg2, "!ting");
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_irc_ischan),
		TESTCASE(test_irc_isnick),
		TESTCASE(test_irc_message_param),
		TESTCASE(test_irc_message_parse),
		TESTCASE(test_irc_message_split),
		TESTCASE(test_irc_pinged),
		TESTCASE(test_irc_strcmp),
		TESTCASE(test_irc_strncmp),
		TESTCASE(test_irc_strsep),
		TESTCASE(test_irc_strtrim),
		TESTCASE(test_irc_strwrap),
		TESTCASE(test_irc_toupper)
	};

	return run_tests(tests);
}
