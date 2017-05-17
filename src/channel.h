#ifndef CHANNEL_H
#define CHANNEL_H

#include "nicklist.h"
#include "buffer.h"

#define MODE_SIZE (26 * 2) + 1 /* Supports modes [az-AZ] */

/* nav activity types */
typedef enum
{
	ACTIVITY_DEFAULT,
	ACTIVITY_ACTIVE,
	ACTIVITY_PINGED,
	ACTIVITY_T_SIZE
} activity_t;

/* Channel */
struct channel
{
	activity_t active;
	char *name;
	char type_flag;
	char chanmodes[MODE_SIZE];
	int parted;
	struct buffer buffer;
	struct channel *next;
	struct channel *prev;
	struct input *input;
	struct nicklist nicklist;
	struct server *server;
	SPLAY_NODE(channel) node;
};

struct channel_list
{
	SPLAY_HEAD(channel);
};

struct channel* channel_list_add(struct channel_list*, struct channel*);
struct channel* channel_list_del(struct channel_list*, char*);
struct channel* channel_list_get(struct channel_list*, char*);

#endif
