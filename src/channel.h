#ifndef CHANNEL_H
#define CHANNEL_H

#include "buffer.h"
#include "tree.h"
#include "user.h"

#define MODE_SIZE (26 * 2) + 1 /* Supports modes [az-AZ] */

/* Channel activity types, in order of precedence */
enum activity_t
{
	ACTIVITY_DEFAULT, /* Default activity */
	ACTIVITY_JPQ,     /* Join/Part/Quit activity */
	ACTIVITY_ACTIVE,  /* Chat activity */
	ACTIVITY_PINGED,  /* Ping activity */
	ACTIVITY_T_SIZE
};

struct channel
{
	char chanmodes[MODE_SIZE];
	char *name;
	char type_flag;
	enum activity_t activity;
	int parted;
	SPLAY_NODE(channel) node; /* Fast unordered retrieval */
	struct buffer buffer;
	struct channel *next;
	struct channel *prev;
	struct input *input;
	struct server *server;
	struct user_list users;
};

struct channel_list
{
	SPLAY_HEAD(channel);
};

struct channel* channel_list_add(struct channel_list*, struct channel*);
struct channel* channel_list_del(struct channel_list*, struct channel*);
struct channel* channel_list_get(struct channel_list*, char*);

//TODO: channel/free channel_list/free
//TODO: name##_TREE_FREE

#endif
