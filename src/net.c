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

void con_server(char*);
void dis_server(void);
void sendf(const char*, ...);
char* cmdcasecmp(char*, char*);
char* cmdcmp(char*, char*);
int get_numeric_code(char**);
char* getarg(char*);
char* getarg_after(char**, char);
channel* get_channel(char*);
int send_priv(char*);
void send_pong(char*);
int send_conn(char*);
int send_join(char*);
void close_channel(char*);
void send_part(char*);
void ins_line(char*, char*, channel*);
void recv_priv(char*, char*);
void recv_join(char*, char*);
void recv_quit(char*, char*);
void recv_part(char*, char*);
void do_recv();

char sendbuff[BUFFSIZE];

/* Config Stuff */
char nick[] = "rirctest";
char user[] = "rirctest";
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
init_chans()
{
	ccur = &rirc;
	ccur->next = &rirc;
	ccur->prev = &rirc;
}

void
channel_sw(int next)
{
	channel *tmp = ccur;
	if (next) {
		ccur = ccur->next;
	} else {
		ccur = ccur->prev;
	}
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
		sendf("NICK %s\r\n", nick);
		sendf("USER %s 8 * :%s\r\n", user, realname);
	}
	strncpy(rirc.name, hostname, 50), draw_chans();
	connected = 1;
}

void
dis_server(void)
{
	if (!connected) {
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
	if (*ptr == '\0')
		return NULL;
	else {
		*p = ptr + 1;
		return ptr;
	}
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
send_priv(char *ptr)
{
	/* TODO: - /msg (target) or if target non-blank*/
	if (ccur == &rirc) {
		ins_line("This is not a channel!", "-!!-", 0);
	} else {
		ins_line(ptr, nick, ccur);
		sendf("PRIVMSG %s :%s\r\n", ccur->name, ptr);
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
		err = send_priv(msg);
	} else if ((ptr = cmdcasecmp("JOIN", ++msg))) {
		err = send_join(ptr);
	} else if ((ptr = cmdcasecmp("CONNECT", msg))) {
		err = send_conn(ptr);
	} else if ((ptr = cmdcasecmp("DISCONNECT", msg))) {
		dis_server();
	} else if ((ptr = cmdcasecmp("CLOSE", msg))) {
		close_channel(msg);
	} else if ((ptr = cmdcasecmp("PART", msg))) {
		send_part(msg);
	} else if ((ptr = cmdcasecmp("QUIT", msg))) {
		dis_server();
		run = 0;
	} else if ((ptr = cmdcasecmp("MSG", msg))) {
		err = send_priv(ptr);
	} else {
		snprintf(errbuff, BUFFSIZE-1, "Unknown command: %.*s%s",
				15, msg, count > 15 ? "..." : "");
		ins_line(errbuff, 0, 0);
		return;
	}
	if (err == 1)
		ins_line("Insufficient arguments", 0, 0);
	if (err == 2)
		ins_line("Incorrect arguments", 0, 0);
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

void
recv_priv(char *pfx, char *msg)
{
	/* TODO create priv channel, or show in correct channel */
	while (*pfx == ' ' || *pfx == ':')
		pfx++;
	char *from = pfx;
	while (*pfx != '!')
		pfx++;
	*pfx = '\0';

	while (*msg == ' ')
		msg++;
	char *dest = msg;
	while (*msg != ' ')
		msg++;
	*msg++ = '\0';
	while (*msg == ' ' || *msg == ':')
		msg++;
	
	channel *c;
	if ((c = get_channel(dest)) != NULL) {
		ins_line(msg, from, c);
	} else {
		ins_line("NO CHANNEL FOUND", 0, 0);
	}
}

void
recv_join(char *pfx, char *msg)
{
	/* :user!~user@localhost.localdomain JOIN :#testing */

	while (*pfx == ' ' || *pfx == ':')
		pfx++;
	while (*msg == ' ' || *msg == ':')
		msg++;
	
	/* compare to nick */
	char *p = pfx;
	char *n = nick;
	int isme = 0;
	while (*p == *n) {
		p++, n++;
		if (*p == '!' && *n == '\0') {
			isme = 1;
			break;
		} else if (*n == '\0') {
			break;
		}
	}
	char buff[BUFFSIZE];
	char *nick = pfx;
	while (*pfx != '!')
		pfx++;
	*pfx = '\0';
	snprintf(buff, BUFFSIZE-1, "%s has joined %s", nick, msg);

	channel *c;
	if (isme) {
		c = malloc(sizeof(channel));
		c->active = 0;
		c->cur_line = 0;
		c->nick_pad = 0;
		c->connected = 1;
		memset(c->chat, 0, sizeof(c->chat));
		strncpy(c->name, msg, 50);

		c->next = ccur->next;
		c->prev = ccur;
		ccur->next->prev = c;
		ccur->next = c;

		ccur = c;
		ins_line(buff, ">", c);

		draw_full();
	} else {
		if ((c = get_channel(msg)) != NULL) {
			ins_line(buff, ">", c);
		} else {
			ins_line("NO CHANNEL FOUND", 0 ,0);
		}
	}
}

void
recv_quit(char *pfx, char *msg)
{
	/* :user!user@localhost.localdomain QUIT :Gone to have lunch */
	while (*pfx == ' ' || *pfx == ':')
		pfx++;
	while (*msg == ' ' || *msg == ':')
		msg++;
	char *nick = pfx;
	while (*pfx != '!')
		pfx++;
	*pfx = '\0';
	char buff[BUFFSIZE];
	snprintf(buff, BUFFSIZE-1, "%s has quit (Quit: %s)", nick, msg);
	ins_line(buff, "<", 0);
}

void
recv_part(char *pfx, char *msg)
{
	//TODO: if no part message, display none
	while (*pfx == ' ' || *pfx == ':')
		pfx++;
	while (*msg == ' ' || *msg == ':')
		msg++;

	char *nick = pfx;
	char *chan = msg;
	while (*pfx++ != '!');
	*pfx = '\0';
	while (*msg != ' ')
		msg++;
	*msg++ = '\0';
	while (*msg++ != ':');

	char buff[BUFFSIZE];
	snprintf(buff, BUFFSIZE-1, "%s has left %s ~ (%s)", nick, chan, msg);

	channel *c;
	ins_line(chan, "test", 0);
	if ((c = get_channel(chan)) != NULL) {
		ins_line(buff, "<", c);
	} else {
		ins_line("NO CHANNEL FOUND", 0 ,0);
	}
}

void
do_recv()
{
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
		recv_priv(pfx, args);
	} else if ((args = cmdcmp("JOIN", ptr))) {
		recv_join(pfx, args);
	} else if ((args = cmdcmp("PART", ptr))) {
		recv_part(pfx, args);
	} else if ((args = cmdcmp("QUIT", ptr))) {
		recv_quit(pfx, args);
	} else if ((args = cmdcmp("PING", ptr))) {
		send_pong(args);
		/* TODO:
	} else if ((args = cmdcmp("NICK", ptr))) {
		recv_nick(...;
	} else if ((args = cmdcmp("MODE", ptr))) {
		recv_mode(...;
	} else if ((args = cmdcmp("NOTICE", ptr))) {
		recv_notice(...
		;
		*/
	} else {
		goto rpl_error;
	}
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
