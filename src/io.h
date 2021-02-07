#ifndef RIRC_IO_H
#define RIRC_IO_H

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
 *                             +--------+
 *                 +----(B1)-- |  rxng  |
 *                 |           +--------+
 *                 |            |      ^
 *   INIT          |         (A2,C)    |
 *    v            |            |     (E)
 *    |            v            v      |
 *    |    +--------+ --(A1)-> +--------+
 *    +--> |  dxed  |          |  cxng  | <--+
 *         +--------+ <-(B2)-- +--------+    |
 *          ^      ^            |      ^   (F2)
 *          |      |           (D)     |     |
 *          |      |            |    (F1)    |
 *          |      |            v      |     |
 *          |      |           +--------+    |
 *          |      +----(B3)-- |  cxed  |    |
 *          |                  +--------+    |
 *          |                   |      ^     |
 *          |                  (G)     |     |
 *          |                   |     (I)    |
 *          |                   v      |     |
 *          |                  +--------+    |
 *          +-----------(B4)-- |  ping  | ---+
 *                             +--------+
 *                              v      ^
 *                              |      |
 *                              +--(H)-+
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
 *   (D) on connection success:  io_cb_cxed
 *   (F) on connection loss:     io_cb_dxed
 *   (G) on ping timeout start:  io_cb_ping
 *   (H) on ping timeout update: io_cb_ping
 *   (I) on ping normal:         io_cb_ping
 *
 * Successful reads on stdin and connected sockets result in data callbacks:
 *   from stdin:  io_cb_read_inp
 *   from socket: io_cb_read_soc
 *
 * SIGWINCH results in a non signal-handler context callback io_cb_singwinch
 *
 * Failed connection attempts enter a retry cycle with exponential
 * backoff time given by:
 *   t(n) = t(n - 1) * factor
 *   t(0) = base
 *
 * Calling io_start starts the io context and doesn't return until after
 * a call to io_stop
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define IO_IPV_UNSPEC        (1 << 1)
#define IO_IPV_4             (1 << 2)
#define IO_IPV_6             (1 << 3)
#define IO_TLS_ENABLED       (1 << 4)
#define IO_TLS_DISABLED      (1 << 5)
#define IO_TLS_VRFY_DISABLED (1 << 6)
#define IO_TLS_VRFY_OPTIONAL (1 << 7)
#define IO_TLS_VRFY_REQUIRED (1 << 8)

struct connection;

struct connection* connection(
	const void*, /* callback object */
	const char*, /* host */
	const char*, /* port */
	uint32_t);   /* flags */

void connection_free(struct connection*);

/* Explicit direction of net state */
int io_cx(struct connection*);
int io_dx(struct connection*);

/* Formatted write to connection */
int io_sendf(struct connection*, const char*, ...);

/* IO error string */
const char* io_err(int);

/* IO data callback */
void io_cb_read_inp(char*, size_t);
void io_cb_read_soc(char*, size_t, const void*);

/* IO event callbacks */
void io_cb_cxed(const void*);
void io_cb_dxed(const void*);
void io_cb_ping(const void*, unsigned);
void io_cb_sigwinch(unsigned, unsigned);

/* IO informational callbacks */
void io_cb_error(const void*, const char*, ...);
void io_cb_info(const void*, const char*, ...);

void io_init(void);
void io_start(void);
void io_stop(void);

#endif
