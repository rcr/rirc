#ifndef CHANNEL_H
#define CHANNEL_H

#include "buffer.h"
#include "nicklist.h"
#include "tree.h"

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
	struct input *input;
	struct nicklist nicklist;
	struct server *server;

	SPLAY_NODE(channel) node; /* Fast unordered retrieval */
	struct channel *next;
	struct channel *prev;
	//TODO: DLL_NODE here since channels will be added to both
};

struct channel_list
{
	SPLAY_HEAD(channel);
};

struct channel* channel_list_add(struct channel_list*, struct channel*);
struct channel* channel_list_del(struct channel_list*, struct channel*);
struct channel* channel_list_get(struct channel_list*, char*);

//TODO: define macro, use the DLL, not the SPLAY stuff
//  channel_list_foreach(channel_list, channel*)   {
//
//  }

#endif
