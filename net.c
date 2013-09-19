#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#define BUFFSIZE 512
#define MAXCHANS 10

int con_server(char*);
void dis_server(void);
void send_msg(char*, int);
void recv_msg(char*, int);
struct in_addr resolve(char*);

char sendbuff[BUFFSIZE];
extern void fatal(char*);


/* Config Stuff */
char nick[] = "test";
char user[] = "rcr";
char realname[] = "Richard Robbins";

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

int num_server = 0;
struct server *s = NULL;

int
con_server(char *hostname)
{
	if (s != NULL)
		return -1;

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
	else {
		/* see NOTES in printf(3) about characters after final %arg */
		snprintf(sendbuff, BUFFSIZE, "NICK %s\r\n", nick);
		send(soc, sendbuff, strlen(sendbuff), 0);

		snprintf(sendbuff, BUFFSIZE, "USER %s 8 * :%s\r\n", user, realname);
		send(soc, sendbuff, strlen(sendbuff), 0);

		char buf3[] = "JOIN #test\r\n";
		send(soc, buf3, strlen(buf3), 0);
	}

	s = malloc(sizeof(server));
	s->socket = soc;
	s->chan_count = 0;
	num_server++;

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
		close(s->socket); /* wait for reply before closing? */
		free(s);
		s = NULL;
		num_server--;
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
