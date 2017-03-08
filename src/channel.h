#ifndef CHANNEL_H
#define CHANNEL_H

/* TODO: refactor -> channel.c */

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
typedef struct channel
{
	activity_t active;
	char *name;
	char type_flag;
	char chanmodes[MODE_SIZE];
	//TODO: remove
	int nick_count;
	int parted;
	struct buffer buffer;
	struct channel *next;
	struct channel *prev;
	//TODO: replace
	struct avl_node *nicklist;
	struct server *server;
	struct input *input;
} channel;

#endif
