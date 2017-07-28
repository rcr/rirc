#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "utils.h"

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
server_set_N005(struct server *s, char *str)
{
	UNUSED(s);

	struct opt *opt, opts[MAX_N005_OPTS];

	if (!parse_N005(opts, str)) {
		return; //error message
	}

	for (opt = opts; opt->arg; opt++) {
		#define X(cmd) if (!strcmp(opt->arg, #cmd)) { set_##cmd(s, opt->val); }
		HANDLED_005
		#undef X
	}
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
