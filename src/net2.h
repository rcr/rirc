#ifndef NET2_H
#define NET2_H

/* net.h
 *
 * Network connection state
 *
 * The state of a connection at any given time can be
 * described by one of the following:
 *
 *  - dxed: disconnected ~ Socket passively disconnected
 *  - cxng: connecting   ~ Socket connecting or in reconnect cycle
 *  - cxed: connected    ~ Socket connected
 *  - ping: timing out   ~ Socket connected, network state in question
 *
 *     NEW             +----+
 *      |              |    |
 *      |              |   (F)
 *      v              v    |
 *  +------+ --(A)-> +--------+
 *  | dxed |         |  cxng  | <--+
 *  +------+ <-(B)-- +--------+    |
 *    ^  ^             |    ^      |
 *    |  |            (D)   |      |
 *    |  |             |   (E)     |
 *    |  |             v    |      |
 *    |  |           +--------+    |
 *    |  +-----(B)-- |  cxed  |    |
 *    |              +--------+    |
 *    |                |    ^      |
 *    |               (G)   |      |
 *    |                |   (D)     |
 *    |                v    |     (E)
 *    |              +--------+    |
 *    +--------(B)-- |  ping  | ---+
 *                   +--------+
 *
 * This module exposes functions for explicitly directing network
 * state as well declaring callback functions for state transitions
 * and network activity handling which must be implemented elsewhere
 *
 * The connection cycle is implements an exponential backoff routine
 *   t(n) = 2 * t(n - 1)
 *   t(0) = 15
 *
 * Network state can be explicitly driven:
 *   (A) net_cx:   enter connection/reconnection cycle
 *   (B) net_dx:   close network connection
 *   (C) net_free: free network connection
 *
 * Network state implicit transitions result in callbacks:
 *   (D) on connection success:   net_cb_cxed
 *   (E) on connection loss:      net_cb_dxed
 *   (F) on reconnection attempt: net_cb_rxng
 *   (G) on network timing out:   net_cb_ping
 *
 * Successful reads on stdin and connected sockets result in callbacks:
 *  - from stdin:  net_cb_inp
 *  - from socket: net_cb_soc
 */

struct connection;

/* Returns a connection, or NULL if limit is reached */
struct connection* connection(
	const char*,  /* host */
	const char*,  /* port */
	const void*); /* callback object */

/* Explicit direction of net state */
void net_cx  (struct connection*);
void net_dx  (struct connection*);
void net_free(struct connection*);
void net_poll(void);

/* Network callbacks */
void net_cb_read_inp(const char*, ssize_t);
void net_cb_read_soc(const char*, ssize_t, void*);
void net_cb_cxed(void*, const char*, ...);
void net_cb_dxed(void*, const char*, ...);
void net_cb_rxng(void*, const char*, ...);
void net_cb_ping(void*);

#endif
