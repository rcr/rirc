#include "src/components/channel.h"
#include "src/components/server.h"
#include "src/handlers/irc_ctcp.h"
#include "src/handlers/irc_send.gperf.out"
#include "src/handlers/irc_send.h"

int
irc_send(struct server *s, struct channel *c, char *m)
{
	(void)s;
	(void)c;
	(void)m;

	(void)send_join;
	(void)send_msg;
	(void)send_nick;
	(void)send_part;
	(void)send_privmsg;
	(void)send_quit;
	(void)send_topic;
	(void)send_version;

	return 0;
}

static int send_join(struct server *s, struct channel *c, char *m) { (void)s; (void)c; (void)m; return 0; }
static int send_msg(struct server *s, struct channel *c, char *m) { (void)s; (void)c; (void)m; return 0; }
static int send_nick(struct server *s, struct channel *c, char *m) { (void)s; (void)c; (void)m; return 0; }
static int send_part(struct server *s, struct channel *c, char *m) { (void)s; (void)c; (void)m; return 0; }
static int send_privmsg(struct server *s, struct channel *c, char *m) { (void)s; (void)c; (void)m; return 0; }
static int send_quit(struct server *s, struct channel *c, char *m) { (void)s; (void)c; (void)m; return 0; }
static int send_topic(struct server *s, struct channel *c, char *m) { (void)s; (void)c; (void)m; return 0; }
static int send_version(struct server *s, struct channel *c, char *m) { (void)s; (void)c; (void)m; return 0; }
