#include "src/handlers/ircv3.h"

#include "src/io.h"
#include "src/state.h"

#include <string.h>

#define failf(S, ...) \
	do { server_error((S), __VA_ARGS__); \
	     return 1; \
	} while (0)

#define sendf(S, ...) \
	do { int ret; \
	     if ((ret = io_sendf((S)->connection, __VA_ARGS__))) \
	         failf((S), "Send fail: %s", io_err(ret)); \
	} while (0)

#define IRCV3_RECV_HANDLERS \
	X(LIST) \
	X(LS) \
	X(ACK) \
	X(NAK) \
	X(DEL) \
	X(NEW)

#define X(CMD) \
static int ircv3_recv_cap_##CMD(struct server*, struct irc_message*);
IRCV3_RECV_HANDLERS
#undef X

static int ircv3_cap_req_count(struct ircv3_caps*);
static int ircv3_cap_req_send(struct ircv3_caps*, struct server*);

int
ircv3_recv_CAP(struct server *s, struct irc_message *m)
{
	char *targ;
	char *cmnd;

	if (!irc_message_param(m, &targ))
		failf(s, "CAP: target is null");

	if (!irc_message_param(m, &cmnd))
		failf(s, "CAP: command is null");

	#define X(CMD) \
	if (!strcmp(cmnd, #CMD)) \
		return ircv3_recv_cap_##CMD(s, m);
	IRCV3_RECV_HANDLERS
	#undef X

	failf(s, "CAP: unrecognized subcommand '%s'", cmnd);
}

static int
ircv3_recv_cap_LS(struct server *s, struct irc_message *m)
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
	 * CAP <targ> LS [*] :[<cap_1> [...]]
	 */

	char *cap;
	char *caps;
	char *multiline;

	irc_message_param(m, &multiline);
	irc_message_param(m, &caps);

	if (!multiline)
		failf(s, "CAP LS: parameter is null");

	if (!strcmp(multiline, "*") && !caps)
		failf(s, "CAP LS: parameter is null");

	if (strcmp(multiline, "*") && caps)
		failf(s, "CAP LS: invalid parameters");

	if (!caps) {
		caps = multiline;
		multiline = NULL;
	}

	if (s->registered) {
		server_info(s, "CAP LS: %s", (*caps ? caps : "(no capabilities)"));
		return 0;
	}

	while ((cap = irc_strsep(&(caps)))) {

		struct ircv3_cap *c;

		if (!(c = ircv3_cap_get(&(s->ircv3_caps), cap)))
			continue;

		c->supported = 1;

		if (c->req_auto)
			c->req = 1;
	}

	if (multiline)
		return 0;

	if (ircv3_cap_req_count(&(s->ircv3_caps)))
		return ircv3_cap_req_send(&(s->ircv3_caps), s);

	sendf(s, "CAP END");

	return 0;
}

static int
ircv3_recv_cap_LIST(struct server *s, struct irc_message *m)
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
	 * CAP <targ> LIST [*] :[<cap_1> [...]]
	 */

	char *caps;
	char *multiline;

	irc_message_param(m, &multiline);
	irc_message_param(m, &caps);

	if (!multiline)
		failf(s, "CAP LIST: parameter is null");

	if (multiline && caps && strcmp(multiline, "*"))
		failf(s, "CAP LIST: invalid parameters");

	if (!strcmp(multiline, "*") && !caps)
		failf(s, "CAP LIST: parameter is null");

	if (!caps)
		caps = multiline;

	server_info(s, "CAP LIST: %s", (*caps ? caps : "(no capabilities)"));

	return 0;
}

static int
ircv3_recv_cap_ACK(struct server *s, struct irc_message *m)
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
	 * CAP <targ> ACK :[-]<cap_1> [[-]<cap_2> [...]]
	 */

	char *cap;
	char *caps;
	int errors = 0;

	if (!irc_message_param(m, &caps))
		failf(s, "CAP ACK: parameter is null");

	if (!(cap = irc_strsep(&(caps))))
		failf(s, "CAP ACK: parameter is empty");

	do {
		int unset;
		struct ircv3_cap *c;

		if ((unset = (*cap == '-')))
			cap++;

		if (!(c = ircv3_cap_get(&(s->ircv3_caps), cap))) {
			server_error(s, "CAP ACK: '%s' not supported", cap);
			errors++;
			continue;
		}

		if (!c->req) {
			server_error(s, "CAP ACK: '%s%s' was not requested", (unset ? "-" : ""), cap);
			errors++;
			continue;
		}

		if (!unset && c->set) {
			server_error(s, "CAP ACK: '%s' was set", cap);
			errors++;
			continue;
		}

		if (unset && !c->set) {
			server_error(s, "CAP ACK: '%s' was not set", cap);
			errors++;
			continue;
		}

		c->req = 0;
		c->set = !unset;

		server_info(s, "capability change accepted: %s%s", (unset ? "-" : ""), cap);

	} while ((cap = irc_strsep(&(caps))));

	if (errors)
		failf(s, "CAP ACK: parameter errors");

	if (!s->registered && !ircv3_cap_req_count(&(s->ircv3_caps)))
		sendf(s, "CAP END");

	return 0;
}

static int
ircv3_recv_cap_NAK(struct server *s, struct irc_message *m)
{
	/* The server MUST NOT make any change to any
	 * capabilities if it replies with a NAK subcommand.
	 *
	 * CAP <targ> NAK :<cap_1> [<cap_2> [...]]
	 */

	char *cap;
	char *caps;

	if (!irc_message_param(m, &caps))
		failf(s, "CAP NAK: parameter is null");

	if (!(cap = irc_strsep(&(caps))))
		failf(s, "CAP NAK: parameter is empty");

	do {
		struct ircv3_cap *c;

		if ((c = ircv3_cap_get(&(s->ircv3_caps), cap)))
			c->req = 0;

		server_info(s, "capability change rejected: %s", cap);

	} while ((cap = irc_strsep(&(caps))));

	if (!s->registered && !ircv3_cap_req_count(&(s->ircv3_caps)))
		sendf(s, "CAP END");

	return 0;
}

static int
ircv3_recv_cap_DEL(struct server *s, struct irc_message *m)
{
	/* Upon receiving a CAP DEL message, the client MUST
	 * treat the listed capabilities as cancelled and no
	 * longer available. Clients SHOULD NOT send CAP REQ
	 * messages to cancel the capabilities in CAP DEL,
	 * as they have already been cancelled by the server.
	 *
	 * CAP <targ> DEL :<cap_1> [<cap_2> [...]]
	 */

	char *cap;
	char *caps;

	if (!irc_message_param(m, &caps))
		failf(s, "CAP DEL: parameter is null");

	if (!(cap = irc_strsep(&(caps))))
		failf(s, "CAP DEL: parameter is empty");

	do {
		struct ircv3_cap *c;

		if (!(c = ircv3_cap_get(&(s->ircv3_caps), cap)))
			continue;

		if (!c->supports_del)
			failf(s, "CAP DEL: '%s' doesn't support DEL", cap);

		c->req = 0;
		c->set = 0;
		c->supported = 0;

		server_info(s, "capability lost: %s", cap);

	} while ((cap = irc_strsep(&(caps))));

	return 0;
}

static int
ircv3_recv_cap_NEW(struct server *s, struct irc_message *m)
{
	/* Clients that support CAP NEW messages SHOULD respond
	 * with a CAP REQ message if they wish to enable one or
	 * more of the newly-offered capabilities.
	 *
	 * CAP <targ> NEW :<cap_1> [<cap_2> [...]]
	 */

	char *cap;
	char *caps;
	struct ircv3_caps batch_reqs = {0};

	if (!irc_message_param(m, &caps))
		failf(s, "CAP NEW: parameter is null");

	if (!(cap = irc_strsep(&(caps))))
		failf(s, "CAP NEW: parameter is empty");

	do {
		struct ircv3_cap *c1 = ircv3_cap_get(&(s->ircv3_caps), cap);
		struct ircv3_cap *c2 = ircv3_cap_get(&batch_reqs, cap);

		if (!c1 || !c2)
			continue;

		c1->supported = 1;

		if (c1->set || c1->req || !c1->req_auto) {
			server_info(s, "new capability: %s", cap);
		} else {
			c1->req = 1;
			c2->req = 1;
			server_info(s, "new capability: %s (auto-req)", cap);
		}
	} while ((cap = irc_strsep(&(caps))));

	if (ircv3_cap_req_count(&batch_reqs))
		return ircv3_cap_req_send(&batch_reqs, s);

	return 0;
}

static int
ircv3_cap_req_count(struct ircv3_caps *caps)
{
	#define X(CAP, VAR, ATTRS) + !!caps->VAR.req
	return 0 IRCV3_CAPS;
	#undef X
}

static int
ircv3_cap_req_send(struct ircv3_caps *caps, struct server *s)
{
	int mid = 0;
	int ret;

	#define X(CAP, VAR, ATTRS) \
	const char *sep_##VAR = (caps->VAR.req && mid++ ? " " : ""); \
	const char *str_##VAR = (caps->VAR.req          ? CAP : "");
	IRCV3_CAPS
	#undef X

	if ((ret = io_sendf(s->connection, "CAP REQ :"
		#define X(CAP, VAR, ATTRS) "%s%s"
		IRCV3_CAPS
		#undef X
		#define X(CAP, VAR, ATTRS) ,sep_##VAR,str_##VAR
		IRCV3_CAPS
		#undef X
	))) {
		failf(s, "Send fail: %s", io_err(ret));
	}

	return 0;
}
