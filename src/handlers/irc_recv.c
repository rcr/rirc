#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "src/components/server.h"
#include "src/handlers/irc_ctcp.h"
#include "src/handlers/irc_recv.gperf.out"
#include "src/handlers/irc_recv.h"
#include "src/handlers/ircv3.h"
#include "src/io.h"
#include "src/state.h"
#include "src/utils/utils.h"

#ifndef JOIN_THRESHOLD
#define JOIN_THRESHOLD 0
#endif

#ifndef PART_THRESHOLD
#define PART_THRESHOLD 0
#endif

#ifndef QUIT_THRESHOLD
#define QUIT_THRESHOLD 0
#endif

#define failf(S, ...) \
	do { server_error((S), __VA_ARGS__); \
	     return 1; \
	} while (0)

#define sendf(S, ...) \
	do { int ret; \
	     if ((ret = io_sendf((S)->connection, __VA_ARGS__))) \
	         failf((S), "Send fail: %s", io_err(ret)); \
	} while (0)

/* Default message handler */
static int irc_message(struct server*, struct irc_message*, const char*);

/* Generic handlers */
static int irc_error(struct server*, struct irc_message*);
static int irc_ignore(struct server*, struct irc_message*);
static int irc_info(struct server*, struct irc_message*);

/* Numeric handlers */
static int irc_001(struct server*, struct irc_message*);
static int irc_004(struct server*, struct irc_message*);
static int irc_005(struct server*, struct irc_message*);
static int irc_221(struct server*, struct irc_message*);
static int irc_324(struct server*, struct irc_message*);
static int irc_328(struct server*, struct irc_message*);
static int irc_329(struct server*, struct irc_message*);
static int irc_332(struct server*, struct irc_message*);
static int irc_333(struct server*, struct irc_message*);
static int irc_353(struct server*, struct irc_message*);
static int irc_433(struct server*, struct irc_message*);

static int irc_recv_numeric(struct server*, struct irc_message*);
static int recv_mode_chanmodes(struct irc_message*, const struct mode_cfg*, struct server*, struct channel*);
static int recv_mode_usermodes(struct irc_message*, const struct mode_cfg*, struct server*);

static const unsigned quit_threshold = QUIT_THRESHOLD;
static const unsigned join_threshold = JOIN_THRESHOLD;
static const unsigned part_threshold = PART_THRESHOLD;

static const irc_recv_f irc_numerics[] = {
	  [1] = irc_001,    /* RPL_WELCOME */
	  [2] = irc_info,   /* RPL_YOURHOST */
	  [3] = irc_info,   /* RPL_CREATED */
	  [4] = irc_004,    /* RPL_MYINFO */
	  [5] = irc_005,    /* RPL_ISUPPORT */
	[200] = irc_info,   /* RPL_TRACELINK */
	[201] = irc_info,   /* RPL_TRACECONNECTING */
	[202] = irc_info,   /* RPL_TRACEHANDSHAKE */
	[203] = irc_info,   /* RPL_TRACEUNKNOWN */
	[204] = irc_info,   /* RPL_TRACEOPERATOR */
	[205] = irc_info,   /* RPL_TRACEUSER */
	[206] = irc_info,   /* RPL_TRACESERVER */
	[207] = irc_info,   /* RPL_TRACESERVICE */
	[208] = irc_info,   /* RPL_TRACENEWTYPE */
	[209] = irc_info,   /* RPL_TRACECLASS */
	[210] = irc_info,   /* RPL_TRACELOG */
	[211] = irc_info,   /* RPL_STATSLINKINFO */
	[212] = irc_info,   /* RPL_STATSCOMMANDS */
	[213] = irc_info,   /* RPL_STATSCLINE */
	[214] = irc_info,   /* RPL_STATSNLINE */
	[215] = irc_info,   /* RPL_STATSILINE */
	[216] = irc_info,   /* RPL_STATSKLINE */
	[217] = irc_info,   /* RPL_STATSQLINE */
	[218] = irc_info,   /* RPL_STATSYLINE */
	[219] = irc_ignore, /* RPL_ENDOFSTATS */
	[221] = irc_221,    /* RPL_UMODEIS */
	[234] = irc_info,   /* RPL_SERVLIST */
	[235] = irc_ignore, /* RPL_SERVLISTEND */
	[240] = irc_info,   /* RPL_STATSVLINE */
	[241] = irc_info,   /* RPL_STATSLLINE */
	[242] = irc_info,   /* RPL_STATSUPTIME */
	[243] = irc_info,   /* RPL_STATSOLINE */
	[244] = irc_info,   /* RPL_STATSHLINE */
	[245] = irc_info,   /* RPL_STATSSLINE */
	[246] = irc_info,   /* RPL_STATSPING */
	[247] = irc_info,   /* RPL_STATSBLINE */
	[250] = irc_info,   /* RPL_STATSCONN */
	[251] = irc_info,   /* RPL_LUSERCLIENT */
	[252] = irc_info,   /* RPL_LUSEROP */
	[253] = irc_info,   /* RPL_LUSERUNKNOWN */
	[254] = irc_info,   /* RPL_LUSERCHANNELS */
	[255] = irc_info,   /* RPL_LUSERME */
	[256] = irc_info,   /* RPL_ADMINME */
	[257] = irc_info,   /* RPL_ADMINLOC1 */
	[258] = irc_info,   /* RPL_ADMINLOC2 */
	[259] = irc_info,   /* RPL_ADMINEMAIL */
	[262] = irc_info,   /* RPL_TRACEEND */
	[263] = irc_info,   /* RPL_TRYAGAIN */
	[265] = irc_info,   /* RPL_LOCALUSERS */
	[266] = irc_info,   /* RPL_GLOBALUSERS */
	[301] = irc_info,   /* RPL_AWAY */
	[302] = irc_info,   /* ERR_USERHOST */
	[303] = irc_info,   /* RPL_ISON */
	[305] = irc_info,   /* RPL_UNAWAY */
	[306] = irc_info,   /* RPL_NOWAWAY */
	[311] = irc_info,   /* RPL_WHOISUSER */
	[312] = irc_info,   /* RPL_WHOISSERVER */
	[313] = irc_info,   /* RPL_WHOISOPERATOR */
	[314] = irc_info,   /* RPL_WHOWASUSER */
	[315] = irc_ignore, /* RPL_ENDOFWHO */
	[317] = irc_info,   /* RPL_WHOISIDLE */
	[318] = irc_ignore, /* RPL_ENDOFWHOIS */
	[319] = irc_info,   /* RPL_WHOISCHANNELS */
	[322] = irc_info,   /* RPL_LIST */
	[323] = irc_ignore, /* RPL_LISTEND */
	[324] = irc_324,    /* RPL_CHANNELMODEIS */
	[325] = irc_info,   /* RPL_UNIQOPIS */
	[328] = irc_328,    /* RPL_CHANNEL_URL */
	[329] = irc_329,    /* RPL_CREATIONTIME */
	[331] = irc_ignore, /* RPL_NOTOPIC */
	[332] = irc_332,    /* RPL_TOPIC */
	[333] = irc_333,    /* RPL_TOPICWHOTIME */
	[341] = irc_info,   /* RPL_INVITING */
	[346] = irc_info,   /* RPL_INVITELIST */
	[347] = irc_ignore, /* RPL_ENDOFINVITELIST */
	[348] = irc_info,   /* RPL_EXCEPTLIST */
	[349] = irc_ignore, /* RPL_ENDOFEXCEPTLIST */
	[351] = irc_info,   /* RPL_VERSION */
	[352] = irc_info,   /* RPL_WHOREPLY */
	[353] = irc_353,    /* RPL_NAMREPLY */
	[364] = irc_info,   /* RPL_LINKS */
	[365] = irc_ignore, /* RPL_ENDOFLINKS */
	[366] = irc_ignore, /* RPL_ENDOFNAMES */
	[367] = irc_info,   /* RPL_BANLIST */
	[368] = irc_ignore, /* RPL_ENDOFBANLIST */
	[369] = irc_ignore, /* RPL_ENDOFWHOWAS */
	[371] = irc_info,   /* RPL_INFO */
	[372] = irc_info,   /* RPL_MOTD */
	[374] = irc_ignore, /* RPL_ENDOFINFO */
	[375] = irc_ignore, /* RPL_MOTDSTART */
	[376] = irc_ignore, /* RPL_ENDOFMOTD */
	[381] = irc_info,   /* RPL_YOUREOPER */
	[391] = irc_info,   /* RPL_TIME */
	[401] = irc_error,  /* ERR_NOSUCHNICK */
	[402] = irc_error,  /* ERR_NOSUCHSERVER */
	[403] = irc_error,  /* ERR_NOSUCHCHANNEL */
	[404] = irc_error,  /* ERR_CANNOTSENDTOCHAN */
	[405] = irc_error,  /* ERR_TOOMANYCHANNELS */
	[406] = irc_error,  /* ERR_WASNOSUCHNICK */
	[407] = irc_error,  /* ERR_TOOMANYTARGETS */
	[408] = irc_error,  /* ERR_NOSUCHSERVICE */
	[409] = irc_error,  /* ERR_NOORIGIN */
	[411] = irc_error,  /* ERR_NORECIPIENT */
	[412] = irc_error,  /* ERR_NOTEXTTOSEND */
	[413] = irc_error,  /* ERR_NOTOPLEVEL */
	[414] = irc_error,  /* ERR_WILDTOPLEVEL */
	[415] = irc_error,  /* ERR_BADMASK */
	[416] = irc_error,  /* ERR_TOOMANYMATCHES */
	[421] = irc_error,  /* ERR_UNKNOWNCOMMAND */
	[422] = irc_error,  /* ERR_NOMOTD */
	[423] = irc_error,  /* ERR_NOADMININFO */
	[431] = irc_error,  /* ERR_NONICKNAMEGIVEN */
	[432] = irc_error,  /* ERR_ERRONEUSNICKNAME */
	[433] = irc_433,    /* ERR_NICKNAMEINUSE */
	[436] = irc_error,  /* ERR_NICKCOLLISION */
	[437] = irc_error,  /* ERR_UNAVAILRESOURCE */
	[441] = irc_error,  /* ERR_USERNOTINCHANNEL */
	[442] = irc_error,  /* ERR_NOTONCHANNEL */
	[443] = irc_error,  /* ERR_USERONCHANNEL */
	[451] = irc_error,  /* ERR_NOTREGISTERED */
	[461] = irc_error,  /* ERR_NEEDMOREPARAMS */
	[462] = irc_error,  /* ERR_ALREADYREGISTRED */
	[463] = irc_error,  /* ERR_NOPERMFORHOST */
	[464] = irc_error,  /* ERR_PASSWDMISMATCH */
	[465] = irc_error,  /* ERR_YOUREBANNEDCREEP */
	[466] = irc_error,  /* ERR_YOUWILLBEBANNED */
	[467] = irc_error,  /* ERR_KEYSET */
	[471] = irc_error,  /* ERR_CHANNELISFULL */
	[472] = irc_error,  /* ERR_UNKNOWNMODE */
	[473] = irc_error,  /* ERR_INVITEONLYCHAN */
	[474] = irc_error,  /* ERR_BANNEDFROMCHAN */
	[475] = irc_error,  /* ERR_BADCHANNELKEY */
	[476] = irc_error,  /* ERR_BADCHANMASK */
	[477] = irc_error,  /* ERR_NOCHANMODES */
	[478] = irc_error,  /* ERR_BANLISTFULL */
	[481] = irc_error,  /* ERR_NOPRIVILEGES */
	[482] = irc_error,  /* ERR_CHANOPRIVSNEEDED */
	[483] = irc_error,  /* ERR_CANTKILLSERVER */
	[484] = irc_error,  /* ERR_RESTRICTED */
	[485] = irc_error,  /* ERR_UNIQOPPRIVSNEEDED */
	[491] = irc_error,  /* ERR_NOOPERHOST */
	[501] = irc_error,  /* ERR_UMODEUNKNOWNFLAG */
	[502] = irc_error,  /* ERR_USERSDONTMATCH */
	[704] = irc_info,   /* RPL_HELPSTART */
	[705] = irc_info,   /* RPL_HELP */
	[706] = irc_ignore, /* RPL_ENDOFHELP */
};

int
irc_recv(struct server *s, struct irc_message *m)
{
	const struct recv_handler* handler;

	if (isdigit(*m->command))
		return irc_recv_numeric(s, m);

	if ((handler = recv_handler_lookup(m->command, m->len_command)))
		return handler->f(s, m);

	return irc_message(s, m, FROM_UNKNOWN);
}

static int
irc_message(struct server *s, struct irc_message *m, const char *from)
{
	char *trailing;

	if (!strtrim(&m->params))
		return 0;

	if (irc_message_split(m, &trailing)) {
		if (m->params)
			newlinef(s->channel, 0, from, "[%s] ~ %s", m->params, trailing);
		else
			newlinef(s->channel, 0, from, "%s", trailing);
	} else if (m->params) {
		newlinef(s->channel, 0, from, "[%s]", m->params);
	}

	return 0;
}

static int
irc_error(struct server *s, struct irc_message *m)
{
	/* Generic error message handling */

	return irc_message(s, m, FROM_ERROR);
}

static int
irc_ignore(struct server *s, struct irc_message *m)
{
	/* Generic handling for ignored message types */

	UNUSED(s);
	UNUSED(m);

	return 0;
}

static int
irc_info(struct server *s, struct irc_message *m)
{
	/* Generic info message handling */

	return irc_message(s, m, FROM_INFO);
}

static int
irc_001(struct server *s, struct irc_message *m)
{
	/* 001 :<Welcome message> */

	char *trailing;
	struct channel *c = s->channel;

	do {
		if (c->type == CHANNEL_T_CHANNEL && !c->parted)
			sendf(s, "JOIN %s", c->name);
	} while ((c = c->next) != s->channel);

	if (irc_message_split(m, &trailing))
		newline(s->channel, 0, FROM_INFO, trailing);

	server_info(s, "You are known as %s", s->nick);

	return 0;
}

static int
irc_004(struct server *s, struct irc_message *m)
{
	/* 004 1*<params> [:message] */

	char *trailing;

	if (irc_message_split(m, &trailing))
		newlinef(s->channel, 0, FROM_INFO, "%s ~ %s", m->params, trailing);
	else
		newlinef(s->channel, 0, FROM_INFO, "%s", m->params);

	server_set_004(s, m->params);

	return 0;
}

static int
irc_005(struct server *s, struct irc_message *m)
{
	/* 005 1*<params> [:message] */

	char *trailing;

	if (irc_message_split(m, &trailing))
		newlinef(s->channel, 0, FROM_INFO, "%s ~ %s", m->params, trailing);
	else
		newlinef(s->channel, 0, FROM_INFO, "%s ~ are supported by this server", m->params);

	server_set_005(s, m->params);

	return 0;
}

static int
irc_221(struct server *s, struct irc_message *m)
{
	/* 221 <modestring> */

	return recv_mode_usermodes(m, &(s->mode_cfg), s);
}

static int
irc_324(struct server *s, struct irc_message *m)
{
	/* 324 <channel> 1*[<modestring> [<mode arguments>]] */

	char *chan;
	struct channel *c;

	if (!irc_message_param(m, &chan))
		failf(s, "RPL_CHANNELMODEIS: channel is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "RPL_CHANNELMODEIS: channel '%s' not found", chan);

	return recv_mode_chanmodes(m, &(s->mode_cfg), s, c);
}

static int
irc_328(struct server *s, struct irc_message *m)
{
	/* 328 <channel> <url> */

	char *chan;
	char *url;
	struct channel *c;

	if (!irc_message_param(m, &chan))
		failf(s, "RPL_CHANNEL_URL: channel is null");

	if (!irc_message_param(m, &url))
		failf(s, "RPL_CHANNEL_URL: url is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "RPL_CHANNEL_URL: channel '%s' not found", chan);

	newlinef(c, 0, FROM_INFO, "URL for %s is: \"%s\"", chan, url);

	return 0;
}

static int
irc_329(struct server *s, struct irc_message *m)
{
	char buf[sizeof("1970-01-01T00:00:00")];
	char *chan;
	char *time_str;
	struct channel *c;
	struct tm tm;
	time_t t = 0;

	if (!irc_message_param(m, &chan))
		failf(s, "RPL_CREATIONTIME: channel is null");

	if (!irc_message_param(m, &time_str))
		failf(s, "RPL_CREATIONTIME: time is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "RPL_CREATIONTIME: channel '%s' not found", chan);

	errno = 0;
	t = strtoul(time_str, NULL, 0);

	if (errno)
		failf(s, "RPL_CREATIONTIME: strtoul error: %s", strerror(errno));

	if (gmtime_r(&t, &tm) == NULL)
		failf(s, "RPL_CREATIONTIME: gmtime_r error: %s", strerror(errno));

	if ((strftime(buf, sizeof(buf), "%FT%T", &tm)) == 0)
		failf(s, "RPL_CREATIONTIME: strftime error");

	newlinef(c, 0, FROM_INFO, "Channel created %s", buf);

	return 0;
}

static int
irc_332(struct server *s, struct irc_message *m)
{
	/* 332 <channel> <topic> */

	char *chan;
	char *topic;
	struct channel *c;

	if (!irc_message_param(m, &chan))
		failf(s, "RPL_TOPIC: channel is null");

	if (!irc_message_param(m, &topic))
		failf(s, "RPL_TOPIC: topic is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "RPL_TOPIC: channel '%s' not found", chan);

	newlinef(c, 0, FROM_INFO, "Topic for %s is \"%s\"", chan, topic);

	return 0;
}

static int
irc_333(struct server *s, struct irc_message *m)
{
	/* 333 <channel> <nick> <time> */

	char buf[sizeof("1970-01-01T00:00:00")];
	char *chan;
	char *nick;
	char *time_str;
	struct channel *c;
	struct tm tm;
	time_t t = 0;

	if (!irc_message_param(m, &chan))
		failf(s, "RPL_TOPICWHOTIME: channel is null");

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_TOPICWHOTIME: nick is null");

	if (!irc_message_param(m, &time_str))
		failf(s, "RPL_TOPICWHOTIME: time is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "RPL_TOPICWHOTIME: channel '%s' not found", chan);

	errno = 0;
	t = strtoul(time_str, NULL, 0);

	if (errno)
		failf(s, "RPL_TOPICWHOTIME: strtoul error: %s", strerror(errno));

	if (gmtime_r(&t, &tm) == NULL)
		failf(s, "RPL_TOPICWHOTIME: gmtime_r error: %s", strerror(errno));

	if ((strftime(buf, sizeof(buf), "%FT%T", &tm)) == 0)
		failf(s, "RPL_TOPICWHOTIME: strftime error");

	newlinef(c, 0, FROM_INFO, "Topic set by %s, %s", nick, buf);

	return 0;
}

static int
irc_353(struct server *s, struct irc_message *m)
{
	/* 353 ("="/"*"/"@") <channel> *([ "@" / "+" ]<nick>) */

	char *chan;
	char *nick;
	char *nicks;
	char *type;
	struct channel *c;

	if (!irc_message_param(m, &type))
		failf(s, "RPL_NAMEREPLY: type is null");

	if (!irc_message_param(m, &chan))
		failf(s, "RPL_NAMEREPLY: channel is null");

	if (!irc_message_param(m, &nicks))
		failf(s, "RPL_NAMEREPLY: nicks is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "RPL_NAMEREPLY: channel '%s' not found", chan);

	if (mode_chanmode_prefix(&(c->chanmodes), &(s->mode_cfg), *type) != MODE_ERR_NONE)
		newlinef(c, 0, FROM_ERROR, "RPL_NAMEREPLY: invalid channel flag: '%c'", *type);

	if ((nick = strsep(&nicks))) {
		do {
			char prefix = 0;
			struct mode m = MODE_EMPTY;

			while (!irc_isnickchar(*nick, 1)) {

				prefix = *nick++;

				if (mode_prfxmode_prefix(&m, &(s->mode_cfg), prefix) != MODE_ERR_NONE)
					newlinef(c, 0, FROM_ERROR, "Invalid user prefix: '%c'", prefix);
			}

			if (user_list_add(&(c->users), s->casemapping, nick, m) == USER_ERR_DUPLICATE)
				newlinef(c, 0, FROM_ERROR, "Duplicate nick: '%s'", nick);

		} while ((nick = strsep(&nicks)));
	}

	draw_status();

	return 0;
}

static int
irc_433(struct server *s, struct irc_message *m)
{
	/* 433 <nick> :Nickname is already in use */

	char *nick;

	if (!irc_message_param(m, &nick))
		failf(s, "ERR_NICKNAMEINUSE: nick is null");

	newlinef(s->channel, 0, FROM_ERROR, "Nick '%s' in use", nick);

	if (!strcmp(nick, s->nick)) {
		server_nicks_next(s);
		newlinef(s->channel, 0, FROM_ERROR, "Trying again with '%s'", s->nick);
		sendf(s, "NICK %s", s->nick);
	}

	return 0;
}

static int
irc_recv_numeric(struct server *s, struct irc_message *m)
{
	/* :server <code> <target> [args] */

	char *targ;
	int code = 0;
	irc_recv_f handler = NULL;

	for (const char *p = m->command; *p; p++) {

		if (!isdigit(*p))
			failf(s, "NUMERIC: invalid");

		code *= 10;
		code += *p - '0';

		if (code > 999)
			failf(s, "NUMERIC: out of range");
	}

	// TODO: 353 ENDs the CAP negotiation

	/* Message target is only used to establish s->nick when registering with a server */
	if (!(irc_message_param(m, &targ))) {
		io_dx(s->connection);
		failf(s, "NUMERIC: target is null");
	}

	/* Message target should match s->nick or '*' if unregistered, otherwise out of sync */
	if (strcmp(targ, s->nick) && strcmp(targ, "*") && code != 1) {
		io_dx(s->connection);
		failf(s, "NUMERIC: target mismatched, nick is '%s', received '%s'", s->nick, targ);
	}

	if (ARR_ELEM(irc_numerics, code))
		handler = irc_numerics[code];

	if (handler)
		return (*handler)(s, m);

	if (m->params)
		failf(s, "Numeric type '%u' unknown: %s", code, m->params);
	else
		failf(s, "Numeric type '%u' unknown", code);
}

static int
recv_cap(struct server *s, struct irc_message *m)
{
	return ircv3_CAP(s, m);
}

static int
recv_error(struct server *s, struct irc_message *m)
{
	/* ERROR <message> */

	char *message;

	if (!irc_message_param(m, &message))
		failf(s, "ERROR: message is null");

	newlinef(s->channel, 0, (s->quitting ? FROM_INFO : "ERROR"), "%s", message);

	return 0;
}

static int
recv_invite(struct server *s, struct irc_message *m)
{
	/* :nick!user@host INVITE <nick> <channel> */

	char *chan;
	char *nick;

	if (!m->from)
		failf(s, "INVITE: sender's nick is null");

	if (!irc_message_param(m, &chan))
		failf(s, "INVITE: target channel is null");

	if (!irc_message_param(m, &nick))
		failf(s, "INVITE: target nick is null");

	if (!strcmp(nick, s->nick))
		newlinef(s->channel, 0, FROM_INFO, "You invited %s to %s", nick, chan);
	else
		newlinef(s->channel, 0, FROM_INFO, "You've been invited to %s by %s", chan, m->from);

	return 0;
}

static int
recv_join(struct server *s, struct irc_message *m)
{
	/* :nick!user@host JOIN <channel> */

	char *chan;
	struct channel *c;

	if (!m->from)
		failf(s, "JOIN: sender's nick is null");

	if (!irc_message_param(m, &chan))
		failf(s, "JOIN: target channel is null");

	if (!strcmp(m->from, s->nick)) {
		if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL) {
			c = channel(chan, CHANNEL_T_CHANNEL);
			c->server = s;
			channel_list_add(&s->clist, c);
			channel_set_current(c);
		}
		c->joined = 1;
		c->parted = 0;
		newlinef(c, BUFFER_LINE_JOIN, FROM_JOIN, "Joined %s", chan);
		sendf(s, "MODE %s", chan);
		draw_all();
		return 0;
	}

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "JOIN: channel '%s' not found", chan);

	if (user_list_add(&(c->users), s->casemapping, m->from, MODE_EMPTY) == USER_ERR_DUPLICATE)
		failf(s, "JOIN: user '%s' alread on channel '%s'", m->from, chan);

	if (!join_threshold || c->users.count <= join_threshold)
		newlinef(c, BUFFER_LINE_JOIN, FROM_JOIN, "%s!%s has joined", m->from, m->host);

	draw_status();

	return 0;
}

static int
recv_kick(struct server *s, struct irc_message *m)
{
	/* :nick!user@host KICK <channel> <user> [message] */

	char *chan;
	char *message;
	char *user;
	struct channel *c;

	if (!m->from)
		failf(s, "KICK: sender's nick is null");

	if (!irc_message_param(m, &chan))
		failf(s, "KICK: channel is null");

	if (!irc_message_param(m, &user))
		failf(s, "KICK: user is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "KICK: channel '%s' not found", chan);

	irc_message_param(m, &message);

	/* RFC 2812, section 3.2.8:
	 *
	 * If a "comment" is given, this will be sent instead of the
	 * default message, the nickname of the user issuing the KICK */
	if (message && !strcmp(m->from, message))
		message = NULL;

	if (!strcmp(user, s->nick)) {

		channel_part(c);

		if (message)
			newlinef(c, 0, FROM_INFO, "Kicked by %s (%s)", m->from, message);
		else
			newlinef(c, 0, FROM_INFO, "Kicked by %s", m->from);

	} else {

		if (user_list_del(&(c->users), s->casemapping, user) == USER_ERR_NOT_FOUND)
			failf(s, "KICK: nick '%s' not found in '%s'", user, chan);

		if (message)
			newlinef(c, 0, FROM_INFO, "%s has kicked %s (%s)", m->from, user, message);
		else
			newlinef(c, 0, FROM_INFO, "%s has kicked %s", m->from, user);
	}

	draw_status();

	return 0;
}

static int
recv_mode(struct server *s, struct irc_message *m)
{
	/* MODE <targ> 1*[<modestring> [<mode arguments>]]
	 *
	 * modestring  =  1*(modeset)
	 * modeset     =  plusminus *(modechar)
	 * plusminus   =  %x53 / %x55            ; '+' / '-'
	 * modechar    =  ALPHA
	 *
	 * Any number of mode flags can be set or unset in a MODE message, but
	 * the maximum number of modes with parameters is given by the server's
	 * MODES configuration.
	 *
	 * Mode flags that require a parameter are configured as the server's
	 * CHANMODE subtypes; A,B,C,D
	 *
	 * The following formats are equivalent, if e.g.:
	 *  - 'a' and 'c' require parameters
	 *  - 'b' has no parameter
	 *
	 *   MODE <channel> +ab  <param a> +c <param c>
	 *   MODE <channel> +abc <param a>    <param c>
	 */

	char *targ;
	struct channel *c;

	if (!irc_message_param(m, &targ))
		failf(s, "NICK: new nick is null");

	if (!strcmp(targ, s->nick))
		return recv_mode_usermodes(m, &(s->mode_cfg), s);

	if ((c = channel_list_get(&s->clist, targ, s->casemapping)))
		return recv_mode_chanmodes(m, &(s->mode_cfg), s, c);

	failf(s, "MODE: target '%s' not found", targ);
}

static int
recv_mode_chanmodes(struct irc_message *m, const struct mode_cfg *cfg, struct server *s, struct channel *c)
{
	char flag;
	char *modestring;
	char *modearg;
	enum mode_err_t mode_err;
	enum mode_set_t mode_set;
	struct mode *chanmodes = &(c->chanmodes);
	struct user *user;

	// TODO: mode string segfaults if args out of order

	if (!irc_message_param(m, &modestring)) {
		newlinef(c, 0, FROM_ERROR, "MODE: modestring is null");
		return 1;
	}

	do {
		mode_set = MODE_SET_INVALID;
		mode_err = MODE_ERR_NONE;

		while ((flag = *modestring++)) {

			if (flag == '+') {
				mode_set = MODE_SET_ON;
				continue;
			}

			if (flag == '-') {
				mode_set = MODE_SET_OFF;
				continue;
			}

			modearg = NULL;

			switch (chanmode_type(cfg, mode_set, flag)) {

				/* Doesn't consume an argument */
				case MODE_FLAG_CHANMODE:

					mode_err = mode_chanmode_set(chanmodes, cfg, flag, mode_set);

					if (mode_err == MODE_ERR_NONE) {
						newlinef(c, 0, FROM_INFO, "%s%s%s mode: %c%c",
								(m->from ? m->from : ""),
								(m->from ? " set " : ""),
								c->name,
								(mode_set == MODE_SET_ON ? '+' : '-'),
								flag);
					}
					break;

				/* Consumes an argument */
				case MODE_FLAG_CHANMODE_PARAM:

					if (!irc_message_param(m, &modearg)) {
						newlinef(c, 0, FROM_ERROR, "MODE: flag '%c' expected argument", flag);
						continue;
					}

					mode_err = mode_chanmode_set(chanmodes, cfg, flag, mode_set);

					if (mode_err == MODE_ERR_NONE) {
						newlinef(c, 0, FROM_INFO, "%s%s%s mode: %c%c %s",
								(m->from ? m->from : ""),
								(m->from ? " set " : ""),
								c->name,
								(mode_set == MODE_SET_ON ? '+' : '-'),
								flag,
								modearg);
					}
					break;

				/* Consumes an argument and sets a usermode */
				case MODE_FLAG_PREFIX:

					if (!irc_message_param(m, &modearg)) {
						newlinef(c, 0, FROM_ERROR, "MODE: flag '%c' argument is null", flag);
						continue;
					}

					if (!(user = user_list_get(&(c->users), s->casemapping, modearg, 0))) {
						newlinef(c, 0, FROM_ERROR, "MODE: flag '%c' user '%s' not found", flag, modearg);
						continue;
					}

					mode_prfxmode_set(&(user->prfxmodes), cfg, flag, mode_set);

					if (mode_err == MODE_ERR_NONE) {
						newlinef(c, 0, FROM_INFO, "%s%suser %s mode: %c%c",
								(m->from ? m->from : ""),
								(m->from ? " set " : ""),
								modearg,
								(mode_set == MODE_SET_ON ? '+' : '-'),
								flag);
					}
					break;

				case MODE_FLAG_INVALID_SET:
					mode_err = MODE_ERR_INVALID_SET;
					break;

				case MODE_FLAG_INVALID_FLAG:
					mode_err = MODE_ERR_INVALID_FLAG;
					break;

				default:
					newlinef(c, 0, FROM_ERROR, "MODE: unhandled error, flag '%c'");
					continue;
			}

			switch (mode_err) {

				case MODE_ERR_INVALID_FLAG:
					newlinef(c, 0, FROM_ERROR, "MODE: invalid flag '%c'", flag);
					break;

				case MODE_ERR_INVALID_SET:
					newlinef(c, 0, FROM_ERROR, "MODE: missing '+'/'-'");
					break;

				default:
					break;
			}
		}
	} while (irc_message_param(m, &modestring));

	mode_str(&(c->chanmodes), &(c->chanmodes_str));
	draw_status();

	return 0;
}

static int
recv_mode_usermodes(struct irc_message *m, const struct mode_cfg *cfg, struct server *s)
{
	char flag;
	char *modestring;
	enum mode_err_t mode_err;
	enum mode_set_t mode_set;
	struct mode *usermodes = &(s->usermodes);

	if (!irc_message_param(m, &modestring))
		failf(s, "MODE: modestring is null");

	do {
		mode_set = MODE_SET_INVALID;

		while ((flag = *modestring++)) {

			if (flag == '+') {
				mode_set = MODE_SET_ON;
				continue;
			}

			if (flag == '-') {
				mode_set = MODE_SET_OFF;
				continue;
			}

			mode_err = mode_usermode_set(usermodes, cfg, flag, mode_set);

			if (mode_err == MODE_ERR_NONE)
				newlinef(s->channel, 0, FROM_INFO, "%s%smode: %c%c",
						(m->from ? m->from : ""),
						(m->from ? " set " : ""),
						(mode_set == MODE_SET_ON ? '+' : '-'),
						flag);

			else if (mode_err == MODE_ERR_INVALID_SET)
				newlinef(s->channel, 0, FROM_ERROR, "MODE: missing '+'/'-'");

			else if (mode_err == MODE_ERR_INVALID_FLAG)
				newlinef(s->channel, 0, FROM_ERROR, "MODE: invalid flag '%c'", flag);
		}
	} while (irc_message_param(m, &modestring));

	mode_str(usermodes, &(s->mode_str));
	draw_status();

	return 0;
}

static int
recv_nick(struct server *s, struct irc_message *m)
{
	/* :nick!user@host NICK <nick> */

	char *nick;
	struct channel *c = s->channel;

	if (!m->from)
		failf(s, "NICK: old nick is null");

	if (!irc_message_param(m, &nick))
		failf(s, "NICK: new nick is null");

	if (!strcmp(m->from, s->nick)) {
		server_nick_set(s, nick);
		newlinef(s->channel, BUFFER_LINE_NICK, FROM_NICK, "Youn nick is '%s'", nick);
	}

	do {
		enum user_err ret;

		if ((ret = user_list_rpl(&(c->users), s->casemapping, m->from, nick)) == USER_ERR_NONE)
			newlinef(c, BUFFER_LINE_NICK, FROM_NICK, "%s  >>  %s", m->from, nick);

		else if (ret == USER_ERR_DUPLICATE)
			server_error(s, "NICK: user '%s' alread on channel '%s'", m->from, c->name);

	} while ((c = c->next) != s->channel);

	return 0;
}

static int
recv_notice(struct server *s, struct irc_message *m)
{
	/* :nick!user@host NOTICE <target> <message> */

	char *message;
	char *target;
	int urgent = 0;
	struct channel *c;

	if (!m->from)
		failf(s, "NOTICE: sender's nick is null");

	if (!irc_message_param(m, &target))
		failf(s, "NOTICE: target is null");

	if (!irc_message_param(m, &message))
		failf(s, "NOTICE: message is null");

	if (user_list_get(&(s->ignore), s->casemapping, m->from, 0))
		return 0;

	if (IS_CTCP(message))
		return ctcp_response(s, m->from, target, message);

	if (!strcmp(target, "*")) {
		c = s->channel;
	} else if (!strcmp(target, s->nick)) {

		if ((c = channel_list_get(&s->clist, m->from, s->casemapping)) == NULL) {
			c = channel(m->from, CHANNEL_T_PRIVATE);
			c->server = s;
			channel_list_add(&s->clist, c);
		}

		if (c != current_channel())
			urgent = 1;

	} else if ((c = channel_list_get(&s->clist, target, s->casemapping)) == NULL) {
		failf(s, "NOTICE: channel '%s' not found", target);
	}

	if (irc_pinged(s->casemapping, message, s->nick)) {

		if (c != current_channel())
			urgent = 1;

		newline(c, BUFFER_LINE_PINGED, m->from, message);
	} else {
		newline(c, BUFFER_LINE_CHAT, m->from, message);
	}

	if (urgent) {
		c->activity = ACTIVITY_PINGED;
		draw_bell();
		draw_nav();
	}

	return 0;
}

static int
recv_part(struct server *s, struct irc_message *m)
{
	/* :nick!user@host PART <channel> [message] */

	char *chan;
	char *message;
	struct channel *c;

	if (!m->from)
		failf(s, "PART: sender's nick is null");

	if (!irc_message_param(m, &chan))
		failf(s, "PART: target channel is null");

	if (!strcmp(m->from, s->nick)) {

		/* If not found, assume channel was closed */
		if ((c = channel_list_get(&s->clist, chan, s->casemapping)) != NULL) {

			if (irc_message_param(m, &message))
				newlinef(c, BUFFER_LINE_PART, FROM_PART, "you have parted (%s)", message);
			else
				newlinef(c, BUFFER_LINE_PART, FROM_PART, "you have parted");

			channel_part(c);
		}
	} else {

		if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
			failf(s, "PART: channel '%s' not found", chan);

		if (user_list_del(&(c->users), s->casemapping, m->from) == USER_ERR_NOT_FOUND)
			failf(s, "PART: nick '%s' not found in '%s'", m->from, chan);

		if (!part_threshold || c->users.count <= part_threshold) {
			if (irc_message_param(m, &message))
				newlinef(c, 0, FROM_PART, "%s!%s has parted (%s)", m->from, m->host, message);
			else
				newlinef(c, 0, FROM_PART, "%s!%s has parted", m->from, m->host);
		}
	}

	draw_status();

	return 0;
}

static int
recv_ping(struct server *s, struct irc_message *m)
{
	/* PING <server> */

	char *server;

	if (!irc_message_param(m, &server))
		failf(s, "PING: server is null");

	sendf(s, "PONG %s", server);

	return 0;
}

static int
recv_pong(struct server *s, struct irc_message *m)
{
	/*  PONG <server> [<server2>] */

	UNUSED(s);
	UNUSED(m);

	return 0;
}

static int
recv_privmsg(struct server *s, struct irc_message *m)
{
	/* :nick!user@host PRIVMSG <target> <message> */

	char *message;
	char *target;
	int urgent = 0;
	struct channel *c;

	if (!m->from)
		failf(s, "PRIVMSG: sender's nick is null");

	if (!irc_message_param(m, &target))
		failf(s, "PRIVMSG: target is null");

	if (!irc_message_param(m, &message))
		failf(s, "PRIVMSG: message is null");

	if (user_list_get(&(s->ignore), s->casemapping, m->from, 0))
		return 0;

	if (IS_CTCP(message))
		return ctcp_request(s, m->from, target, message);

	if (!strcmp(target, s->nick)) {

		if ((c = channel_list_get(&s->clist, m->from, s->casemapping)) == NULL) {
			c = channel(m->from, CHANNEL_T_PRIVATE);
			c->server = s;
			channel_list_add(&s->clist, c);
		}

		if (c != current_channel())
			urgent = 1;

	} else if ((c = channel_list_get(&s->clist, target, s->casemapping)) == NULL) {
		failf(s, "PRIVMSG: channel '%s' not found", target);
	}

	if (irc_pinged(s->casemapping, message, s->nick)) {

		if (c != current_channel())
			urgent = 1;

		newline(c, BUFFER_LINE_PINGED, m->from, message);
	} else {
		newline(c, BUFFER_LINE_CHAT, m->from, message);
	}

	if (urgent) {
		c->activity = ACTIVITY_PINGED;
		draw_bell();
		draw_nav();
	}

	return 0;
}

static int
recv_topic(struct server *s, struct irc_message *m)
{
	/* :nick!user@host TOPIC <channel> [topic] */

	char *chan;
	char *topic;
	struct channel *c;

	if (!m->from)
		failf(s, "TOPIC: sender's nick is null");

	if (!irc_message_param(m, &chan))
		failf(s, "TOPIC: target channel is null");

	if (!irc_message_param(m, &topic))
		failf(s, "TOPIC: topic is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "TOPIC: target channel '%s' not found", chan);

	if (*topic) {
		newlinef(c, 0, FROM_INFO, "%s has changed the topic:", m->from);
		newlinef(c, 0, FROM_INFO, "\"%s\"", topic);
	} else {
		newlinef(c, 0, FROM_INFO, "%s has unset the topic", m->from);
	}

	return 0;
}

static int
recv_quit(struct server *s, struct irc_message *m)
{
	/* :nick!user@host QUIT [message] */

	char *message = NULL;
	struct channel *c = s->channel;

	if (!m->from)
		failf(s, "QUIT: sender's nick is null");

	irc_message_param(m, &message);

	do {
		if (user_list_del(&(c->users), s->casemapping, m->from) == USER_ERR_NONE) {
			if (!quit_threshold || c->users.count <= quit_threshold) {
				if (message)
					newlinef(c, BUFFER_LINE_QUIT, FROM_QUIT, "%s!%s has quit (%s)", m->from, m->host, message);
				else
					newlinef(c, BUFFER_LINE_QUIT, FROM_QUIT, "%s!%s has quit", m->from, m->host);
			}
		}
	} while ((c = c->next) != s->channel);

	draw_status();

	return 0;
}

#undef failf
#undef sendf
