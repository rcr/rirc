#ifndef SERVER_H
#define SERVER_H

#include "src/components/buffer.h"
#include "src/components/channel.h"
#include "src/components/mode.h"
#include "src/io.h"

struct server
{
	const char *host;
	const char *port;
	const char *pass;
	const char *username;
	const char *realname;
	const char *nick;
	struct {
		size_t next;
		size_t size;
		const char *base;
		const char **set;
	} nicks;
	struct channel *channel;
	//TODO: WIP
	struct channel_list clist;
	struct connection *connection;
	struct mode usermodes;
	struct mode_str mode_str;
	struct mode_cfg mode_cfg;
	struct server *next;
	struct server *prev;
	struct user_list ignore;
	unsigned latency_delta;
};

struct server_list
{
	/* Circular DLL */
	struct server *head;
	struct server *tail;
};

struct server* server(
	const char*,  /* host */
	const char*,  /* port */
	const char*,  /* pass */
	const char*,  /* username */
	const char*); /* realname */

struct server* server_list_add(struct server_list*, struct server*);
struct server* server_list_del(struct server_list*, struct server*);
struct server* server_list_get(struct server_list*, const char *, const char*);

void server_set_004(struct server*, char*);
void server_set_005(struct server*, char*);
int server_set_nicks(struct server*, const char*);

void server_nick_set(struct server*, const char*);
void server_nicks_next(struct server*);
void server_nicks_reset(struct server*);

void server_free(struct server*);

#endif
