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

//TODO: refactor, not used
struct server*
server(char *host, char *port, char *nicks)
{
	struct server *s;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal("calloc", errno);

	//TODO: user/host/port/modes/flag can be added as a user struct
	s->host = strdup(host);
	s->port = strdup(port);

	s->nicks = strdup(nicks);

	/* Set server defaults */
	#define X(cmd) set_##cmd(s, NULL);
	HANDLED_005
	#undef X

	return s;
}

void
server_set_005(struct server *s, char *str)
{
	/* Iterate over options parsed from str and set for server s */

	struct opt opt;

	/* TODO: gperf */
	while (parse_opt(&opt, &str)) {
		#define X(cmd)                                        \
		if (!strcmp(opt.arg, #cmd) && !set_##cmd(s, opt.val)) \
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
set_CHANMODES(struct server *s, char *val)
{
	UNUSED(s);

	if (val) {
		; //set val
	} else {
		; //set default
	}

	return 1;
}

static int
set_PREFIX(struct server *s, char *val)
{
	/* `(modes)prefixes` in order of precedence
	 *
	 * RFC 2812, numeric 319 (RPL_WHOISCHANNELS) states `(@+)ov`
	 */

	char *f, *t = val;

	if (val == NULL) {
		strncpy(s->config.PREFIX.F, "ov", sizeof(s->config.PREFIX.F) - 1);
		strncpy(s->config.PREFIX.T, "@+", sizeof(s->config.PREFIX.T) - 1);
		return 1;
	}

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

	strcpy(s->config.PREFIX.F, f);
	strcpy(s->config.PREFIX.T, t);

	return 1;
}

char
server_get_prefix(struct server *s, char prefix, char mode)
{
	/* Return the most prescedent user prefix, given a current prefix
	 * and a new mode flag */

	int from = 0, to = 0;

	char *prefix_f = s->config.PREFIX.F,
	     *prefix_t = s->config.PREFIX.T;

	while (*prefix_f && *prefix_f++ != mode)
		from++;

	while (*prefix_t && *prefix_t++ != prefix)
		to++;

	return (from < to) ? s->config.PREFIX.T[from] : prefix;
}
