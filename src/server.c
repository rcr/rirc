#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "state.h"
#include "utils.h"

struct opt
{
	char *arg;
	char *val;
};

static int parse_opt(struct opt*, char **);

#define HANDLED_005 \
	X(CHANMODES)    \
	X(PREFIX)

#define X(cmd) static int server_set_##cmd(struct server*, char*);
HANDLED_005
#undef X

//TODO: refactor, not currently used
struct server*
server(char *host, char *port, char *nicks)
{
	struct server *s;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal("calloc", errno);

	s->host = strdup(host);
	s->port = strdup(port);

	s->nicks = strdup(nicks);

	mode_config(&(s->mode_config), NULL, MODE_CONFIG_DEFAULTS);

	return s;
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

	enum mode_err err;

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

#ifdef DEBUG
		if (opt.val == NULL)
			newlinef(s->channel, 0, "DEBUG", "Setting numeric 005 %s", opt.arg);
		else
			newlinef(s->channel, 0, "DEBUG", "Setting numeric 005 %s: %s", opt.arg, opt.val);
#endif

		#define X(cmd) \
		if (!strcmp(opt.arg, #cmd) && server_set_##cmd(s, opt.val)) \
			newlinef(s->channel, 0, "-!!-", "invalid %s: %s", #cmd, opt.val);
		HANDLED_005
		#undef X
	}
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

	enum mode_err err;

	err = mode_config(&(s->mode_config), val, MODE_CONFIG_SUBTYPES);

	return (err != MODE_ERR_NONE);
}

static int
server_set_PREFIX(struct server *s, char *val)
{
	/* Delegated to mode.c  */

	enum mode_err err;

	err = mode_config(&(s->mode_config), val, MODE_CONFIG_PREFIX);

	return (err != MODE_ERR_NONE);
}
