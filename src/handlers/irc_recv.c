#include "src/handlers/irc_recv.h"

// TODO: test these with the test-servers.sh script with/without privmsg buffer,
// requires connecting with at least 3 instances


// TODO: need explicit handling of:
// * (352) RPL_WHOREPLY


// TODO: ???
// -??- ~ [309] [CTCPServ] is a Network Service



// TODO:
// - show a better version of /who output
// - fix grammar of messages that are responses to whois and whowas
//   - either make them neutral ("is"/"was") or look at what command is being responded to
//   - e.g. "is connected to ..." is replied from whowas
// - fix consistent who/whois/whowas message responses, gather an example of ALL of them,
//   line them up here and come up with something consistent, like:
//   ~ (who <nick>) : <info>
//
// something like:
// ~ /who <nick>
// ~   ...
// ~   ...
// ~   ...
// ~ /who <nick> END


// who/whois/whowas related numerics:
#if 0
	 * (276) RPL_WHOISCERTFP
	 * (301) RPL_AWAY
	 * (307) RPL_WHOISREGNICK
	 * (311) RPL_WHOISUSER
	 * (312) RPL_WHOISSERVER
	 * (313) RPL_WHOISOPERATOR
	 * (314) RPL_WHOWASUSER
	 * (315) RPL_ENDOFWHO
	 * (317) RPL_WHOISIDLE
	 * (318) RPL_ENDOFWHOIS
	 * (319) RPL_WHOISCHANNELS
	 * (320) RPL_WHOISSPECIAL
	 * (330) RPL_WHOISACCOUNT
	 * (338) RPL_WHOISACTUALLY
	 * (352) RPL_WHOREPLY
	 * (369) RPL_ENDOFWHOWAS
	 * (378) RPL_WHOISHOST
	 * (379) RPL_WHOISMODES
	 * (401) ERR_NOSUCHNICK
	 * (402) ERR_NOSUCHSERVER
	 * (406) ERR_WASNOSUCHNICK
	 * (431) ERR_NONICKNAMEGIVEN
	 * (461) ERR_NEEDMOREPARAMS */
	 * (671) RPL_WHOISSECURE */
#endif



#include "config.h"
#include "src/components/server.h"
#include "src/draw.h"
#include "src/handlers/irc_ctcp.h"
#include "src/handlers/irc_recv.gperf.out"
#include "src/handlers/ircv3.h"
#include "src/io.h"
#include "src/state.h"
#include "src/utils/utils.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#define failf(S, ...) \
	do { server_error((S), __VA_ARGS__); \
	     return 1; \
	} while (0)

#define sendf(S, ...) \
	do { int ret; \
	     if ((ret = io_sendf((S)->connection, __VA_ARGS__))) \
	         failf((S), "Send fail: %s", io_err(ret)); \
	} while (0)

static int irc_generic(struct server*, struct irc_message*, const char*, const char*);
static int irc_generic_error(struct server*, struct irc_message*);
static int irc_generic_ignore(struct server*, struct irc_message*);
static int irc_generic_info(struct server*, struct irc_message*);
static int irc_generic_unknown(struct server*, struct irc_message*);
static int irc_recv_numeric(struct server*, struct irc_message*);
static int irc_recv_threshold_filter(unsigned, unsigned);
static int recv_mode_chanmodes(struct irc_message*, const struct mode_cfg*, struct server*, struct channel*);
static int recv_mode_usermodes(struct irc_message*, const struct mode_cfg*, struct server*);

/* List of explicitly handled IRC numeric replies */
#define IRC_RECV_NUMERICS \
	X(001) /* RPL_WELCOME */         \
	X(004) /* RPL_MYINFO */          \
	X(005) /* RPL_ISUPPORT */        \
	X(042) /* RPL_YOURID */          \
	X(221) /* RPL_UMODEIS */         \
	X(252) /* RPL_LUSEROP */         \
	X(253) /* RPL_LUSERUNKNOWN */    \
	X(254) /* RPL_LUSERCHANNELS */   \
	X(265) /* RPL_LOCALUSERS */      \
	X(266) /* RPL_GLOBALUSERS */     \
	X(275) /* RPL_USINGSSL */        \
	X(276) /* RPL_WHOISCERTFP */     \
	X(301) /* RPL_AWAY */            \
	X(307) /* RPL_WHOISREGNICK */    \
	X(311) /* RPL_WHOISUSER */       \
	X(312) /* RPL_WHOISSERVER */     \
	X(313) /* RPL_WHOISOPERATOR */   \
	X(314) /* RPL_WHOWASUSER */      \
	X(315) /* RPL_ENDOFWHO */        \
	X(317) /* RPL_WHOISIDLE */       \
	X(318) /* RPL_ENDOFWHOIS */      \
	X(319) /* RPL_WHOISCHANNELS */   \
	X(320) /* RPL_WHOISSPECIAL */    \
	X(324) /* RPL_CHANNELMODEIS */   \
	X(328) /* RPL_CHANNEL_URL */     \
	X(329) /* RPL_CREATIONTIME */    \
	X(330) /* RPL_WHOISACCOUNT */    \
	X(332) /* RPL_TOPIC */           \
	X(333) /* RPL_TOPICWHOTIME */    \
	X(338) /* RPL_WHOISACTUALLY */   \
	X(341) /* RPL_INVITING */        \
	X(352) /* RPL_WHOREPLY */        \
	X(353) /* RPL_NAMEREPLY */       \
	X(366) /* RPL_ENDOFNAMES */      \
	X(369) /* RPL_ENDOFWHOWAS */     \
	X(378) /* RPL_WHOISHOST */       \
	X(379) /* RPL_WHOISMODES */      \
	X(401) /* ERR_NOSUCHNICK */      \
	X(402) /* ERR_NOSUCHSERVER */    \
	X(403) /* ERR_NOSUCHCHANNEL */   \
	X(406) /* ERR_WASNOSUCHNICK */   \
	X(433) /* ERR_NICKNAMEINUSE */   \
	X(671) /* RPL_WHOISSECURE */     \
	X(716) /* RPL_TARGUMODEG */      \
	X(717) /* RPL_TARGNOTIFY */      \
	X(718) /* RPL_UMODEGMSG */       \

#define X(numeric) \
static int irc_recv_##numeric(struct server*, struct irc_message*);
IRC_RECV_NUMERICS
#undef X

static const irc_recv_f irc_numerics[] = {
	  [1] = irc_recv_001,       /* RPL_WELCOME */
	  [2] = irc_generic_info,   /* RPL_YOURHOST */
	  [3] = irc_generic_info,   /* RPL_CREATED */
	  [4] = irc_recv_004,       /* RPL_MYINFO */
	  [5] = irc_recv_005,       /* RPL_ISUPPORT */
	 [42] = irc_recv_042,       /* RPL_YOURID */
	[200] = irc_generic_info,   /* RPL_TRACELINK */
	[201] = irc_generic_info,   /* RPL_TRACECONNECTING */
	[202] = irc_generic_info,   /* RPL_TRACEHANDSHAKE */
	[203] = irc_generic_info,   /* RPL_TRACEUNKNOWN */
	[204] = irc_generic_info,   /* RPL_TRACEOPERATOR */
	[205] = irc_generic_info,   /* RPL_TRACEUSER */
	[206] = irc_generic_info,   /* RPL_TRACESERVER */
	[207] = irc_generic_info,   /* RPL_TRACESERVICE */
	[208] = irc_generic_info,   /* RPL_TRACENEWTYPE */
	[209] = irc_generic_info,   /* RPL_TRACECLASS */
	[210] = irc_generic_info,   /* RPL_TRACERECONNECT */
	[211] = irc_generic_info,   /* RPL_STATSLINKINFO */
	[212] = irc_generic_info,   /* RPL_STATSCOMMANDS */
	[213] = irc_generic_info,   /* RPL_STATSCLINE */
	[214] = irc_generic_info,   /* RPL_STATSNLINE */
	[215] = irc_generic_info,   /* RPL_STATSILINE */
	[216] = irc_generic_info,   /* RPL_STATSKLINE */
	[217] = irc_generic_info,   /* RPL_STATSQLINE */
	[218] = irc_generic_info,   /* RPL_STATSYLINE */
	[219] = irc_generic_ignore, /* RPL_ENDOFSTATS */
	[221] = irc_recv_221,       /* RPL_UMODEIS */
	[234] = irc_generic_info,   /* RPL_SERVLIST */
	[235] = irc_generic_ignore, /* RPL_SERVLISTEND */
	[240] = irc_generic_info,   /* RPL_STATSVLINE */
	[241] = irc_generic_info,   /* RPL_STATSLLINE */
	[242] = irc_generic_info,   /* RPL_STATSUPTIME */
	[243] = irc_generic_info,   /* RPL_STATSOLINE */
	[244] = irc_generic_info,   /* RPL_STATSHLINE */
	[245] = irc_generic_info,   /* RPL_STATSSLINE */
	[246] = irc_generic_info,   /* RPL_STATSPING */
	[247] = irc_generic_info,   /* RPL_STATSBLINE */
	[250] = irc_generic_info,   /* RPL_STATSDLINE */
	[251] = irc_generic_info,   /* RPL_LUSERCLIENT */
	[252] = irc_recv_252,       /* RPL_LUSEROP */
	[253] = irc_recv_253,       /* RPL_LUSERUNKNOWN */
	[254] = irc_recv_254,       /* RPL_LUSERCHANNELS */
	[255] = irc_generic_info,   /* RPL_LUSERME */
	[256] = irc_generic_info,   /* RPL_ADMINME */
	[257] = irc_generic_info,   /* RPL_ADMINLOC1 */
	[258] = irc_generic_info,   /* RPL_ADMINLOC2 */
	[259] = irc_generic_info,   /* RPL_ADMINEMAIL */
	[261] = irc_generic_info,   /* RPL_TRACELOG */
	[262] = irc_generic_ignore, /* RPL_TRACEEND */
	[263] = irc_generic_info,   /* RPL_TRYAGAIN */
	[265] = irc_recv_265,       /* RPL_LOCALUSERS */
	[266] = irc_recv_266,       /* RPL_GLOBALUSERS */
	[275] = irc_recv_275,       /* RPL_USINGSSL */
	[276] = irc_recv_276,       /* RPL_WHOISCERTFP */
	[301] = irc_recv_301,       /* RPL_AWAY */
	[302] = irc_generic_info,   /* RPL_USERHOST */
	[303] = irc_generic_info,   /* RPL_ISON */
	[304] = irc_generic_info,   /* RPL_TEXT */
	[305] = irc_generic_info,   /* RPL_UNAWAY */
	[306] = irc_generic_info,   /* RPL_NOWAWAY */
	[307] = irc_recv_307,       /* RPL_WHOISREGNICK */
	[311] = irc_recv_311,       /* RPL_WHOISUSER */
	[312] = irc_recv_312,       /* RPL_WHOISSERVER */
	[313] = irc_recv_313,       /* RPL_WHOISOPERATOR */
	[314] = irc_recv_314,       /* RPL_WHOWASUSER */
	[315] = irc_recv_315,       /* RPL_ENDOFWHO */
	[317] = irc_recv_317,       /* RPL_WHOISIDLE */
	[318] = irc_recv_318,       /* RPL_ENDOFWHOIS */
	[319] = irc_recv_319,       /* RPL_WHOISCHANNELS */
	[320] = irc_recv_320,       /* RPL_WHOISSPECIAL */
	[322] = irc_generic_info,   /* RPL_LIST */
	[323] = irc_generic_ignore, /* RPL_LISTEND */
	[324] = irc_recv_324,       /* RPL_CHANNELMODEIS */
	[325] = irc_generic_info,   /* RPL_UNIQOPIS */
	[328] = irc_recv_328,       /* RPL_CHANNEL_URL */
	[329] = irc_recv_329,       /* RPL_CREATIONTIME */
	[330] = irc_recv_330,       /* RPL_WHOISACCOUNT */
	[331] = irc_generic_ignore, /* RPL_NOTOPIC */
	[332] = irc_recv_332,       /* RPL_TOPIC */
	[333] = irc_recv_333,       /* RPL_TOPICWHOTIME */
	[338] = irc_recv_338,       /* RPL_WHOISACTUALLY */
	[341] = irc_recv_341,       /* RPL_INVITING */
	[346] = irc_generic_info,   /* RPL_INVITELIST */
	[347] = irc_generic_ignore, /* RPL_ENDOFINVITELIST */
	[348] = irc_generic_info,   /* RPL_EXCEPTLIST */
	[349] = irc_generic_ignore, /* RPL_ENDOFEXCEPTLIST */
	[351] = irc_generic_info,   /* RPL_VERSION */
	[352] = irc_recv_352,       /* RPL_WHOREPLY */
	[353] = irc_recv_353,       /* RPL_NAMEREPLY */
	[364] = irc_generic_info,   /* RPL_LINKS */
	[365] = irc_generic_ignore, /* RPL_ENDOFLINKS */
	[366] = irc_recv_366,       /* RPL_ENDOFNAMES */
	[367] = irc_generic_info,   /* RPL_BANLIST */
	[368] = irc_generic_ignore, /* RPL_ENDOFBANLIST */
	[369] = irc_recv_369,       /* RPL_ENDOFWHOWAS */
	[371] = irc_generic_info,   /* RPL_INFO */
	[372] = irc_generic_info,   /* RPL_MOTD */
	[374] = irc_generic_ignore, /* RPL_ENDOFINFO */
	[375] = irc_generic_ignore, /* RPL_MOTDSTART */
	[376] = irc_generic_ignore, /* RPL_ENDOFMOTD */
	[378] = irc_recv_378,       /* RPL_WHOISHOST */
	[379] = irc_recv_379,       /* RPL_WHOISMODES */
	[381] = irc_generic_info,   /* RPL_YOUREOPER */
	[391] = irc_generic_info,   /* RPL_TIME */
	[396] = irc_generic_info,   /* RPL_VISIBLEHOST */
	[401] = irc_recv_401,       /* ERR_NOSUCHNICK */
	[402] = irc_recv_402,       /* ERR_NOSUCHSERVER */
	[403] = irc_recv_403,       /* ERR_NOSUCHCHANNEL */
	[404] = irc_generic_error,  /* ERR_CANNOTSENDTOCHAN */
	[405] = irc_generic_error,  /* ERR_TOOMANYCHANNELS */
	[406] = irc_recv_406,       /* ERR_WASNOSUCHNICK */
	[407] = irc_generic_error,  /* ERR_TOOMANYTARGETS */
	[408] = irc_generic_error,  /* ERR_NOSUCHSERVICE */
	[409] = irc_generic_error,  /* ERR_NOORIGIN */
	[410] = irc_generic_error,  /* ERR_INVALIDCAPCMD */
	[411] = irc_generic_error,  /* ERR_NORECIPIENT */
	[412] = irc_generic_error,  /* ERR_NOTEXTTOSEND */
	[413] = irc_generic_error,  /* ERR_NOTOPLEVEL */
	[414] = irc_generic_error,  /* ERR_WILDTOPLEVEL */
	[415] = irc_generic_error,  /* ERR_BADMASK */
	[416] = irc_generic_error,  /* ERR_TOOMANYMATCHES */
	[421] = irc_generic_error,  /* ERR_UNKNOWNCOMMAND */
	[422] = irc_generic_error,  /* ERR_NOMOTD */
	[423] = irc_generic_error,  /* ERR_NOADMININFO */
	[431] = irc_generic_error,  /* ERR_NONICKNAMEGIVEN */
	[432] = irc_generic_error,  /* ERR_ERRONEUSNICKNAME */
	[433] = irc_recv_433,       /* ERR_NICKNAMEINUSE */
	[436] = irc_generic_error,  /* ERR_NICKCOLLISION */
	[437] = irc_generic_error,  /* ERR_UNAVAILRESOURCE */
	[441] = irc_generic_error,  /* ERR_USERNOTINCHANNEL */
	[442] = irc_generic_error,  /* ERR_NOTONCHANNEL */
	[443] = irc_generic_error,  /* ERR_USERONCHANNEL */
	[451] = irc_generic_error,  /* ERR_NOTREGISTERED */
	[461] = irc_generic_error,  /* ERR_NEEDMOREPARAMS */
	[462] = irc_generic_error,  /* ERR_ALREADYREGISTRED */
	[463] = irc_generic_error,  /* ERR_NOPERMFORHOST */
	[464] = irc_generic_error,  /* ERR_PASSWDMISMATCH */
	[465] = irc_generic_error,  /* ERR_YOUREBANNEDCREEP */
	[466] = irc_generic_error,  /* ERR_YOUWILLBEBANNED */
	[467] = irc_generic_error,  /* ERR_KEYSET */
	[471] = irc_generic_error,  /* ERR_CHANNELISFULL */
	[472] = irc_generic_error,  /* ERR_UNKNOWNMODE */
	[473] = irc_generic_error,  /* ERR_INVITEONLYCHAN */
	[474] = irc_generic_error,  /* ERR_BANNEDFROMCHAN */
	[475] = irc_generic_error,  /* ERR_BADCHANNELKEY */
	[476] = irc_generic_error,  /* ERR_BADCHANMASK */
	[477] = irc_generic_error,  /* ERR_NOCHANMODES */
	[478] = irc_generic_error,  /* ERR_BANLISTFULL */
	[481] = irc_generic_error,  /* ERR_NOPRIVILEGES */
	[482] = irc_generic_error,  /* ERR_CHANOPRIVSNEEDED */
	[483] = irc_generic_error,  /* ERR_CANTKILLSERVER */
	[484] = irc_generic_error,  /* ERR_RESTRICTED */
	[485] = irc_generic_error,  /* ERR_UNIQOPPRIVSNEEDED */
	[491] = irc_generic_error,  /* ERR_NOOPERHOST */
	[501] = irc_generic_error,  /* ERR_UMODEUNKNOWNFLAG */
	[502] = irc_generic_error,  /* ERR_USERSDONTMATCH */
	[524] = irc_generic_error,  /* ERR_HELPNOTFOUND */
	[671] = irc_recv_671,       /* RPL_WHOISSECURE */
	[704] = irc_generic_info,   /* RPL_HELPSTART */
	[705] = irc_generic_info,   /* RPL_HELP */
	[706] = irc_generic_ignore, /* RPL_ENDOFHELP */
	[716] = irc_recv_716,       /* RPL_TARGUMODEG */
	[717] = irc_recv_717,       /* RPL_TARGNOTIFY */
	[718] = irc_recv_718,       /* RPL_UMODEGMSG */
	[900] = ircv3_recv_900,     /* IRCv3 RPL_LOGGEDIN */
	[901] = ircv3_recv_901,     /* IRCv3 RPL_LOGGEDOUT */
	[902] = ircv3_recv_902,     /* IRCv3 ERR_NICKLOCKED */
	[903] = ircv3_recv_903,     /* IRCv3 RPL_SASLSUCCESS */
	[904] = ircv3_recv_904,     /* IRCv3 ERR_SASLFAIL */
	[905] = ircv3_recv_905,     /* IRCv3 ERR_SASLTOOLONG */
	[906] = ircv3_recv_906,     /* IRCv3 ERR_SASLABORTED */
	[907] = ircv3_recv_907,     /* IRCv3 ERR_SASLALREADY */
	[908] = ircv3_recv_908,     /* IRCv3 RPL_SASLMECHS */
	[1000] = NULL               /* Out of range */
};

static unsigned threshold_account = FILTER_THRESHOLD_ACCOUNT;
static unsigned threshold_away    = FILTER_THRESHOLD_AWAY;
static unsigned threshold_chghost = FILTER_THRESHOLD_CHGHOST;
static unsigned threshold_join    = FILTER_THRESHOLD_JOIN;
static unsigned threshold_nick    = FILTER_THRESHOLD_NICK;
static unsigned threshold_part    = FILTER_THRESHOLD_PART;
static unsigned threshold_quit    = FILTER_THRESHOLD_QUIT;

int
irc_recv(struct server *s, struct irc_message *m)
{
	const struct recv_handler* handler;

	if (isdigit(*m->command))
		return irc_recv_numeric(s, m);

	if ((handler = recv_handler_lookup(m->command, m->len_command)))
		return handler->f(s, m);

	return irc_generic_unknown(s, m);
}

static int
irc_generic(struct server *s, struct irc_message *m, const char *command, const char *from)
{
	/* Generic handling of messages, in the form:
	 *
	 *   [command] [params] trailing
	 */

	const char *params       = NULL;
	const char *params_sep   = NULL;
	const char *trailing     = NULL;
	const char *trailing_sep = NULL;

	irc_message_split(m, &params, &trailing);

	if (!command && (!params || !*params) && (!trailing || !*trailing))
		return 1;

	if (command && params)
		params_sep = " ";

	if ((command || params) && trailing)
		trailing_sep = " ";

	newlinef(s->channel, 0, from,
		"%s%s%s" "%s%s%s%s" "%s%s",
		(!command      ? "" : "["),
		(!command      ? "" : command),
		(!command      ? "" : "]"),
		(!params_sep   ? "" : params_sep),
		(!params       ? "" : "["),
		(!params       ? "" : params),
		(!params       ? "" : "]"),
		(!trailing_sep ? "" : trailing_sep),
		(!trailing     ? "" : trailing)
	);

	return 0;
}

static int
irc_generic_error(struct server *s, struct irc_message *m)
{
	/* Generic error message handling */

	return irc_generic(s, m, NULL, FROM_ERROR);
}

static int
irc_generic_ignore(struct server *s, struct irc_message *m)
{
	/* Generic handling for ignored message types */

	UNUSED(s);
	UNUSED(m);

	return 0;
}

static int
irc_generic_info(struct server *s, struct irc_message *m)
{
	/* Generic info message handling */

	return irc_generic(s, m, NULL, FROM_INFO);
}

static int
irc_generic_unknown(struct server *s, struct irc_message *m)
{
	/* Generic handling of unknown commands */

	return irc_generic(s, m, m->command, FROM_UNKNOWN);
}

static int
irc_recv_001(struct server *s, struct irc_message *m)
{
	/* RPL_WELCOME
	 *
	 * :Welcome to the <networkname> Network <nick>[!<user>@<host>] */

	const char *params;
	const char *trailing;
	struct channel *c = s->channel;

	s->registered = 1;

	if (irc_message_split(m, &params, &trailing))
		server_info(s, "%s", trailing);

	server_info(s, "You are known as %s", s->nick);

	if (s->mode)
		sendf(s, "MODE %s +%s", s->nick, s->mode);

	do {
		if (c->type == CHANNEL_T_CHANNEL && !c->parted)
			sendf(s, "JOIN %s%s%s",
				c->name,
				(!c->key ? "" : " "),
				(!c->key ? "" : c->key));
	} while ((c = c->next) != s->channel);

	return 0;
}

static int
irc_recv_004(struct server *s, struct irc_message *m)
{
	/* RPL_MYINFO
	 *
	 * <servername> <version> <usermodes> <chanmodes> [chanmodes with parameter] */

	const char *params;
	const char *trailing;

	if (irc_message_split(m, &params, &trailing))
		server_info(s, "%s", params);

	server_set_004(s, m->params);

	return 0;
}

static int
irc_recv_005(struct server *s, struct irc_message *m)
{
	/* RPL_ISUPPORT
	 *
	 * 1*<params> [:message] */

	const char *params;
	const char *trailing;

	if (irc_message_split(m, &params, &trailing))
		server_info(s, "%s", params);

	server_set_005(s, m->params);

	return 0;
}

static int
irc_recv_042(struct server *s, struct irc_message *m)
{
	/* RPL_YOURID
	 *
	 * <id> :your unique ID */

	char *param;

	if (!irc_message_param(m, &param))
		failf(s, "RPL_YOURID: param is null");

	server_info(s, "Your unique ID is: [%s]", param);

	return 0;
}

static int
irc_recv_221(struct server *s, struct irc_message *m)
{
	/* RPL_UMODEIS
	 *
	 * <usermodes> */

	return recv_mode_usermodes(m, &(s->mode_cfg), s);
}

static int
irc_recv_252(struct server *s, struct irc_message *m)
{
	/* RPL_LUSEROP
	 *
	 * <int> :operator(s) online */

	char *param;

	if (!irc_message_param(m, &param))
		failf(s, "RPL_LUSEROP: param is null");

	server_info(s, "%s operator(s) online", param);

	return 0;
}

static int
irc_recv_253(struct server *s, struct irc_message *m)
{
	/* RPL_LUSERUNKNOWN
	 *
	 * <int> :unknown connection(s) */

	char *param;

	if (!irc_message_param(m, &param))
		failf(s, "RPL_LUSERUNKNOWN: param is null");

	server_info(s, "%s unknown connection(s)", param);

	return 0;
}

static int
irc_recv_254(struct server *s, struct irc_message *m)
{
	/* RPL_LUSERCHANNELS
	 *
	 * <int> :channel(s) formed */

	char *param;

	if (!irc_message_param(m, &param))
		failf(s, "RPL_LUSERCHANNELS: param is null");

	server_info(s, "%s channel(s) formed", param);

	return 0;
}

static int
irc_recv_265(struct server *s, struct irc_message *m)
{
	/* RPL_LOCALUSERS
	 *
	 * [<int> <int>] :Current local users <int>, max <int> */

	const char *params;
	const char *trailing;

	irc_message_split(m, &params, &trailing);

	if (!trailing || !*trailing)
		failf(s, "RPL_LOCALUSERS: trailing is null");

	server_info(s, "%s", trailing);

	return 0;
}

static int
irc_recv_266(struct server *s, struct irc_message *m)
{
	/* RPL_GLOBALUSERS
	 *
	 * [<int> <int>] :Current global users <int>, max <int> */

	const char *params;
	const char *trailing;

	irc_message_split(m, &params, &trailing);

	if (!trailing || !*trailing)
		failf(s, "RPL_GLOBALUSERS: trailing is null");

	server_info(s, "%s", trailing);

	return 0;
}

static int
irc_recv_275(struct server *s, struct irc_message *m)
{
	/* RPL_USINGSSL
	 *
	 * <nick> :is using a secure connection (SSL) */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_USINGSSL: nick is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_USINGSSL: message is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s %s", nick, message);

	return 0;
}

static int
irc_recv_276(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISCERTFP
	 *
	 * <nick> :has client certificate fingerprint <fingerprint> */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISCERTFP: nick is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_WHOISCERTFP: message is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s %s", nick, message);

	return 0;
}

static int
irc_recv_301(struct server *s, struct irc_message *m)
{
	/* RPL_AWAY
	 *
	 * <nick> :<message> */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_AWAY: nick is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_AWAY: message is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s is away (%s)", nick, message);

	return 0;
}

static int
irc_recv_307(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISREGNICK
	 *
	 * <nick> :has identified for this nick */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISREGNICK: nick is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_WHOISREGNICK: message is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s %s", nick, message);

	return 0;
}

static int
irc_recv_311(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISUSER
	 *
	 * <nick> <username> <host> * :<realname> */

	char *nick;
	char *username;
	char *host;
	char *unused;
	char *realname;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISUSER: nick is null");

	if (!irc_message_param(m, &username))
		failf(s, "RPL_WHOISUSER: username is null");

	if (!irc_message_param(m, &host))
		failf(s, "RPL_WHOISUSER: host is null");

	if (!irc_message_param(m, &unused))
		failf(s, "RPL_WHOISUSER: unused is null");

	if (!irc_message_param(m, &realname))
		failf(s, "RPL_WHOISUSER: realname is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s is %s@%s (%s)", nick, username, host, realname);

	return 0;
}

static int
irc_recv_312(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISSERVER
	 *
	 * <nick> <server> :<server info> */

	char *nick;
	char *servername;
	char *serverinfo;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISSERVER: nick is null");

	if (!irc_message_param(m, &servername))
		failf(s, "RPL_WHOISSERVER: servername is null");

	if (!irc_message_param(m, &serverinfo))
		failf(s, "RPL_WHOISSERVER: serverinfo is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s is connected to: %s (%s)", nick, servername, serverinfo);

	return 0;
}

static int
irc_recv_313(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISOPERATOR
	 *
	 * <nick> :is an IRC operator */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISOPERATOR: nick is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_WHOISOPERATOR: message is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s %s", nick, message);

	return 0;
}

static int
irc_recv_314(struct server *s, struct irc_message *m)
{
	/* RPL_WHOWASUSER
	 *
	 * <nick> <username> <host> * :<realname> */

	char *nick;
	char *username;
	char *host;
	char *unused;
	char *realname;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOWASUSER: nick is null");

	if (!irc_message_param(m, &username))
		failf(s, "RPL_WHOWASUSER: username is null");

	if (!irc_message_param(m, &host))
		failf(s, "RPL_WHOWASUSER: host is null");

	if (!irc_message_param(m, &unused))
		failf(s, "RPL_WHOWASUSER: unused is null");

	if (!irc_message_param(m, &realname))
		failf(s, "RPL_WHOWASUSER: realname is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s was %s@%s (%s)", nick, username, host, realname);

	return 0;
}

static int
irc_recv_315(struct server *s, struct irc_message *m)
{
	/* RPL_ENDOFWHO
	 *
	 * <mask> :End of /WHO list */

	char *nick;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_ENDOFWHO: nick is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "/who %s END", nick);

	return 0;
}

static int
irc_recv_317(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISIDLE
	 *
	 * <nick> <secs> <signon> :seconds idle, signon time */

	char *idle;
	char *nick;
	char *signon;
	char buf[sizeof("1970-01-01T00:00:00")];
	struct tm tm;
	time_t t_idle = 0;
	time_t t_signon = 0;
	unsigned idle_d;
	unsigned idle_h;
	unsigned idle_m;
	unsigned idle_s;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISIDLE: nick is null");

	if (!irc_message_param(m, &idle))
		failf(s, "RPL_WHOISIDLE: idle is null");

	if (!irc_message_param(m, &signon))
		failf(s, "RPL_WHOISIDLE: signon is null");

	errno = 0;
	t_idle = strtoul(idle, NULL, 0);

	if (errno)
		failf(s, "RPL_WHOISIDLE: t_idle strtoul error: %s", strerror(errno));

	errno = 0;
	t_signon = strtoul(signon, NULL, 0);

	if (errno)
		failf(s, "RPL_WHOISIDLE: t_signon strtoul error: %s", strerror(errno));

	if (gmtime_r(&t_signon, &tm) == NULL)
		failf(s, "RPL_WHOISIDLE: gmtime_r error: %s", strerror(errno));

	if ((strftime(buf, sizeof(buf), "%FT%T", &tm)) == 0)
		failf(s, "RPL_WHOISIDLE: strftime error");

	idle_d = t_idle / 86400;
	idle_h = t_idle % 86400 / 3600;
	idle_m = t_idle % 3600 / 60;
	idle_s = t_idle % 60;

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s has been idle %.0u%s%.0u%s%.0u%s%u seconds, connected since %s",
		nick,
		idle_d,
		idle_d ? " days " : "",
		idle_h,
		idle_h ? " hours " : "",
		idle_m,
		idle_m ? " minutes " : "",
		idle_s,
		buf);

	return 0;
}

static int
irc_recv_318(struct server *s, struct irc_message *m)
{
	/* RPL_ENDOFWHOIS
	 *
	 * <nick> :End of /WHOIS list */

	char *nick;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_ENDOFWHOIS: nick is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "/whois %s END", nick);

	return 0;
}

static int
irc_recv_319(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISCHANNELS
	 *
	 * <nick> :1*[prefix]<channel> */

	char *nick;
	char *channels;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISCHANNELS: nick is null");

	if (!irc_message_param(m, &channels))
		failf(s, "RPL_WHOISCHANNELS: message is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s is on channels: %s", nick, channels);

	return 0;
}

static int
irc_recv_320(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISSPECIAL
	 *
	 * <nick> :message */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISSPECIAL: nick is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_WHOISSPECIAL: message is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s %s", nick, message);

	return 0;
}

static int
irc_recv_324(struct server *s, struct irc_message *m)
{
	/* RPL_CHANNELMODEIS
	 *
	 * <channel> 1*[<modestring> [<mode arguments>]] */

	char *chan;
	struct channel *c;

	if (!irc_message_param(m, &chan))
		failf(s, "RPL_CHANNELMODEIS: channel is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "RPL_CHANNELMODEIS: channel '%s' not found", chan);

	channel_key_del(c);

	return recv_mode_chanmodes(m, &(s->mode_cfg), s, c);
}

static int
irc_recv_328(struct server *s, struct irc_message *m)
{
	/* RPL_CHANNEL_URL
	 *
	 * <channel> :<url> */

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
irc_recv_329(struct server *s, struct irc_message *m)
{
	/* RPL_CREATIONTIME
	 *
	 * <channel> <creationtime> */

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
irc_recv_330(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISACCOUNT
	 *
	 * <nick> <account> :is logged in as */

	char *nick;
	char *account;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISACCOUNT: nick is null");

	if (!irc_message_param(m, &account))
		failf(s, "RPL_WHOISACCOUNT: account is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s is logged in as %s", nick, account);

	return 0;
}

static int
irc_recv_332(struct server *s, struct irc_message *m)
{
	/* RPL_TOPIC
	 *
	 * <channel> :<topic> */

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
irc_recv_333(struct server *s, struct irc_message *m)
{
	/* RPL_TOPICWHOTIME
	 *
	 * <channel> <nick> <time> */

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
irc_recv_338(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISACTUALLY
	 *
	 * <nick> :Is actually
	 * <nick> <host|ip> :Is actually using host
	 * <nick> <username>@<hostname> <ip> :Is actually using host */

	char *nick;
	const char *params;
	const char *trailing;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISACTUALLY: nick is null");

	irc_message_split(m, &params, &trailing);

	if (!trailing)
		failf(s, "RPL_WHOISACTUALLY: trailing is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	if (params && trailing)
		newlinef(c, 0, FROM_INFO, "%s %s %s", nick, trailing, params);
	else
		newlinef(c, 0, FROM_INFO, "%s %s", nick, trailing);

	return 0;
}

static int
irc_recv_341(struct server *s, struct irc_message *m)
{
	/* RPL_INVITING
	 *
	 * <nick> <channel> */

	char *nick;
	char *chan;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_INVITING: nick is null");

	if (!irc_message_param(m, &chan))
		failf(s, "RPL_INVITING: channel is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "RPL_INVITING: channel '%s' not found", chan);

	newlinef(c, 0, FROM_INFO, "Invited %s to %s", nick, chan);

	return 0;
}

#if 0
08:26 -!-          * rirc-test H   0  ~u@7ts7bn7azdaug.oragono [rirc-test]
08:26 -!- End of /WHO list
08:27 -!- rirc-test- [~u@q23tfk77z59jq.oragono]
08:27 -!-  was      : rirc-test
08:27 -!- rirc-test- [~u@q23tfk77z59jq.oragono]
08:27 -!-  was      : rirc-test
08:27 -!- rirc-test- [~u@q23tfk77z59jq.oragono]
08:27 -!-  was      : rirc-test
08:27 -!- rirc-test- [~u@q23tfk77z59jq.oragono]
08:27 -!-  was      : rirc-test
08:27 -!- rirc-test- [~u@q23tfk77z59jq.oragono]
08:27 -!-  was      : rirc-test
08:27 -!- rirc-test- [~u@q23tfk77z59jq.oragono]
08:27 -!-  was      : rirc-test
08:27 -!- End of WHOWAS
08:27 -!-          * rirc-test H   0  ~u@7ts7bn7azdaug.oragono [rirc-test]
08:27 -!- End of /WHO list
08:27 -!- rirc-test [~u@7ts7bn7azdaug.oragono]
08:27 -!-  ircname  : rirc-test
08:27 -!-           : is using a secure connection
08:27 -!-  idle     : 0 days 0 hours 41 mins 53 secs [signon: Sat Apr 15 07:45:49 2023]
08:27 -!- End of WHOIS
08:28 -!- rcr [~u@7ts7bn7azdaug.oragono]
08:28 -!-  ircname  : Unknown
08:28 -!-  hostname : ~u@208.98.219.26 208.98.219.26
08:28 -!-  modes    : +i
08:28 -!-  idle     : 0 days 0 hours 2 mins 39 secs [signon: Sat Apr 15 08:25:38 2023]
08:28 -!- End of WHOIS
#endif

static int
irc_recv_352(struct server *s, struct irc_message *m)
{
	/* RPL_WHOREPLY
	 *
	 * <channel> <username> <host> <server> <nick> <flags> :<hopcount> <realname> */


	// THIS IS A WIP needs to be completed before testing /who command



	// TODO, this command might have a bunch of output, it can be returned from a mask
	// to lookup users, channels
	//
	//
	// compare the output of the same query on irssi vs rirc, see what information is potentially
	// worth disaplying, ircv3 specs say hopcount is unreliable
	//
	//
	// > <channel> is an arbitrary channel the client is joined to or a literal
	// asterisk character ('*', 0x2A) if no channel is returned.

	// irssi looks like:
	//
	// 08:27 -!-          * rirc-test H   0  ~u@7ts7bn7azdaug.oragono [rirc-test]
	// 08:27 -!- End of /WHO list


	// XXX: this one should always be printed in the network buffer, all other whois/whowas numerics
	// should be buffer targeted

	/* something like:


    /who <mask>
	  .. <result>
	  .. <result>
	  .. <result>
	/who END


	 where result is...

	  .. nick: <info> [username realname]
	 */

	(void)s;
	(void)m;
	return 0;
}

static int
irc_recv_353(struct server *s, struct irc_message *m)
{
	/* RPL_NAMREPLY
	 *
	 * <type> <channel> :1*([prefix]<nick>) */

	char *chan;
	char *nicks;
	char *prefix;
	struct channel *c;

	if (!irc_message_param(m, &prefix))
		failf(s, "RPL_NAMEREPLY: type is null");

	if (!irc_message_param(m, &chan))
		failf(s, "RPL_NAMEREPLY: channel is null");

	if (!irc_message_param(m, &nicks))
		failf(s, "RPL_NAMEREPLY: nicks is null");

	if (!(c = channel_list_get(&s->clist, chan, s->casemapping)))
		c = s->channel;

	if (c->type == CHANNEL_T_CHANNEL && !c->_366) {

		char *nick;

		if (*prefix != '@' && *prefix != '*' && *prefix != '=') {
			server_error(s, "RPL_NAMEREPLY: invalid channel type: '%c'", *prefix);
			return 1;
		} else {

			if (*prefix == '@')
				(void) mode_chanmode_set(&(c->chanmodes), &(s->mode_cfg), 's', 1);

			if (*prefix == '*')
				(void) mode_chanmode_set(&(c->chanmodes), &(s->mode_cfg), 'p', 1);

			c->chanmodes.prefix = *prefix;
		}

		while ((prefix = nick = irc_strsep(&nicks))) {

			struct mode prfxmode = {0};

			while (*nick && strchr(s->mode_cfg.PREFIX.T, *nick))
				(void) mode_prfxmode_set(&prfxmode, &(s->mode_cfg), *nick++, 1);

			if (*nick == 0) {
				server_error(s, "RPL_NAMEREPLY: invalid nick: '%s'", prefix);
				continue;
			}

			if (user_list_add(&(c->users), s->casemapping, nick, prfxmode) == USER_ERR_DUPLICATE) {
				server_error(s, "RPL_NAMEREPLY: duplicate nick: '%s'", nick);
				continue;
			}
		}

		if (c == current_channel())
			draw(DRAW_STATUS);

	} else {
		newlinef(c, 0, FROM_INFO, "%s: %s", chan, nicks);
	}

	return 0;
}

static int
irc_recv_366(struct server *s, struct irc_message *m)
{
	/* RPL_ENDOFNAMES
	 *
	 * <channel> :End of NAMES list */

	char *chan;
	struct channel *c;

	if (!irc_message_param(m, &chan))
		failf(s, "RPL_NAMEREPLY: channel is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)))
		c->_366 = 1;

	return 0;
}

static int
irc_recv_369(struct server *s, struct irc_message *m)
{
	/* RPL_ENDOFWHOWAS
	 *
	 * <nick> :End of /WHOWAS list */

	char *nick;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_ENDOFWHOWAS: nick is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "/whowas %s END", nick);

	return 0;
}

static int
irc_recv_378(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISHOST
	 *
	 * <nick> :is connecting from ... */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISHOST: nick is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_WHOISHOST: message is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s %s", nick, message);

	return 0;
}

static int
irc_recv_379(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISMODES
	 *
	 * <nick> :is using modes +ailosw */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISMODES: nick is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_WHOISMODES: message is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s %s", nick, message);

	return 0;
}

static int
irc_recv_401(struct server *s, struct irc_message *m)
{
	/* ERR_NOSUCHNICK
	 *
	 * <nick> :No such nick/channel */

	char *message;
	char *nick;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "ERR_NOSUCHNICK: nick is null");

	if (!(c = channel_list_get(&(s->clist), nick, s->casemapping)))
		c = s->channel;

	irc_message_param(m, &message);

	if (message && *message)
		newlinef(c, 0, FROM_ERROR, "[%s] %s", nick, message);
	else
		newlinef(c, 0, FROM_ERROR, "[%s] No such nick/channel", nick);

	return 0;
}

static int
irc_recv_402(struct server *s, struct irc_message *m)
{
	/* ERR_NOSUCHSERVER
	 *
	 * <server> :No such server */

	char *server;
	char *message;

	if (!irc_message_param(m, &server))
		failf(s, "ERR_NOSUCHSERVER: server is null");

	irc_message_param(m, &message);

	if (message && *message)
		server_error(s, "[%s] %s", server, message);
	else
		server_error(s, "[%s] No such server", server);

	return 0;
}

static int
irc_recv_403(struct server *s, struct irc_message *m)
{
	/* ERR_NOSUCHCHANNEL
	 *
	 * <channel> :No such channel */

	char *chan;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &chan))
		failf(s, "ERR_NOSUCHCHANNEL: channel is null");

	if (!(c = channel_list_get(&(s->clist), chan, s->casemapping)))
		c = s->channel;

	irc_message_param(m, &message);

	if (message && *message)
		newlinef(c, 0, FROM_ERROR, "[%s] %s", chan, message);
	else
		newlinef(c, 0, FROM_ERROR, "[%s] No such channel", chan);

	return 0;
}

static int
irc_recv_406(struct server *s, struct irc_message *m)
{
	/* ERR_WASNOSUCHNICK
	 *
	 * <nick> :There was no such nickname */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "ERR_WASNOSUCHNICK: nick is null");

	if (!(c = channel_list_get(&(s->clist), nick, s->casemapping)))
		c = s->channel;

	irc_message_param(m, &message);

	if (message && *message)
		newlinef(c, 0, FROM_ERROR, "[%s] %s", nick, message);
	else
		newlinef(c, 0, FROM_ERROR, "[%s] No such nick", nick);

	return 0;
}

static int
irc_recv_433(struct server *s, struct irc_message *m)
{
	/* ERR_NICKNAMEINUSE
	 *
	 * <nick> :Nickname is already in use */

	char *nick;

	if (!irc_message_param(m, &nick))
		failf(s, "ERR_NICKNAMEINUSE: nick is null");

	server_error(s, "Nick '%s' in use", nick);

	if (!strcmp(nick, s->nick)) {
		server_nicks_next(s);
		server_error(s, "Trying again with '%s'", s->nick);
		sendf(s, "NICK %s", s->nick);
	}

	return 0;
}

static int
irc_recv_671(struct server *s, struct irc_message *m)
{
	/* RPL_WHOISSECURE
	 *
	 * <nick> :is using a secure connection */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_WHOISSECURE: nick is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_WHOISSECURE: message is null");

	if (!(c = channel_list_get(&s->clist, nick, s->casemapping)))
		c = s->channel;

	newlinef(c, 0, FROM_INFO, "%s %s", nick, message);

	return 0;
}

static int
irc_recv_716(struct server *s, struct irc_message *m)
{
	/* RPL_TARGUMODEG
	 *
	 * <nick> :<info> */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_TARGUMODEG: nick is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_TARGUMODEG: message is null");

	if (!(c = channel_list_get(&(s->clist), nick, s->casemapping)))
		c = s->channel;

	if (message && *message)
		newlinef(c, 0, FROM_INFO, "%s %s", nick, message);
	else
		newlinef(c, 0, FROM_INFO, "%s has +g mode enabled (server-side ignore)", nick);

	return 0;
}

static int
irc_recv_717(struct server *s, struct irc_message *m)
{
	/* RPL_TARGNOTIFY
	 *
	 * <nick> :<info> */

	char *nick;
	char *message;
	struct channel *c;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_TARGNOTIFY: nick is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_TARGNOTIFY: message is null");

	if (!(c = channel_list_get(&(s->clist), nick, s->casemapping)))
		c = s->channel;

	if (message && *message)
		newlinef(c, 0, FROM_INFO, "%s %s", nick, message);
	else
		newlinef(c, 0, FROM_INFO, "%s has been informed that you messaged them", nick);

	return 0;
}

static int
irc_recv_718(struct server *s, struct irc_message *m)
{
	/* RPL_UMODEGMSG
	 *
	 * <nick> <user>@<host> :<info> */

	char *nick;
	char *host;
	char *message;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_UMODEGMSG: nick is null");

	if (!irc_message_param(m, &host))
		failf(s, "RPL_UMODEGMSG: host is null");

	if (!irc_message_param(m, &message))
		failf(s, "RPL_UMODEGMSG: message is null");

	server_info(s, "%s (%s) %s", nick, host, message);

	return 0;
}

static int
irc_recv_numeric(struct server *s, struct irc_message *m)
{
	/* :server <code> <target> [args] */

	char *targ;
	unsigned code = 0;

	if ((m->command[0] && isdigit(m->command[0]))
	 && (m->command[1] && isdigit(m->command[1]))
	 && (m->command[2] && isdigit(m->command[2]))
	 && (m->command[3] == 0))
	{
		code += (m->command[0] - '0') * 100;
		code += (m->command[1] - '0') * 10;
		code += (m->command[2] - '0');
	}

	if (!code)
		failf(s, "NUMERIC: '%s' invalid", m->command);

	if (!(irc_message_param(m, &targ)))
		failf(s, "NUMERIC: target is null");

	if (strcmp(targ, s->nick) && strcmp(targ, "*"))
		failf(s, "NUMERIC: target '%s' is invalid", targ);

	if (!irc_numerics[code])
		return irc_generic_unknown(s, m);

	return (*irc_numerics[code])(s, m);
}

static int
recv_error(struct server *s, struct irc_message *m)
{
	/* ERROR :<message> */

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
	struct channel *c;

	if (!m->from)
		failf(s, "INVITE: sender's nick is null");

	if (!irc_message_param(m, &nick))
		failf(s, "INVITE: nick is null");

	if (!irc_message_param(m, &chan))
		failf(s, "INVITE: channel is null");

	if (!strcmp(nick, s->nick)) {
		server_info(s, "%s invited you to %s", m->from, chan);
		return 0;
	}

	/* IRCv3 CAP invite-notify, sent to all users on the target channel.
	 *
	 * Server is not required to send this to all users on the channel,
	 * e.g. it may choose to send this message only to channel OPs or
	 * users only to users with /invite privileges */
	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "INVITE: channel '%s' not found", chan);

	newlinef(c, 0, FROM_INFO, "%s invited %s to %s", m->from, nick, chan);

	return 0;
}

static int
recv_join(struct server *s, struct irc_message *m)
{
	/* :nick!user@host JOIN <channel>
	 * :nick!user@host JOIN <channel> <account> :<realname> */

	char *chan;
	struct channel *c;

	if (!m->from)
		failf(s, "JOIN: sender's nick is null");

	if (!irc_message_param(m, &chan))
		failf(s, "JOIN: channel is null");

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
		draw(DRAW_ALL);
		return 0;
	}

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "JOIN: channel '%s' not found", chan);

	/* JOIN increments count, filter first */

	int filter = irc_recv_threshold_filter(threshold_join, c->users.count);

	if (user_list_add(&(c->users), s->casemapping, m->from, (struct mode){0}) == USER_ERR_DUPLICATE)
		failf(s, "JOIN: user '%s' already on channel '%s'", m->from, chan);

	if (c == current_channel())
		draw(DRAW_STATUS);

	if (!filter) {

		if (s->ircv3_caps.extended_join.set) {

			char *account;
			char *realname;

			if (!irc_message_param(m, &account))
				failf(s, "JOIN: account is null");

			if (!irc_message_param(m, &realname))
				failf(s, "JOIN: realname is null");

			newlinef(c, BUFFER_LINE_JOIN, FROM_JOIN, "%s!%s has joined [%s - %s]",
				m->from, m->host, account, realname);
		} else {
			newlinef(c, BUFFER_LINE_JOIN, FROM_JOIN, "%s!%s has joined",
				m->from, m->host);
		}
	}

	return 0;
}

static int
recv_kick(struct server *s, struct irc_message *m)
{
	/* :nick!user@host KICK <channel> <user> [:message] */

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

		if (message && *message)
			newlinef(c, 0, FROM_INFO, "Kicked by %s (%s)", m->from, message);
		else
			newlinef(c, 0, FROM_INFO, "Kicked by %s", m->from);

	} else {

		if (user_list_del(&(c->users), s->casemapping, user) == USER_ERR_NOT_FOUND)
			failf(s, "KICK: nick '%s' not found in '%s'", user, chan);

		if (message && *message)
			newlinef(c, 0, FROM_INFO, "%s has kicked %s (%s)", m->from, user, message);
		else
			newlinef(c, 0, FROM_INFO, "%s has kicked %s", m->from, user);
	}

	draw(DRAW_STATUS);

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
		failf(s, "MODE: target nick is null");

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
	struct mode *chanmodes = &(c->chanmodes);
	struct user *user;

	if (!irc_message_param(m, &modestring)) {
		newlinef(c, 0, FROM_ERROR, "MODE: modestring is null");
		return 1;
	}

	do {
		int set = -1;

		while ((flag = *modestring++)) {

			if (flag == '-') {
				set = 0;
				continue;
			}

			if (flag == '+') {
				set = 1;
				continue;
			}

			if (set == -1) {
				newlinef(c, 0, FROM_ERROR, "MODE: missing '+'/'-'");
				continue;
			}

			modearg = NULL;

			switch (mode_type(cfg, flag, set)) {

				/* Doesn't consume an argument */
				case MODE_FLAG_CHANMODE:

					if (mode_chanmode_set(chanmodes, cfg, flag, set)) {
						server_error(s, "MODE: invalid flag '%c'", flag);
					} else {
						newlinef(c, 0, FROM_INFO, "%s%s%s mode: %c%c",
								(m->from ? m->from : ""),
								(m->from ? " set " : ""),
								c->name,
								(set ? '+' : '-'),
								flag);
					}
					break;

				/* Consumes an argument */
				case MODE_FLAG_CHANMODE_PARAM:

					if (!irc_message_param(m, &modearg)) {
						newlinef(c, 0, FROM_ERROR, "MODE: flag '%c' expected argument", flag);
						continue;
					}

					if (flag == 'k') {
						if (set)
							channel_key_add(c, modearg);
						else
							channel_key_del(c);
					}

					if (mode_chanmode_set(chanmodes, cfg, flag, set)) {
						server_error(s, "MODE: invalid flag '%c'", flag);
					} else {
						newlinef(c, 0, FROM_INFO, "%s%s%s mode: %c%c %s",
								(m->from ? m->from : ""),
								(m->from ? " set " : ""),
								c->name,
								(set ? '+' : '-'),
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

					if (mode_prfxmode_set(&(user->prfxmodes), cfg, flag, set)) {
						server_error(s, "MODE: invalid flag '%c'", flag);
					} else {
						newlinef(c, 0, FROM_INFO, "%s%suser %s mode: %c%c",
								(m->from ? m->from : ""),
								(m->from ? " set " : ""),
								modearg,
								(set ? '+' : '-'),
								flag);
					}
					break;

				case MODE_FLAG_INVALID_FLAG:
					newlinef(c, 0, FROM_ERROR, "MODE: invalid flag '%c'", flag);
					break;

				default:
					newlinef(c, 0, FROM_ERROR, "MODE: unhandled error, flag '%c'", flag);
					continue;
			}
		}
	} while (irc_message_param(m, &modestring));

	mode_str(&(c->chanmodes), &(c->chanmodes_str));
	draw(DRAW_STATUS);

	return 0;
}

static int
recv_mode_usermodes(struct irc_message *m, const struct mode_cfg *cfg, struct server *s)
{
	char *modestring;

	if (!irc_message_param(m, &modestring))
		failf(s, "MODE: modestring is null");

	do {
		char flag;
		int set = -1;

		while ((flag = *modestring++)) {

			if (flag == '-') {
				set = 0;
				continue;
			}

			if (flag == '+') {
				set = 1;
				continue;
			}

			if (set == -1) {
				server_error(s, "MODE: missing '+'/'-'");
				continue;
			}

			if (mode_usermode_set(&(s->usermodes), cfg, flag, set)) {
				server_error(s, "MODE: invalid flag '%c'", flag);
			} else {
				server_info(s, "%s%smode: %c%c",
						(m->from ? m->from : ""),
						(m->from ? " set " : ""),
						(set ? '+' : '-'),
						flag);
			}
		}
	} while (irc_message_param(m, &modestring));

	mode_str(&(s->usermodes), &(s->mode_str));
	draw(DRAW_STATUS);

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
		newlinef(s->channel, BUFFER_LINE_NICK, FROM_INFO, "Your nick is now '%s'", nick);
		draw(DRAW_STATUS);
	}

	do {
		enum user_err ret;

		if ((ret = user_list_rpl(&(c->users), s->casemapping, m->from, nick)) == USER_ERR_NOT_FOUND)
			continue;

		if (ret == USER_ERR_DUPLICATE)
			server_error(s, "NICK: user '%s' already on channel '%s'", nick, c->name);

		if (irc_recv_threshold_filter(threshold_nick, c->users.count))
			continue;

		newlinef(c, BUFFER_LINE_NICK, FROM_INFO, "%s  >>  %s", m->from, nick);

	} while ((c = c->next) != s->channel);

	return 0;
}

static int
recv_notice(struct server *s, struct irc_message *m)
{
	/* :nick!user@host NOTICE <target> :<message> */

	char *message;
	char *target;
	struct channel *c;

	if (!m->from)
		failf(s, "NOTICE: sender's nick is null");

	if (!irc_message_param(m, &target))
		failf(s, "NOTICE: target is null");

	if (!irc_message_param(m, &message))
		failf(s, "NOTICE: message is null");

	if (IS_CTCP(message))
		return ctcp_response(s, m->from, target, message);

	if (!(c = channel_list_get(&(s->clist), m->from, s->casemapping)))
		c = s->channel;

	newlinef(c, BUFFER_LINE_CHAT, m->from, "%s", message);

	return 0;
}

static int
recv_part(struct server *s, struct irc_message *m)
{
	/* :nick!user@host PART <channel> [:message] */

	char *chan;
	char *message;
	struct channel *c;

	if (!m->from)
		failf(s, "PART: sender's nick is null");

	if (!irc_message_param(m, &chan))
		failf(s, "PART: channel is null");

	irc_message_param(m, &message);

	if (!strcmp(m->from, s->nick)) {

		/* If not found, assume channel was closed */
		if ((c = channel_list_get(&s->clist, chan, s->casemapping)) != NULL) {

			if (message && *message)
				newlinef(c, BUFFER_LINE_PART, FROM_PART, "you have parted (%s)", message);
			else
				newlinef(c, BUFFER_LINE_PART, FROM_PART, "you have parted");

			channel_part(c);
		}
	} else {
		if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
			failf(s, "PART: channel '%s' not found", chan);

		/* PART decrements count, filter first */

		int filter = irc_recv_threshold_filter(threshold_part, c->users.count);

		if (user_list_del(&(c->users), s->casemapping, m->from) == USER_ERR_NOT_FOUND)
			failf(s, "PART: nick '%s' not found in '%s'", m->from, chan);

		if (!filter) {

			if (message && *message)
				newlinef(c, 0, FROM_PART, "%s!%s has parted (%s)", m->from, m->host, message);
			else
				newlinef(c, 0, FROM_PART, "%s!%s has parted", m->from, m->host);
		}
	}

	draw(DRAW_STATUS);

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
	/* PONG <server> [<server2>] */

	UNUSED(s);
	UNUSED(m);

	return 0;
}

static int
recv_privmsg(struct server *s, struct irc_message *m)
{
	/* :nick!user@host PRIVMSG <target> :<message> */

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

	if (IS_CTCP(message))
		return ctcp_request(s, m->from, target, message);

	if (!strcmp(target, s->nick)) {

		if ((c = channel_list_get(&s->clist, m->from, s->casemapping)) == NULL) {
			c = channel(m->from, CHANNEL_T_PRIVMSG);
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

		newlinef(c, BUFFER_LINE_PINGED, m->from, "%s", message);
	} else {
		newlinef(c, BUFFER_LINE_CHAT, m->from, "%s", message);
	}

	if (urgent) {
		c->activity = ACTIVITY_PINGED;
		draw(DRAW_BELL);
		draw(DRAW_NAV);
	}

	return 0;
}

static int
recv_quit(struct server *s, struct irc_message *m)
{
	/* :nick!user@host QUIT [:message] */

	char *message;
	struct channel *c = s->channel;

	if (!m->from)
		failf(s, "QUIT: sender's nick is null");

	irc_message_param(m, &message);

	do {
		/* QUIT decrements count, filter first */

		int filter = irc_recv_threshold_filter(threshold_quit, c->users.count);

		if (user_list_del(&(c->users), s->casemapping, m->from) == USER_ERR_NOT_FOUND)
			continue;

		if (filter)
			continue;

		if (message && *message)
			newlinef(c, BUFFER_LINE_QUIT, FROM_QUIT, "%s!%s has quit (%s)",
				m->from, m->host, message);
		else
			newlinef(c, BUFFER_LINE_QUIT, FROM_QUIT, "%s!%s has quit",
				m->from, m->host);

	} while ((c = c->next) != s->channel);

	draw(DRAW_STATUS);

	return 0;
}

static int
recv_topic(struct server *s, struct irc_message *m)
{
	/* :nick!user@host TOPIC <channel> [:topic] */

	char *chan;
	char *topic;
	struct channel *c;

	if (!m->from)
		failf(s, "TOPIC: sender's nick is null");

	if (!irc_message_param(m, &chan))
		failf(s, "TOPIC: channel is null");

	if (!irc_message_param(m, &topic))
		failf(s, "TOPIC: topic is null");

	if ((c = channel_list_get(&s->clist, chan, s->casemapping)) == NULL)
		failf(s, "TOPIC: channel '%s' not found", chan);

	if (*topic) {
		newlinef(c, 0, FROM_INFO, "%s has set the topic:", m->from);
		newlinef(c, 0, FROM_INFO, "\"%s\"", topic);
	} else {
		newlinef(c, 0, FROM_INFO, "%s has unset the topic", m->from);
	}

	return 0;
}

static int
recv_wallops(struct server *s, struct irc_message *m)
{
	/* :nick!user@host WALLOPS <:message> */

	const char *params;
	const char *trailing;

	irc_message_split(m, &params, &trailing);

	if (!m->from)
		failf(s, "WALLOPS: sender's nick is null");

	if (!trailing)
		failf(s, "WALLOPS: message is null");
		
	newlinef(s->channel, BUFFER_LINE_NICK, m->from, "%s", trailing);

	return 0;
}

static int
recv_ircv3_cap(struct server *s, struct irc_message *m)
{
	int ret;

	if ((ret = ircv3_recv_CAP(s, m)) && !s->registered)
		io_dx(s->connection, 0);

	return ret;
}

static int
recv_ircv3_authenticate(struct server *s, struct irc_message *m)
{
	int ret;

	if ((ret = ircv3_recv_AUTHENTICATE(s, m)) && !s->registered)
		io_dx(s->connection, 0);

	return ret;
}

static int
recv_ircv3_account(struct server *s, struct irc_message *m)
{
	/* :nick!user@host ACCOUNT <account> */

	char *account;
	struct channel *c = s->channel;

	if (!m->from)
		failf(s, "ACCOUNT: sender's nick is null");

	if (!irc_message_param(m, &account))
		failf(s, "ACCOUNT: account is null");

	do {
		if (irc_recv_threshold_filter(threshold_account, c->users.count))
			continue;

		if (!user_list_get(&(c->users), s->casemapping, m->from, 0))
			continue;

		if (!strcmp(account, "*"))
			newlinef(c, 0, FROM_INFO, "%s has logged out", m->from);
		else
			newlinef(c, 0, FROM_INFO, "%s has logged in as %s", m->from, account);

	} while ((c = c->next) != s->channel);

	return 0;
}

static int
recv_ircv3_away(struct server *s, struct irc_message *m)
{
	/* :nick!user@host AWAY [:message] */

	char *message;
	struct channel *c = s->channel;

	if (!m->from)
		failf(s, "AWAY: sender's nick is null");

	irc_message_param(m, &message);

	do {
		if (irc_recv_threshold_filter(threshold_away, c->users.count))
			continue;

		if (!user_list_get(&(c->users), s->casemapping, m->from, 0))
			continue;

		if (message)
			newlinef(c, 0, FROM_INFO, "%s is now away: %s", m->from, message);
		else
			newlinef(c, 0, FROM_INFO, "%s is no longer away", m->from);

	} while ((c = c->next) != s->channel);

	return 0;
}

static int
recv_ircv3_chghost(struct server *s, struct irc_message *m)
{
	/* :nick!user@host CHGHOST new_user new_host */

	char *user;
	char *host;
	struct channel *c = s->channel;

	if (!m->from)
		failf(s, "CHGHOST: sender's nick is null");

	if (!irc_message_param(m, &user))
		failf(s, "CHGHOST: user is null");

	if (!irc_message_param(m, &host))
		failf(s, "CHGHOST: host is null");

	do {
		if (irc_recv_threshold_filter(threshold_chghost, c->users.count))
			continue;

		if (!user_list_get(&(c->users), s->casemapping, m->from, 0))
			continue;

		newlinef(c, 0, FROM_INFO, "%s has changed user/host: %s/%s", m->from, user, host);

	} while ((c = c->next) != s->channel);

	return 0;
}

static int
irc_recv_threshold_filter(unsigned filter, unsigned count)
{
	/* returns 1 if message should be filtered */

	if (filter == UINT_MAX)
		return 1;

	if (filter == 0)
		return 0;

	return (filter < count);
}

#undef failf
#undef sendf
