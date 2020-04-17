#include <ctype.h>
#include <sys/time.h>

#include "config.h"
#include "src/components/buffer.h"
#include "src/components/channel.h"
#include "src/components/server.h"
#include "src/handlers/irc_send.gperf.out"
#include "src/handlers/irc_send.h"
#include "src/io.h"
#include "src/state.h"
#include "src/utils/utils.h"

// TODO: should privmsg/notice open a PRIVATE/CHANNEL buffer for the target?

#define failf(C, ...) \
	do { newlinef((C), 0, FROM_ERROR, __VA_ARGS__); \
	     return 1; \
	} while (0)

#define sendf(S, C, ...) \
	do { int ret; \
	     if ((ret = io_sendf((S)->connection, __VA_ARGS__))) \
	         failf((C), "Send fail: %s", io_err(ret)); \
	} while (0)

static const char* targ_or_type(struct channel*, char*, enum channel_t type);

int
irc_send_command(struct server *s, struct channel *c, char *m)
{
	char *command, *command_args, *p;
	const struct send_handler *send;

	if (!s)
		failf(c, "This is not a server");

	if (!s->registered)
		failf(c, "Not registered with server");

	if (*m == ' ' || !(command = strsep(&m)))
		failf(c, "Messages beginning with '/' require a command");

	for (p = command; *p; p++)
		*p = toupper(*p);

	command_args = strtrim(&m);

	if ((send = send_handler_lookup(command, strlen(command))))
		return send->f(s, c, command_args);

	if (strtrim(&command_args))
		sendf(s, c, "%s %s", command, command_args);
	else
		sendf(s, c, "%s", command);

	return 0;
}

int
irc_send_privmsg(struct server *s, struct channel *c, char *m)
{
	if (!s)
		failf(c, "This is not a server");

	if (!s->registered)
		failf(c, "Not registered with server");

	if (!(c->type == CHANNEL_T_CHANNEL || c->type == CHANNEL_T_PRIVATE))
		failf(c, "This is not a channel");

	if (c->type == CHANNEL_T_CHANNEL && (!c->joined || c->parted))
		failf(c, "Not on channel");

	if (*m == 0)
		failf(c, "Message is empty");

	sendf(s, c, "PRIVMSG %s :%s", c->name, m);

	newline(c, BUFFER_LINE_CHAT, s->nick, m);

	return 0;
}

static const char*
targ_or_type(struct channel *c, char *m, enum channel_t type)
{
	const char *targ;

	if ((targ = strsep(&m)))
		return targ;

	if (c->type == type)
		return c->name;

	return NULL;
}

static int
send_ircv3_cap_ls(struct server *s, struct channel *c, char *m)
{
	if (strtrim(&m))
		failf(c, "Usage: /cap-ls");

	sendf(s, c, "CAP LS 302");

	return 0;
}

static int
send_ircv3_cap_list(struct server *s, struct channel *c, char *m)
{
	if (strtrim(&m))
		failf(c, "Usage: /cap-list");

	sendf(s, c, "CAP LIST");

	return 0;
}

static int
send_ctcp_action(struct server *s, struct channel *c, char *m)
{
	if (!(c->type == CHANNEL_T_CHANNEL || c->type == CHANNEL_T_PRIVATE))
		failf(c, "This is not a channel");

	sendf(s, c, "PRIVMSG %s :\001ACTION %s\001", c->name, m);

	return 0;
}

static int
send_ctcp_clientinfo(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_type(c, m, CHANNEL_T_PRIVATE)))
		failf(c, "Usage: /ctcp-clientinfo <target>");

	sendf(s, c, "PRIVMSG %s :\001CLIENTINFO\001", targ);

	return 0;
}

static int
send_ctcp_finger(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_type(c, m, CHANNEL_T_PRIVATE)))
		failf(c, "Usage: /ctcp-finger <target>");

	sendf(s, c, "PRIVMSG %s :\001FINGER\001", targ);

	return 0;
}

static int
send_ctcp_ping(struct server *s, struct channel *c, char *m)
{
	const char *targ;
	struct timeval t;

	if (!(targ = targ_or_type(c, m, CHANNEL_T_PRIVATE)))
		failf(c, "Usage: /ctcp-ping <target>");

	(void) gettimeofday(&t, NULL);

	sendf(s, c, "PRIVMSG %s :\001PING %llu %llu\001", targ, t.tv_sec, t.tv_usec);

	return 0;
}

static int
send_ctcp_source(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_type(c, m, CHANNEL_T_PRIVATE)))
		failf(c, "Usage: /ctcp-source <target>");

	sendf(s, c, "PRIVMSG %s :\001SOURCE\001", targ);

	return 0;
}

static int
send_ctcp_time(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_type(c, m, CHANNEL_T_PRIVATE)))
		failf(c, "Usage: /ctcp-time <target>");

	sendf(s, c, "PRIVMSG %s :\001TIME\001", targ);

	return 0;
}

static int
send_ctcp_userinfo(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_type(c, m, CHANNEL_T_PRIVATE)))
		failf(c, "Usage: /ctcp-userinfo <target>");

	sendf(s, c, "PRIVMSG %s :\001USERINFO\001", targ);

	return 0;
}

static int
send_ctcp_version(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = targ_or_type(c, m, CHANNEL_T_PRIVATE)))
		failf(c, "Usage: /ctcp-version <target>");

	sendf(s, c, "PRIVMSG %s :\001VERSION\001", targ);

	return 0;
}

static int
send_notice(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = strsep(&m)))
		failf(c, "Usage: /notice <target> <message>");

	if (!m || !*m)
		failf(c, "Usage: /notice <target> <message>");

	sendf(s, c, "NOTICE %s :%s", targ, m);

	return 0;
}

static int
send_part(struct server *s, struct channel *c, char *m)
{
	if (c->type != CHANNEL_T_CHANNEL)
		failf(c, "This is not a channel");

	if (strtrim(&m))
		sendf(s, c, "PART %s :%s", c->name, m);
	else
		sendf(s, c, "PART %s :%s", c->name, DEFAULT_PART_MESG);

	return 0;
}

static int
send_privmsg(struct server *s, struct channel *c, char *m)
{
	const char *targ;

	if (!(targ = strsep(&m)))
		failf(c, "Usage: /privmsg <target> <message>");

	if (!m || !*m)
		failf(c, "Usage: /privmsg <target> <message>");

	sendf(s, c, "PRIVMSG %s :%s", targ, m);

	return 0;
}

static int
send_quit(struct server *s, struct channel *c, char *m)
{
	s->quitting = 1;

	if (strtrim(&m))
		sendf(s, c, "QUIT :%s", m);
	else
		sendf(s, c, "QUIT :%s", DEFAULT_PART_MESG);

	return 0;
}

static int
send_topic(struct server *s, struct channel *c, char *m)
{
	if (c->type != CHANNEL_T_CHANNEL)
		failf(c, "This is not a channel");

	if (strtrim(&m))
		sendf(s, c, "TOPIC %s :%s", c->name, m);
	else
		sendf(s, c, "TOPIC %s", c->name);

	return 0;
}

#undef failf
#undef sendf
