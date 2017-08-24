#include "test.h"
#include "../src/server.c"
#include "../src/utils.c"   /* skip_sp */

void newlinef(struct channel *c, enum buffer_line_t t, const char *f, const char *m, ...)
{
	/* Mock */

	UNUSED(c);
	UNUSED(t);
	UNUSED(f);
	UNUSED(m);
}

static void
test_parse_opt(void)
{
	/* Test numeric 005 parsing  */

	struct opt opt;

	char *ptr;

	char opts0[] = "";
	ptr = opts0;
	assert_eq(parse_opt(&opt, &ptr), 0);

	char opts1[] = "=";
	ptr = opts1;
	assert_eq(parse_opt(&opt, &ptr), 0);

	char opts2[] = " ";
	ptr = opts2;
	assert_eq(parse_opt(&opt, &ptr), 0);

	char opts3[] = "=TESTING";
	ptr = opts3;
	assert_eq(parse_opt(&opt, &ptr), 0);

	char opts4[] = ":test,trailing";
	ptr = opts4;
	assert_eq(parse_opt(&opt, &ptr), 0);

#define CHECK(R, A, V) \
	assert_eq(parse_opt(&opt, &ptr), R); assert_strcmp(opt.arg, A); assert_strcmp(opt.val, V);

	char opts5[] = "TESTING";
	ptr = opts5;
	CHECK(1, "TESTING", NULL);
	CHECK(0, NULL,      NULL);

	char opts6[] = " TESTING ";
	ptr = opts6;
	CHECK(1, "TESTING", NULL);
	CHECK(0, NULL,      NULL);

	char opts7[] = "TESTING=";
	ptr = opts7;
	CHECK(1, "TESTING", NULL);
	CHECK(0, NULL,      NULL);

	char opts8[] = "TESTING1=TESTING2";
	ptr = opts8;
	CHECK(1, "TESTING1", "TESTING2");
	CHECK(0, NULL,       NULL);

	char opts9[] = "TESTING1=TESTING2 TESTING3=";
	ptr = opts9;
	CHECK(1, "TESTING1", "TESTING2");
	CHECK(1, "TESTING3", NULL);
	CHECK(0, NULL,       NULL);

	char opts10[] = "000 1=t 2=t! 3= 4=  5=t,t, 6= 7=7 8===D 9=9 10=10 11= 12 13 ";
	ptr = opts10;

	CHECK(1, "000", NULL);
	CHECK(1, "1",   "t");
	CHECK(1, "2",   "t!");
	CHECK(1, "3",   NULL);
	CHECK(1, "4",   NULL);
	CHECK(1, "5",   "t,t,");
	CHECK(1, "6",   NULL);
	CHECK(1, "7",   "7");
	CHECK(1, "8",   "==D");
	CHECK(1, "9",   "9");
	CHECK(1, "10",  "10");
	CHECK(1, "11",  NULL);
	CHECK(1, "12",  NULL);
	CHECK(1, "13",  NULL);
	CHECK(0, NULL,  NULL);

#undef CHECK
}

static void
test_set_PREFIX(void)
{
	struct server s;

	char *ptr;

	char s0[] = "abcd)!@#$";
	ptr = s0;
	assert_eq(set_PREFIX(&s, ptr), 0);

	char s1[] = "(abcd!@#$";
	ptr = s1;
	assert_eq(set_PREFIX(&s, ptr), 0);

	char s2[] = "(abcd)@#$";
	ptr = s2;
	assert_eq(set_PREFIX(&s, ptr), 0);

	char s3[] = "(abcdefghijklmnopqrstuvwxyza)!@#$";
	ptr = s3;
	assert_eq(set_PREFIX(&s, ptr), 0);

	char s4[] = "!@#$(abcdefghijklmnopqrstuvwxyza)";
	ptr = s4;
	assert_eq(set_PREFIX(&s, ptr), 0);

	char s5[] = "(abcd)!@#$";
	ptr = s5;
	assert_eq(set_PREFIX(&s, ptr), 1);
	assert_strcmp(s.config.PREFIX.F, "abcd");
	assert_strcmp(s.config.PREFIX.T, "!@#$");
}

static void
test_server_get_prefix(void)
{
	struct server s = {
		.config.PREFIX.F = "abc",
		.config.PREFIX.T = "123"
	};

	/* Test lower mode flag doesn't take prescedence */
	assert_eq(server_get_prefix(&s, '1', 'c'), '1');

	/* Test higher mode flag takes prescedence */
	assert_eq(server_get_prefix(&s, '3', 'b'), '2');

	/* Test abscent prefix */
	assert_eq(server_get_prefix(&s, 0, 'b'), '2');

	/* Test new mode not in PREFIX config */
	assert_eq(server_get_prefix(&s, '3', 'd'), '3');

	/* Test abscent prefix and new mode not in PREFIX config */
	assert_eq(server_get_prefix(&s, 0, 'd'), 0);
}

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_parse_opt),
		TESTCASE(test_set_PREFIX),
		TESTCASE(test_server_get_prefix)
	};

	return run_tests(tests);
}
