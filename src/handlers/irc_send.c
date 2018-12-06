#include <ctype.h>

#include "src/components/channel.h"
#include "src/components/server.h"
#include "src/handlers/irc_ctcp.h"
#include "src/handlers/irc_send.gperf.out"
#include "src/handlers/irc_send.h"
#include "src/state.h"
#include "src/io.h"

#define failf(C, ...) \
	do { newlinef((C), 0, "-!!-", __VA_ARGS__); return 1; } while (0)

static int default_send_handler(
	struct server*,
	struct channel*,
	const char*,
	const char*);

int
irc_send_command(struct server *s, struct channel *c, char *m)
{
	char *cmnd, *p;
	const struct send_handler *send;

	if (!s)
		failf(c, "This is not a server");

	if (!(cmnd = getarg(&m, " ")))
		failf(c, "Messages beginning with '/' require a command");

	for (p = cmnd; *p; p++)
		*p = toupper(*p);

	if (!(send = send_handler_lookup(cmnd, strlen(cmnd))))
		return default_send_handler(s, c, cmnd, m);

	return send->f(s, c, m);
}

int
irc_send_privmsg(struct server *s, struct channel *c, char *m)
{
	int ret;

	if (!s)
		failf(c, "This is not a server");

	if (!(c->type == CHANNEL_T_CHANNEL || c->type == CHANNEL_T_PRIVATE))
		failf(c, "This is not a channel");

	if (!c->joined || c->parted)
		failf(c, "Not on channel");

	if ((ret = io_sendf(s->connection, "PRIVMSG %s :%s", c->name, m)))
		failf(c, "Send fail: %s", io_err(ret));

	return 0;
}

static int
default_send_handler(struct server *s, struct channel *c, const char *cmnd, const char *args)
{
	int ret;

	if ((ret = io_sendf(s->connection, "%s %s", cmnd, args)))
		failf(c, "Send fail: %s", io_err(ret));

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

static int
send_ctcp_action(struct server *s, struct channel *c, char *m)
{
	(void)s; (void)c; (void)m;
	return 0;
}

static int
send_ctcp_clientinfo(struct server *s, struct channel *c, char *m)
{
	(void)s; (void)c; (void)m;
	return 0;
}

static int
send_ctcp_finger(struct server *s, struct channel *c, char *m)
{
	(void)s; (void)c; (void)m;
	return 0;
}

static int
send_ctcp_ping(struct server *s, struct channel *c, char *m)
{
	(void)s; (void)c; (void)m;
	return 0;
}

static int
send_ctcp_source(struct server *s, struct channel *c, char *m)
{
	(void)s; (void)c; (void)m;
	return 0;
}

static int
send_ctcp_time(struct server *s, struct channel *c, char *m)
{
	(void)s; (void)c; (void)m;
	return 0;
}

static int
send_ctcp_userinfo(struct server *s, struct channel *c, char *m)
{
	(void)s; (void)c; (void)m;
	return 0;
}

static int
send_ctcp_version(struct server *s, struct channel *c, char *m)
{
	(void)s; (void)c; (void)m;
	return 0;
}
