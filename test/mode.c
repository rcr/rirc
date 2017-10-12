#include "test.h"

#include "../src/mode.c"

#define MODE_EMPTY {0}

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

#define ALL_LOWERS "abcdefghijklmnopqrstuvwxyz"
#define ALL_UPPERS "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

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

#undef ALL_LOWERS
#undef ALL_UPPERS
}

static void
test_chanmode_set(void)
{
	/* Test setting/unsetting chanmode flag, prefix and mode string */

	struct mode m = MODE_EMPTY;
	struct mode_str m_str = { .type = MODE_STR_CHANMODE };

	struct mode_config c = {
		.chanmodes = "abcdefghijsp",
		.CHANMODES = {
			.A = "abc",
			.B = "def",
			.C = "ghi",
			.D = "jsp"
		}
	};

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
		.chanmodes = "abc",
		.PREFIX = {
			.F = "abc",
			.T = "123"
		}
	};

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

	struct mode_config c = {
		.usermodes = "azAZ"
	};

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
	struct mode_str m_str = { .type = MODE_STR_CHANMODE };

	struct mode_config c = {
		.chanmodes = "sp",
	};

	/* Test setting invalid chanmode prefix */
	assert_eq(mode_chanmode_prefix(&m, &c, '$'), MODE_ERR_INVALID_PREFIX);
	assert_eq(m.prefix, 0);
	assert_strcmp(mode_str(&m, &m_str), "");
	assert_false(mode_isset(&m, 'p'));
	assert_false(mode_isset(&m, 's'));

	/* Test setting valid chanmode prefixes by precedence*/
	assert_eq(mode_chanmode_prefix(&m, &c, MODE_CHANMODE_PREFIX_OTHER), MODE_ERR_NONE);
	assert_eq(m.prefix, MODE_CHANMODE_PREFIX_OTHER);
	assert_strcmp(mode_str(&m, &m_str), "");
	assert_false(mode_isset(&m, 'p'));
	assert_false(mode_isset(&m, 's'));

	assert_eq(mode_chanmode_prefix(&m, &c, MODE_CHANMODE_PREFIX_PRIVATE), MODE_ERR_NONE);
	assert_eq(m.prefix, MODE_CHANMODE_PREFIX_PRIVATE);
	assert_strcmp(mode_str(&m, &m_str), "");
	assert_true(mode_isset(&m, 'p'));
	assert_false(mode_isset(&m, 's'));

	assert_eq(mode_chanmode_prefix(&m, &c, MODE_CHANMODE_PREFIX_SECRET), MODE_ERR_NONE);
	assert_eq(m.prefix, MODE_CHANMODE_PREFIX_SECRET);
	assert_strcmp(mode_str(&m, &m_str), "");
	assert_false(mode_isset(&m, 'p'));
	assert_true(mode_isset(&m, 's'));

	/* Test silently ignored setting by precedence */

	/* PRIVATE > OTHER */
	struct mode m2 = MODE_EMPTY;

	assert_eq(mode_chanmode_prefix(&m2, &c, MODE_CHANMODE_PREFIX_PRIVATE), MODE_ERR_NONE);
	assert_eq(mode_chanmode_prefix(&m2, &c, MODE_CHANMODE_PREFIX_OTHER), MODE_ERR_NONE);
	assert_eq(m2.prefix, MODE_CHANMODE_PREFIX_PRIVATE);
	assert_strcmp(mode_str(&m2, &m_str), "");
	assert_true(mode_isset(&m2, 'p'));
	assert_false(mode_isset(&m2, 's'));

	/* SECRET > PRIVATE, OTHER */
	struct mode m3 = MODE_EMPTY;

	assert_eq(mode_chanmode_prefix(&m3, &c, MODE_CHANMODE_PREFIX_SECRET), MODE_ERR_NONE);
	assert_eq(mode_chanmode_prefix(&m3, &c, MODE_CHANMODE_PREFIX_PRIVATE), MODE_ERR_NONE);
	assert_eq(mode_chanmode_prefix(&m3, &c, MODE_CHANMODE_PREFIX_OTHER), MODE_ERR_NONE);
	assert_eq(m3.prefix, MODE_CHANMODE_PREFIX_SECRET);
	assert_strcmp(mode_str(&m3, &m_str), "");
	assert_false(mode_isset(&m3, 'p'));
	assert_true(mode_isset(&m3, 's'));
}

static void
test_prfxmode_prefix(void)
{
	/* Test setting prfxmode by prefix */

	struct mode_config c = {
		.chanmodes = "abc",
		.PREFIX = {
			.F = "abc",
			.T = "123"
		}
	};

	struct mode m = MODE_EMPTY;
	struct mode_str m_str = { .type = MODE_STR_PRFXMODE };

	/* Test setting invalid prfxmode prefix */
	assert_eq(mode_prfxmode_prefix(&m, &c, '4'), MODE_ERR_INVALID_PREFIX);

	/* Test setting valid prfxmode prefix */
	assert_eq(mode_prfxmode_prefix(&m, &c, '2'), MODE_ERR_NONE);

	assert_strcmp(mode_str(&m, &m_str), "b");
	assert_eq(m.prefix, '2');
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
		TESTCASE(test_prfxmode_prefix)
	};

	return run_tests(tests);
}
