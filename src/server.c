#include <stdlib.h>

#include "server.h"
#include "utils.h"

struct server*
server(struct server *s, char *host, char *port, char *nicks)
{
	if (s == NULL && (s = calloc(1, sizeof(*s))) == NULL)
		fatal("calloc");

	s->host = strdup(host);
	s->port = strdup(port);
	s->nicks = strdup(nicks);

	return s;
}
