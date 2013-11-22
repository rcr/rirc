#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <arpa/inet.h>

#include "common.h"

#define MAXCHANS 10

channel* get_channel(char*);
channel* new_channel(char*);
char* cmdcasecmp(char*, char*);
char* cmdcmp(char*, char*);
char* getarg(char*);
char* getarg_after(char**, char);
int get_numeric_code(char**);
int recv_join(char*, char*);
int recv_nick(char*, char*);
int recv_note(char*, char*);
int recv_part(char*, char*);
int recv_priv(char*, char*);
int recv_quit(char*, char*);
int send_conn(char*);
int send_join(char*);
int send_priv(char*, int);
void close_channel(char*);
void con_server(char*);
void dis_server(int);
void do_recv(void);
void newline(channel*, line_t, char*, char*, int);
void newlinef(channel*, line_t, char*, char*, ...);
void send_part(char*);
void send_pong(char*);
void sendf(const char*, ...);
void trimarg_after(char**, char);

/* Config Stuff */
char nick_me[] = "r18449";
char user_me[] = "r18449";
char realname[] = "Richard Robbins";

int soc;
int connected = 0;

char recv_buff[BUFFSIZE];
char *recv_i = recv_buff;

time_t raw_t;
struct tm *t;

channel *ccur, *cserver;
channel rirc = {
	.active = 0,
	.cur_line = 0,
	.nick_pad = 0,
	.name = "rirc",
	.chat = {{0}}
};

void
init_chans(void)
{
	ccur = &rirc;
	ccur->next = &rirc;
	ccur->prev = &rirc;
}

void
channel_sw(int next)
{
	channel *tmp = ccur;
	if (next)
		ccur = ccur->next;
	else
		ccur = ccur->prev;
	if (tmp != ccur)
		draw_full();
}

void
con_server(char *hostname)
{
	if (connected)
		return;

	struct hostent *host;
	struct in_addr h_addr;
	if ((host = gethostbyname(hostname)) == NULL)
		fatal("nslookup");
	h_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);

	struct sockaddr_in server;
	if ((soc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		fatal("socket");

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(inet_ntoa(h_addr));
	server.sin_port = htons(6667);
	if (connect(soc, (struct sockaddr *) &server, sizeof(server)) < 0)
		fatal("connect");
	else {
		sendf("NICK %s\r\n", nick_me);
		sendf("USER %s 8 * :%s\r\n", user_me, realname);
	}
	strncpy(rirc.name, hostname, 50), draw_chans();
	connected = 1;
}

void
dis_server(int kill)
{
	if (kill) {
		run = 0;
	} else if (!connected) {
		newline(ccur, DEFAULT, "-!!-", "Not connected", 0);
	} else {
		sendf("QUIT :Quitting!\r\n");
		close(soc); /* wait for reply before closing? */
		strcpy(rirc.name, "rirc"), draw_chans();
		channel *c = &rirc;
		do {
			newline(c, DEFAULT, "-!!-", "(disconnected)", 0);
			c = c->next;
		} while (c != &rirc);
		connected = 0;
	}
}

/* utils */
void
sendf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buff[BUFFSIZE];
	int len = vsnprintf(buff, BUFFSIZE-1, fmt, args);
	send(soc, buff, len, 0);
	va_end(args);
}

char*
cmdcasecmp(char *cmd, char *inp)
{
	while (*cmd++ == toupper(*inp++))
		if (*cmd == '\0' && (*inp == '\0' || *inp == ' ')) return inp;
	return 0;
}

char*
cmdcmp(char *cmd, char *inp)
{
	while (*cmd++ == *inp++)
		if (*cmd == '\0' && (*inp == '\0' || *inp == ' ')) return inp;
	return 0;
}

int
get_numeric_code(char **c)
{
	/* Codes are always three digits */
	char *code = *c;
	int sum = 0, factor = 100;
	do {
		sum += factor * (*code - '0');
		factor = factor / 10;
	} while (isdigit(*++code) && factor > 0);

	if (*code != ' ' || factor > 0)
		return 0;

	*c = code + 1;
	return sum;
}

char*
getarg(char *ptr)
{
	while (*ptr == ' ')
		ptr++;

	if (*ptr != '\0')
		return ptr;
	else
		return NULL;
}

char*
getarg_after(char **p1, char c)
{
	char *p2 = *p1;
	while (*p2 != c && *p2 != '\0')
		p2++;
	while (*p2 == c && *p2 != '\0')
		p2++;
	*p1 = p2;

	if (*p1 == '\0')
		return NULL;
	else
		return p2;
}


void
trimarg_after(char **arg, char c)
{
	char *p = *arg;
	for (;;) {
		if (*p == '\0') {
			*arg = p;
			break;
		} else if (*p == c) {
			*p = '\0';
			*arg = (p + 1);
			break;
		} else {
			p++;
		}
	}
}

channel*
new_channel(char *name)
{
	/* TODO: track channel count */
	channel *c;
	if ((c = malloc(sizeof(channel))) == NULL)
		fatal("new_channel");
	c->active = 0;
	c->cur_line = 0;
	c->nick_pad = 0;
	c->connected = 1;
	memset(c->chat, 0, sizeof(c->chat));
	strncpy(c->name, name, 50);

	/* Insert into linked list */
	c->next = ccur->next;
	c->prev = ccur;
	ccur->next->prev = c;
	ccur->next = c;

	return c;
}

int
is_me(char *nick)
{
	char *n = nick_me;
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
	channel *c = &rirc;
	do {
		if (!strcmp(c->name, chan))
			return c;
		c = c->next;
	} while (c != &rirc);
	return NULL;
}
/* end utils */

int
send_priv(char *mesg, int to_chan)
{
	if (to_chan) {
		if (ccur == &rirc)
			newline(0, DEFAULT, "-!!-", "This is not a channel!", 0);
		else {
			newline(ccur, DEFAULT, nick_me, mesg, 0);
			sendf("PRIVMSG %s :%s\r\n", ccur->name, mesg);
		}
	} else {
		/* TODO: the parsing logic here can be simplified and combined 
		 * with getarg */
		char *targ;
		if ((targ = getarg_after(&mesg, ' ')) == NULL)
			return 1;
		trimarg_after(&mesg, ' ');
		while (*mesg == ' ' && *mesg != '\0')
			mesg++;
		if (*mesg == '\0')
			return 1;

		channel *c;
		if ((c = get_channel(targ)) == NULL)
			ccur = new_channel(targ);
		else
			ccur = c;
		newline(ccur, DEFAULT, nick_me, mesg, 0);
		draw_full();

		sendf("PRIVMSG %s :%s\r\n", targ, mesg);
	}
	return 0;
}

void
send_pong(char *server)
{
	sendf("PONG%s\r\n", server);
}

int
send_conn(char *ptr)
{
	if (!(ptr = getarg(ptr)))
		return 1;
	con_server(ptr);
	return 0;
}

int
send_join(char *ptr)
{
	if (!(ptr = getarg(ptr)))
		return 1;
	sendf("JOIN %s\r\n", ptr);
	return 0;
}

void
close_channel(char *ptr)
{
	if (ccur == &rirc)
		newline(0, DEFAULT, "-!!-", "Cannot execute 'close' on server", 0);
	else {
		sendf("PART %s\r\n", ccur->name);
		channel *c = ccur;
		c->next->prev = c->prev;
		c->prev->next = c->next;
		ccur = c->next;
		free(c);
		draw_full();
	}
}

void
send_part(char *ptr)
{
	if (ccur == &rirc)
		newline(0, DEFAULT, "-!!-", "Cannot execute 'part' on server", 0);
	else {
		newline(ccur, DEFAULT, "", "(disconnected)", 0);
		sendf("PART %s\r\n", ccur->name);
		ccur->connected = 0;
	}
}

void
send_msg(char *msg, int count)
{
	char *ptr;
	int err = 0;
	/* 512 bytes: Max IRC msg length */
	if (*msg != '/') {
		err = send_priv(msg, 1);
	} else if ((ptr = cmdcasecmp("JOIN", ++msg))) {
		err = send_join(ptr);
	} else if ((ptr = cmdcasecmp("CONNECT", msg))) {
		err = send_conn(ptr);
	} else if ((ptr = cmdcasecmp("DISCONNECT", msg))) {
		dis_server(0);
	} else if ((ptr = cmdcasecmp("CLOSE", msg))) {
		close_channel(msg);
	} else if ((ptr = cmdcasecmp("PART", msg))) {
		send_part(msg);
	} else if ((ptr = cmdcasecmp("QUIT", msg))) {
		dis_server(1);
	} else if ((ptr = cmdcasecmp("MSG", msg))) {
		err = send_priv(ptr, 0);
	} else {
		newlinef(ccur, DEFAULT, "-!!-", "Unknown command: %.*s%s",
				15, msg, count > 15 ? "..." : "");
		return;
	}
	if (err == 1)
		newline(ccur, DEFAULT, "-!!-", "Insufficient arguments", 0);
	if (err == 2)
		newline(ccur, DEFAULT, "-!!-", "Incorrect arguments", 0);
}


/* not even really needed, just check if args are null? */
void
newline(channel *c, line_t type, char *from, char *mesg, int len)
{
	if (!connected) {
		from = "-!!-";
		mesg = "You are not connected to a server";
		len = strlen(mesg);
	}

	if (len == 0)
		len = strlen(mesg);

	if (c == 0)
		c = &rirc;

	struct line *l;
	l = &c->chat[c->cur_line];

	if (l->len)
		free(l->text);

	l->len = len;
	l->text = malloc(len);
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

	c->cur_line++;
	c->cur_line %= SCROLLBACK;

	if (c == ccur)
		draw_chat();
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

int
recv_priv(char *prfx, char *mesg)
{
	/* :nick!user@hostname.localdomain PRIVMSG <target> :Message */

	char *from, *targ;

	/* Get the sender's nick */
	if ((from = getarg_after(&prfx, ':')) == NULL)
		return 1;
	trimarg_after(&prfx, '!');

	/* Get the message target */
	if ((targ = getarg_after(&mesg, ' ')) == NULL)
		return 1;
	trimarg_after(&mesg, ' ');

	/* Get the message */
	if ((mesg = getarg_after(&mesg, ':')) == NULL)
		return 1;

	// line_t type = DEFAULT;

	/* Check for markup */
	if (*mesg == 0x01) {

		char *ptr;
		if ((ptr = getarg_after(&mesg, 0x01)) == NULL)
			return 1;
		trimarg_after(&mesg, ' ');

		if (cmdcmp("ACTION", ptr)) {
			// type = ACTION;
			/* TODO: ine_line(buff, from, chanel, line_t type) */
			/* strip terminating 0x01 */
			newlinef(0, DEFAULT, "*", "%s %s", from, mesg);
		} else {
			return 1;
		}
	}

	channel *c;
	if (is_me(targ)) {
		/* Private message, */
		if ((c = get_channel(from)) == NULL)
			c = new_channel(from);
		newline(c, DEFAULT, from, mesg, 0);
		draw_chans();
	} else {
		if ((c = get_channel(targ)) != NULL)
			newline(c, DEFAULT, from, mesg, 0);
		else
			newlinef(0, DEFAULT, "ERR", "PRIVMSG: target %s not found", targ);
	}
	return 0;
}

int
recv_note(char *prfx, char *mesg)
{
	/* :name.hostname.localdomain NOTICE <target> :Message */

	char *targ;

	/* Get the message target */
	if ((targ = getarg_after(&mesg, ' ')) == NULL)
		return 1;
	trimarg_after(&mesg, ' ');

	/* Get the message */
	if ((mesg = getarg_after(&mesg, ':')) == NULL)
		return 1;

	if (is_me(targ)) {
		newline(0, DEFAULT, 0, mesg, 0);
	} else {
		channel *c;
		if ((c = get_channel(targ)) != NULL)
			newline(c, DEFAULT, 0, mesg, 0);
		else
			newline(0, DEFAULT, 0, mesg, 0);
	}
	return 0;
}

int
recv_join(char *prfx, char *mesg)
{
	/* :user!~user@localhost.localdomain JOIN :#testing */

	char *nick, *chan;

	/* Get the joining nick */
	if ((nick = getarg_after(&prfx, ':')) == NULL)
		return 1;
	trimarg_after(&prfx, '!');

	/* Get the channel to join */
	if ((chan = getarg_after(&mesg, ' ')) == NULL)
		return 1;

	/* FIXME: Stupid. ngircd uses JOIN :#channel, freenode used JOIN #channel */
	if (*chan == ':')
		chan++;

	char buff[BUFFSIZE];
	snprintf(buff, BUFFSIZE-1, "%s has joined %s", nick, chan);

	if (is_me(nick)) {
		ccur = new_channel(chan);
		ins_line(buff, ">", ccur);
		draw_full();
	} else {
		channel *c;
		if ((c = get_channel(chan)) != NULL)
			ins_line(buff, ">", c);
		else {
			snprintf(errbuff, BUFFSIZE-1, "JOIN: channel %s not found", chan);
			ins_line(errbuff, "ERR", 0);
		}
	}
	return 0;
}

void

int
recv_nick(char *prfx, char *mesg)
{
	/* :nick!user@localhost.localdomain NICK rcr2 */

	char *cur_nick, *new_nick;

	if ((cur_nick = getarg_after(&prfx, ':')) == NULL)
		return 1;
	trimarg_after(&prfx, '!');

	if ((new_nick = getarg_after(&mesg, ':')) == NULL)
		return 1;

	/* TODO: - change name in all channels where use is */
	/*       - display message in those channels */
	newlinef(0, NICK, "", "NICK: %s -> %s", cur_nick, new_nick);

	return 0;
}

int
recv_quit(char *prfx, char *mesg)
{
	/* :nick!user@localhost.localdomain QUIT [:Optional part message] */

	char *nick;
	if ((nick = getarg_after(&prfx, ':')) == NULL)
		return 1;
	trimarg_after(&prfx, '!');

	/* TODO: this should be inserted into any channel where the user was... */

	if ((mesg = getarg_after(&mesg, ':')) == NULL)
		newlinef(0, JOINPART, "<", "%s has quit", nick);
	else
		newlinef(0, JOINPART, "<", "%s has quit (%s)", nick, mesg);

	return 0;
}

int
recv_part(char *prfx, char *mesg)
{
	/* :nick!user@hostname.localdomain PART #channel [:Optional part message] */

	char *nick, *chan;

	if ((nick = getarg_after(&prfx, ':')) == NULL)
		return 1;
	trimarg_after(&prfx, '!');

	if (is_me(nick))
		return 0;

	if ((chan = getarg_after(&mesg, ' ')) == NULL)
		return 1;
	trimarg_after(&mesg, ' ');

	channel *c;
	if ((c = get_channel(chan)) != NULL)
		if ((mesg = getarg_after(&mesg, ':')) == NULL)
			newlinef(c, JOINPART, "<", "%s has left %s", nick, chan);
		else
			newlinef(c, JOINPART, "<", "%s has left %s (%s)", nick, chan, mesg);
	else
		newlinef(0, JOINPART, "<", "PART: channel %s not found", chan);

	return 0;
}

void
do_recv(void)
{
	int err = 0;
	char *args, *pfx = 0, *ptr = recv_buff;

	if (*ptr == ':') {
		pfx = ptr;
		while (*ptr++ != ' ' && *ptr != '\0');
	}

	if (isdigit(*ptr)) { /* Reply code */

		int code = get_numeric_code(&ptr);

		char *args;
		/* Cant parse until ':' because of 004, 005, 254, 255, etc */
		if ((args = getarg_after(&ptr, ' ')) == NULL)
			goto rpl_error;
		/* So remove it here */
		if (*ptr == ':')
			ptr++;

		if (!code) {
			goto rpl_error;
		} else if (code < 200) {
			newline(0, NUMRPL, "CON", ptr, 0);
		} else if (code < 400) {
			newline(0, NUMRPL, "INFO", ptr, 0);
		} else if (code < 600) {
			newline(0, NUMRPL, "ERROR", ptr, 0);
		} else {
			goto rpl_error;
		}
	} else if ((args = cmdcmp("PRIVMSG", ptr))) {
		err = recv_priv(pfx, args);
	} else if ((args = cmdcmp("JOIN", ptr))) {
		err = recv_join(pfx, args);
	} else if ((args = cmdcmp("PART", ptr))) {
		err = recv_part(pfx, args);
	} else if ((args = cmdcmp("QUIT", ptr))) {
		err = recv_quit(pfx, args);
	} else if ((args = cmdcmp("NOTICE", ptr))) {
		err = recv_note(pfx, args);
	} else if ((args = cmdcmp("NICK", ptr))) {
		err = recv_nick(pfx, args);
	} else if ((args = cmdcmp("PING", ptr))) {
		send_pong(args);
		/* TODO:
	} else if ((args = cmdcmp("MODE", ptr))) {
		recv_mode(...;
	} else if ((args = cmdcmp("INFO", ptr))) {
		recv_mode(...;
		;
		*/
	} else {
		goto rpl_error;
	}

	if (!err)
		return;

rpl_error:
	newlinef(0, DEFAULT, 0, "RPL ERROR: %s", recv_buff);
}

void
recv_msg(char *input, int count)
{
	while (count-- > 0) {

		if (recv_i < recv_buff + BUFFSIZE)
			*recv_i++ = *input;

		input++;
		if (*input == '\r' && *(input + 1) == '\n') {
			*recv_i = '\0';
			input += 2;
			count -= 2;
			do_recv();
			recv_i = recv_buff;
		}
	}
}
