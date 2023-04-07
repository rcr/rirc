#ifndef RIRC_COMPONENTS_SERVER_H
#define RIRC_COMPONENTS_SERVER_H

#include "src/components/buffer.h"
#include "src/components/channel.h"
#include "src/components/ircv3.h"
#include "src/components/mode.h"

// TODO: move this to utils
#define IRC_MESSAGE_LEN 510

struct server
{
	const char *host;
	const char *port;
	const char *pass;
	const char *username;
	const char *realname;
	const char *nick;
	const char *mode;
	enum casemapping casemapping;
	struct {
		size_t next;
		size_t size;
		const char *base;
		const char **set;
	} nicks;
	struct channel *channel;
	struct channel_list clist;
	struct ircv3_caps ircv3_caps;
	struct ircv3_sasl ircv3_sasl;
	struct mode usermodes;
	struct mode_cfg mode_cfg;
	struct mode_str mode_str;
	struct server *next;
	struct server *prev;
	unsigned ping;
	unsigned connected  : 1;
	unsigned quitting   : 1;
	unsigned registered : 1;
	void *connection;
	// TODO: move this to utils
	struct {
		size_t i;
		char cl;
		char buf[IRC_MESSAGE_LEN + 1]; /* callback message buffer */
	} read;
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
	const char*,  /* realname */
	const char*); /* mode */

struct server* server_list_add(struct server_list*, struct server*);
struct server* server_list_del(struct server_list*, struct server*);
struct server* server_list_get(struct server_list*, const char*, const char*);

int server_set_chans(struct server*, const char*);
int server_set_nicks(struct server*, const char*);
void server_set_004(struct server*, char*);
void server_set_005(struct server*, char*);
void server_set_sasl(struct server*, const char*, const char*, const char*);

void server_nick_set(struct server*, const char*);
void server_nicks_next(struct server*);

void server_reset(struct server*);
void server_free(struct server*);

#endif
