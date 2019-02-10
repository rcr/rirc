#ifndef IRC_RECV_H
#define IRC_RECV_H

/* Summary of irc protocol implementation:
 *
 * 3.1  Connection Registration
 *
 *     3.1.1  PASS <password>
 *         ERR_NEEDMOREPARAMS   461
 *         ERR_ALREADYREGISTRED 462
 *
 *     3.1.2  NICK <nick>
 *         ERR_NONICKNAMEGIVEN  431
 *         ERR_ERRONEUSNICKNAME 432
 *         ERR_NICKNAMEINUSE    433
 *         ERR_NICKCOLLISION    436
 *         ERR_UNAVAILRESOURCE  437
 *         ERR_RESTRICTED       484
 *         ---
 *         :<from>[!<host>] NICK <nick>
 *
 *     3.1.3  USER <user> <mode> <unused> <realname>
 *         ERR_NEEDMOREPARAMS   461
 *         ERR_ALREADYREGISTRED 462
 *
 *     3.1.4  OPER <nick> <password>
 *         RPL_YOUREOPER      381
 *         ERR_NEEDMOREPARAMS 461
 *         ERR_PASSWDMISMATCH 464
 *         ERR_NOOPERHOST     491
 *
 *     3.1.5  MODE <target> 1*(<modestring> *[<modeargs>])
 *         RPL_UMODEIS          221
 *         ERR_NEEDMOREPARAMS   461
 *         ERR_UMODEUNKNOWNFLAG 501
 *         ERR_USERSDONTMATCH   502
 *         ---
 *         :<from>[!<host>] MODE <target> 1*(<modestring> *[<modeargs>])
 *
 *     3.1.6  SERVICE
 *         --- NOT IMPLEMENTED ---
 *
 *     3.1.7  QUIT [<message>]
 *         :<from>[!<host>] QUIT [<message>]
 *
 *     3.1.8  SQUIT
 *         --- NOT IMPLEMENTED ---
 *
 * 3.2  Channel operations
 *
 *     3.2.1  JOIN (<channel> *("," <channel>) [ <key> *("," <key>)])
 *            JOIN 0
 *         RPL_TOPIC           332
 *         RPL_TOPICWHOTIME    333 -- extension
 *         RPL_NAMREPLY        353
 *         RPL_ENDOFNAMES      366
 *         ERR_NOSUCHCHANNEL   403
 *         ERR_TOOMANYCHANNELS 405
 *         ERR_TOOMANYTARGETS  407
 *         ERR_UNAVAILRESOURCE 437
 *         ERR_NEEDMOREPARAMS  461
 *         ERR_CHANNELISFULL   471
 *         ERR_INVITEONLYCHAN  473
 *         ERR_BANNEDFROMCHAN  474
 *         ERR_BADCHANNELKEY   475
 *         ERR_BADCHANMASK     476
 *         ---
 *         :<from>[!<host>] JOIN <channel>
 *
 *     3.2.2  PART <channel> *("," <channel>) [<message>]
 *         ERR_NOSUCHCHANNEL  403
 *         ERR_NOTONCHANNEL   442
 *         ERR_NEEDMOREPARAMS 461
 *         ---
 *         :<from>[!<host>] PART <channel> [<message>]
 *
 *     3.2.3  MODE <target> 1*(<modestring> *[<modeargs>])
 *         RPL_CHANNELMODEIS    324
 *         RPL_UNIQOPIS         325
 *         RPL_INVITELIST       346
 *         RPL_ENDOFINVITELIST  347
 *         RPL_EXCEPTLIST       348
 *         RPL_ENDOFEXCEPTLIST  349
 *         RPL_BANLIST          367
 *         RPL_ENDOFBANLIST     368
 *         ERR_USERNOTINCHANNEL 441
 *         ERR_NEEDMOREPARAMS   461
 *         ERR_KEYSET           467
 *         ERR_UNKNOWNMODE      472
 *         ERR_NOCHANMODES      477
 *         ERR_CHANOPRIVSNEEDED 482
 *         ---
 *         :<from>[!<host>] MODE <target> 1*(<modestring> *[<modeargs>])
 *
 *     3.2.4  TOPIC <channel> [:[<message>]]
 *         RPL_NOTOPIC          331
 *         RPL_TOPIC            332
 *         RPL_TOPICWHOTIME     333 -- extension
 *         ERR_NOTONCHANNEL     442
 *         ERR_NEEDMOREPARAMS   461
 *         ERR_NOCHANMODES      477
 *         ERR_CHANOPRIVSNEEDED 482
 *         ---
 *         :<from>[!<host>] TOPIC <channel> [:[<topic>]]
 *
 *     3.2.5  NAMES [<channel> *["," <channel>]]
 *         RPL_NAMREPLY       353
 *         RPL_ENDOFNAMES     366
 *         ERR_NOSUCHSERVER   402
 *         ERR_TOOMANYMATCHES 416
 *
 *     3.2.6  LIST [<channel> *["," <channel>]]
 *         RPL_LIST           322
 *         RPL_LISTEND        323
 *         ERR_NOSUCHSERVER   402
 *         ERR_TOOMANYMATCHES 416
 *
 *     3.2.7  INVITE <nick> <channel>
 *         RPL_AWAY             301
 *         RPL_INVITING         341
 *         ERR_NOSUCHNICK       401
 *         ERR_NOTONCHANNEL     442
 *         ERR_USERONCHANNEL    443
 *         ERR_NEEDMOREPARAMS   461
 *         ERR_CHANOPRIVSNEEDED 482
 *         ---
 *         :<from>[!<host>] INVITE <nick> <channel>
 *
 *     3.2.8  KICK <channel> *("," <channel>) <nick> *("," <user>) <message>
 *         ERR_NOSUCHCHANNEL    403
 *         ERR_USERNOTINCHANNEL 441
 *         ERR_NOTONCHANNEL     442
 *         ERR_NEEDMOREPARAMS   461
 *         ERR_BADCHANMASK      476
 *         ERR_CHANOPRIVSNEEDED 482
 *         ---
 *         :<from>[!<host>] KICK <channel> <nick>
 *
 * 3.3  Sending
 *
 *     3.3.1  PRIVMSG <target> <text>
 *         RPL_AWAY             301
 *         ERR_NOSUCHNICK       401
 *         ERR_CANNOTSENDTOCHAN 404
 *         ERR_TOOMANYTARGETS   407
 *         ERR_NORECIPIENT      411
 *         ERR_NOTEXTTOSEND     412
 *         ERR_NOTOPLEVEL       413
 *         ERR_WILDTOPLEVEL     414
 *         ---
 *         :<from>[!<host>] PRIVMSG <target> <text>
 *
 *     3.3.2  NOTICE <target> <text>
 *         --- Identical to PRIVMSG ---
 *
 * 3.4  Server queries and commands
 *
 *     3.4.1  MOTD [<target>]
 *         RPL_MOTD         372
 *         RPL_MOTDSTART    375
 *         RPL_ENDOFMOTD    376
 *         ERR_NOSUCHSERVER 402
 *         ERR_NOMOTD       422
 *
 *     3.4.2  LUSERS [<mask> [<target>]]
 *         RPL_LUSERCLIENT   251
 *         RPL_LUSEROP       252
 *         RPL_LUSERUNKNOWN  253
 *         RPL_LUSERCHANNELS 254
 *         RPL_LUSERME       255
 *         ERR_NOSUCHSERVER  402
 *
 *     3.4.3  VERSION [<target>]
 *         RPL_ISUPPORT     005 -- extension
 *         RPL_VERSION      351
 *         ERR_NOSUCHSERVER 402
 *
 *     3.4.4  STATS <query> [<target>]
 *         RPL_STATSLINKINFO  211
 *         RPL_STATSCOMMANDS  212
 *         RPL_STATSCLINE     213
 *         RPL_STATSNLINE     214
 *         RPL_STATSILINE     215
 *         RPL_STATSKLINE     216
 *         RPL_STATSQLINE     217
 *         RPL_STATSYLINE     218
 *         RPL_ENDOFSTATS     219
 *         RPL_STATSVLINE     240
 *         RPL_STATSLLINE     241
 *         RPL_STATSUPTIME    242
 *         RPL_STATSOLINE     243
 *         RPL_STATSHLINE     244
 *         RPL_STATSSLINE     245
 *         RPL_STATSPING      246
 *         RPL_STATSBLINE     247
 *         ERR_NOSUCHSERVER   402
 *         ERR_NEEDMOREPARAMS 461
 *         ERR_NOPRIVILEGES   481
 *
 *     3.4.5  LINKS [[<remote server>] <server mask>]
 *         ERR_NOSUCHSERVER   402
 *         RPL_LINKS          364
 *         RPL_ENDOFLINKS     365
 *
 *     3.4.6  TIME [<target>]
 *         RPL_TIME         391
 *         ERR_NOSUCHSERVER 402
 *
 *     3.4.7  CONNECT <target server> <port> [<remote server>]
 *         ERR_NOSUCHSERVER   402
 *         ERR_NEEDMOREPARAMS 461
 *         ERR_NOPRIVILEGES   481
 *
 *     3.4.8  TRACE [<target>]
 *         RPL_TRACELINK       200
 *         RPL_TRACECONNECTING 201
 *         RPL_TRACEHANDSHAKE  202
 *         RPL_TRACEUNKNOWN    203
 *         RPL_TRACEOPERATOR   204
 *         RPL_TRACEUSER       205
 *         RPL_TRACESERVER     206
 *         RPL_TRACESERVICE    207
 *         RPL_TRACENEWTYPE    208
 *         RPL_TRACECLASS      209
 *         RPL_TRACELOG        210
 *         RPL_TRACEEND        262
 *         ERR_NOSUCHSERVER    402
 *
 *     3.4.9  ADMIN [<target>]
 *         RPL_ADMINME      256
 *         RPL_ADMINLOC1    257
 *         RPL_ADMINLOC2    258
 *         RPL_ADMINEMAIL   259
 *         ERR_NOSUCHSERVER 402
 *         ERR_NOADMININFO  423
 *
 *     3.4.10  INFO [<target>]
 *         RPL_INFO         371
 *         RPL_ENDOFINFO    374
 *         ERR_NOSUCHSERVER 402
 *
 * 3.5  Service Query and Commands
 *
 *     3.5.1  SERVLIST [<mask> [<type]]
 *         RPL_SERVLIST    234
 *         RPL_SERVLISTEND 235
 *
 *     3.5.2  SQUERY <target> <text>
 *         --- Identical to PRIVMSG ---
 *
 * 3.6  User based queries
 *
 *     3.6.1  WHO [<mask> ["o"]]
 *         RPL_ENDOFWHO     315
 *         RPL_WHOREPLY     352
 *         ERR_NOSUCHSERVER 402
 *
 *     3.6.2  WHOIS [<target>]
 *         RPL_AWAY            301
 *         RPL_WHOISUSER       311
 *         RPL_WHOISSERVER     312
 *         RPL_WHOISOPERATOR   313
 *         RPL_WHOISIDLE       317
 *         RPL_ENDOFWHOIS      318
 *         RPL_WHOISCHANNELS   319
 *         ERR_NOSUCHNICK      401
 *         ERR_NOSUCHSERVER    402
 *         ERR_NONICKNAMEGIVEN 431
 *
 *     3.6.3  WHOWAS <nick> [<count> [<server>]]
 *         RPL_WHOISSERVER     312
 *         RPL_WHOWASUSER      314
 *         RPL_ENDOFWHOWAS     369
 *         ERR_WASNOSUCHNICK   406
 *         ERR_NONICKNAMEGIVEN 431
 *
 * 3.7  Miscellaneous
 *
 *     3.7.1  KILL <nick> <comment>
 *         ERR_NOPRIVILEGES   481
 *         ERR_NEEDMOREPARAMS 461
 *         ERR_NOSUCHNICK     401
 *         ERR_CANTKILLSERVER 483
 *
 *     3.7.2  PING <server1> [<server2>]
 *         ERR_NOSUCHSERVER 402
 *         ERR_NOORIGIN     409
 *         :<from> PING <server1> [<server2]
 *
 *     3.7.3  PONG <server1> [<server2>]
 *         ERR_NOSUCHSERVER 402
 *         ERR_NOORIGIN     409
 *         :<from> PONG <server1> [<server2]
 *
 *     3.7.4  ERROR
 *         ---
 *         :<from> ERROR <message>
 *
 * 4.  Optional features
 *
 *     4.1  AWAY [ <text> ]
 *         RPL_NOWAWAY 306
 *         RPL_UNAWAY  305
 *     4.2  REHASH
 *         --- NOT IMPLEMENTED ---
 *     4.3  DIE
 *         --- NOT IMPLEMENTED ---
 *     4.4  RESTART
 *         --- NOT IMPLEMENTED ---
 *     4.5  SUMMON
 *         --- NOT IMPLEMENTED ---
 *     4.6  USERS
 *         --- NOT IMPLEMENTED ---
 *     4.7  OPERWALL
 *         --- NOT IMPLEMENTED ---
 *     4.8  USERHOST <nickname> *( SPACE <nickname> )
 *         ERR_USERHOST         302
 *         ERR_NEEDMOREPARAMS   461
 *     4.9  ISON <nickname> *( SPACE <nickname> )
 *         RPL_ISON             303
 *         ERR_NEEDMOREPARAMS   461
 *
 * 5. Additional numeric replies
 *
 *     RPL_WELCOME           001
 *     RPL_YOURHOST          002
 *     RPL_CREATED           003
 *     RPL_MYINFO            004
 *     RPL_ISUPPORT          005 -- extension
 *     RPL_STATSCONN         250 -- extension
 *     RPL_TRYAGAIN          263
 *     RPL_LOCALUSERS        265 -- extension
 *     RPL_GLOBALUSERS       266 -- extension
 *     RPL_CHANNEL_URL       328 -- extension
 *     RPL_CREATIONTIME      329 -- extension
 *     ERR_NOSUCHSERVICE     408
 *     ERR_BADMASK           415
 *     ERR_UNKNOWNCOMMAND    421
 *     ERR_NOTREGISTERED     451
 *     ERR_NOPERMFORHOST     463
 *     ERR_YOUREBANNEDCREEP  465
 *     ERR_YOUWILLBEBANNED   466
 *     ERR_BANLISTFULL       478
 *     ERR_UNIQOPPRIVSNEEDED 485
 *     RPL_HELPSTART         704 -- extension
 *     RPL_HELP              705 -- extension
 *     RPL_ENDOFHELP         706 -- extension
 *
 * 7. IRCv3 features
 *
 *     7.1 CAP
 *         --- NOT IMPLEMENTED ---
 *
 *     7.2 AUTHENTICATE
 *         --- NOT IMPLEMENTED ---
 *
 *     7.3 TLS
 *         --- NOT IMPLEMENTED ---
 *
 *     7.4 SASL
 *         --- NOT IMPLEMENTED ---
 */

int irc_recv(struct server*, struct irc_message*);

#endif
