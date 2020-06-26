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
	do { server_error((S), __VA_ARGS__); \
	     return 1; \
	} while (0)

#define sendf(S, ...) \
	do { int ret; \
	     if ((ret = io_sendf((S)->connection, __VA_ARGS__))) \
	         failf((S), "Send fail: %s", io_err(ret)); \
	} while (0)

static int
parse_ctcp(struct server *s, const char *from, char **args, const char **cmd)
{
	char *message = *args;
	char *command;
	char *p;

	if (!from)
		failf(s, "Received CTCP from unknown sender");

	if (*message != '\001')
		failf(s, "Received malformed CTCP from %s", from);

	if ((p = strchr(message + 1, '\001')))
		*p = 0;

	*message++ = 0;

	if (!(command = strsep(&message)))
		failf(s, "Received empty CTCP from %s", from);

	for (p = command; *p; p++)
		*p = toupper(*p);

	*cmd = command;
	*args = strtrim(&message);

	return 0;
}

int
ctcp_request(struct server *s, const char *from, const char *targ, char *message)
{
	const char *command;
	const struct ctcp_handler *ctcp;
	int ret;

	if ((ret = parse_ctcp(s, from, &message, &command)) != 0)
		return ret;

	if (!(ctcp = ctcp_handler_lookup(command, strlen(command))))
		failf(s, "Received unsupported CTCP request '%s' from %s", command, from);

	return ctcp->f_request(s, from, targ, message);
}

int
ctcp_response(struct server *s, const char *from, const char *targ, char *message)
{
	const char *command;
	const struct ctcp_handler *ctcp;
	int ret;

	if ((ret = parse_ctcp(s, from, &message, &command)) != 0)
		return ret;

	if (!(ctcp = ctcp_handler_lookup(command, strlen(command))) || !ctcp->f_response)
		failf(s, "Received unsupported CTCP response '%s' from %s", command, from);

	return ctcp->f_response(s, from, targ, message);
}

static int
ctcp_request_action(struct server *s, const char *from, const char *targ, char *m)
{
	struct channel *c;

	if (!targ)
		failf(s, "CTCP ACTION: target is NULL");

	if (!irc_strcmp(s->casemapping, targ, s->nick)) {
		if ((c = channel_list_get(&s->clist, from, s->casemapping)) == NULL) {
			c = channel(from, CHANNEL_T_PRIVATE);
			c->activity = ACTIVITY_PINGED;
			c->server = s;
			channel_list_add(&s->clist, c);
		}
	} else if ((c = channel_list_get(&s->clist, targ, s->casemapping)) == NULL) {
		failf(s, "CTCP ACTION: target '%s' not found", targ);
	}

	if (strtrim(&m))
		newlinef(c, 0, "*", "%s %s", from, m);
	else
		newlinef(c, 0, "*", "%s", from);

	return 0;
}

static int
ctcp_request_clientinfo(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (strtrim(&m))
		server_info(s, "CTCP CLIENTINFO from %s (%s)", from, m);
	else
		server_info(s, "CTCP CLIENTINFO from %s", from);

	sendf(s, "NOTICE %s :\001CLIENTINFO ACTION CLIENTINFO PING SOURCE TIME VERSION\001", from);

	return 0;
}

static int
ctcp_request_finger(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (strtrim(&m))
		server_info(s, "CTCP FINGER from %s (%s)", from, m);
	else
		server_info(s, "CTCP FINGER from %s", from);

	sendf(s, "NOTICE %s :\001FINGER rirc v"VERSION" ("__DATE__")\001", from);

	return 0;
}

static int
ctcp_request_ping(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (strtrim(&m)) {
		server_info(s, "CTCP PING from %s", from);
		sendf(s, "NOTICE %s :\001PING %s\001", from, m);
	}

	return 0;
}

static int
ctcp_request_source(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (strtrim(&m))
		server_info(s, "CTCP SOURCE from %s (%s)", from, m);
	else
		server_info(s, "CTCP SOURCE from %s", from);

	sendf(s, "NOTICE %s :\001SOURCE rcr.io/rirc\001", from);

	return 0;
}

static int
ctcp_request_time(struct server *s, const char *from, const char *targ, char *m)
{
	/* ISO 8601 */
	char buf[sizeof("1970-01-01T00:00:00")];
	struct tm tm;
	time_t t = 0;

	UNUSED(targ);

	if (strtrim(&m))
		server_info(s, "CTCP TIME from %s (%s)", from, m);
	else
		server_info(s, "CTCP TIME from %s", from);

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

	sendf(s, "NOTICE %s :\001TIME %s\001", from, buf);

	return 0;
}

static int
ctcp_request_userinfo(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (strtrim(&m))
		server_info(s, "CTCP USERINFO from %s (%s)", from, m);
	else
		server_info(s, "CTCP USERINFO from %s", from);

	sendf(s, "NOTICE %s :\001USERINFO %s (%s)\001", from, s->nick, s->realname);

	return 0;
}

static int
ctcp_request_version(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (strtrim(&m))
		server_info(s, "CTCP VERSION from %s (%s)", from, m);
	else
		server_info(s, "CTCP VERSION from %s", from);

	sendf(s, "NOTICE %s :\001VERSION rirc v"VERSION" ("__DATE__")\001", from);

	return 0;
}

static int
ctcp_response_clientinfo(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (!strtrim(&m))
		failf(s, "CTCP CLIENTINFO response from %s: empty message", from);

	server_info(s, "CTCP CLIENTINFO response from %s: %s", from, m);

	return 0;
}

static int
ctcp_response_finger(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (!strtrim(&m))
		failf(s, "CTCP FINGER response from %s: empty message", from);

	server_info(s, "CTCP FINGER response from %s: %s", from, m);

	return 0;
}

static int
ctcp_response_ping(struct server *s, const char *from, const char *targ, char *m)
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

	UNUSED(targ);

	if (!(sec = strsep(&m)))
		failf(s, "CTCP PING response from %s: sec is NULL", from);

	if (!(usec = strsep(&m)))
		failf(s, "CTCP PING response from %s: usec is NULL", from);

	for (const char *p = sec; *p; p++) {
		if (!(isdigit(*p)))
			failf(s, "CTCP PING response from %s: sec is invalid", from);
	}

	for (const char *p = usec; *p; p++) {
		if (!(isdigit(*p)))
			failf(s, "CTCP PING response from %s: usec is invalid", from);
	}

#ifdef TESTING
	t2_sec = 123;
	t2_usec = 456789;
#else
	struct timeval t;

	(void) gettimeofday(&t, NULL);

	t2_sec = (long unsigned) t.tv_sec;
	t2_usec = (long unsigned) t.tv_usec;
#endif

	errno = 0;

	t1_sec = strtoul(sec, NULL, 10);
	t1_usec = strtoul(usec, NULL, 10);

	if (errno)
		failf(s, "CTCP PING response from %s: failed to parse timestamp", from);

	if ((t1_usec > 999999) || (t1_sec > t2_sec) || (t1_sec == t2_sec && t1_usec > t2_usec))
		failf(s, "CTCP PING response from %s: invalid timestamp", from);

	res = (t2_usec + (1000000 * t2_sec)) - (t1_usec + (1000000 * t1_sec));
	res_sec = res / 1000000;
	res_usec = res % 1000000;

	server_info(s, "CTCP PING response from %s: %llu.%llus", from, res_sec, res_usec);

	return 0;
}

static int
ctcp_response_source(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (!strtrim(&m))
		failf(s, "CTCP SOURCE response from %s: empty message", from);

	server_info(s, "CTCP SOURCE response from %s: %s", from, m);

	return 0;
}

static int
ctcp_response_time(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (!strtrim(&m))
		failf(s, "CTCP TIME response from %s: empty message", from);

	server_info(s, "CTCP TIME response from %s: %s", from, m);

	return 0;
}

static int
ctcp_response_userinfo(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (!strtrim(&m))
		failf(s, "CTCP USERINFO response from %s: empty message", from);

	server_info(s, "CTCP USERINFO response from %s: %s", from, m);

	return 0;
}

static int
ctcp_response_version(struct server *s, const char *from, const char *targ, char *m)
{
	UNUSED(targ);

	if (!strtrim(&m))
		failf(s, "CTCP VERSION response from %s: empty message", from);

	server_info(s, "CTCP VERSION response from %s: %s", from, m);

	return 0;
}

#undef failf
#undef sendf
