#include "test.h"

#include "../src/mode.c"

#define ALL_LOWERS "abcdefghijklmnopqrstuvwxyz"
#define ALL_UPPERS "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

static void
test_flag_bit(void)
{
	/* Test mode flag -> mode bit */

	assert_eq(flag_bit('a' - 1), 0);
	assert_eq(flag_bit('a'),     1 << 0);
	assert_eq(flag_bit('z'),     1 << 25);
	assert_eq(flag_bit('z' + 1), 0);

	assert_eq(flag_bit('A' - 1), 0);
	assert_eq(flag_bit('A'),     1 << 0);
	assert_eq(flag_bit('Z'),     1 << 25);
	assert_eq(flag_bit('Z' + 1), 0);
}

static void
test_mode_str(void)
{
	/* Test setting mode string */

	struct mode m = MODE_EMPTY;
	struct mode_str m_str = {0};

	/* mode_str type not set */
	assert_fatal(mode_str(&m, &m_str));

	m_str.type = MODE_STR_T_SIZE;

	/* mode_str type unknown */
	assert_fatal(mode_str(&m, &m_str));

	m_str.type = MODE_STR_USERMODE;

	/* Test no mode */
	assert_strcmp(mode_str(&m, &m_str), "");

	m.lower = UINT32_MAX;
	m.upper = 0;

	assert_strcmp(mode_str(&m, &m_str), ALL_LOWERS);

	m.lower = 0;
	m.upper = UINT32_MAX;

	assert_strcmp(mode_str(&m, &m_str), ALL_UPPERS);

	m.lower = UINT32_MAX;
	m.upper = UINT32_MAX;

	assert_strcmp(mode_str(&m, &m_str), ALL_LOWERS ALL_UPPERS);
}

static void
test_chanmode_set(void)
{
	/* Test setting/unsetting chanmode flag, prefix and mode string */

	struct mode m = MODE_EMPTY;
	struct mode_config c;
	struct mode_str m_str = { .type = MODE_STR_CHANMODE };

	mode_config_chanmodes(&c, "abcdefghijsp");
	mode_config_subtypes(&c, "abc,def,ghi,jsp");

#define CHECK(STR, PRFX) \
	assert_strcmp(mode_str(&m, &m_str), (STR)); assert_eq(m.prefix, (PRFX));

	/* Test setting/unsetting invalid chanmode flag */
	assert_eq(mode_chanmode_set(&m, &c, 'z', MODE_SET_ON), MODE_ERR_INVALID_FLAG);
	CHECK("", 0);
	assert_eq(mode_chanmode_set(&m, &c, 'z', MODE_SET_OFF), MODE_ERR_INVALID_FLAG);
	CHECK("", 0);

	/* Test valid CHANMODE subtype A doesn't set flag */
	assert_eq(mode_chanmode_set(&m, &c, 'a', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("", 0);

	/* Test valid CHANMODE subtypes B,C,D set flags */
	assert_eq(mode_chanmode_set(&m, &c, 'd', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("d", 0);

	assert_eq(mode_chanmode_set(&m, &c, 'e', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("de", 0);

	assert_eq(mode_chanmode_set(&m, &c, 'j', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("dej", 0);

	/* Test 's'/'p' chanmodes set prefix but do not appear in mode string
	 *
	 * Ensure 's' always supercedes and unsets 'p',
	 * Ensure 'p' is silently ignored when 's' is set
	 * Ensure unsetting the current 's'/'p' flag always sets default prefix */

	assert_eq(mode_chanmode_set(&m, &c, 'p', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("dej", MODE_CHANMODE_PREFIX_PRIVATE);
	assert_true(mode_isset(&m, 'p'));
	assert_false(mode_isset(&m, 's'));

	/* Unsetting 'p' sets default */
	assert_eq(mode_chanmode_set(&m, &c, 'p', MODE_SET_OFF), MODE_ERR_NONE);
	CHECK("dej", MODE_CHANMODE_PREFIX_OTHER);
	assert_false(mode_isset(&m, 'p'));
	assert_false(mode_isset(&m, 's'));

	/* 's' supercedes 'p' */
	assert_eq(mode_chanmode_set(&m, &c, 'p', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(mode_chanmode_set(&m, &c, 's', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("dej", MODE_CHANMODE_PREFIX_SECRET);
	assert_true(mode_isset(&m, 's'));
	assert_false(mode_isset(&m, 'p'));

	/* 'p' is silently ignored when 's' is set */
	assert_eq(mode_chanmode_set(&m, &c, 'p', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("dej", MODE_CHANMODE_PREFIX_SECRET);
	assert_true(mode_isset(&m, 's'));
	assert_false(mode_isset(&m, 'p'));

	/* Unsetting 's' sets default */
	assert_eq(mode_chanmode_set(&m, &c, 's', MODE_SET_OFF), MODE_ERR_NONE);
	CHECK("dej", MODE_CHANMODE_PREFIX_OTHER);
	assert_false(mode_isset(&m, 'p'));
	assert_false(mode_isset(&m, 's'));

	/* Test unsetting previous flags */
	assert_eq(mode_chanmode_set(&m, &c, 'j', MODE_SET_OFF), MODE_ERR_NONE);
	CHECK("de", MODE_CHANMODE_PREFIX_OTHER);

	assert_eq(mode_chanmode_set(&m, &c, 'e', MODE_SET_OFF), MODE_ERR_NONE);
	CHECK("d", MODE_CHANMODE_PREFIX_OTHER);

	assert_eq(mode_chanmode_set(&m, &c, 'd', MODE_SET_OFF), MODE_ERR_NONE);
	CHECK("", MODE_CHANMODE_PREFIX_OTHER);

#undef CHECK
}

static void
test_prfxmode_set(void)
{
	/* Test setting/unsetting prfxmode flag and prefix */

	struct mode m = MODE_EMPTY;

	struct mode_config c = {
		.PREFIX = {
			.F = "abc",
			.T = "123"
		}
	};

	mode_config_chanmodes(&c, "abc");

	/* Test setting/unsetting invalid prfxmode flag */
	assert_eq(mode_prfxmode_set(&m, &c, 'd', MODE_SET_ON), MODE_ERR_INVALID_FLAG);
	assert_eq(m.prefix, 0);
	assert_eq(mode_prfxmode_set(&m, &c, 'd', MODE_SET_OFF), MODE_ERR_INVALID_FLAG);
	assert_eq(m.prefix, 0);

	/* Test setting valid flags respects PREFIX precedence */
	assert_eq(mode_prfxmode_set(&m, &c, 'b', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(m.prefix, '2');
	assert_eq(mode_prfxmode_set(&m, &c, 'c', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(m.prefix, '2');
	assert_eq(mode_prfxmode_set(&m, &c, 'a', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(m.prefix, '1');

	/* Test unsetting valid flags respects PREFIX precedence */
	assert_eq(mode_prfxmode_set(&m, &c, 'b', MODE_SET_OFF), MODE_ERR_NONE);
	assert_eq(m.prefix, '1');
	assert_eq(mode_prfxmode_set(&m, &c, 'a', MODE_SET_OFF), MODE_ERR_NONE);
	assert_eq(m.prefix, '3');
	assert_eq(mode_prfxmode_set(&m, &c, 'c', MODE_SET_OFF), MODE_ERR_NONE);
	assert_eq(m.prefix, 0);
}

static void
test_usermode_set(void)
{
	/* Test setting/unsetting usermode flag and mode string */

	struct mode m = MODE_EMPTY;
	struct mode_str m_str = { .type = MODE_STR_USERMODE };
	struct mode_config c;

	mode_config_usermodes(&c, "azAZ");

	/* Test setting invalid usermode flag */
	assert_eq(mode_usermode_set(&m, &c, 'b', MODE_SET_ON), MODE_ERR_INVALID_FLAG);

	/* Test setting valid flags */
	assert_eq(mode_usermode_set(&m, &c, 'a', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &c, 'Z', MODE_SET_ON), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &m_str), "aZ");

	assert_eq(mode_usermode_set(&m, &c, 'z', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &c, 'A', MODE_SET_ON), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &m_str), "azAZ");

	/* Test unsetting invalid usermode flag */
	assert_eq(mode_usermode_set(&m, &c, 'c', MODE_SET_OFF), MODE_ERR_INVALID_FLAG);

	/* Test unsetting valid flags */
	assert_eq(mode_usermode_set(&m, &c, 'z', MODE_SET_OFF), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &c, 'Z', MODE_SET_OFF), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &m_str), "aA");

	assert_eq(mode_usermode_set(&m, &c, 'a', MODE_SET_OFF), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &c, 'A', MODE_SET_OFF), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &m_str), "");
}

static void
test_chanmode_prefix(void)
{
	/* Test setting chanmode by prefix */

	struct mode m = MODE_EMPTY;
	struct mode_config c;
	struct mode_str m_str = { .type = MODE_STR_CHANMODE };

	mode_config_chanmodes(&c, "sp");

#define CHECK(M, PRFX, P, S, STR) \
	assert_eq((M).prefix, (PRFX));         \
	assert_eq(mode_isset(&(M), 'p'), (P)); \
	assert_eq(mode_isset(&(M), 's'), (S)); \
	assert_strcmp(mode_str(&(M), &m_str), (STR));

	/* Test setting invalid chanmode prefix */
	assert_eq(mode_chanmode_prefix(&m, &c, '$'), MODE_ERR_INVALID_PREFIX);
	CHECK(m, 0, 0, 0, "");

	/* Test setting valid chanmode prefixes by precedence*/
	assert_eq(mode_chanmode_prefix(&m, &c, MODE_CHANMODE_PREFIX_OTHER), MODE_ERR_NONE);
	CHECK(m, MODE_CHANMODE_PREFIX_OTHER, 0, 0, "");

	assert_eq(mode_chanmode_prefix(&m, &c, MODE_CHANMODE_PREFIX_PRIVATE), MODE_ERR_NONE);
	CHECK(m, MODE_CHANMODE_PREFIX_PRIVATE, 1, 0, "");

	assert_eq(mode_chanmode_prefix(&m, &c, MODE_CHANMODE_PREFIX_SECRET), MODE_ERR_NONE);
	CHECK(m, MODE_CHANMODE_PREFIX_SECRET, 0, 1, "");

	/* Test silently ignored setting by precedence */

	/* PRIVATE > OTHER */
	struct mode m2 = MODE_EMPTY;

	assert_eq(mode_chanmode_prefix(&m2, &c, MODE_CHANMODE_PREFIX_PRIVATE), MODE_ERR_NONE);
	assert_eq(mode_chanmode_prefix(&m2, &c, MODE_CHANMODE_PREFIX_OTHER), MODE_ERR_NONE);
	CHECK(m2, MODE_CHANMODE_PREFIX_PRIVATE, 1, 0, "");

	/* SECRET > PRIVATE, OTHER */
	struct mode m3 = MODE_EMPTY;

	assert_eq(mode_chanmode_prefix(&m3, &c, MODE_CHANMODE_PREFIX_SECRET), MODE_ERR_NONE);
	assert_eq(mode_chanmode_prefix(&m3, &c, MODE_CHANMODE_PREFIX_PRIVATE), MODE_ERR_NONE);
	assert_eq(mode_chanmode_prefix(&m3, &c, MODE_CHANMODE_PREFIX_OTHER), MODE_ERR_NONE);
	CHECK(m3, MODE_CHANMODE_PREFIX_SECRET, 0, 1, "");

#undef CHECK
}

static void
test_prfxmode_prefix(void)
{
	/* Test setting prfxmode by prefix */

	struct mode m = MODE_EMPTY;
	struct mode_str m_str = { .type = MODE_STR_PRFXMODE };

	struct mode_config c = {
		.PREFIX = {
			.F = "abc",
			.T = "123"
		}
	};

	mode_config_chanmodes(&c, "abc");

	/* Test setting invalid prfxmode prefix */
	assert_eq(mode_prfxmode_prefix(&m, &c, '4'), MODE_ERR_INVALID_PREFIX);

	/* Test setting valid prfxmode prefix */
	assert_eq(mode_prfxmode_prefix(&m, &c, '2'), MODE_ERR_NONE);

	assert_strcmp(mode_str(&m, &m_str), "b");
	assert_eq(m.prefix, '2');
}

static void
test_mode_config_usermodes(void)
{
	/* Test configuring server usermodes */

	struct mode_config c;
	struct mode_str m_str = { .type = MODE_STR_USERMODE };

	/* Test empty string */
	assert_eq(mode_config_usermodes(&c, ""), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(c.usermodes), &m_str), "");

	/* Test invalid flags */
	assert_eq(mode_config_usermodes(&c, "$abc1!xyz."), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(c.usermodes), &m_str), "abcxyz");

	/* Test duplicate flags */
	assert_eq(mode_config_usermodes(&c, "aaabbc"), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(c.usermodes), &m_str), "abc");

	/* Test valid string */
	assert_eq(mode_config_usermodes(&c, ALL_LOWERS ALL_UPPERS), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(c.usermodes), &m_str), ALL_LOWERS ALL_UPPERS);
}

static void
test_mode_config_chanmodes(void)
{
	/* Test configuring server chanmodes */

	struct mode_config c;
	struct mode_str m_str = { .type = MODE_STR_USERMODE };

	/* Test empty string */
	assert_eq(mode_config_chanmodes(&c, ""), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(c.chanmodes), &m_str), "");

	/* Test invalid flags */
	assert_eq(mode_config_chanmodes(&c, "$abc1!xyz."), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(c.chanmodes), &m_str), "abcxyz");

	/* Test duplicate flags */
	assert_eq(mode_config_chanmodes(&c, "aaabbc"), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(c.chanmodes), &m_str), "abc");

	/* Test valid string */
	assert_eq(mode_config_chanmodes(&c, ALL_LOWERS ALL_UPPERS), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(c.chanmodes), &m_str), ALL_LOWERS ALL_UPPERS);
}

static void
test_mode_config_subtypes(void)
{
	/* Test configuring CHANMODE subtypes */

	struct mode_config c;
	struct mode_str m_str = { .type = MODE_STR_USERMODE };

#define CHECK(_A, _B, _C, _D) \
	assert_strcmp(mode_str(&(c.CHANMODES.A), &m_str), (_A)); \
	assert_strcmp(mode_str(&(c.CHANMODES.B), &m_str), (_B)); \
	assert_strcmp(mode_str(&(c.CHANMODES.C), &m_str), (_C)); \
	assert_strcmp(mode_str(&(c.CHANMODES.D), &m_str), (_D));

	/* Test empty string */
	assert_eq(mode_config_subtypes(&c, ""), MODE_ERR_NONE);
	CHECK("", "", "", "");

	/* Test missing commas */
	assert_eq(mode_config_subtypes(&c, "abc,def"), MODE_ERR_NONE);
	CHECK("abc", "def", "", "");

	/* Test extra commas */
	assert_eq(mode_config_subtypes(&c, "abc,def,,xyz,,,abc"), MODE_ERR_INVALID_CONFIG);
	CHECK("abc", "def", "", "xyz");

	/* Test invalid flags */
	assert_eq(mode_config_subtypes(&c, "!!abc,d123e,fg!-@,^&"), MODE_ERR_NONE);
	CHECK("abc", "de", "fg", "");

	/* Test duplicate flags */
	assert_eq(mode_config_subtypes(&c, "zaabc,deefz,zghh,zzz"), MODE_ERR_NONE);
	CHECK("abcz", "def", "gh", "");

	const char *all_flags =
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS;

	/* Test valid string */
	assert_eq(mode_config_subtypes(&c, all_flags), MODE_ERR_NONE);
	CHECK(ALL_LOWERS ALL_UPPERS, "", "", "");

#undef CHECK
}

static void
test_mode_config_prefix(void)
{
	/* Test configuring PREFIX */

	struct mode_config c;

#define CHECK(_F, _T) \
	assert_strcmp(c.PREFIX.F, (_F)); assert_strcmp(c.PREFIX.T, (_T));

	/* Test empty string */
	assert_eq(mode_config_prefix(&c, ""), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test invalid formats */
	assert_eq(mode_config_prefix(&c, "abc123"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	assert_eq(mode_config_prefix(&c, "abc)123"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	assert_eq(mode_config_prefix(&c, "(abc123"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	assert_eq(mode_config_prefix(&c, ")(abc"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test unequal lengths */
	assert_eq(mode_config_prefix(&c, "(abc)12"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	assert_eq(mode_config_prefix(&c, "(ab)123"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test invalid flags */
	assert_eq(mode_config_prefix(&c, "(ab1)12"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test unprintable prefix */
	assert_eq(mode_config_prefix(&c, "(abc)1" "\x01" "3"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test duplicates flags */
	assert_eq(mode_config_prefix(&c, "(aabc)1234"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test valid string */
	assert_eq(mode_config_prefix(&c, "(abc)123"), MODE_ERR_NONE);
	CHECK("abc", "123");

#undef CHECK
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_flag_bit),
		TESTCASE(test_mode_str),
		TESTCASE(test_chanmode_set),
		TESTCASE(test_prfxmode_set),
		TESTCASE(test_usermode_set),
		TESTCASE(test_chanmode_prefix),
		TESTCASE(test_prfxmode_prefix),
		TESTCASE(test_mode_config_usermodes),
		TESTCASE(test_mode_config_chanmodes),
		TESTCASE(test_mode_config_subtypes),
		TESTCASE(test_mode_config_prefix)
	};

	return run_tests(tests);
}
