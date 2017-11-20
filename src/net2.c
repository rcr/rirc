#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "net2.h"
#include "utils.h"

#define MAX_CONNECTIONS 15

struct connection {
	const char *host;
	const char *port;
	const void *cb_obj;
	int soc;
};

static void net_poll_inp(int);
static void net_poll_soc(int);

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
net_cx(struct connection *c)
{
	/* TODO */
	(void)(c);
}

void
net_dx(struct connection *c)
{
	/* TODO */
	(void)(c);
}

void
net_free(struct connection *c)
{
	/* TODO */
	(void)(c);

	fds_packed = 0;
}

void
net_poll(void)
{
	int ret, timeout = 1000;

	static nfds_t i, nfds;
	static struct pollfd fds[MAX_CONNECTIONS + 1];

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
				net_poll_soc(fds[i].fd);

			if (--ret == 0)
				return;
		}
	}
}

static void
net_poll_inp(int fd)
{
	(void)fd;
}

static void
net_poll_soc(int fd)
{
	(void)fd;
}
