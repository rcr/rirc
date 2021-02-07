#include "test/test.h"
#include "src/components/mode.c"

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
	struct mode_str str;

	memset(&str, 0, sizeof(struct mode_str));

	/* mode_str type not set */
	assert_fatal(mode_str(&m, &str));

	str.type = MODE_STR_T_SIZE;

	/* mode_str type unknown */
	assert_fatal(mode_str(&m, &str));

	str.type = MODE_STR_USERMODE;

	/* Test no mode */
	assert_strcmp(mode_str(&m, &str), "");

	m.lower = UINT32_MAX;
	m.upper = 0;

	assert_strcmp(mode_str(&m, &str), ALL_LOWERS);

	m.lower = 0;
	m.upper = UINT32_MAX;

	assert_strcmp(mode_str(&m, &str), ALL_UPPERS);

	m.lower = UINT32_MAX;
	m.upper = UINT32_MAX;

	assert_strcmp(mode_str(&m, &str), ALL_LOWERS ALL_UPPERS);
}

static void
test_chanmode_set(void)
{
	/* Test setting/unsetting chanmode flag, prefix and mode string */

	struct mode m = MODE_EMPTY;
	struct mode_cfg cfg;
	struct mode_str str = { .type = MODE_STR_CHANMODE };

	mode_cfg_chanmodes(&cfg, "abcdefghijsp");
	mode_cfg_subtypes(&cfg, "abc,def,ghi,jsp");

#define CHECK(STR, PRFX) \
	assert_strcmp(mode_str(&m, &str), (STR)); assert_eq(m.prefix, (PRFX));

	/* Test setting/unsetting invalid chanmode flag */
	assert_eq(mode_chanmode_set(&m, &cfg, 'z', MODE_SET_ON), MODE_ERR_INVALID_FLAG);
	CHECK("", 0);
	assert_eq(mode_chanmode_set(&m, &cfg, 'z', MODE_SET_OFF), MODE_ERR_INVALID_FLAG);
	CHECK("", 0);

	/* Test valid CHANMODE subtype A doesn't set flag */
	assert_eq(mode_chanmode_set(&m, &cfg, 'a', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("", 0);

	/* Test valid CHANMODE subtypes B,C,D set flags */
	assert_eq(mode_chanmode_set(&m, &cfg, 'd', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("d", 0);

	assert_eq(mode_chanmode_set(&m, &cfg, 'e', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("de", 0);

	assert_eq(mode_chanmode_set(&m, &cfg, 'j', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("dej", 0);

	/* Test 's'/'p' chanmodes set prefix but do not appear in mode string
	 *
	 * Ensure 's' always supercedes and unsets 'p',
	 * Ensure 'p' is silently ignored when 's' is set */

	assert_eq(mode_chanmode_set(&m, &cfg, 'p', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("dejp", MODE_CHANMODE_PREFIX_PRIVATE);
	assert_true(mode_isset(&m, 'p'));
	assert_false(mode_isset(&m, 's'));

	/* Unsetting 'p' sets default */
	assert_eq(mode_chanmode_set(&m, &cfg, 'p', MODE_SET_OFF), MODE_ERR_NONE);
	CHECK("dej", MODE_CHANMODE_PREFIX_OTHER);
	assert_false(mode_isset(&m, 'p'));
	assert_false(mode_isset(&m, 's'));

	/* 's' supercedes 'p' */
	assert_eq(mode_chanmode_set(&m, &cfg, 'p', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(mode_chanmode_set(&m, &cfg, 's', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("dejs", MODE_CHANMODE_PREFIX_SECRET);
	assert_true(mode_isset(&m, 's'));
	assert_false(mode_isset(&m, 'p'));

	/* 'p' is silently ignored when 's' is set */
	assert_eq(mode_chanmode_set(&m, &cfg, 'p', MODE_SET_ON), MODE_ERR_NONE);
	CHECK("dejs", MODE_CHANMODE_PREFIX_SECRET);
	assert_true(mode_isset(&m, 's'));
	assert_false(mode_isset(&m, 'p'));

	/* Unsetting 's' sets default */
	assert_eq(mode_chanmode_set(&m, &cfg, 's', MODE_SET_OFF), MODE_ERR_NONE);
	CHECK("dej", MODE_CHANMODE_PREFIX_OTHER);
	assert_false(mode_isset(&m, 'p'));
	assert_false(mode_isset(&m, 's'));

	/* Test unsetting previous flags */
	assert_eq(mode_chanmode_set(&m, &cfg, 'j', MODE_SET_OFF), MODE_ERR_NONE);
	CHECK("de", MODE_CHANMODE_PREFIX_OTHER);

	assert_eq(mode_chanmode_set(&m, &cfg, 'e', MODE_SET_OFF), MODE_ERR_NONE);
	CHECK("d", MODE_CHANMODE_PREFIX_OTHER);

	assert_eq(mode_chanmode_set(&m, &cfg, 'd', MODE_SET_OFF), MODE_ERR_NONE);
	CHECK("", MODE_CHANMODE_PREFIX_OTHER);

#undef CHECK
}

static void
test_prfxmode_set(void)
{
	/* Test setting/unsetting prfxmode flag and prefix */

	struct mode m = MODE_EMPTY;

	struct mode_cfg cfg = {
		.PREFIX = {
			.F = "abc",
			.T = "123"
		}
	};

	mode_cfg_chanmodes(&cfg, "abc");

	/* Test setting/unsetting invalid prfxmode flag */
	assert_eq(mode_prfxmode_set(&m, &cfg, 'd', MODE_SET_ON), MODE_ERR_INVALID_FLAG);
	assert_eq(m.prefix, 0);
	assert_eq(mode_prfxmode_set(&m, &cfg, 'd', MODE_SET_OFF), MODE_ERR_INVALID_FLAG);
	assert_eq(m.prefix, 0);

	/* Test setting valid flags respects PREFIX precedence */
	assert_eq(mode_prfxmode_set(&m, &cfg, 'b', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(m.prefix, '2');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'c', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(m.prefix, '2');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'a', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(m.prefix, '1');

	/* Test unsetting valid flags respects PREFIX precedence */
	assert_eq(mode_prfxmode_set(&m, &cfg, 'b', MODE_SET_OFF), MODE_ERR_NONE);
	assert_eq(m.prefix, '1');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'a', MODE_SET_OFF), MODE_ERR_NONE);
	assert_eq(m.prefix, '3');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'c', MODE_SET_OFF), MODE_ERR_NONE);
	assert_eq(m.prefix, 0);
}

static void
test_usermode_set(void)
{
	/* Test setting/unsetting usermode flag and mode string */

	struct mode m = MODE_EMPTY;
	struct mode_str str = { .type = MODE_STR_USERMODE };
	struct mode_cfg cfg;

	mode_cfg_usermodes(&cfg, "azAZ");

	/* Test setting invalid usermode flag */
	assert_eq(mode_usermode_set(&m, &cfg, 'b', MODE_SET_ON), MODE_ERR_INVALID_FLAG);

	/* Test setting valid flags */
	assert_eq(mode_usermode_set(&m, &cfg, 'a', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &cfg, 'Z', MODE_SET_ON), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "aZ");

	assert_eq(mode_usermode_set(&m, &cfg, 'z', MODE_SET_ON), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &cfg, 'A', MODE_SET_ON), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "azAZ");

	/* Test unsetting invalid usermode flag */
	assert_eq(mode_usermode_set(&m, &cfg, 'c', MODE_SET_OFF), MODE_ERR_INVALID_FLAG);

	/* Test unsetting valid flags */
	assert_eq(mode_usermode_set(&m, &cfg, 'z', MODE_SET_OFF), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &cfg, 'Z', MODE_SET_OFF), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "aA");

	assert_eq(mode_usermode_set(&m, &cfg, 'a', MODE_SET_OFF), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &cfg, 'A', MODE_SET_OFF), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "");
}

static void
test_chanmode_prefix(void)
{
	/* Test setting chanmode by prefix */

	struct mode m = MODE_EMPTY;
	struct mode_cfg cfg;
	struct mode_str str = { .type = MODE_STR_CHANMODE };

	mode_cfg_chanmodes(&cfg, "sp");

#define CHECK(M, PRFX, P, S, STR) \
	assert_eq((M).prefix, (PRFX));         \
	assert_eq(mode_isset(&(M), 'p'), (P)); \
	assert_eq(mode_isset(&(M), 's'), (S)); \
	assert_strcmp(mode_str(&(M), &str), (STR));

	/* Test setting invalid chanmode prefix */
	assert_eq(mode_chanmode_prefix(&m, &cfg, '$'), MODE_ERR_INVALID_PREFIX);
	CHECK(m, 0, 0, 0, "");

	/* Test setting valid chanmode prefixes by precedence*/
	assert_eq(mode_chanmode_prefix(&m, &cfg, MODE_CHANMODE_PREFIX_OTHER), MODE_ERR_NONE);
	CHECK(m, MODE_CHANMODE_PREFIX_OTHER, 0, 0, "");

	assert_eq(mode_chanmode_prefix(&m, &cfg, MODE_CHANMODE_PREFIX_PRIVATE), MODE_ERR_NONE);
	CHECK(m, MODE_CHANMODE_PREFIX_PRIVATE, 1, 0, "p");

	assert_eq(mode_chanmode_prefix(&m, &cfg, MODE_CHANMODE_PREFIX_SECRET), MODE_ERR_NONE);
	CHECK(m, MODE_CHANMODE_PREFIX_SECRET, 0, 1, "s");

	/* Test silently ignored setting by precedence */

	/* PRIVATE > OTHER */
	struct mode m2 = MODE_EMPTY;

	assert_eq(mode_chanmode_prefix(&m2, &cfg, MODE_CHANMODE_PREFIX_PRIVATE), MODE_ERR_NONE);
	assert_eq(mode_chanmode_prefix(&m2, &cfg, MODE_CHANMODE_PREFIX_OTHER), MODE_ERR_NONE);
	CHECK(m2, MODE_CHANMODE_PREFIX_PRIVATE, 1, 0, "p");

	/* SECRET > PRIVATE, OTHER */
	struct mode m3 = MODE_EMPTY;

	assert_eq(mode_chanmode_prefix(&m3, &cfg, MODE_CHANMODE_PREFIX_SECRET), MODE_ERR_NONE);
	assert_eq(mode_chanmode_prefix(&m3, &cfg, MODE_CHANMODE_PREFIX_PRIVATE), MODE_ERR_NONE);
	assert_eq(mode_chanmode_prefix(&m3, &cfg, MODE_CHANMODE_PREFIX_OTHER), MODE_ERR_NONE);
	CHECK(m3, MODE_CHANMODE_PREFIX_SECRET, 0, 1, "s");

#undef CHECK
}

static void
test_prfxmode_prefix(void)
{
	/* Test setting prfxmode by prefix */

	struct mode m = MODE_EMPTY;
	struct mode_str str = { .type = MODE_STR_PRFXMODE };

	struct mode_cfg cfg = {
		.PREFIX = {
			.F = "abc",
			.T = "123"
		}
	};

	mode_cfg_chanmodes(&cfg, "abc");

	/* Test setting invalid prfxmode prefix */
	assert_eq(mode_prfxmode_prefix(&m, &cfg, 0),   MODE_ERR_INVALID_PREFIX);
	assert_eq(mode_prfxmode_prefix(&m, &cfg, '0'), MODE_ERR_INVALID_PREFIX);
	assert_eq(mode_prfxmode_prefix(&m, &cfg, '4'), MODE_ERR_INVALID_PREFIX);

	/* Test setting valid prfxmode prefix */
	assert_eq(mode_prfxmode_prefix(&m, &cfg, '2'), MODE_ERR_NONE);
	assert_eq(mode_prfxmode_prefix(&m, &cfg, '3'), MODE_ERR_NONE);

	assert_strcmp(mode_str(&m, &str), "bc");
	assert_eq(m.prefix, '2');
}

static void
test_mode_cfg_usermodes(void)
{
	/* Test configuring server usermodes */

	struct mode_cfg cfg;
	struct mode_str str = { .type = MODE_STR_USERMODE };

	/* Test empty string */
	assert_eq(mode_cfg_usermodes(&cfg, ""), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(cfg.usermodes), &str), "");

	/* Test invalid flags */
	assert_eq(mode_cfg_usermodes(&cfg, "$abc1!xyz."), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(cfg.usermodes), &str), "abcxyz");

	/* Test duplicate flags */
	assert_eq(mode_cfg_usermodes(&cfg, "aaabbc"), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(cfg.usermodes), &str), "abc");

	/* Test valid string */
	assert_eq(mode_cfg_usermodes(&cfg, ALL_LOWERS ALL_UPPERS), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(cfg.usermodes), &str), ALL_LOWERS ALL_UPPERS);
}

static void
test_mode_cfg_chanmodes(void)
{
	/* Test configuring server chanmodes */

	struct mode_cfg cfg;
	struct mode_str str = { .type = MODE_STR_USERMODE };

	/* Test empty string */
	assert_eq(mode_cfg_chanmodes(&cfg, ""), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(cfg.chanmodes), &str), "");

	/* Test invalid flags */
	assert_eq(mode_cfg_chanmodes(&cfg, "$abc1!xyz."), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(cfg.chanmodes), &str), "abcxyz");

	/* Test duplicate flags */
	assert_eq(mode_cfg_chanmodes(&cfg, "aaabbc"), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(cfg.chanmodes), &str), "abc");

	/* Test valid string */
	assert_eq(mode_cfg_chanmodes(&cfg, ALL_LOWERS ALL_UPPERS), MODE_ERR_NONE);
	assert_strcmp(mode_str(&(cfg.chanmodes), &str), ALL_LOWERS ALL_UPPERS);
}

static void
test_mode_cfg_subtypes(void)
{
	/* Test configuring CHANMODE subtypes */

	struct mode_cfg cfg;
	struct mode_str str = { .type = MODE_STR_USERMODE };

#define CHECK(_A, _B, _C, _D) \
	assert_strcmp(mode_str(&(cfg.CHANMODES.A), &str), (_A)); \
	assert_strcmp(mode_str(&(cfg.CHANMODES.B), &str), (_B)); \
	assert_strcmp(mode_str(&(cfg.CHANMODES.C), &str), (_C)); \
	assert_strcmp(mode_str(&(cfg.CHANMODES.D), &str), (_D));

	/* Test empty string */
	assert_eq(mode_cfg_subtypes(&cfg, ""), MODE_ERR_NONE);
	CHECK("", "", "", "");

	/* Test missing commas */
	assert_eq(mode_cfg_subtypes(&cfg, "abc,def"), MODE_ERR_NONE);
	CHECK("abc", "def", "", "");

	/* Test extra commas */
	assert_eq(mode_cfg_subtypes(&cfg, "abc,def,,xyz,,,abc"), MODE_ERR_INVALID_CONFIG);
	CHECK("abc", "def", "", "xyz");

	/* Test invalid flags */
	assert_eq(mode_cfg_subtypes(&cfg, "!!abc,d123e,fg!-@,^&"), MODE_ERR_NONE);
	CHECK("abc", "de", "fg", "");

	/* Test duplicate flags */
	assert_eq(mode_cfg_subtypes(&cfg, "zaabc,deefz,zghh,zzz"), MODE_ERR_NONE);
	CHECK("abcz", "def", "gh", "");

	const char *all_flags =
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS;

	/* Test valid string */
	assert_eq(mode_cfg_subtypes(&cfg, all_flags), MODE_ERR_NONE);
	CHECK(ALL_LOWERS ALL_UPPERS, "", "", "");

#undef CHECK
}

static void
test_mode_cfg_prefix(void)
{
	/* Test configuring PREFIX */

	struct mode_cfg cfg;

#define CHECK(_F, _T) \
	assert_strcmp(cfg.PREFIX.F, (_F)); \
	assert_strcmp(cfg.PREFIX.T, (_T));

	/* Test empty string */
	assert_eq(mode_cfg_prefix(&cfg, ""), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test invalid formats */
	assert_eq(mode_cfg_prefix(&cfg, "abc123"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	assert_eq(mode_cfg_prefix(&cfg, "abc)123"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	assert_eq(mode_cfg_prefix(&cfg, "(abc123"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	assert_eq(mode_cfg_prefix(&cfg, ")(abc"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test unequal lengths */
	assert_eq(mode_cfg_prefix(&cfg, "(abc)12"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	assert_eq(mode_cfg_prefix(&cfg, "(ab)123"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test invalid flags */
	assert_eq(mode_cfg_prefix(&cfg, "(ab1)12"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test unprintable prefix */
	assert_eq(mode_cfg_prefix(&cfg, "(abc)1" "\x01" "3"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test duplicates flags */
	assert_eq(mode_cfg_prefix(&cfg, "(aabc)1234"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "");

	/* Test valid string */
	assert_eq(mode_cfg_prefix(&cfg, "(abc)123"), MODE_ERR_NONE);
	CHECK("abc", "123");

#undef CHECK
}

static void
test_mode_cfg_modes(void)
{
	/* Test configuring MODES */

	struct mode_cfg cfg = {
		.MODES = 3
	};

	/* Test empty string */
	assert_eq(mode_cfg_modes(&cfg, ""), MODE_ERR_INVALID_CONFIG);
	assert_eq(cfg.MODES, 3);

	/* Test not a number */
	assert_eq(mode_cfg_modes(&cfg, "1abc"), MODE_ERR_INVALID_CONFIG);
	assert_eq(mode_cfg_modes(&cfg, "wxyz"), MODE_ERR_INVALID_CONFIG);
	assert_eq(cfg.MODES, 3);

	/* Test invalid number (i.e.: not [1-99]) */
	assert_eq(mode_cfg_modes(&cfg, "0"),   MODE_ERR_INVALID_CONFIG);
	assert_eq(mode_cfg_modes(&cfg, "100"), MODE_ERR_INVALID_CONFIG);
	assert_eq(cfg.MODES, 3);

	/* Teset valid numbers */
	assert_eq(mode_cfg_modes(&cfg, "1"),  MODE_ERR_NONE);
	assert_eq(cfg.MODES, 1);

	assert_eq(mode_cfg_modes(&cfg, "99"), MODE_ERR_NONE);
	assert_eq(cfg.MODES, 99);
}

static void
test_chanmode_type(void)
{
	/* Test retrieving a mode flag type */

	struct mode_cfg cfg;

	int config_errs = 0;

	config_errs -= mode_cfg(&cfg, "a",       MODE_CFG_USERMODES);
	config_errs -= mode_cfg(&cfg, "bcdef",   MODE_CFG_CHANMODES);
	config_errs -= mode_cfg(&cfg, "b,c,d,e", MODE_CFG_SUBTYPES);
	config_errs -= mode_cfg(&cfg, "(f)@",    MODE_CFG_PREFIX);

	if (config_errs != MODE_ERR_NONE)
		test_abort("Configuration error");

	/* Test invalid '+'/'-' */
	assert_eq(chanmode_type(&cfg, MODE_SET_INVALID, 'a'), MODE_FLAG_INVALID_SET);

	/* Test invalid flag */
	assert_eq(chanmode_type(&cfg, MODE_SET_ON, '!'), MODE_FLAG_INVALID_FLAG);

	/* Test flag not in usermodes, chanmodes */
	assert_eq(chanmode_type(&cfg, MODE_SET_ON, 'z'), MODE_FLAG_INVALID_FLAG);

	/* Test chanmode A (always has a parameter) */
	assert_eq(chanmode_type(&cfg, MODE_SET_ON,  'b'), MODE_FLAG_CHANMODE_PARAM);
	assert_eq(chanmode_type(&cfg, MODE_SET_OFF, 'b'), MODE_FLAG_CHANMODE_PARAM);

	/* Test chanmode B (always has a parameter) */
	assert_eq(chanmode_type(&cfg, MODE_SET_ON,  'c'), MODE_FLAG_CHANMODE_PARAM);
	assert_eq(chanmode_type(&cfg, MODE_SET_OFF, 'c'), MODE_FLAG_CHANMODE_PARAM);

	/* Test chanmode C (only has a parameter when set) */
	assert_eq(chanmode_type(&cfg, MODE_SET_ON,  'd'), MODE_FLAG_CHANMODE_PARAM);
	assert_eq(chanmode_type(&cfg, MODE_SET_OFF, 'd'), MODE_FLAG_CHANMODE);

	/* Test chanmode D (never has a parameter) */
	assert_eq(chanmode_type(&cfg, MODE_SET_ON,  'e'), MODE_FLAG_CHANMODE);
	assert_eq(chanmode_type(&cfg, MODE_SET_OFF, 'e'), MODE_FLAG_CHANMODE);

	/* Test prefix flag */
	assert_eq(chanmode_type(&cfg, MODE_SET_ON,  'f'), MODE_FLAG_PREFIX);
	assert_eq(chanmode_type(&cfg, MODE_SET_OFF, 'f'), MODE_FLAG_PREFIX);
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_flag_bit),
		TESTCASE(test_mode_str),
		TESTCASE(test_chanmode_set),
		TESTCASE(test_prfxmode_set),
		TESTCASE(test_usermode_set),
		TESTCASE(test_chanmode_prefix),
		TESTCASE(test_prfxmode_prefix),
		TESTCASE(test_mode_cfg_usermodes),
		TESTCASE(test_mode_cfg_chanmodes),
		TESTCASE(test_mode_cfg_subtypes),
		TESTCASE(test_mode_cfg_prefix),
		TESTCASE(test_mode_cfg_modes),
		TESTCASE(test_chanmode_type)
	};

	return run_tests(tests);
}
