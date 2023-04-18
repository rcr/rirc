#include "src/handlers/irc_send.h"

// XXX:
//
//
// numerics returning from /who <mask> MIGHT include nicks, channels that are currently open...
//
// those numerics SHOULD be printed in the network buffer that we just jumped to...
//
// might require adding a flag server to keep note of what the last command that was run was, and
// not allow sending another until END is received?
//
//
// but also, /who ONLY responds with the one numeric 352????
// so maybe this one explicitly is in the network buffer, everything else can be targeted

#include "config.h"
#include "src/components/buffer.h"
#include "src/components/channel.h"
#include "src/components/ircv3.h"
#include "src/handlers/irc_send.gperf.out"
#include "src/io.h"
#include "src/state.h"
#include "src/utils/utils.h"

#include <ctype.h>
#include <sys/time.h>

#define failf(C, ...) \
	do { newlinef((C), 0, FROM_ERROR, __VA_ARGS__); \
	     return 1; \
	} while (0)

#define sendf(S, C, ...) \
	do { int ret; \
	     if ((ret = io_sendf((S)->connection, __VA_ARGS__))) \
	         failf((C), "Send fail: %s", io_err(ret)); \
	} while (0)

static const char* irc_send_args(struct channel*, char*, enum channel_type);

int
irc_send_command(struct server *s, struct channel *c, char *m)
{
	char *command;
	char *command_args;
	char *p;
	const struct send_handler *send;

	if (!s)
		failf(c, "This is not a server");

	if (!s->registered)
		failf(c, "Not registered with server");

	if (*m == ' ' || !(command = irc_strsep(&m)))
		failf(c, "Messages beginning with '/' require a command");

	for (p = command; *p; p++)
		*p = toupper(*p);

	command_args = irc_strtrim(&m);

	if ((send = send_handler_lookup(command, strlen(command))))
		return send->f(s, c, command_args);

	if (irc_strtrim(&command_args))
		sendf(s, c, "%s %s", command, command_args);
	else
		sendf(s, c, "%s", command);

	return 0;
}

int
irc_send_message(struct server *s, struct channel *c, const char *m)
{
	if (!s)
		failf(c, "This is not a server");

	if (!s->registered)
		failf(c, "Not registered with server");

	if (!(c->type == CHANNEL_T_CHANNEL || c->type == CHANNEL_T_PRIVMSG))
		failf(c, "This is not a channel");

	if (c->type == CHANNEL_T_CHANNEL && (!c->joined || c->parted))
		failf(c, "Not on channel");

	if (*m == 0)
		failf(c, "Message is empty");

	sendf(s, c, "PRIVMSG %s :%s", c->name, m);

	newlinef(c, BUFFER_LINE_CHAT_RIRC, s->nick, "%s", m);

	return 0;
}

static const char*
irc_send_args(struct channel *c, char *m, enum channel_type type)
{
	const char *args;

	if ((args = irc_strtrim(&m)))
		return args;

	if (c->type == type)
		return c->name;

	return NULL;
}

static int
irc_send_away(struct server *s, struct channel *c, char *m)
{
	if (irc_strtrim(&m))
		sendf(s, c, "AWAY :%s", m);
	else
		sendf(s, c, "AWAY");

	return 0;
}

static int
irc_send_names(struct server *s, struct channel *c, char *m)
{
	const char *target;

	if ((target = irc_send_args(c, m, CHANNEL_T_CHANNEL)))
		sendf(s, c, "NAMES %s", target);
	else
		sendf(s, c, "NAMES");

	return 0;
}

static int
irc_send_notice(struct server *s, struct channel *c, char *m)
{
	const char *target;

	if (!(target = irc_strsep(&m)))
		failf(c, "Usage: /notice <target> <message>");

	if (!m || !*m)
		failf(c, "Usage: /notice <target> <message>");

	sendf(s, c, "NOTICE %s :%s", target, m);

	return 0;
}

static int
irc_send_part(struct server *s, struct channel *c, char *m)
{
	if (c->type != CHANNEL_T_CHANNEL)
		failf(c, "This is not a channel");

	if (irc_strtrim(&m))
		sendf(s, c, "PART %s :%s", c->name, m);
	else
		sendf(s, c, "PART %s :%s", c->name, DEFAULT_PART_MESG);

	return 0;
}

static int
irc_send_privmsg(struct server *s, struct channel *c, char *m)
{
	const char *target;
	char *dup;
	char *p1;
	char *p2;

	if (!(target = irc_strsep(&m)))
		failf(c, "Usage: /privmsg <target> <message>");

	if (!m || !*m)
		failf(c, "Usage: /privmsg <target> <message>");

	p2 = dup = irc_strdup(target);

	do {
		struct channel *c_target;

		p1 = p2;
		p2 = strchr(p2, ',');

		if (p2)
			*p2++ = 0;

		if (!(c_target = channel_list_get(&s->clist, p1, s->casemapping))) {

			if (irc_isnick(p1))
				c_target = channel(p1, CHANNEL_T_PRIVMSG);
			else
				c_target = channel(p1, CHANNEL_T_CHANNEL);

			c_target->server = s;
			channel_list_add(&s->clist, c_target);
		}

		newlinef(c_target, BUFFER_LINE_CHAT_RIRC, s->nick, "%s", m);

	} while (p2);

	free(dup);

	sendf(s, c, "PRIVMSG %s :%s", target, m);

	return 0;
}

static int
irc_send_quit(struct server *s, struct channel *c, char *m)
{
	s->quitting = 1;

	if (irc_strtrim(&m))
		sendf(s, c, "QUIT :%s", m);
	else
		sendf(s, c, "QUIT :%s", DEFAULT_PART_MESG);

	return 0;
}

static int
irc_send_topic(struct server *s, struct channel *c, char *m)
{
	if (c->type != CHANNEL_T_CHANNEL)
		failf(c, "This is not a channel");

	if (irc_strtrim(&m))
		sendf(s, c, "TOPIC %s :%s", c->name, m);
	else
		sendf(s, c, "TOPIC %s", c->name);

	return 0;
}

static int
irc_send_topic_unset(struct server *s, struct channel *c, char *m)
{
	if (c->type != CHANNEL_T_CHANNEL)
		failf(c, "This is not a channel");

	if (irc_strtrim(&m))
		failf(c, "Usage: /topic-unset");

	sendf(s, c, "TOPIC %s :", c->name);

	return 0;
}

static int
irc_send_who(struct server *s, struct channel *c, char *m)
{
	/* WHO <mask>
	 *
	 * (352) RPL_WHOREPLY
	 * (315) RPL_ENDOFWHO
	 * (402) ERR_NOSUCHSERVER */

	const char *target;

	if (!(target = irc_send_args(c, m, CHANNEL_T_PRIVMSG)))
		failf(c, "Usage: /who <target>");

	// TODO: this one should just indiscriminately jump to network buffer and print there

	sendf(s, c, "WHO %s", target);

	return 0;
}

static int
irc_send_whois(struct server *s, struct channel *c, char *m)
{
	/* WHOIS [target] <nick>
	 *
	 * (276) RPL_WHOISCERTFP
	 * (301) RPL_AWAY
	 * (307) RPL_WHOISREGNICK
	 * (311) RPL_WHOISUSER
	 * (312) RPL_WHOISSERVER
	 * (313) RPL_WHOISOPERATOR
	 * (317) RPL_WHOISIDLE
	 * (318) RPL_ENDOFWHOIS
	 * (319) RPL_WHOISCHANNELS
	 * (320) RPL_WHOISSPECIAL
	 * (330) RPL_WHOISACCOUNT
	 * (338) RPL_WHOISACTUALLY
	 * (378) RPL_WHOISHOST
	 * (379) RPL_WHOISMODES
	 * (401) ERR_NOSUCHNICK
	 * (402) ERR_NOSUCHSERVER
	 * (431) ERR_NONICKNAMEGIVEN
	 * (671) RPL_WHOISSECURE */

	const char *target;

	if (!(target = irc_send_args(c, m, CHANNEL_T_PRIVMSG)))
		failf(c, "Usage: /whois <target>");

	// TODO: the numeric replies should also be targeting the buffer

	if (!(c = channel_list_get(&(s->clist), target, s->casemapping)))
		c = s->channel;

	channel_set_current(c);

	newlinef(c, 0, FROM_INFO, "/whois %s", target);

	sendf(s, c, "WHOIS %s", target);

	return 0;
}

static int
irc_send_whowas(struct server *s, struct channel *c, char *m)
{
	/* WHOWAS <nick> [count]
	 *
	 * (312) RPL_WHOISSERVER
	 * (314) RPL_WHOWASUSER
	 * (338) RPL_WHOISACTUALLY
	 * (369) RPL_ENDOFWHOWAS
	 * (406) ERR_WASNOSUCHNICK
	 * (431) ERR_NONICKNAMEGIVEN
	 * (461) ERR_NEEDMOREPARAMS */

	const char *target;

	if (!(target = irc_send_args(c, m, CHANNEL_T_PRIVMSG)))
		failf(c, "Usage: /whowas <target> [count]");

	sendf(s, c, "WHOWAS %s", target);

	return 0;
}

static int
irc_send_ctcp_action(struct server *s, struct channel *c, char *m)
{
	const char *target;

	if (!(target = irc_strsep(&m)) || !irc_strtrim(&m))
		failf(c, "Usage: /ctcp-action <target> <text>");

	sendf(s, c, "PRIVMSG %s :\001ACTION %s\001", target, m);

	return 0;
}

static int
irc_send_ctcp_clientinfo(struct server *s, struct channel *c, char *m)
{
	const char *target;

	if (!(target = irc_send_args(c, m, CHANNEL_T_PRIVMSG)))
		failf(c, "Usage: /ctcp-clientinfo <target>");

	sendf(s, c, "PRIVMSG %s :\001CLIENTINFO\001", target);

	return 0;
}

static int
irc_send_ctcp_finger(struct server *s, struct channel *c, char *m)
{
	const char *target;

	if (!(target = irc_send_args(c, m, CHANNEL_T_PRIVMSG)))
		failf(c, "Usage: /ctcp-finger <target>");

	sendf(s, c, "PRIVMSG %s :\001FINGER\001", target);

	return 0;
}

static int
irc_send_ctcp_ping(struct server *s, struct channel *c, char *m)
{
	const char *target;
	struct timeval t;

	if (!(target = irc_send_args(c, m, CHANNEL_T_PRIVMSG)))
		failf(c, "Usage: /ctcp-ping <target>");

	(void) gettimeofday(&t, NULL);

	sendf(s, c, "PRIVMSG %s :\001PING %llu %llu\001", target,
		(unsigned long long)t.tv_sec,
		(unsigned long long)t.tv_usec);

	return 0;
}

static int
irc_send_ctcp_source(struct server *s, struct channel *c, char *m)
{
	const char *target;

	if (!(target = irc_send_args(c, m, CHANNEL_T_PRIVMSG)))
		failf(c, "Usage: /ctcp-source <target>");

	sendf(s, c, "PRIVMSG %s :\001SOURCE\001", target);

	return 0;
}

static int
irc_send_ctcp_time(struct server *s, struct channel *c, char *m)
{
	const char *target;

	if (!(target = irc_send_args(c, m, CHANNEL_T_PRIVMSG)))
		failf(c, "Usage: /ctcp-time <target>");

	sendf(s, c, "PRIVMSG %s :\001TIME\001", target);

	return 0;
}

static int
irc_send_ctcp_userinfo(struct server *s, struct channel *c, char *m)
{
	const char *target;

	if (!(target = irc_send_args(c, m, CHANNEL_T_PRIVMSG)))
		failf(c, "Usage: /ctcp-userinfo <target>");

	sendf(s, c, "PRIVMSG %s :\001USERINFO\001", target);

	return 0;
}

static int
irc_send_ctcp_version(struct server *s, struct channel *c, char *m)
{
	const char *target;

	if (!(target = irc_send_args(c, m, CHANNEL_T_PRIVMSG)))
		failf(c, "Usage: /ctcp-version <target>");

	sendf(s, c, "PRIVMSG %s :\001VERSION\001", target);

	return 0;
}

static int
irc_send_ircv3_cap_ls(struct server *s, struct channel *c, char *m)
{
	if (irc_strtrim(&m))
		failf(c, "Usage: /cap-ls");

	sendf(s, c, "CAP LS " IRCV3_CAP_VERSION);

	return 0;
}

static int
irc_send_ircv3_cap_list(struct server *s, struct channel *c, char *m)
{
	if (irc_strtrim(&m))
		failf(c, "Usage: /cap-list");

	sendf(s, c, "CAP LIST");

	return 0;
}

#undef failf
#undef sendf
