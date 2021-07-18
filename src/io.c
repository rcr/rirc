#include "src/io.h"

#include "config.h"
#include "src/rirc.h"
#include "src/utils/utils.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

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

#define PT_CF(X) \
	do {                           \
		int _ptcf = (X);           \
		if (_ptcf < 0) {           \
			io_fatal((#X), _ptcf); \
		}                          \
	} while (0)

#define PT_LK(X) PT_CF(pthread_mutex_lock((X)))
#define PT_UL(X) PT_CF(pthread_mutex_unlock((X)))

/* IO callback */
#define IO_CB(X) \
	do { PT_LK(&io_cb_mutex); (X); PT_UL(&io_cb_mutex); } while (0)

#define io_cxed(C)       IO_CB(io_cb_cxed((C)->obj))
#define io_dxed(C)       IO_CB(io_cb_dxed((C)->obj))
#define io_error(C, ...) IO_CB(io_cb_error((C)->obj,  __VA_ARGS__))
#define io_info(C, ...)  IO_CB(io_cb_info((C)->obj, __VA_ARGS__))
#define io_ping(C, P)    IO_CB(io_cb_ping((C)->obj, P))

/* state transition */
#define ST_X(OLD, NEW) (((OLD) << 3) | (NEW))

enum io_err
{
	IO_ERR_NONE,
	IO_ERR_CXED,
	IO_ERR_CXNG,
	IO_ERR_DXED,
	IO_ERR_FMT,
	IO_ERR_SSL_WRITE,
	IO_ERR_THREAD,
	IO_ERR_TRUNC,
};

struct connection
{
	const void *obj;
	const char *host;
	const char *port;
	enum io_state {
		IO_ST_INVALID,
		IO_ST_DXED, /* Socket disconnected, passive */
		IO_ST_RXNG, /* Socket disconnected, pending reconnect */
		IO_ST_CXNG, /* Socket connection in progress */
		IO_ST_CXED, /* Socket connected */
		IO_ST_PING, /* Socket connected, network state in question */
	} st_cur, /* current thread state */
	  st_new; /* new thread state */
	mbedtls_ctr_drbg_context tls_ctr_drbg;
	mbedtls_entropy_context tls_entropy;
	mbedtls_net_context net_ctx;
	mbedtls_ssl_config tls_conf;
	mbedtls_ssl_context tls_ctx;
	mbedtls_x509_crt tls_x509_crt;
	pthread_mutex_t mtx;
	pthread_t tid;
	uint32_t flags;
	unsigned ping;
	unsigned rx_sleep;
};

static enum io_state io_state_cxed(struct connection*);
static enum io_state io_state_cxng(struct connection*);
static enum io_state io_state_ping(struct connection*);
static enum io_state io_state_rxng(struct connection*);
static int io_cx_read(struct connection*, uint32_t);
static void io_fatal(const char*, int);
static void io_sig_handle(int);
static void io_sig_init(void);
static void io_tty_init(void);
static void io_tty_term(void);
static void io_tty_winsize(void);
static void* io_thread(void*);

static int io_running;
static pthread_mutex_t io_cb_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct termios term;
static volatile sig_atomic_t flag_sigwinch_cb; /* sigwinch callback */

static const char* io_strerror(char*, size_t);
static int io_net_connect(struct connection*);
static void io_net_close(int);

/* TLS */
static const char* io_tls_err(int);
static int io_tls_establish(struct connection*);
static int io_tls_x509_vrfy(struct connection*);

const char *ca_cert_paths[] = {
	"/etc/ssl/ca-bundle.pem",
	"/etc/ssl/cert.pem",
	"/etc/ssl/certs/ca-certificates.crt",
	"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
	"/etc/pki/tls/cacert.pem",
	"/etc/pki/tls/certs/ca-bundle.crt",
};

struct connection*
connection(const void *obj, const char *host, const char *port, uint32_t flags)
{
	struct connection *cx;

	if ((cx = calloc(1U, sizeof(*cx))) == NULL)
		fatal("malloc: %s", strerror(errno));

	cx->obj = obj;
	cx->flags = flags;
	cx->host = strdup(host);
	cx->port = strdup(port);
	cx->st_cur = IO_ST_DXED;
	cx->st_new = IO_ST_INVALID;
	PT_CF(pthread_mutex_init(&(cx->mtx), NULL));

	return cx;
}

void
connection_free(struct connection *cx)
{
	PT_CF(pthread_mutex_destroy(&(cx->mtx)));
	free((void*)cx->host);
	free((void*)cx->port);
	free(cx);
}

int
io_cx(struct connection *cx)
{
	enum io_err err = IO_ERR_NONE;
	sigset_t sigset;
	sigset_t sigset_old;

	PT_LK(&(cx->mtx));

	switch (cx->st_cur) {
		case IO_ST_DXED:
			PT_CF(sigfillset(&sigset));
			PT_CF(pthread_sigmask(SIG_BLOCK, &sigset, &sigset_old));
			if (pthread_create(&(cx->tid), NULL, io_thread, cx) < 0)
				err = IO_ERR_THREAD;
			PT_CF(pthread_sigmask(SIG_SETMASK, &sigset_old, NULL));
			break;
		case IO_ST_CXNG:
			err = IO_ERR_CXNG;
			break;
		case IO_ST_CXED:
		case IO_ST_PING:
			err = IO_ERR_CXED;
			break;
		case IO_ST_RXNG:
			PT_CF(pthread_kill(cx->tid, SIGUSR1));
			break;
		default:
			fatal("unknown state");
	}

	PT_UL(&(cx->mtx));

	return err;
}

int
io_dx(struct connection *cx)
{
	enum io_err err = IO_ERR_NONE;

	if (cx->st_cur == IO_ST_DXED)
		return IO_ERR_DXED;

	PT_LK(&(cx->mtx));
	cx->st_new = IO_ST_DXED;
	PT_UL(&(cx->mtx));

	PT_CF(pthread_detach(cx->tid));
	PT_CF(pthread_kill(cx->tid, SIGUSR1));

	return err;
}

int
io_sendf(struct connection *cx, const char *fmt, ...)
{
	unsigned char sendbuf[IO_MESG_LEN + 2];
	int ret;
	size_t len;
	size_t written;
	va_list ap;

	if (cx->st_cur != IO_ST_CXED && cx->st_cur != IO_ST_PING)
		return IO_ERR_DXED;

	va_start(ap, fmt);
	ret = vsnprintf((char*)sendbuf, sizeof(sendbuf) - 2, fmt, ap);
	va_end(ap);

	if (ret <= 0)
		return IO_ERR_FMT;

	len = (size_t) ret;

	if (len >= sizeof(sendbuf) - 2)
		return IO_ERR_TRUNC;

	debug_send(len, sendbuf);

	sendbuf[len++] = '\r';
	sendbuf[len++] = '\n';

	ret = 0;
	written = 0;

	do {
		if (cx->flags & IO_TLS_ENABLED) {
			ret = mbedtls_ssl_write(&(cx->tls_ctx), sendbuf + ret, len - ret);
		} else {
			ret = mbedtls_net_send(&(cx->net_ctx), sendbuf + ret, len - ret);
		}

		if (ret >= 0)
			continue;

		switch (ret) {
			case MBEDTLS_ERR_SSL_WANT_READ:
			case MBEDTLS_ERR_SSL_WANT_WRITE:
				ret = 0;
				continue;
			default:
				io_dx(cx);
				io_cx(cx);
				return IO_ERR_SSL_WRITE;
		}
	} while ((written += ret) < len);

	return IO_ERR_NONE;
}

void
io_init(void)
{
	io_sig_init();
	io_tty_init();
}

void
io_start(void)
{
	io_running = 1;

	io_tty_winsize();

	while (io_running) {

		char buf[128];
		ssize_t ret = read(STDIN_FILENO, buf, sizeof(buf));

		if (ret > 0) {
			IO_CB(io_cb_read_inp(buf, ret));
		} else {
			if (errno == EINTR) {
				if (flag_sigwinch_cb) {
					flag_sigwinch_cb = 0;
					io_tty_winsize();
				}
			} else {
				fatal("read: %s", ret ? strerror(errno) : "EOF");
			}
		}
	}
}

void
io_stop(void)
{
	io_running = 0;
}

static void
io_tty_winsize(void)
{
	struct winsize tty_ws;

	if (ioctl(0, TIOCGWINSZ, &tty_ws) < 0)
		fatal("ioctl: %s", strerror(errno));

	IO_CB(io_cb_sigwinch(tty_ws.ws_col, tty_ws.ws_row));
}

const char*
io_err(int err)
{
	switch (err) {
		case IO_ERR_NONE:      return "success";
		case IO_ERR_CXED:      return "socket connected";
		case IO_ERR_CXNG:      return "socket connection in progress";
		case IO_ERR_DXED:      return "socket not connected";
		case IO_ERR_FMT:       return "failed to format message";
		case IO_ERR_THREAD:    return "failed to create thread";
		case IO_ERR_SSL_WRITE: return "ssl write failure";
		case IO_ERR_TRUNC:     return "data truncated";
		default:
			return "unknown error";
	}
}

static enum io_state
io_state_rxng(struct connection *cx)
{
	if (cx->rx_sleep == 0) {
		cx->rx_sleep = IO_RECONNECT_BACKOFF_BASE;
	} else {
		cx->rx_sleep = MIN(
			IO_RECONNECT_BACKOFF_FACTOR * cx->rx_sleep,
			IO_RECONNECT_BACKOFF_MAX
		);
	}

	io_info(cx, "Attemping reconnect in %02u:%02u",
		(cx->rx_sleep / 60),
		(cx->rx_sleep % 60));

	sleep(cx->rx_sleep);

	return IO_ST_CXNG;
}

static enum io_state
io_state_cxng(struct connection *cx)
{
	if ((io_net_connect(cx)) < 0)
		return IO_ST_RXNG;

	if ((cx->flags & IO_TLS_ENABLED) && io_tls_establish(cx) < 0)
		return IO_ST_RXNG;

	return IO_ST_CXED;
}

static enum io_state
io_state_cxed(struct connection *cx)
{
	int ret;

	while ((ret = io_cx_read(cx, SEC_IN_MS(IO_PING_MIN))) > 0)
		continue;

	if (ret == MBEDTLS_ERR_SSL_TIMEOUT)
		return IO_ST_PING;

	switch (ret) {
		case MBEDTLS_ERR_SSL_WANT_READ:
			break;
		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			io_info(cx, "connection closed gracefully");
			break;
		case MBEDTLS_ERR_NET_CONN_RESET:
		case 0:
			io_error(cx, "connection reset by peer");
			break;
		default:
			io_error(cx, "connection error");
			break;
	}

	mbedtls_net_free(&(cx->net_ctx));

	if (cx->flags & IO_TLS_ENABLED) {
		mbedtls_ctr_drbg_free(&(cx->tls_ctr_drbg));
		mbedtls_entropy_free(&(cx->tls_entropy));
		mbedtls_ssl_config_free(&(cx->tls_conf));
		mbedtls_ssl_free(&(cx->tls_ctx));
		mbedtls_x509_crt_free(&(cx->tls_x509_crt));
	}

	return IO_ST_CXNG;
}

static enum io_state
io_state_ping(struct connection *cx)
{
	int ret;

	if (cx->ping >= IO_PING_MAX)
		return IO_ST_CXNG;

	if ((ret = io_cx_read(cx, SEC_IN_MS(IO_PING_REFRESH))) > 0)
		return IO_ST_CXED;

	if (ret == MBEDTLS_ERR_SSL_TIMEOUT)
		return IO_ST_PING;

	switch (ret) {
		case MBEDTLS_ERR_SSL_WANT_READ:
			break;
		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			io_info(cx, "connection closed gracefully");
			break;
		case MBEDTLS_ERR_NET_CONN_RESET:
		case 0:
			io_error(cx, "connection reset by peer");
			break;
		default:
			io_error(cx, "connection error");
			break;
	}

	mbedtls_net_free(&(cx->net_ctx));

	if (cx->flags & IO_TLS_ENABLED) {
		mbedtls_ctr_drbg_free(&(cx->tls_ctr_drbg));
		mbedtls_entropy_free(&(cx->tls_entropy));
		mbedtls_ssl_config_free(&(cx->tls_conf));
		mbedtls_ssl_free(&(cx->tls_ctx));
		mbedtls_x509_crt_free(&(cx->tls_x509_crt));
	}

	return IO_ST_CXNG;
}

static void*
io_thread(void *arg)
{
	struct connection *cx = arg;

	/* SIGUSR1 indicates to a thread that it should return
	 * to the state machine and check for a new state */

	sigset_t sigset;

	PT_CF(sigaddset(&sigset, SIGUSR1));
	PT_CF(pthread_sigmask(SIG_UNBLOCK, &sigset, NULL));

	cx->st_cur = IO_ST_CXNG;

	io_info(cx, "Connecting to %s:%s", cx->host, cx->port);

	do {
		enum io_state st_cur;
		enum io_state st_new;

		switch ((st_cur = cx->st_cur)) {
			case IO_ST_CXED: st_new = io_state_cxed(cx); break;
			case IO_ST_CXNG: st_new = io_state_cxng(cx); break;
			case IO_ST_PING: st_new = io_state_ping(cx); break;
			case IO_ST_RXNG: st_new = io_state_rxng(cx); break;
			default:
				fatal("invalid state: %d", cx->st_cur);
		}

		PT_LK(&(cx->mtx));

		/* state set by io_cx/io_dx */
		if (cx->st_new != IO_ST_INVALID)
			st_new = cx->st_new;

		cx->st_cur = st_new;
		cx->st_new = IO_ST_INVALID;

		PT_UL(&(cx->mtx));

		/* State transitions */
		switch (ST_X(st_cur, st_new)) {
			case ST_X(IO_ST_DXED, IO_ST_CXNG): /* A1 */
			case ST_X(IO_ST_RXNG, IO_ST_CXNG): /* A2,C */
				io_info(cx, "Connecting to %s:%s", cx->host, cx->port);
				break;
			case ST_X(IO_ST_CXED, IO_ST_CXNG): /* F1 */
				io_dxed(cx);
				break;
			case ST_X(IO_ST_PING, IO_ST_CXNG): /* F2 */
				io_error(cx, "Connection timeout (%u)", cx->ping);
				io_dxed(cx);
				break;
			case ST_X(IO_ST_RXNG, IO_ST_DXED): /* B1 */
			case ST_X(IO_ST_CXNG, IO_ST_DXED): /* B2 */
				io_info(cx, "Connection cancelled");
				break;
			case ST_X(IO_ST_CXED, IO_ST_DXED): /* B3 */
			case ST_X(IO_ST_PING, IO_ST_DXED): /* B4 */
				io_info(cx, "Connection closed");
				io_dxed(cx);
				break;
			case ST_X(IO_ST_CXNG, IO_ST_CXED): /* D */
				io_info(cx, " .. Connection successful");
				io_cxed(cx);
				cx->rx_sleep = 0;
				break;
			case ST_X(IO_ST_CXNG, IO_ST_RXNG): /* E */
				io_error(cx, " .. Connection failed -- retrying");
				break;
			case ST_X(IO_ST_CXED, IO_ST_PING): /* G */
				io_ping(cx, (cx->ping = IO_PING_MIN));
				break;
			case ST_X(IO_ST_PING, IO_ST_PING): /* H */
				io_ping(cx, (cx->ping += IO_PING_REFRESH));
				break;
			case ST_X(IO_ST_PING, IO_ST_CXED): /* I */
				io_ping(cx, (cx->ping = 0));
				break;
			default:
				fatal("BAD ST_X from: %d to: %d", st_cur, st_new);
		}

	} while (cx->st_cur != IO_ST_DXED);

	return NULL;
}

static int
io_cx_read(struct connection *cx, uint32_t timeout)
{
	int ret;
	struct pollfd fd[1];
	unsigned char buf[1024];

	fd[0].fd = cx->net_ctx.MBEDTLS_PRIVATE(fd);
	fd[0].events = POLLIN;

	while ((ret = poll(fd, 1, timeout)) < 0 && errno == EAGAIN)
		continue;

	if (ret == 0)
		return MBEDTLS_ERR_SSL_TIMEOUT;

	if (ret < 0 && errno == EINTR)
		return MBEDTLS_ERR_SSL_WANT_READ;

	if (ret < 0)
		fatal("poll: %s", strerror(errno));

	if (cx->flags & IO_TLS_ENABLED) {
		ret = mbedtls_ssl_read(&(cx->tls_ctx), buf, sizeof(buf));
	} else {
		ret = mbedtls_net_recv(&(cx->net_ctx), buf, sizeof(buf));
	}

	if (ret > 0) {
		IO_CB(io_cb_read_soc((char *)buf, (size_t)ret,  cx->obj));
	}

	return ret;
}

static void
io_fatal(const char *f, int errnum)
{
	char errbuf[512];

	if (strerror_r(errnum, errbuf, sizeof(errbuf)) == 0) {
		fatal("%s: (%d): %s", f, errnum, errbuf);
	} else {
		fatal("%s: (%d): (failed to get error message)", f, errnum);
	}
}

static void
io_sig_handle(int sig)
{
	if (sig == SIGWINCH)
		flag_sigwinch_cb = 1;
}

static void
io_sig_init(void)
{
	struct sigaction sa = {0};

	sa.sa_handler = io_sig_handle;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGWINCH, &sa, NULL) < 0)
		fatal("sigaction - SIGWINCH: %s", strerror(errno));

	if (sigaction(SIGUSR1, &sa, NULL) < 0)
		fatal("sigaction - SIGUSR1: %s", strerror(errno));
}

static void
io_tty_init(void)
{
	struct termios nterm;

	if (isatty(STDIN_FILENO) == 0)
		fatal("isatty: %s", strerror(errno));

	if (tcgetattr(STDIN_FILENO, &term) < 0)
		fatal("tcgetattr: %s", strerror(errno));

	nterm = term;
	nterm.c_lflag &= ~(ECHO | ICANON | ISIG);
	nterm.c_cc[VMIN]  = 1;
	nterm.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSANOW, &nterm) < 0)
		fatal("tcsetattr: %s", strerror(errno));

	if (atexit(io_tty_term))
		fatal("atexit");
}

static void
io_tty_term(void)
{
	/* Exit handler, must return normally */

	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &term) < 0)
		fatal_noexit("tcsetattr: %s", strerror(errno));
}

static int
io_net_connect(struct connection *cx)
{
	char buf[MAX(INET6_ADDRSTRLEN, 512)];
	const void *addr;
	int ret;
	int soc = -1;
	struct addrinfo *p, *res;
	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_flags    = AI_PASSIVE,
		.ai_protocol = IPPROTO_TCP,
		.ai_socktype = SOCK_STREAM,
	};

	if (cx->flags & IO_IPV_4)
		hints.ai_family = AF_INET;

	if (cx->flags & IO_IPV_6)
		hints.ai_family = AF_INET6;

	errno = 0;

	if ((ret = getaddrinfo(cx->host, cx->port, &hints, &res))) {

		if (ret == EAI_SYSTEM && errno == EINTR)
			return -1;

		if (ret == EAI_SYSTEM) {
			io_error(cx, " .. Failed to resolve host: %s",
				io_strerror(buf, sizeof(buf)));
		} else {
			io_error(cx, " .. Failed to resolve host: %s",
				gai_strerror(ret));
		}

		return -1;
	}

	ret = -1;

	for (p = res; p; p = p->ai_next) {

		if ((soc = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
			continue;

		if (connect(soc, p->ai_addr, p->ai_addrlen) == 0)
			break;

		io_net_close(soc);

		if (errno == EINTR)
			goto err;
	}

	if (!p && soc < 0) {
		io_error(cx, " .. Failed to obtain socket: %s", io_strerror(buf, sizeof(buf)));
		goto err;
	}

	if (!p && soc >= 0) {
		io_error(cx, " .. Failed to connect: %s", io_strerror(buf, sizeof(buf)));
		goto err;
	}

	if (p->ai_family == AF_INET)
		addr = &(((struct sockaddr_in*)p->ai_addr)->sin_addr);
	else
		addr = &(((struct sockaddr_in6*)p->ai_addr)->sin6_addr);

	if (inet_ntop(p->ai_family, addr, buf, sizeof(buf)))
		io_info(cx, " .. Connected [%s]", buf);

	ret = soc;

err:
	freeaddrinfo(res);

	return (cx->net_ctx.MBEDTLS_PRIVATE(fd) = ret);
}

static void
io_net_close(int soc)
{
	int errno_save = errno;

	while (close(soc) && errno == EINTR)
		errno_save = EINTR;

	errno = errno_save;
}

static const char*
io_strerror(char *buf, size_t buflen)
{
	if (strerror_r(errno, buf, buflen))
		snprintf(buf, buflen, "(failed to get error message)");

	return buf;
}

static int
io_tls_establish(struct connection *cx)
{
	const unsigned char pers[] = "rirc-drbg-seed";

	int ret;

	io_info(cx, " .. Establishing TLS connection");

	mbedtls_ctr_drbg_init(&(cx->tls_ctr_drbg));
	mbedtls_entropy_init(&(cx->tls_entropy));
	mbedtls_ssl_init(&(cx->tls_ctx));
	mbedtls_ssl_config_init(&(cx->tls_conf));
	mbedtls_x509_crt_init(&(cx->tls_x509_crt));

	if ((ret = mbedtls_ssl_config_defaults(
			&(cx->tls_conf),
			MBEDTLS_SSL_IS_CLIENT,
			MBEDTLS_SSL_TRANSPORT_STREAM,
			MBEDTLS_SSL_PRESET_DEFAULT))) {
		io_error(cx, " .. %s ", io_tls_err(ret));
		goto err;
	}

	mbedtls_ssl_conf_max_version(
			&(cx->tls_conf),
			MBEDTLS_SSL_MAJOR_VERSION_3,
			MBEDTLS_SSL_MINOR_VERSION_3);

	mbedtls_ssl_conf_min_version(
			&(cx->tls_conf),
			MBEDTLS_SSL_MAJOR_VERSION_3,
			MBEDTLS_SSL_MINOR_VERSION_3);

	if ((ret = mbedtls_ctr_drbg_seed(
			&(cx->tls_ctr_drbg),
			mbedtls_entropy_func,
			&(cx->tls_entropy),
			pers,
			sizeof(pers)))) {
		io_error(cx, " .. %s ", io_tls_err(ret));
		goto err;
	}

	if (ca_cert_path && *ca_cert_path) {

		if ((ret = mbedtls_x509_crt_parse_file(&(cx->tls_x509_crt), ca_cert_path)) < 0) {
			io_error(cx, "  .. Failed to load ca cert: '%s': %s", ca_cert_path, io_tls_err(ret));
			goto err;
		}

	} else {

		for (size_t i = 0; i < ARR_LEN(ca_cert_paths); i++) {
			if ((ret = mbedtls_x509_crt_parse_file(&(cx->tls_x509_crt), ca_cert_paths[i])) >= 0) {
				break;
			} else if (i == ARR_LEN(ca_cert_paths)) {
				io_error(cx, "  .. Failed to load ca cert: %s", io_tls_err(ret));
				goto err;
			}
		}
	}

	mbedtls_ssl_conf_rng(&(cx->tls_conf), mbedtls_ctr_drbg_random, &(cx->tls_ctr_drbg));

	if (cx->flags & IO_TLS_VRFY_DISABLED) {
		mbedtls_ssl_conf_authmode(&(cx->tls_conf), MBEDTLS_SSL_VERIFY_NONE);
	} else {
		mbedtls_ssl_conf_ca_chain(&(cx->tls_conf), &(cx->tls_x509_crt), NULL);

		if (cx->flags & IO_TLS_VRFY_OPTIONAL)
			mbedtls_ssl_conf_authmode(&(cx->tls_conf), MBEDTLS_SSL_VERIFY_OPTIONAL);

		if (cx->flags & IO_TLS_VRFY_REQUIRED)
			mbedtls_ssl_conf_authmode(&(cx->tls_conf), MBEDTLS_SSL_VERIFY_REQUIRED);
	}

	if ((ret = mbedtls_net_set_block(&(cx->net_ctx)))) {
		io_error(cx, " .. %s ", io_tls_err(ret));
		goto err;
	}

	if ((ret = mbedtls_ssl_setup(&(cx->tls_ctx), &(cx->tls_conf)))) {
		io_error(cx, " .. %s ", io_tls_err(ret));
		goto err;
	}

	if ((ret = mbedtls_ssl_set_hostname(&(cx->tls_ctx), cx->host))) {
		io_error(cx, " .. %s ", io_tls_err(ret));
		goto err;
	}

	mbedtls_ssl_set_bio(
		&(cx->tls_ctx),
		&(cx->net_ctx),
		mbedtls_net_send,
		mbedtls_net_recv,
		NULL);

	while ((ret = mbedtls_ssl_handshake(&(cx->tls_ctx)))) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ
		 && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
			break;
	}

	if (ret && cx->flags & IO_TLS_VRFY_DISABLED) {
		io_error(cx, " .. %s ", io_tls_err(ret));
		goto err;
	}

	if (io_tls_x509_vrfy(cx) < 0)
		io_error(cx, " .... Unknown x509 error");

	if (ret) {
		io_error(cx, " .. %s ", io_tls_err(ret));
		goto err;
	}

	io_info(cx, " .. TLS connection established");
	io_info(cx, " ..   - version:     %s", mbedtls_ssl_get_version(&(cx->tls_ctx)));
	io_info(cx, " ..   - ciphersuite: %s", mbedtls_ssl_get_ciphersuite(&(cx->tls_ctx)));

	return 0;

err:

	io_error(cx, " .. TLS connection failure");

	mbedtls_ctr_drbg_free(&(cx->tls_ctr_drbg));
	mbedtls_entropy_free(&(cx->tls_entropy));
	mbedtls_ssl_config_free(&(cx->tls_conf));
	mbedtls_ssl_free(&(cx->tls_ctx));
	mbedtls_x509_crt_free(&(cx->tls_x509_crt));
	mbedtls_net_free(&(cx->net_ctx));

	return -1;
}

static int
io_tls_x509_vrfy(struct connection *cx)
{
	char *s, *p;
	char buf[1024];
	uint32_t ret;

	if (!(ret = mbedtls_ssl_get_verify_result(&(cx->tls_ctx))))
		return 0;

	if (ret == (uint32_t)(-1))
		return -1;

	if (mbedtls_x509_crt_verify_info(buf, sizeof(buf), "", ret) < 0)
		return -1;

	for (s = buf; s && *s; s = p) {

		if ((p = strchr(buf, '\n')))
			*p++ = 0;

		io_error(cx, " .... %s", s);
	}

	return 0;
}

static const char*
io_tls_err(int err)
{
	const char *str;

	if ((str = mbedtls_high_level_strerr(err)))
		return str;

	if ((str = mbedtls_low_level_strerr(err)))
		return str;

	return "Unknown error";
}
