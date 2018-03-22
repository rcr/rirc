#ifndef SERVER_H
#define SERVER_H

#include <time.h>

#include "src/components/buffer.h"
#include "src/components/channel.h"
#include "src/components/mode.h"
#include "src/rirc.h"

struct server
{
	//TODO: struct string. Remove arbitrary NICKSIZE
	char nick[NICKSIZE + 1];
	//TODO: can be grouped together, autonick
	char *nicks;
	char *nptr;
	char *host;
	char *port;
	//TODO: this shouldn't persist with the server,
	// its only relevant on successful connection
	// when parsing the cli args, add channels to the channel
	// list, they'll be joined on connect
	char *join;
	struct user_list ignore;
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

	//TODO: WIP
	struct channel_list clist;
	struct mode        usermodes;
	struct mode_str    usermodes_str;
	struct mode_config mode_config;
};

struct server_list
{
	/* Circular DLL */
	struct server *head;
	struct server *tail;
};

struct server* server(const char*, const char*, const char*);

struct server* server_list_add(struct server_list*, struct server*);
struct server* server_list_del(struct server_list*, struct server*);

void server_set_004(struct server*, char*);
void server_set_005(struct server*, char*);

void server_free(struct server*);

#endif
