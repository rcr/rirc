#ifndef SERVER_H
#define SERVER_H

#include <time.h>

#include "rirc.h"

#include "buffer.h"
#include "channel.h"

struct server_list
{
	struct server *head;
};

struct server
{
	//TODO: strdup this. Remove arbitrary NICKSIZE
	char nick[NICKSIZE + 1];
	//TODO: can be grouped together, autonick
	char *nicks;
	char *nptr;
	char *host;
	char *port;
	//TODO: this shouldn't persist with the server,
	// its only relevant on successful connection
	char *join;
	char usermodes[MODE_SIZE];
	//TODO: nicklist
	struct avl_node *ignore;
	//TODO channel_list
	struct channel *channel;
	//TODO:
	struct server *next;
	struct server *prev;
	//TODO: connection stuff
	char input[BUFFSIZE];
	char *iptr;
	int pinging;
	int soc;
	time_t latency_delta;
	time_t latency_time;
	time_t reconnect_delta;
	time_t reconnect_time;
	void *connecting;
};

//TODO:
struct server* server(struct server*, char*, char*, char*);
struct server* server_add(struct server_list*, struct server*);
struct server* server_get(struct server_list*, struct server*);
struct server* server_del(struct server_list*, struct server*);

#endif
