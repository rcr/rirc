#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>

struct in_addr resolve_host(char*);

extern void fatal(char*);

struct in_addr
resolve(char *hostname)
{
	hostname = "google.com";
	struct hostent *host;
	struct in_addr h_addr;
	if ((host = gethostbyname(hostname)) == NULL)
		fatal("nslookup");
	h_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);
	return h_addr;
}
