#include "test/test.h"
#include "src/components/server.c"
#include "src/components/mode.c"   /* mode_config_defaults */
#include "src/utils/utils.c"       /* skip_sp */

void
channel_set_current(struct channel *c)
{
	UNUSED(c);
}

struct channel*
new_channel(const char *n, struct server *s, struct channel *l, enum buffer_t t)
{
	/* FIXME: mock new_channel until channel() is implemented */

	UNUSED(n);
	UNUSED(s);
	UNUSED(l);
	UNUSED(t);
	return NULL;
}

struct connection*
connection(const void *s, const char *h, const char *p)
{
	/* Mock */

	UNUSED(s);
	UNUSED(h);
	UNUSED(p);
	return (void *)(s);
}

void
newline(struct channel *c, enum buffer_line_t t, const char *f, const char *m)
{
	/* Mock */

	UNUSED(c);
	UNUSED(t);
	UNUSED(f);
	UNUSED(m);
}

void
newlinef(struct channel *c, enum buffer_line_t t, const char *f, const char *m, ...)
{
	/* Mock */

	UNUSED(c);
	UNUSED(t);
	UNUSED(f);
	UNUSED(m);
}

static void
test_server_list(void)
{
	/* Test add/del/get servers */

	struct server_list servers;
	struct server *s1 = NULL,
	              *s2 = NULL,
	              *s3 = NULL,
	              *s4 = NULL,
	              *s5 = NULL,
	              *s6 = NULL,
	              *s7 = NULL;

	memset(&servers, 0, sizeof(servers));

	s1 = server("host1", "port1", NULL, "", "");
	s2 = server("host1", "port1", "foo1", "", ""); /* duplicate host, port (s1) */
	s3 = server("host1", "port2", "foo2", "", ""); /* duplicate host (s1), different port */
	s4 = server("host2", "port1", "foo3", "", ""); /* duplicate port (s1), different host */
	s5 = server("host2", "port2", NULL, "", "");   /* duplicate host (s4), duplicate port (s3) */
	s6 = server("host1", "port2", NULL, "", "");   /* duplicate host, port (s4) */
	s7 = server("host2", "port1", NULL, "", "");   /* duplicate host, port (s5) */

	/* Test add */
	assert_ptrequals(server_list_add(&servers, s1), NULL);
	assert_ptrequals(server_list_add(&servers, s1), s1); /* duplicate pointer */
	assert_ptrequals(server_list_add(&servers, s2), s2); /* duplicate by host/port */
	assert_ptrequals(server_list_add(&servers, s3), NULL);
	assert_ptrequals(server_list_add(&servers, s4), NULL);
	assert_ptrequals(server_list_add(&servers, s5), NULL);
	assert_ptrequals(server_list_add(&servers, s6), s6); /* duplicate by host/port */
	assert_ptrequals(server_list_add(&servers, s7), s7); /* duplicate by host/port */

	/* Test del */
	assert_ptrequals(server_list_del(&servers, s2), NULL); /* not in list */
	assert_ptrequals(server_list_del(&servers, s6), NULL); /* not in list */
	assert_ptrequals(server_list_del(&servers, s7), NULL); /* not in list */

	/* head              tail
	 *  v                 v
	 *  s1 -> s3 -> s4 -> s5
	 */
	assert_ptrequals(servers.head, s1);
	assert_ptrequals(servers.tail, s5);

	/* delete from middle */
	assert_ptrequals(server_list_del(&servers, s4), s4);
	assert_ptrequals(servers.head, s1);
	assert_ptrequals(servers.tail, s5);

	/* delete head */
	assert_ptrequals(server_list_del(&servers, s1), s1);
	assert_ptrequals(servers.head, s3);
	assert_ptrequals(servers.tail, s5);

	/* delete tail */
	assert_ptrequals(server_list_del(&servers, s5), s5);
	assert_ptrequals(servers.head, s3);
	assert_ptrequals(servers.tail, s3);

	/* delete last */
	assert_ptrequals(server_list_del(&servers, s3), s3);
	assert_ptrequals(servers.head, NULL);
	assert_ptrequals(servers.tail, NULL);

	/* server was removed from list */
	assert_ptrequals(s1->next, NULL);
	assert_ptrequals(s1->prev, NULL);

	/* server was never added to list */
	assert_ptrequals(s7->next, NULL);
	assert_ptrequals(s7->prev, NULL);

	server_free(s1);
	server_free(s2);
	server_free(s3);
	server_free(s4);
	server_free(s5);
	server_free(s6);
	server_free(s7);
}

static void
test_server_set_chans(void)
{
	/* TODO */
}

static void
test_server_set_nicks(void)
{
	/* TODO */
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
	assert_eq(parse_opt(&opt, &ptr), (R)); \
	assert_strcmp(opt.arg, (A)); \
	assert_strcmp(opt.val, (V));

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

int
main(void)
{
	testcase tests[] = {
		TESTCASE(test_server_list),
		TESTCASE(test_server_set_chans),
		TESTCASE(test_server_set_nicks),
		TESTCASE(test_parse_opt)
	};

	return run_tests(tests);
}
