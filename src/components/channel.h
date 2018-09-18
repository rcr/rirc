#ifndef CHANNEL_H
#define CHANNEL_H

#include "src/components/buffer.h"
#include "src/components/input.h"
#include "src/components/mode.h"
#include "src/components/user.h"
#include "src/utils/tree.h"

/* Channel activity types, in order of precedence */
enum activity_t
{
	ACTIVITY_DEFAULT, /* Default activity */
	ACTIVITY_JPQ,     /* Join/Part/Quit activity */
	ACTIVITY_ACTIVE,  /* Chat activity */
	ACTIVITY_PINGED,  /* Ping activity */
	ACTIVITY_T_SIZE
};

enum channel_t
{
	CHANNEL_T_INVALID,
	CHANNEL_T_OTHER,   /* Default/all other buffers */
	CHANNEL_T_CHANNEL, /* Channel message buffer */
	CHANNEL_T_SERVER,  /* Server message buffer */
	CHANNEL_T_PRIVATE, /* Private message buffer */
	CHANNEL_T_SIZE
};

struct channel
{
	enum channel_t type;
	enum activity_t activity;
	struct buffer buffer;
	struct channel *next;
	struct channel *prev;
	struct input *input;
	struct mode chanmodes;
	struct mode_str chanmodes_str;
	struct server *server;
	struct string name;
	struct user_list users;
	unsigned int parted : 1;
	char _[];
};

struct channel_list
{
	struct channel *head;
	struct channel *tail;
};

struct channel* channel(const char*, enum channel_t);

struct channel* channel_list_add(struct channel_list*, struct channel*);
struct channel* channel_list_del(struct channel_list*, struct channel*);
struct channel* channel_list_get(struct channel_list*, const char*);

/* TODO: `channel_tree_*` for fast retrieval
 *   struct channel_tree_node slist  -> for server channel list
 *   struct channel_list_node glist  -> unordered global channel list
 */

/* TODO: pointer from channel back to server can be eliminated which
 * simplifies architecture by having stateful functions aware of the
 * current context, and having input keybind functions, and send_mesg
 * returned from input() instead of calling them from input.c
 *
 * input shouldn't be a pointer, and thus shouldn't require being freed
 */

void channel_free(struct channel*);

#endif
