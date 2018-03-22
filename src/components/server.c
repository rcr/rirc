#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "src/components/server.h"
#include "src/state.h"
#include "src/utils/utils.h"

/* TODO: CASEMAPPING (ascii, rfc1459, strict-rfc1459, set server fptr) */
#define HANDLED_005 \
	X(CHANMODES)    \
	X(PREFIX)       \
	X(MODES)

struct opt
{
	char *arg;
	char *val;
};

static int parse_opt(struct opt*, char**);
static int server_cmp(const struct server*, const struct server*);
static struct server* server_list_get(struct server_list*, struct server*);

#define X(cmd) static int server_set_##cmd(struct server*, char*);
HANDLED_005
#undef X

struct server*
server(const char *host, const char *port, const char *pass)
{
	struct server *s;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal("calloc", errno);

	s->host = strdup(host);
	s->port = strdup(port);

	if (pass)
		s->pass = strdup(pass);

	s->usermodes_str.type = MODE_STR_USERMODE;
	mode_config(&(s->mode_config), NULL, MODE_CONFIG_DEFAULTS);

	return s;
}

static struct server*
server_list_get(struct server_list *sl, struct server *s)
{
	struct server *tmp;

	if ((tmp = sl->head) == NULL)
		return NULL;

	if (!server_cmp(sl->head, s))
		return sl->head;

	while ((tmp = tmp->next) != sl->head) {

		if (!server_cmp(tmp, s))
			return tmp;
	}

	return NULL;
}

struct server*
server_list_add(struct server_list *sl, struct server *s)
{
	struct server *tmp;

	if ((tmp = server_list_get(sl, s)) != NULL)
		return s;

	if (sl->head == NULL) {
		sl->head = s->next = s;
		sl->tail = s->prev = s;
	} else {
		s->next = sl->tail->next;
		s->prev = sl->tail;
		sl->head->prev = s;
		sl->tail->next = s;
		sl->tail = s;
	}

	return NULL;
}

struct server*
server_list_del(struct server_list *sl, struct server *s)
{
	struct server *tmp_h,
	              *tmp_t;

	if (sl->head == s &&  sl->tail == s) {
		/* Removing last server */
		sl->head = NULL;
		sl->tail = NULL;

	} else if ((tmp_h = sl->head) == s) {
		/* Removing head */
		sl->head = sl->head->next;
		sl->head->prev = sl->tail;

	} else if ((tmp_t = sl->tail) == s) {
		/* Removing tail */
		sl->tail = sl->tail->prev;
		sl->tail->next = sl->head;

	} else {
		/* Removing some server (head, tail) */
		while ((tmp_h = tmp_h->next) != s) {
			if (tmp_h == tmp_t)
				return NULL;
		}
		s->next->prev = s->prev;
		s->prev->next = s->next;
	}

	s->next = NULL;
	s->prev = NULL;

	return s;
}

void
server_free(struct server *s)
{
	free(s->host);
	free(s->port);
	free(s->nicks);
	free(s);
}

/* TODO:
	should return int, 005 as well
 */
void
server_set_004(struct server *s, char *str)
{
	/* <server_name> <version> <user_modes> <chan_modes> */

	struct channel *c = s->channel;

	char *server_name, /* Not used */
	     *version,     /* Not used */
	     *user_modes,  /* Configure server usermodes */
	     *chan_modes;  /* Configure server chanmodes */

	if (!(server_name = getarg(&str, " ")))
		newline(c, 0, "-!!-", "invalid numeric 004: server_name is null");

	if (!(version = getarg(&str, " ")))
		newline(c, 0, "-!!-", "invalid numeric 004: version is null");

	if (!(user_modes = getarg(&str, " ")))
		newline(c, 0, "-!!-", "invalid numeric 004: user_modes is null");

	if (!(chan_modes = getarg(&str, " ")))
		newline(c, 0, "-!!-", "invalid numeric 004: chan_modes is null");

	enum mode_err_t err;

	if (user_modes) {

#ifdef DEBUG
		newlinef(c, 0, "DEBUG", "Setting numeric 004 user_modes: %s", user_modes);
#endif

		err = mode_config(&(s->mode_config), user_modes, MODE_CONFIG_USERMODES);

		if (err != MODE_ERR_NONE)
			newlinef(c, 0, "-!!-", "invalid numeric 004 user_modes: %s", user_modes);
	}

	if (chan_modes) {

#ifdef DEBUG
		newlinef(c, 0, "DEBUG", "Setting numeric 004 chan_modes: %s", chan_modes);
#endif

		err = mode_config(&(s->mode_config), chan_modes, MODE_CONFIG_CHANMODES);

		if (err != MODE_ERR_NONE)
			newlinef(c, 0, "-!!-", "invalid numeric 004 chan_modes: %s", chan_modes);
	}
}

void
server_set_005(struct server *s, char *str)
{
	/* Iterate over options parsed from str and set for server s */

	struct opt opt;

	while (parse_opt(&opt, &str)) {
		#define X(cmd) \
		if (!strcmp(opt.arg, #cmd) && server_set_##cmd(s, opt.val)) \
			newlinef(s->channel, 0, "-!!-", "invalid %s: %s", #cmd, opt.val);
		HANDLED_005
		#undef X
	}
}

int
server_set_chans(struct server *s, const char *chans)
{
	/* TODO: parse comma seperated list
	 *       test
	 */
	(void)s;
	(void)chans;
	return 0;
}

int
server_set_nicks(struct server *s, const char *nicks)
{
	/* TODO: parse comma seperated list
	 *       test
	 */
	(void)s;
	(void)nicks;
	return 0;
}

static int
server_cmp(const struct server *s1, const struct server *s2)
{
	int cmp;

	if (s1 == s2)
		return 0;

	if ((cmp = strcmp(s1->host, s2->host)))
		return cmp;

	if ((cmp = strcmp(s1->port, s2->port)))
		return cmp;

	return 0;
}

static int
parse_opt(struct opt *opt, char **str)
{
	/* Parse a single argument from numeric 005 (ISUPPORT)
	 *
	 * docs/ISUPPORT.txt, section 2
	 *
	 * ":" servername SP "005" SP nickname SP 1*13( token SP ) ":are supported by this server"
	 *
	 * token     =  *1"-" parameter / parameter *1( "=" value )
	 * parameter =  1*20letter
	 * value     =  *letpun
	 * letter    =  ALPHA / DIGIT
	 * punct     =  %d33-47 / %d58-64 / %d91-96 / %d123-126
	 * letpun    =  letter / punct
	 */

	char *t, *p = *str;

	opt->arg = NULL;
	opt->val = NULL;

	if (!skip_sp(&p))
		return 0;

	if (!isalnum(*p))
		return 0;

	opt->arg = p;

	if ((t = strchr(p, ' '))) {
		*t++ = 0;
		*str = t;
	} else {
		*str = strchr(p, 0);
	}

	if ((p = strchr(opt->arg, '='))) {
		*p++ = 0;

		if (*p)
			opt->val = p;
	}

	return 1;
}

static int
server_set_CHANMODES(struct server *s, char *val)
{
	/* Delegated to mode.c  */

#ifdef DEBUG
			newlinef(s->channel, 0, "DEBUG", "Setting numeric 005 CHANMODES: %s", val);
#endif

	enum mode_err_t err;

	err = mode_config(&(s->mode_config), val, MODE_CONFIG_SUBTYPES);

	return (err != MODE_ERR_NONE);
}

static int
server_set_PREFIX(struct server *s, char *val)
{
	/* Delegated to mode.c  */

#ifdef DEBUG
			newlinef(s->channel, 0, "DEBUG", "Setting numeric 005 PREFIX: %s", val);
#endif

	enum mode_err_t err;

	err = mode_config(&(s->mode_config), val, MODE_CONFIG_PREFIX);

	return (err != MODE_ERR_NONE);
}

static int
server_set_MODES(struct server *s, char *val)
{
	/* Delegated to mode.c */

#ifdef DEBUG
			newlinef(s->channel, 0, "DEBUG", "Setting numeric 005 MODES: %s", val);
#endif

	enum mode_err_t err;

	err = mode_config(&(s->mode_config), val, MODE_CONFIG_MODES);

	return (err != MODE_ERR_NONE);
}
