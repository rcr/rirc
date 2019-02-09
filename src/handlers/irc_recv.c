#include <ctype.h>

#include "src/components/server.h"
#include "src/handlers/irc_ctcp.h"
#include "src/handlers/irc_recv.gperf.out"
#include "src/handlers/irc_recv.h"
#include "src/io.h"
#include "src/state.h"
#include "src/utils/utils.h"

#define failf(S, ...) \
	do { server_err((S), __VA_ARGS__); \
	     return 1; \
	} while (0)

#define sendf(S, ...) \
	do { int ret; \
	     if ((ret = io_sendf((S)->connection, __VA_ARGS__))) \
	         failf((S), "Send fail: %s", io_err(ret)); \
	     return 0; \
	} while (0)

/* Default message handler */
static int irc_message(struct server*, struct irc_message*, const char*);

/* Generic handlers */
static int irc_error(struct server*, struct irc_message*);
static int irc_ignore(struct server*, struct irc_message*);
static int irc_info(struct server*, struct irc_message*);
static int irc_unknown(struct server*, struct irc_message*);

/* Numeric handlers */
static int irc_001(struct server*, struct irc_message*);
static int irc_004(struct server*, struct irc_message*);
static int irc_005(struct server*, struct irc_message*);

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
	[221] = NULL,       /* RPL_UMODEIS */
	[234] = irc_ignore, /* RPL_SERVLIST */
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
	[301] = NULL,       /* RPL_AWAY */
	[302] = NULL,       /* ERR_USERHOST */
	[303] = NULL,       /* RPL_ISON */
	[305] = NULL,       /* RPL_UNAWAY */
	[306] = NULL,       /* RPL_NOWAWAY */
	[311] = NULL,       /* RPL_WHOISUSER */
	[312] = NULL,       /* RPL_WHOISSERVER */
	[313] = NULL,       /* RPL_WHOISOPERATOR */
	[314] = NULL,       /* RPL_WHOWASUSER */
	[315] = NULL,       /* RPL_ENDOFWHO */
	[317] = NULL,       /* RPL_WHOISIDLE */
	[318] = NULL,       /* RPL_ENDOFWHOIS */
	[319] = NULL,       /* RPL_WHOISCHANNELS */
	[322] = NULL,       /* RPL_LIST */
	[323] = NULL,       /* RPL_LISTEND */
	[324] = NULL,       /* RPL_CHANNELMODEIS */
	[325] = NULL,       /* RPL_UNIQOPIS */
	[328] = NULL,       /* RPL_CHANNEL_URL */
	[331] = irc_ignore, /* RPL_NOTOPIC */
	[332] = NULL,       /* RPL_TOPIC */
	[333] = NULL,       /* RPL_TOPICWHOTIME */
	[341] = NULL,       /* RPL_INVITING */
	[346] = NULL,       /* RPL_INVITELIST */
	[347] = irc_ignore, /* RPL_ENDOFINVITELIST */
	[348] = NULL,       /* RPL_EXCEPTLIST */
	[349] = irc_ignore, /* RPL_ENDOFEXCEPTLIST */
	[351] = NULL,       /* RPL_VERSION */
	[352] = NULL,       /* RPL_WHOREPLY */
	[353] = NULL,       /* RPL_NAMREPLY */
	[364] = NULL,       /* RPL_LINKS */
	[365] = irc_ignore, /* RPL_ENDOFLINKS */
	[366] = irc_ignore, /* RPL_ENDOFNAMES */
	[367] = NULL,       /* RPL_BANLIST */
	[368] = NULL,       /* RPL_ENDOFBANLIST */
	[369] = NULL,       /* RPL_ENDOFWHOWAS */
	[371] = irc_info,   /* RPL_INFO */
	[372] = irc_info,   /* RPL_MOTD */
	[374] = irc_ignore, /* RPL_ENDOFINFO */
	[375] = irc_ignore, /* RPL_MOTDSTART */
	[376] = irc_ignore, /* RPL_ENDOFMOTD */
	[381] = NULL,       /* RPL_YOUREOPER */
	[391] = NULL,       /* RPL_TIME */
	[401] = NULL,       /* ERR_NOSUCHNICK */
	[402] = NULL,       /* ERR_NOSUCHSERVER */
	[403] = NULL,       /* ERR_NOSUCHCHANNEL */
	[404] = NULL,       /* ERR_CANNOTSENDTOCHAN */
	[405] = NULL,       /* ERR_TOOMANYCHANNELS */
	[406] = NULL,       /* ERR_WASNOSUCHNICK */
	[407] = NULL,       /* ERR_TOOMANYTARGETS */
	[408] = NULL,       /* ERR_NOSUCHSERVICE */
	[409] = NULL,       /* ERR_NOORIGIN */
	[411] = NULL,       /* ERR_NORECIPIENT */
	[412] = NULL,       /* ERR_NOTEXTTOSEND */
	[413] = NULL,       /* ERR_NOTOPLEVEL */
	[414] = NULL,       /* ERR_WILDTOPLEVEL */
	[415] = NULL,       /* ERR_BADMASK */
	[416] = NULL,       /* ERR_TOOMANYMATCHES */
	[421] = NULL,       /* ERR_UNKNOWNCOMMAND */
	[422] = NULL,       /* ERR_NOMOTD */
	[423] = NULL,       /* ERR_NOADMININFO */
	[431] = NULL,       /* ERR_NONICKNAMEGIVEN */
	[432] = NULL,       /* ERR_ERRONEUSNICKNAME */
	[433] = NULL,       /* ERR_NICKNAMEINUSE */
	[436] = NULL,       /* ERR_NICKCOLLISION */
	[437] = NULL,       /* ERR_UNAVAILRESOURCE */
	[441] = NULL,       /* ERR_USERNOTINCHANNEL */
	[442] = NULL,       /* ERR_NOTONCHANNEL */
	[443] = NULL,       /* ERR_USERONCHANNEL */
	[451] = NULL,       /* ERR_NOTREGISTERED */
	[461] = NULL,       /* ERR_NEEDMOREPARAMS */
	[462] = NULL,       /* ERR_ALREADYREGISTRED */
	[463] = NULL,       /* ERR_NOPERMFORHOST */
	[464] = NULL,       /* ERR_PASSWDMISMATCH */
	[465] = NULL,       /* ERR_YOUREBANNEDCREEP */
	[466] = NULL,       /* ERR_YOUWILLBEBANNED */
	[467] = NULL,       /* ERR_KEYSET */
	[471] = NULL,       /* ERR_CHANNELISFULL */
	[472] = NULL,       /* ERR_UNKNOWNMODE */
	[473] = irc_error,  /* ERR_INVITEONLYCHAN */
	[474] = NULL,       /* ERR_BANNEDFROMCHAN */
	[475] = NULL,       /* ERR_BADCHANNELKEY */
	[476] = NULL,       /* ERR_BADCHANMASK */
	[477] = irc_error,  /* ERR_NOCHANMODES */
	[478] = NULL,       /* ERR_BANLISTFULL */
	[481] = NULL,       /* ERR_NOPRIVILEGES */
	[482] = NULL,       /* ERR_CHANOPRIVSNEEDED */
	[483] = NULL,       /* ERR_CANTKILLSERVER */
	[484] = NULL,       /* ERR_RESTRICTED */
	[485] = NULL,       /* ERR_UNIQOPPRIVSNEEDED */
	[491] = NULL,       /* ERR_NOOPERHOST */
	[501] = NULL,       /* ERR_UMODEUNKNOWNFLAG */
	[502] = NULL,       /* ERR_USERSDONTMATCH */
	[704] = irc_ignore, /* RPL_HELPSTART */
	[705] = irc_info,   /* RPL_HELP */
	[706] = irc_ignore, /* RPL_ENDOFHELP */
};

static int
irc_message(struct server *s, struct irc_message *m, const char *from)
{
	char *trailing;

	if (!str_trim(&m->params))
		return 0;

	if (irc_message_split(m, &trailing)) {
		if (m->params)
			server_message(s, from, "[%s] ~ %s", m->params, trailing);
		else
			server_message(s, from, "%s", trailing);
	} else if (m->params) {
		server_message(s, from, "[%s]", m->params);
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
irc_unknown(struct server *s, struct irc_message *m)
{
	/* Unknown message handling */

	return irc_message(s, m, FROM_UNKNOWN);
}

static int
irc_001(struct server *s, struct irc_message *m)
{
	/* 001 :<Welcome message> */

	char *trailing;
	struct channel *c = s->channel;

	/* join non-parted channels */
	do {
		if (c->type == CHANNEL_T_CHANNEL && !c->parted)
			sendf(s, "JOIN %s", c->name);
		c = c->next;
	} while (c != s->channel);

	if (irc_message_split(m, &trailing))
		newline(s->channel, 0, "--", trailing);

	newlinef(s->channel, 0, "--", "You are known as %s", s->nick);
	return 0;
}

static int
irc_004(struct server *s, struct irc_message *m)
{
	/* 004 1*<params> [:message] */

	char *trailing;

	if (irc_message_split(m, &trailing))
		newlinef(s->channel, 0, "--", "%s ~ %s", m->params, trailing);
	else
		newlinef(s->channel, 0, "--", "%s", m->params);

	server_set_004(s, m->params);
	return 0;
}

static int
irc_005(struct server *s, struct irc_message *m)
{
	/* 005 1*<params> [:message] */

	char *trailing;

	if (irc_message_split(m, &trailing))
		newlinef(s->channel, 0, "--", "%s ~ %s", m->params, trailing);
	else
		newlinef(s->channel, 0, "--", "%s ~ are supported by this server", m->params);

	server_set_005(s, m->params);
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

	failf(s, "Numeric type '%u' unknown", code);
}

int
irc_recv(struct server *s, struct irc_message *m)
{
	const struct recv_handler* handler;

	if (isdigit(*m->command))
		return irc_recv_numeric(s, m);

	if ((handler = recv_handler_lookup(m->command, m->len_command)))
		return handler->f(s, m);

	return irc_unknown(s, m);
}

static int recv_error(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_invite(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_join(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_kick(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_mode(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_nick(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_notice(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_part(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_ping(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_pong(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_privmsg(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_quit(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }
static int recv_topic(struct server *s, struct irc_message *m) { (void)s; (void)m; return 0; }

#undef failf
#undef sendf
