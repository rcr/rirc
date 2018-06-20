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
 *          |                  |     (H)    |
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
 * Network state implicit transitions result in informational callback types:
 *   (C) on connection attempt:  IO_CB_INFO
 *   (E) on connection failure:  IO_CB_ERROR
 *   (D) on connection success:  IO_CB_CXED
 *   (F) on connection loss:     IO_CB_DXED
 *   (G) on network ping normal: IO_CB_PING_0
 *   (H) on network ping rising: IO_CB_PING_N
 *
 * Successful reads on stdin and connected sockets result in data callbacks:
 *   from stdin:  io_cb_read_inp
 *   from socket: io_cb_read_soc
 *
 * Signals registered to be caught result in non-signal handler context
 * callback with type IO_CB_SIGNAL
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

enum io_sig_t
{
	IO_SIG_INVALID,
	IO_SIGWINCH,
	IO_SIG_SIZE
};

enum io_cb_t
{
	IO_CB_INVALID,
	IO_CB_CXED,   /* fmt, [args] */
	IO_CB_DXED,   /* fmt, [args] */
	IO_CB_ERROR,  /* fmt, [args] */
	IO_CB_INFO,   /* fmt, [args] */
	IO_CB_PING_0, /* ping = 0 */
	IO_CB_PING_N, /* ping = N */
	IO_CB_SIGNAL, /* io_sig_t sig */
	IO_CB_SIZE
};

/* Returns a connection, or NULL if limit is reached */
struct connection* connection(
	const void*,  /* callback object */
	const char*,  /* host */
	const char*); /* port */

void io_free(struct connection*);

/* Start non-returning IO context */
void io_loop(void (*)(void));

/* Formatted write to connection */
int io_sendf(struct connection*, const char*, ...);

/* Explicit direction of net state */
int io_cx(struct connection*);
int io_dx(struct connection*);

/* IO state callback */
void io_cb(enum io_cb_t, const void*, ...);

/* IO data callback */
void io_cb_read_inp(char*, size_t);
void io_cb_read_soc(char*, size_t, const void*);

/* IO error string */
const char* io_err(int);

#endif
