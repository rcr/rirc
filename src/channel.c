#include "channel.h"
#include "utils.h"

static inline int channel_cmp(struct channel*, struct channel*);

SPLAY_GENERATE(channel_list, channel, node, channel_cmp)

struct channel*
channel_list_add(struct channel_list *cl, struct channel *c)
{
	return SPLAY_ADD(channel_list, cl, c);
}

struct channel*
channel_list_del(struct channel_list *cl, char *name)
{
	struct channel _ = { .name = name };

	return SPLAY_DEL(channel_list, cl, &_);
}

struct channel*
channel_list_get(struct channel_list *cl, char *name)
{
	struct channel _ = { .name = name };

	return SPLAY_GET(channel_list, cl, &_);
}

static inline int
channel_cmp(struct channel *c1, struct channel *c2)
{
	return irc_strcmp(c1->name, c2->name);
}
