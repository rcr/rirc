#include <string.h>

#include "src/io.h"
#include "src/handlers/ircv3.h"

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
static int ircv3_CAP_##CMD(struct server*, struct irc_message*);
IRCV3_HANDLERS
#undef X

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
	if (!strcmp(cmnd, #CMD)) return ircv3_CAP_##CMD(s, m);
	IRCV3_HANDLERS
	#undef X

	failf(s, "CAP: unrecognized subcommand '%s'", cmnd);
}

static int
ircv3_CAP_LS(struct server *s, struct irc_message *m)
{
	/* The last parameter is a space-separated list of
	 * capabilities. If no capabilities are available,
	 * an empty parameter MUST be sent.
	 *
	 * Servers MAY send multiple lines in response to
	 * CAP LS and CAP LIST. If the reply contains
	 * multiple lines, all but the last reply MUST
	 * have a parameter containing only an asterisk (*)
	 * preceding the capability list
	 *
	 * E.g.:
	 *   - CAP targ LS :
	 *   - CAP targ LS :cap-1 cap-2 ...
	 *   - CAP targ LS cap-1
	 *   - CAP targ LS * :cap-1 ...
	 *     CAP targ LS * :cap-2 ...
	 *     CAP targ LS :cap-3 ...
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

	return 0;
}

static int
ircv3_CAP_LIST(struct server *s, struct irc_message *m)
{
	(void)s;
	(void)m;
	return 0;
}

static int
ircv3_CAP_ACK(struct server *s, struct irc_message *m)
{
#if 0
	const char *arg;

	while ((arg = strsep(&(m)))) {

		debug("ircv3 cap ACK: %s", arg);

		#define X(CAP, VAR) \
		if (!strcmp(arg, CAP)) {               \
			s->ircv3_caps.VAR = IRCV3_CAP_ACK; \
		}
		IRCV3_CAPS
		#undef X
	}

	// TODO: this should actually check if any are pending?
	// TODO: this should check cap_ls is done
	if (1
	#define X(CAP, VAR) \
		&& s->ircv3_caps.VAR
		IRCV3_CAPS
	#undef X
	   ) {
		sendf(s, "CAP END");
	}
#endif
	(void)s;
	(void)m;
	return 0;
}

static int
ircv3_CAP_NAK(struct server *s, struct irc_message *m)
{
#if 0
	const char *arg;

	while ((arg = strsep(&(m)))) {

		debug("ircv3 cap NAK: %s", arg);

		#define X(CAP, VAR) \
		if (!strcmp(arg, CAP)) {               \
			s->ircv3_caps.VAR = IRCV3_CAP_NAK; \
		}
		IRCV3_CAPS
		#undef X
	}

	// TODO: this should actually check if any are pending?
	// TODO: this should check cap_ls is done
	if (1
	#define X(CAP, VAR) \
		&& s->ircv3_caps.VAR
		IRCV3_CAPS
	#undef X
	   ) {
		sendf(s, "CAP END");
	}
#endif
	(void)s;
	(void)m;
	return 0;
}

static int
ircv3_CAP_DEL(struct server *s, struct irc_message *m)
{
	(void)s;
	(void)m;
	return 0;
}

static int
ircv3_CAP_NEW(struct server *s, struct irc_message *m)
{
	(void)s;
	(void)m;
	return 0;
}
