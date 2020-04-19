#include <string.h>

#include "src/handlers/ircv3.h"
#include "src/io.h"
#include "src/state.h"

#define failf(S, ...) \
	do { server_error((S), __VA_ARGS__); \
	     return 1; \
	} while (0)

#define sendf(S, ...) \
	do { int ret; \
	     if ((ret = io_sendf((S)->connection, __VA_ARGS__))) \
	         failf((S), "Send fail: %s", io_err(ret)); \
	} while (0)

#define IRCV3_HANDLERS \
	X(LIST) \
	X(LS) \
	X(ACK) \
	X(NAK) \
	X(DEL) \
	X(NEW)

#define X(CMD) \
static int ircv3_cap_##CMD(struct server*, struct irc_message*);
IRCV3_HANDLERS
#undef X

static int ircv3_cap_print(struct server*, const char*, char*);

int
ircv3_CAP(struct server *s, struct irc_message *m)
{
	char *targ;
	char *cmnd;

	if (!irc_message_param(m, &targ))
		failf(s, "CAP: target is null");

	if (!irc_message_param(m, &cmnd))
		failf(s, "CAP: command is null");

	#define X(CMD) \
	if (!strcmp(cmnd, #CMD)) return ircv3_cap_##CMD(s, m);
	IRCV3_HANDLERS
	#undef X

	failf(s, "CAP: unrecognized subcommand '%s'", cmnd);
}

static int
ircv3_cap_LS(struct server *s, struct irc_message *m)
{
	/* If no capabilities are available, an empty
	 * parameter MUST be sent.
	 *
	 * Servers MAY send multiple lines in response to
	 * CAP LS and CAP LIST. If the reply contains
	 * multiple lines, all but the last reply MUST
	 * have a parameter containing only an asterisk (*)
	 * preceding the capability list
	 *
	 * CAP <targ> LS [*] :[<cap 1> [...]]
	 */

	char *cap;
	char *caps;
	char *multiline;

	irc_message_param(m, &multiline);
	irc_message_param(m, &caps);

	if (!multiline)
		failf(s, "CAP: parameter is null");

	if (multiline && caps && strcmp(multiline, "*"))
		failf(s, "CAP: invalid parameters");

	if (!strcmp(multiline, "*") && !caps)
		failf(s, "CAP: parameter is null");

	if (!caps) {
		caps = multiline;
		multiline = NULL;
	}

	if (s->registered)
		return ircv3_cap_print(s, "LS", caps);

	while ((cap = strsep(&(caps)))) {

		debug("%s:%s -- CAP %s", s->host, s->port, cap);

		#define X(CAP, VAR) \
		if (!strcmp(cap, CAP)) \
			s->ircv3_caps.VAR = IRCV3_CAP_PENDING_SEND;
		IRCV3_CAPS
		#undef X
	}

	if (!multiline) {
		#define X(CAP, VAR) \
		if (s->ircv3_caps.VAR == IRCV3_CAP_PENDING_SEND) { \
			s->ircv3_caps.VAR = IRCV3_CAP_PENDING_RECV; \
			sendf(s, "CAP REQ :"CAP); \
		}
		IRCV3_CAPS
		#undef X
	}

	// TODO: message for no CAPS available when sending a cap-ls 
	// after registered... combine this with how cap LIST is printed
	// in some other function that gets type string and error
	// message string?
	//
	// ircv3_cap_print

	return 0;
}

static int
ircv3_cap_LIST(struct server *s, struct irc_message *m)
{
	/* If no capabilities are available, an empty
	 * parameter MUST be sent.
	 *
	 * Servers MAY send multiple lines in response to
	 * CAP LS and CAP LIST. If the reply contains
	 * multiple lines, all but the last reply MUST
	 * have a parameter containing only an asterisk (*)
	 * preceding the capability list
	 *
	 * CAP <targ> LIST [*] :[<cap 1> [...]]
	 */

	char *caps;
	char *multiline;

	irc_message_param(m, &multiline);
	irc_message_param(m, &caps);

	if (!multiline)
		failf(s, "CAP: parameter is null");

	if (multiline && caps && strcmp(multiline, "*"))
		failf(s, "CAP: invalid parameters");

	if (!strcmp(multiline, "*") && !caps)
		failf(s, "CAP: parameter is null");

	if (!caps)
		caps = multiline;

	return ircv3_cap_print(s, "LIST", caps);
}

static int
ircv3_cap_ACK(struct server *s, struct irc_message *m)
{
	/* Each capability name may be prefixed with a
	 * dash (-), indicating that this capability has
	 * been disabled as requested.
	 *
	 * If an ACK reply originating from the server is
	 * spread across multiple lines, a client MUST NOT
	 * change capabilities until the last ACK of the
	 * set is received. Equally, a server MUST NOT change
	 * the capabilities of the client until the last ACK
	 * of the set has been sent.
	 *
	 * CAP <targ> ACK :[-]<cap 1> [[-]<cap 2> [...]]
	 */

	(void)s;
	(void)m;
	return 0;
}

static int
ircv3_cap_NAK(struct server *s, struct irc_message *m)
{
	/* The server MUST NOT make any change to any
	 * capabilities if it replies with a NAK subcommand.
	 *
	 * CAP <targ> NAK :<cap 1> [<cap 2> [...]]
	 */

	(void)s;
	(void)m;
	return 0;
}

static int
ircv3_cap_DEL(struct server *s, struct irc_message *m)
{
	/* Upon receiving a CAP DEL message, the client MUST
	 * treat the listed capabilities as cancelled and no
	 * longer available. Clients SHOULD NOT send CAP REQ
	 * messages to cancel the capabilities in CAP DEL,
	 * as they have already been cancelled by the server.
	 *
	 * CAP <targ> DEL :<cap 1> [<cap 2> [...]]
	 */

	(void)s;
	(void)m;
	return 0;
}

static int
ircv3_cap_NEW(struct server *s, struct irc_message *m)
{
	/* Clients that support CAP NEW messages SHOULD respond
	 * with a CAP REQ message if they wish to enable one or
	 * more of the newly-offered capabilities.
	 *
	 * CAP <targ> NEW :<cap 1> [<cap 2> [...]]
	 */

	(void)s;
	(void)m;
	return 0;
}

static int
ircv3_cap_print(struct server *s, const char *cmnd, char *caps)
{
	if (!caps[0])
		server_info(s, "CAP %s: (no caps set)", cmnd);
	else
		server_info(s, "CAP %s: %s", cmnd, caps);

	return 0;
}
