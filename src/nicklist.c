#include "nicklist.h"

#define UNUSED(X) ((void)(X))

int
nicklist_add(struct nicklist *l, char *nick)
{
	UNUSED(l);
	UNUSED(nick);

	return 0;
}

int
nicklist_del(struct nicklist *l, char *nick)
{
	UNUSED(l);
	UNUSED(nick);

	return 0;
}

struct nick*
nicklist_get(struct nicklist *l, char *nick, size_t len)
{
	UNUSED(l);
	UNUSED(nick);
	UNUSED(len);

	return NULL;
}

void
nicklist_free(struct nicklist *l)
{
	UNUSED(l);
}
