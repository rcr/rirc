#include "test/test.h"
#include "src/components/mode.c"
#include "src/utils/utils.c"

#define ALL_LOWERS "abcdefghijklmnopqrstuvwxyz"
#define ALL_UPPERS "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

static void
test_mode_bit(void)
{
	/* Test mode flag -> mode bit */

	assert_eq(mode_bit('a' - 1), 0);
	assert_eq(mode_bit('a'),     1 << 0);
	assert_eq(mode_bit('z'),     1 << 25);
	assert_eq(mode_bit('z' + 1), 0);

	assert_eq(mode_bit('A' - 1), 0);
	assert_eq(mode_bit('A'),     1 << 0);
	assert_eq(mode_bit('Z'),     1 << 25);
	assert_eq(mode_bit('Z' + 1), 0);
}

static void
test_mode_str(void)
{
	/* Test setting mode string */

	struct mode m = {0};
	struct mode_str str;

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
test_mode_chanmode_set(void)
{
	/* Test setting/unsetting chanmode flags */

	struct mode m = {0};
	struct mode_cfg cfg;
	struct mode_str str;

	mode_cfg_chanmodes(&cfg, "abcdefghijsp");
	mode_cfg_subtypes(&cfg, "abc,def,ghi,jsp");

	/* Test setting/unsetting invalid chanmode flag */
	assert_eq(mode_chanmode_set(&m, &cfg, 'z', 1), -1);
	assert_strcmp(mode_str(&m, &str), "");

	assert_eq(mode_chanmode_set(&m, &cfg, 'z', 0), -1);
	assert_strcmp(mode_str(&m, &str), "");

	/* Test valid CHANMODE subtype A doesn't set flag */
	assert_eq(mode_chanmode_set(&m, &cfg, 'a', 1), 0);
	assert_strcmp(mode_str(&m, &str), "");

	/* Test valid CHANMODE subtypes B,C,D set flags */
	assert_eq(mode_chanmode_set(&m, &cfg, 'd', 1), 0);
	assert_strcmp(mode_str(&m, &str), "d");

	assert_eq(mode_chanmode_set(&m, &cfg, 'e', 1), 0);
	assert_strcmp(mode_str(&m, &str), "de");

	assert_eq(mode_chanmode_set(&m, &cfg, 'j', 1), 0);
	assert_strcmp(mode_str(&m, &str), "dej");

	/* test unsetting flags */
	assert_eq(mode_chanmode_set(&m, &cfg, 's', 0), 0);
	assert_eq(mode_chanmode_set(&m, &cfg, 'j', 0), 0);
	assert_eq(mode_chanmode_set(&m, &cfg, 'e', 0), 0);
	assert_eq(mode_chanmode_set(&m, &cfg, 'd', 0), 0);
	assert_strcmp(mode_str(&m, &str), "");
}

static void
test_mode_prfxmode_set(void)
{
	/* Test setting/unsetting prfxmode flag and prefix */

	struct mode m = {0};
	struct mode_cfg cfg = {
		.PREFIX = {
			.F = "abc",
			.T = "123"
		}
	};
	struct mode_str str;

	mode_cfg_chanmodes(&cfg, "abc");

	/* Test setting/unsetting invalid prfxmode flag */
	assert_eq(mode_prfxmode_set(&m, &cfg, 'd', 1), -1);
	assert_eq(m.prefix, 0);
	assert_eq(mode_prfxmode_set(&m, &cfg, 'd', 0), -1);
	assert_eq(m.prefix, 0);

	/* Test setting/unsetting invalid prfxmode prefix */
	assert_eq(mode_prfxmode_set(&m, &cfg, '4', 1), -1);
	assert_eq(m.prefix, 0);
	assert_eq(mode_prfxmode_set(&m, &cfg, '4', 0), -1);
	assert_eq(m.prefix, 0);

	/* Test setting valid flags respects PREFIX precedence */
	assert_eq(mode_prfxmode_set(&m, &cfg, 'b', 1), 0);
	assert_eq(m.prefix, '2');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'c', 1), 0);
	assert_eq(m.prefix, '2');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'a', 1), 0);
	assert_eq(m.prefix, '1');

	/* Test unsetting valid flags respects PREFIX precedence */
	assert_eq(mode_prfxmode_set(&m, &cfg, 'b', 0), 0);
	assert_eq(m.prefix, '1');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'a', 0), 0);
	assert_eq(m.prefix, '3');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'c', 0), 0);
	assert_eq(m.prefix, 0);

	/* Test setting valid prefixes */
	assert_eq(mode_prfxmode_set(&m, &cfg, '2', 1), 0);
	assert_eq(m.prefix, '2');
	assert_eq(mode_prfxmode_set(&m, &cfg, '3', 1), 0);
	assert_eq(m.prefix, '2');

	assert_strcmp(mode_str(&m, &str), "bc");
}

static void
test_mode_usermode_set(void)
{
	/* Test setting/unsetting usermode flag and mode string */

	struct mode m = {0};
	struct mode_cfg cfg;
	struct mode_str str;

	mode_cfg_usermodes(&cfg, "azAZ");

	/* Test setting invalid usermode flag */
	assert_eq(mode_usermode_set(&m, &cfg, 'b', 1), -1);

	/* Test setting valid flags */
	assert_eq(mode_usermode_set(&m, &cfg, 'a', 1), 0);
	assert_eq(mode_usermode_set(&m, &cfg, 'Z', 1), 0);
	assert_strcmp(mode_str(&m, &str), "aZ");

	assert_eq(mode_usermode_set(&m, &cfg, 'z', 1), 0);
	assert_eq(mode_usermode_set(&m, &cfg, 'A', 1), 0);
	assert_strcmp(mode_str(&m, &str), "azAZ");

	/* Test unsetting invalid usermode flag */
	assert_eq(mode_usermode_set(&m, &cfg, 'c', 0), -1);

	/* Test unsetting valid flags */
	assert_eq(mode_usermode_set(&m, &cfg, 'z', 0), 0);
	assert_eq(mode_usermode_set(&m, &cfg, 'Z', 0), 0);
	assert_strcmp(mode_str(&m, &str), "aA");

	assert_eq(mode_usermode_set(&m, &cfg, 'a', 0), 0);
	assert_eq(mode_usermode_set(&m, &cfg, 'A', 0), 0);
	assert_strcmp(mode_str(&m, &str), "");
}

static void
test_mode_cfg_usermodes(void)
{
	/* Test configuring server usermodes */

	struct mode_cfg cfg;
	struct mode_str str;

	/* Test empty string */
	assert_eq(mode_cfg_usermodes(&cfg, ""), 0);
	assert_strcmp(mode_str(&(cfg.usermodes), &str), "");

	/* Test invalid flags */
	assert_eq(mode_cfg_usermodes(&cfg, "$abc1!xyz."), 0);
	assert_strcmp(mode_str(&(cfg.usermodes), &str), "abcxyz");

	/* Test duplicate flags */
	assert_eq(mode_cfg_usermodes(&cfg, "aaabbc"), 0);
	assert_strcmp(mode_str(&(cfg.usermodes), &str), "abc");

	/* Test valid string */
	assert_eq(mode_cfg_usermodes(&cfg, ALL_LOWERS ALL_UPPERS), 0);
	assert_strcmp(mode_str(&(cfg.usermodes), &str), ALL_LOWERS ALL_UPPERS);
}

static void
test_mode_cfg_chanmodes(void)
{
	/* Test configuring server chanmodes */

	struct mode_cfg cfg;
	struct mode_str str;

	/* Test empty string */
	assert_eq(mode_cfg_chanmodes(&cfg, ""), 0);
	assert_strcmp(mode_str(&(cfg.chanmodes), &str), "");

	/* Test invalid flags */
	assert_eq(mode_cfg_chanmodes(&cfg, "$abc1!xyz."), 0);
	assert_strcmp(mode_str(&(cfg.chanmodes), &str), "abcxyz");

	/* Test duplicate flags */
	assert_eq(mode_cfg_chanmodes(&cfg, "aaabbc"), 0);
	assert_strcmp(mode_str(&(cfg.chanmodes), &str), "abc");

	/* Test valid string */
	assert_eq(mode_cfg_chanmodes(&cfg, ALL_LOWERS ALL_UPPERS), 0);
	assert_strcmp(mode_str(&(cfg.chanmodes), &str), ALL_LOWERS ALL_UPPERS);
}

static void
test_mode_cfg_subtypes(void)
{
	/* Test configuring CHANMODE subtypes */

	struct mode_cfg cfg;
	struct mode_str str;

#define CHECK(_A, _B, _C, _D) \
	assert_strcmp(mode_str(&(cfg.CHANMODES.A), &str), (_A)); \
	assert_strcmp(mode_str(&(cfg.CHANMODES.B), &str), (_B)); \
	assert_strcmp(mode_str(&(cfg.CHANMODES.C), &str), (_C)); \
	assert_strcmp(mode_str(&(cfg.CHANMODES.D), &str), (_D));

	/* Test empty string */
	assert_eq(mode_cfg_subtypes(&cfg, ""), 0);
	CHECK("", "", "", "");

	/* Test missing commas */
	assert_eq(mode_cfg_subtypes(&cfg, "abc,def"), 0);
	CHECK("abc", "def", "", "");

	/* Test extra commas */
	assert_eq(mode_cfg_subtypes(&cfg, "abc,def,,xyz,,,abc"), -1);
	CHECK("", "", "", "");

	/* Test invalid flags */
	assert_eq(mode_cfg_subtypes(&cfg, "!!abc,d123e,fg!-@,^&"), -1);
	CHECK("", "", "", "");

	const char *all_flags =
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS;

	/* Test valid string */
	assert_eq(mode_cfg_subtypes(&cfg, all_flags), 0);
	CHECK(ALL_LOWERS ALL_UPPERS,
	      ALL_LOWERS ALL_UPPERS,
	      ALL_LOWERS ALL_UPPERS,
	      ALL_LOWERS ALL_UPPERS);

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
	assert_eq(mode_cfg_prefix(&cfg, ""), -1);
	CHECK("", "");

	/* Test invalid formats */
	assert_eq(mode_cfg_prefix(&cfg, "abc123"), -1);
	CHECK("", "");

	assert_eq(mode_cfg_prefix(&cfg, "abc)123"), -1);
	CHECK("", "");

	assert_eq(mode_cfg_prefix(&cfg, "(abc123"), -1);
	CHECK("", "");

	assert_eq(mode_cfg_prefix(&cfg, ")(abc"), -1);
	CHECK("", "");

	/* Test unequal lengths */
	assert_eq(mode_cfg_prefix(&cfg, "(abc)12"), -1);
	CHECK("", "");

	assert_eq(mode_cfg_prefix(&cfg, "(ab)123"), -1);
	CHECK("", "");

	/* Test invalid flags */
	assert_eq(mode_cfg_prefix(&cfg, "(ab1)12"), -1);
	CHECK("", "");

	/* Test unprintable prefix */
	assert_eq(mode_cfg_prefix(&cfg, "(abc)1" "\x01" "3"), -1);
	CHECK("", "");

	/* Test duplicates flags */
	assert_eq(mode_cfg_prefix(&cfg, "(aabc)1234"), -1);
	CHECK("", "");

	/* Test valid string */
	assert_eq(mode_cfg_prefix(&cfg, "(abc)123"), 0);
	CHECK("abc", "123");

#undef CHECK
}

static void
test_mode_type(void)
{
	/* Test retrieving a mode flag type */

	struct mode_cfg cfg;

	int config_errs = 0;

	config_errs -= mode_cfg(&cfg, "a",       MODE_CFG_USERMODES);
	config_errs -= mode_cfg(&cfg, "bcdef",   MODE_CFG_CHANMODES);
	config_errs -= mode_cfg(&cfg, "b,c,d,e", MODE_CFG_SUBTYPES);
	config_errs -= mode_cfg(&cfg, "(f)@",    MODE_CFG_PREFIX);

	if (config_errs != 0)
		test_abort("Configuration error");

	/* Test invalid flag */
	assert_eq(mode_type(&cfg, '!', 1), MODE_FLAG_INVALID_FLAG);

	/* Test flag not in usermodes, chanmodes */
	assert_eq(mode_type(&cfg, 'z', 1), MODE_FLAG_INVALID_FLAG);

	/* Test chanmode A (always has a parameter) */
	assert_eq(mode_type(&cfg, 'b', 1), MODE_FLAG_CHANMODE_PARAM);
	assert_eq(mode_type(&cfg, 'b', 0), MODE_FLAG_CHANMODE_PARAM);

	/* Test chanmode B (always has a parameter) */
	assert_eq(mode_type(&cfg, 'c', 1), MODE_FLAG_CHANMODE_PARAM);
	assert_eq(mode_type(&cfg, 'c', 0), MODE_FLAG_CHANMODE_PARAM);

	/* Test chanmode C (only has a parameter when set) */
	assert_eq(mode_type(&cfg, 'd', 1), MODE_FLAG_CHANMODE_PARAM);
	assert_eq(mode_type(&cfg, 'd', 0), MODE_FLAG_CHANMODE);

	/* Test chanmode D (never has a parameter) */
	assert_eq(mode_type(&cfg, 'e', 1), MODE_FLAG_CHANMODE);
	assert_eq(mode_type(&cfg, 'e', 0), MODE_FLAG_CHANMODE);

	/* Test prefix flag */
	assert_eq(mode_type(&cfg, 'f', 1), MODE_FLAG_PREFIX);
	assert_eq(mode_type(&cfg, 'f', 0), MODE_FLAG_PREFIX);
}

int
main(void)
{
	struct testcase tests[] = {
		TESTCASE(test_mode_bit),
		TESTCASE(test_mode_str),
		TESTCASE(test_mode_chanmode_set),
		TESTCASE(test_mode_prfxmode_set),
		TESTCASE(test_mode_usermode_set),
		TESTCASE(test_mode_cfg_usermodes),
		TESTCASE(test_mode_cfg_chanmodes),
		TESTCASE(test_mode_cfg_subtypes),
		TESTCASE(test_mode_cfg_prefix),
		TESTCASE(test_mode_type)
	};

	return run_tests(NULL, NULL, tests);
}
