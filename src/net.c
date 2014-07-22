#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>
#include <poll.h>
#include <stdarg.h>
#include <arpa/inet.h>

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
#define ERR_NICKNAMEINUSE    433
#define ERR_ERRONEUSNICKNAME 432

channel* get_channel(char*);
int get_numeric_code(char*);

char* recv_ctcp(parsed_mesg *p);
char* recv_error(parsed_mesg *p);
char* recv_join(parsed_mesg *p);
char* recv_mode(parsed_mesg *p);
char* recv_nick(parsed_mesg *p);
char* recv_notice(parsed_mesg *p);
char* recv_numeric(parsed_mesg *p);
char* recv_part(parsed_mesg *p);
char* recv_ping(parsed_mesg *p);
char* recv_priv(parsed_mesg *p);
char* recv_quit(parsed_mesg *p);

void send_conn(char*);
void send_emot(char*);
void send_join(char*);
void send_nick(char*);
void send_quit(char*);
void send_part(char*);
void send_ping(char*);
void send_priv(char*, int);

server* new_server(char*, int, int);
void con_server(char*, int);
void dis_server(server*, int);
void get_auto_nick(char**, char*);
void newline(channel*, line_t, char*, char*, int);
void newlinef(channel*, line_t, char*, char*, ...);
void sendf(int, const char*, ...);

int rplsoc = 0;
int numfds = 1; /* 1 for stdin */
extern struct pollfd fds[MAXSERVERS + 1];
/* For server indexing by socket. 3 for stdin/out/err unused */
server *s[MAXSERVERS + 3];

/* Config Stuff */
char *user_me = "rcr";
char *realname = "Richard Robbins";
/* comma separated list of channels to join on connect*/
char *autojoin = "#abc";
/* comma and/or space separated list of nicks */
char *nicks = "rcr, rcr_, rcr__";

time_t raw_t;
struct tm *t;

void
channel_switch(int next)
{
	if (ccur->next == ccur)
		return;
	else if (next)
		ccur = ccur->next;
	else
		ccur = ccur->prev;
	ccur->active = NONE;
	draw_full();
}

void
con_server(char *hostname, int port)
{
	struct hostent *host;
	struct in_addr h_addr;
	if ((host = gethostbyname(hostname)) == NULL) {
		newlinef(0, DEFAULT, "-!!-", "Error while resolving: %s", hostname);
		return;
	}

	h_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);

	struct sockaddr_in s_addr;
	if ((rplsoc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		fatal("socket");

	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sin_family = AF_INET;
	s_addr.sin_addr.s_addr = inet_addr(inet_ntoa(h_addr));
	s_addr.sin_port = htons(port);
	if (connect(rplsoc, (struct sockaddr *) &s_addr, sizeof(s_addr)) < 0) {
		newlinef(0, DEFAULT, "-!!-", "Error connecting to: %s", hostname);
		close(rplsoc);
		return;
	} else {
		server *ss = new_server(hostname, port, rplsoc);
		s[rplsoc] = ss;

		get_auto_nick(&(ss->nptr), ss->nick_me);

		/* Keep server channel buffers at front of list */
		ccur = cfirst;
		ccur = new_channel(hostname);
		ss->channel = ccur;

		if (cfirst == rirc)
			cfirst = ccur;

		fds[numfds++].fd = rplsoc;
		draw_chans();

		sendf(rplsoc, "NICK %s\r\n", ss->nick_me);
		sendf(rplsoc, "USER %s 8 * :%s\r\n", user_me, realname);
	}
}

void
dis_server(server *s, int kill)
{
	if (cfirst == rirc) {
		newline(0, DEFAULT, "-!!-", "Cannot close main buffer", 0);
		return;
	}

	if (s->soc != 0) {
		sendf(s->soc, "QUIT :Quitting!\r\n");
		close(s->soc);
	}

	int i; /* Shuffle fds to front of array */
	for (i = 1; i < numfds; i++)
		if (fds[i].fd == s->soc) break;
	fds[i] = fds[--numfds];

	if (kill) {
		channel *t, *c = cfirst;
		do {
			t = c;
			c = c->next;
			if (t->server == s && t != cfirst)
				free_channel(t);
		} while (c != cfirst);

		if (cfirst->server == s) {
			t = cfirst;
			if (cfirst->next == cfirst)
				cfirst = rirc;
			else
				cfirst = cfirst->next;
			free_channel(t);
		}
		ccur = cfirst;
		free(s);
	} else {
		channel *c = cfirst;
		do {
			if (c->server == s)
				newline(c, DEFAULT, "-!!-", "(disconnected)", 0);
			c = c->next;
		} while (c != cfirst);
		s->soc = 0;
	}
	draw_full();
}

void
con_lost(int socket)
{
	close(socket);
	s[socket]->soc = 0;
	dis_server(s[socket], 0);
	/* TODO: reconnect routine */
}

void
newline(channel *c, line_t type, char *from, char *mesg, int len)
{
	if (len == 0)
		len = strlen(mesg);

	if (c == 0) {
		if (cfirst == rirc)
			c = rirc;
		else
			c = s[rplsoc]->channel;
	}

	line *l = c->cur_line++;

	if (l->len)
		free(l->text);

	l->len = len;
	if ((l->text = malloc(len)) == NULL)
		fatal("newline");
	memcpy(l->text, mesg, len);

	time(&raw_t);
	t = localtime(&raw_t);
	l->time_h = t->tm_hour;
	l->time_m = t->tm_min;

	l->type = type;

	if (!from) /* Server message */
		strncpy(l->from, c->name, 50);
	else
		strncpy(l->from, from, 50);

	int len_from;
	if ((len_from = strlen(l->from)) > c->nick_pad)
		c->nick_pad = len_from;

	if (c->cur_line == &c->chat[SCROLLBACK])
		c->cur_line = c->chat;

	if (c == ccur)
		draw_chat();
	else if (type == DEFAULT && c->active < ACTIVE) {
		c->active = ACTIVE;
		draw_chans();
	}
}

void
newlinef(channel *c, line_t type, char *from, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buff[BUFFSIZE];
	int len = vsnprintf(buff, BUFFSIZE-1, fmt, args);
	newline(c, type, from, buff, len);
	va_end(args);
}

void
sendf(int soc, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buff[BUFFSIZE];
	int len = vsnprintf(buff, BUFFSIZE-1, fmt, args);
	send(soc, buff, len, 0);
	va_end(args);
}

void
get_auto_nick(char **autonick, char *nick)
{
	char *p = *autonick;
	while (*p == ' ' || *p == ',')
		p++;

	if (*p == '\0') { /* Autonicks exhausted, generate a random nick */
		char *base = "rirc_";
		char *cset = "0123456789";

		strcpy(nick, base);
		nick += strlen(base);

		int i, len = strlen(cset);
		for (i = 0; i < 4; i++)
			*nick++ = cset[rand() % len];
	} else {
		int c = 0;
		while (*p != ' ' && *p != ',' && *p != '\0' && c++ < 50)
			*nick++ = *p++;
		*autonick = p;
	}
	*nick = '\0';
}

int
get_numeric_code(char *code)
{
	/* Codes are always three digits */
	int sum = 0, factor = 100;
	while (isdigit(*code) && factor > 0) {
		sum += factor * (*code++ - '0');
		factor /= 10;
	}

	if (*code != '\0' || factor > 0)
		return 0;

	return sum;
}

channel*
new_channel(char *name)
{
	channel *c;
	if ((c = malloc(sizeof(channel))) == NULL)
		fatal("new_channel");

	c->type = '\0';
	c->nick_pad = 0;
	c->chanmode = 0;
	c->nick_count = 0;
	c->nicklist = NULL;
	c->cur_line = c->chat;
	c->active = NONE;
	c->server = s[rplsoc];
	c->input = new_input();

	strncpy(c->name, name, 50);
	memset(c->chat, 0, sizeof(c->chat));

	if (ccur == rirc || rplsoc == 0)
		c->prev = c->next = c;
	else {
		/* Skip to end of server channels */
		while (!ccur->next->type && ccur->next != cfirst)
			ccur = ccur->next;
		c->prev = ccur;
		c->next = ccur->next;
		ccur->next->prev = c;
		ccur->next = c;
	}
	return c;
}

server*
new_server(char *name, int port, int soc)
{
	server *s;
	if ((s = malloc(sizeof(server))) == NULL)
		fatal("new_server");
	s->soc = soc;
	s->port = port;
	s->nptr = nicks;
	s->usermode = 0;
	s->iptr = s->input;
	strncpy(s->name, name, 50);
	return s;
}

int
is_me(char *nick)
{
	char *n = s[rplsoc]->nick_me;
	while (*n == *nick) {
		if (*n == '\0')
			return 1;
		else
			n++, nick++;
	}
	return 0;
}

channel*
get_channel(char *chan)
{
	channel *c = cfirst;
	do {
		if (!strcmp(c->name, chan) && c->server->soc == rplsoc)
			return c;
		c = c->next;
	} while (c != cfirst);
	return NULL;
}

void
channel_close(void)
{
	if (!ccur->type) {
		dis_server(ccur->server, 1);
	} else {
		channel *c = ccur;
		sendf(ccur->server->soc, "PART %s\r\n", ccur->name);
		ccur = (ccur->next == cfirst) ? ccur->prev : ccur->next;
		free_channel(c);
		draw_full();
	}
}

void
free_channel(channel *c)
{
	line *l = c->chat;
	line *e = l + SCROLLBACK;
	c->next->prev = c->prev;
	c->prev->next = c->next;
	while (l->len && l < e)
		free((l++)->text);
	free_nicklist(c->nicklist);
	free_input(c->input);
	free(c);
}

/*
 * Message sending handlers
 * */

void
send_mesg(char *mesg)
{
	char *cmd;
	if (*mesg != '/')
		send_priv(mesg, 1);
	else if (*++mesg && !(cmd = getarg(&mesg, 1)))
		; /* "/" only message, do nothing */
	else if (streqi(cmd, "JOIN"))
		send_join(mesg);
	else if (streqi(cmd, "CONNECT"))
		send_conn(mesg);
	else if (streqi(cmd, "DISCONNECT"))
		dis_server(ccur->server, 0);
	else if (streqi(cmd, "CLOSE"))
		channel_close();
	else if (streqi(cmd, "PART"))
		send_part(mesg);
	else if (streqi(cmd, "NICK"))
		send_nick(mesg);
	else if (streqi(cmd, "QUIT"))
		send_quit(mesg);
	else if (streqi(cmd, "MSG"))
		send_priv(mesg, 0);
	else if (streqi(cmd, "ME"))
		send_emot(mesg);
	else if (streqi(cmd, "RAW"))
		sendf(ccur->server->soc, "%s\r\n", mesg);
	else {
		int len = strlen(cmd);
		newlinef(ccur, DEFAULT, "-!!-", "Unknown command: %.*s%s",
				15, cmd, len > 15 ? "..." : "");
		return;
	}
}

void
send_conn(char *ptr)
{
	char *hostname, *p;

	/* Default IRC port */
	int port = 6667;

	if (!(hostname = getarg(&ptr, 1)))
		; /* TODO check if disconnected, send reconnect */
	else {

		/* Check for port */
		/* TODO can this be simplified */
		for (p = hostname; *p; p++) {
			if (*p == ':') {
				*p++ = '\0';
				break;
			}
		}

		/* TODO: while is digit, *10, add digit */

		if (*p) {

			int digits = port = 0, factor = 1;

			while (*p) {

				if (!(isdigit(*p++))) {
					newline(0, DEFAULT, "-!!-", "Invalid Port number", 0);
					return;
				} else if (++digits > 5) {
					newline(0, DEFAULT, "-!!-", "Port number out of range", 0);
					return;
				}
			}

			while (digits--) {
				port += (*(--p) - '0') * factor;
				factor *= 10;
			}

			if (port > 65534) {
				newline(0, DEFAULT, "-!!-", "Port number out of range", 0);
				return;
			}
		}
	}

	con_server(hostname, port);
}

void
send_emot(char *ptr)
{
	if (!ccur->type)
		newline(ccur, DEFAULT, "-!!-", "This is not a channel!", 0);
	else {
		newlinef(ccur, ACTION, "*", "%s %s", ccur->server->nick_me, ptr);
		sendf(ccur->server->soc, "PRIVMSG %s :\x01""ACTION %s\x01\r\n", ccur->name, ptr);
	}
}

void
send_join(char *ptr)
{
	sendf(ccur->server->soc, "JOIN %s\r\n", ptr);
}

void
send_nick(char *ptr)
{
	sendf(ccur->server->soc, "NICK %s\r\n", ptr);
}

void
send_quit(char *ptr)
{
	/* TODO: send the quitting message to all servers
	 * use ptr, it it exists, otherwise use the default */
	printf("\x1b[H\x1b[J"); /* Clear */
	exit(EXIT_SUCCESS);
}

void
send_part(char *ptr)
{
	if (ccur == rirc)
		newline(0, DEFAULT, "-!!-", "Cannot execute 'part' on server", 0);
	else {
		newline(ccur, DEFAULT, "", "(disconnected)", 0);
		sendf(ccur->server->soc, "PART %s\r\n", ccur->name);
	}
}

void
send_ping(char *ptr)
{
	/* TODO: */
}

void
send_priv(char *args, int to_chan)
{
	/* TODO: clean all of this up...  */

	if (to_chan) {
		if (!ccur->type)
			newline(ccur, DEFAULT, "-!!-", "This is not a channel!", 0);
		else {
			newline(ccur, DEFAULT, ccur->server->nick_me, args, 0);
			sendf(ccur->server->soc, "PRIVMSG %s :%s\r\n", ccur->name, args);
		}
	} else {
		char *targ;

		/* TODO THIS IS A MESS NOW */
		if (!(args = getarg(&targ, 1)))
			return; /* TODO: error? */

		if (*args == '\0')
			return; /* TODO: error? */

		channel *c;
		ccur = (c = get_channel(targ)) ? c : new_channel(targ);

		sendf(ccur->server->soc, "PRIVMSG %s :%s\r\n", targ, args);
		newline(ccur, DEFAULT, ccur->server->nick_me, args, 0);
		draw_full();
	}
}

/*
 * Message receiving handlers
 * */

void
recv_mesg(char *inp, int count, int soc)
{
	char *ptr = s[soc]->iptr;
	char *max = s[soc]->input + BUFFSIZE;

	while (count--) {
		if (*inp == '\r') {

			*ptr = '\0';

			rplsoc = soc;

			parsed_mesg *p;

			char *err = NULL;
			if (!(p = parse(s[soc]->input)))
				err = "Failed to parse message";
			else if (isdigit(*p->command))
				err = recv_numeric(p);
			else if (streq(p->command, "PRIVMSG"))
				err = recv_priv(p);
			else if (streq(p->command, "JOIN"))
				err = recv_join(p);
			else if (streq(p->command, "PART"))
				err = recv_part(p);
			else if (streq(p->command, "QUIT"))
				err = recv_quit(p);
			else if (streq(p->command, "NOTICE"))
				err = recv_notice(p);
			else if (streq(p->command, "NICK"))
				err = recv_nick(p);
			else if (streq(p->command, "PING"))
				err = recv_ping(p);
			else if (streq(p->command, "MODE"))
				err = recv_mode(p);
			else if (streq(p->command, "ERROR"))
				err = recv_error(p);
			else
				err = "Message type unknown";

			if (err) {
				newline(0, 0, 0, err, 0);
				/* TODO: reset the inserted nulls in s[soc]->input from parsing */
				newlinef(0, DEFAULT, 0, "RPL ERROR: %s", s[soc]->input);
			}

			ptr = s[soc]->input;

		/* Don't accept unprintable characters unless space or ctcp markup */
		} else if (ptr < max && (isgraph(*inp) || *inp == ' ' || *inp == 0x01))
			*ptr++ = *inp;

		inp++;
	}

	s[soc]->iptr = ptr;
}

char*
recv_ctcp(parsed_mesg *p)
{
	/* CTCP extension: trailing = 0x01<target> <command> <arguments>0x01 */

	/* TODO */
	return NULL;
}

char*
recv_error(parsed_mesg *p)
{
	/* TODO */
	return NULL;
}

char*
recv_join(parsed_mesg *p)
{
	/* :nick!user@hostname.domain JOIN <channel> */

	char *chan;
	channel *c;

	if (!p->from)
		return "JOIN: nick is null";

	/* Some servers put the JOIN target in the trailing */
	if (!(chan = getarg(&p->trailing, 1)) && !(chan = getarg(&p->params, 1)))
		return "JOIN: target is null";

	if (is_me(p->from)) {
		ccur = new_channel(chan);
		draw_full();
	} else {

		if ((c = get_channel(chan)) == NULL)
			return errf("JOIN: channel '%s' not found", chan);

		if (nicklist_insert(&(c->nicklist), p->from)) {
			c->nick_count++;

			if (c->nick_count < JOINPART_THRESHOLD)
				newlinef(c, JOINPART, ">", "%s has joined %s", p->from, chan);

			draw_status();
		} else {
			return errf("JOIN: nick '%s' already in '%s'", p->from, chan);
		}
	}

	return NULL;
}

char*
recv_mode(parsed_mesg *p)
{
	/* :nick MODE <targ> :<flags> */

	char *targ, *flags;

	if (!(targ = getarg(&p->params, 1)))
		return "MODE: target is null";

	if (!(flags = p->trailing))
		return "MODE: flags are null";

	int modebit;
	char plusminus = '\0';

	channel *c;
	if ((c = get_channel(targ))) {

		newlinef(c, DEFAULT, "--", "%s chanmode: [%s]", targ, flags);

		int *chanmode = &c->chanmode;

		/* Chanmodes */
		do {
			if (*flags == '+' || *flags == '-')
				plusminus = *flags;
			else if (plusminus == '\0') {
				return "MODE: +/- flag is null";
			} else {
				switch(*flags) {
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
						modebit = 0;
						newlinef(0, DEFAULT, "-!!-", "Unknown mode '%c'", *flags);
				}
				if (modebit) {
					if (plusminus == '+')
						*chanmode |= modebit;
					else
						*chanmode &= ~modebit;
				}
			}
		} while (*(++flags) != '\0');
	}

	if (is_me(targ)) {

		newlinef(0, DEFAULT, "--", "%s usermode: [%s]", targ, flags);

		int *usermode = &s[rplsoc]->usermode;

		/* Usermodes */
		do {
			if (*flags == '+' || *flags == '-')
				plusminus = *flags;
			else if (plusminus == '\0') {
				return "MODE: +/- flag is null";
			} else {
				switch(*flags) {
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
						modebit = 0;
						newlinef(0, DEFAULT, "-!!-", "Unknown mode '%c'", *flags);
				}
				if (modebit) {
					if (plusminus == '+')
						*usermode |= modebit;
					else
						*usermode &= ~modebit;
				}
			}
		} while (*(++flags) != '\0');

		draw_status();
	}

	return NULL;
}

char*
recv_nick(parsed_mesg *p)
{
	/* :nick!user@hostname.domain NICK <new nick> */

	char *nick;

	if (!p->from)
		return "NICK: old nick is null";

	/* Check params first, then trailing */
	if (!(nick = getarg(&p->params, 1)) && !(nick = getarg(&p->trailing, 1)))
		return "NICK: new nick is null";

	if (is_me(p->from))
		strncpy(s[rplsoc]->nick_me, nick, NICKSIZE-1);

	channel *c = cfirst;

	do {
		if (c->server == s[rplsoc] && nicklist_delete(&c->nicklist, p->from)) {
			nicklist_insert(&c->nicklist, nick);
			newlinef(c, NICK, "--", "%s  >>  %s", p->from, nick);
		}
		c = c->next;
	} while (c != cfirst);

	return NULL;
}

char*
recv_notice(parsed_mesg *p)
{
	/* :nick.hostname.domain NOTICE <target> :<message> */

	char *targ;
	channel *c;

	if (!(targ = getarg(&p->params, 1)))
		return "NOTICE: message target is null";

	if (!p->trailing)
		return "NOTICE: message is null";

	if ((c = get_channel(targ)))
		newline(c, DEFAULT, 0, p->trailing, 0);
	else
		newline(0, DEFAULT, 0, p->trailing, 0);

	return NULL;
}

char*
recv_numeric(parsed_mesg *p)
{

	/* Numeric types: https://www.alien.net.au/irc/irc2numerics.html */

	channel *c;
	char *chan, *time, *type, *num;

	/* First parameter in numerics is always your nick */
	char *nick = getarg(&p->params, 1);

	int code;
	if (!(code = get_numeric_code(p->command)))
		return "NUMERIC: code is unknown or null";

	/* Shortcuts */
	if (code > 400) goto num_400;
	if (code > 200) goto num_200;

	/* Numeric types (000, 200) */
	switch (code) {

	/* 001 <nick> :<Welcome message> */
	case RPL_WELCOME:

		/* Reset list of auto nicks */
		s[rplsoc]->nptr = nicks;

		if (*autojoin)
			sendf(rplsoc, "JOIN %s\r\n", autojoin);

		newline(0, NUMRPL, "--", p->trailing, 0);
		return NULL;


	case RPL_YOURHOST:  /* 002 <nick> :<Host info, server version, etc> */
	case RPL_CREATED:   /* 003 <nick> :<Server creation date message> */
		newline(0, NUMRPL, "--", p->trailing, 0);
		return NULL;


	case RPL_MYINFO:    /* 004 <nick> <params> :Are supported by this server */
	case RPL_ISUPPORT:  /* 005 <nick> <params> :Are supported by this server */
		newlinef(0, NUMRPL, "--", "%s ~ %s", p->params, p->trailing);
		return NULL;


	default:

		newlinef(0, NUMRPL, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return NULL;
	}

num_200:

	/* Numeric types (200, 400) */
	switch (code) {

	/* 328 <channel> :<url> */
	case RPL_CHANNEL_URL:

		if (!(chan = getarg(&p->params, 1)))
			return "RPL_CHANNEL_URL: channel is null";

		if ((c = get_channel(chan)) == NULL)
			return "RPL_CHANNEL_URL: channel not found";

		newlinef(c, NUMRPL, "--", "URL for %s is: \"%s\"", chan, p->trailing);
		return NULL;


	/* 332 <channel> :<topic> */
	case RPL_TOPIC:

		if (!(chan = getarg(&p->params, 1)))
			return "RPL_TOPIC: channel is null";

		if ((c = get_channel(chan)) == NULL)
			return "RPL_TOPIC: channel not found";

		newlinef(c, NUMRPL, "--", "Topic for %s is \"%s\"", chan, p->trailing);
		return NULL;


	/* 333 <channel> <nick> <time> */
	case RPL_TOPICWHOTIME:

		if (!(chan = getarg(&p->params, 1)))
			return "RPL_TOPICWHOTIME: channel is null";

		if (!(nick = getarg(&p->params, 1)))
			return "RPL_TOPICWHOTIME: nick is null";

		if (!(time = getarg(&p->params, 1)))
			return "RPL_TOPICWHOTIME: time is null";

		if ((c = get_channel(chan)) == NULL)
			return "RPL_TOPICWHOTIME: channel not found";

		time_t raw_time = atoi(time);
		time = ctime(&raw_time);

		newlinef(c, NUMRPL, "--", "Topic set by %s, %s", nick, time);
		return NULL;


	/* 353 ("="/"*"/"@") <channel> :*([ "@" / "+" ]<nick>) */
	case RPL_NAMREPLY:

		/* @:secret   *:private   =:public */
		if (!(type = getarg(&p->params, 1)))
			return "RPL_NAMEREPLY: type is null";

		if (!(chan = getarg(&p->params, 1)))
			return "RPL_NAMEREPLY: channel is null";

		if ((c = get_channel(chan)) == NULL)
			return "RPL_NAMEREPLY: channel not found";

		c->type = *type;

		while ((nick = getarg(&p->trailing, 1))) {
			if (*nick == '@' || *nick == '+')
				nick++;
			if (nicklist_insert(&c->nicklist, nick))
				c->nick_count++;
		}

		draw_status();

		return NULL;


	case RPL_STATSCONN:    /* 250 :<Message> */
	case RPL_LUSERCLIENT:  /* 251 :<Message> */

		newline(0, NUMRPL, "--", p->trailing, 0);
		return NULL;


	case RPL_LUSEROP:        /* 252 <int> :IRC Operators online */
	case RPL_LUSERUNKNOWN:   /* 253 <int> :Unknown connections */
	case RPL_LUSERCHANNELS:  /* 254 <int> :Channels formed */

		if (!(num = getarg(&p->params, 1)))
			num = "NULL";

		newlinef(0, NUMRPL, "--", "%s %s", num, p->trailing);
		return NULL;


	case RPL_LUSERME:       /* 255 :I have <int> clients and <int> servers */
	case RPL_LOCALUSERS:    /* 265 <int> <int> :Local users <int>, max <int> */
	case RPL_GLOBALUSERS:   /* 266 <int> <int> :Global users <int>, max <int> */
	case RPL_MOTD:          /* 372 : - <Message> */
	case RPL_MOTDSTART:     /* 375 :<server> Message of the day */

		newline(0, NUMRPL, "--", p->trailing, 0);
		return NULL;


	/* Not printing these */
	case RPL_NOTOPIC:     /* 331 <chan> :<Message> */
	case RPL_ENDOFNAMES:  /* 366 <chan> :<Message> */
	case RPL_ENDOFMOTD:   /* 376 :<Message> */
		return NULL;


	default:

		newlinef(0, NUMRPL, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return NULL;
	}

num_400:

	/* Numeric types (400, 600) */
	switch (code) {

	case ERR_ERRONEUSNICKNAME:  /* 432 <nick> :Erroneous nickname */

		if (!(nick = getarg(&p->params, 1)))
			return "RPL_ERRONEUSNICKNAME: nick is null";

		newlinef(0, NUMRPL, "-!!-", "Erroneous nickname: '%s'", nick);
		return NULL;


	case ERR_NICKNAMEINUSE:  /* 433 <nick> :Nickname is already in use */

		newlinef(0, NUMRPL, "-!!-", "Nick '%s' in use", s[rplsoc]->nick_me);

		get_auto_nick(&(s[rplsoc]->nptr), s[rplsoc]->nick_me);

		newlinef(0, NUMRPL, "-!!-", "Trying again with '%s'", s[rplsoc]->nick_me);

		sendf(rplsoc, "NICK %s\r\n", s[rplsoc]->nick_me);
		return NULL;


	default:

		newlinef(0, NUMRPL, "UNHANDLED", "%d %s :%s", code, p->params, p->trailing);
		return NULL;
	}

	return NULL;
}

char*
recv_part(parsed_mesg *p)
{
	/* :nick!user@hostname.domain PART <channel> <:optional message> */

	char *targ;
	channel *c;

	if (!p->from)
		return "PART: from is null";

	if (is_me(p->from))
		return NULL;

	if (!(targ = getarg(&p->params, 1)))
		return "PART: target is null";

	if ((c = get_channel(targ)) && nicklist_delete(&c->nicklist, p->from)) {
		c->nick_count--;
		if (c->nick_count < JOINPART_THRESHOLD) {
			if (p->trailing)
				newlinef(c, JOINPART, "<", "%s left %s (%s)", p->from, targ, p->trailing);
			else
				newlinef(c, JOINPART, "<", "%s left %s", p->from, targ);
		}
	}

	draw_status();

	return NULL;
}

char*
recv_ping(parsed_mesg *p)
{
	if (!p->trailing)
		return "PING: trailing is null";

	sendf(rplsoc, "PONG %s\r\n", p->trailing);

	return NULL;
}

char*
recv_priv(parsed_mesg *p)
{
	/* :nick!user@hostname.domain PRIVMSG <target> :<message> */

	char *targ;

	if (!p->from)
		return "PRIVMSG: sender's name is null";

	if (!p->trailing)
		return "PRIVMSG: message is null";

	if (!(targ = getarg(&p->params, 1)))
		return "PRIVMSG: message target is null";

	channel *c;

	if (is_me(targ)) {

		if ((c = get_channel(p->from)) == NULL)
			c = new_channel(p->from);

		if (c != ccur)
			c->active = PINGED;

		draw_chans();

	} else if ((c = get_channel(targ)) == NULL)
		newlinef(0, DEFAULT, "ERR", "PRIVMSG: target %s not found", targ);

	/* CTCP Markup */
	if (*p->trailing == 0x01)
		recv_ctcp(p);
	else
		newline(c, DEFAULT, p->from, p->trailing, 0);

	return NULL;
}

char*
recv_quit(parsed_mesg *p)
{
	/* :nick!user@hostname.domain QUIT <:optional message> */

	channel *c = cfirst;

	if (!p->from)
		return "QUIT: username not present in prefix";

	do {
		if (c->server == s[rplsoc] && nicklist_delete(&c->nicklist, p->from)) {
			c->nick_count--;
			if (c->nick_count < JOINPART_THRESHOLD) {
				if (p->trailing)
					newlinef(c, JOINPART, "<", "%s has quit (%s)", p->from, p->trailing);
				else
					newlinef(c, JOINPART, "<", "%s has quit", p->from);
			}
		}
		c = c->next;
	} while (c != cfirst);

	draw_status();

	return NULL;
}
