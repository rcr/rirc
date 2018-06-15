#ifndef IO_H
#define IO_H

/* FIXME: refactoring, stubbed until removal */
struct server;
int sendf(char*, struct server*, const char*, ...);
void server_disconnect(struct server*, int, int, char*);


/* Handling off all network io, user input and signals
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
 *   (A) io_cx: establish network connection
 *   (B) io_dx: close network connection
 *
 * Network state implicit transitions result in informational callbacks:
 *   (C) on connection attempt: io_cb_cxng
 *   (D) on connection success: io_cb_cxed
 *   (E) on connection failure: io_cb_fail
 *   (F) on connection loss:    io_cb_lost
 *   (G) on network ping event: io_cb_ping
 *
 * Successful reads on stdin and connected sockets result in data callbacks:
 *   from stdin:  io_cb_read_inp
 *   from socket: io_cb_read_soc
 *
 * Signals registered to be caught result in non-signal handler context callback:
 *   io_cb_signal
 *
 * Failed connection attempts enter a retry cycle with exponential
 * backoff time given by:
 *   t(n) = t(n - 1) * factor
 *   t(0) = base
 *
 * Calling io_loop starts the io context and never returns, a callback
 * function can be passed to io_loop which is executed on all input events
 */

#define IO_MAX_CONNECTIONS 8

struct connection;

/* Returns a connection, or NULL if limit is reached */
struct connection* connection(
	const void*,  /* callback object */
	const char*,  /* host */
	const char*); /* port */

void io_free(struct connection*);
void io_loop(void (*)(void));

/* Formatted write to connection */
int io_sendf(struct connection*, const char*, ...);

/* Explicit direction of net state */
int io_cx(struct connection*);
int io_dx(struct connection*);

/* Informational network state callbacks */
void io_cb_cxng(const void*, const char*, ...);
void io_cb_cxed(const void*, const char*, ...);
void io_cb_fail(const void*, const char*, ...);
void io_cb_lost(const void*, const char*, ...);
void io_cb_ping(const void*, unsigned int);

/* Signal callback in non-signal handler context */
void io_cb_signal(int);

/* Network data callback */
void io_cb_read_inp(char*, size_t);
void io_cb_read_soc(char*, size_t, const void*);

/* Get error string */
const char* io_err(int);

#endif
