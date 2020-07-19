#ifndef IO_NET_H
#define IO_NET_H

#include <stddef.h>

enum io_net_err
{
	IO_NET_ERR_NONE = 0,
	IO_NET_ERR_SOCKET_FAILED,
	IO_NET_ERR_UNKNOWN_HOST,
	IO_NET_ERR_CONNECT_FAILED,
	IO_NET_ERR_EINTR,
	IO_NET_ERR_IP,
	IO_NET_ERR_UNKNOWN
};

enum io_net_err io_net_connect(int*, const char*, const char*);
enum io_net_err io_net_ip_str(int, char*, size_t);

#endif
