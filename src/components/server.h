#ifndef SERVER_H
#define SERVER_H

#include <time.h>

#include "src/components/buffer.h"
#include "src/components/channel.h"
#include "src/components/mode.h"
#include "src/io.h"

#include "src/rirc.h" // NICKSIZE

struct server
{
	//TODO: struct string. Remove arbitrary NICKSIZE
	char nick[NICKSIZE + 1];
	//TODO: can be grouped together, autonick
	char *nicks;
	char *nptr;
	char *host;
	char *port;
	char *pass;
	//TODO channel_list
	struct channel *channel;
	//TODO: WIP
	struct channel_list clist;
	struct connection *connection;
	struct mode        usermodes;
	struct mode_str    usermodes_str;
	struct mode_config mode_config;
	struct server *next;
	struct server *prev;
	struct user_list ignore;
	time_t latency_delta;
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
int server_set_chans(struct server*, const char*);
int server_set_nicks(struct server*, const char*);

void server_free(struct server*);

#endif
