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
void ins_line(char*, char*, channel*);
void send_part(char*);
void send_pong(char*);
void sendf(const char*, ...);
void trimarg_after(char**, char);

char sendbuff[BUFFSIZE];

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
		ins_line("Not connected", "-!!-", ccur);
	} else {
		sendf("QUIT :Quitting!\r\n");
		close(soc); /* wait for reply before closing? */
		strcpy(rirc.name, "rirc"), draw_chans();
		channel *c = &rirc;
		do {
			ins_line("(disconnected)", "-!!-", c);
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
	vsnprintf(sendbuff, BUFFSIZE - 1, fmt, args);
	send(soc, sendbuff, strlen(sendbuff), 0);
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
getarg_after(char **p, char c)
{
	char *ptr = *p;

	while (*ptr != c && *ptr != '\0')
		ptr++;
	while (*ptr == c && *ptr != '\0')
		ptr++;
	*p = ptr;

	if (*ptr == '\0')
		return NULL;
	else
		return ptr;
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
			ins_line("This is not a channel!", "-!!-", 0);
		else {
			ins_line(mesg, nick_me, ccur);
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
		if ((c = get_channel(targ)) != NULL) {
			ins_line(mesg, nick_me, c);
		} else {
			/* new channel */
			c = malloc(sizeof(channel));
			c->active = 0;
			c->cur_line = 0;
			c->nick_pad = 0;
			c->connected = 1;
			memset(c->chat, 0, sizeof(c->chat));
			strncpy(c->name, targ, 50);

			/* Insert into linked list */
			c->next = ccur->next;
			c->prev = ccur;
			ccur->next->prev = c;
			ccur->next = c;

			ccur = c;
			ins_line(mesg, nick_me, c);
			draw_full();
		}
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
		ins_line("Cannot execute 'close' on server buffer", "-!!-", &rirc);
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
		ins_line("Cannot execute 'part' on server buffer", "-!!-", &rirc);
	else {
		ins_line("(disconnected)", "", ccur);
		ccur->connected = 0;
		sendf("PART %s\r\n", ccur->name);
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
		snprintf(errbuff, BUFFSIZE-1, "Unknown command: %.*s%s",
				15, msg, count > 15 ? "..." : "");
		ins_line(errbuff, "-!!-", ccur);
		return;
	}
	if (err == 1)
		ins_line("Insufficient arguments", "-!!-", ccur);
	if (err == 2)
		ins_line("Incorrect arguments", "-!!-", ccur);
}

void
ins_line(char *inp, char *from, channel *chan)
{
	if (!connected) {
		from = "-!!-";
		inp = "You are not connected to a server";
	}

	if (chan == 0)
		chan = &rirc;

	struct line *l;
	l = &chan->chat[chan->cur_line];

	if (l->len)
		free(l->text);

	l->len = strlen(inp);
	l->text = malloc(l->len);
	memcpy(l->text, inp, l->len);

	time(&raw_t);
	t = localtime(&raw_t);
	l->time_h = t->tm_hour;
	l->time_m = t->tm_min;

	if (!from) /* Server message */
		strncpy(l->from, chan->name, 50);
	else
		strncpy(l->from, from, 50);

	int len;
	if ((len = strlen(l->from)) > chan->nick_pad)
		chan->nick_pad = len;

	chan->cur_line++;
	chan->cur_line %= SCROLLBACK;

	if (chan == ccur) {
		draw_chat();
	}
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
			char test[BUFFSIZE];
			snprintf(test, BUFFSIZE-1, "%s %s", from, mesg);
			ins_line(test, "*", 0);
		} else {
			return 1;
		}
	}

	channel *c;
	if (is_me(targ)) {
		/* Private message, */
		if ((c = get_channel(from)) != NULL)
			ins_line(mesg, from, c);
		else {
			c = malloc(sizeof(channel));
			c->active = 0;
			c->cur_line = 0;
			c->nick_pad = 0;
			c->connected = 1;
			memset(c->chat, 0, sizeof(c->chat));
			strncpy(c->name, from, 50);

			/* Insert into linked list */
			c->next = ccur->next;
			c->prev = ccur;
			ccur->next->prev = c;
			ccur->next = c;

			ins_line(mesg, from, c);
			draw_chans();
		}
	} else {
		if ((c = get_channel(targ)) != NULL)
			ins_line(mesg, from, c);
		else {
			snprintf(errbuff, BUFFSIZE-1, "PRIVMSG: target %s not found", targ);
			ins_line(errbuff, "ERR", 0);
		}
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
		ins_line(mesg, 0, 0);
	} else {
		channel *c;
		if ((c = get_channel(targ)) != NULL)
			ins_line(mesg, 0, c);
		else
			ins_line(mesg, 0, 0);
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

	channel *c;
	if (is_me(nick)) {
		/* Server confirmed join, create channel buffer */
		c = malloc(sizeof(channel));
		c->active = 0;
		c->cur_line = 0;
		c->nick_pad = 0;
		c->connected = 1;
		memset(c->chat, 0, sizeof(c->chat));
		strncpy(c->name, chan, 50);

		/* Insert into linked list */
		c->next = ccur->next;
		c->prev = ccur;
		ccur->next->prev = c;
		ccur->next = c;

		ccur = c;
		ins_line(buff, ">", c);

		draw_full();
	} else {
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
	char buff[BUFFSIZE];
	snprintf(buff, BUFFSIZE-1, "%s -> %s", cur_nick, new_nick);
	ins_line(buff, "+", 0);

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

	char buff[BUFFSIZE];
	if ((mesg = getarg_after(&mesg, ':')) == NULL)
		snprintf(buff, BUFFSIZE-1, "%s has quit", nick);
	else
		snprintf(buff, BUFFSIZE-1, "%s has quit (%s)", nick, mesg);

	ins_line(buff, "<", 0);
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

	char buff[BUFFSIZE];
	if ((mesg = getarg_after(&mesg, ':')) == NULL)
		snprintf(buff, BUFFSIZE-1, "%s has left %s", nick, chan);
	else
		snprintf(buff, BUFFSIZE-1, "%s has left %s (%s)", nick, chan, mesg);

	channel *c;
	if ((c = get_channel(chan)) != NULL)
		ins_line(buff, "<", c);
	else {
		snprintf(errbuff, BUFFSIZE-1, "PART: channel %s not found", chan);
		ins_line(errbuff, "ERR", 0);
	}
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
			ins_line(ptr, "CON", 0);
		} else if (code < 400) {
			ins_line(ptr, "INFO", 0);
		} else if (code < 600) {
			ins_line(ptr, "ERROR", 0);
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
	snprintf(errbuff, BUFFSIZE-1, "RPL ERROR: %s", recv_buff);
	ins_line(errbuff, 0, &rirc);
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
