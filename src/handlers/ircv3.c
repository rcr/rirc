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

// TODO: some of these are server-only (e.g. END)
#define IRCV3_HANDLERS \
	X(ack) \
	X(del) \
	X(end) \
	X(list) \
	X(ls) \
	X(nak) \
	X(new) \
	X(req)

#define X(cmd) static int ircv3_##cmd(struct server*, char*);
IRCV3_HANDLERS
#undef X

// Freenode:
#if 0
 13:44                 -- ~ arg: 'account-notify'
 13:44                 -- ~ arg: 'away-notify'
 13:44                 -- ~ arg: 'cap-notify'
 13:44                 -- ~ arg: 'chghost'
 13:44                 -- ~ arg: 'extended-join'
 13:44                 -- ~ arg: 'identify-msg'
 13:44                 -- ~ arg: 'multi-prefix'
 13:44                 -- ~ arg: 'sasl'
 13:44                 -- ~ arg: 'tls'
#endif

int
irc_recv_ircv3(struct server *s, struct irc_message *m)
{
	const char *targ;
	const char *cmnd;

	if (!(targ = strsep(&m->params)))
		failf(s, "CAP: target is null");

	if (!(cmnd = strsep(&m->params)))
		failf(s, "CAP: command is null");

	if (*m->params == ':')
		m->params++;

	#define X(cmd) if (!strcmp(cmnd, #cmd)) return ircv3_##cmd(s, m->params);
	IRCV3_HANDLERS
	#undef X

	// FIXME: error for non-recognized message
	return 0;
}

static int
ircv3_ack(struct server *s, char *m)
{
	const char *arg;

	while ((arg = strsep(&(m)))) {

		debug("Setting ircv3 cap: %s", arg);

		if (!strcmp(arg, "multi-prefix")) {
			s->ircv3_multiprefix = 1;
		}
	}

	return 0;
}

static int
ircv3_ls(struct server *s, char *m)
{
	const char *arg;

	while ((arg = strsep(&(m)))) {

		debug("Requesting ircv3 cap: %s", arg);

		if (!strcmp(arg, "multi-prefix")) {
			sendf(s, "CAP REQ :multi-prefix");
		}
	}

	return 0;
}




static int
ircv3_list(struct server *s, char *m)
{
	(void)s;
	(void)m;
	return 0;
}
static int
ircv3_req(struct server *s, char *m)
{
	(void)s;
	(void)m;
	return 0;
}
static int
ircv3_nak(struct server *s, char *m)
{
	(void)s;
	(void)m;
	return 0;
}
static int
ircv3_end(struct server *s, char *m)
{
	(void)s;
	(void)m;
	return 0;
}
static int
ircv3_new(struct server *s, char *m)
{
	(void)s;
	(void)m;
	return 0;
}
static int
ircv3_del(struct server *s, char *m)
{
	(void)s;
	(void)m;
	return 0;
}
