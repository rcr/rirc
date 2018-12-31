// FIXME: use strerror_r here, elsewhere, move the io_strerror to utils

#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include "src/components/channel.h"
#include "src/components/server.h"
#include "src/handlers/irc_ctcp.gperf.out"
#include "src/handlers/irc_ctcp.h"
#include "src/io.h"
#include "src/state.h"
#include "src/utils/utils.h"

#define failf(S, ...) \
	do { server_err((S), __VA_ARGS__); \
	     return 1; \
	} while (0)

#define sendf(S, ...) \
	do { int ret; \
	     if ((ret = io_sendf((S)->connection, __VA_ARGS__))) \
	         failf((S), "Send fail: %s", io_err(ret)); \
	     return 0; \
	} while (0)

int
recv_ctcp_request(struct server *s, struct parsed_mesg *m)
{
	char *term;
	const char *command;
	const char *from;
	const struct ctcp_handler *ctcp;

	if (!(from = m->from))
		failf(s, "Received CTCP message from unknown sender");

	if (*m->trailing != '\001' || !(term = strchr(m->trailing + 1, '\001')))
		failf(s, "Received malformed CTCP message from %s", from);

	*m->trailing++ = *term = 0;

	if (!(command = getarg(&m->trailing, " ")))
		failf(s, "Received empty CTCP message from %s", from);

	if ((ctcp = ctcp_handler_lookup(command, strlen(command))))
		return ctcp->f_request(s, m);

	server_err(s, "Received unsupported CTCP command '%s' from %s", command, from);
	sendf(s, "NOTICE %s :\001ERRMSG Unsupported CTCP command: '%s'\001", from, command);
}

int
recv_ctcp_response(struct server *s, struct parsed_mesg *m)
{
	// TODO: should handle ERROR response too
	(void)s;
	(void)m;
	return 0;
}

static int
recv_ctcp_request_action(struct server *s, struct parsed_mesg *m)
{
	const char *targ;
	struct channel *c;

	if (!(targ = getarg(&m->params, " ")))
		failf(s, "CTCP ACTION: target is NULL");

	if (!s->cmp(targ, s->nick)) {
		if ((c = channel_list_get(&s->clist, m->from)) == NULL) {
			c = channel(m->from, CHANNEL_T_PRIVATE);
			c->activity = ACTIVITY_PINGED;
			c->server = s;
			channel_list_add(&s->clist, c);
		}
	} else if ((c = channel_list_get(&s->clist, targ)) == NULL) {
		failf(s, "CTCP ACTION: target '%s' not found", targ);
	}

	newlinef(c, 0, "*", "%s %s", m->from, m->trailing);

	return 0;
}

static int
recv_ctcp_request_clientinfo(struct server *s, struct parsed_mesg *m)
{
	if (str_trim(&m->trailing))
		server_msg(s, "CTCP CLIENTINFO from %s (%s)", m->from, m->trailing);
	else
		server_msg(s, "CTCP CLIENTINFO from %s", m->from);

	sendf(s, "NOTICE %s :\001CLIENTINFO ACTION CLIENTINFO PING SOURCE TIME VERSION\001", m->from);
}

static int
recv_ctcp_request_ping(struct server *s, struct parsed_mesg *m)
{
	server_msg(s, "CTCP PING from %s", m->from);

	sendf(s, "NOTICE %s :\001PING %s\001", m->from, m->trailing);
}

static int
recv_ctcp_request_source(struct server *s, struct parsed_mesg *m)
{
	if (str_trim(&m->trailing))
		server_msg(s, "CTCP SOURCE from %s (%s)", m->from, m->trailing);
	else
		server_msg(s, "CTCP SOURCE from %s", m->from);

	sendf(s, "NOTICE %s :\001SOURCE rcr.io/rirc\001", m->from);
}

static int
recv_ctcp_request_time(struct server *s, struct parsed_mesg *m)
{
	/* ISO 8601 */
	char buf[sizeof("1970-01-01T00:00:00Z+0000")];
	struct tm tm;
	time_t t = 0;

	if (str_trim(&m->trailing))
		server_msg(s, "CTCP TIME from %s (%s)", m->from, m->trailing);
	else
		server_msg(s, "CTCP TIME from %s", m->from);

#ifdef TESTING
	if (gmtime_r(&t, &tm) == NULL)
		failf(s, "CTCP TIME: gmtime_r error: %s", strerror(errno));
#else
	t = time(NULL);

	if (localtime_r(&t, &tm) == NULL)
		failf(s, "CTCP TIME: localtime_r error: %s", strerror(errno));
#endif

	if ((strftime(buf, sizeof(buf), "%FT%TZ%z", &tm)) == 0)
		failf(s, "CTCP TIME: strftime error");

	sendf(s, "NOTICE %s :\001TIME %s\001", m->from, buf);
}

static int
recv_ctcp_request_version(struct server *s, struct parsed_mesg *m)
{
	if (str_trim(&m->trailing))
		server_msg(s, "CTCP VERSION from %s (%s)", m->from, m->trailing);
	else
		server_msg(s, "CTCP VERSION from %s", m->from);

	sendf(s, "NOTICE %s :\001VERSION rirc v"VERSION" ("__DATE__")\001", m->from);
}

static int
recv_ctcp_response_action(struct server *s, struct parsed_mesg *m)
{
	(void)s;
	(void)m;
	return 0;
}

static int
recv_ctcp_response_clientinfo(struct server *s, struct parsed_mesg *m)
{
	(void)s;
	(void)m;
	return 0;
}

static int
recv_ctcp_response_ping(struct server *s, struct parsed_mesg *m)
{
	(void)s;
	(void)m;
	return 0;
}

static int
recv_ctcp_response_source(struct server *s, struct parsed_mesg *m)
{
	(void)s;
	(void)m;
	return 0;
}

static int
recv_ctcp_response_time(struct server *s, struct parsed_mesg *m)
{
	(void)s;
	(void)m;
	return 0;
}

static int
recv_ctcp_response_version(struct server *s, struct parsed_mesg *m)
{
	(void)s;
	(void)m;
	return 0;
}
