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
 *      |              |   (E)
 *      v              v    |
 *  +------+ --(A)-> +--------+
 *  | dxed |         |  cxng  | <--+
 *  +------+ <-(B)-- +--------+    |
 *    ^  ^             |    ^      |
 *    |  |            (C)   |      |
 *    |  |             |   (D)     |
 *    |  |             v    |      |
 *    |  |           +--------+    |
 *    |  +-----(B)-- |  cxed  |    |
 *    |              +--------+    |
 *    |                |    ^      |
 *    |               (F)   |      |
 *    |                |   (C)     |
 *    |                v    |     (D)
 *    |              +--------+    |
 *    +--------(B)-- |  ping  | ---+
 *                   +--------+
 *
 * This module exposes functions for explicitly directing network
 * state as well declaring callback functions for state transitions
 * and network activity handling which must be implemented elsewhere
 *
 * The connection cycle is implements a backoff routine:
 *   t(n) = t(n - 1) * factor
 *   t(0) = base
 *
 * Network state can be explicitly driven, returning network error code:
 *   (A) net_cx: enter connection/reconnection cycle
 *   (B) net_dx: close network connection
 *
 * Network state implicit transitions result in informational callbacks:
 *   (C) on connection success:   net_cb_cxed
 *   (D) on connection loss:      net_cb_dxed
 *   (E) on reconnection attempt: net_cb_rxng
 *   (F) on network timing out:   net_cb_ping
 *
 * Successful reads on stdin and connected sockets result in data callbacks:
 *  - from stdin:  net_cb_inp
 *  - from socket: net_cb_soc
 */

struct connection;

/* Returns a connection, or NULL if limit is reached */
struct connection* connection(
	const char*,  /* host */
	const char*,  /* port */
	const void*); /* callback object */

void net_free(struct connection*);
void net_poll(void);

/* Explicit direction of net state */
int net_cx(struct connection*);
int net_dx(struct connection*);

/* Formatted write to connection */
int net_sendf(struct connection*, const char*, ...);

/* Informational network state callbacks */
void net_cb_cxed(const void*, const char*, ...);
void net_cb_dxed(const void*, const char*, ...);
void net_cb_rxng(const void*, const char*, ...);
void net_cb_ping(const void*, int);

/* Network data callback */
void net_cb_read_inp(const char*, size_t);
void net_cb_read_soc(const char*, size_t, const void*);

/* Network error code string */
const char* net_strerr(int);

#endif
