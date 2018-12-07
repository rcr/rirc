#include <ctype.h>
#include <sys/time.h>

#include "src/components/channel.h"
#include "src/components/server.h"
#include "src/handlers/irc_send.gperf.out"
#include "src/handlers/irc_send.h"
#include "src/state.h"
#include "src/io.h"

#define failf(C, ...) \
	do { newlinef((C), 0, "-!!-", __VA_ARGS__); return 1; } while (0)

#define sendf(S, ...) \
	do { int ret; \
	     if ((ret = io_sendf((S)->connection, __VA_ARGS__))) \
	         failf(c, "Send fail: %s", io_err(ret)); \
	     return 0; \
	} while (0)

static const char* targ_or_private(struct channel *c, char *m);

int
irc_send_command(struct server *s, struct channel *c, char *m)
{
	char *command, *p;
	const struct send_handler *send;

	if (!s)
		failf(c, "This is not a server");

	if (!(command = getarg(&m, " ")))
		failf(c, "Messages beginning with '/' require a command");

	for (p = command; *p; p++)
		*p = toupper(*p);

	if (!(send = send_handler_lookup(command, strlen(command))))
		sendf(s, "%s %s", command, m);

	return send->f(s, c, m);
}

int
irc_send_privmsg(struct server *s, struct channel *c, char *m)
{
	if (!s)
		failf(c, "This is not a server");

	if (!(c->type == CHANNEL_T_CHANNEL || c->type == CHANNEL_T_PRIVATE))
		failf(c, "This is not a channel");

	if (!c->joined || c->parted)
		failf(c, "Not on channel");

	sendf(s, "PRIVMSG %s :%s", c->name, m);
}

static const char*
targ_or_private(struct channel *c, char *m)
{
	const char *targ;

	if ((targ = getarg(&m, " ")))
		return targ;

	if (c->type == CHANNEL_T_PRIVATE)
		return c->name;

	return NULL;
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
	if (!(c->type == CHANNEL_T_CHANNEL || c->type == CHANNEL_T_PRIVATE))
		failf(c, "This is not a channel");

	sendf(s, "PRIVMSG %s :\001""ACTION %s\001", c->name, m);
}

static int
send_ctcp_clientinfo(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_private(c, m)))
		failf(c, "usage: /ctcp-clientinfo <target>");

	sendf(s, "PRIVMSG %s :\001CLIENTINFO\001", targ);
}

static int
send_ctcp_finger(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_private(c, m)))
		failf(c, "usage: /ctcp-finger <target>");

	sendf(s, "PRIVMSG %s :\001FINGER\001", targ);
}

static int
send_ctcp_ping(struct server *s, struct channel *c, char *m)
{
	const char *targ;
	struct timeval t;

	if (!(targ = targ_or_private(c, m)))
		failf(c, "usage: /ctcp-ping <target>");

	(void) gettimeofday(&t, NULL);

	sendf(s, "PRIVMSG %s :\001PING %llu %llu\001", t.tv_sec, t.tv_usec);
}

static int
send_ctcp_source(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_private(c, m)))
		failf(c, "usage: /ctcp-source <target>");

	sendf(s, "PRIVMSG %s :\001SOURCE\001", targ);
}

static int
send_ctcp_time(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_private(c, m)))
		failf(c, "usage: /ctcp-time <target>");

	sendf(s, "PRIVMSG %s :\001TIME\001", targ);
}

static int
send_ctcp_userinfo(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_private(c, m)))
		failf(c, "usage: /ctcp-userinfo <target>");

	sendf(s, "PRIVMSG %s :\001USERINFO\001", targ);
}

static int
send_ctcp_version(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_private(c, m)))
		failf(c, "usage: /ctcp-version <target>");

	sendf(s, "PRIVMSG %s :\001VERSION\001", targ);
}
