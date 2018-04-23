#ifndef MESG_H
#define MESG_H

#include <stddef.h>

#include "src/components/channel.h"
#include "src/components/server.h"

void recv_mesg(struct server*, struct parsed_mesg*);
void send_mesg(struct server*, struct channel*, char*);

#if 0

SEND:
	3.1.1  PASS <password>
	3.1.2  NICK <nick>
	3.1.3  USER <user> <mode> <unused> <realname>
	3.1.4  OPER <nick> <password>
	3.1.7  QUIT [<message>]
	3.2.1  JOIN 0 \ (<channel> *("," <channel>) [ <key> *("," <key>)])
	3.2.2  PART <channel> *("," <channel>) [<message>]
	3.2.3  MODE <target> 1*(<modestring> *[<modeargs>])
	3.2.4  TOPIC <channel> [:[<message>]]
	3.2.5  NAMES [<channel> *["," <channel>]]
	3.2.6  LIST [<channel> *["," <channel>]]
	3.2.7  INVITE <nick> <channel>
	3.2.8  KICK <channel> *("," <channel>) <nick> *("," <user>) <message>
	3.3.1  PRIVMSG <target> <text>
	3.3.2  NOTICE <target> <text>
	3.4.1  MOTD [<target>]
	3.4.2  LUSERS [<mask> [<target>]]
	3.4.3  VERSION [<target>]
	3.4.4  STATS <query> [<target>]
	3.4.5  LINKS [[<remote server>] <server mask>]
	3.4.6  TIME [<target>]
	3.4.7  CONNECT <target server> <port> [<remote server>]
	3.4.8  TRACE [<target>]
	3.4.9  ADMIN [<target>]
	3.4.10 INFO [<target>]
	3.5.1  SERVLIST [<mask> [<type]]
	3.5.2  SQUERY <target> <text>
	3.6.1  WHO [<mask> ["o"]]
	3.6.2  WHOIS [<target>]
	3.6.3  WHOWAS <nick> [<count> [<server>]]
	3.7.1  KILL <nick> <comment>
	3.7.2  PING <server1> [<server2>]
	3.7.3  PONG <server1> [<server2>]
	CTCP:
		6.1  ACTION <text>
		6.2  CLIENTINFO
		6.4  FINGER
		6.5  PING 1*<arg>
		6.6  SOURCE
		6.7  TIME
		6.7  VERSION
		6.8  USERINFO

RECEIVE:
	3.1.2  :<from>[!<host>] NICK <nick>
	3.1.7  :<from>[!<host>] QUIT [<message>]
	3.2.1  :<from>[!<host>] JOIN <channel>
	3.2.2  :<from>[!<host>] PART <channel> [<message>]
	3.2.3  :<from>[!<host>] MODE <target> 1*(<modestring> *[<modeargs>])
	3.2.4  :<from>[!<host>] TOPIC <channel> [:[<topic>]]
	3.2.7  :<from>[!<host>] INVITE <nick> <channel>
	3.2.8  :<from>[!<host>] KICK <channel> <nick>
	3.3.1  :<from>[!<host>] PRIVMSG <target> <text>
	3.3.2  :<from>[!<host>] NOTICE <target> <text>
	3.5.2  :<from>[!<host>] SQUERY <target> <text>
	3.7.2  :<from> PING <server1> [<server2]
	3.7.3  :<from> PONG <server1> [<server2]
	3.7.4  :<from> ERROR <message>
	CTCP:
		6.1  ... :\x01ACTION <text>\x01
		6.2  ... :\x01CLIENTINFO 1*<arg>\x01
		6.4  ... :\x01FINGER <text>\x01
		6.5  ... :\x01PING 1*<arg>\x01
		6.6  ... :\x01SOURCE <text>\x01
		6.7  ... :\x01TIME <text>\x01
		6.8  ... :\x01VERSION <text>\x01
		6.9  ... :\x01USERINFO <text>\x01

NUMERIC:
	RPL_WELCOME,             1
	RPL_YOURHOST,            2
	RPL_CREATED,             3
	RPL_MYINFO,              4
	RPL_ISUPPORT,            5
	RPL_TRACELINK,         200
	RPL_TRACECONNECTING,   201
	RPL_TRACEHANDSHAKE,    202
	RPL_TRACEUNKNOWN,      203
	RPL_TRACEOPERATOR,     204
	RPL_TRACEUSER,         205
	RPL_TRACESERVER,       206
	RPL_TRACESERVICE,      207
	RPL_TRACENEWTYPE,      208
	RPL_TRACECLASS,        209
	RPL_TRACELOG,          210
	RPL_STATSLINKINFO,     211
	RPL_STATSCOMMANDS,     212
	RPL_STATSCLINE,        213
	RPL_STATSNLINE,        214
	RPL_STATSILINE,        215
	RPL_STATSKLINE,        216
	RPL_STATSQLINE,        217
	RPL_STATSYLINE,        218
	RPL_ENDOFSTATS,        219
	RPL_UMODEIS,           221
	RPL_SERVLIST,          234
	RPL_SERVLISTEND,       235
	RPL_STATSVLINE,        240
	RPL_STATSLLINE,        241
	RPL_STATSUPTIME,       242
	RPL_STATSOLINE,        243
	RPL_STATSHLINE,        244
	RPL_STATSSLINE,        245
	RPL_STATSPING,         246
	RPL_STATSBLINE,        247
	RPL_STATSDLINE,        250
	RPL_LUSERCLIENT,       251
	RPL_LUSEROP,           252
	RPL_LUSERUNKNOWN,      253
	RPL_LUSERCHANNELS,     254
	RPL_LUSERME,           255
	RPL_ADMINME,           256
	RPL_ADMINLOC1,         257
	RPL_ADMINLOC2,         258
	RPL_ADMINEMAIL,        259
	RPL_TRACEEND,          262
	RPL_TRYAGAIN,          263
	RPL_LOCALUSERS,        265
	RPL_GLOBALUSERS,       266
	RPL_AWAY,              301
	RPL_WHOISUSER,         311
	RPL_WHOISSERVER,       312
	RPL_WHOISOPERATOR,     313
	RPL_WHOWASUSER,        314
	RPL_ENDOFWHO,          315
	RPL_WHOISIDLE,         317
	RPL_ENDOFWHOIS,        318
	RPL_WHOISCHANNELS,     319
	RPL_LIST,              322
	RPL_LISTEND,           323
	RPL_CHANNELMODEIS,     324
	RPL_UNIQOPIS,          325
	RPL_CHANNEL_URL,       328
	RPL_NOTOPIC,           331
	RPL_TOPIC,             332
	RPL_TOPICWHOTIME,      333
	RPL_INVITING,          341
	RPL_INVITELIST,        346
	RPL_ENDOFINVITELIST,   347
	RPL_EXCEPTLIST,        348
	RPL_ENDOFEXCEPTLIST,   349
	RPL_VERSION,           351
	RPL_WHOREPLY,          352
	RPL_NAMREPLY,          353
	RPL_LINKS,             364
	RPL_ENDOFLINKS,        365
	RPL_ENDOFNAMES,        366
	RPL_BANLIST,           367
	RPL_ENDOFBANLIST,      368
	RPL_ENDOFWHOWAS,       369
	RPL_INFO,              371
	RPL_MOTD,              372
	RPL_ENDOFINFO,         374
	RPL_MOTDSTART,         375
	RPL_ENDOFMOTD,         376
	RPL_YOUREOPER,         381
	RPL_TIME,              391
	ERR_NOSUCHNICK,        401
	ERR_NOSUCHSERVER,      402
	ERR_NOSUCHCHANNEL,     403
	ERR_CANNOTSENDTOCHAN,  404
	ERR_TOOMANYCHANNELS,   405
	ERR_WASNOSUCHNICK,     406
	ERR_TOOMANYTARGETS,    407
	ERR_NOSUCHSERVICE,     408
	ERR_NOORIGIN,          409
	ERR_NORECIPIENT,       411
	ERR_NOTEXTTOSEND,      412
	ERR_NOTOPLEVEL,        413
	ERR_WILDTOPLEVEL,      414
	ERR_BADMASK,           415
	ERR_TOOMANYMATCHES,    416
	ERR_UNKNOWNCOMMAND,    421
	ERR_NOMOTD,            422
	ERR_NOADMININFO,       423
	ERR_NONICKNAMEGIVEN,   431
	ERR_ERRONEUSNICKNAME,  432
	ERR_NICKNAMEINUSE,     433
	ERR_NICKCOLLISION,     436
	ERR_UNAVAILRESOURCE,   437
	ERR_USERNOTINCHANNEL,  441
	ERR_NOTONCHANNEL,      442
	ERR_USERONCHANNEL,     443
	ERR_NOTREGISTERED,     451
	ERR_NEEDMOREPARAMS,    461
	ERR_ALREADYREGISTRED,  462
	ERR_NOPERMFORHOST,     463
	ERR_PASSWDMISMATCH,    464
	ERR_YOUREBANNEDCREEP,  465
	ERR_YOUWILLBEBANNED,   466
	ERR_KEYSET,            467
	ERR_CHANNELISFULL,     471
	ERR_UNKNOWNMODE,       472
	ERR_INVITEONLYCHAN,    473
	ERR_BANNEDFROMCHAN,    474
	ERR_BADCHANNELKEY,     475
	ERR_BADCHANMASK,       476
	ERR_NOCHANMODES,       477
	ERR_BANLISTFULL,       478
	ERR_NOPRIVILEGES,      481
	ERR_CHANOPRIVSNEEDED,  482
	ERR_CANTKILLSERVER,    483
	ERR_RESTRICTED,        484
	ERR_UNIQOPPRIVSNEEDED, 485
	ERR_NOOPERHOST,        491
	ERR_UMODEUNKNOWNFLAG,  501
	ERR_USERSDONTMATCH,    502
	RPL_HELP,              705
	RPL_ENDOFHELP,         706

NOT IMPLEMENTED (Including associated numerics):
	3.1.6 SERVICE
	3.1.8 SQUIT
	4.1   AWAY
	4.2   REHASH
	4.3   DIE
	4.4   RESTART
	4.5   SUMMON
	4.6   USERS
	4.7   OPERWALL
	4.8   USERHOST
	4.9   ISON
	6.3   DCC
	7.1   CAP
	7.2   AUTHENTICATE


#endif

#endif
