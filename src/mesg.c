/* All irc related messages
 *
 * interface to this file:
 *	recv_mesg
 *	send_mesg
 *
 *	consider how pasted input might make its way to this file
 *
 * For each send* type defined in the rfc, include a comment
 * with the list of errors this command can return
 * and make sure it's implemented
 *
 * For each send* type not defined by the rfc (eg: /version
 * include brief comment about what it does
 *
 * For each recv* type, include the standard format
 *
 * */

/* For strtok_r */
#define _POSIX_C_SOURCE 200112L

#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"

/* Numeric Reply Codes */
#define RPL_WELCOME            1
#define RPL_YOURHOST           2
#define RPL_CREATED            3
#define RPL_MYINFO             4
#define RPL_ISUPPORT           5
#define RPL_STATSCONN        250
#define RPL_LUSERCLIENT      251
#define RPL_LUSEROP          252
#define RPL_LUSERUNKNOWN     253
#define RPL_LUSERCHANNELS    254
#define RPL_LUSERME          255
#define RPL_LOCALUSERS       265
#define RPL_GLOBALUSERS      266
#define RPL_CHANNEL_URL      328
#define RPL_NOTOPIC          331
#define RPL_TOPIC            332
#define RPL_TOPICWHOTIME     333
#define RPL_NAMREPLY         353
#define RPL_ENDOFNAMES       366
#define RPL_MOTD             372
#define RPL_MOTDSTART        375
#define RPL_ENDOFMOTD        376
#define ERR_CANNOTSENDTOCHAN 404
#define ERR_ERRONEUSNICKNAME 432
#define ERR_NICKNAMEINUSE    433

/* Fail macros used in message sending/receiving handlers */
#define fail(M) \
	do { if (err) { strncpy(err, M, MAX_ERROR); } return 1; } while (0)

/* Fail with formatted message */
#define failf(M, ...) \
	do { if (err) { snprintf(err, MAX_ERROR, M, ##__VA_ARGS__); } return 1; } while (0)

/* Conditionally fail */
#define fail_if(C) \
	do { if (C) return 1; } while (0)

#define IS_ME(X) !strcmp(X, s->nick_me)

/* Message receiving handlers */
static int recv_ctcp_req(char*, parsed_mesg*, server*);
static int recv_ctcp_rpl(char*, parsed_mesg*);
static int recv_error(char*, parsed_mesg*, server*);
static int recv_join(char*, parsed_mesg*, server*);
static int recv_mode(char*, parsed_mesg*, server*);
static int recv_nick(char*, parsed_mesg*, server*);
static int recv_notice(char*, parsed_mesg*, server*);
static int recv_numeric(char*, parsed_mesg*, server*);
static int recv_part(char*, parsed_mesg*, server*);
static int recv_ping(char*, parsed_mesg*, server*);
static int recv_priv(char*, parsed_mesg*, server*);
static int recv_quit(char*, parsed_mesg*, server*);

/* Message sending handlers */
static int send_connect(char*, char*);
static int send_default(char*, char*);
static int send_disconnect(char*, char*);
static int send_emote(char*, char*);
static int send_join(char*, char*);
static int send_nick(char*, char*);
static int send_part(char*, char*);
static int send_priv(char*, char*);
static int send_quit(char*, char*);
static int send_raw(char*, char*);
static int send_version(char*, char*);

/*
 * Message sending handlers
 * */

void
send_mesg(char *mesg)
{
	char *cmd, errbuff[MAX_ERROR];

	int err = 0;

	if (*mesg != '/')
		err = send_default(errbuff, mesg);
	else {
		mesg++;

		if (!(cmd = strtok_r(mesg, " ", &mesg)))
			newline(ccur, 0, "-!!-", "Messages beginning with '/' require a command");
		else if (!strcasecmp(cmd, "JOIN"))
			err = send_join(errbuff, mesg);
		else if (!strcasecmp(cmd, "CONNECT"))
			err = send_connect(errbuff, mesg);
		else if (!strcasecmp(cmd, "DISCONNECT"))
			err = send_disconnect(errbuff, mesg);
		else if (!strcasecmp(cmd, "CLOSE"))
			ccur = channel_close(ccur);
		else if (!strcasecmp(cmd, "PART"))
			err = send_part(errbuff, mesg);
		else if (!strcasecmp(cmd, "NICK"))
			err = send_nick(errbuff, mesg);
		else if (!strcasecmp(cmd, "QUIT"))
			err = send_quit(errbuff, mesg);
		else if (!strcasecmp(cmd, "MSG"))
			err = send_priv(errbuff, mesg);
		else if (!strcasecmp(cmd, "PRIV"))
			err = send_priv(errbuff, mesg);
		else if (!strcasecmp(cmd, "ME"))
			err = send_emote(errbuff, mesg);
		else if (!strcasecmp(cmd, "VERSION"))
			err = send_version(errbuff, mesg);
		else if (!strcasecmp(cmd, "RAW"))
			err = send_raw(errbuff, mesg);
		else
			newlinef(ccur, 0, "-!!-", "Unknown command: %s", cmd);
	}

	if (err)
		newline(ccur, 0, "-!!-", errbuff);
}

void
send_paste(char *paste)
{
	/* TODO: send the paste buffer, which is preformatted with \r\n, and then
	 * split the messages and newline them into the buffer * */

	UNUSED(paste);
}

static int
send_connect(char *err, char *mesg)
{
	/* /connect [(host) | (host:port) | (host port)] */

	char *host, *port;

	if (!(host = strtok_r(mesg, " :", &mesg))) {

		if (!ccur->server || ccur->server->soc >= 0 || ccur->server->connecting)
			fail("Error: Connect requires a hostname argument");

		/* If no hostname arg and server is disconnected, attempt to reconnect */
		host = ccur->server->host;
		port = ccur->server->port;

	} else if (!(port = strtok_r(NULL, " ", &mesg)))
		port = "6667";

	server_connect(host, port);

	return 0;
}

static int
send_default(char *err, char *mesg)
{
	/* All messages not beginning with '/'  */

	if (!ccur->type)
		fail("Error: This is not a channel");

	if (ccur->parted)
		fail("Error: Parted from channel");

	fail_if(sendf(err, ccur->server, "PRIVMSG %s :%s", ccur->name, mesg));

	newline(ccur, LINE_CHAT, ccur->server->nick_me, mesg);

	return 0;
}

static int
send_disconnect(char *err, char *mesg)
{
	/* /disconnect [quit message] */

	server *s = ccur->server;

	/* Server isn't connecting, connected or waiting to connect */
	if (!s || (!s->connecting && s->soc < 0 && !s->reconnect_time))
		fail("Error: Not connected to server");

	server_disconnect(ccur->server, 0, 0, (*mesg) ? mesg : DEFAULT_QUIT_MESG);

	return 0;
}

static int
send_emote(char *err, char *mesg)
{
	/* /me <message> */

	if (!ccur->type)
		fail("Error: This is not a channel");

	if (ccur->parted)
		fail("Error: Parted from channel");

	fail_if(sendf(err, ccur->server, "PRIVMSG %s :\x01""ACTION %s\x01", ccur->name, mesg));

	newlinef(ccur, 0, "*", "%s %s", ccur->server->nick_me, mesg);

	return 0;
}

static int
send_join(char *err, char *mesg)
{
	/* /join [target[,targets]*] */

	char *targ;

	if ((targ = strtok(mesg, " ")))
		return sendf(err, ccur->server, "JOIN %s", targ);

	if (!ccur->type)
		fail("Error: JOIN requires a target");

	if (ccur->type == 'p')
		fail("Error: Can't rejoin private buffers");

	if (!ccur->parted)
		fail("Error: Not parted from channel");

	return sendf(err, ccur->server, "JOIN %s", ccur->name);
}

static int
send_nick(char *err, char *mesg)
{
	/* /nick [nick] */

	char *nick;

	if ((nick = strtok(mesg, " ")))
		return sendf(err, ccur->server, "NICK %s", mesg);

	newlinef(ccur, 0, "--", "Your nick is %s", ccur->server->nick_me);

	return 0;
}

static int
send_part(char *err, char *mesg)
{
	/* /part [[target[,targets]*] part message]*/

	char *targ;

	if ((targ = strtok_r(mesg, " ", &mesg)))
		return sendf(err, ccur->server, "PART %s :%s", targ, (*mesg) ? mesg : DEFAULT_QUIT_MESG);

	if (!ccur->type)
		fail("Error: PART requires a target");

	if (ccur->type == 'p')
		fail("Error: Can't part private buffers");

	if (ccur->parted)
		fail("Error: Already parted from channel");

	return sendf(err, ccur->server, "PART %s :%s", ccur->name, DEFAULT_QUIT_MESG);
}

static int
send_priv(char *err, char *mesg) {
	/* /(priv | msg) <target> <message> */

	char *targ;
	channel *c;

	if (!(targ = strtok_r(mesg, " ", &mesg)))
		fail("Error: Private messages require a target");

	if (*mesg == '\0')
		fail("Error: Private messages was null");

	fail_if(sendf(err, ccur->server, "PRIVMSG %s :%s", targ, mesg));

	if ((c = channel_get(targ, ccur->server)) == NULL) {
		c = new_channel(targ, ccur->server, ccur);
		c->type = 'p';
	}

	newline(c, LINE_CHAT, ccur->server->nick_me, mesg);

	return 0;
}

static int
send_raw(char *err, char *mesg)
{
	/* /raw <raw message> */

	fail_if(sendf(err, ccur->server, "%s", mesg));

	newline(ccur, 0, "RAW >>", mesg);

	return 0;
}

static int
send_quit(char *err, char *mesg)
{
	/* /quit [quit message] */

	UNUSED(err);

	server *t, *s = ccur->server;

	if (s) do {
		t = s;
		s = s->next;
		server_disconnect(t, 0, 1, (*mesg) ? mesg : DEFAULT_QUIT_MESG);
	} while (t != s);

	free_channel(rirc);

#ifndef DEBUG
	/* Clear screen */
	printf("\x1b[H\x1b[J");
#endif

	exit(EXIT_SUCCESS);

	return 0;
}

static int
send_version(char *err, char *mesg)
{
	/* /version [target] */

	char *targ;

	if (ccur->server == NULL) {
		newline(ccur, 0, "--", "rirc version " VERSION);
		newline(ccur, 0, "--", "http://rcr.io/rirc.html");
		return 0;
	}

	if ((targ = strtok(mesg, " "))) {
		newlinef(ccur, 0, "--", "Sending CTCP VERSION request to %s", targ);
		return sendf(err, ccur->server, "PRIVMSG %s :\x01""VERSION\x01", targ);
	}

	newlinef(ccur, 0, "--", "Sending CTCP VERSION request to %s", ccur->server->host);
	return sendf(err, ccur->server, "VERSION");
}

/*
 * Message receiving handlers
 * */

void
recv_mesg(char *inp, int count, server *s)
{
	char *ptr = s->iptr;
	char *max = s->input + BUFFSIZE;

	char errbuff[MAX_ERROR];

	int err = 0;

	parsed_mesg p;

	while (count--) {
		if (*inp == '\r') {

			*ptr = '\0';

#ifdef DEBUG
			newline(s->channel, 0, "", "");
			newline(s->channel, 0, "DEBUG <<", s->input);
#endif
			if (!(parse(&p, s->input)))
				newline(s->channel, 0, "-!!-", "Failed to parse message");
			else if (isdigit(*p.command))
				err = recv_numeric(errbuff, &p, s);
			else if (!strcmp(p.command, "PRIVMSG"))
				err = recv_priv(errbuff, &p, s);
			else if (!strcmp(p.command, "JOIN"))
				err = recv_join(errbuff, &p, s);
			else if (!strcmp(p.command, "PART"))
				err = recv_part(errbuff, &p, s);
			else if (!strcmp(p.command, "QUIT"))
				err = recv_quit(errbuff, &p, s);
			else if (!strcmp(p.command, "NOTICE"))
				err = recv_notice(errbuff, &p, s);
			else if (!strcmp(p.command, "NICK"))
				err = recv_nick(errbuff, &p, s);
			else if (!strcmp(p.command, "PING"))
				err = recv_ping(errbuff, &p, s);
			else if (!strcmp(p.command, "MODE"))
				err = recv_mode(errbuff, &p, s);
			else if (!strcmp(p.command, "ERROR"))
				err = recv_error(errbuff, &p, s);
			else
				newlinef(s->channel, 0, "-!!-", "Message type '%s' unknown", p.command);

			if (err)
				newlinef(s->channel, 0, "-!!-", "%s", errbuff);

			ptr = s->input;

		/* Don't accept unprintable characters unless space or ctcp markup */
		} else if (ptr < max && (isgraph(*inp) || *inp == ' ' || *inp == 0x01))
			*ptr++ = *inp;

		inp++;
	}

	s->iptr = ptr;
}

static int
recv_ctcp_req(char *err, parsed_mesg *p, server *s)
{
	/* CTCP Requests:
	 * PRIVMSG <target> :0x01<command> <arguments>0x01 */

	char *targ, *cmd, *mesg;
	channel *c;

	if (!p->from)
		fail("CTCP: sender's nick is null");

	if (!(targ = strtok(p->params, " ")))
		fail("CTCP: target is null");

	if (!(mesg = strtok(p->trailing, "\x01")))
		fail("CTCP: invalid markup");

	/* Markup is valid, get command */
	if (!(cmd = strtok_r(mesg, " ", &mesg)))
		fail("CTCP: command is null");

	if (!strcmp(cmd, "ACTION")) {
		/* ACTION <message> */

		if (IS_ME(targ)) {
			/* Sending emote to private channel */

			if ((c = channel_get(p->from, s)) == NULL) {
				c = new_channel(p->from, s, s->channel);
				c->type = 'p';
			}

			if (c != ccur)
				c->active = ACTIVITY_PINGED;

		} else if ((c = channel_get(targ, s)) == NULL)
			failf("CTCP ACTION: channel '%s' not found", targ);

		newlinef(c, 0, "*", "%s %s", p->from, mesg);

		return 0;
	}

	if (!strcmp(cmd, "VERSION")) {
		/* VERSION */

		if ((c = channel_get(p->from, s)) == NULL)
			c = s->channel;

		newlinef(c, 0, "--", "CTCP VERSION request from %s", p->from);

		fail_if(sendf(err, s, "NOTICE %s :\x01""VERSION rirc version "VERSION"\x01", p->from));
		fail_if(sendf(err, s, "NOTICE %s :\x01""VERSION http://rcr.io/rirc.html\x01", p->from));

		return 0;
	}

	fail_if(sendf(err, s, "NOTICE %s :\x01""ERRMSG %s not supported\x01", p->from, cmd));

	failf("CTCP: Unknown command '%s' from %s", cmd, p->from);
}

static int
recv_ctcp_rpl(char *err, parsed_mesg *p)
{
	/* CTCP replies:
	 * NOTICE <target> :0x01<command> <arguments>0x01 */

	char *cmd, *mesg;

	if (!p->from)
		fail("CTCP: sender's nick is null");

	if (!(mesg = strtok(p->trailing, "\x01")))
		fail("CTCP: invalid markup");

	/* Markup is valid, get command */
	if (!(cmd = strtok_r(mesg, " ", &mesg)))
		fail("CTCP: command is null");

	newlinef(ccur, 0, "--", "CTCP %s reply from %s", cmd, p->from);
	newlinef(ccur, 0, "--", "%s", mesg);

	return 0;
}

static int
recv_error(char *err, parsed_mesg *p, server *s)
{
	/* ERROR :<message> */

	UNUSED(err);

	server_disconnect(s, 1, 0, p->trailing ? p->trailing : "Remote hangup");

	return 0;
}

static int
recv_join(char *err, parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain JOIN [:]<channel> */

	char *chan;
	channel *c;

	if (!p->from)
		fail("JOIN: sender's nick is null");

	/* FIXME:
	 *
	 * the channel name can come as the param or trailing... when its in the trailing
	 * we might try strtok(p->params, " ") first BUT this is NULL so strtok is repeating
	 * it's previous search!
	 *
	 * so clearly we do need some sort of strsep like function that simply returns NULL
	 * if input is NULL and doesn't try to continue parsing whatever it happens to point to
	 *
	 * */
	/* FIXME: temporary fix for the above issue */
	if (!p->params)
		p->params = p->trailing;
	if (!(chan = strtok(p->params, " ")))
		fail("JOIN: channel is null");

	if (IS_ME(p->from)) {
		if ((c = channel_get(chan, s)) == NULL)
			ccur = new_channel(chan, s, ccur);
		else {
			c->parted = 0;
			newlinef(c, 0, ">", "You have rejoined %s", chan);
		}
		draw(D_FULL);
	} else {

		if ((c = channel_get(chan, s)) == NULL)
			failf("JOIN: channel '%s' not found", chan);

		if (!nicklist_insert(&(c->nicklist), p->from))
			failf("JOIN: nick '%s' already in '%s'", p->from, chan);

		c->nick_count++;

		if (c->nick_count < config.join_part_quit_threshold)
			newlinef(c, 0, ">", "%s/%s has joined %s", p->from, p->hostinfo, chan);

		draw(D_STATUS);
	}

	return 0;
}

static int
recv_mode(char *err, parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain MODE <targ> :<flags> */

	int modebit;
	char *targ, *flags, plusminus = '\0';

	if (!p->from)
		fail("MODE: sender's nick is null");

	if (!(targ = strtok(p->params, " ")))
		fail("MODE: target is null");

	/* FIXME: is this true?? why do i even get mode message then? */
	/* Flags can be null */
	if (!(flags = p->trailing))
		return 0;

	channel *c;
	if ((c = channel_get(targ, s))) {

		int *chanmode = &c->chanmode;

		newlinef(c, 0, "--", "%s set %s mode: [%s]", p->from, targ, flags);

		/* Chanmodes */
		do {
			switch (*flags) {
				case '+':
				case '-':
					plusminus = *flags;
					continue;
				case 'O':
					modebit = CMODE_O;
					break;
				case 'o':
					modebit = CMODE_o;
					break;
				case 'v':
					modebit = CMODE_v;
					break;
				case 'a':
					modebit = CMODE_a;
					break;
				case 'i':
					modebit = CMODE_i;
					break;
				case 'm':
					modebit = CMODE_m;
					break;
				case 'n':
					modebit = CMODE_n;
					break;
				case 'q':
					modebit = CMODE_q;
					break;
				case 'p':
					modebit = CMODE_p;
					break;
				case 's':
					modebit = CMODE_s;
					break;
				case 'r':
					modebit = CMODE_r;
					break;
				case 't':
					modebit = CMODE_t;
					break;
				case 'k':
					modebit = CMODE_k;
					break;
				case 'l':
					modebit = CMODE_l;
					break;
				case 'b':
					modebit = CMODE_b;
					break;
				case 'e':
					modebit = CMODE_e;
					break;
				case 'I':
					modebit = CMODE_I;
					break;
				default:
					newlinef(s->channel, 0, "-!!-", "Unknown mode '%c'", *flags);
					continue;
			}

			if (plusminus == '\0')
				failf("MODE: invalid format (%s)", p->trailing);

			if (plusminus == '+')
				*chanmode |= modebit;
			else
				*chanmode &= ~modebit;

		} while (*(++flags) != '\0');
	}

	if (IS_ME(targ)) {

		int *usermode = &s->usermode;

		newlinef(s->channel, 0, "--", "%s mode: [%s]", targ, flags);

		/* Usermodes */
		do {
			switch (*flags) {
				case '+':
				case '-':
					plusminus = *flags;
					continue;
				case 'a':
					modebit = UMODE_a;
					break;
				case 'i':
					modebit = UMODE_i;
					break;
				case 'w':
					modebit = UMODE_w;
					break;
				case 'r':
					modebit = UMODE_r;
					break;
				case 'o':
					modebit = UMODE_o;
					break;
				case 'O':
					modebit = UMODE_O;
					break;
				case 's':
					modebit = UMODE_s;
					break;
				default:
					newlinef(s->channel, 0, "-!!-", "Unknown mode '%c'", *flags);
					continue;
			}

			if (plusminus == '\0')
				failf("MODE: invalid format (%s)", p->trailing);

			if (plusminus == '+')
				*usermode |= modebit;
			else
				*usermode &= ~modebit;

		} while (*(++flags) != '\0');
	} else {
		/* TODO: Usermode for other users */
		newlinef(s->channel, 0, "--", "%s mode: [%s]", targ, flags);
	}

	return 0;
}

static int
recv_nick(char *err, parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain NICK [:]<new nick> */

	char *nick;

	if (!p->from)
		fail("NICK: old nick is null");

	/* Some servers seem to send the new nick in the trailing */
	/* FIXME: temporary fix for the issue strtok issue */
	if (!p->params)
		p->params = p->trailing;
	if (!(nick = strtok(p->params, " ")))
		fail("NICK: new nick is null");

	if (IS_ME(p->from)) {
		strncpy(s->nick_me, nick, NICKSIZE-1);
		newlinef(s->channel, 0, "--", "You are now known as %s", nick);
	}

	channel *c = s->channel;
	do {
		if (nicklist_delete(&c->nicklist, p->from)) {
			nicklist_insert(&c->nicklist, nick);
			newlinef(c, 0, "--", "%s  >>  %s", p->from, nick);
		}
	} while ((c = c->next) != s->channel);

	return 0;
}

static int
recv_notice(char *err, parsed_mesg *p, server *s)
{
	/* :nick.hostname.domain NOTICE <target> :<message> */

	char *targ;
	channel *c;

	if (!p->trailing)
		fail("NOTICE: message is null");

	/* CTCP reply */
	if (*p->trailing == 0x01)
		return recv_ctcp_rpl(err, p);

	if (!p->from)
		fail("NOTICE: sender's nick is null");

	if (!(targ = strtok(p->params, " ")))
		fail("NOTICE: target is null");

	if ((c = channel_get(targ, s)))
		newline(c, 0, p->from, p->trailing);
	else
		newline(s->channel, 0, p->from, p->trailing);

	return 0;
}

static int
recv_numeric(char *err, parsed_mesg *p, server *s)
{
	/* :server <numeric> <target> [args] */

	channel *c;
	char *nick, *chan, *time, *type, *num;

	/* Target should be s->nick_me, or '*' if unregistered.
	 * Currently not used for anything */
	if (!(nick = strtok_r(p->params, " ", &p->params)))
		fail("NUMERIC: target is null");

	/* Extract numeric code */
	int code = 0;
	do {
		code = code * 10 + (*p->command++ - '0');

		if (code > 999)
			fail("NUMERIC: greater than 999");

	} while (isdigit(*p->command));

	/* Shortcuts */
	if (!code)
		fail("NUMERIC: code is null");
	else if (code > 400) goto num_400;
	else if (code > 200) goto num_200;

	/* Numeric types (000, 200) */
	switch (code) {

	/* 001 <nick> :<Welcome message> */
	case RPL_WELCOME:

		/* Reset list of auto nicks */
		s->nptr = config.nicks;

		if (config.auto_join) {
			/* Only send the autojoin on command-line connect */
			fail_if(sendf(err, s, "JOIN %s", config.auto_join));
			config.auto_join = NULL;
		} else {
			/* If reconnecting to server, join any non-parted channels */
			c = s->channel;
			do {
				if (c->type && c->type != 'p' && !c->parted)
					fail_if(sendf(err, s, "JOIN %s", c->name));
				c = c->next;
			} while (c != s->channel);
		}

		newline(s->channel, 0, "--", p->trailing);
		return 0;


	case RPL_YOURHOST:  /* 002 <nick> :<Host info, server version, etc> */
	case RPL_CREATED:   /* 003 <nick> :<Server creation date message> */

		newline(s->channel, 0, "--", p->trailing);
		return 0;


	case RPL_MYINFO:    /* 004 <nick> <params> :Are supported by this server */
	case RPL_ISUPPORT:  /* 005 <nick> <params> :Are supported by this server */

		newlinef(s->channel, 0, "--", "%s ~ supported by this server", p->params);
		return 0;


	default:

		newlinef(s->channel, 0, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return 0;
	}

num_200:

	/* Numeric types (200, 400) */
	switch (code) {

	/* 328 <channel> :<url> */
	case RPL_CHANNEL_URL:

		if (!(chan = strtok_r(p->params, " ", &p->params)))
			fail("RPL_CHANNEL_URL: channel is null");

		if ((c = channel_get(chan, s)) == NULL)
			failf("RPL_CHANNEL_URL: channel '%s' not found", chan);

		newlinef(c, 0, "--", "URL for %s is: \"%s\"", chan, p->trailing);
		return 0;


	/* 332 <channel> :<topic> */
	case RPL_TOPIC:

		if (!(chan = strtok_r(p->params, " ", &p->params)))
			fail("RPL_TOPIC: channel is null");

		if ((c = channel_get(chan, s)) == NULL)
			failf("RPL_TOPIC: channel '%s' not found", chan);

		newlinef(c, 0, "--", "Topic for %s is \"%s\"", chan, p->trailing);
		return 0;


	/* 333 <channel> <nick> <time> */
	case RPL_TOPICWHOTIME:

		if (!(chan = strtok_r(p->params, " ", &p->params)))
			fail("RPL_TOPICWHOTIME: channel is null");

		if (!(nick = strtok_r(p->params, " ", &p->params)))
			fail("RPL_TOPICWHOTIME: nick is null");

		if (!(time = strtok_r(p->params, " ", &p->params)))
			fail("RPL_TOPICWHOTIME: time is null");

		if ((c = channel_get(chan, s)) == NULL)
			failf("RPL_TOPICWHOTIME: channel '%s' not found", chan);

		time_t raw_time = atoi(time);
		time = ctime(&raw_time);

		newlinef(c, 0, "--", "Topic set by %s, %s", nick, time);
		return 0;


	/* 353 ("="/"*"/"@") <channel> :*([ "@" / "+" ]<nick>) */
	case RPL_NAMREPLY:

		/* @:secret   *:private   =:public */
		if (!(type = strtok_r(p->params, " ", &p->params)))
			fail("RPL_NAMEREPLY: type is null");

		if (!(chan = strtok_r(p->params, " ", &p->params)))
			fail("RPL_NAMEREPLY: channel is null");

		if ((c = channel_get(chan, s)) == NULL)
			failf("RPL_NAMEREPLY: channel '%s' not found", chan);

		c->type = *type;

		while ((nick = strtok_r(p->trailing, " ", &p->trailing))) {
			if (*nick == '@' || *nick == '+')
				nick++;
			if (nicklist_insert(&c->nicklist, nick))
				c->nick_count++;
		}

		draw(D_STATUS);
		return 0;


	case RPL_STATSCONN:    /* 250 :<Message> */
	case RPL_LUSERCLIENT:  /* 251 :<Message> */

		newline(s->channel, 0, "--", p->trailing);
		return 0;


	case RPL_LUSEROP:        /* 252 <int> :IRC Operators online */
	case RPL_LUSERUNKNOWN:   /* 253 <int> :Unknown connections */
	case RPL_LUSERCHANNELS:  /* 254 <int> :Channels formed */

		if (!(num = strtok_r(p->params, " ", &p->params)))
			num = "NULL";

		newlinef(s->channel, 0, "--", "%s %s", num, p->trailing);
		return 0;


	case RPL_LUSERME:      /* 255 :I have <int> clients and <int> servers */
	case RPL_LOCALUSERS:   /* 265 <int> <int> :Local users <int>, max <int> */
	case RPL_GLOBALUSERS:  /* 266 <int> <int> :Global users <int>, max <int> */
	case RPL_MOTD:         /* 372 : - <Message> */
	case RPL_MOTDSTART:    /* 375 :<server> Message of the day */

		newline(s->channel, 0, "--", p->trailing);
		return 0;


	/* Not printing these */
	case RPL_NOTOPIC:     /* 331 <chan> :<Message> */
	case RPL_ENDOFNAMES:  /* 366 <chan> :<Message> */
	case RPL_ENDOFMOTD:   /* 376 :<Message> */

		return 0;


	default:

		newlinef(s->channel, 0, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return 0;
	}

num_400:

	/* Numeric types (400, 600) */
	switch (code) {

	case ERR_CANNOTSENDTOCHAN:  /* <channel> :<reason> */

		if (!(chan = strtok_r(p->params, " ", &p->params)))
			fail("ERR_CANNOTSENDTOCHAN: channel is null");

		channel *c;

		if ((c = channel_get(chan, s)))
			newline(c, 0, 0, p->trailing);
		else
			newline(s->channel, 0, 0, p->trailing);

		if (p->trailing)
			newlinef(c, 0, "--", "Cannot send to '%s' - %s", chan, p->trailing);
		else
			newlinef(c, 0, "--", "Cannot send to '%s'", chan);
		return 0;


	case ERR_ERRONEUSNICKNAME:  /* 432 <nick> :<reason> */

		if (!(nick = strtok_r(p->params, " ", &p->params)))
			fail("ERR_ERRONEUSNICKNAME: nick is null");

		newlinef(s->channel, 0, "-!!-", "'%s' - %s", nick, p->trailing);
		return 0;

	case ERR_NICKNAMEINUSE:  /* 433 <nick> :Nickname is already in use */

		if (!(nick = strtok(p->params, " ")))
			fail("ERR_NICKNAMEINUSE: nick is null");

		newlinef(s->channel, 0, "-!!-", "Nick '%s' in use", nick);

		if (IS_ME(nick)) {
			auto_nick(&(s->nptr), s->nick_me);

			newlinef(s->channel, 0, "-!!-", "Trying again with '%s'", s->nick_me);

			return sendf(err, s, "NICK %s", s->nick_me);
		}
		return 0;


	default:

		newlinef(s->channel, 0, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return 0;
	}

	return 0;
}

static int
recv_part(char *err, parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain PART <channel> [:message] */

	char *targ;
	channel *c;

	if (!p->from)
		fail("PART: sender's nick is null");

	if (!(targ = strtok_r(p->params, " ", &p->params)))
		fail("PART: target is null");

	if (IS_ME(p->from)) {

		/* If receving a PART message from myself channel isn't found, assume it was closed */
		if ((c = channel_get(targ, s)) != NULL) {

			c->parted = 1;
			c->chanmode = 0;
			c->nick_count = 0;

			free_nicklist(c->nicklist);
			c->nicklist = NULL;

			if (p->trailing)
				newlinef(c, 0, "<", "you have left %s (%s)", targ, p->trailing);
			else
				newlinef(c, 0, "<", "you have left %s", targ);
		}

		draw(D_STATUS);

		return 0;
	}

	if ((c = channel_get(targ, s)) == NULL)
		failf("PART: channel '%s' not found", targ);

	if (!nicklist_delete(&c->nicklist, p->from))
		failf("PART: nick '%s' not found in '%s'", p->from, targ);

	c->nick_count--;

	if (c->nick_count < config.join_part_quit_threshold) {
		if (p->trailing)
			newlinef(c, 0, "<", "%s/%s has left %s (%s)", p->from, p->hostinfo, targ, p->trailing);
		else
			newlinef(c, 0, "<", "%s/%s has left %s", p->from, p->hostinfo, targ);
	}

	draw(D_STATUS);

	return 0;
}

static int
recv_ping(char *err, parsed_mesg *p, server *s)
{
	/* PING :<server> */

	if (!p->trailing)
		fail("PING: server is null");

	return sendf(err, s, "PONG %s", p->trailing);
}

static int
recv_priv(char *err, parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain PRIVMSG <target> :<message> */

	char *targ;
	channel *c;

	if (!p->trailing)
		fail("PRIVMSG: message is null");

	/* CTCP request */
	if (*p->trailing == 0x01)
		return recv_ctcp_req(err, p, s);

	if (!p->from)
		fail("PRIVMSG: sender's nick is null");

	if (!(targ = strtok_r(p->params, " ", &p->params)))
		fail("PRIVMSG: target is null");

	/* Find the target channel */
	if (IS_ME(targ)) {

		if ((c = channel_get(p->from, s)) == NULL) {
			c = new_channel(p->from, s, s->channel);
			c->type = 'p';
		}

		if (c != ccur)
			c->active = ACTIVITY_PINGED;

	} else if ((c = channel_get(targ, s)) == NULL)
		failf("PRIVMSG: channel '%s' not found", targ);

	if (check_pinged(p->trailing, s->nick_me)) {

		if (c != ccur)
			c->active = ACTIVITY_PINGED;

		newline(c, LINE_PINGED, p->from, p->trailing);
	} else
		newline(c, LINE_CHAT, p->from, p->trailing);

	return 0;
}

static int
recv_quit(char *err, parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain QUIT [:message] */

	if (!p->from)
		fail("QUIT: sender's nick is null");

	channel *c = s->channel;
	do {
		if (nicklist_delete(&c->nicklist, p->from)) {
			c->nick_count--;
			if (c->nick_count < config.join_part_quit_threshold) {
				if (p->trailing)
					newlinef(c, 0, "<", "%s/%s has quit (%s)", p->from, p->hostinfo, p->trailing);
				else
					newlinef(c, 0, "<", "%s/%s has quit", p->from, p->hostinfo);
			}
		}
		c = c->next;
	} while (c != s->channel);

	draw(D_STATUS);

	return 0;
}
