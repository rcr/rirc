#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "common.h"

#define MAXCHANS 10

void send_pong(char*);
void dis_server(void);
void con_server(char*);
char* cmdcmp(char*, char*);
char* cmdcasecmp(char*, char*);
int get_numeric_code(char*)

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
	.name = "rirc",
	.chat = {{0}}}
};

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
		/* see NOTES in printf(3) about characters after final %arg */
		snprintf(sendbuff, BUFFSIZE, "NICK %s\r\n", nick);
		send(soc, sendbuff, strlen(sendbuff), 0);

		snprintf(sendbuff, BUFFSIZE, "USER %s 8 * :%s\r\n", user, realname);
		send(soc, sendbuff, strlen(sendbuff), 0);
	}
	strncpy(chan_list[0].name, hostname, 50), draw_chans();
	connected = 1;
}

void
dis_server(void)
{
	if (!connected) {
		puts("Not connected");
	} else {
		char quit_msg[] = "QUIT :Quitting!\r\n";
		send(soc, quit_msg, strlen(quit_msg), 0);
		close(soc); /* wait for reply before closing? */
		strcpy(chan_list[0].name, "rirc"), draw_chans();
		connected = 0;
	}
}

/* utils */
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
		if (*cmd == '\0' && *inp == ' ') return inp;
	return 0;
}

int
get_numeric_code(char *code)
{
	/* Codes are always three digits */
	int sum = 0, factor = 100;
	do {
		sum += factor * (*code - '0');
		factor = factor / 10;
	} while (isdigit(*++code) && factor > 0);

	if (*code != ' ' || factor > 0)
		return 0;
	else
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

void
send_priv(char *ptr, int count)
{
	;
}

void
send_pong(char *server)
{
	send(soc, "PONG\r\n", 7, 0);
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
	strcpy(sendbuff, "JOIN ");
	strcat(sendbuff, ptr);
	strcpy(&sendbuff[count], "\r\n\0");
	send(soc, sendbuff, strlen(sendbuff), 0);
	return 0;
}

void
send_msg(char *msg, int count)
{
	char *ptr;
	int err = 0;
	/* 512 bytes: Max IRC msg length */
	if (*msg != '/') {
		send_priv(msg, count);
	} else if ((ptr = cmdcasecmp("JOIN", ++msg))) {
		err = send_join(ptr, count);
	} else if ((ptr = cmdcasecmp("CONNECT", msg))) {
		err = send_conn(ptr, count);
	} else if ((ptr = cmdcasecmp("DISCONNECT", msg))) {
		dis_server();
	} else if ((ptr = cmdcasecmp("QUIT", msg))) {
		dis_server();
		run = 0;
	}
		/*
	} else if ((ptr = cmdcasecmp("MSG", msg))) {
		if (!(ptr = getarg(ptr))) {
			goto argerr;
		}
		if (!(ptr = getarg(ptr))) {
			goto argerr;
		}
		puts("GOT MSG");
		*/

	/*
	--- High Priority ---
	MSG -> PRIVMSG target: nick
	NICK
	PART
	--- Low Priority ---
	MODE
	TOPIC
	NAMES
	LIST
	INVITE
	KICK
	--- Not Implementing ---
	PASS
	*/
	else {
		printf("\nUnknown command: %.*s%s\n", 15, msg, count > 15 ? "..." : "");
		return;
	}
	if (err == 1)
		puts("Insufficient arguments");
	if (err == 2)
		puts("Inconrrect arguments");
}

int buff_limit = 0;
char recv_buff[BUFFSIZE];
char *recv_i = recv_buff;

void
do_recv()
{
	char *cmd, *ptr = recv_buff;
	/* check for prefix, then get code and switch */
	if (*ptr == ':') { /* prefix */
		while (*ptr++ != ' ');
	}

	if (isdigit(*ptr)) { /* code */
		puts("GOT CODE");
	} else if ((cmd = cmdcmp("JOIN", ptr))) {
		puts("GOT JOIN");
		; /* TODO: create a channel */
	} else if ((cmd = cmdcmp("PING", ptr))) {
		send_pong(ptr);
	} else {
		; /* TODO: unknown server message */
	}

	printf("%s%s\n", recv_buff, buff_limit ? " (MSG LIM)" : "");
	buff_limit = 0;
}

void
recv_msg(char *input, int count)
{
	while (count--) {

		if (recv_i < recv_buff + BUFFSIZE)
			*recv_i++ = *input;
		else
			buff_limit = 1;

		if (*input++ == '\r' && *input++ == '\n') {
			*recv_i = '\0';
			do_recv();
			recv_i = recv_buff;
			count--;
		}
	}
}



