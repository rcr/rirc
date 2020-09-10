#ifndef IO_NET_H
#define IO_NET_H

enum io_net_err
{
	IO_NET_ERR_FAIL  = -1,
	IO_NET_ERR_EINTR = -2,
};

/* Returns a socket on success, or io_net_err value on eror */
int io_net_connect(const void*, const char*, const char*);

#endif
