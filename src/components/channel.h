#ifndef RIRC_COMPONENTS_CHANNEL_H
#define RIRC_COMPONENTS_CHANNEL_H

#include "src/components/buffer.h"
#include "src/components/input.h"
#include "src/components/mode.h"
#include "src/components/user.h"

/* Channel activity types, in order of precedence */
enum activity
{
	ACTIVITY_DEFAULT, /* Default activity */
	ACTIVITY_JPQ,     /* Join/Part/Quit activity */
	ACTIVITY_ACTIVE,  /* Chat activity */
	ACTIVITY_PINGED,  /* Ping activity */
	ACTIVITY_T_SIZE
};

enum channel_type
{
	CHANNEL_T_INVALID,
	CHANNEL_T_RIRC,    /* Default buffer */
	CHANNEL_T_CHANNEL, /* Channel buffer */
	CHANNEL_T_PRIVMSG, /* Privmsg buffer */
	CHANNEL_T_SERVER,  /* Server message buffer */
	CHANNEL_T_SIZE
};

struct channel
{
	const char *name;
	const char *key;
	enum activity activity;
	enum channel_type type;
	size_t name_len;
	struct buffer buffer;
	struct channel *next;
	struct channel *prev;
	struct input input;
	struct mode chanmodes;
	struct mode_str chanmodes_str;
	struct server *server;
	struct user_list users;
	unsigned parted : 1;
	unsigned joined : 1;
	char _[];
};

struct channel_list
{
	struct channel *head;
	struct channel *tail;
	unsigned count;
};

struct channel* channel(const char*, enum channel_type);
struct channel* channel_list_get(struct channel_list*, const char*, enum casemapping);
void channel_free(struct channel*);
void channel_key_add(struct channel*, const char*);
void channel_key_del(struct channel*);
void channel_list_add(struct channel_list*, struct channel*);
void channel_list_del(struct channel_list*, struct channel*);
void channel_list_free(struct channel_list*);
void channel_part(struct channel*);
void channel_reset(struct channel*);

#endif
