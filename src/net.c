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
void ins_line(char*, char*, int);
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
int chan_count = 1;
int current_chan = 0;
channel chan_list[MAXCHANS] = {{
	.active = 0,
	.cur_line = 0,
	.nick_pad = 0,
	.name = "rirc",
	.chat = {{0}}}
};

void
channel_sw(int next)
{
	if (next) {
		current_chan = (current_chan + 1) % chan_count;
	} else {
		current_chan = (current_chan - 1 + chan_count) % chan_count;
	}
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
	strncpy(chan_list[0].name, hostname, 20), draw_chans();
	connected = 1;
}

void
dis_server(void)
{
	if (!connected) {
		ins_line("Not connected", 0, 0);
	} else {
		sendf("QUIT :Quitting!\r\n");
		close(soc); /* wait for reply before closing? */
		strcpy(chan_list[0].name, "rirc"), draw_chans();
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
/* end utils */

int
send_priv(char *ptr, int count)
{
	/* TODO: - /msg (target) or if target non-blank*/
	if (current_chan > 0) {
		ins_line(ptr, nick, current_chan);
		sendf("PRIVMSG %s :%s\r\n", chan_list[current_chan].name, ptr);
	} else {
		ins_line("This is not a channel!", 0, 0);
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
	if (chan_count < MAXCHANS) {
		if (!(ptr = getarg(ptr)))
			return 1;
		sendf("JOIN %s\r\n", ptr);
	} else {
		ins_line("ERROR: Max Channels", 0, 0);
	}
	return 0;
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
ins_line(char *inp, char *from, int chan)
{
	line *l = &chan_list[chan].chat[chan_list[chan].cur_line];

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
		strncpy(l->from, chan_list[0].name, 20);
	else
		strncpy(l->from, from, 20);

	int len;
	if ((len = strlen(l->from)) > chan_list[chan].nick_pad)
		chan_list[chan].nick_pad = len;

	chan_list[chan].cur_line++;
	chan_list[chan].cur_line %= SCROLLBACK;

	if (chan == current_chan) {
		draw_chat();
	}
}

void
recv_priv(char *msg)
{
	/* create priv channel, or show in correct channel */
	ins_line("GOT PRIV", 0, 0);
}

void
recv_join(char *msg)
{
	/* TODO on user join: */
	/* :user!~user@localhost.localdomain JOIN :#testing */
	while (*msg == ' ' || *msg == ':')
		msg++;
	channel c = {
		.active = 0,
		.cur_line = 0,
		.nick_pad = 0,
		.chat = {{0}}
	};
	strncpy(c.name, msg, 20);
	chan_list[chan_count++] = c;
	draw_chans();
}

void
do_recv()
{
	char *cmd, *ptr = recv_buff;

	if (*ptr == ':') /* ignore prefix */
		while (*ptr++ != ' ');

	if (isdigit(*ptr)) { /* code */
		int code = get_numeric_code(ptr);
		if (!code) {
			; /* CODE ERROR */
		} else {
			ptr += 4;
			while (*ptr++ != ' ');
			if (*ptr == ':') /* nick arg */
				ptr++;
			/* now we have our suffix/message */
			ins_line(ptr, 0, 0);
		}
	} else if ((cmd = cmdcmp("PRIVMSG", ptr))) {
		recv_priv(cmd);
	} else if ((cmd = cmdcmp("JOIN", ptr))) {
		recv_join(cmd);
	} else if ((cmd = cmdcmp("PING", ptr))) {
		send_pong(cmd);
	} else {
		char errbuff[BUFFSIZE];
		snprintf(errbuff, BUFFSIZE-1, "ERROR ~ %s", recv_buff);
		ins_line(errbuff, 0, 0);
	}
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
