#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>
#include <arpa/inet.h>

#define BUFFSIZE 512
#define MAXCHANS 10

int con_server(char*);
void dis_server(void);
void send_msg(char*, int);
void recv_msg(char*, int);
struct in_addr resolve(char*);
char* cmdcasecmp(char*, char*);

char sendbuff[BUFFSIZE];
extern void fatal(char*);

extern int soc;


/* Config Stuff */
char nick[] = "test";
char user[] = "rcr";
char realname[] = "Richard Robbins";

#define SCROLLBACK 10 /* Number of lines to keep */
struct channel
{
	int cur_line;
	char name[50];
	char *chat[SCROLLBACK];
	/* TODO:
	nicklist
	*/
};

/* For now, limit to one server connection */
struct server
{
	int cur_chan;
	int chan_count;
	struct channel chan_list[MAXCHANS];
};

int connected = 0;
struct server *s = NULL;

int
con_server(char *hostname)
{
	if (s != NULL)
		return -1;

	struct in_addr iadr = resolve(hostname);

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
	}

	s = malloc(sizeof(server));
	s->chan_count = 0;
	connected = 1;

	return soc;
}

void
dis_server(void)
{
	if (s == NULL) {
		puts("Not connected");
	} else {
		char quit_msg[] = "QUIT :Quitting!\r\n";
		send(soc, quit_msg, strlen(quit_msg), 0);
		close(soc); /* wait for reply before closing? */
		free(s);
		s = NULL;
		connected = 0;
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

/* utils */
char*
cmdcasecmp(char *cmd, char *inp)
{
	char *tmp = inp;
	while (*cmd++ == toupper(*tmp++))
		if (*cmd == '\0' && (*tmp == '\0' || *tmp == ' ')) return tmp;
	return 0;
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
	printf("\n\n%s\n",sendbuff);
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
		send_priv(ptr, count);
	} else if ((ptr = cmdcasecmp("JOIN", ++msg))) {
		err = send_join(ptr, count);
	} else if ((ptr = cmdcasecmp("CONNECT", msg))) {
		err = send_conn(ptr, count);
	} else if ((ptr = cmdcasecmp("QUIT", msg))) {
		puts("GOT QUIT");
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
	else {
		printf("\nUnknown command: %.*s%s\n", 15, msg, count > 15 ? "..." : "");
		return;
	}
	if (err == 1)
		puts("Insufficient arguments");
	if (err == 2)
		puts("Inconrrect arguments");
}

void
recv_msg(char *msg, int count)
{
	/* Parse incoming messages, send to chan */
	printf("\033[3;1H\033[2K");
	printf("%.*s", count, msg);
}
