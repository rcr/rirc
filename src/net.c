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
int recv_join(char*, char*);
int recv_mode(char*, char*);
int recv_nick(char*, char*);
int recv_note(char*, char*);
int recv_part(char*, char*);
int recv_priv(char*, char*);
int recv_quit(char*, char*);
int send_conn(char*);
int send_emot(char*);
int send_join(char*);
int send_nick(char*);
int send_part(char*);
int send_pong(char*);
int send_priv(char*, int);
server* new_server(char*, int, int);
void con_server(char*, int);
void dis_server(server*, int);
void do_recv(int);
void get_auto_nick(char**, char*);
void newline(channel*, line_t, char*, char*, int);
void newlinef(channel*, line_t, char*, char*, ...);
void recv_000(int, char*);
void recv_200(int, char*);
void recv_400(int, char*);
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
channel_sw(int next)
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
	s->reg = 0;
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

int
send_conn(char *ptr)
{
	int port = 6667;
	char *hostname, *p;

	if (!getargc(&hostname, &ptr, ':'))
		return 1;

	if (getarg(&p, &ptr)) {
		/* Extract port number, max is 65535 */
		int digits = port = 0, factor = 1;
		while (*p != '\0' && isdigit(*p))
			digits++, p++;
		if (digits > 5) {
			newline(0, DEFAULT, 0, "Invalid port number", 0);
			return 0;
		} else {
			while (digits--) {
				port += (*(--p) - '0') * factor;
				factor *= 10;
			}
		}
		if (port > 65535) {
			newline(0, DEFAULT, 0, "Invalid port number", 0);
			return 0;
		}
	}

	con_server(hostname, port);
	return 0;
}

int
send_emot(char *ptr)
{
	if (!ccur->type)
		newline(ccur, DEFAULT, "-!!-", "This is not a channel!", 0);
	else {
		newlinef(ccur, ACTION, "*", "%s %s", ccur->server->nick_me, ptr);
		sendf(ccur->server->soc, "PRIVMSG %s :\x01""ACTION %s\x01\r\n", ccur->name, ptr);
	}
	return 0;
}

int
send_join(char *ptr)
{
	sendf(ccur->server->soc, "JOIN %s\r\n", ptr);
	return 0;
}

int
send_nick(char *ptr)
{
	sendf(ccur->server->soc, "NICK %s\r\n", ptr);
	return 0;
}

int
send_part(char *ptr)
{
	if (ccur == rirc)
		newline(0, DEFAULT, "-!!-", "Cannot execute 'part' on server", 0);
	else {
		newline(ccur, DEFAULT, "", "(disconnected)", 0);
		sendf(ccur->server->soc, "PART %s\r\n", ccur->name);
	}
	return 0;
}

int
send_pong(char *ptr)
{
	sendf(rplsoc, "PONG %s\r\n", ptr);
	return 0;
}

int
send_priv(char *args, int to_chan)
{
	if (to_chan) {
		if (!ccur->type)
			newline(ccur, DEFAULT, "-!!-", "This is not a channel!", 0);
		else {
			newline(ccur, DEFAULT, ccur->server->nick_me, args, 0);
			sendf(ccur->server->soc, "PRIVMSG %s :%s\r\n", ccur->name, args);
		}
	} else {
		char *targ;

		if (!getarg(&targ, &args))
			return 1;

		if (*args == '\0')
			return 1;

		channel *c;
		ccur = (c = get_channel(targ)) ? c : new_channel(targ);

		sendf(ccur->server->soc, "PRIVMSG %s :%s\r\n", targ, args);
		newline(ccur, DEFAULT, ccur->server->nick_me, args, 0);
		draw_full();
	}

	return 0;
}

void
send_mesg(char *mesg)
{
	char *cmd;
	int err = 0;

	if (*mesg != '/') {
		send_priv(mesg, 1);
	} else if (mesg++ && !getarg(&cmd, &mesg)) {
		; /* Error */
	} else if (cmdcmpc(cmd, "JOIN")) {
		err = send_join(mesg);
	} else if (cmdcmpc(cmd, "CONNECT")) {
		err = send_conn(mesg);
	} else if (cmdcmpc(cmd, "DISCONNECT")) {
		dis_server(ccur->server, 0);
	} else if (cmdcmpc(cmd, "CLOSE")) {
		channel_close();
	} else if (cmdcmpc(cmd, "PART")) {
		send_part(mesg);
	} else if (cmdcmpc(cmd, "NICK")) {
		err = send_nick(mesg);
	} else if (cmdcmpc(cmd, "QUIT")) {
		run = 0;
	} else if (cmdcmpc(cmd, "MSG")) {
		err = send_priv(mesg, 0);
	} else if (cmdcmpc(cmd, "ME")) {
		err = send_emot(mesg);
	} else if (cmdcmpc(cmd, "RAW")) {
		sendf(ccur->server->soc, "%s\r\n", mesg);
	} else {
		int len = strlen(cmd);
		newlinef(ccur, DEFAULT, "-!!-", "Unknown command: %.*s%s",
				15, cmd, len > 15 ? "..." : "");
		return;
	}

	if (err == 1)
		newline(ccur, DEFAULT, "-!!-", "Insufficient arguments", 0);
	if (err == 2)
		newline(ccur, DEFAULT, "-!!-", "Incorrect arguments", 0);
}

int
recv_join(char *prfx, char *args)
{
	/* :nick!user@hostname.domain JOIN :<channel> */

	char *nick, *chan;
	channel *c;

	if (!getargc(&nick, &prfx, '!'))
		return 1;

	if (!getarg(&chan, &args))
		return 1;

	if (is_me(nick)) {
		ccur = new_channel(chan);
		draw_full();
	} else {
		if ((c = get_channel(chan)) && nicklist_insert(&(c->nicklist), nick)) {
			if (c->nick_count < JOINPART_THRESHOLD)
				newlinef(c, JOINPART, ">", "%s has joined %s", nick, chan);
			c->nick_count++;
			draw_status();
		} else if (c == NULL) {
			newlinef(0, DEFAULT, "ERR", "JOIN: channel %s not found", chan);
		} else {
			newlinef(0, DEFAULT, "ERR", "JOIN: %s already in %s", chan, nick);
		}
	}

	return 0;
}

int
recv_mode(char *prfx, char *args)
{
	/* :nick MODE <targ> :<flags> */

	char *targ, *flags;

	if (!getarg(&targ, &args))
		return 1;

	if (!getarg(&flags, &args))
		return 1;

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
				return 1; /* error, flag not set */
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
				return 1; /* error, flag not set */
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

	return 0;
}

int
recv_nick(char *prfx, char *args)
{
	/* :nick!user@hostname.domain NICK <new nick> */

	char *cur_nick, *new_nick;

	if (!getargc(&cur_nick, &prfx, '!'))
		return 1;

	if (!getarg(&new_nick, &args))
		return 1;

	if (is_me(cur_nick)) {
		strncpy(s[rplsoc]->nick_me, new_nick, 50);
		newlinef(ccur, NICK, "*", "You are now known as %s", new_nick);
	} else {
		channel *c = cfirst;
		do {
			if (c->server == s[rplsoc] && nicklist_delete(&c->nicklist, cur_nick)) {
				nicklist_insert(&c->nicklist, new_nick);
				newlinef(c, NICK, "*", "%s  >>  %s", cur_nick, new_nick);
			}
			c = c->next;
		} while (c != cfirst);
	}

	return 0;
}

int
recv_note(char *prfx, char *args)
{
	/* :nick.hostname.domain NOTICE <target> :<message> */

	char *targ, *mesg;
	channel *c;

	if (!getarg(&targ, &args))
		return 1;

	if (!getarg(&mesg, &args))
		return 1;

	if ((c = get_channel(targ)) != NULL)
		newline(c, DEFAULT, 0, mesg, 0);
	else
		newline(0, DEFAULT, 0, mesg, 0);

	return 0;
}

int
recv_part(char *prfx, char *args)
{
	/* :nick!user@hostname.domain PART <channel> <:optional message> */

	char *nick, *chan, *mesg;
	channel *c;

	if (!getargc(&nick, &prfx, '!'))
		return 1;

	if (!getarg(&chan, &args))
		return 1;

	if (is_me(nick))
		return 0;

	if ((c = get_channel(chan)) && nicklist_delete(&c->nicklist, nick)) {
		c->nick_count--;
		if (c->nick_count < JOINPART_THRESHOLD) {
			if (getarg(&mesg, &args))
				newlinef(c, JOINPART, "<", "%s left %s (%s)", nick, chan, mesg);
			else
				newlinef(c, JOINPART, "<", "%s left %s", nick, chan);
		}
	}

	draw_status();

	return 0;
}

int
recv_priv(char *prfx, char *args)
{
	/* :nick!user@hostname.domain PRIVMSG <target> :<message> */

	char *from, *targ, *mesg;
	channel *c;

	if (!getargc(&from, &prfx, '!'))
		return 1;

	if (!getarg(&targ, &args))
		return 1;

	if (!getarg(&mesg, &args))
		return 1;

	if (is_me(targ)) {

		if ((c = get_channel(from)) == NULL)
			c = new_channel(from);

		if (c != ccur)
			c->active = PINGED;

		draw_chans();
	} else if ((c = get_channel(targ)) == NULL)
		newlinef(0, DEFAULT, "ERR", "PRIVMSG: target %s not found", targ);

	/* Check for markup */
	if (*mesg == 0x01) {
		mesg++;

		char *cmd;
		if (!getarg(&cmd, &mesg))
			return 1;

		char *ptr = mesg;
		while (*ptr != '\0' && *ptr != 0x01)
			ptr++;
		*ptr = '\0';

		if (cmdcmp(cmd, "ACTION"))
			newlinef(c, ACTION, "*", "%s %s", from, mesg);
		else
			newlinef(0, DEFAULT, "ERR", "PRIVMSG: unknown command %s", cmd);
	} else {
		newline(c, DEFAULT, from, mesg, 0);
	}

	return 0;
}

int
recv_quit(char *prfx, char *args)
{
	/* :nick!user@hostname.domain QUIT <:optional message> */

	char *nick, *mesg;
	channel *c = cfirst;

	if (!getargc(&nick, &prfx, '!'))
		return 1;

	if (!getarg(&mesg, &args))
		mesg = NULL;

	do {
		if (c->server == s[rplsoc] && nicklist_delete(&c->nicklist, nick)) {
			c->nick_count--;
			if (c->nick_count < JOINPART_THRESHOLD) {
				if (mesg != NULL)
					newlinef(c, JOINPART, "<", "%s has quit (%s)", nick, mesg);
				else
					newlinef(c, JOINPART, "<", "%s has quit", nick);
			}
		}
		c = c->next;
	} while (c != cfirst);

	draw_status();

	return 0;
}

void
recv_000(int code, char *args)
{
	switch(code) {
		case RPL_WELCOME:
			/* Got welcome: send autojoins, reset autonicks, set registered */
			if (*autojoin != '\0')
				sendf(rplsoc, "JOIN %s\r\n", autojoin);
			s[rplsoc]->nptr = nicks;
			s[rplsoc]->reg = 1;
			if (*args == ':')
				args++;
			newline(s[rplsoc]->channel, DEFAULT, "CON", args, 0);
			break;

		case RPL_YOURHOST:
		case RPL_CREATED:
		case RPL_MYINFO:
		case RPL_ISUPPORT:
			if (*args == ':')
				args++;
			newline(0, NUMRPL, "CON", args, 0);
			break;

		default:
			newlinef(0, NUMRPL, "UNHANDLED", "%d ::: %s", code, args);
	}
}

void
recv_200(int code, char *args)
{
	char *chan, *nick, *mesg, *time, *type;
	channel *c;

	switch(code) {

		/* "("="/"*"/"@") <channel> :*([ "@" / "+" ] <nick>) */
		case RPL_NAMREPLY:

			/* @:secret   *:private   =:public */
			if (!getarg(&type, &args))
				goto error;

			if (!getarg(&chan, &args))
				goto error;

			if ((c = get_channel(chan)) == NULL)
				goto error;

			if (!getarg(&args, &args))
				goto error;

			c->type = *type;

			while (getarg(&nick, &args)) {
				/* TODO: ops status */
				if (*nick == '@' || *nick == '+')
					nick++;
				if (nicklist_insert(&c->nicklist, nick))
					c->nick_count++;
			}
			draw_status();
			break;

		/* <channel> :<topic> */
		case RPL_TOPIC:

			if (!getarg(&chan, &args))
				goto error;

			if (!getarg(&args, &args))
				goto error;

			if ((c = get_channel(chan)) == NULL)
				goto error;

			newlinef(c, NUMRPL, "--", "Topic for %s is \"%s\"", chan, args);
			break;

		/* <channel> <nick> <time> */
		case RPL_TOPICWHOTIME:

			if (!getarg(&chan, &args))
				goto error;

			if (!getarg(&nick, &args))
				goto error;

			if (!getarg(&time, &args))
				goto error;

			if ((c = get_channel(chan)) == NULL)
				goto error;

			time_t raw_time = atoi(time);
			time = ctime(&raw_time);

			newlinef(c, NUMRPL, "--", "Topic set by %s, %s", nick, time);
			break;

		/* <channel> :<url> */
		case RPL_CHANNEL_URL:

			if (!getarg(&chan, &args))
				goto error;

			if (!getarg(&mesg, &args))
				goto error;

			if ((c = get_channel(chan)) == NULL)
				goto error;

			newlinef(c, NUMRPL, "--", "URL: %s", mesg);
			break;

		/* Print */
		case RPL_STATSCONN:
		case RPL_LUSERCLIENT:
		case RPL_LUSEROP:
		case RPL_LUSERUNKNOWN:
		case RPL_LUSERCHANNELS:
		case RPL_LUSERME:
		case RPL_LOCALUSERS:
		case RPL_GLOBALUSERS:
		case RPL_MOTDSTART:
		case RPL_MOTD:
			if (*args == ':')
				args++;
			newline(0, NUMRPL, "INFO", args, 0);
			break;

		/* No Print */
		case RPL_ENDOFMOTD:
		case RPL_ENDOFNAMES:
		case RPL_NOTOPIC:
			break;

		default:
			newlinef(0, NUMRPL, "UNHANDLED", "%d ::: %s", code, args);
	}
	return;

error:
	newlinef(0, NUMRPL, "INFO", "%d ::: %s", code, args);
}

void
recv_400(int code, char *args)
{
	switch(code) {

		/* <nick> :Nickname is already in use */
		case ERR_NICKNAMEINUSE:
			if (!s[rplsoc]->reg) {
				newlinef(0, DEFAULT, "--", "nick '%s' in use", s[rplsoc]->nick_me);
				get_auto_nick(&(s[rplsoc]->nptr), s[rplsoc]->nick_me);
				sendf(rplsoc, "NICK %s\r\n", s[rplsoc]->nick_me);
			} else
				newline(0, NUMRPL, "--", args, 0);
			break;

		/* <nick> :Erroneous nickname */
		case ERR_ERRONEUSNICKNAME:
			newline(0, NUMRPL, "--", args, 0);
			break;

		default:
			newlinef(0, NUMRPL, "UNHANDLED", "%d ::: %s", code, args);
	}
}

void
recv_mesg(char *input, int count, int soc)
{
	char *i = s[soc]->iptr;
	char *max = s[soc]->input + BUFFSIZE;

	while (count--) {
		if (*input == '\r') {
			*i = '\0';
			do_recv(soc);
			i = s[soc]->input;
		/* Don't accept unprintable characters unless space or ctcp markup */
		} else if (i < max && (isgraph(*input) || *input == ' ' || *input == 0x01))
			*i++ = *input;

		input++;
	}
	s[soc]->iptr = i;
}

void
do_recv(int soc)
{
	rplsoc = soc;

	int code, err = 0;
	char *args, *prfx, *cmd;
	char *ptr = s[soc]->input;

	/* Check for message prefix */
	if (*ptr == ':' && !getargc(&prfx, &ptr, ' '))
		goto rpl_error;

	if (!getarg(&cmd, &ptr)) {
		goto rpl_error;

	/* Reply code */
	} else if ((code = get_numeric_code(cmd))) {

		if (!getarg(&args, &ptr)) {
			goto rpl_error;
		} else if (code < 200) {
			recv_000(code, ptr);
		} else if (code < 400) {
			recv_200(code, ptr);
		} else if (code < 600) {
			recv_400(code, ptr);
		} else {
			goto rpl_error;
		}
	} else if (cmdcmp(cmd, "PRIVMSG")) {
		err = recv_priv(prfx, ptr);
	} else if (cmdcmp(cmd, "JOIN")) {
		err = recv_join(prfx, ptr);
	} else if (cmdcmp(cmd, "PART")) {
		err = recv_part(prfx, ptr);
	} else if (cmdcmp(cmd, "QUIT")) {
		err = recv_quit(prfx, ptr);
	} else if (cmdcmp(cmd, "NOTICE")) {
		err = recv_note(prfx, ptr);
	} else if (cmdcmp(cmd, "NICK")) {
		err = recv_nick(prfx, ptr);
	} else if (cmdcmp(cmd, "PING")) {
		err = send_pong(ptr);
	} else if (cmdcmp(cmd, "MODE")) {
		err = recv_mode(prfx, ptr);
	} else if (cmdcmp(cmd, "ERROR")) {
		newline(0, DEFAULT, 0, s[soc]->input, 0);
	} else {
		goto rpl_error;
	}

	if (!err)
		return;

rpl_error:
	newlinef(0, DEFAULT, 0, "RPL ERROR: %s", s[soc]->input);
}
