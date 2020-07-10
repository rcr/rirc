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

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509.h"

#include "config.h"
#include "rirc.h"
#include "src/io.h"
#include "src/io_net.h"
#include "utils/utils.h"

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
		PT_LK(&cb_mutex);   \
		io_cb(__VA_ARGS__); \
		PT_UL(&cb_mutex);   \
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
	mbedtls_net_context ssl_fd;
	mbedtls_ssl_config ssl_conf;
	mbedtls_ssl_context ssl_ctx;
	pthread_mutex_t mtx;
	pthread_t tid;
	unsigned ping;
	unsigned rx_sleep;
};

static enum io_state_t io_state_cxed(struct connection*);
static enum io_state_t io_state_cxng(struct connection*);
static enum io_state_t io_state_ping(struct connection*);
static enum io_state_t io_state_rxng(struct connection*);
static int io_cx_read(struct connection*);
static void io_fatal(const char*, int);
static void io_sig_handle(int);
static void io_sig_init(void);
static void io_ssl_init(void);
static void io_ssl_term(void);
static void io_tty_init(void);
static void io_tty_term(void);
static void io_tty_winsize(void);
static void* io_thread(void*);

static int io_running;
static mbedtls_ctr_drbg_context ssl_ctr_drbg;
static mbedtls_entropy_context ssl_entropy;
static mbedtls_ssl_config ssl_conf;
static mbedtls_x509_crt ssl_cacert;
static pthread_mutex_t cb_mutex = PTHREAD_MUTEX_INITIALIZER;
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
		if ((ret = mbedtls_ssl_write(&(cx->ssl_ctx), sendbuf + ret, len - ret)) < 0) {
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
	io_ssl_init();
}

void
io_start(void)
{
	io_running = 1;

	while (io_running) {

		char buf[128];
		ssize_t ret = read(STDIN_FILENO, buf, sizeof(buf));

		if (ret > 0) {
			PT_LK(&cb_mutex);
			io_cb_read_inp(buf, ret);
			PT_UL(&cb_mutex);
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
	char addr_buf[INET6_ADDRSTRLEN];
	char vrfy_buf[512];
	enum io_state_t st = IO_ST_RXNG;
	int ret;
	int soc;
	uint32_t cert_ret;

	io_cb_info(cx, "Connecting to %s:%s", cx->host, cx->port);

	if ((ret = io_net_connect(&soc, cx->host, cx->port)) != IO_NET_ERR_NONE) {
		switch (ret) {
			case IO_NET_ERR_EINTR:
				st = IO_ST_DXED;
				goto error_net;
			case IO_NET_ERR_SOCKET_FAILED:
				io_cb_err(cx, " ... Failed to obtain socket");
				goto error_net;
			case IO_NET_ERR_UNKNOWN_HOST:
				io_cb_err(cx, " ... Failed to resolve host");
				goto error_net;
			case IO_NET_ERR_CONNECT_FAILED:
				io_cb_err(cx, " ... Failed to connect to host");
				goto error_net;
			default:
				fatal("unknown net error");
		}
	}

	if ((ret = io_net_ip_str(soc, addr_buf, sizeof(addr_buf))) != IO_NET_ERR_NONE) {
		if (ret == IO_NET_ERR_EINTR) {
			st = IO_ST_DXED;
			goto error_net;
		}
		io_cb_info(cx, " ... Connected (failed to optain IP address)");
	} else {
		io_cb_info(cx, " ... Connected to [%s]", addr_buf);
	}

	io_cb_info(cx, " ... Establishing SSL");

	mbedtls_net_init(&(cx->ssl_fd));
	mbedtls_ssl_init(&(cx->ssl_ctx));
	mbedtls_ssl_config_init(&(cx->ssl_conf));

	cx->ssl_conf = ssl_conf;
	cx->ssl_fd.fd = soc;

	if ((ret = mbedtls_net_set_block(&(cx->ssl_fd))) != 0) {
		io_cb_err(cx, " ... mbedtls_net_set_block failure");
		goto error_ssl;
	}

	if ((ret = mbedtls_ssl_setup(&(cx->ssl_ctx), &(cx->ssl_conf))) != 0) {
		io_cb_err(cx, " ... mbedtls_ssl_setup failure");
		goto error_ssl;
	}

	if ((ret = mbedtls_ssl_set_hostname(&(cx->ssl_ctx), cx->host)) != 0) {
		io_cb_err(cx, " ... mbedtls_ssl_set_hostname failure");
		goto error_ssl;
	}

	mbedtls_ssl_set_bio(
		&(cx->ssl_ctx),
		&(cx->ssl_fd),
		mbedtls_net_send,
		NULL,
		mbedtls_net_recv_timeout);

	while ((ret = mbedtls_ssl_handshake(&(cx->ssl_ctx))) != 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			io_cb_err(cx, " ... mbedtls_ssl_handshake failure");
			goto error_ssl;
		}
	}

	if ((cert_ret = mbedtls_ssl_get_verify_result(&(cx->ssl_ctx))) != 0) {
		if (mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "", cert_ret) <= 0) {
			io_cb_err(cx, " ... failed to verify cert: unknown failure");
		} else {
			io_cb_err(cx, " ... failed to verify cert: %s", vrfy_buf);
		}
		goto error_ssl;
	}

	io_cb_info(cx, " ... SSL connection established");
	io_cb_info(cx, " ...   - version:     %s", mbedtls_ssl_get_version(&(cx->ssl_ctx)));
	io_cb_info(cx, " ...   - ciphersuite: %s", mbedtls_ssl_get_ciphersuite(&(cx->ssl_ctx)));

	return IO_ST_CXED;

error_ssl:

	mbedtls_net_free(&(cx->ssl_fd));
	mbedtls_ssl_free(&(cx->ssl_ctx));
	mbedtls_ssl_config_free(&(cx->ssl_conf));

error_net:

	return st;
}

static enum io_state_t
io_state_cxed(struct connection *cx)
{
	int ret;
	enum io_state_t st = IO_ST_CXNG;

	mbedtls_ssl_conf_read_timeout(&(cx->ssl_conf), SEC_IN_MS(IO_PING_MIN));

	while ((ret = io_cx_read(cx)) > 0)
		continue;

	switch (ret) {
		case MBEDTLS_ERR_SSL_WANT_READ:
		case MBEDTLS_ERR_SSL_WANT_WRITE:
			break;
		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			break;
		case MBEDTLS_ERR_SSL_TIMEOUT:
			return IO_ST_PING;
		case MBEDTLS_ERR_NET_CONN_RESET:
		case 0:
			io_cb_err(cx, "connection reset by peer");
			break;
		default:
			io_cb_err(cx, "connection ssl error");
			break;
	}

	mbedtls_net_free(&(cx->ssl_fd));
	mbedtls_ssl_free(&(cx->ssl_ctx));
	mbedtls_ssl_config_free(&(cx->ssl_conf));

	return st;
}

static enum io_state_t
io_state_ping(struct connection *cx)
{
	int ret;
	enum io_state_t st = IO_ST_CXNG;

	mbedtls_ssl_conf_read_timeout(&(cx->ssl_conf), SEC_IN_MS(IO_PING_REFRESH));

	while ((ret = io_cx_read(cx)) <= 0 && ret == MBEDTLS_ERR_SSL_TIMEOUT) {
		if ((cx->ping += IO_PING_REFRESH) < IO_PING_MAX) {
			io_cb_ping_n(cx, cx->ping);
		} else {
			break;
		}
	}

	if (ret > 0)
		return IO_ST_CXED;

	switch (ret) {
		case MBEDTLS_ERR_SSL_WANT_READ:
		case MBEDTLS_ERR_SSL_WANT_WRITE:
			break;
		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			break;
		case MBEDTLS_ERR_SSL_TIMEOUT: /* read timeout */
			io_cb_err(cx, "connection timeout (%u)", cx->ping);
			break;
		case MBEDTLS_ERR_NET_CONN_RESET:
		case 0:
			io_cb_err(cx, "connection reset by peer");
			break;
		default:
			io_cb_err(cx, "connection ssl error");
			break;
	}

	mbedtls_net_free(&(cx->ssl_fd));
	mbedtls_ssl_free(&(cx->ssl_ctx));
	mbedtls_ssl_config_free(&(cx->ssl_conf));

	return st;
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
		enum io_state_t st_new;
		enum io_state_t st_old;

		switch (cx->st_cur) {
			case IO_ST_CXED: st_new = io_state_cxed(cx); break;
			case IO_ST_CXNG: st_new = io_state_cxng(cx); break;
			case IO_ST_PING: st_new = io_state_ping(cx); break;
			case IO_ST_RXNG: st_new = io_state_rxng(cx); break;
			default:
				fatal("invalid state: %d", cx->st_cur);
		}

		st_old = cx->st_cur;

		PT_LK(&(cx->mtx));

		/* New state set by io_cx/io_dx */
		if (cx->st_new != IO_ST_INVALID) {
			cx->st_new = IO_ST_INVALID;
			cx->st_cur = st_new = cx->st_new;
		} else {
			cx->st_cur = st_new;
		}

		PT_UL(&(cx->mtx));

		/* State transitions */
		switch (ST_X(st_old, st_new)) {
			case ST_X(IO_ST_CXED, IO_ST_CXNG): /* F1 */
			case ST_X(IO_ST_PING, IO_ST_CXNG): /* F2 */
				io_cb_dxed(cx);
			case ST_X(IO_ST_DXED, IO_ST_CXNG): /* A1 */
			case ST_X(IO_ST_RXNG, IO_ST_CXNG): /* A2,C */
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
				io_cb_ping_n(cx, cx->ping);
				break;
			case ST_X(IO_ST_PING, IO_ST_CXED): /* I */
				cx->ping = 0;
				io_cb_ping_0(cx, cx->ping);
				break;
			default:
				fatal("BAD ST_X from: %d to: %d", st_old, st_new);
		}

	} while (cx->st_cur != IO_ST_DXED);

	return NULL;
}

static int
io_cx_read(struct connection *cx)
{
	int ret;
	unsigned char ssl_readbuf[1024];

	if ((ret = mbedtls_ssl_read(&(cx->ssl_ctx), ssl_readbuf, sizeof(ssl_readbuf))) > 0) {
		PT_LK(&cb_mutex);
		io_cb_read_soc((char *)ssl_readbuf, (size_t)ret,  cx->obj);
		PT_UL(&cb_mutex);
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
io_ssl_init(void)
{
	const char *tls_pers = "rirc-drbg-ctr-pers";

	mbedtls_ssl_config_init(&ssl_conf);
	mbedtls_ctr_drbg_init(&ssl_ctr_drbg);
	mbedtls_entropy_init(&ssl_entropy);
	mbedtls_x509_crt_init(&ssl_cacert);

	if (mbedtls_x509_crt_parse_path(&ssl_cacert, ca_cert_path) != 0) {
		fatal("ssl init failed: mbedtls_x509_crt_parse_path");
	}

	if (mbedtls_ctr_drbg_seed(
			&ssl_ctr_drbg,
			mbedtls_entropy_func,
			&ssl_entropy,
			(unsigned char *)tls_pers,
			strlen(tls_pers)) != 0) {
		fatal("ssl init failed: mbedtls_ctr_drbg_seed");
	}

	if (mbedtls_ssl_config_defaults(
			&ssl_conf,
			MBEDTLS_SSL_IS_CLIENT,
			MBEDTLS_SSL_TRANSPORT_STREAM,
			MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
		fatal("ssl init failed: mbedtls_ssl_config_defaults");
	}

	mbedtls_ssl_conf_ca_chain(&ssl_conf, &ssl_cacert, NULL);
	mbedtls_ssl_conf_read_timeout(&ssl_conf, SEC_IN_MS(IO_PING_MIN));
	mbedtls_ssl_conf_rng(&ssl_conf, mbedtls_ctr_drbg_random, &ssl_ctr_drbg);

	if (atexit(io_ssl_term) != 0)
		fatal("atexit");
}

static void
io_ssl_term(void)
{
	/* Exit handler, must return normally */

	mbedtls_ctr_drbg_free(&ssl_ctr_drbg);
	mbedtls_entropy_free(&ssl_entropy);
	mbedtls_ssl_config_free(&ssl_conf);
	mbedtls_x509_crt_free(&ssl_cacert);
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
