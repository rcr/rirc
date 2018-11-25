#include "src/components/server.h"
#include "src/handlers/irc_ctcp.h"
#include "src/handlers/irc_recv.gperf.out"
#include "src/handlers/irc_recv.h"
#include "src/utils/utils.h"

enum irc_numerics
{
	RPL_WELCOME             =   1,
	RPL_YOURHOST            =   2,
	RPL_CREATED             =   3,
	RPL_MYINFO              =   4,
	RPL_ISUPPORT            =   5,
	RPL_TRACELINK           = 200,
	RPL_TRACECONNECTING     = 201,
	RPL_TRACEHANDSHAKE      = 202,
	RPL_TRACEUNKNOWN        = 203,
	RPL_TRACEOPERATOR       = 204,
	RPL_TRACEUSER           = 205,
	RPL_TRACESERVER         = 206,
	RPL_TRACESERVICE        = 207,
	RPL_TRACENEWTYPE        = 208,
	RPL_TRACECLASS          = 209,
	RPL_TRACELOG            = 210,
	RPL_STATSLINKINFO       = 211,
	RPL_STATSCOMMANDS       = 212,
	RPL_STATSCLINE          = 213,
	RPL_STATSNLINE          = 214,
	RPL_STATSILINE          = 215,
	RPL_STATSKLINE          = 216,
	RPL_STATSQLINE          = 217,
	RPL_STATSYLINE          = 218,
	RPL_ENDOFSTATS          = 219,
	RPL_UMODEIS             = 221,
	RPL_SERVLIST            = 234,
	RPL_SERVLISTEND         = 235,
	RPL_STATSVLINE          = 240,
	RPL_STATSLLINE          = 241,
	RPL_STATSUPTIME         = 242,
	RPL_STATSOLINE          = 243,
	RPL_STATSHLINE          = 244,
	RPL_STATSSLINE          = 245,
	RPL_STATSPING           = 246,
	RPL_STATSBLINE          = 247,
	RPL_STATSCONN           = 250,
	RPL_LUSERCLIENT         = 251,
	RPL_LUSEROP             = 252,
	RPL_LUSERUNKNOWN        = 253,
	RPL_LUSERCHANNELS       = 254,
	RPL_LUSERME             = 255,
	RPL_ADMINME             = 256,
	RPL_ADMINLOC1           = 257,
	RPL_ADMINLOC2           = 258,
	RPL_ADMINEMAIL          = 259,
	RPL_TRACEEND            = 262,
	RPL_TRYAGAIN            = 263,
	RPL_LOCALUSERS          = 265,
	RPL_GLOBALUSERS         = 266,
	RPL_AWAY                = 301,
	ERR_USERHOST            = 302,
	RPL_ISON                = 303,
	RPL_UNAWAY              = 305,
	RPL_NOWAWAY             = 306,
	RPL_WHOISUSER           = 311,
	RPL_WHOISSERVER         = 312,
	RPL_WHOISOPERATOR       = 313,
	RPL_WHOWASUSER          = 314,
	RPL_ENDOFWHO            = 315,
	RPL_WHOISIDLE           = 317,
	RPL_ENDOFWHOIS          = 318,
	RPL_WHOISCHANNELS       = 319,
	RPL_LIST                = 322,
	RPL_LISTEND             = 323,
	RPL_CHANNELMODEIS       = 324,
	RPL_UNIQOPIS            = 325,
	RPL_CHANNEL_URL         = 328,
	RPL_NOTOPIC             = 331,
	RPL_TOPIC               = 332,
	RPL_TOPICWHOTIME        = 333,
	RPL_INVITING            = 341,
	RPL_INVITELIST          = 346,
	RPL_ENDOFINVITELIST     = 347,
	RPL_EXCEPTLIST          = 348,
	RPL_ENDOFEXCEPTLIST     = 349,
	RPL_VERSION             = 351,
	RPL_WHOREPLY            = 352,
	RPL_NAMREPLY            = 353,
	RPL_LINKS               = 364,
	RPL_ENDOFLINKS          = 365,
	RPL_ENDOFNAMES          = 366,
	RPL_BANLIST             = 367,
	RPL_ENDOFBANLIST        = 368,
	RPL_ENDOFWHOWAS         = 369,
	RPL_INFO                = 371,
	RPL_MOTD                = 372,
	RPL_ENDOFINFO           = 374,
	RPL_MOTDSTART           = 375,
	RPL_ENDOFMOTD           = 376,
	RPL_YOUREOPER           = 381,
	RPL_TIME                = 391,
	ERR_NOSUCHNICK          = 401,
	ERR_NOSUCHSERVER        = 402,
	ERR_NOSUCHCHANNEL       = 403,
	ERR_CANNOTSENDTOCHAN    = 404,
	ERR_TOOMANYCHANNELS     = 405,
	ERR_WASNOSUCHNICK       = 406,
	ERR_TOOMANYTARGETS      = 407,
	ERR_NOSUCHSERVICE       = 408,
	ERR_NOORIGIN            = 409,
	ERR_NORECIPIENT         = 411,
	ERR_NOTEXTTOSEND        = 412,
	ERR_NOTOPLEVEL          = 413,
	ERR_WILDTOPLEVEL        = 414,
	ERR_BADMASK             = 415,
	ERR_TOOMANYMATCHES      = 416,
	ERR_UNKNOWNCOMMAND      = 421,
	ERR_NOMOTD              = 422,
	ERR_NOADMININFO         = 423,
	ERR_NONICKNAMEGIVEN     = 431,
	ERR_ERRONEUSNICKNAME    = 432,
	ERR_NICKNAMEINUSE       = 433,
	ERR_NICKCOLLISION       = 436,
	ERR_UNAVAILRESOURCE     = 437,
	ERR_USERNOTINCHANNEL    = 441,
	ERR_NOTONCHANNEL        = 442,
	ERR_USERONCHANNEL       = 443,
	ERR_NOTREGISTERED       = 451,
	ERR_NEEDMOREPARAMS      = 461,
	ERR_ALREADYREGISTRED    = 462,
	ERR_NOPERMFORHOST       = 463,
	ERR_PASSWDMISMATCH      = 464,
	ERR_YOUREBANNEDCREEP    = 465,
	ERR_YOUWILLBEBANNED     = 466,
	ERR_KEYSET              = 467,
	ERR_CHANNELISFULL       = 471,
	ERR_UNKNOWNMODE         = 472,
	ERR_INVITEONLYCHAN      = 473,
	ERR_BANNEDFROMCHAN      = 474,
	ERR_BADCHANNELKEY       = 475,
	ERR_BADCHANMASK         = 476,
	ERR_NOCHANMODES         = 477,
	ERR_BANLISTFULL         = 478,
	ERR_NOPRIVILEGES        = 481,
	ERR_CHANOPRIVSNEEDED    = 482,
	ERR_CANTKILLSERVER      = 483,
	ERR_RESTRICTED          = 484,
	ERR_UNIQOPPRIVSNEEDED   = 485,
	ERR_NOOPERHOST          = 491,
	ERR_UMODEUNKNOWNFLAG    = 501,
	ERR_USERSDONTMATCH      = 502,
	RPL_HELP                = 705,
	RPL_ENDOFHELP           = 706,
};

int
irc_recv(struct server *s, struct parsed_mesg *m)
{
	(void)s;
	(void)m;

	(void)recv_error;
	(void)recv_invite;
	(void)recv_join;
	(void)recv_kick;
	(void)recv_mode;
	(void)recv_nick;
	(void)recv_notice;
	(void)recv_part;
	(void)recv_ping;
	(void)recv_pong;
	(void)recv_privmsg;
	(void)recv_quit;
	(void)recv_topic;

	return 0;
}

static int recv_error(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_invite(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_join(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_kick(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_mode(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_nick(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_notice(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_part(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_ping(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_pong(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_privmsg(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_quit(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
static int recv_topic(struct server *s, struct parsed_mesg *m) { (void)s; (void)m; return 0; }
