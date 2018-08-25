#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "config.h"
#include "src/io.h"
#include "utils/utils.h"

/* RFC 2812, section 2.3 */
#define IO_MESG_LEN 512

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
	#define IO_RECONNECT_BACKOFF_BASE 4
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

#define PT_CF(X) do { int ret; if ((ret = (X)) < 0) fatal((#X), ret); } while (0)
#define PT_LK(X) PT_CF(pthread_mutex_lock((X)))
#define PT_UL(X) PT_CF(pthread_mutex_unlock((X)))
#define PT_CB(R, ...) \
	do { \
		PT_LK(&cb_mutex); \
		io_cb((R), __VA_ARGS__); \
		PT_UL(&cb_mutex); \
	} while (0)

enum io_err_t
{
	IO_ERR_NONE,
	IO_ERR_TRUNC,
	IO_ERR_DXED,
	IO_ERR_CXNG,
	IO_ERR_CXED,
	IO_ERR_TERM
};

struct io_lock
{
	pthread_cond_t  cnd;
	pthread_mutex_t mtx;
	volatile int predicate;
};

struct connection
{
	const void *obj;
	const char *host;
	const char *port;
	enum io_state_t {
		IO_ST_INVALID,
		IO_ST_DXED, /* Socket disconnected, passive */
		IO_ST_RXNG, /* Socket disconnected, pending reconnect */
		IO_ST_CXNG, /* Socket connection in progress */
		IO_ST_CXED, /* Socket connected */
		IO_ST_PING, /* Socket connected, network state in question */
		IO_ST_TERM /* Terminal thread state */
	} state_c, /* current thread state */
	  state_f; /* forced thread state */
	char ip[INET6_ADDRSTRLEN];
	int soc;
	pthread_t pt_tid;
	struct {
		size_t i;
		char buf[IO_MESG_LEN];
	} read;
	struct io_lock lock;
	unsigned rx_backoff;
};

static char* io_strerror(int, char*, size_t);
static struct winsize* io_tty_winsize(void);
static void io_close(int*);
static void io_shutdown(int);
static void io_init(void);
static void io_init_sig(void);
static void io_init_tty(void);
static void io_recv(struct connection*, char*, size_t);
static void io_term(void);
static void io_term_tty(void);
static void* io_thread(void*);
static void io_state_force(struct connection*, enum io_state_t);

static enum io_state_t io_state_dxed(struct connection*);
static enum io_state_t io_state_rxng(struct connection*);
static enum io_state_t io_state_cxng(struct connection*);
static enum io_state_t io_state_cxed(struct connection*);
static enum io_state_t io_state_ping(struct connection*);

static pthread_mutex_t cb_mutex;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static struct termios term;

static volatile sig_atomic_t flag_sigwinch_cb; /* sigwinch callback */
static volatile sig_atomic_t flag_tty_resized; /* sigwinch ws resize */

static void io_net_set_timeout(struct connection*, unsigned);

static void io_lock_init(struct io_lock*);
static void io_lock_term(struct io_lock*);
static void io_lock_wake(struct io_lock*);
static void io_lock_wait(struct io_lock*, struct timespec*);

static char*
io_strerror(int errnum, char *buf, size_t buflen)
{
	PT_CF(strerror_r(errnum, buf, buflen));
	return buf;
}

static void
io_close(int *soc)
{
	if (*soc >= 0 && close(*soc) < 0) {
		fatal("close", errno);
	}

	*soc = -1;
}

static void
io_shutdown(int soc)
{
	if (soc >= 0 && shutdown(soc, SHUT_RDWR) < 0) {
		fatal("shutdown", errno);
	}
}

static void
io_init(void)
{
	pthread_mutexattr_t m_attr;

	PT_CF(pthread_mutexattr_init(&m_attr));
	PT_CF(pthread_mutexattr_settype(&m_attr, PTHREAD_MUTEX_ERRORCHECK));
	PT_CF(pthread_mutex_init(&cb_mutex, &m_attr));
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
	PT_CF(pthread_mutex_destroy(&cb_mutex));
}

static void
io_lock_init(struct io_lock *lock)
{
	pthread_mutexattr_t m_attr;

	PT_CF(pthread_mutexattr_init(&m_attr));
	PT_CF(pthread_mutexattr_settype(&m_attr, PTHREAD_MUTEX_RECURSIVE));

	PT_CF(pthread_cond_init(&(lock->cnd), NULL));
	PT_CF(pthread_mutex_init(&(lock->mtx), &m_attr));

	PT_CF(pthread_mutexattr_destroy(&m_attr));

	lock->predicate = 0;
}

static void
io_lock_term(struct io_lock *lock)
{
	PT_CF(pthread_cond_destroy(&(lock->cnd)));
	PT_CF(pthread_mutex_destroy(&(lock->mtx)));
}

static void
io_lock_wait(struct io_lock *lock, struct timespec *timeout)
{
	PT_LK(&(lock->mtx));

	int ret = 0;

	while (lock->predicate == 0 && ret == 0) {
		if (timeout) {
			ret = pthread_cond_timedwait(&(lock->cnd), &(lock->mtx), timeout);
		} else {
			ret = pthread_cond_wait(&(lock->cnd), &(lock->mtx));
		}
	}

	if (ret && (timeout == NULL || ret != ETIMEDOUT))
		fatal("io_lock_wait", ret);

	lock->predicate = 0;

	PT_UL(&(lock->mtx));
}

static void
io_lock_wake(struct io_lock *lock)
{
	PT_LK(&(lock->mtx));
	lock->predicate = 1;
	PT_CF(pthread_cond_signal(&(lock->cnd)));
	PT_UL(&(lock->mtx));
}

struct connection*
connection(const void *obj, const char *host, const char *port)
{
	struct connection *c;

	if ((c = calloc(1U, sizeof(*c))) == NULL)
		fatal("malloc", errno);

	c->obj = obj;
	c->host = strdup(host);
	c->port = strdup(port);
	c->state_c = IO_ST_DXED;
	c->state_f = IO_ST_INVALID;
	io_lock_init(&(c->lock));

	PT_CF(pthread_once(&init_once, io_init));

	pthread_attr_t t_attr;

	PT_CF(pthread_attr_init(&t_attr));
	PT_CF(pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED));
	PT_CF(pthread_create(&(c->pt_tid), &t_attr, io_thread, c));
	PT_CF(pthread_attr_destroy(&t_attr));

	return c;
}

int
io_sendf(struct connection *c, const char *fmt, ...)
{
	char sendbuf[512];

	va_list ap;
	int ret;
	size_t len;

	if (c->state_c != IO_ST_CXED && c->state_c != IO_ST_PING)
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

	DEBUG_MSG("send: (%zu) %s", len, sendbuf);

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
	/* Force a socket thread into IO_ST_CXNG state */

	enum io_err_t err = IO_ERR_NONE;

	PT_LK(&(c->lock.mtx));

	switch (c->state_c) {
		case IO_ST_CXNG: err = IO_ERR_CXNG; break;
		case IO_ST_CXED: err = IO_ERR_CXED; break;
		case IO_ST_PING: err = IO_ERR_CXED; break;
		case IO_ST_TERM: err = IO_ERR_TERM; break;
		default:
			io_state_force(c, IO_ST_CXNG);
	}

	PT_UL(&(c->lock.mtx));

	return err;
}

int
io_dx(struct connection *c)
{
	/* Force a socket thread into IO_ST_DXED state */

	enum io_err_t err = IO_ERR_NONE;

	PT_LK(&(c->lock.mtx));

	switch (c->state_c) {
		case IO_ST_DXED: err = IO_ERR_DXED; break;
		case IO_ST_TERM: err = IO_ERR_TERM; break;
		default:
			io_state_force(c, IO_ST_DXED);
	}

	PT_UL(&(c->lock.mtx));

	return err;
}

void
io_free(struct connection *c)
{
	/* Force a socket thread into the IO_ST_TERM state */

	PT_LK(&(c->lock.mtx));
	io_state_force(c, IO_ST_TERM);
	PT_UL(&(c->lock.mtx));
}

static void
io_state_force(struct connection *c, enum io_state_t state_f)
{
	/* Wake and force a connection thread's state */

	c->state_f = state_f;

	switch (c->state_c) {
		case IO_ST_DXED: /* io_lock_wait() */
		case IO_ST_RXNG: /* io_lock_wait() */
			io_lock_wake(&(c->lock));
			break;
		case IO_ST_CXNG: /* connect() */
		case IO_ST_CXED: /* recv() */
		case IO_ST_PING: /* recv() */
			io_shutdown(c->soc);
			break;
		case IO_ST_TERM:
			break;
		default:
			fatal("Unknown net state", 0);
	}
}

static enum io_state_t
io_state_dxed(struct connection *c)
{
	io_lock_wait(&c->lock, NULL);

	return IO_ST_CXNG;
}

static enum io_state_t
io_state_rxng(struct connection *c)
{
	struct timespec ts;

	if (c->rx_backoff == 0) {
		c->rx_backoff = IO_RECONNECT_BACKOFF_BASE;
	} else {
		c->rx_backoff = MIN(
			IO_RECONNECT_BACKOFF_FACTOR * c->rx_backoff,
			IO_RECONNECT_BACKOFF_MAX
		);
	}

	PT_CB(IO_CB_INFO, c->obj, "Attemping reconnect in %02u:%02u",
		(c->rx_backoff / 60),
		(c->rx_backoff % 60));

	if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
		fatal("clock_gettime", errno);

	ts.tv_sec += c->rx_backoff;

	io_lock_wait(&c->lock, &ts);

	return IO_ST_CXNG;
}

static enum io_state_t
io_state_cxng(struct connection *c)
{
	/* TODO: handle shutdown() on socket at all points, will fatal for now */
	/* TODO: mutex should protect access to c->soc, else race condition
	 *       when the main thread tries to shutdown() for cancel */
	/* TODO: how to cancel getaddrinfo/getnameinfo? */

	char errbuf[1024];
	int ret, soc = -1;

	struct addrinfo *p, *res, hints = {
		.ai_family   = AF_UNSPEC,
		.ai_flags    = AI_PASSIVE,
		.ai_protocol = IPPROTO_TCP,
		.ai_socktype = SOCK_STREAM
	};

	PT_CB(IO_CB_INFO, c->obj, "Connecting to %s:%s ...", c->host, c->port);

	if ((ret = getaddrinfo(c->host, c->port, &hints, &res))) {

		if (ret == EAI_SYSTEM)
			PT_CB(IO_CB_ERR, c->obj, "Error resolving host: %s", io_strerror(errno, errbuf, sizeof(errbuf)));
		else
			PT_CB(IO_CB_ERR, c->obj, "Error resolving host: %s", gai_strerror(ret));

		return IO_ST_RXNG;
	}

	for (p = res; p != NULL; p = p->ai_next) {

		if ((soc = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			continue;

		if (connect(soc, p->ai_addr, p->ai_addrlen) == 0)
			break;

		io_close(&soc);
	}

	if (p == NULL) {
		PT_CB(IO_CB_ERR, c->obj, "Error connecting: %s", io_strerror(errno, errbuf, sizeof(errbuf)));
		freeaddrinfo(res);
		return IO_ST_RXNG;
	}

	c->soc = soc;

	if (getnameinfo(p->ai_addr, p->ai_addrlen, c->ip, sizeof(c->ip), NULL, 0, NI_NUMERICHOST)) {

		if (ret == EAI_SYSTEM)
			PT_CB(IO_CB_ERR, c->obj, "Error resolving numeric host: %s", io_strerror(errno, errbuf, sizeof(errbuf)));
		else
			PT_CB(IO_CB_ERR, c->obj, "Error resolving numeric host: %s", gai_strerror(ret));

		*c->ip = 0;
	}

	freeaddrinfo(res);

	return IO_ST_CXED;
}

static enum io_state_t
io_state_cxed(struct connection *c)
{
	io_net_set_timeout(c, IO_PING_MIN);

	char errbuf[1024];
	char recvbuf[IO_MESG_LEN];
	ssize_t ret;

	while ((ret = recv(c->soc, recvbuf, sizeof(recvbuf), 0)) > 0)
		io_recv(c, recvbuf, (size_t) ret);

	if (errno == EAGAIN || errno == EWOULDBLOCK) {
		return IO_ST_PING;
	}

	if (ret == 0) {
		PT_CB(IO_CB_DXED, c->obj, "Connection closed");
	} else if (errno == EPIPE || errno == ECONNRESET) {
		PT_CB(IO_CB_DXED, c->obj, "Connection closed by peer");
	} else {
		PT_CB(IO_CB_DXED, c->obj, "recv error:", io_strerror(errno, errbuf, sizeof(errbuf)));
	}

	io_close(&(c->soc));

	return (ret == 0 ? IO_ST_DXED : IO_ST_CXNG);
}

static enum io_state_t
io_state_ping(struct connection *c)
{
	io_net_set_timeout(c, IO_PING_REFRESH);

	char errbuf[1024];
	char recvbuf[IO_MESG_LEN];
	ssize_t ret;
	unsigned ping = IO_PING_MIN;

	for (;;) {

		if ((ret = recv(c->soc, recvbuf, sizeof(recvbuf), 0)) > 0) {
			io_recv(c, recvbuf, (size_t) ret);
			return IO_ST_CXED;
		}

		if (ret == 0) {
			PT_CB(IO_CB_DXED, c->obj, "Connection closed");
		} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
			if ((ping += IO_PING_REFRESH) < IO_PING_MAX) {
				PT_CB(IO_CB_PING_N, c->obj, ping);
				continue;
			}
			PT_CB(IO_CB_DXED, c->obj, "Connection timeout (%u)", ping);
		} else {
			PT_CB(IO_CB_DXED, c->obj, "recv error:", io_strerror(errno, errbuf, sizeof(errbuf)));
		}

		break;
	}

	io_close(&(c->soc));

	return (ret == 0 ? IO_ST_DXED : IO_ST_CXNG);
}

static void*
io_thread(void *arg)
{
	struct connection *c = arg;

	sigset_t sigset;
	PT_CF(sigfillset(&sigset));
	PT_CF(pthread_sigmask(SIG_BLOCK, &sigset, NULL));

	for (;;) {

		enum io_state_t st_f, /* transition state from */
		                st_t; /* transition state to */

		enum io_state_t (*const st_fns[])(struct connection*) = {
			[IO_ST_INVALID] = NULL,
			[IO_ST_DXED]    = io_state_dxed,
			[IO_ST_CXNG]    = io_state_cxng,
			[IO_ST_RXNG]    = io_state_rxng,
			[IO_ST_CXED]    = io_state_cxed,
			[IO_ST_PING]    = io_state_ping,
			[IO_ST_TERM]    = NULL,
		};

		st_f = c->state_c;

		if (!ARR_ELEM(st_fns, st_f) || !st_fns[st_f])
			fatal("invalid state", 0);

		st_t = st_fns[st_f](c);

		PT_LK(&(c->lock.mtx));

		if (c->state_f != IO_ST_INVALID) {
			c->state_c = c->state_f;
			c->state_f = IO_ST_INVALID;
		} else {
			c->state_c = st_t;
		}

		PT_UL(&(c->lock.mtx));

		if (c->state_c == IO_ST_TERM)
			break;

		if (st_f == IO_ST_PING && st_t == IO_ST_CXED)
			PT_CB(IO_CB_PING_0, c->obj);

		if (st_f == IO_ST_CXED && st_t == IO_ST_PING)
			PT_CB(IO_CB_PING_1, c->obj, IO_PING_MIN);

		if (st_f == IO_ST_CXNG && st_t == IO_ST_CXED)
			PT_CB(IO_CB_CXED, c->obj, "Connected to %s [%s]", c->host, c->ip);

		if (st_t == IO_ST_DXED || (st_f == IO_ST_CXNG && st_t == IO_ST_CXED))
			c->rx_backoff = 0;
	}

	io_shutdown(c->soc);
	io_close(&(c->soc));
	io_lock_term(&(c->lock));

	free((void*)c->host);
	free((void*)c->port);
	free(c);

	return NULL;
}

static void
io_recv(struct connection *c, char *buf, size_t n)
{
	for (size_t i = 0; i < n; i++) {

		if (buf[i] == '\n' && c->read.i && c->read.buf[c->read.i - 1] == '\r') {
			c->read.buf[--c->read.i] = 0;

			if (c->read.i) {
				DEBUG_MSG("recv: (%zu) %s", c->read.i, c->read.buf);
				PT_LK(&cb_mutex);
				io_cb_read_soc(c->read.buf, c->read.i - 1, c->obj);
				PT_UL(&cb_mutex);
				c->read.i = 0;
			}
		} else {
			c->read.buf[c->read.i++] = buf[i];
		}
	}
}

static void
io_net_set_timeout(struct connection *c, unsigned timeout)
{
	struct timeval tv = {
		.tv_sec = timeout
	};

	if (setsockopt(c->soc, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		fatal("setsockopt", errno);
}

static void
sigaction_sigwinch(int sig)
{
	UNUSED(sig);

	flag_sigwinch_cb = 1;
	flag_tty_resized = 0;
}

static void
io_init_sig(void)
{
	struct sigaction sa;

	sa.sa_handler = sigaction_sigwinch;
	sa.sa_flags = 0;
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
io_loop(void)
{
	PT_CF(pthread_once(&init_once, io_init));

	io_init_sig();
	io_init_tty();

	for (;;) {
		char buf[512];
		ssize_t ret = read(STDIN_FILENO, buf, sizeof(buf));

		if (ret > 0) {
			PT_LK(&cb_mutex);
			io_cb_read_inp(buf, ret);
			PT_UL(&cb_mutex);
		}

		if (ret <= 0) {
			if (errno == EINTR) {
				if (flag_sigwinch_cb) {
					flag_sigwinch_cb = 0;
					PT_CB(IO_CB_SIGNAL, NULL, IO_SIGWINCH);
				}
			} else {
				fatal("read", ret ? errno : 0);
			}
		}
	}
}

static struct winsize*
io_tty_winsize(void)
{
	static struct winsize tty_ws;

	if (flag_tty_resized == 0) {

		if (ioctl(0, TIOCGWINSZ, &tty_ws) < 0)
			fatal("ioctl", errno);

		flag_tty_resized = 1;
	}

	return &tty_ws;
}

unsigned
io_tty_cols(void)
{
	return io_tty_winsize()->ws_col;
}

unsigned
io_tty_rows(void)
{
	return io_tty_winsize()->ws_row;
}

const char*
io_err(int err)
{
	const char *const err_strs[] = {
		[IO_ERR_NONE]  = "success",
		[IO_ERR_TRUNC] = "data truncated",
		[IO_ERR_DXED]  = "socket not connected",
		[IO_ERR_CXNG]  = "socket connection in progress",
		[IO_ERR_CXED]  = "socket connected",
		[IO_ERR_TERM]  = "thread is terminating"
	};

	const char *err_str = NULL;

	if (ARR_ELEM(err_strs, err))
		err_str = err_strs[err];

	return err_str ? err_str : "unknown error";
}
