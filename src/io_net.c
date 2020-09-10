#include "io_net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define IO_INTERNAL

#include "io.h"
#include "utils/utils.h"

#define IO_NET_ERRSIZE 512

#define io_net_log(O, L, F, ...) \
	do { \
		io_cb_lk(); \
		io_cb_log((O), (L), (F), __VA_ARGS__); \
		io_cb_ul(); \
	} while (0)

static const char* io_net_strerror(int, char*, size_t);
static void io_net_close(int);

int
io_net_connect(const void *cb_obj, const char *host, const char *port)
{
	char buf[MAX(IO_NET_ERRSIZE, INET6_ADDRSTRLEN)];
	int ret;
	int soc;
	struct addrinfo *p, *res;
	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_flags    = AI_PASSIVE,
		.ai_protocol = IPPROTO_TCP,
		.ai_socktype = SOCK_STREAM,
	};

	errno = 0;

	if ((ret = getaddrinfo(host, port, &hints, &res)) != 0) {

		if (ret == EAI_SYSTEM && errno == EINTR)
			return IO_NET_ERR_EINTR;

		if (ret == EAI_SYSTEM) {
			io_net_log(cb_obj, IO_LOG_ERROR, " ... Failed to resolve host: %s",
				io_net_strerror(errno, buf, sizeof(buf)));
		} else {
			io_net_log(cb_obj, IO_LOG_ERROR, " ... Failed to resolve host: %s",
				gai_strerror(ret));
		}

		return IO_NET_ERR_FAIL;
	}

	for (p = res; p; p = p->ai_next) {

		if ((soc = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
			continue;

		if (connect(soc, p->ai_addr, p->ai_addrlen) == 0)
			break;

		io_net_close(soc);

		if (errno == EINTR)
			break;
	}

	if (errno == EINTR) {
		ret = IO_NET_ERR_EINTR;
	} else if (!p && soc == -1) {
		io_net_log(cb_obj, IO_LOG_ERROR, " ... Failed to obtain socket: %s",
			io_net_strerror(errno, buf, sizeof(buf)));
		ret = IO_NET_ERR_FAIL;
	} else if (!p) {
		io_net_log(cb_obj, IO_LOG_ERROR, " ... Failed to connect: %s",
			io_net_strerror(errno, buf, sizeof(buf)));
		ret = IO_NET_ERR_FAIL;
	} else {
		const void *addr;

		if (p->ai_family == AF_INET)
			addr = &(((struct sockaddr_in*)p)->sin_addr);
		else
			addr = &(((struct sockaddr_in6*)p)->sin6_addr);

		if (inet_ntop(p->ai_family, addr, buf, sizeof(buf)))
			io_net_log(cb_obj, IO_LOG_INFO, " ... Connected [%s]", buf);

		ret = soc;
	}

	freeaddrinfo(res);

	return ret;
}

static const char*
io_net_strerror(int errnum, char *buf, size_t buflen)
{
	if (strerror_r(errnum, buf, buflen))
		snprintf(buf, buflen, "(failed to get error message)");

	return buf;
}

static void
io_net_close(int soc)
{
	int errno_save;

	while (close(soc) && errno == EINTR) {
		errno_save = EINTR;
	}

	errno = errno_save;
}
