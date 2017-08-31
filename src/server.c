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

#define X(cmd) static int set_##cmd(struct server*, char*);
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

	/* Set server defaults */
	#define X(cmd) set_##cmd(s, NULL);
	HANDLED_005
	#undef X

	mode_config_defaults(&(s->mode_config));

	return s;
}

void
server_set_005(struct server *s, char *str)
{
	/* Iterate over options parsed from str and set for server s */

	struct opt opt;

	while (parse_opt(&opt, &str)) {
		#define X(cmd)                                        \
		if (!strcmp(opt.arg, #cmd) && !set_##cmd(s, opt.val)) \
			newlinef(s->channel, 0, "-!!-", "invalid %s: %s", #cmd, opt.val);
		HANDLED_005
		#undef X
	}
}

void
server_set_004(struct server *s, char *str)
{
	/* <server_name> <version> <user_modes> <chan_modes> */

	//TODO
	(void)(s);
	(void)(str);
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
set_CHANMODES(struct server *s, char *val)
{
	UNUSED(s);

	/* Server defaults are set by mode_defaults */
	if (val == NULL)
		return 1;

	/* TODO, parse CHANMODES */

	return 1;
}

static int
set_PREFIX(struct server *s, char *val)
{
	/* `(modes)prefixes` in order of precedence */

	char *f, *t = val;

	/* Server defaults are set by mode_defaults */
	if (val == NULL)
		return 1;

	if ((t = strchr(t, '(')) == NULL)
		return 0;

	f = ++t;

	if ((t = strchr(t, ')')) == NULL)
		return 0;

	*t++ = 0;

	if ((strlen(f) != strlen(t)))
		return 0;

	if (strlen(f) > MODE_LEN)
		return 0;

	strcpy(s->mode_config.PREFIX.F, f);
	strcpy(s->mode_config.PREFIX.T, t);

	return 1;
}
