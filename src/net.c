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

void sendf(const char*, ...);
void send_pong(char*);
void dis_server(void);
void con_server(char*);
void ins_line(char*, char*, channel*);
char* cmdcmp(char*, char*);
char* cmdcasecmp(char*, char*);
int get_numeric_code(char**);

char sendbuff[BUFFSIZE];

/* Config Stuff */
char nick[] = "rcr";
char user[] = "rcr";
char realname[] = "Richard Robbins";

int soc;
int connected = 0;

channel *ccur;
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
	strncpy(rirc.name, hostname, 20), draw_chans();
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

channel*
get_channel(char *chan)
{
	channel *c = &rirc;
	do {
		if (strcmp(c->name, chan))
			return c;
	} while (c->next != &rirc);
	return NULL;
}
/* end utils */

int
send_priv(char *ptr, int count)
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
send_conn(char *ptr, int count)
{
	if (!(ptr = getarg(ptr)))
		return 1;
	con_server(ptr);
	return 0;
}

int
send_join(char *ptr, int count)
{
	if (!(ptr = getarg(ptr)))
		return 1;
	sendf("JOIN %s\r\n", ptr);
	return 0;
}

void send_part(char*);
void close_channel(char*);

void
close_channel(char *ptr)
{
	if (ccur == &rirc)
		ins_line("Cannot execute 'close' on server buffer", 0, 0);
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
		ins_line("Cannot execute 'part' on server buffer", 0, 0);
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
		err = send_priv(msg, count);
	} else if ((ptr = cmdcasecmp("JOIN", ++msg))) {
		err = send_join(ptr, count);
	} else if ((ptr = cmdcasecmp("CONNECT", msg))) {
		err = send_conn(ptr, count);
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
		err = send_priv(ptr, count);
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

char recv_buff[BUFFSIZE];
char *recv_i = recv_buff;

time_t raw_t;
struct tm *t;

void
ins_line(char *inp, char *from, channel *chan)
{
	if (chan == 0)
		chan = &rirc;

	struct line *l;
	l = &chan->chat[chan->cur_line];

	if (l->len)
		free(l->text);

	l->len = strlen(inp) + 1;
	l->text = malloc(l->len);
	memcpy(l->text, inp, l->len);

	time(&raw_t);
	t = localtime(&raw_t);
	l->time_h = t->tm_hour;
	l->time_m = t->tm_min;

	if (!from) /* Server message */
		strncpy(l->from, ccur->name, 20);
	else
		strncpy(l->from, from, 20);

	int len;
	if ((len = strlen(l->from)) > ccur->nick_pad)
		ccur->nick_pad = len;

	ccur->cur_line++;
	ccur->cur_line %= SCROLLBACK;

	if (chan == ccur) {
		draw_chat();
	}
}

void
recv_priv(char *pfx, char *msg)
{
	/* get username from pfx */
	/* TODO create priv channel, or show in correct channel */
	//ins_line("GOT PRIV", 0, 0);
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
	while (*p++ == *n++) {
		if (*p == '!')
			isme = 1;
	}

	if (isme) {
		channel *c = malloc(sizeof(channel));
		c->active = 0;
		c->cur_line = 0;
		c->nick_pad = 0;
		c->connected = 1;
		memset(c->chat, 0, sizeof(c->chat));
		strncpy(c->name, msg, 20);

		c->next = ccur->next;
		c->prev = ccur;
		ccur->next->prev = c;
		ccur->next = c;

		ccur = c;
		ins_line(pfx, 0, c);
		ins_line(msg, 0, c);

		draw_full();
	} else {
		channel *c;
		if ((c = get_channel(msg))!= NULL) {
			ins_line(pfx, 0, c);
			ins_line(msg, 0, c);
		} else {
			ins_line("NO CHANNEL FOUND", 0 ,0);
		}
	}
}

void
recv_part(char *pfx, char *msg)
{
	;
}

void
do_recv()
{
	char *args, *pfx = 0, *ptr = recv_buff;

	if (*ptr == ':') {
		pfx = ptr;
		while (*ptr++ != ' ' && *ptr != '\0');
	}

	if (isdigit(*ptr)) { /* code */
		int code = get_numeric_code(&ptr);
		if (!code) {
			goto rpl_error;
		} else if (code < 200) {
			;
		} else if (code < 400) {
			;
		} else if (code < 600) {
			;
		} else {
			;
		}
	} else if ((args = cmdcmp("PRIVMSG", ptr))) {
		recv_priv(pfx, args);
	} else if ((args = cmdcmp("JOIN", ptr))) {
		recv_join(pfx, args);
	} else if ((args = cmdcmp("PART", ptr))) {
		recv_part(pfx, args);
	} else if ((args = cmdcmp("PING", ptr))) {
		send_pong(args);
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
