#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"
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

#define X(cmd) static void set_##cmd(struct server*, char*);
HANDLED_005
#undef X

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

	return s;
}

void
server_set_005(struct server *s, char *str)
{
	/* Iterate over options parsed from str and set for server s */

	struct opt opt;

	while (parse_opt(&opt, &str)) {
		#define X(cmd) if (!strcmp(opt.arg, #cmd)) { set_##cmd(s, opt.val); }
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

static void
set_CHANMODES(struct server *s, char *val)
{
	UNUSED(s);

	if (val) {
		; //set val
	} else {
		; //set default
	}
}

static void
set_PREFIX(struct server *s, char *val)
{
	UNUSED(s);

	if (val) {
		; //set val
	} else {
		; //set default
	}
}

#if 0
int
parse_N005(struct opt *opts, size_t max_opts, char *str)
{
	/* Parse server configuration received in numeric 005 (ISUPPORT)
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

	char c, *arg, *val;

	size_t opt_i = 0;

	while (skip_sp(&str) && opt_i < max_opts) {

		if (!isalnum(*str))
			return 0;

		arg = str;
		val = NULL;

		while ((c = *str) && c != '=' && c != ' ')
			str++;

		if (c)
			*str++ = 0;

		if (c == '=') {

			if (*str && *str != ' ')
				val = str;

			for (; *str && *str != ' '; str++)
				;

			if (*str == ' ')
				*str++ = 0;
		}

		opts[opt_i].arg = arg;
		opts[opt_i].val = val;

		opt_i++;
	}

	//FIXME: this is setting opt_i == 14

	opts[opt_i].arg = NULL;
	opts[opt_i].val = NULL;

	return !!opt_i;
}
#endif
