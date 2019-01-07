#include <ctype.h>
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
	char *command;
	char *term;
	const char *from;
	const struct ctcp_handler *ctcp;

	if (!(from = m->from))
		failf(s, "Received CTCP request from unknown sender");

	if (*m->trailing != '\001')
		failf(s, "Received malformed CTCP request from %s", from);

	if ((term = strchr(m->trailing + 1, '\001')))
		*term = 0;

	*m->trailing++ = 0;

	if (!(command = getarg(&m->trailing, " ")))
		failf(s, "Received empty CTCP request from %s", from);

	for (char *p = command; *p; p++)
		*p = toupper(*p);

	if ((ctcp = ctcp_handler_lookup(command, strlen(command))))
		return ctcp->f_request(s, m);

	failf(s, "Received unsupported CTCP request '%s' from %s", command, from);
}

int
recv_ctcp_response(struct server *s, struct parsed_mesg *m)
{
	char *command;
	char *term;
	const char *from;
	const struct ctcp_handler *ctcp;

	if (!(from = m->from))
		failf(s, "Received CTCP response from unknown sender");

	if (*m->trailing != '\001')
		failf(s, "Received malformed CTCP response from %s", from);

	if ((term = strchr(m->trailing + 1, '\001')))
		*term = 0;

	*m->trailing++ = 0;

	if (!(command = getarg(&m->trailing, " ")))
		failf(s, "Received empty CTCP response from %s", from);

	for (char *p = command; *p; p++)
		*p = toupper(*p);

	if ((ctcp = ctcp_handler_lookup(command, strlen(command))))
		return ctcp->f_response(s, m);

	failf(s, "Received unsupported CTCP response '%s' from %s", command, from);
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

	if (str_trim(&m->trailing))
		newlinef(c, 0, "*", "%s %s", m->from, m->trailing);
	else
		newlinef(c, 0, "*", "%s", m->from);

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
	char buf[sizeof("1970-01-01T00:00:00")];
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

	if ((strftime(buf, sizeof(buf), "%FT%T", &tm)) == 0)
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
recv_ctcp_response_clientinfo(struct server *s, struct parsed_mesg *m)
{
	if (!str_trim(&m->trailing))
		failf(s, "CTCP CLIENTINFO response from %s: empty message", m->from);

	server_msg(s, "CTCP CLIENTINFO response from %s: %s", m->from, m->trailing);
	return 0;
}

static int
recv_ctcp_response_ping(struct server *s, struct parsed_mesg *m)
{
	const char *sec;
	const char *usec;
	long long unsigned res;
	long long unsigned res_sec;
	long long unsigned res_usec;
	long unsigned t1_sec;
	long unsigned t1_usec;
	long unsigned t2_sec;
	long unsigned t2_usec;
	struct timeval t;

	if (!(sec = getarg(&m->trailing, " ")))
		failf(s, "CTCP PING response from %s: sec is NULL", m->from);

	if (!(usec = getarg(&m->trailing, " ")))
		failf(s, "CTCP PING response from %s: usec is NULL", m->from);

	for (const char *p = sec; *p; p++) {
		if (!(isdigit(*p)))
			failf(s, "CTCP PING response from %s: sec is invalid", m->from);
	}

	for (const char *p = usec; *p; p++) {
		if (!(isdigit(*p)))
			failf(s, "CTCP PING response from %s: usec is invalid", m->from);
	}

#ifdef TESTING
	(void)t;

	t2_sec = 123;
	t2_usec = 456789;
#else
	(void)gettimeofday(&t, NULL);

	t2_sec = (long unsigned) t.tv_sec;
	t2_usec = (long unsigned) t.tv_usec;
#endif

	errno = 0;

	t1_sec = strtoul(sec, NULL, 10);
	t1_usec = strtoul(usec, NULL, 10);

	if (errno)
		failf(s, "CTCP PING response from %s: failed to parse timestamp", m->from);

	if ((t1_usec > 999999) || (t1_sec > t2_sec) || (t1_sec == t2_sec && t1_usec > t2_usec))
		failf(s, "CTCP PING response from %s: invalid timestamp", m->from);

	res = (t2_usec + (1000000 * t2_sec)) - (t1_usec + (1000000 * t1_sec));
	res_sec = res / 1000000;
	res_usec = res % 1000000;

	server_msg(s, "CTCP PING response from %s: %llu.%llus", m->from, res_sec, res_usec);
	return 0;
}

static int
recv_ctcp_response_source(struct server *s, struct parsed_mesg *m)
{
	if (!str_trim(&m->trailing))
		failf(s, "CTCP SOURCE response from %s: empty message", m->from);

	server_msg(s, "CTCP SOURCE response from %s: %s", m->from, m->trailing);
	return 0;
}

static int
recv_ctcp_response_time(struct server *s, struct parsed_mesg *m)
{
	if (!str_trim(&m->trailing))
		failf(s, "CTCP TIME response from %s: empty message", m->from);

	server_msg(s, "CTCP TIME response from %s: %s", m->from, m->trailing);
	return 0;
}

static int
recv_ctcp_response_version(struct server *s, struct parsed_mesg *m)
{
	if (!str_trim(&m->trailing))
		failf(s, "CTCP VERSION response from %s: empty message", m->from);

	server_msg(s, "CTCP VERSION response from %s: %s", m->from, m->trailing);
	return 0;
}
