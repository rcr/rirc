#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "src/components/server.h"
#include "src/state.h"
#include "src/utils/utils.h"

#define HANDLED_005 \
	X(CASEMAPPING)  \
	X(CHANMODES)    \
	X(MODES)        \
	X(PREFIX)

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
server(const char *host, const char *port, const char *pass, const char *user, const char *real)
{
	struct server *s;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal("calloc", errno);

	s->host = strdup(host);
	s->port = strdup(port);
	s->pass = pass ? strdup(pass) : NULL;
	s->username = strdup(user);
	s->realname = strdup(real);

	s->usermodes_str.type = MODE_STR_USERMODE;

	mode_config(&(s->mode_config), NULL, MODE_CONFIG_DEFAULTS);

	// FIXME: channel()
	s->channel = new_channel(host, s, NULL, BUFFER_SERVER);
	channel_set_current(s->channel);

	// FIXME:
	if ((s->connection = connection(s, host, port)) == NULL)
		goto err;

	return s;

err:
	server_free(s);
	return NULL;
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
	free((void *)s->host);
	free((void *)s->port);
	free((void *)s->pass);
	free((void *)s->nicks);
	free((void *)s->username);
	free((void *)s->realname);
	free(s);
}

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

	if (user_modes) {

		DEBUG_MSG("Setting numeric 004 user_modes: %s", user_modes);

		if (mode_config(&(s->mode_config), user_modes, MODE_CONFIG_USERMODES) != MODE_ERR_NONE)
			newlinef(c, 0, "-!!-", "invalid numeric 004 user_modes: %s", user_modes);
	}

	if (chan_modes) {

		DEBUG_MSG("Setting numeric 004 chan_modes: %s", chan_modes);

		if (mode_config(&(s->mode_config), chan_modes, MODE_CONFIG_CHANMODES) != MODE_ERR_NONE)
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

	/* FIXME: (see docs)
	 *
	 * '-PARAMETER' is valid and negates a previously set parameter to its default
	 * 'PARAMETER', 'PARAMTER=' are equivalent
	 *
	 * The parameter's value may contain sequences of the form "\xHH", where
	 * HH is a two-digit hexadecimal number.  Each such sequence is
	 * considered identical to the equivalent octet after parsing of the
	 * reply into separate tokens has occurred.
	 *
	 * [Example: X=A\x20B defines one token, "X", with the value "A B",
	 * rather than two tokens "X" and "B".]
	 * [Note: The literal string "\x" must therefore be encoded as
	 * "\x5Cx".]
	 *
	 * If the server has not advertised a CHARSET parameter, it MUST not use
	 * such sequences with a value outside those permitted by the above ABNF
	 * grammar, with the exception of "\x20";  if it has advertised CHARSET,
	 * then it may in addition send any printable character defined in that
	 * encoding. Characters in multibyte encodings such as UTF-8 should be
	 * sent as a series of \x sequences.
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
server_set_CASEMAPPING(struct server *s, char *val)
{
	/* TODO: sets a function pointer to be used for
	 * nick/chan string cmps specific to this server */
	(void)(s);
	(void)(val);
	return 0;
}

static int
server_set_CHANMODES(struct server *s, char *val)
{
	/* Delegated to mode.c  */

	DEBUG_MSG("Setting numeric 005 CHANMODES: %s", val);

	return (mode_config(&(s->mode_config), val, MODE_CONFIG_SUBTYPES) != MODE_ERR_NONE);
}

static int
server_set_MODES(struct server *s, char *val)
{
	/* Delegated to mode.c */

	DEBUG_MSG("Setting numeric 005 MODES: %s", val);

	return (mode_config(&(s->mode_config), val, MODE_CONFIG_MODES) != MODE_ERR_NONE);
}

static int
server_set_PREFIX(struct server *s, char *val)
{
	/* Delegated to mode.c  */

	DEBUG_MSG("Setting numeric 005 PREFIX: %s", val);

	return (mode_config(&(s->mode_config), val, MODE_CONFIG_PREFIX) != MODE_ERR_NONE);
}
