#ifndef SERVER_H
#define SERVER_H

#include "rirc.h"
#include "channel.h"

/* TODO: refactor -> server.c */

/* Server */
typedef struct server
{
	char *host;
	char input[BUFFSIZE];
	char *iptr;
	char nick[NICKSIZE + 1];
	char *nicks;
	char *nptr;
	char *port;
	char *join;
	char usermodes[MODE_SIZE];
	int soc;
	int pinging;
	struct avl_node *ignore;
	struct channel *channel;
	struct server *next;
	struct server *prev;
	time_t latency_delta;
	time_t latency_time;
	time_t reconnect_delta;
	time_t reconnect_time;
	void *connecting;
} server;

#endif
