#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "common.h"
#include "state.h"

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
#define failf(...) \
	do { if (err) { snprintf(err, MAX_ERROR, __VA_ARGS__); } return 1; } while (0)

/* Conditionally fail */
#define fail_if(C) \
	do { if (C) return 1; } while (0)

#define IS_ME(X) !strcmp(X, s->nick)

/* List of common IRC commands with no explicit handling */
#define UNHANDLED_SEND_CMDS \
	X(admin)   X(away)     X(die) \
	X(encap)   X(help)     X(info) \
	X(invite)  X(ison)     X(kick) \
	X(kill)    X(knock)    X(links) \
	X(list)    X(lusers)   X(mode) \
	X(motd)    X(names)    X(namesx) \
	X(notice)  X(oper)     X(pass) \
	X(rehash)  X(restart)  X(rules) \
	X(server)  X(service)  X(servlist) \
	X(setname) X(silence)  X(squery) \
	X(squit)   X(stats)    X(summon) \
	X(time)    X(trace)    X(uhnames) \
	X(user)    X(userhost) X(userip) \
	X(users)   X(wallops)  X(watch) \
	X(who)     X(whois)    X(whowas)

/* List of commands (some rirc-specific) which are explicitly handled */
#define HANDLED_SEND_CMDS \
	X(clear) \
	X(close) \
	X(connect) \
	X(ctcp) \
	X(disconnect) \
	X(ignore) \
	X(join) \
	X(me) \
	X(msg) \
	X(nick) \
	X(part) \
	X(privmsg) \
	X(quit) \
	X(raw) \
	X(topic) \
	X(unignore) \
	X(version)

/* Function prototypes for explicitly handled commands */
#define X(cmd) static int send_##cmd(char*, char*, channel*);
HANDLED_SEND_CMDS
#undef X

/* extern in common.h */
avl_node* commands;

/* Handler for errors deemed fatal to a server's state */
static void server_fatal(server*, char*, ...);

/* Special case handler for sending non-command input */
static int send_default(char*, char*, channel*);

/* Default case handler for sending commands */
static int send_unhandled(char*, char*, char*, channel*);

/* Encapsulate a function pointer in a struct so AVL tree cleanup can free it */
struct command { int (*fptr)(char*, char*, channel*); };
static struct command* new_command(int (*fptr)(char*, char*, channel*));

//TODO: mimic the send handler macros and build/free a tree of handlers
/* Message receiving handlers */
static int recv_ctcp_req(char*, parsed_mesg*, server*);
static int recv_ctcp_rpl(char*, parsed_mesg*);
static int recv_error(char*, parsed_mesg*, server*);
static int recv_join(char*, parsed_mesg*, server*);
static int recv_kick(char*, parsed_mesg*, server*);
static int recv_mode(char*, parsed_mesg*, server*);
static int recv_nick(char*, parsed_mesg*, server*);
static int recv_notice(char*, parsed_mesg*, server*);
static int recv_numeric(char*, parsed_mesg*, server*);
static int recv_part(char*, parsed_mesg*, server*);
static int recv_ping(char*, parsed_mesg*, server*);
static int recv_pong(char*, parsed_mesg*, server*);
static int recv_priv(char*, parsed_mesg*, server*);
static int recv_quit(char*, parsed_mesg*, server*);
static int recv_topic(char*, parsed_mesg*, server*);

static void
server_fatal(server *s, char *fmt, ...)
{
	/* Encountered an error fatal to a server, disconnect and begin a reconnect */
	char errbuff[MAX_ERROR];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(errbuff, MAX_ERROR, fmt, ap);
	va_end(ap);

	server_disconnect(s, 1, 0, errbuff);
}

void
init_mesg(void)
{
	/* Build and AVL tree of commands and function pointers to handlers */

	/* Add the unhandled commands with no explicit handler */
	#define X(cmd) avl_add(&commands, #cmd, NULL);
	UNHANDLED_SEND_CMDS
	#undef X

	/* Add the handled commands with explicit handlers */
	#define X(cmd) avl_add(&commands, #cmd, new_command(send_##cmd));
	HANDLED_SEND_CMDS
	#undef X
}

void
free_mesg(void)
{
	free_avl(commands);
}

static struct command*
new_command(int (*fptr)(char*, char*, channel *c))
{
	/* Allocate a command handler */

	struct command *c;

	if ((c = malloc(sizeof(struct command))) == NULL)
		fatal("malloc");

	c->fptr = fptr;

	return c;
}

/*
 * Message sending handlers
 * */

void
send_mesg(char *mesg, channel *chan)
{
	/* Handle the input to a channel, ie:
	 *	- a default message to the channel
	 *	- a default message to the channel beginning with '/'
	 *	- a handled command beginning with '/'
	 *	- an unhandled command beginning with '/'
	 */

	char *cmd_str, errbuff[MAX_ERROR];
	const avl_node *cmd;
	int err = 0;

	if (*mesg == '/') {

		mesg++;

		if (*mesg == '/')
			err = send_default(errbuff, mesg, chan);

		else if (!(cmd_str = getarg(&mesg, " ")))
			newline(chan, 0, "-!!-", "Messages beginning with '/' require a command");

		else if (!(cmd = avl_get(commands, cmd_str, strlen(cmd_str))))
			newlinef(chan, 0, "-!!-", "Unknown command: '%s'", cmd_str);

		else {
			struct command *c = (struct command*)(cmd->val);

			if (c)
				err = c->fptr(errbuff, mesg, chan);
			else
				err = send_unhandled(errbuff, cmd_str, mesg, chan);
		}
	} else {
		err = send_default(errbuff, mesg, chan);
	}

	if (err)
		newline(chan, 0, "-!!-", errbuff);
}

//TODO: move this function to state?
void
send_paste(char *paste)
{
	/* TODO: send the paste buffer, which is preformatted with \r\n, and then
	 * split the messages and newline them into the buffer * */
	UNUSED(paste);
}

static int
send_unhandled(char *err, char *cmd, char *args, channel *c)
{
	/* All commands defined in the UNHANDLED_CMDS */

	char *ptr;

	/* command -> COMMAND */
	for (ptr = cmd; *ptr; ptr++)
		*ptr = toupper(*ptr);

	return sendf(err, c->server, "%s %s", cmd, args);
}

static int
send_clear(char *err, char *mesg, channel *c)
{
	/* /clear [channel] */

	char *targ;
	channel *cc;

	if (!(targ = getarg(&mesg, " ")))
		channel_clear(c);
	else if ((cc = channel_get(targ, c->server)))
		channel_clear(cc);
	else
		failf("Error: Channel '%s' not found", targ);

	return 0;
}

static int
send_close(char *err, char *mesg, channel *c)
{
	/* /close [channel] */

	char *targ;
	channel *cc;

	if (!(targ = getarg(&mesg, " ")))
		channel_close(c);
	else if ((cc = channel_get(targ, c->server)))
		channel_close(cc);
	else
		failf("Error: Channel '%s' not found", targ);

	return 0;
}

static int
send_connect(char *err, char *mesg, channel *c)
{
	/* /connect [(host) | (host:port) | (host port)] */

	char *host, *port;

	if (!(host = getarg(&mesg, " :"))) {

		/* If no hostname arg is given, attempt to reconnect on the current server */

		if (!c->server)
			fail("Error: /connect <host | host:port | host port>");

		else if (c->server->soc >= 0 || c->server->connecting)
			fail("Error: Already connected or reconnecting to server");

		host = c->server->host;
		port = c->server->port;

	} else if (!(port = getarg(&mesg, " "))) {
		port = "6667";
	}

	server_connect(host, port);

	return 0;
}

static int
send_ctcp(char *err, char *mesg, channel *c)
{
	/* /ctcp <target> <message> */

	char *targ, *p;

	if (!(targ = getarg(&mesg, " ")))
		fail("Error: /ctcp <target> <command> [arguments]");

	/* Crude to check that at least some ctcp command exists */
	while (*mesg == ' ')
		mesg++;

	if (*mesg == '\0')
		fail("Error: /ctcp <target> <command> [arguments]");

	/* Ensure the command is uppercase */
	for (p = mesg; *p && *p != ' '; p++)
		*p = toupper(*p);

	return sendf(err, c->server, "PRIVMSG %s :\x01""%s\x01", targ, mesg);
}

static int
send_default(char *err, char *mesg, channel *c)
{
	/* All messages not beginning with '/'  */

	if (c->buffer_type == BUFFER_SERVER)
		fail("Error: This is not a channel");

	if (c->parted)
		fail("Error: Parted from channel");

	fail_if(sendf(err, c->server, "PRIVMSG %s :%s", c->name, mesg));

	newline(c, LINE_CHAT, c->server->nick, mesg);

	return 0;
}

static int
send_disconnect(char *err, char *mesg, channel *c)
{
	/* /disconnect [quit message] */

	server *s = c->server;

	/* Server isn't connecting, connected or waiting to connect */
	if (!s || (!s->connecting && s->soc < 0 && !s->reconnect_time))
		fail("Error: Not connected to server");

	server_disconnect(c->server, 0, 0, (*mesg) ? mesg : DEFAULT_QUIT_MESG);

	return 0;
}

static int
send_me(char *err, char *mesg, channel *c)
{
	/* /me <message> */

	if (c->buffer_type == BUFFER_SERVER)
		fail("Error: This is not a channel");

	if (c->parted)
		fail("Error: Parted from channel");

	fail_if(sendf(err, c->server, "PRIVMSG %s :\x01""ACTION %s\x01", c->name, mesg));

	newlinef(c, 0, "*", "%s %s", c->server->nick, mesg);

	return 0;
}

static int
send_ignore(char *err, char *mesg, channel *c)
{
	/* /ignore [nick] */

	char *nick;

	if (!c->server)
		fail("Error: Not connected to server");

	if (!(nick = getarg(&mesg, " ")))
		nicklist_print(c);

	else if (!avl_add(&(c->server->ignore), nick, NULL))
		failf("Error: Already ignoring '%s'", nick);

	else
		newlinef(c, 0, "--", "Ignoring '%s'", nick);

	return 0;
}

static int
send_join(char *err, char *mesg, channel *c)
{
	/* /join [target[,targets]*] */

	char *targ;

	if ((targ = getarg(&mesg, " ")))
		return sendf(err, c->server, "JOIN %s", targ);

	if (c->buffer_type == BUFFER_SERVER)
		fail("Error: JOIN requires a target");

	if (c->buffer_type == BUFFER_PRIVATE)
		fail("Error: Can't rejoin private buffers");

	if (!c->parted)
		fail("Error: Not parted from channel");

	return sendf(err, c->server, "JOIN %s", c->name);
}

static int
send_msg(char *err, char *mesg, channel *c)
{
	/* Alias for /priv */

	return send_privmsg(err, mesg, c);
}

static int
send_nick(char *err, char *mesg, channel *c)
{
	/* /nick [nick] */

	char *nick;

	if ((nick = getarg(&mesg, " ")))
		return sendf(err, c->server, "NICK %s", nick);

	if (!c->server)
		fail("Error: Not connected to server");

	newlinef(c, 0, "--", "Your nick is %s", c->server->nick);

	return 0;
}

static int
send_part(char *err, char *mesg, channel *c)
{
	/* /part [[target[,targets]*] part message]*/

	char *targ;

	if ((targ = getarg(&mesg, " ")))
		return sendf(err, c->server, "PART %s :%s", targ, (*mesg) ? mesg : DEFAULT_QUIT_MESG);

	if (c->buffer_type == BUFFER_SERVER)
		fail("Error: PART requires a target");

	if (c->buffer_type == BUFFER_PRIVATE)
		fail("Error: Can't part private buffers");

	if (c->parted)
		fail("Error: Already parted from channel");

	return sendf(err, c->server, "PART %s :%s", c->name, DEFAULT_QUIT_MESG);
}

static int
send_privmsg(char *err, char *mesg, channel *c)
{
	/* /(priv | msg) <target> <message> */

	char *targ;
	channel *cc;

	if (!(targ = getarg(&mesg, " ")))
		fail("Error: Private messages require a target");

	if (*mesg == '\0')
		fail("Error: Private messages was null");

	fail_if(sendf(err, c->server, "PRIVMSG %s :%s", targ, mesg));

	/* FIXME: wait.... cc? does newline go to cc then not c? is this true elsewhere?*/
	if ((cc = channel_get(targ, c->server)) == NULL)
		cc = new_channel(targ, c->server, c, BUFFER_PRIVATE);

	newline(c, LINE_CHAT, c->server->nick, mesg);

	return 0;
}

static int
send_raw(char *err, char *mesg, channel *c)
{
	/* /raw <raw message> */

	fail_if(sendf(err, c->server, "%s", mesg));

	newline(c, 0, "RAW >>", mesg);

	return 0;
}

static int
send_topic(char *err, char *mesg, channel *c)
{
	/* /topic [topic] */

	/* If no actual message is given, retrieve the current topic */
	while (*mesg == ' ')
		mesg++;

	if (*mesg == '\0')
		return sendf(err, c->server, "TOPIC %s", c->name);

	return sendf(err, c->server, "TOPIC %s :%s", c->name, mesg);
}

static int
send_unignore(char *err, char *mesg, channel *c)
{
	/* /unignore [nick] */

	char *nick;

	if (!c->server)
		fail("Error: Not connected to server");

	if (!(nick = getarg(&mesg, " ")))
		nicklist_print(c);

	else if (!avl_del(&(c->server->ignore), nick))
		failf("Error: '%s' not on ignore list", nick);

	else
		newlinef(c, 0, "--", "No longer ignoring '%s'", nick);

	return 0;
}

static int
send_quit(char *err, char *mesg, channel *c)
{
	/* /quit [quit message] */

	UNUSED(err);

	server *t, *s = c->server;

	if (s) do {
		t = s;
		s = s->next;
		server_disconnect(t, 0, 1, (*mesg) ? mesg : DEFAULT_QUIT_MESG);
	} while (t != s);

	exit(EXIT_SUCCESS);

	return 0;
}

static int
send_version(char *err, char *mesg, channel *c)
{
	/* /version [target] */

	char *targ;

	if (c->server == NULL) {
		newline(c, 0, "--", "rirc v"VERSION);
		newline(c, 0, "--", "http://rcr.io/rirc.html");

		return 0;
	}

	if ((targ = getarg(&mesg, " ")))
		return sendf(err, c->server, "VERSION %s", targ);
	else
		return sendf(err, c->server, "VERSION");
}

/*
 * Message receiving handlers
 * */

/* FIXME: lots of incorrect instances of ccur below */

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
			else if (!strcmp(p.command, "PONG"))
				err = recv_pong(errbuff, &p, s);
			else if (!strcmp(p.command, "KICK"))
				err = recv_kick(errbuff, &p, s);
			else if (!strcmp(p.command, "MODE"))
				err = recv_mode(errbuff, &p, s);
			else if (!strcmp(p.command, "ERROR"))
				err = recv_error(errbuff, &p, s);
			else if (!strcmp(p.command, "TOPIC"))
				err = recv_topic(errbuff, &p, s);
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
	 * PRIVMSG <target> :0x01<command> <arguments>0x01
	 *
	 * All replies must be:
	 * NOTICE <target> :0x01<reply>0x01 */

	char *targ, *cmd, *mesg;

	if (!p->from)
		fail("CTCP: sender's nick is null");

	/* CTCP request from ignored user, do nothing */
	if (avl_get(ccur->server->ignore, p->from, strlen(p->from)))
		return 0;

	if (!(targ = getarg(&p->params, " ")))
		fail("CTCP: target is null");

	if (!(mesg = getarg(&p->trailing, "\x01")))
		fail("CTCP: invalid markup");

	/* Markup is valid, get command */
	if (!(cmd = getarg(&mesg, " ")))
		fail("CTCP: command is null");

	/* Handle the CTCP request if supported */

	if (!strcmp(cmd, "ACTION")) {
		/* ACTION <message> */

		channel *c;

		if (IS_ME(targ)) {
			/* Sending emote to private channel */

			if ((c = channel_get(p->from, s)) == NULL)
				c = new_channel(p->from, s, s->channel, BUFFER_PRIVATE);

			if (c != ccur)
				c->active = ACTIVITY_PINGED;

		} else if ((c = channel_get(targ, s)) == NULL)
			failf("CTCP ACTION: channel '%s' not found", targ);

		newlinef(c, 0, "*", "%s %s", p->from, mesg);

		return 0;
	}

	if (!strcmp(cmd, "CLIENTINFO")) {
		/* CLIENTINFO
		 *
		 * Returns a list of CTCP commands supported by rirc */

		newlinef(s->channel, 0, "--", "CTCP CLIENTINFO request from %s", p->from);

		return sendf(err, s, "NOTICE %s :\x01""CLIENTINFO ACTION PING VERSION TIME\x01", p->from);
	}

	if (!strcmp(cmd, "PING")) {
		/* PING
		 *
		 * Returns a millisecond precision timestamp */

		struct timeval t;

		gettimeofday(&t, NULL);

		long long milliseconds = t.tv_sec * 1000LL + t.tv_usec;

		newlinef(s->channel, 0, "--", "CTCP PING request from %s", p->from);

		return sendf(err, s, "NOTICE %s :\x01""PING %lld\x01", p->from, milliseconds);
	}

	if (!strcmp(cmd, "VERSION")) {
		/* VERSION
		 *
		 * Returns version info about rirc */

		newlinef(s->channel, 0, "--", "CTCP VERSION request from %s", p->from);

		return sendf(err, s,
			"NOTICE %s :\x01""VERSION rirc v"VERSION", http://rcr.io/rirc.html\x01", p->from);
	}

	if (!strcmp(cmd, "TIME")) {
		/* TIME
		 *
		 * Returns the localtime in human readable form */

		char time_str[64];
		struct tm *tm;
		time_t t;

		t = time(NULL);
		tm = localtime(&t);

		/* Mon Jan 01 20:30 GMT */
		strftime(time_str, sizeof(time_str), "%a %b %d %H:%M %Z", tm);

		newlinef(s->channel, 0, "--", "CTCP TIME request from %s", p->from);

		return sendf(err, s, "NOTICE %s :\x01""TIME %s\x01", p->from, time_str);
	}

	/* Unsupported CTCP request */
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

	/* CTCP reply from ignored user, do nothing */
	if (avl_get(ccur->server->ignore, p->from, strlen(p->from)))
		return 0;

	if (!(mesg = getarg(&p->trailing, "\x01")))
		fail("CTCP: invalid markup");

	/* Markup is valid, get command */
	if (!(cmd = getarg(&mesg, " ")))
		fail("CTCP: command is null");

	newlinef(ccur, 0, p->from, "CTCP %s reply: %s", cmd, mesg);

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

	if (!(chan = getarg(&p->params, " ")) && !(chan = getarg(&p->trailing, " ")))
		fail("JOIN: channel is null");

	if (IS_ME(p->from)) {
		if ((c = channel_get(chan, s)) == NULL)
			channel_set_current(new_channel(chan, s, ccur, BUFFER_CHANNEL));
		else {
			c->parted = 0;
			newlinef(c, 0, ">", "You have rejoined %s", chan);
		}
		draw(D_FULL);
	} else {

		if ((c = channel_get(chan, s)) == NULL)
			failf("JOIN: channel '%s' not found", chan);

		if (!avl_add(&(c->nicklist), p->from, NULL))
			failf("JOIN: nick '%s' already in '%s'", p->from, chan);

		c->nick_count++;

		if (c->nick_count < config.join_part_quit_threshold)
			newlinef(c, 0, ">", "%s!%s has joined %s", p->from, p->hostinfo, chan);

		draw(D_STATUS);
	}

	return 0;
}

static int
recv_kick(char *err, parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain KICK <channel> <user> :comment */

	char *chan, *user;
	channel *c;

	if (!p->from)
		fail("KICK: sender's nick is null");

	if (!(chan = getarg(&p->params, " ")))
		fail("KICK: channel is null");

	if (!(user = getarg(&p->params, " ")))
		fail("KICK: user is null");

	if ((c = channel_get(chan, s)) == NULL)
		failf("KICK: channel '%s' not found", chan);

	/* RFC 2812, 3.2.8:
	 *
	 * If a "comment" is given, this will be sent instead of the default message,
	 * the nickname of the user issuing the KICK.
	 * */
	if (!strcmp(p->from, p->trailing))
		p->trailing = NULL;

	if (IS_ME(user)) {

		part_channel(c);

		if (p->trailing)
			newlinef(c, 0, "--", "You've been kicked by %s (%s)", p->from, p->trailing);
		else
			newlinef(c, 0, "--", "You've been kicked by %s", p->from, user);
	} else {

		if (!avl_del(&c->nicklist, user))
			failf("KICK: nick '%s' not found in '%s'", user, chan);

		c->nick_count--;

		if (p->trailing)
			newlinef(c, 0, "--", "%s has kicked %s (%s)", p->from, user, p->trailing);
		else
			newlinef(c, 0, "--", "%s has kicked %s", p->from, user);
	}

	draw(D_STATUS);

	return 0;
}

static int
recv_mode(char *err, parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain MODE <targ> *( ( "-" / "+" ) *<modes> *<modeparams> ) */

	channel *c;
	char *targ;

	if (!(targ = getarg(&p->params, " ")))
		fail("MODE: target is null");

	/* If the target channel isn't found,  */
	if (IS_ME(targ))
		c = s->channel;
	else
		c = channel_get(targ, s);

	char *modes, *modeparams, *modetmp = NULL;

	/* FIXME: Some servers do this...
	 * MODE user :+abc
	 * MODE #chan +abc
	 *
	 * instead, getarg(parsed_mesg) should try params, then trailing
	 * */
	while ((modes = modetmp) || (modes = getarg(&p->params, " ")) || (modes = getarg(&p->trailing, " "))) {

		if (!(*modes == '+') && !(*modes == '-'))
			fail("MODE: invalid mode format");

		/* Modeparams are optional, and only used for printing when present */
		if ((modeparams = getarg(&p->params, " ")) && (*modeparams == '+' || *modeparams == '-'))
			/* modeparam here is actually a modestring, set modetmp for the next iteration to use */
			modetmp = modeparams;
		else
			modetmp = NULL;

		/* Having c set means the target is the server modes or a specific channel's modes */
		if (c) {
			if (IS_ME(targ))
				server_set_mode(s, modes);
			else
				channel_set_mode(c, modes);

			/* [<user> set ]<target> mode: [<mode>][ <modeparams>] */
			newlinef(c, 0, "--", "%s%s%s mode: [%s%s%s]",
				(p->from ? p->from : ""),
				(p->from ? " set " : ""),
				targ,
				modes,
				(modeparams ? " " : ""),
				(modeparams ? modeparams : "")
			);
		} else {

			/* If the channel isn't found, search for the target as a user in all channels
			 * and print where found */
			c = s->channel;

			do {
				if (avl_get(c->nicklist, targ, strlen(targ)))
					/* [<user> set ]<target> mode: [<mode>][ <modeparams>] */
					newlinef(c, 0, "--", "%s%s%s mode: [%s%s%s]",
						(p->from ? p->from : ""),
						(p->from ? " set " : ""),
						targ,
						modes,
						(modeparams ? " " : ""),
						(modeparams ? modeparams : "")
					);
			} while ((c = c->next) != s->channel);
		}
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
	if (!(nick = getarg(&p->params, " ")) && !(nick = getarg(&p->trailing, " ")))
		fail("NICK: new nick is null");

	if (IS_ME(p->from)) {
		strncpy(s->nick, nick, NICKSIZE);
		newlinef(s->channel, 0, "--", "You are now known as %s", nick);
	}

	channel *c = s->channel;
	do {
		if (avl_del(&c->nicklist, p->from)) {
			avl_add(&c->nicklist, nick, NULL);
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

	/* Notice from ignored user, do nothing */
	if (avl_get(ccur->server->ignore, p->from, strlen(p->from)))
		return 0;

	if (!(targ = getarg(&p->params, " ")))
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
	/* :server <code> <target> [args] */

	channel *c;
	char *targ, *nick, *chan, *time, *type, *num;
	int code;

	/* Extract numeric code */
	for (code = 0; isdigit(*p->command); p->command++) {

		code = code * 10 + (*p->command - '0');

		if (code > 999)
			fail("NUMERIC: greater than 999");
	}

	/* Message target is only used to establish s->nick when registering with a server */
	if (!(targ = getarg(&p->params, " "))) {
		server_fatal(s, "NUMERIC: target is null");
		return 1;
	}

	/* Message target should match s->nick or '*' if unregistered, otherwise out of sync */
	if (strcmp(targ, s->nick) && strcmp(targ, "*") && code != RPL_WELCOME) {
		server_fatal(s, "NUMERIC: target mismatched, nick is '%s', received '%s'", s->nick, targ);
		return 1;
	}

	/* Shortcuts */
	if (!code)
		fail("NUMERIC: code is null");
	else if (code > 400) goto num_400;
	else if (code > 200) goto num_200;

	/* Numeric types (000, 200) */
	switch (code) {

	/* 001 :<Welcome message> */
	case RPL_WELCOME:

		/* Establishing new connection with a server, set initial nick,
		 * handle any channel auto-join or rejoins */

		strncpy(s->nick, targ, NICKSIZE);

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
				if (c->buffer_type == BUFFER_CHANNEL && !c->parted)
					fail_if(sendf(err, s, "JOIN %s", c->name));
				c = c->next;
			} while (c != s->channel);
		}

		if (p->trailing)
			newline(s->channel, 0, "--", p->trailing);

		newlinef(s->channel, 0, "--", "You are known as %s", s->nick);
		return 0;


	case RPL_YOURHOST:  /* 002 :<Host info, server version, etc> */
	case RPL_CREATED:   /* 003 :<Server creation date message> */

		newline(s->channel, 0, "--", p->trailing);
		return 0;


	case RPL_MYINFO:    /* 004 <params> :Are supported by this server */
	case RPL_ISUPPORT:  /* 005 <params> :Are supported by this server */

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

		if (!(chan = getarg(&p->params, " ")))
			fail("RPL_CHANNEL_URL: channel is null");

		if ((c = channel_get(chan, s)) == NULL)
			failf("RPL_CHANNEL_URL: channel '%s' not found", chan);

		newlinef(c, 0, "--", "URL for %s is: \"%s\"", chan, p->trailing);
		return 0;


	/* 332 <channel> :<topic> */
	case RPL_TOPIC:

		if (!(chan = getarg(&p->params, " ")))
			fail("RPL_TOPIC: channel is null");

		if ((c = channel_get(chan, s)) == NULL)
			failf("RPL_TOPIC: channel '%s' not found", chan);

		newlinef(c, 0, "--", "Topic for %s is \"%s\"", chan, p->trailing);
		return 0;


	/* 333 <channel> <nick> <time> */
	case RPL_TOPICWHOTIME:

		if (!(chan = getarg(&p->params, " ")))
			fail("RPL_TOPICWHOTIME: channel is null");

		if (!(nick = getarg(&p->params, " ")))
			fail("RPL_TOPICWHOTIME: nick is null");

		if (!(time = getarg(&p->params, " ")))
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
		if (!(type = getarg(&p->params, " ")))
			fail("RPL_NAMEREPLY: type is null");

		if (!(chan = getarg(&p->params, " ")))
			fail("RPL_NAMEREPLY: channel is null");

		if ((c = channel_get(chan, s)) == NULL)
			failf("RPL_NAMEREPLY: channel '%s' not found", chan);

		c->type_flag = *type;

		while ((nick = getarg(&p->trailing, " "))) {
			if (*nick == '@' || *nick == '+')
				nick++;
			if (avl_add(&c->nicklist, nick, NULL))
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

		if (!(num = getarg(&p->params, " ")))
			num = "NULL";

		newlinef(s->channel, 0, "--", "%s %s", num, p->trailing);
		return 0;


	case RPL_LUSERME:      /* 255 :I have <int> clients and <int> servers */
	case RPL_LOCALUSERS:   /* 265 <int> <int> :Local users <int>, max <int> */
	case RPL_GLOBALUSERS:  /* 266 <int> <int> :Global users <int>, max <int> */
	case RPL_MOTD:         /* 372 :- <text> */
	case RPL_MOTDSTART:    /* 375 :- <server> Message of the day - */

		newline(s->channel, 0, "--", p->trailing);
		return 0;


	/* Not printing these */
	case RPL_NOTOPIC:     /* 331 <chan> :<Message> */
	case RPL_ENDOFNAMES:  /* 366 <chan> :<Message> */
	case RPL_ENDOFMOTD:   /* 376 :End of MOTD command */
		return 0;


	default:

		newlinef(s->channel, 0, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return 0;
	}

num_400:

	/* Numeric types (400, 600) */
	switch (code) {

	case ERR_CANNOTSENDTOCHAN:  /* <channel> :<reason> */

		if (!(chan = getarg(&p->params, " ")))
			fail("ERR_CANNOTSENDTOCHAN: channel is null");

		/* Channel buffer might not exist */
		if ((c = channel_get(chan, s)) == NULL)
			c = s->channel;

		if (p->trailing)
			newlinef(c, 0, "--", "Cannot send to '%s': %s", chan, p->trailing);
		else
			newlinef(c, 0, "--", "Cannot send to '%s'", chan);
		return 0;


	case ERR_ERRONEUSNICKNAME:  /* 432 <nick> :<reason> */

		if (!(nick = getarg(&p->params, " ")))
			fail("ERR_ERRONEUSNICKNAME: nick is null");

		newlinef(s->channel, 0, "-!!-", "'%s' - %s", nick, p->trailing);
		return 0;

	case ERR_NICKNAMEINUSE:  /* 433 <nick> :Nickname is already in use */

		if (!(nick = getarg(&p->params, " ")))
			fail("ERR_NICKNAMEINUSE: nick is null");

		newlinef(s->channel, 0, "-!!-", "Nick '%s' in use", nick);

		if (IS_ME(nick)) {
			auto_nick(&(s->nptr), s->nick);

			newlinef(s->channel, 0, "-!!-", "Trying again with '%s'", s->nick);

			return sendf(err, s, "NICK %s", s->nick);
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

	if (!(targ = getarg(&p->params, " ")))
		fail("PART: target is null");

	if (IS_ME(p->from)) {

		/* If receving a PART message from myself channel isn't found, assume it was closed */
		if ((c = channel_get(targ, s)) != NULL) {

			part_channel(c);

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

	if (!avl_del(&c->nicklist, p->from))
		failf("PART: nick '%s' not found in '%s'", p->from, targ);

	c->nick_count--;

	if (c->nick_count < config.join_part_quit_threshold) {
		if (p->trailing)
			newlinef(c, 0, "<", "%s!%s has left %s (%s)", p->from, p->hostinfo, targ, p->trailing);
		else
			newlinef(c, 0, "<", "%s!%s has left %s", p->from, p->hostinfo, targ);
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
recv_pong(char *err, parsed_mesg *p, server *s)
{
	/*  PONG <server> [<server2>] */

	UNUSED(err);

	/*  PING sent explicitly by the user */
	if (!s->pinging)
		newlinef(ccur, 0, "!!", "PONG %s", p->params);

	s->pinging = 0;

	return 0;
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

	/* Privmesg from ignored user, do nothing */
	if (avl_get(ccur->server->ignore, p->from, strlen(p->from)))
		return 0;

	if (!(targ = getarg(&p->params, " ")))
		fail("PRIVMSG: target is null");

	/* Find the target channel */
	if (IS_ME(targ)) {

		if ((c = channel_get(p->from, s)) == NULL)
			c = new_channel(p->from, s, s->channel, BUFFER_PRIVATE);

		if (c != ccur)
			c->active = ACTIVITY_PINGED;

	} else if ((c = channel_get(targ, s)) == NULL)
		failf("PRIVMSG: channel '%s' not found", targ);

	if (check_pinged(p->trailing, s->nick)) {

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
		if (avl_del(&c->nicklist, p->from)) {
			c->nick_count--;
			if (c->nick_count < config.join_part_quit_threshold) {
				if (p->trailing)
					newlinef(c, 0, "<", "%s!%s has quit (%s)", p->from, p->hostinfo, p->trailing);
				else
					newlinef(c, 0, "<", "%s!%s has quit", p->from, p->hostinfo);
			}
		}
		c = c->next;
	} while (c != s->channel);

	draw(D_STATUS);

	return 0;
}

static int
recv_topic(char *err, parsed_mesg *p, server *s)
{
	/* :nick!user@hostname.domain TOPIC <channel> :[topic] */

	channel *c;
	char *targ;

	if (!p->from)
		fail("TOPIC: sender's nick is null");

	if (!(targ = getarg(&p->params, " ")))
		fail("TOPIC: target is null");

	if (!p->trailing)
		fail("TOPIC: topic is null");

	if ((c = channel_get(targ, s)) == NULL)
		failf("TOPIC: channel '%s' not found", targ);

	if (*p->trailing) {
		newlinef(c, 0, "--", "%s has changed the topic:", p->from);
		newlinef(c, 0, "--", "\"%s\"", p->trailing);
	} else {
		newlinef(c, 0, "--", "%s has unset the topic", p->from);
	}

	return 0;
}
