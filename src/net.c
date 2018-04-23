#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include <unistd.h>
#include <pthread.h>

#include "src/net.h"
#include "src/utils/utils.h"
#include "config.h"

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

#define PT_CHECK_FATAL(X) do { int ret; if ((ret = (X)) != 0) fatal((#X), ret); } while (0)
#define PT_LK(X) PT_CHECK_FATAL(pthread_mutex_lock((X)))
#define PT_UL(X) PT_CHECK_FATAL(pthread_mutex_unlock((X)))
#define PT_CB(X) do { PT_LK(&cb_mutex); (X); PT_UL(&cb_mutex); } while (0)

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

static void* net_thread(void*);

static char* net_strerror(int, char*, size_t);

static void net_close(struct connection*, int*);
static void net_init(void);
static void net_term(void);

static pthread_cond_t init_cond;
static pthread_mutex_t cb_mutex;
static pthread_mutex_t init_mutex;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static void
net_close(struct connection *c, int *fd)
{
	/* Lock thread state to prevent ambiguous handling of EINTR on close() */

	if (*fd >= 0) {
		PT_LK(&(c->pt_state_mutex));
		PT_CHECK_FATAL(close(*fd));
		PT_UL(&(c->pt_state_mutex));
		*fd = -1;
	}
}

static char*
net_strerror(int errnum, char *buf, size_t buflen)
{
	PT_CHECK_FATAL(strerror_r(errnum, buf, buflen));
	return buf;
}

static void
net_init(void)
{
	pthread_mutexattr_t m_attr;

	PT_CHECK_FATAL(pthread_mutexattr_init(&m_attr));
	PT_CHECK_FATAL(pthread_mutexattr_settype(&m_attr, PTHREAD_MUTEX_ERRORCHECK));

	PT_CHECK_FATAL(pthread_cond_init(&init_cond, NULL));
	PT_CHECK_FATAL(pthread_mutex_init(&cb_mutex, &m_attr));
	PT_CHECK_FATAL(pthread_mutex_init(&init_mutex, &m_attr));

	PT_CHECK_FATAL(pthread_mutexattr_destroy(&m_attr));

	if (atexit(net_term) != 0)
		fatal("atexit", 0);
}

static void
net_term(void)
{
	// FIXME: at this point the callback mutex might have already been used uninitialized
	// because the main thread is running...
	PT_UL(&cb_mutex);
	PT_CHECK_FATAL(pthread_cond_destroy(&init_cond));
	PT_CHECK_FATAL(pthread_mutex_destroy(&cb_mutex));
	PT_CHECK_FATAL(pthread_mutex_destroy(&init_mutex));
}

struct connection*
connection(const void *obj, const char *host, const char *port)
{
	struct connection *c;

	if ((c = malloc(sizeof(*c))) == NULL)
		fatal("malloc", errno);

	c->obj   = obj;
	c->host  = strdup(host);
	c->port  = strdup(port);
	c->state = NET_ST_INIT;

	pthread_attr_t      t_attr;
	pthread_mutexattr_t m_attr;

	PT_CHECK_FATAL(pthread_attr_init(&t_attr));
	PT_CHECK_FATAL(pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED));

	PT_CHECK_FATAL(pthread_mutexattr_init(&m_attr));
	PT_CHECK_FATAL(pthread_mutexattr_settype(&m_attr, PTHREAD_MUTEX_ERRORCHECK));

	PT_CHECK_FATAL(pthread_cond_init(&(c->pt_state_cond), NULL));
	PT_CHECK_FATAL(pthread_mutex_init(&(c->pt_state_mutex), &m_attr));

	PT_CHECK_FATAL(pthread_once(&init_once, net_init));

	/* Wait for thread to reach initialized and waiting state */
	PT_LK(&init_mutex);
	PT_CHECK_FATAL(pthread_create(&(c->pt_tid), &t_attr, net_thread, c));
	PT_CHECK_FATAL(pthread_cond_wait(&init_cond, &init_mutex));
	PT_UL(&init_mutex);

	PT_CHECK_FATAL(pthread_attr_destroy(&t_attr));
	PT_CHECK_FATAL(pthread_mutexattr_destroy(&m_attr));

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
	 * Valid only for 'waiting' states, blocked on:
	 *   - NET_ST_INIT: pthread_cond_wait()
	 *   - NET_ST_DXED: pthread_cond_wait()
	 *   - NET_ST_RXNG: pthread_cond_timedwait()
	 */

	enum net_err_t ret = NET_ERR_NONE;

	PT_LK(&(c->pt_state_mutex));

	switch (c->state) {
		case NET_ST_INIT:
		case NET_ST_DXED:
		case NET_ST_RXNG:
			PT_CHECK_FATAL(pthread_cond_signal(&(c->pt_state_cond)));
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

	PT_UL(&(c->pt_state_mutex));

	return ret;
}

int
net_dx(struct connection *c)
{
	/* Force a socket thread into NET_ST_DXED state
	 *
	 * Valid for connecting and connected states blocked on:
	 *   - NET_ST_RXNG: pthread_cond_timedwait()
	 *   - NET_ST_CXNG: connect()
	 *   - NET_ST_CXED: recv()
	 *   - NET_ST_PING: recv()
	 */

	enum net_err_t ret = NET_ERR_NONE;

	PT_LK(&(c->pt_state_mutex));

	switch (c->state) {
		case NET_ST_INIT:
		case NET_ST_DXED:
			ret = NET_ERR_DXED;
			break;
		case NET_ST_RXNG:
			PT_CHECK_FATAL(pthread_cond_signal(&(c->pt_state_cond)));
			break;
		case NET_ST_CXNG:
		case NET_ST_CXED:
		case NET_ST_PING:
			do {
				PT_CHECK_FATAL(pthread_kill(c->pt_tid, SIGUSR1));
				PT_CHECK_FATAL(sched_yield());
			} while (0 /* TODO target thread flags encountered EINTR */);
			break;
		default:
			fatal("Unknown net state", 0);
	}

	PT_UL(&(c->pt_state_mutex));

	return ret;
}

static enum net_state_t
net_state_init(enum net_state_t o_state, struct connection *c)
{
	/* Initial thread state */

	(void) o_state;

	// TODO: install network thread signal handler for recv wakeup

	PT_LK(&(c->pt_state_mutex));
	PT_CHECK_FATAL(pthread_cond_signal(&init_cond));
	PT_CHECK_FATAL(pthread_cond_wait(&(c->pt_state_cond), &(c->pt_state_mutex)));
	PT_UL(&(c->pt_state_mutex));

	return NET_ST_CXNG;
}

static enum net_state_t
net_state_dxed(enum net_state_t o_state, struct connection *c)
{
	(void) o_state;

	PT_LK(&(c->pt_state_mutex));
	/* TODO: check forced state */
	PT_CHECK_FATAL(pthread_cond_wait(&(c->pt_state_cond), &(c->pt_state_mutex)));
	PT_UL(&(c->pt_state_mutex));

	return NET_ST_CXNG;
}

static enum net_state_t
net_state_rxng(enum net_state_t o_state, struct connection *c)
{
	(void) o_state;

	struct timespec ts;
	struct timeval  tv;

	const int delta_s = 3;

	// FIXME: clock_gettime
	gettimeofday(&tv, NULL);

	/* TODO: base/backoff */
	ts.tv_sec  = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	ts.tv_sec += delta_s;

	PT_LK(&(c->pt_state_mutex));
	/* TODO: check forced state */
	/* TODO: check non-timeout error */
	pthread_cond_timedwait(&(c->pt_state_cond), &(c->pt_state_mutex), &ts);
	PT_UL(&(c->pt_state_mutex));

	return NET_ST_CXNG;
}

static enum net_state_t
net_state_cxng(enum net_state_t o_state, struct connection *c)
{
	(void) o_state;

	int ret, soc;

	char errbuf[1024];
	char ipbuf[INET6_ADDRSTRLEN];

	PT_CB(net_cb_cxng(c->obj, "Connecting to %s:%s ...", c->host, c->port));

	struct addrinfo *p, *res, hints = {
		.ai_family   = AF_UNSPEC,
		.ai_flags    = AI_PASSIVE,
		.ai_socktype = SOCK_STREAM
	};

	if ((ret = getaddrinfo(c->host, c->port, &hints, &res))) {
		PT_CB(net_cb_fail(c->obj, "Connection error: %s", gai_strerror(ret)));
		return NET_ST_RXNG;
	}

	for (p = res; p != NULL; p = p->ai_next) {

		if ((soc = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			continue;

		if (connect(soc, p->ai_addr, p->ai_addrlen) == 0)
			break;

		if ((ret = errno) == EINTR) {
			/* Connection was interrupted  by signal, canceled */
			;
		} else {
			/* Connection failed for other reasons */
			net_close(c, &soc);
		}
	}

	if (p == NULL) {
		PT_CB(net_cb_fail(c->obj, "Connection failure [%s]", net_strerror(ret, errbuf, sizeof(errbuf))));
		freeaddrinfo(res);
		return NET_ST_RXNG;
	}

	// FIXME: calling net_cb_cxed sends pass/user/nick
	if ((ret = getnameinfo(p->ai_addr, p->ai_addrlen, ipbuf, INET6_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST))) {
		// ; net_cb_cxed(c->obj, "Connected to %s [IP lookup failure: %s]", c->host, gai_strerror(ret));
	} else {
		// ; net_cb_cxed(c->obj, "Connected to %s [%s]", c->host, ipbuf);
	}

	freeaddrinfo(res);
	c->soc = soc;

	return NET_ST_CXED;
}

static enum net_state_t
net_state_cxed(enum net_state_t o_state, struct connection *c)
{
	(void) o_state;

	PT_CB(net_cb_cxed(c->obj, "Connected to %s [TODO: ip]", c->host));

	struct timeval tv = {
		.tv_sec = 5
	};

	ssize_t i, ret;

	// TODO: testing the timeout of receiving a signal.... does it reset the 5 seconds?
	if (setsockopt(c->soc, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		{ fatal("setsockopt", errno); }

	char recvbuf[512];

	for (;;) {

		if ((ret = recv(c->soc, recvbuf, sizeof(recvbuf), 0)) <= 0) {

			if (ret == 0)
				{ ; }

			if (errno == EAGAIN || errno == EWOULDBLOCK)
				{ fatal("timed out", 0); }

			if (errno == ECONNRESET)
				{ ; } /* forcibly closed by peer */

			if (errno == EINTR)
				{ ; } /* signal, affects ping timeout */

			/* TODO: all other errnos -> socket close */

			fatal("other error", 0);
		}

		for (i = 0; i < ret; i++) {

			if (recvbuf[i] == '\n')
				continue;

			if (recvbuf[i] == '\r') {
				if (c->read.i && c->read.i <= NET_MESG_LEN) {
					c->read.buf[c->read.i + 1] = 0;
					PT_CB(net_cb_read_soc(c->read.buf, c->read.i, c->obj));
					c->read.i = 0;
				}
			} else {
				c->read.buf[c->read.i++] = recvbuf[i];
			}
		}
	}

	// FIXME
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
net_thread(void *arg)
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


		// XXX what happens if returning from rxng, and here, but aren't waiting
		// on cond anymore, and cond signal is called?

		c->state = n_state;

	} while (c->state != NET_ST_TERM);

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
net_loop(void)
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



	// TODO: rename net_read() or something?



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

			PT_CB(net_cb_read_inp(buff, n));
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

	if (err >= 0 && (size_t)err < ELEMS(err_strs))
		return err_strs[err];

	return NULL;
}
