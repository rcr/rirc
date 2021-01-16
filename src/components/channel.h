#ifndef RIRC_COMPONENTS_CHANNEL_H
#define RIRC_COMPONENTS_CHANNEL_H

#include "src/components/buffer.h"
#include "src/components/input.h"
#include "src/components/mode.h"
#include "src/components/user.h"

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
	CHANNEL_T_RIRC,    /* Default buffer */
	CHANNEL_T_CHANNEL, /* Channel message buffer */
	CHANNEL_T_SERVER,  /* Server message buffer */
	CHANNEL_T_PRIVATE, /* Private message buffer */
	CHANNEL_T_SIZE
};

struct channel
{
	const char *name;
	enum activity_t activity;
	enum channel_t type;
	size_t name_len;
	struct buffer buffer;
	struct channel *next;
	struct channel *prev;
	struct input input;
	struct mode chanmodes;
	struct mode_str chanmodes_str;
	struct server *server;
	struct user_list users;
	unsigned int parted : 1;
	unsigned int joined : 1;
	char _[];
};

struct channel_list
{
	struct channel *head;
	struct channel *tail;
	unsigned count;
};

struct channel* channel(const char*, enum channel_t);
struct channel* channel_list_get(struct channel_list*, const char*, enum casemapping_t);
void channel_free(struct channel*);
void channel_list_add(struct channel_list*, struct channel*);
void channel_list_del(struct channel_list*, struct channel*);
void channel_list_free(struct channel_list*);
void channel_part(struct channel*);
void channel_reset(struct channel*);

#endif
