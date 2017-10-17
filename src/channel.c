#include <stdlib.h>
#include <string.h>

#include "channel.h"
#include "utils.h"

static inline int channel_cmp(struct channel*, struct channel*);

SPLAY_GENERATE(channel_list, channel, node, channel_cmp)

static inline int
channel_cmp(struct channel *c1, struct channel *c2)
{
	/* TODO: CASEMAPPING, as ftpr held by the server */
	return irc_strcmp(c1->name.str, c2->name.str);
}

struct channel*
channel(const char *name)
{
	struct channel *c;

	if ((c = calloc(1, sizeof(*c))) == NULL)
		fatal("calloc", errno);

	string(&(c->name), name);

	return c;
}

void
channel_free(struct channel *c)
{
	free(c->name.str);
	free(c);
}

struct channel*
channel_list_add(struct channel_list *cl, struct channel *c)
{
	return SPLAY_ADD(channel_list, cl, c);
}

struct channel*
channel_list_del(struct channel_list *cl, struct channel *c)
{
	return SPLAY_DEL(channel_list, cl, c);
}

struct channel*
channel_list_get(struct channel_list *cl, char *name)
{
	struct channel _chan = { .name = { .str = name } };

	return SPLAY_GET(channel_list, cl, &_chan);
}
