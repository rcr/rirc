#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "src/net2.h"
#include "src/utils.h"
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
	const char *host;
	const char *port;
	const void *cb_obj;
	enum {
		NET_DISCONNECTED
	} state;
	int soc;
};

static void net_poll_inp(int);
static void net_poll_soc(int, const void*);

static int fds_packed;
static unsigned int num_connections;

static struct connection *connections[MAX_CONNECTIONS];

struct connection*
connection(const char *host, const char *port, const void *cb_obj)
{
	struct connection *c = NULL;

	(void)(host);
	(void)(port);
	(void)(cb_obj);
	/* TODO */

	fds_packed = 0;

	return c;
}

void
net_free_connection(struct connection *c)
{
	/* TODO */
	(void)(c);

	/* if not in disconnected state, fatal */

	fds_packed = 0;
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

		nfds = 1 + num_connections;

		fds[0].fd = STDIN_FILENO;
		fds[0].events = POLLIN;

		for (i = 1; i <= num_connections; i++) {
			fds[i].fd = connections[i]->soc;
			fds[i].events = POLLIN;
		}
	}

	while ((ret = poll(fds, nfds, timeout)) <= 0) {

		if (ret == 0)
			return;

		if (!(errno == EAGAIN || errno == EINTR))
			fatal("poll", errno);
	}

	for (i = 0; i < nfds; i++) {

		if (fds[i].revents & POLLIN) {
			fds[i].revents = 0;

			if (i == STDIN_FILENO)
				net_poll_inp(fds[i].fd);
			else
				net_poll_soc(fds[i].fd, connections[i]->cb_obj);

			if (--ret == 0)
				return;
		}
	}
}

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

static void
net_poll_inp(int fd)
{
	ssize_t count;
	char readbuff[1024];

	while ((count = read(fd, readbuff, sizeof(readbuff))) <= 0) {

		if (count == 0)
			fatal("stdin closed", 0);

		if (errno != EINTR)
			fatal("read", errno);
	}

	net_cb_read_inp(readbuff, (size_t) count);
}

static void
net_poll_soc(int fd, const void *cb_obj)
{
	ssize_t count;
	char readbuff[1024];

	while ((count = read(fd, readbuff, sizeof(readbuff)))) {

		if (count == 0) {
			; /* disconnect */
		}

		if (count < 0) {
			; /* error, possibly eintr? */
		}
	}
	net_cb_read_soc(readbuff, (size_t) count, cb_obj);
}
