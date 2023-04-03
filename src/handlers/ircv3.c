#include "src/handlers/ircv3.h"

#include "src/io.h"
#include "src/state.h"

#include "mbedtls/base64.h"

#include <string.h>

#define failf(S, ...) \
	do { server_error((S), __VA_ARGS__); \
	     return -1; \
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
static int ircv3_cap_end(struct server*);
static int ircv3_sasl_init(struct server*);

static int ircv3_recv_AUTHENTICATE_EXTERNAL(struct server*, struct irc_message*);
static int ircv3_recv_AUTHENTICATE_PLAIN(struct server*, struct irc_message*);

int
ircv3_recv_AUTHENTICATE(struct server *s, struct irc_message *m)
{
	if (s->ircv3_sasl.mech == IRCV3_SASL_MECH_NONE)
		failf(s, "AUTHENTICATE: no SASL mechanism");

	if (s->ircv3_sasl.mech == IRCV3_SASL_MECH_EXTERNAL)
		return (ircv3_recv_AUTHENTICATE_EXTERNAL(s, m));

	if (s->ircv3_sasl.mech == IRCV3_SASL_MECH_PLAIN)
		return (ircv3_recv_AUTHENTICATE_PLAIN(s, m));

	fatal("unknown SASL authentication mechanism");

	return 0;
}

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

int
ircv3_numeric_900(struct server *s, struct irc_message *m)
{
	/* <nick>!<ident>@<host> <account> :You are now logged in as <user> */

	char *account;
	char *message;
	char *nick;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_LOGGEDIN: missing nick");

	if (!irc_message_param(m, &account))
		failf(s, "RPL_LOGGEDIN: missing account");

	irc_message_param(m, &message);

	if (message && *message)
		server_info(s, "SASL success: %s", message);
	else
		server_info(s, "SASL success: you are logged in as %s", account);

	return 0;
}

int
ircv3_numeric_901(struct server *s, struct irc_message *m)
{
	/* <nick>!<ident>@<host> :You are now logged out */

	char *message;
	char *nick;

	if (!irc_message_param(m, &nick))
		failf(s, "RPL_LOGGEDOUT: missing nick");

	irc_message_param(m, &message);

	if (message && *message)
		server_info(s, "%s", message);
	else
		server_info(s, "You are now logged out");

	return 0;
}

int
ircv3_numeric_902(struct server *s, struct irc_message *m)
{
	/* :You must use a nick assigned to you */

	char *message;

	irc_message_param(m, &message);

	if (message && *message)
		server_error(s, "%s", message);
	else
		server_error(s, "You must use a nick assigned to you");

	s->ircv3_sasl.state = IRCV3_SASL_STATE_NONE;

	if (!s->registered)
		io_dx(s->connection, 0);

	return 0;
}

int
ircv3_numeric_903(struct server *s, struct irc_message *m)
{
	/* :SASL authentication successful */

	char *message;

	irc_message_param(m, &message);

	if (message && *message)
		server_info(s, "%s", message);
	else
		server_info(s, "SASL authentication successful");

	s->ircv3_sasl.state = IRCV3_SASL_STATE_AUTHENTICATED;

	return ircv3_cap_end(s);
}

int
ircv3_numeric_904(struct server *s, struct irc_message *m)
{
	/* :SASL authentication failed */

	char *message;

	irc_message_param(m, &message);

	if (message && *message)
		server_error(s, "%s", message);
	else
		server_error(s, "SASL authentication failed");

	s->ircv3_sasl.state = IRCV3_SASL_STATE_NONE;

	if (!s->registered)
		io_dx(s->connection, 0);

	return 0;
}

int
ircv3_numeric_905(struct server *s, struct irc_message *m)
{
	/* :SASL message too long */

	char *message;

	irc_message_param(m, &message);

	if (message && *message)
		server_error(s, "%s", message);
	else
		server_error(s, "SASL message too long");

	s->ircv3_sasl.state = IRCV3_SASL_STATE_NONE;

	if (!s->registered)
		io_dx(s->connection, 0);

	return 0;
}

int
ircv3_numeric_906(struct server *s, struct irc_message *m)
{
	/* :SASL authentication aborted */

	char *message;

	irc_message_param(m, &message);

	if (message && *message)
		server_error(s, "%s", message);
	else
		server_error(s, "SASL authentication aborted");

	s->ircv3_sasl.state = IRCV3_SASL_STATE_NONE;

	if (!s->registered)
		io_dx(s->connection, 0);

	return 0;
}

int
ircv3_numeric_907(struct server *s, struct irc_message *m)
{
	/* :You have already authenticated using SASL */

	char *message;

	irc_message_param(m, &message);

	if (message && *message)
		server_error(s, "%s", message);
	else
		server_error(s, "You have already authenticated using SASL");

	s->ircv3_sasl.state = IRCV3_SASL_STATE_NONE;

	if (!s->registered)
		io_dx(s->connection, 0);

	return 0;
}

int
ircv3_numeric_908(struct server *s, struct irc_message *m)
{
	/* <mechanisms> :are available SASL mechanisms */

	char *mechanisms;
	char *message;

	if (!irc_message_param(m, &mechanisms))
		failf(s, "RPL_SASLMECHS: missing mechanisms");

	irc_message_param(m, &message);

	if (message && *message)
		server_info(s, "%s %s", mechanisms, message);
	else
		server_info(s, "%s are available SASL mechanisms", mechanisms);

	return 0;
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

	char *cap_key;
	char *cap_val;
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

	while ((cap_key = irc_strsep(&(caps)))) {

		struct ircv3_cap *c;

		if ((cap_val = strchr(cap_key, '=')))
			*cap_val++ = 0;

		if (!(c = ircv3_cap_get(&(s->ircv3_caps), cap_key)))
			continue;

		c->supported = 1;
		c->val = (cap_val ? irc_strdup(cap_val) : NULL);

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

		server_info(s, "capability change accepted: %s%s%s%s",
			(unset ? "-" : ""), cap, (c->val ? "=" : ""), (c->val ? c->val : ""));

		if (!strcmp(cap, "sasl"))
			ircv3_sasl_init(s);

	} while ((cap = irc_strsep(&(caps))));

	if (errors)
		failf(s, "CAP ACK: parameter errors");

	return ircv3_cap_end(s);
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

	return ircv3_cap_end(s);
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
ircv3_recv_AUTHENTICATE_EXTERNAL(struct server *s, struct irc_message *m)
{
	/* C: AUTHENTICATE EXTERNAL
	 * S: AUTHENTICATE +
	 * C: AUTHENTICATE +
	 */

	char *resp;

	if (s->ircv3_sasl.state != IRCV3_SASL_STATE_REQ_MECH)
		failf(s, "Invalid SASL state for mechanism EXTERNAL: %d", s->ircv3_sasl.state);

	if (!irc_message_param(m, &resp))
		failf(s, "Invalid SASL response for mechanism EXTERNAL: response is null");

	if (strcmp(resp, "+"))
		failf(s, "Invalid SASL response for mechanism EXTERNAL: '%s'", resp);

	sendf(s, "AUTHENTICATE +");

	return 0;
}

static int
ircv3_recv_AUTHENTICATE_PLAIN(struct server *s, struct irc_message *m)
{
	/* C: AUTHENTICATE PLAIN
	 * S: AUTHENTICATE +
	 * C: AUTHENTICATE base64(<authzid><null><authcid><null><passwd>)
	 */

	/* (((4 * 300 / 3) + 3) & ~3) */
	char *resp;
	unsigned char resp_dec[300];
	unsigned char resp_enc[400];
	size_t len;

	if (!s->ircv3_sasl.user || !*s->ircv3_sasl.user)
		failf(s, "SASL mechanism PLAIN requires a username");

	if (!s->ircv3_sasl.pass || !*s->ircv3_sasl.pass)
		failf(s, "SASL mechanism PLAIN requires a password");

	if (s->ircv3_sasl.state != IRCV3_SASL_STATE_REQ_MECH)
		failf(s, "Invalid SASL state for mechanism PLAIN: %d", s->ircv3_sasl.state);

	if (!irc_message_param(m, &resp))
		failf(s, "Invalid SASL response for mechanism PLAIN: response is null");

	if (strcmp(resp, "+"))
		failf(s, "Invalid SASL response for mechanism PLAIN: '%s'", resp);

	len = snprintf((char *)resp_dec, sizeof(resp_dec), "%s%c%s%c%s",
		s->ircv3_sasl.user, 0,
		s->ircv3_sasl.user, 0,
		s->ircv3_sasl.pass);

	if (len >= sizeof(resp_dec))
		failf(s, "SASL decoded auth message too long");

	if (mbedtls_base64_encode(resp_enc, sizeof(resp_enc), &len, resp_dec, len))
		failf(s, "SASL encoded auth message too long");

	sendf(s, "AUTHENTICATE %s", resp_enc);

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

static int
ircv3_cap_end(struct server *s)
{
	/* Previously registered or doesn't support IRCv3 */
	if (s->registered)
		return 0;

	/* IRCv3 CAP negotiation in progress */
	if (ircv3_cap_req_count(&(s->ircv3_caps)))
		return 0;

	/* IRCv3 SASL authentication in progress */
	if (s->ircv3_sasl.state != IRCV3_SASL_STATE_NONE
	 && s->ircv3_sasl.state != IRCV3_SASL_STATE_AUTHENTICATED)
		return 0;

	sendf(s, "CAP END");
	
	return 0;
}

static int
ircv3_sasl_init(struct server *s)
{
	if (s->ircv3_sasl.mech == IRCV3_SASL_MECH_NONE)
		return 0;

	switch (s->ircv3_sasl.state) {

		/* Start authentication process */
		case IRCV3_SASL_STATE_NONE:
			break;

		/* Authentication in progress */
		case IRCV3_SASL_STATE_REQ_MECH:
			return 0;

		/* Previously authenticated */
		case IRCV3_SASL_STATE_AUTHENTICATED:
			return 0;

		default:
			fatal("unknown SASL state");
	}

	switch (s->ircv3_sasl.mech) {
		case IRCV3_SASL_MECH_EXTERNAL:
			sendf(s, "AUTHENTICATE EXTERNAL");
			break;
		case IRCV3_SASL_MECH_PLAIN:
			sendf(s, "AUTHENTICATE PLAIN");
			break;
		default:
			fatal("unknown SASL authentication mechanism");
	}

	s->ircv3_sasl.state = IRCV3_SASL_STATE_REQ_MECH;

	return 0;
}
