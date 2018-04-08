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
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>

#include <unistd.h>
#include <pthread.h>

#include "src/net.h"
#include "src/utils/utils.h"
#include "config.h"

int
sendf(char *a, struct server *b, const char *c, ...)
{
	(void)a;
	(void)b;
	(void)c;
	fatal("Not implemented", 0);
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

enum net_err_t
{
	NET_ERR_DXED = -2,
	NET_ERR_CXED = -1,
	NET_ERR_NONE
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
		char buf[NET_MESG_LEN];
	} read;
	volatile enum net_force_state {
		NET_FORCE_ST_NONE,
		NET_FORCE_ST_CXNG, /* Asynchronously force connection */
		NET_FORCE_ST_DXED, /* Asynchronously force disconnect */
	} force_state;
};

static void* net_routine(void*);

static pthread_mutex_t cb_mutex = PTHREAD_MUTEX_INITIALIZER;

#define CB(X)                      \
	pthread_mutex_lock(&cb_mutex); \
	(X);                           \
	pthread_mutex_unlock(&cb_mutex);

struct connection*
connection(const void *obj, const char *host, const char *port)
{
	struct connection *c;

	if ((c = calloc(1, sizeof(*c))) == NULL)
		fatal("calloc", errno);

	/* TODO: check host and port are valid? */

	c->obj   = obj;
	c->host  = strdup(host);
	c->port  = strdup(port);
	c->state = NET_ST_INIT;

	if (0 != pthread_attr_init(&(c->pt_attr)))
		{ ; }

	if (0 != pthread_attr_setdetachstate(&(c->pt_attr), PTHREAD_CREATE_DETACHED))
		{ ; }

	if (0 != pthread_cond_init(&(c->pt_state_cond), NULL))
		{ ; }

	if (0 != pthread_mutex_init(&(c->pt_state_mutex), NULL))
		{ ; }

	if (0 != pthread_create(&(c->pt_tid), &(c->pt_attr), net_routine, c))
		{ ; }

	pthread_mutex_lock(&(c->pt_state_mutex));

	return c;
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
			break;
		case NET_ST_CXED:
		case NET_ST_PING:
			break;
		default:
			ret = NET_ERR_CXED;
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

enum net_state_t
net_state_init(enum net_state_t o_state, struct connection *c)
{
	/* Initial thread state */

	(void) o_state;

	pthread_cond_wait(&(c->pt_state_cond), &(c->pt_state_mutex));
	pthread_mutex_unlock(&(c->pt_state_mutex));

	return NET_ST_CXNG;
}

enum net_state_t
net_state_dxed(enum net_state_t o_state, struct connection *c)
{
	(void) o_state;

	pthread_mutex_lock(&(c->pt_state_mutex));
	/* TODO: check forced state */
	pthread_cond_wait(&(c->pt_state_cond), &(c->pt_state_mutex));
	pthread_mutex_unlock(&(c->pt_state_mutex));

	return NET_ST_CXNG;
}

enum net_state_t
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

enum net_state_t
net_state_cxng(enum net_state_t o_state, struct connection *c)
{
	(void) o_state;

	int ret, soc;

	char errbuf[512], *errfunc = NULL;

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

		struct timeval tv = { .tv_usec = 5000 };

		/* Approximate timeout in microseconds */
		int timeout_us = 30 * 1000 * 1000;

		for (errno = 0;;) {

			if (c->force_state == NET_FORCE_ST_DXED)
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

	if ((ret = fcntl(soc, F_SETFL, ~O_NONBLOCK)) < 0) {
		errfunc = "fcntl O_NONBLOCK";
		goto failure;
	}


	freeaddrinfo(res);

	return NET_ST_DXED;

	//strerror_r(errno, errbuf, sizeof(errbuf));
	// cb
	// freeaddrinfo
	// close socket if open

canceled:
	CB(net_cb_cxng(c->obj, "canned"));
	return NET_ST_DXED;

failure:
	CB(net_cb_cxng(c->obj, "failed"));
	return NET_ST_RXNG;

success:
	/* Failing to get the numeric IP isn't a fatal connection error */
//	if ((ret = getnameinfo(p->ai_addr, p->ai_addrlen, ct->ipstr,
//					INET6_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST))) {
//		strncpy(ct->error, gai_strerror(ret), MAX_ERROR);
//	}
	CB(net_cb_cxng(c->obj, "success"));
	return NET_ST_CXED;
}

enum net_state_t
net_state_cxed(enum net_state_t o_state, struct connection *c)
{
	/* TODO: SO_RCVTIMEO */
	(void) o_state;
	(void) c;
	return 0;
}

enum net_state_t
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
			case NET_ST_PING:
			case NET_ST_TERM:
				n_state = NET_ST_TERM; // TODO
				break;
			default:
				n_state = NET_ST_TERM; // TODO
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
