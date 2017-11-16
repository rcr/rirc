#ifndef NET2_H
#define NET2_H

/* net.h
 *
 * The state of a connection at any given time can be
 * described by one of the following:
 *
 *  - dx:   disconnected ~ Socket passively disconnected
 *  - cxng: connecting   ~ Socket connecting or in reconnect cycle
 *  - cx:   connected    ~ Socket connected
 *  - ping: timing out   ~ Socket connected, network state in question
 *
 *     NEW            +----+
 *      |             |    |
 *      v             v    ^
 *  +------+ -----> +--------+
 *  |  dx  |        |  cxng  | <--+
 *  +------+ <----- +--------+    |
 *    ^  ^            |    ^      |
 *    |  |            |    |      |
 *    |  |            v    |      |
 *    |  |          +--------+    |
 *    |  +--------- |   cx   |    |
 *    |             +--------+    |
 *    |               |    ^      |
 *    |               |    |      |
 *    |               v    |      |
 *    |             +--------+    |
 *    +------------ |  ping  | ---+
 *                  +--------+
 *
 * This module exposes functions for explicitly directing network
 * state as well declaring callback functions for state transitions
 * and network activity handling which must be implemented elsewhere
 *
 * The connection cycle is implements an exponential backoff routine
 *
 * Network state transitions result in callbacks:
 *     TODO: TBD
 *
 * Successful reads on stdin and connected sockets result in callbacks:
 *     from stdin:  net_cb_inp
 *     from socket: net_cb_soc
 */

struct connection;
struct connection* connection(void*, const char*, const char*);

/* Explicit direction of net state */
void net_cx(struct connection*);
void net_dx(struct connection*);
void net_poll(void);

/* Network callbacks */
void net_cb_read_inp(void*);
void net_cb_read_soc(void*);

#endif
