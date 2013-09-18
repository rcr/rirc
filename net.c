#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MAXCHANS 10

int con_server(char*);
void dis_server(void);
void send_msg(char*, int);
void recv_msg(char*, int);
struct in_addr resolve(char*);

extern void fatal(char*);

struct channel
{
	char name[50];
	/* TODO:
	nicklist
	channel history
	*/
};

/* For now, limit to one server connection */
struct server
{
	int socket;
	int cur_chan;
	int chan_count;
	struct channel chan_list[MAXCHANS];
};

struct server *s = NULL;

int
con_server(char *hostname)
{
	struct in_addr iadr = resolve(hostname);

	int soc;
	struct sockaddr_in server;
	if ((soc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		fatal("socket");

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(inet_ntoa(iadr));
	server.sin_port = htons(6667);
	if (connect(soc, (struct sockaddr *) &server, sizeof(server)) < 0)
		fatal("connect");

	s = malloc(sizeof(server));
	s->socket = soc;
	s->chan_count = 0;

	return soc;
}

void
dis_server(void)
{
	if (s == NULL) {
		puts("Not connected");
	} else {
		char quit_msg[] = "QUIT :Quitting!\r\n";
		send(s->socket, quit_msg, strlen(quit_msg), 0);
		close(s->socket);
		free(s);
		s = NULL;
	}
}

struct in_addr
resolve(char *hostname)
{
	struct hostent *host;
	struct in_addr h_addr;
	if ((host = gethostbyname(hostname)) == NULL)
		fatal("nslookup");
	h_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);
	return h_addr;
}

void
send_msg(char *msg, int count)
{
	/* 512 bytes: Max IRC msg length */
	if (*msg == '/') {
		while (++*msg != ' ') {
			/* Copy the command, save msg ptr for more parsing */
			;
		}
		/*
		--- On Connect ---
		USER
		--- High Priority ---
		QUIT
		JOIN
		MSG -> PRIVMSG target: nick
		NICK
		PART
		--- Med Priority ---
		CONNECT
		DISCONNECT
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
	} else {
		/*
		PRIVMSG target: current_channel
		*/
	}
	/* send to cur_channel on server->socket */
}

void
recv_msg(char *msg, int count)
{
	/* Parse incoming messages, send to chan */
	printf("%.*s", count, msg);
}
