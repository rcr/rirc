#include "test/test.h"
#include "src/components/mode.c"

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

	struct mode m = MODE_EMPTY;
	struct mode_str str;

	memset(&str, 0, sizeof(struct mode_str));

	/* mode_str type not set */
	assert_fatal(mode_str(&m, &str));

	str.type = -1;

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
test_mode_chanmode_set(void)
{
	/* Test setting/unsetting chanmode flags */

	struct mode m = MODE_EMPTY;
	struct mode_cfg cfg;
	struct mode_str str = { .type = MODE_STR_CHANMODE };

	mode_cfg_chanmodes(&cfg, "abcdefghijsp");
	mode_cfg_subtypes(&cfg, "abc,def,ghi,jsp");

	/* Test setting/unsetting invalid chanmode flag */
	assert_eq(mode_chanmode_set(&m, &cfg, 'z', 1), MODE_ERR_INVALID_FLAG);
	assert_strcmp(mode_str(&m, &str), "");

	assert_eq(mode_chanmode_set(&m, &cfg, 'z', 0), MODE_ERR_INVALID_FLAG);
	assert_strcmp(mode_str(&m, &str), "");

	/* Test valid CHANMODE subtype A doesn't set flag */
	assert_eq(mode_chanmode_set(&m, &cfg, 'a', 1), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "");

	/* Test valid CHANMODE subtypes B,C,D set flags */
	assert_eq(mode_chanmode_set(&m, &cfg, 'd', 1), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "d");

	assert_eq(mode_chanmode_set(&m, &cfg, 'e', 1), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "de");

	assert_eq(mode_chanmode_set(&m, &cfg, 'j', 1), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "dej");

	/* test unsetting flags */
	assert_eq(mode_chanmode_set(&m, &cfg, 's', 0), MODE_ERR_NONE);
	assert_eq(mode_chanmode_set(&m, &cfg, 'j', 0), MODE_ERR_NONE);
	assert_eq(mode_chanmode_set(&m, &cfg, 'e', 0), MODE_ERR_NONE);
	assert_eq(mode_chanmode_set(&m, &cfg, 'd', 0), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "");
}

static void
test_mode_prfxmode_prefix(void)
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
test_mode_prfxmode_set(void)
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
	assert_eq(mode_prfxmode_set(&m, &cfg, 'd', 1), MODE_ERR_INVALID_FLAG);
	assert_eq(m.prefix, 0);
	assert_eq(mode_prfxmode_set(&m, &cfg, 'd', 0), MODE_ERR_INVALID_FLAG);
	assert_eq(m.prefix, 0);

	/* Test setting valid flags respects PREFIX precedence */
	assert_eq(mode_prfxmode_set(&m, &cfg, 'b', 1), MODE_ERR_NONE);
	assert_eq(m.prefix, '2');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'c', 1), MODE_ERR_NONE);
	assert_eq(m.prefix, '2');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'a', 1), MODE_ERR_NONE);
	assert_eq(m.prefix, '1');

	/* Test unsetting valid flags respects PREFIX precedence */
	assert_eq(mode_prfxmode_set(&m, &cfg, 'b', 0), MODE_ERR_NONE);
	assert_eq(m.prefix, '1');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'a', 0), MODE_ERR_NONE);
	assert_eq(m.prefix, '3');
	assert_eq(mode_prfxmode_set(&m, &cfg, 'c', 0), MODE_ERR_NONE);
	assert_eq(m.prefix, 0);
}

static void
test_mode_usermode_set(void)
{
	/* Test setting/unsetting usermode flag and mode string */

	struct mode m = MODE_EMPTY;
	struct mode_str str = { .type = MODE_STR_USERMODE };
	struct mode_cfg cfg;

	mode_cfg_usermodes(&cfg, "azAZ");

	/* Test setting invalid usermode flag */
	assert_eq(mode_usermode_set(&m, &cfg, 'b', 1), MODE_ERR_INVALID_FLAG);

	/* Test setting valid flags */
	assert_eq(mode_usermode_set(&m, &cfg, 'a', 1), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &cfg, 'Z', 1), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "aZ");

	assert_eq(mode_usermode_set(&m, &cfg, 'z', 1), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &cfg, 'A', 1), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "azAZ");

	/* Test unsetting invalid usermode flag */
	assert_eq(mode_usermode_set(&m, &cfg, 'c', 0), MODE_ERR_INVALID_FLAG);

	/* Test unsetting valid flags */
	assert_eq(mode_usermode_set(&m, &cfg, 'z', 0), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &cfg, 'Z', 0), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "aA");

	assert_eq(mode_usermode_set(&m, &cfg, 'a', 0), MODE_ERR_NONE);
	assert_eq(mode_usermode_set(&m, &cfg, 'A', 0), MODE_ERR_NONE);
	assert_strcmp(mode_str(&m, &str), "");
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
	CHECK("", "", "", "");

	/* Test invalid flags */
	assert_eq(mode_cfg_subtypes(&cfg, "!!abc,d123e,fg!-@,^&"), MODE_ERR_INVALID_CONFIG);
	CHECK("", "", "", "");

	const char *all_flags =
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS ","
		ALL_LOWERS ALL_UPPERS;

	/* Test valid string */
	assert_eq(mode_cfg_subtypes(&cfg, all_flags), MODE_ERR_NONE);
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
test_mode_type(void)
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
		TESTCASE(test_mode_prfxmode_prefix),
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
