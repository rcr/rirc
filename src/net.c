#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdarg.h>

#include <unistd.h>
#include <pthread.h>

#include "src/net.h"
#include "src/utils/utils.h"
#include "config.h"


/* FIXME: testing: */
#include "src/components/channel.h"
#include "src/components/server.h"


#define ELEMS(X) (sizeof((X)) / sizeof((X)[0]))

int
sendf(char *a, struct server *b, const char *c, ...)
{
	(void)a;
	(void)b;
	(void)c;
	fatal("Not implemented", 0);
	return 0;
}

void
server_connect(char *a, char *b, char *c, char *d)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	fatal("Not implemented", 0);
}

void
server_disconnect(struct server *a, int b, int c, char *d)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	fatal("Not implemented", 0);
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

/* Lock the global state mutex on callback */
#define CB(X) \
	pthread_mutex_lock(&cb_mutex); (X); pthread_mutex_unlock(&cb_mutex);

enum net_err_t
{
	NET_ERR_NONE,
	NET_ERR_TRUNC,
	NET_ERR_DXED,
	NET_ERR_CXNG,
	NET_ERR_CXED
};

struct connection {
	const void *obj;
	const char *host;
	const char *port;
	enum net_state_t {
		NET_ST_INIT, /* Initial thread state */
		NET_ST_DXED, /* Socket disconnected, passive */
		NET_ST_RXNG, /* Socket disconnected, pending reconnect */
		NET_ST_CXNG, /* Socket connection in progress */
		NET_ST_CXED, /* Socket connected */
		NET_ST_PING, /* Socket connected, network state in question */
		NET_ST_TERM, /* Terminal thread state */
		NET_ST_SIZE
	} state;
	int soc;
	pthread_attr_t  pt_attr;
	pthread_cond_t  pt_state_cond;
	pthread_mutex_t pt_state_mutex;
	pthread_t       pt_tid;
	struct {
		size_t i;
		char buf[NET_MESG_LEN + 1];
	} read;
	volatile enum net_force_state {
		NET_ST_FORCE_NONE,
		NET_ST_FORCE_CXNG, /* Asynchronously force connection */
		NET_ST_FORCE_DXED, /* Asynchronously force disconnect */
	} force_state;
	char ipbuf[INET6_ADDRSTRLEN];
};

static void* net_routine(void*);

static pthread_mutex_t cb_mutex = PTHREAD_MUTEX_INITIALIZER;

struct connection*
connection(const void *obj, const char *host, const char *port)
{
	struct connection *c;

	if ((c = malloc(sizeof(*c))) == NULL)
		fatal("malloc", errno);

	c->obj  = obj;
	c->host = strdup(host);
	c->port = strdup(port);

	if (0 != pthread_attr_init(&(c->pt_attr))
	|| (0 != pthread_attr_setdetachstate(&(c->pt_attr), PTHREAD_CREATE_DETACHED))
	|| (0 != pthread_cond_init(&(c->pt_state_cond), NULL))
	|| (0 != pthread_mutex_init(&(c->pt_state_mutex), NULL))
	|| (0 != pthread_create(&(c->pt_tid), &(c->pt_attr), net_routine, c)))
	{
		free(c);
		return NULL;
	}

	/* Lock until the INIT state */
	pthread_mutex_lock(&(c->pt_state_mutex));

	return c;
}

int
net_sendf(struct connection *c, const char *fmt, ...)
{
	char sendbuf[512];

	va_list ap;
	int ret;
	size_t len;

	if (c->state != NET_ST_CXED && c->state != NET_ST_PING)
		return NET_ERR_DXED;

	va_start(ap, fmt);
	ret = vsnprintf(sendbuf, sizeof(sendbuf) - 2, fmt, ap);
	va_end(ap);

	if (ret <= 0) {
		return NET_ERR_NONE; /* TODO handle error */
	}

	len = ret;

	if (len >= sizeof(sendbuf) - 2)
		return NET_ERR_TRUNC;

	sendbuf[len++] = '\r';
	sendbuf[len++] = '\n';

	if (send(c->soc, sendbuf, len, 0) < 0) {
		return NET_ERR_NONE; /* TODO: handle error */
	}

	return NET_ERR_NONE;
}

int
net_cx(struct connection *c)
{
	/* Force a socket thread into NET_ST_CXNG state
	 *
	 * For 'waiting' states, blocked on:
	 *   - NET_ST_INIT: pthread_cond_wait()
	 *   - NET_ST_DXED: pthread_cond_wait()
	 *   - NET_ST_RXNG: pthread_cond_timedwait()
	 *
	 * For 'connected' states, blocked on:
	 *   - NET_ST_CXED: recv()
	 *   - NET_ST_PING: recv()
	 */

	enum net_err_t ret = NET_ERR_NONE;

	pthread_mutex_lock(&(c->pt_state_mutex));

	switch (c->state) {
		case NET_ST_INIT:
		case NET_ST_DXED:
		case NET_ST_RXNG:
			pthread_cond_signal(&(c->pt_state_cond));
			break;
		case NET_ST_CXNG:
			ret = NET_ERR_CXNG;
			break;
		case NET_ST_CXED:
		case NET_ST_PING:
			ret = NET_ERR_CXED;
			break;
		default:
			fatal("Unknown net state", 0);
	}

	pthread_mutex_unlock(&(c->pt_state_mutex));

	return ret;
}

int
net_dx(struct connection *c)
{
	/* Force a socket thread into NET_ST_DXED state
	 *
	 * This is valid for all states except for NET_ST_DXED
	 */
	(void) c;
	return 0;
}

static enum net_state_t
net_state_init(enum net_state_t o_state, struct connection *c)
{
	/* Initial thread state */

	(void) o_state;

	pthread_cond_wait(&(c->pt_state_cond), &(c->pt_state_mutex));
	pthread_mutex_unlock(&(c->pt_state_mutex));

	return NET_ST_CXNG;
}

static enum net_state_t
net_state_dxed(enum net_state_t o_state, struct connection *c)
{
	(void) o_state;

	pthread_mutex_lock(&(c->pt_state_mutex));
	/* TODO: check forced state */
	pthread_cond_wait(&(c->pt_state_cond), &(c->pt_state_mutex));
	pthread_mutex_unlock(&(c->pt_state_mutex));

	return NET_ST_CXNG;
}

static enum net_state_t
net_state_rxng(enum net_state_t o_state, struct connection *c)
{
	(void) o_state;

	struct timespec ts;
	struct timeval  tv;

	const int delta_s = 3;

	gettimeofday(&tv, NULL);

	ts.tv_sec  = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	ts.tv_sec += delta_s;

	pthread_mutex_lock(&(c->pt_state_mutex));
	/* TODO: check forced state */
	pthread_cond_timedwait(&(c->pt_state_cond), &(c->pt_state_mutex), &ts);
	pthread_mutex_unlock(&(c->pt_state_mutex));

	return NET_ST_CXNG;
}

static enum net_state_t
net_state_cxng(enum net_state_t o_state, struct connection *c)
{
	(void) o_state;

	int ret, soc, flags;

	char errbuf[512], *errfunc = NULL;
	char ipbuf[INET6_ADDRSTRLEN];

	CB(net_cb_cxng(c->obj, "Connecting to %s:%s ...", c->host, c->port));

	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_flags    = AI_PASSIVE,
		.ai_socktype = SOCK_STREAM
	};

	struct addrinfo *p, *res = NULL;

	/* Resolve host */
	if ((ret = getaddrinfo(c->host, c->port, &hints, &res))) {
		CB(net_cb_fail(c->obj, "Connection error: %s", gai_strerror(ret)));
		return NET_ST_RXNG;
	}

	/* Attempt to connect to all address results */
	for (p = res; p; p = p->ai_next) {

		if ((soc = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			continue;

		if ((ret = fcntl(soc, F_SETFL, O_NONBLOCK)) < 0) {
			errfunc = "fcntl O_NONBLOCK";
			goto failure;
		}

		/* Connection established immediately */
		if (connect(soc, p->ai_addr, p->ai_addrlen) == 0)
			goto success;

		if (errno != EINPROGRESS) {
			close(soc);
			continue;
		}

		fd_set w_fds;

		FD_ZERO(&w_fds);
		FD_SET(soc, &w_fds);

		struct timeval tv = { .tv_usec = 500 * 1000 };

		/* Approximate timeout in microseconds */
		int timeout_us = 30 * 1000 * 1000;

		for (errno = 0;;) {

			if (c->force_state == NET_ST_FORCE_DXED)
				goto canceled;

			if ((ret = select(soc + 1, NULL, &w_fds, NULL, &tv)) < 0 && errno != EINTR) {
				errfunc = "select";
				goto failure;
			}

			if (ret == 0 && (timeout_us -= tv.tv_usec) <= 0) {
				errfunc = "timeout";
				goto failure;
			}

			if (ret == 1) {

				int sockopt;
				socklen_t sockopt_len = sizeof(sockopt);

				if (getsockopt(soc, SOL_SOCKET, SO_ERROR, &sockopt, &sockopt_len) < 0) {
					errfunc = "getsockopt";
					goto failure;
				}

				if (sockopt == 0)
					goto success;
				else
					{ ; } // failure pass sockopt along?
			}
		}
	}

	if (p == NULL) {
		errfunc = "no usable address", errno = 0;
		goto failure;
	}

success:

	if ((flags = fcntl(soc, F_GETFL, 0)) < 0) {
		errfunc = "fcntl F_GETFL";
		goto failure;
	}

	if ((ret = fcntl(soc, F_SETFL, (flags & ~O_NONBLOCK))) < 0) {
		errfunc = "fcntl O_NONBLOCK";
		goto failure;
	}

	// FIXME: call with net_cb_cxng? net_cb_info?
	if ((ret = getnameinfo(p->ai_addr, p->ai_addrlen, ipbuf, INET6_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST))) {
		// ; net_cb_cxed(c->obj, "Connected to %s [IP lookup failure: %s]", c->host, gai_strerror(ret));
	} else {
		// ; net_cb_cxed(c->obj, "Connected to %s [%s]", c->host, ipbuf);
	}

	freeaddrinfo(res);
	c->soc = soc;

	return NET_ST_CXED;

canceled:

	// TODO
	// close socket
	// message
	freeaddrinfo(res);

	return NET_ST_DXED;

failure:
	net_cb_fail(c->obj, "Connection failure %s, [%s]", errfunc, strerror(errno));

	// TODO close socket
	freeaddrinfo(res);

	return NET_ST_RXNG;
}

static enum net_state_t
net_state_cxed(enum net_state_t o_state, struct connection *c)
{
	(void) o_state;

	net_cb_cxed(c->obj, "Connected to %s [TODO: ip]", c->host);

	struct timeval tv = {
		.tv_sec = 5
	};

	ssize_t i, ret;

	if (setsockopt(c->soc, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		{ fatal("setsockopt", errno); }

	char recvbuf[512];

	if ((ret = recv(c->soc, recvbuf, sizeof(recvbuf), 0)) <= 0) {

		if (ret == 0)
			{ ; }

		if (errno == EAGAIN || errno == EWOULDBLOCK)
			{ ; }

		if (errno == ECONNRESET)
			{ ; } /* forcibly closed by peer */

		if (errno == EINTR)
			{ ; } /* signal, affects ping timeout */

		/* TODO: all other errnos -> socket close */
	}

	for (i = 0; i < ret; i++) {

		if (recvbuf[i] == '\n')
			continue;

		if (recvbuf[i] == '\r') {
			if (c->read.i && c->read.i <= NET_MESG_LEN) {
				c->read.buf[c->read.i + 1] = 0;
				net_cb_read_soc(c->read.buf, c->read.i, c->obj);
				c->read.i = 0;
			}
		} else {
			c->read.buf[c->read.i++] = recvbuf[i];
		}
	}

	return 0;
}

static enum net_state_t
net_state_ping(enum net_state_t o_state, struct connection *c)
{
	/* TODO: SO_RCVTIMEO */
	(void) o_state;
	(void) c;
	return 0;
}

static void*
net_routine(void *arg)
{
	struct connection *c = arg;

	enum net_state_t n_state;

	do {

		/* TODO:
		 *
		 * here we know the old state and we know the new state, we can call the informational
		 * transition callback */

		switch (c->state) {
			case NET_ST_INIT:
				n_state = net_state_init(c->state, c);
				break;
			case NET_ST_DXED:
				n_state = net_state_dxed(c->state, c);
				break;
			case NET_ST_CXNG:
				n_state = net_state_cxng(c->state, c);
				break;
			case NET_ST_RXNG:
				n_state = net_state_rxng(c->state, c);
				break;
			case NET_ST_CXED:
				n_state = net_state_cxed(c->state, c);
				break;
			case NET_ST_PING:
				n_state = net_state_ping(c->state, c);
				break;
			case NET_ST_TERM:
				n_state = NET_ST_TERM; // TODO
				break;
			default:
				fatal("Unknown net state", 0);
		}

		// TODO: here lock mutext before calling next state
		// and check through volatile pointer if a force state was given

		c->state = n_state;

	} while (c->state != NET_ST_TERM);

	pthread_attr_destroy(&(c->pt_attr));
	pthread_cond_destroy(&(c->pt_state_cond));
	pthread_mutex_destroy(&(c->pt_state_mutex));

	free((void*)c->host);
	free((void*)c->port);
	free(c);

	return NULL;
}

void
net_free(struct connection *c)
{
	/* TODO */
	(void)c;
}

void
net_poll(void)
{
	static struct pollfd fds[1];

	fds[0].events = POLLIN;
	fds[0].fd = STDIN_FILENO;

	// TODO: ditch poll, just block on read

	while (poll(fds, 1, -1) < 0) {

		/* Exit polling loop to handle signal event */
		if (errno == EINTR)
			/* TODO: ensure that user signal isnt interupting this */
			return;

		if (errno == EAGAIN)
			continue;

		fatal("poll", errno);
	}

	/* Handle user input */
	if (fds[0].revents) {

		if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
			fatal("stdin error", 0);

		if (fds[0].revents & POLLIN) {
			char buff[512];
			int c;
			size_t n = 0;

			flockfile(stdin);

			while ((c = getc_unlocked(stdin)) != EOF) {

				if (n == sizeof(buff))
					continue;

				buff[n++] = (char) c;
			}

			if (ferror(stdin))
				fatal("ferrof", errno);

			clearerr(stdin);
			funlockfile(stdin);

			net_cb_read_inp(buff, n);
		}
	}
}

const char*
net_err(int err)
{
	const char *err_strs[] = {
		[NET_ERR_TRUNC] = "data truncated",
		[NET_ERR_DXED]  = "socket not connected",
		[NET_ERR_CXNG]  = "socket connection in progress",
		[NET_ERR_CXED]  = "socket connected"
	};

	if (err >= 0 && err <= ELEMS(err_strs))
		return err_strs[err];

	return NULL;
}
