#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#include "src/net2.h"
#include "src/utils/utils.h"
#include "config.h"

#ifndef NET_PING_MIN
	#define NET_PING_MIN 150
#elif (NET_PING_MIN < 0 || NET_PING_MIN > 86400)
	#error "NET_PING_MIN: [0, 86400]"
#endif

#ifndef NET_PING_REFRESH
	#define NET_PING_REFRESH 5
#elif (NET_PING_REFRESH < 0 || NET_PING_REFRESH > 86400)
	#error "NET_PING_REFRESH: [0, 86400]"
#endif

#ifndef NET_PING_MAX
	#define NET_PING_MAX 300
#elif (NET_PING_MAX < 0 || NET_PING_MAX > 86400)
	#error "NET_PING_MAX: [0, 86400]"
#endif

#ifndef NET_RECONNECT_BACKOFF_BASE
	#define NET_RECONNECT_BACKOFF_BASE 10
#elif (NET_RECONNECT_BACKOFF_BASE < 1 || NET_RECONNECT_BACKOFF_BASE > 86400)
	#error "NET_RECONNECT_BACKOFF_BASE: [1, 32]"
#endif

#ifndef NET_RECONNECT_BACKOFF_FACTOR
	#define NET_RECONNECT_BACKOFF_FACTOR 2
#elif (NET_RECONNECT_BACKOFF_FACTOR < 1 || NET_RECONNECT_BACKOFF_FACTOR > 32)
	#error "NET_RECONNECT_BACKOFF_FACTOR: [1, 32]"
#endif

#ifndef NET_RECONNECT_BACKOFF_MAX
	#define NET_RECONNECT_BACKOFF_MAX 86400
#elif (NET_RECONNECT_BACKOFF_MAX < 1 || NET_RECONNECT_BACKOFF_MAX > 86400)
	#error "NET_RECONNECT_BACKOFF_MAX: [0, 86400]"
#endif

/* TODO: check sanity of config */

#define MAX_CONNECTIONS 15

enum NET_ERR_T
{
	NET_ERR_NONE
};

struct connection {
	const void *cb_obj;
	const char *host;
	const char *port;
	enum {
		NET_CONNECTING,
		NET_RECONNECTING,
		NET_DISCONNECTED
	} state;
	int soc;
	size_t conn_idx;
	size_t read_idx;
	char readbuff[NET_MESG_LEN];
};

static void net_poll_inp(int);
static void net_poll_soc(int, struct connection*);

static int fds_packed;
static unsigned int n_connections;

static struct connection *connections[MAX_CONNECTIONS];

struct connection*
connection(const void *cb_obj, const char *host, const char *port)
{
	/* Instantiate a new connection
	 *
	 * Doesn't flag fds for repack, since socket
	 * enters into disconnected state */

	struct connection *c;

	if (n_connections == MAX_CONNECTIONS)
		return NULL;

	if ((c = calloc(1, sizeof(*c))) == NULL)
		fatal("calloc", errno);

	c->cb_obj = cb_obj;
	c->host = strdup(host);
	c->port = strdup(port);
	c->soc = -1;
	c->state = NET_DISCONNECTED;
	c->conn_idx = n_connections++;

	connections[c->conn_idx] = c;

	return c;
}

void
net_free_connection(struct connection *c)
{
	if (c->state != NET_DISCONNECTED)
		fatal("Freeing open connection", 0);

	/* Swap the last connection into this index */
	connections[c->conn_idx] = connections[--n_connections];

	free((void*)c->host);
	free((void*)c->port);
	free(c);

	fds_packed = 0;
}

/* TODO: here, just check the state of the connection and return error, or
 * call the internal disconnect function that ignores already disconnected
 * sockets. this way we can call disconnect twice, e.g. POLLIN callback
 * or POLLHUP*/
int
net_cx(struct connection *c)
{
	/* TODO */
	(void)(c);

	return NET_ERR_NONE;
}

int
net_dx(struct connection *c)
{
	/* TODO */
	(void)(c);

	return NET_ERR_NONE;
}

void
net_poll(void)
{
	int ret, timeout = 1000;

	static nfds_t i, nfds;
	static struct pollfd fds[MAX_CONNECTIONS + 1];

	/* Repack only before polling */
	if (fds_packed == 0) {
		fds_packed = 1;

		memset(fds, 0, sizeof(fds));

		nfds = 1 + n_connections;

		fds[0].fd = STDIN_FILENO;
		fds[0].events = POLLIN;

		for (i = 1; i <= n_connections; i++) {
			fds[i].fd = connections[i]->soc;
			fds[i].events = POLLIN;
		}
	}

	while ((ret = poll(fds, nfds, timeout)) < 0) {

		if (!(errno == EAGAIN || errno == EINTR))
			fatal("poll", errno);
	}


	/* Handle user input */
	if (fds[0].revents) {

		if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
			fatal("stdin error", 0);

		if (fds[0].revents & POLLIN)
			net_poll_inp(fds[i].fd);

		ret--;
	}

	/* Handle sockets */
	for (i = 1; ret && i < nfds; i++) {

		if (fds[i].revents == 0)
			continue;

		/* POLLNVAL results from invalid file descriptor and
		 * is treated as fatal programming error
		 *
		 * POLLERR, POLLHUP, POLLIN can all result in disconnect:
		 *  - POLLERR: socket error
		 *  - POLLHUP: remote hangup
		 *  - POLLIN:  via actions in callback handler
		 *
		 * POLLHUP and POLLIN are not mutually exclusive, data
		 * might still be readable on the socket after hangup
		 */

		if (fds[i].revents & POLLNVAL)
			fatal("invalid fd", 0);

		if (fds[i].revents & (POLLERR | POLLHUP)) {

			net_dx(connections[i]);

			if (fds[i].revents & POLLERR)
				net_cb_dxed(connections[i]->cb_obj, "Disconnected: socket error");
			else
				net_cb_dxed(connections[i]->cb_obj, "Disconnected: remote hangup");
		}

		if (fds[i].revents & POLLIN) {
			net_poll_soc(fds[i].fd, connections[i]);
		}

		ret--;
	}
}

static void
net_poll_inp(int fd)
{
	ssize_t count;
	char inp_readbuff[4096];

	while ((count = read(fd, inp_readbuff, sizeof(inp_readbuff)))) {

		if (count < 0 && errno != EINTR)
			fatal("read", errno);

		net_cb_read_inp(inp_readbuff, (size_t) count);
	}
}

static void
net_poll_soc(int fd, struct connection *c)
{
	ssize_t count, i;
	char soc_readbuff[4096];

	while ((count = read(fd, soc_readbuff, sizeof(soc_readbuff)))) {

		if (count < 0 && errno != EINTR)
			fatal("read", errno);

		size_t read_idx = c->read_idx;

		for (i = 0; i < count; i++) {

			if (read_idx == NET_MESG_LEN || soc_readbuff[i] == '\r') {
				c->readbuff[i] = 0;
				net_cb_read_soc(c->readbuff, read_idx, c->cb_obj);
				read_idx = 0;

			}

			/* Filter printable characters and CTCP markup */
			if (isgraph(soc_readbuff[i]) || soc_readbuff[i] == ' ' || soc_readbuff[i] == 0x01)
				c->readbuff[read_idx++] = soc_readbuff[i];
		}

		c->read_idx = read_idx;
	}
}
