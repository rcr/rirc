/* TODO:
 * Dynamic poll timeout for connecting/pinging connections
 * Check sanity of config
 *
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "src/net.h"
#include "src/utils/utils.h"
#include "config.h"

void check_servers(void) { }

/* FIXME: refactoring, stubbed until removal */
int
sendf(char *a, struct server *b, const char *c, ...)
{
	(void)a;
	(void)b;
	(void)c;
	return 0;
}

void
server_connect(char *a, char *b, char *c, char *d)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
}

void
server_disconnect(struct server *a, int b, int c, char *d)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
}

/* RFC 2812, section 2.3 */
#define NET_MESG_LEN 510

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

enum NET_ERR_T
{
	NET_ERR_DXED = -2,
	NET_ERR_CXED = -1,
	NET_ERR_NONE
};

struct connection {
	const void *cb_obj;
	const char *host;
	const char *port;
	enum {
		NET_DXED, /* Socket disconnected, passive */
		NET_RXNG, /* Socket disconnected, pending reconnect */
		NET_CXNG, /* Socket connection in progress */
		NET_CXED, /* Socket connected */
		NET_PING  /* Socket connected, network state in question */
	} status;
	int soc;
	size_t conn_i;
	size_t read_i;
	char readbuff[NET_MESG_LEN];
};

static void net_cx_failure(struct connection*, int);
static void net_cx_pollerr(struct connection*, int);
static void net_cx_pollhup(struct connection*);
static void net_cx_readsoc(struct connection*);
static void net_cx_success(struct connection*);

static void net_file_read(FILE*);

static void _net_cx(struct connection*);
static void _net_dx(struct connection*);

static int fds_packed;
static unsigned int n_connections;
static struct connection *connections[NET_MAX_CONNECTIONS];

struct connection*
connection(const void *cb_obj, const char *host, const char *port)
{
	/* Instantiate a new connection
	 *
	 * Doesn't flag fds for repack, since socket
	 * enters into disconnected state */

	struct connection *c;

	if (n_connections == NET_MAX_CONNECTIONS)
		return NULL;

	if ((c = calloc(1, sizeof(*c))) == NULL)
		fatal("calloc", errno);

	c->cb_obj = cb_obj;
	c->conn_i = n_connections++;
	c->host   = strdup(host);
	c->port   = strdup(port);
	c->soc    = -1;
	c->status = NET_DXED;

	connections[c->conn_i] = c;

	return c;
}

void
net_free(struct connection *c)
{
	if (c->status != NET_DXED)
		fatal("Freeing open connection", 0);

	/* Swap the last connection into this index */
	connections[c->conn_i] = connections[--n_connections];

	free((void*)c->host);
	free((void*)c->port);
	free(c);

	fds_packed = 0;
}

int
net_cx(struct connection *c)
{
	if (c->status == NET_CXED || c->status == NET_PING)
		return NET_ERR_CXED;

	_net_cx(c);

	return NET_ERR_NONE;
}

int
net_dx(struct connection *c)
{
	if (c->status == NET_DXED)
		return NET_ERR_DXED;

	_net_dx(c);

	return NET_ERR_NONE;
}

void
net_poll(void)
{
	int ret, timeout = 1000;

	static nfds_t nfds, i;
	static struct pollfd fds[NET_MAX_CONNECTIONS + 1];

	if (fds_packed == 0) {

		memset(fds, 0, sizeof(fds));

		for (nfds = 0; nfds < n_connections; nfds++) {

			if (connections[nfds]->status == NET_CXNG)
				fds[nfds].events = POLLOUT;
			else
				fds[nfds].events = POLLIN;

			fds[nfds].fd = connections[nfds]->soc;
		}

		fds[nfds].events = POLLIN;
		fds[nfds].fd = STDIN_FILENO;

		fds_packed = 1;
	}

	while ((ret = poll(fds, nfds + 1, timeout)) < 0) {

		/* Exit polling loop to handle signal event */
		if (errno == EINTR)
			return;

		if (errno == EAGAIN)
			continue;

		fatal("poll", errno);
	}

	/* Handle user input */
	if (fds[nfds].revents) {

		if (fds[nfds].revents & (POLLERR | POLLHUP | POLLNVAL))
			fatal("stdin error", 0);

		if (fds[nfds].revents & POLLIN)
			net_file_read(stdin);

		ret--;
	}

	/* Handle sockets */
	for (i = 0; ret && i < nfds; i++) {

		/* POLLNVAL results from invalid file descriptor and
		 * is treated as fatal programming error
		 *
		 * POLLERR superscedes POLLIN, POLLOUT, POLLHUP and
		 * indicates a fatal error for the device or stream
		 *
		 * POLLOUT indicates success or failure for a socket
		 * while connecting. It is mutually exclusive with POLLHUP
		 *
		 * POLLIN indicates data is readable from this socket
		 * and is not mutually exclusive with POLLHUP; data
		 * may be readable on a socket after remote hangup
		 *
		 * POLLHUP indicates remote hangup for active connection
		 *
		 * POLLERR, POLLHUP, POLLIN can all result in disconnect
		 * for an active connection:
		 *  - POLLERR: socket error
		 *  - POLLHUP: remote hangup
		 *  - POLLIN:  via actions in callback handler
		 */

		int optval;
		socklen_t optlen = sizeof(optval);

		if (fds[i].revents == 0)
			continue;

		if (fds[i].revents & POLLNVAL)
			fatal("invalid fd", 0);

		if (fds[i].revents & POLLERR) {

			if (0 > getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &optval, &optlen))
				fatal("getsockopt", errno);
			else
				net_cx_pollerr(connections[i], optval);

		} else if (fds[i].revents & POLLOUT) {

			if (0 > getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &optval, &optlen))
				fatal("getsockopt", errno);

			if (optval == 0)
				net_cx_success(connections[i]);
			else
				net_cx_failure(connections[i], optval);

		} else {

			if (fds[i].revents & POLLIN)
				net_cx_readsoc(connections[i]);

			if (fds[i].revents & POLLHUP)
				net_cx_pollhup(connections[i]);
		}

		ret--;
	}
}

static void
_net_cx(struct connection *c)
{
	/* TODO */
	(void)c;
}

static void
_net_dx(struct connection *c)
{
	/* TODO */
	(void)c;
}

static void
net_cx_failure(struct connection *c, int err)
{
	/* TODO */
	(void)err; //strerror(err)
	(void)c;
}

static void
net_cx_pollerr(struct connection *c, int err)
{
	/* TODO */
	(void)err;
	(void)c;
}

static void
net_cx_pollhup(struct connection *c)
{
	/* TODO */
	(void)c;
}

static void
net_cx_success(struct connection *c)
{
	/* TODO */
	(void)c;
}

static void
net_cx_readsoc(struct connection *c)
{
	ssize_t count, i;
	char soc_readbuff[4096];

	while ((count = read(c->soc, soc_readbuff, sizeof(soc_readbuff)))) {

		if (count < 0 && errno != EINTR)
			fatal("read", errno);

		size_t read_i = c->read_i;

		for (i = 0; i < count; i++) {

			if (read_i == NET_MESG_LEN || soc_readbuff[i] == '\r') {
				c->readbuff[i] = 0;
				net_cb_read_soc(c->readbuff, read_i, c->cb_obj);
				read_i = 0;
			}

			/* Filter printable characters and CTCP markup */
			if (isgraph(soc_readbuff[i]) || soc_readbuff[i] == ' ' || soc_readbuff[i] == 0x01)
				c->readbuff[read_i++] = soc_readbuff[i];
		}

		c->read_i = read_i;
	}
}

static void
net_file_read(FILE *f)
{
	char buff[512];
	int c;
	size_t n = 0;

	flockfile(f);

	while ((c = getc_unlocked(f)) != EOF) {

		if (n == sizeof(buff))
			continue;

		buff[n++] = (char) c;
	}

	if (ferror(f))
		fatal("ferrof", errno);

	clearerr(f);
	funlockfile(f);

	net_cb_read_inp(buff, n);
}
