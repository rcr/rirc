#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

int connect_irc(char*);
void diconnect_irc(int);
struct in_addr resolve(char*);

extern void fatal(char*);

int
connect_irc(char *hostname)
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

	return soc;
}

void
disconnect_irc(int soc)
{
	/* TODO: send:   QUIT :<quit message> */
	close(soc);
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
