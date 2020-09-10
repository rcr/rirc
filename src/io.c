#include "src/io.h"

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "config.h"
#include "rirc.h"
#include "src/io_net.h"
#include "utils/utils.h"

/* lib/mbedtls.h is the compile time config
 * and must precede the other mbedtls headers */
#include "lib/mbedtls.h"
#include "lib/mbedtls/include/mbedtls/ctr_drbg.h"
#include "lib/mbedtls/include/mbedtls/entropy.h"
#include "lib/mbedtls/include/mbedtls/error.h"
#include "lib/mbedtls/include/mbedtls/net_sockets.h"
#include "lib/mbedtls/include/mbedtls/ssl.h"
#include "lib/mbedtls/include/mbedtls/x509_crt.h"

/* RFC 2812, section 2.3 */
#ifndef IO_MESG_LEN
#define IO_MESG_LEN 510
#endif

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
#define PT_CB(...) \
	do {                    \
		PT_LK(&io_cb_mutex);   \
		io_cb(__VA_ARGS__); \
		PT_UL(&io_cb_mutex);   \
	} while (0)

/* state transition */
#define ST_X(OLD, NEW) (((OLD) << 3) | (NEW))

#define io_cb_cxed(C)        PT_CB(IO_CB_CXED, (C)->obj)
#define io_cb_dxed(C)        PT_CB(IO_CB_DXED, (C)->obj)
#define io_cb_err(C, ...)    PT_CB(IO_CB_ERR, (C)->obj, __VA_ARGS__)
#define io_cb_info(C, ...)   PT_CB(IO_CB_INFO, (C)->obj, __VA_ARGS__)
#define io_cb_ping_0(C, ...) PT_CB(IO_CB_PING_0, (C)->obj, __VA_ARGS__)
#define io_cb_ping_1(C, ...) PT_CB(IO_CB_PING_1, (C)->obj, __VA_ARGS__)
#define io_cb_ping_n(C, ...) PT_CB(IO_CB_PING_N, (C)->obj, __VA_ARGS__)
#define io_cb_signal(S)      PT_CB(IO_CB_SIGNAL, NULL, (S))

enum io_err_t
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
	enum io_state_t {
		IO_ST_INVALID,
		IO_ST_DXED, /* Socket disconnected, passive */
		IO_ST_RXNG, /* Socket disconnected, pending reconnect */
		IO_ST_CXNG, /* Socket connection in progress */
		IO_ST_CXED, /* Socket connected */
		IO_ST_PING, /* Socket connected, network state in question */
	} st_cur, /* current thread state */
	  st_new; /* new thread state */
	mbedtls_net_context tls_fd;
	mbedtls_ssl_config  tls_conf;
	mbedtls_ssl_context tls_ctx;
	pthread_mutex_t mtx;
	pthread_t tid;
	unsigned ping;
	unsigned rx_sleep;
};

static const char* io_tls_strerror(int, char*, size_t);
static enum io_state_t io_state_cxed(struct connection*);
static enum io_state_t io_state_cxng(struct connection*);
static enum io_state_t io_state_ping(struct connection*);
static enum io_state_t io_state_rxng(struct connection*);
static int io_cx_read(struct connection*, unsigned);
static void io_fatal(const char*, int);
static void io_sig_handle(int);
static void io_sig_init(void);
static void io_tls_init(void);
static void io_tls_term(void);
static void io_tty_init(void);
static void io_tty_term(void);
static void io_tty_winsize(void);
static void* io_thread(void*);

static int io_running;
static mbedtls_ctr_drbg_context tls_ctr_drbg;
static mbedtls_entropy_context  tls_entropy;
static mbedtls_x509_crt         tls_x509_crt;
static pthread_mutex_t io_cb_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct termios term;
static unsigned io_cols;
static unsigned io_rows;
static volatile sig_atomic_t flag_sigwinch_cb; /* sigwinch callback */
static volatile sig_atomic_t flag_tty_resized; /* sigwinch ws resize */

struct connection*
connection(const void *obj, const char *host, const char *port)
{
	struct connection *cx;

	if ((cx = calloc(1U, sizeof(*cx))) == NULL)
		fatal("malloc: %s", strerror(errno));

	cx->obj = obj;
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
	enum io_err_t err = IO_ERR_NONE;
	enum io_state_t st;
	sigset_t sigset;
	sigset_t sigset_old;

	PT_LK(&(cx->mtx));

	switch ((st = cx->st_cur)) {
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
	enum io_err_t err = IO_ERR_NONE;

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
		if ((ret = mbedtls_ssl_write(&(cx->tls_ctx), sendbuf + ret, len - ret)) < 0) {
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
		}
	} while ((written += ret) < len);

	return IO_ERR_NONE;
}

void
io_init(void)
{
	io_sig_init();
	io_tty_init();
	io_tls_init();
}

void
io_start(void)
{
	io_running = 1;

	while (io_running) {

		char buf[128];
		ssize_t ret = read(STDIN_FILENO, buf, sizeof(buf));

		if (ret > 0) {
			PT_LK(&io_cb_mutex);
			io_cb_read_inp(buf, ret);
			PT_UL(&io_cb_mutex);
		} else {
			if (errno == EINTR) {
				if (flag_sigwinch_cb) {
					flag_sigwinch_cb = 0;
					io_cb_signal(IO_SIGWINCH);
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

void
io_cb_lk(void)
{
	PT_LK(&io_cb_mutex);
}

void
io_cb_ul(void)
{
	PT_UL(&io_cb_mutex);
}

static void
io_tty_winsize(void)
{
	static struct winsize tty_ws;

	if (flag_tty_resized == 0) {
		flag_tty_resized = 1;

		if (ioctl(0, TIOCGWINSZ, &tty_ws) < 0)
			fatal("ioctl: %s", strerror(errno));

		io_rows = tty_ws.ws_row;
		io_cols = tty_ws.ws_col;
	}
}

unsigned
io_tty_cols(void)
{
	io_tty_winsize();
	return io_cols;
}

unsigned
io_tty_rows(void)
{
	io_tty_winsize();
	return io_rows;
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

static const char*
io_tls_strerror(int err, char *buf, size_t len)
{
	mbedtls_strerror(err, buf, len);
	return buf;
}

static enum io_state_t
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

	io_cb_info(cx, "Attemping reconnect in %02u:%02u",
		(cx->rx_sleep / 60),
		(cx->rx_sleep % 60));

	sleep(cx->rx_sleep);

	return IO_ST_CXNG;
}

static enum io_state_t
io_state_cxng(struct connection *cx)
{
	char buf[MAX(INET6_ADDRSTRLEN, 512)];
	int ret;
	int soc;
	uint32_t cert_ret;

	io_cb_info(cx, "Connecting to %s:%s", cx->host, cx->port);

	if ((soc = io_net_connect(cx->obj, cx->host, cx->port)) < 0)
		goto err_net;

	io_cb_info(cx, " ... Establishing TLS connection");

	mbedtls_net_init(&(cx->tls_fd));
	mbedtls_ssl_init(&(cx->tls_ctx));
	mbedtls_ssl_config_init(&(cx->tls_conf));

	cx->tls_fd.fd = soc;

	if ((ret = mbedtls_ssl_config_defaults(
			&(cx->tls_conf),
			MBEDTLS_SSL_IS_CLIENT,
			MBEDTLS_SSL_TRANSPORT_STREAM,
			MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		io_cb_err(cx, " ... mbedtls_ssl_config_defaults: %s", io_tls_strerror(ret, buf, sizeof(buf)));
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

	mbedtls_ssl_conf_ca_chain(&(cx->tls_conf), &tls_x509_crt, NULL);
	mbedtls_ssl_conf_rng(&(cx->tls_conf), mbedtls_ctr_drbg_random, &tls_ctr_drbg);

	if ((ret = mbedtls_net_set_block(&(cx->tls_fd))) != 0) {
		io_cb_err(cx, " ... mbedtls_net_set_block: %s", io_tls_strerror(ret, buf, sizeof(buf)));
		goto err;
	}

	if ((ret = mbedtls_ssl_setup(&(cx->tls_ctx), &(cx->tls_conf))) != 0) {
		io_cb_err(cx, " ... mbedtls_ssl_setup: %s", io_tls_strerror(ret, buf, sizeof(buf)));
		goto err;
	}

	if ((ret = mbedtls_ssl_set_hostname(&(cx->tls_ctx), cx->host)) != 0) {
		io_cb_err(cx, " ... mbedtls_ssl_set_hostname: %s", io_tls_strerror(ret, buf, sizeof(buf)));
		goto err;
	}

	mbedtls_ssl_set_bio(
		&(cx->tls_ctx),
		&(cx->tls_fd),
		mbedtls_net_send,
		NULL,
		mbedtls_net_recv_timeout);

	while ((ret = mbedtls_ssl_handshake(&(cx->tls_ctx))) != 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			io_cb_err(cx, " ... mbedtls_ssl_handshake: %s", io_tls_strerror(ret, buf, sizeof(buf)));
			goto err;
		}
	}

	if ((cert_ret = mbedtls_ssl_get_verify_result(&(cx->tls_ctx))) != 0) {
		if (mbedtls_x509_crt_verify_info(buf, sizeof(buf), "", cert_ret) <= 0) {
			io_cb_err(cx, " ... failed to verify cert: unknown failure");
		} else {
			io_cb_err(cx, " ... failed to verify cert: %s", buf);
		}
		goto err;
	}

	io_cb_info(cx, " ... TLS connection established");
	io_cb_info(cx, " ...   - version:     %s", mbedtls_ssl_get_version(&(cx->tls_ctx)));
	io_cb_info(cx, " ...   - ciphersuite: %s", mbedtls_ssl_get_ciphersuite(&(cx->tls_ctx)));

	return IO_ST_CXED;

err:

	io_cb_err(cx, " ... TLS connection failure");

	mbedtls_ssl_config_free(&(cx->tls_conf));
	mbedtls_ssl_free(&(cx->tls_ctx));
	mbedtls_net_free(&(cx->tls_fd));

err_net:

	return IO_ST_RXNG;
}

static enum io_state_t
io_state_cxed(struct connection *cx)
{
	int ret;

	while ((ret = io_cx_read(cx, IO_PING_MIN)) > 0)
		continue;

	if (ret == MBEDTLS_ERR_SSL_TIMEOUT)
		return IO_ST_PING;

	switch (ret) {
		case MBEDTLS_ERR_SSL_WANT_READ:
		case MBEDTLS_ERR_SSL_WANT_WRITE:
			break;
		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			io_cb_info(cx, "connection closed gracefully");
			break;
		case MBEDTLS_ERR_NET_CONN_RESET:
		case 0:
			io_cb_err(cx, "connection reset by peer");
			break;
		default:
			io_cb_err(cx, "connection tls error");
			break;
	}

	mbedtls_net_free(&(cx->tls_fd));
	mbedtls_ssl_config_free(&(cx->tls_conf));
	mbedtls_ssl_free(&(cx->tls_ctx));

	return IO_ST_CXNG;
}

static enum io_state_t
io_state_ping(struct connection *cx)
{
	int ret;

	if (cx->ping >= IO_PING_MAX)
		return IO_ST_CXNG;

	if ((ret = io_cx_read(cx, IO_PING_REFRESH)) > 0)
		return IO_ST_CXED;

	if (ret == MBEDTLS_ERR_SSL_TIMEOUT)
		return IO_ST_PING;

	switch (ret) {
		case MBEDTLS_ERR_SSL_WANT_READ:
		case MBEDTLS_ERR_SSL_WANT_WRITE:
			break;
		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			io_cb_info(cx, "connection closed gracefully");
			break;
		case MBEDTLS_ERR_NET_CONN_RESET:
		case 0:
			io_cb_err(cx, "connection reset by peer");
			break;
		default:
			io_cb_err(cx, "connection ssl error");
			break;
	}

	mbedtls_net_free(&(cx->tls_fd));
	mbedtls_ssl_config_free(&(cx->tls_conf));
	mbedtls_ssl_free(&(cx->tls_ctx));

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

	do {
		enum io_state_t st_cur;
		enum io_state_t st_new;

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
			st_new = cx->st_cur;

		cx->st_cur = st_new;
		cx->st_new = IO_ST_INVALID;

		PT_UL(&(cx->mtx));

		/* State transitions */
		switch (ST_X(st_cur, st_new)) {
			case ST_X(IO_ST_DXED, IO_ST_CXNG): /* A1 */
			case ST_X(IO_ST_RXNG, IO_ST_CXNG): /* A2,C */
				break;
			case ST_X(IO_ST_CXED, IO_ST_CXNG): /* F1 */
				io_cb_dxed(cx);
				break;
			case ST_X(IO_ST_PING, IO_ST_CXNG): /* F2 */
				io_cb_err(cx, "connection timeout (%u)", cx->ping);
				io_cb_dxed(cx);
				break;
			case ST_X(IO_ST_RXNG, IO_ST_DXED): /* B1 */
			case ST_X(IO_ST_CXNG, IO_ST_DXED): /* B2 */
				io_cb_info(cx, "Connection cancelled");
				break;
			case ST_X(IO_ST_CXED, IO_ST_DXED): /* B3 */
			case ST_X(IO_ST_PING, IO_ST_DXED): /* B4 */
				io_cb_info(cx, "Connection closed");
				io_cb_dxed(cx);
				break;
			case ST_X(IO_ST_CXNG, IO_ST_CXED): /* D */
				io_cb_info(cx, " ... Connection successful");
				io_cb_cxed(cx);
				cx->rx_sleep = 0;
				break;
			case ST_X(IO_ST_CXNG, IO_ST_RXNG): /* E */
				io_cb_err(cx, " ... Connection failed -- retrying");
				break;
			case ST_X(IO_ST_CXED, IO_ST_PING): /* G */
				cx->ping = IO_PING_MIN;
				io_cb_ping_1(cx, cx->ping);
				break;
			case ST_X(IO_ST_PING, IO_ST_PING): /* H */
				cx->ping += IO_PING_REFRESH;
				io_cb_ping_n(cx, cx->ping);
				break;
			case ST_X(IO_ST_PING, IO_ST_CXED): /* I */
				cx->ping = 0;
				io_cb_ping_0(cx, cx->ping);
				break;
			default:
				fatal("BAD ST_X from: %d to: %d", st_cur, st_new);
		}

	} while (cx->st_cur != IO_ST_DXED);

	return NULL;
}

static int
io_cx_read(struct connection *cx, unsigned timeout)
{
	int ret;
	unsigned char ssl_readbuf[1024];

	mbedtls_ssl_conf_read_timeout(&(cx->tls_conf), SEC_IN_MS(timeout));

	if ((ret = mbedtls_ssl_read(&(cx->tls_ctx), ssl_readbuf, sizeof(ssl_readbuf))) > 0) {
		PT_LK(&io_cb_mutex);
		io_cb_read_soc((char *)ssl_readbuf, (size_t)ret,  cx->obj);
		PT_UL(&io_cb_mutex);
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
	if (sig == SIGWINCH) {
		flag_sigwinch_cb = 1;
		flag_tty_resized = 0;
	}
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
io_tls_init(void)
{
	char buf[512];
	int err;
	struct timespec ts;

	mbedtls_ctr_drbg_init(&tls_ctr_drbg);
	mbedtls_entropy_init(&tls_entropy);
	mbedtls_x509_crt_init(&tls_x509_crt);

	if (atexit(io_tls_term) != 0)
		fatal("atexit");

	if (timespec_get(&ts, TIME_UTC) != TIME_UTC)
		fatal("timespec_get");

	if (snprintf(buf, sizeof(buf), "rirc-%lu-%lu", ts.tv_sec, ts.tv_nsec) < 0)
		fatal("snprintf");

	if ((err = mbedtls_ctr_drbg_seed(
			&tls_ctr_drbg,
			mbedtls_entropy_func,
			&tls_entropy,
			(const unsigned char *)buf,
			strlen(buf))) != 0) {
		fatal("mbedtls_ctr_drbg_seed: %s", io_tls_strerror(err, buf, sizeof(buf)));
	}

	if ((err = mbedtls_x509_crt_parse_path(&tls_x509_crt, ca_cert_path)) < 0)
		fatal("mbedtls_x509_crt_parse_path: %s", io_tls_strerror(err, buf, sizeof(buf)));
}

static void
io_tls_term(void)
{
	/* Exit handler, must return normally */

	mbedtls_ctr_drbg_free(&tls_ctr_drbg);
	mbedtls_entropy_free(&tls_entropy);
	mbedtls_x509_crt_free(&tls_x509_crt);
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

	if (atexit(io_tty_term) != 0)
		fatal("atexit");
}

static void
io_tty_term(void)
{
	/* Exit handler, must return normally */

	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &term) < 0)
		fatal_noexit("tcsetattr: %s", strerror(errno));
}
