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
#include <termios.h>

#include <unistd.h>
#include <pthread.h>

#include "config.h"
#include "src/io.h"
#include "utils/utils.h"

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
#define IO_MESG_LEN 510

#ifndef IO_PING_MIN
	#define IO_PING_MIN 150
#elif (IO_PING_MIN < 0 || IO_PING_MIN > 86400)
	#error "IO_PING_MIN: [0, 86400]"
#endif

#ifndef IO_PING_REFRESH
	#define IO_PING_REFRESH 5
#elif (IO_PING_REFRESH < 0 || IO_PING_REFRESH > 86400)
	#error "IO_PING_REFRESH: [0, 86400]"
#endif

#ifndef IO_PING_MAX
	#define IO_PING_MAX 300
#elif (IO_PING_MAX < 0 || IO_PING_MAX > 86400)
	#error "IO_PING_MAX: [0, 86400]"
#endif

#ifndef IO_RECONNECT_BACKOFF_BASE
	#define IO_RECONNECT_BACKOFF_BASE 10
#elif (IO_RECONNECT_BACKOFF_BASE < 1 || IO_RECONNECT_BACKOFF_BASE > 86400)
	#error "IO_RECONNECT_BACKOFF_BASE: [1, 32]"
#endif

#ifndef IO_RECONNECT_BACKOFF_FACTOR
	#define IO_RECONNECT_BACKOFF_FACTOR 2
#elif (IO_RECONNECT_BACKOFF_FACTOR < 1 || IO_RECONNECT_BACKOFF_FACTOR > 32)
	#error "IO_RECONNECT_BACKOFF_FACTOR: [1, 32]"
#endif

#ifndef IO_RECONNECT_BACKOFF_MAX
	#define IO_RECONNECT_BACKOFF_MAX 86400
#elif (IO_RECONNECT_BACKOFF_MAX < 1 || IO_RECONNECT_BACKOFF_MAX > 86400)
	#error "IO_RECONNECT_BACKOFF_MAX: [0, 86400]"
#endif

#define PT_CF(X) do { int ret; if ((ret = (X)) != 0) fatal((#X), ret); } while (0)
#define PT_LK(X) PT_CF(pthread_mutex_lock((X)))
#define PT_UL(X) PT_CF(pthread_mutex_unlock((X)))
#define PT_CB(X) do { PT_LK(&cb_mutex); (X); PT_UL(&cb_mutex); } while (0)

enum io_err_t
{
	IO_ERR_NONE,
	IO_ERR_TRUNC,
	IO_ERR_DXED,
	IO_ERR_CXNG,
	IO_ERR_CXED
};

struct connection {
	const void *obj;
	const char *host;
	const char *port;
	enum io_state_t {
		IO_ST_INIT, /* Initial thread state */
		IO_ST_DXED, /* Socket disconnected, passive */
		IO_ST_RXNG, /* Socket disconnected, pending reconnect */
		IO_ST_CXNG, /* Socket connection in progress */
		IO_ST_CXED, /* Socket connected */
		IO_ST_PING, /* Socket connected, network state in question */
		IO_ST_TERM, /* Terminal thread state */
		IO_ST_SIZE
	} state;
	int soc;
	pthread_cond_t  pt_state_cond;
	pthread_mutex_t pt_state_mutex;
	pthread_t       pt_tid;
	struct {
		size_t i;
		char buf[IO_MESG_LEN + 1];
	} read;
	volatile enum io_force_state {
		IO_ST_FORCE_NONE,
		IO_ST_FORCE_CXNG, /* Asynchronously force connection */
		IO_ST_FORCE_DXED, /* Asynchronously force disconnect */
	} force_state;
	char ipbuf[INET6_ADDRSTRLEN];
};

static void* io_thread(void*);
static char* io_strerror(int, char*, size_t);

static void io_close(struct connection*);
static void io_init(void);
static void io_term(void);

static enum io_state_t io_state_init(enum io_state_t, struct connection*);
static enum io_state_t io_state_dxed(enum io_state_t, struct connection*);
static enum io_state_t io_state_rxng(enum io_state_t, struct connection*);
static enum io_state_t io_state_cxng(enum io_state_t, struct connection*);
static enum io_state_t io_state_cxed(enum io_state_t, struct connection*);
static enum io_state_t io_state_ping(enum io_state_t, struct connection*);
static void io_state_term(enum io_state_t, struct connection*);

static void io_init_sig(void);
static void io_init_tty(void);
static void io_term_tty(void);

static struct termios term;
static pthread_cond_t init_cond;
static pthread_mutex_t cb_mutex;
static pthread_mutex_t init_mutex;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static volatile sig_atomic_t flag_sigwinch;

static void
io_close(struct connection *c)
{
	/* Lock thread state to prevent ambiguous handling of EINTR on close() */
	if (c->soc >= 0) {
		PT_LK(&(c->pt_state_mutex));
		PT_CF(close(c->soc));
		PT_UL(&(c->pt_state_mutex));
		c->soc = -1;
	}
}

static char*
io_strerror(int errnum, char *buf, size_t buflen)
{
	PT_CF(strerror_r(errnum, buf, buflen));
	return buf;
}

static void
io_init(void)
{
	pthread_mutexattr_t m_attr;

	PT_CF(pthread_mutexattr_init(&m_attr));
	PT_CF(pthread_mutexattr_settype(&m_attr, PTHREAD_MUTEX_ERRORCHECK));

	PT_CF(pthread_cond_init(&init_cond, NULL));
	PT_CF(pthread_mutex_init(&cb_mutex, &m_attr));
	PT_CF(pthread_mutex_init(&init_mutex, &m_attr));

	PT_CF(pthread_mutexattr_destroy(&m_attr));

	if (atexit(io_term) != 0)
		fatal("atexit", 0);
}

static void
io_term(void)
{
	int ret;

	if ((ret = pthread_mutex_trylock(&cb_mutex)) < 0 && ret != EBUSY)
		fatal("pthread_mutex_trylock", ret);

	PT_UL(&cb_mutex);
	PT_CF(pthread_cond_destroy(&init_cond));
	PT_CF(pthread_mutex_destroy(&cb_mutex));
	PT_CF(pthread_mutex_destroy(&init_mutex));
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
	c->state = IO_ST_INIT;

	pthread_attr_t      t_attr;
	pthread_mutexattr_t m_attr;

	PT_CF(pthread_attr_init(&t_attr));
	PT_CF(pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED));

	PT_CF(pthread_mutexattr_init(&m_attr));
	PT_CF(pthread_mutexattr_settype(&m_attr, PTHREAD_MUTEX_ERRORCHECK));

	PT_CF(pthread_cond_init(&(c->pt_state_cond), NULL));
	PT_CF(pthread_mutex_init(&(c->pt_state_mutex), &m_attr));

	PT_CF(pthread_once(&init_once, io_init));

	/* Wait for thread to reach initialized and waiting state */
	PT_LK(&init_mutex);
	PT_CF(pthread_create(&(c->pt_tid), &t_attr, io_thread, c));
	/* TODO: check spurrious wakeups */
	PT_CF(pthread_cond_wait(&init_cond, &init_mutex));
	PT_UL(&init_mutex);

	PT_CF(pthread_attr_destroy(&t_attr));
	PT_CF(pthread_mutexattr_destroy(&m_attr));

	return c;
}

int
io_sendf(struct connection *c, const char *fmt, ...)
{
	char sendbuf[512];

	va_list ap;
	int ret;
	size_t len;

	if (c->state != IO_ST_CXED && c->state != IO_ST_PING)
		return IO_ERR_DXED;

	va_start(ap, fmt);
	ret = vsnprintf(sendbuf, sizeof(sendbuf) - 2, fmt, ap);
	va_end(ap);

	if (ret <= 0) {
		return IO_ERR_NONE; /* TODO handle error */
	}

	len = ret;

	if (len >= sizeof(sendbuf) - 2)
		return IO_ERR_TRUNC;

	sendbuf[len++] = '\r';
	sendbuf[len++] = '\n';

	if (send(c->soc, sendbuf, len, 0) < 0) {
		return IO_ERR_NONE; /* TODO: handle error */
	}

	return IO_ERR_NONE;
}

int
io_cx(struct connection *c)
{
	/* Force a socket thread into IO_ST_CXNG state
	 *
	 * Valid only for states blocked on:
	 *   - IO_ST_INIT: pthread_cond_wait()
	 *   - IO_ST_DXED: pthread_cond_wait()
	 *   - IO_ST_RXNG: pthread_cond_timedwait()
	 */

	enum io_err_t ret = IO_ERR_NONE;

	PT_LK(&(c->pt_state_mutex));

	switch (c->state) {
		case IO_ST_INIT:
		case IO_ST_DXED:
		case IO_ST_RXNG:
			PT_CF(pthread_cond_signal(&(c->pt_state_cond)));
			break;
		case IO_ST_CXNG:
			ret = IO_ERR_CXNG;
			break;
		case IO_ST_CXED:
		case IO_ST_PING:
			ret = IO_ERR_CXED;
			break;
		default:
			fatal("Unknown net state", 0);
	}

	PT_UL(&(c->pt_state_mutex));

	return ret;
}

int
io_dx(struct connection *c)
{
	/* Force a socket thread into IO_ST_DXED state
	 *
	 * Valid only for states blocked on:
	 *   - IO_ST_RXNG: pthread_cond_timedwait()
	 *   - IO_ST_CXNG: connect()
	 *   - IO_ST_CXED: recv()
	 *   - IO_ST_PING: recv()
	 */

	enum io_err_t ret = IO_ERR_NONE;

	PT_LK(&(c->pt_state_mutex));

	switch (c->state) {
		case IO_ST_INIT:
		case IO_ST_DXED:
			ret = IO_ERR_DXED;
			break;
		case IO_ST_RXNG:
			PT_CF(pthread_cond_signal(&(c->pt_state_cond)));
			break;
		case IO_ST_CXNG:
		case IO_ST_CXED:
		case IO_ST_PING:
			do {
				/* Signal and yield the cpu until the target thread
				 * is flagged as having reached an EINTR handler */
				PT_CF(pthread_kill(c->pt_tid, SIGUSR1));
				PT_CF(sched_yield());
			} while (0 /* TODO */);
			break;
		default:
			fatal("Unknown net state", 0);
	}

	PT_UL(&(c->pt_state_mutex));

	return ret;
}

static enum io_state_t
io_state_init(enum io_state_t o_state, struct connection *c)
{
	/* Initial thread state */

	(void) o_state;

	sigset_t set;
	sigfillset(&set);
	sigdelset(&set, SIGUSR1);
	PT_CF(pthread_sigmask(SIG_BLOCK, &set, NULL));

	PT_LK(&(c->pt_state_mutex));
	PT_LK(&init_mutex);
	PT_UL(&init_mutex);
	PT_CF(pthread_cond_signal(&init_cond));
	/* TODO: check spurrious wakeups */
	PT_CF(pthread_cond_wait(&(c->pt_state_cond), &(c->pt_state_mutex)));
	PT_UL(&(c->pt_state_mutex));

	return IO_ST_CXNG;
}

static enum io_state_t
io_state_dxed(enum io_state_t o_state, struct connection *c)
{
	(void) o_state;

	PT_LK(&(c->pt_state_mutex));
	/* TODO: check forced state */
	/* TODO: check spurrious wakeups */
	PT_CF(pthread_cond_wait(&(c->pt_state_cond), &(c->pt_state_mutex)));
	PT_UL(&(c->pt_state_mutex));

	return IO_ST_CXNG;
}

static enum io_state_t
io_state_rxng(enum io_state_t o_state, struct connection *c)
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
	/* TODO: check spurrious wakeups */
	pthread_cond_timedwait(&(c->pt_state_cond), &(c->pt_state_mutex), &ts);
	PT_UL(&(c->pt_state_mutex));

	return IO_ST_CXNG;
}

static enum io_state_t
io_state_cxng(enum io_state_t o_state, struct connection *c)
{
	(void) o_state;

	int ret, soc;

	char errbuf[1024];
	char ipbuf[INET6_ADDRSTRLEN];

	PT_CB(io_cb_cxng(c->obj, "Connecting to %s:%s ...", c->host, c->port));

	struct addrinfo *p, *res, hints = {
		.ai_family   = AF_UNSPEC,
		.ai_flags    = AI_PASSIVE,
		.ai_socktype = SOCK_STREAM
	};

	if ((ret = getaddrinfo(c->host, c->port, &hints, &res))) {
		PT_CB(io_cb_fail(c->obj, "Connection error: %s", gai_strerror(ret)));
		return IO_ST_RXNG;
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
			io_close(c);
		}
	}

	if (p == NULL) {
		PT_CB(io_cb_fail(c->obj, "Connection failure [%s]", io_strerror(ret, errbuf, sizeof(errbuf))));
		freeaddrinfo(res);
		return IO_ST_RXNG;
	}

	// FIXME: calling io_cb_cxed sends pass/user/nick
	if ((ret = getnameinfo(p->ai_addr, p->ai_addrlen, ipbuf, INET6_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST))) {
		// ; io_cb_cxed(c->obj, "Connected to %s [IP lookup failure: %s]", c->host, gai_strerror(ret));
	} else {
		// ; io_cb_cxed(c->obj, "Connected to %s [%s]", c->host, ipbuf);
	}

	freeaddrinfo(res);
	c->soc = soc;

	return IO_ST_CXED;
}

static enum io_state_t
io_state_cxed(enum io_state_t o_state, struct connection *c)
{
	(void) o_state;

	PT_CB(io_cb_cxed(c->obj, "Connected to %s [TODO: ip]", c->host));

	struct timeval tv = {
		.tv_sec = 3
	};

	ssize_t i, ret;

	if (setsockopt(c->soc, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		{ fatal("setsockopt", errno); }

	char recvbuf[512];

	for (;;) {

		if ((ret = recv(c->soc, recvbuf, sizeof(recvbuf), 0)) <= 0) {

			if (ret == 0) {
				PT_CB(io_cb_lost(c->obj, "Connection closed"));
				return IO_ST_CXNG;
			}

			if (errno == ECONNRESET) {
				PT_CB(io_cb_lost(c->obj, "Connection forcibly reset by peer"));
				return IO_ST_CXNG;
			}

			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				PT_CB(io_cb_ping(c->obj, 0));
				return IO_ST_PING;
			}

			if (errno == EINTR) {
				return IO_ST_DXED;
			}

			fatal("recv", errno);
		}

		for (i = 0; i < ret; i++) {

			if (recvbuf[i] == '\n')
				continue;

			if (recvbuf[i] == '\r') {
				if (c->read.i && c->read.i <= IO_MESG_LEN) {
					c->read.buf[c->read.i + 1] = 0;
					PT_CB(io_cb_read_soc(c->read.buf, c->read.i, c->obj));
					c->read.i = 0;
				}
			} else {
				c->read.buf[c->read.i++] = recvbuf[i];
			}
		}
	}
}

static enum io_state_t
io_state_ping(enum io_state_t o_state, struct connection *c)
{
	struct timeval tv = {
		.tv_sec = 2
	};

	ssize_t i, ret;

	if (setsockopt(c->soc, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		{ fatal("setsockopt", errno); }

	char recvbuf[512];

	unsigned ping = 0;

	for (;;) {

		if ((ret = recv(c->soc, recvbuf, sizeof(recvbuf), 0)) <= 0) {

			if (ret == 0) {
				PT_CB(io_cb_lost(c->obj, "Connection closed"));
				return IO_ST_CXNG;
			}

			if (errno == ECONNRESET) {
				PT_CB(io_cb_lost(c->obj, "Connection forcibly reset by peer"));
				return IO_ST_CXNG;
			}

			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if ((ping += 2) >= 10) {
					// TODO: close the socket, etc
					PT_CB(io_cb_lost(c->obj, "Connection lost: ping timeout"));
					io_close(c);
					return IO_ST_CXNG;
				} else {
					PT_CB(io_cb_ping(c->obj, ping));
					continue;
				}
			}

			if (errno == EINTR) {
				return IO_ST_DXED;
			}

			fatal("recv", errno);
		}

		for (i = 0; i < ret; i++) {

			if (recvbuf[i] == '\n')
				continue;

			if (recvbuf[i] == '\r') {
				if (c->read.i && c->read.i <= IO_MESG_LEN) {
					c->read.buf[c->read.i + 1] = 0;
					PT_CB(io_cb_read_soc(c->read.buf, c->read.i, c->obj));
					c->read.i = 0;
				}
			} else {
				c->read.buf[c->read.i++] = recvbuf[i];
			}
		}

		return IO_ST_CXED;
	}
}

static void
io_state_term(enum io_state_t o_state, struct connection *c)
{
	(void) o_state;

	pthread_cond_destroy(&(c->pt_state_cond));
	pthread_mutex_destroy(&(c->pt_state_mutex));

	free((void*)c->host);
	free((void*)c->port);
	free(c);

	pthread_exit(EXIT_SUCCESS);
}

static void*
io_thread(void *arg)
{
	struct connection *c = arg;

	enum io_state_t n_state;

	for (;;) {

		/* TODO:
		 *
		 * here we know the old state and we know the new state, we can call the informational
		 * transition callback */

		switch (c->state) {
			case IO_ST_INIT:
				n_state = io_state_init(c->state, c);
				break;
			case IO_ST_DXED:
				n_state = io_state_dxed(c->state, c);
				break;
			case IO_ST_CXNG:
				n_state = io_state_cxng(c->state, c);
				break;
			case IO_ST_RXNG:
				n_state = io_state_rxng(c->state, c);
				break;
			case IO_ST_CXED:
				n_state = io_state_cxed(c->state, c);
				break;
			case IO_ST_PING:
				n_state = io_state_ping(c->state, c);
				break;
			case IO_ST_TERM:
				io_state_term(c->state, c);
				break;
			default:
				fatal("Unknown net state", 0);
		}

		// TODO: here lock mutext before calling next state
		// and check through volatile pointer if a force state was given


		// XXX what happens if returning from rxng, and here, but aren't waiting
		// on cond anymore, and cond signal is called?

		c->state = n_state;
	}

	/* Not reached */
	return NULL;
}

void
io_free(struct connection *c)
{
	/* TODO */
	(void)c;
}

static void
sigaction_sigwinch(int sig)
{
	UNUSED(sig);
	flag_sigwinch = 1;
}

static void
io_init_sig(void)
{
	struct sigaction sa;

	sa.sa_handler = sigaction_sigwinch;
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGWINCH, &sa, NULL) < 0)
		fatal("sigaction - SIGWINCH", errno);
}

static void
io_init_tty(void)
{
	struct termios nterm;

	if (isatty(STDIN_FILENO) == 0)
		fatal("isatty", errno);

	if (tcgetattr(STDIN_FILENO, &term) < 0)
		fatal("tcgetattr", errno);

	nterm = term;
	nterm.c_lflag &= ~(ECHO | ICANON | ISIG);
	nterm.c_cc[VMIN]  = 1;
	nterm.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSANOW, &nterm) < 0)
		fatal("tcsetattr", errno);

	if (atexit(io_term_tty) < 0)
		fatal("atexit", 0);
}

static void
io_term_tty(void)
{
	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &term) < 0)
		fatal("tcsetattr", errno);
}

void
io_loop(void (*io_loop_cb)(void))
{
	PT_CF(pthread_once(&init_once, io_init));

	io_init_sig();
	io_init_tty();

	for (;;) {
		char buf[512];
		ssize_t ret = read(STDIN_FILENO, buf, sizeof(buf));

		if (ret > 0)
			PT_CB(io_cb_read_inp(buf, ret));

		if (ret <= 0) {
			if (errno == EINTR) {
				if (flag_sigwinch) {
					flag_sigwinch = 0;
					PT_CB(io_cb_signal(SIGWINCH));
				}
			} else {
				fatal("read", ret ? errno : 0);
			}
		}

		if (io_loop_cb)
			io_loop_cb();
	}
}

const char*
io_err(int err)
{
	const char *err_strs[] = {
		[IO_ERR_TRUNC] = "data truncated",
		[IO_ERR_DXED]  = "socket not connected",
		[IO_ERR_CXNG]  = "socket connection in progress",
		[IO_ERR_CXED]  = "socket connected"
	};

	const char *err_str = NULL;

	if (err >= 0 && (size_t)err < ELEMS(err_strs))
		err_str = err_strs[err];

	return err_str ? err_str : "unknown error";
}
