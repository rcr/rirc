#include <string.h>

#include "channel.h"
#include "utils.h"

static inline int channel_cmp(struct channel*, struct channel*);

SPLAY_GENERATE(channel_list, channel, node, channel_cmp)

static inline int
channel_cmp(struct channel *c1, struct channel *c2)
{
	struct string *n1 = c1->name;
	struct string *n2 = c2->name;

	return (n1->len == n2->len) && irc_strcmp(n1->str, n2->str);
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
	struct string _name = { .str = name, .len = strlen(name) };
	struct channel _chan = { .name = &_name };

	return SPLAY_GET(channel_list, cl, &_chan);
}
