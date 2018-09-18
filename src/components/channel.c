#include <stdlib.h>
#include <string.h>

#include "src/components/channel.h"
#include "src/utils/utils.h"

static inline int channel_cmp(struct channel*, struct channel*);

SPLAY_GENERATE(channel_list, channel, node, channel_cmp)

static inline int
channel_cmp(struct channel *c1, struct channel *c2)
{
	/* TODO: CASEMAPPING, as ftpr held by the server */
	return irc_strcmp(c1->name.str, c2->name.str);
}

struct channel*
channel(const char *name, enum channel_t type)
{
	struct channel *c;

	size_t len = strlen(name);

	if ((c = calloc(1, sizeof(*c) + len + 1)) == NULL)
		fatal("calloc", errno);

	c->chanmodes_str.type = MODE_STR_CHANMODE;
	c->input = new_input();
	c->name.len = len;
	c->name.str = memcpy(c->_, name, len + 1);
	c->type = type;

	return c;
}

void
channel_free(struct channel *c)
{
	user_list_free(&(c->users));
	free_input(c->input);
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
channel_list_get(struct channel_list *cl, const char *name)
{
	struct channel _chan = { .name = { .str = name } };

	return SPLAY_GET(channel_list, cl, &_chan);
}
