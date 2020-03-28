#include <errno.h>
#include <netdb.h>
#include <unistd.h>

#include "io_net.h"
#include "utils/utils.h"

enum io_net_err
io_net_connect(int *soc_set, const char *host, const char *port)
{
	int soc = -1;
	int ret = IO_NET_ERR_UNKNOWN;
	struct addrinfo *p, *results;

	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_flags    = AI_PASSIVE,
		.ai_protocol = IPPROTO_TCP,
		.ai_socktype = SOCK_STREAM,
	};

	errno = 0;

	if ((ret = getaddrinfo(host, port, &hints, &results)) != 0) {

		if (ret == EAI_SYSTEM && errno == EINTR)
			return IO_NET_ERR_EINTR;

		return IO_NET_ERR_UNKNOWN_HOST;
	}

	for (p = results; p != NULL; p = p->ai_next) {

		if ((soc = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
			ret = IO_NET_ERR_SOCKET_FAILED;
			continue;
		}

		if (connect(soc, p->ai_addr, p->ai_addrlen) == 0) {
			ret = IO_NET_ERR_NONE;
			break;
		}

		if (errno == EINTR) {
			ret = IO_NET_ERR_EINTR;
			break;
		}

		close(soc);
		ret = IO_NET_ERR_CONNECT_FAILED;
	}

	freeaddrinfo(results);

	if (ret == IO_NET_ERR_NONE)
		*soc_set = soc;

	return ret;
}

enum io_net_err
io_net_ip_str(int soc, char *buf, size_t len)
{
	int ret;
	socklen_t addr_len;
	struct sockaddr addr;

	addr_len = sizeof(addr);

	if ((ret = getpeername(soc, &addr, &addr_len)) == -1)
		return IO_NET_ERR_IP;

	if ((ret = getnameinfo(&addr, addr_len, buf, len, NULL, 0, NI_NUMERICHOST)) < 0) {
		if (ret == EAI_SYSTEM && errno == EINTR)
			return IO_NET_ERR_EINTR;
	}

	return (ret ? IO_NET_ERR_IP : IO_NET_ERR_NONE);
}
