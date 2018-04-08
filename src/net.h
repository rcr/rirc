#ifndef NET2_H
#define NET2_H


/* FIXME: refactoring, stubbed until removal */
struct server;
int sendf(char*, struct server*, const char*, ...);
void check_servers(void);
void server_connect(char*, char*, char*, char*);
void server_disconnect(struct server*, int, int, char*);


/* Network connection handling and state
 *
 * The state of a connection at any given time can be
 * described by one of the following:
 *
 *  - dxed: disconnected ~ Socket disconnected, passive
 *  - rxng: reconnecting ~ Socket disconnected, pending reconnect
 *  - cxng: connecting   ~ Socket connection in progress
 *  - cxed: connected    ~ Socket connected
 *  - ping: timing out   ~ Socket connected, network state in question
 *
 *                            +--------+
 *                 +----(B)-- |  rxng  |
 *                 |          +--------+
 *  INIT           |           |      ^
 *    v            |         (A,C)    |
 *    |            |           |     (E)
 *    |            v           v      |
 *    +--> +--------+ --(A)-> +--------+
 *         |  dxed  |         |  cxng  | <--+
 *    +--< +--------+ <-(B)-- +--------+    |
 *    |     ^      ^           |      ^    (F)
 *    v     |      |          (D)     |     |
 *  TERM    |      |           |     (F)    |
 *          |      |           v      |     |
 *          |      |          +--------+    |
 *          |      +----(B)-- |  cxed  |    |
 *          |                 +--------+    |
 *          |                  |      ^     |
 *          |                 (G)     |     |
 *          |                  |     (G)    |
 *          |                  v      |     |
 *          |                 +--------+    |
 *          +-----------(B)-- |  ping  | ---+
 *                            +--------+
 *                             v      ^
 *                             |      |
 *                             +--(G)-+
 *
 * This module exposes functions for explicitly directing network
 * state as well declaring callback functions for state transitions
 * and network activity handling which must be implemented elsewhere
 *
 * Network state can be explicitly driven, returning non-zero error:
 *   (A) net_cx: establish network connection
 *   (B) net_dx: close network connection
 *
 * Network state implicit transitions result in informational callbacks:
 *   (C) on connection attempt: net_cb_cxng
 *   (D) on connection success: net_cb_cxed
 *   (E) on connection failure: net_cb_fail
 *   (F) on connection loss:    net_cb_lost
 *   (G) on network ping event: net_cb_ping
 *
 * Successful reads on stdin and connected sockets result in data callbacks:
 *   from stdin:  net_cb_read_inp
 *   from socket: net_cb_read_soc
 *
 * Failed connection attempts enter a retry cycle with exponential
 * backoff time given by:
 *   t(n) = t(n - 1) * factor
 *   t(0) = base
 */

#define NET_MAX_CONNECTIONS 8

struct connection;

/* Returns a connection, or NULL if limit is reached */
struct connection* connection(
	const void*,  /* callback object */
	const char*,  /* host */
	const char*); /* port */

void net_free(struct connection*);
void net_poll(void);

/* Formatted write to connection */
int net_sendf(struct connection*, const char*, ...);

/* Explicit direction of net state */
int net_cx(struct connection*);
int net_dx(struct connection*);

/* Informational network state callbacks */
void net_cb_cxng(const void*, const char*, ...);
void net_cb_cxed(const void*, const char*, ...);
void net_cb_fail(const void*, const char*, ...);
void net_cb_lost(const void*, const char*, ...);
void net_cb_ping(const void*, unsigned int);

/* Network data callback */
void net_cb_read_inp(char*, size_t);
void net_cb_read_soc(char*, size_t, const void*);

#endif
