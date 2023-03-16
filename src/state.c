#include "src/state.h"

#include "config.h"
#include "src/components/channel.h"
#include "src/draw.h"
#include "src/handlers/irc_recv.h"
#include "src/handlers/irc_send.h"
#include "src/io.h"
#include "src/rirc.h"
#include "src/utils/utils.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* See: https://vt100.net/docs/vt100-ug/chapter3.html */
#define CTRL(k) ((k) & 0x1f)

#define COMMAND_HANDLERS \
	X(clear) \
	X(close) \
	X(connect) \
	X(disconnect) \
	X(quit)

#define X(CMD) \
static void command_##CMD(struct channel*, char*);
COMMAND_HANDLERS
#undef X

static void newlinev(struct channel*, enum buffer_line_type, const char*, const char*, va_list);

static int state_input_linef(struct channel*);
static int state_input_ctrlch(const char*, size_t);
static int state_input_action(const char*, size_t);

static void buffer_scrollback_bot(void);
static void buffer_scrollback_top(void);
static void buffer_scrollback_back(void);
static void buffer_scrollback_forw(void);

static uint16_t state_complete(char*, uint16_t, uint16_t, int);
static uint16_t state_complete_list(char*, uint16_t, uint16_t, const char**);
static uint16_t state_complete_user(char*, uint16_t, uint16_t, int);

static void state_channel_clear(int);
static void state_channel_close(int);

static void channel_move_prev(void);
static void channel_move_next(void);

static int action_clear(char);
static int action_close(char);
static int action_error(char);
static int (*action_handler)(char);
static char action_buff[256];

static void command(struct channel*, char*);

static struct
{
	struct channel *current_channel; /* the current channel being drawn */
	struct channel *default_channel; /* the default rirc channel at startup */
	struct server_list servers;
} state;

static unsigned state_tty_cols;
static unsigned state_tty_rows;

struct server_list*
state_server_list(void)
{
	return &state.servers;
}

struct channel*
current_channel(void)
{
	return state.current_channel;
}

/* List of IRC commands for tab completion */
static const char *irc_list[] = {
	"cap-ls",
	"cap-list",
	"ctcp-action",
	"ctcp-clientinfo",
	"ctcp-finger",
	"ctcp-ping",
	"ctcp-source",
	"ctcp-time",
	"ctcp-userinfo",
	"ctcp-version",
	"away",
	"topic-unset",
	"admin",   "connect", "info",     "invite", "join",
	"kick",    "kill",    "links",    "list",   "lusers",
	"mode",    "motd",    "names",    "nick",   "notice",
	"oper",    "part",    "pass",     "ping",   "pong",
	"privmsg", "quit",    "servlist", "squery", "stats",
	"time",    "topic",   "trace",    "user",   "version",
	"who",     "whois",   "whowas",   NULL };

/* List of rirc commands for tab completeion */
static const char *cmd_list[] = {
	"clear", "close", "connect", "disconnect", "quit", NULL};

void
state_init(void)
{
	state.default_channel = channel("rirc", CHANNEL_T_RIRC);

	newlinef(state.default_channel, 0, FROM_INFO, "      _");
	newlinef(state.default_channel, 0, FROM_INFO, " _ __(_)_ __ ___");
	newlinef(state.default_channel, 0, FROM_INFO, "| '__| | '__/ __|");
	newlinef(state.default_channel, 0, FROM_INFO, "| |  | | | | (__");
	newlinef(state.default_channel, 0, FROM_INFO, "|_|  |_|_|  \\___|");
	newlinef(state.default_channel, 0, FROM_INFO, "");
	newlinef(state.default_channel, 0, FROM_INFO, " - version %s", STR(VERSION));
	newlinef(state.default_channel, 0, FROM_INFO, " - compiled %s, %s", __DATE__, __TIME__);
#ifndef NDEBUG
	newlinef(state.default_channel, 0, FROM_INFO, " - compiled with DEBUG flags");
#endif

	channel_set_current(state.default_channel);
}

void
state_term(void)
{
	struct server *s1;
	struct server *s2;

	channel_free(state.default_channel);

	state.current_channel = NULL;
	state.default_channel = NULL;

	action_handler = NULL;
	action_buff[0] = 0;

	if ((s1 = state_server_list()->head) == NULL)
		return;

	do {
		s2 = s1;
		s1 = s2->next;
		connection_free(s2->connection);
		server_free(s2);
	} while (s1 != state_server_list()->head);

	state.servers.head = NULL;
	state.servers.tail = NULL;
}

unsigned
state_cols(void)
{
	return state_tty_cols;
}

unsigned
state_rows(void)
{
	return state_tty_rows;
}

void
newlinef(struct channel *c, enum buffer_line_type type, const char *from, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	newlinev(c, type, from, fmt, ap);
	va_end(ap);
}

static void
newlinev(struct channel *c, enum buffer_line_type type, const char *from, const char *fmt, va_list ap)
{
	char buf[TEXT_LENGTH_MAX];
	char prefix = 0;
	const char *from_str;
	const char *text_str;
	int len;
	size_t from_len;
	size_t text_len;

	if ((len = vsnprintf(buf, sizeof(buf), fmt, ap)) < 0) {
		text_str = "newlinef error: vsprintf failure";
		text_len = strlen(text_str);
		from_str = FROM_ERROR;
		from_len = strlen(from_str);
	} else {
		text_str = buf;
		text_len = len;
		from_str = from;

		const struct user *u = NULL;

		if (type == BUFFER_LINE_CHAT || type == BUFFER_LINE_PINGED) {
			u = user_list_get(&(c->users), c->server->casemapping, from, 0);
		}

		if (u) {
			prefix = u->prfxmodes.prefix;
			from_len = u->nick_len;
		} else {
			from_len = strlen(from);
		}
	}

	buffer_newline(
		&(c->buffer),
		type,
		from_str,
		text_str,
		from_len,
		text_len,
		prefix);

	if (c == current_channel()) {
		draw(DRAW_BUFFER);
		draw(DRAW_STATUS);
	} else {
		c->activity = MAX(c->activity, ACTIVITY_ACTIVE);
		draw(DRAW_NAV);
	}
}

static int
state_input_action(const char *input, size_t len)
{
	/* Waiting for user confirmation */

	/* ^c canceled the action, or the action was resolved */
	if (len == 1 && (*input == CTRL('c') || action_handler(*input))) {
		action_handler = NULL;
		return 1;
	}

	return 0;
}

static int
action_error(char c)
{
	UNUSED(c);

	return 1;
}

static int
action_clear(char c)
{
	if (toupper(c) == 'N')
		return 1;

	if (toupper(c) == 'Y') {
		state_channel_clear(0);
		return 1;
	}

	return 0;
}

static int
action_close(char c)
{
	if (toupper(c) == 'N')
		return 1;

	if (toupper(c) == 'Y') {
		state_channel_close(0);
		return 1;
	}

	return 0;
}

void
action(int (*a_handler)(char), const char *fmt, ...)
{
	/* Begin a user action
	 *
	 * The action handler is then passed any future input, and is
	 * expected to return a non-zero value when the action is resolved
	 * */

	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(action_buff, sizeof(action_buff), fmt, ap);
	va_end(ap);

	if (len < 0) {
		debug("vsnprintf failed");
	} else {
		action_handler = a_handler;
		draw(DRAW_INPUT);
	}
}

const char*
action_message(void)
{
	return (action_handler ? action_buff : NULL);
}

static void
state_channel_clear(int action_confirm)
{
	struct channel *c = current_channel();

	if (action_confirm) {
		action(action_clear, "Clear buffer '%s'?   [y/n]", c->name);
	} else {
		memset(&(c->buffer), 0, sizeof(c->buffer));
		draw(DRAW_BUFFER);
	}
}

static void
state_channel_close(int action_confirm)
{
	/* Close the current channel */

	int ret;
	struct channel *c = current_channel();
	struct server *s = c->server;

	if (c->type == CHANNEL_T_RIRC) {
		action(action_error, "Type :quit to exit rirc");
		return;
	}

	if (action_confirm) {

		if (c->type == CHANNEL_T_CHANNEL || c->type == CHANNEL_T_PRIVMSG)
			action(action_close, "Close '%s'?   [y/n]", c->name);

		if (c->type == CHANNEL_T_SERVER)
			action(action_close, "Close server '%s'? [%d channels]   [y/n])",
				c->name, (s->clist.count - 1));

		return;
	}

	if (c->type == CHANNEL_T_CHANNEL || c->type == CHANNEL_T_PRIVMSG) {

		if (s->connected && c->type == CHANNEL_T_CHANNEL && !c->parted) {
			if ((ret = io_sendf(s->connection, "PART %s :%s", c->name, DEFAULT_PART_MESG)))
				server_error(s, "sendf fail: %s", io_err(ret));
		}

		if (c == channel_get_last())
			channel_set_current(channel_get_prev(c));
		else
			channel_set_current(channel_get_next(c));

		channel_list_del(&(s->clist), c);
		channel_free(c);
		return;
	}

	if (c->type == CHANNEL_T_SERVER) {

		if (s->connected) {
			if ((ret = io_sendf(s->connection, "QUIT :%s", DEFAULT_QUIT_MESG)))
				server_error(s, "sendf fail: %s", io_err(ret));
			io_dx(s->connection);
		}

		channel_set_current((s->next != s ? s->next->channel : state.default_channel));
		connection_free(s->connection);
		server_list_del(state_server_list(), s);
		server_free(s);
		return;
	}
}

static void
buffer_scrollback_bot(void)
{
	state.current_channel->buffer.scrollback = state.current_channel->buffer.tail;
	draw(DRAW_BUFFER);
	draw(DRAW_STATUS);
}

static void
buffer_scrollback_top(void)
{
	state.current_channel->buffer.scrollback = state.current_channel->buffer.head - 1;
	draw(DRAW_BUFFER);
	draw(DRAW_STATUS);
}

static void
buffer_scrollback_back(void)
{
	draw(DRAW_BUFFER_BACK);
	draw(DRAW_BUFFER);
	draw(DRAW_STATUS);
}

static void
buffer_scrollback_forw(void)
{
	draw(DRAW_BUFFER_FORW);
	draw(DRAW_BUFFER);
	draw(DRAW_STATUS);
}

struct channel*
channel_get_first(void)
{
	struct server *s = state.servers.head;

	return s ? s->channel : NULL;
}

struct channel*
channel_get_last(void)
{
	struct server *s = state.servers.tail;

	return s ? s->channel->prev : NULL;
}

struct channel*
channel_get_next(struct channel *c)
{
	/* Return the next channel, accounting for server wrap around */

	if (c == state.default_channel)
		return c;

	return !(c->next == c->server->channel) ? c->next : c->server->next->channel;
}

struct channel*
channel_get_prev(struct channel *c)
{
	/* Return the previous channel, accounting for server wrap around */

	if (c == state.default_channel)
		return c;

	return !(c == c->server->channel) ? c->prev : c->server->prev->channel->prev;
}

void
channel_move_prev(void)
{
	/* Set the current channel to the previous channel */

	struct channel *c;

	if ((c = channel_get_prev(current_channel())) == current_channel())
		return;

	channel_set_current(c);
}

void
channel_move_next(void)
{
	/* Set the current channel to the next channel */

	struct channel *c;

	if ((c = channel_get_next(current_channel())) == current_channel())
		return;

	channel_set_current(c);
}

void
channel_set_current(struct channel *c)
{
	/* Set the state to an arbitrary channel */

	state.current_channel = c;

	draw(DRAW_ALL);
}

static uint16_t
state_complete_list(char *str, uint16_t len, uint16_t max, const char **list)
{
	size_t list_len = 0;

	if (len == 0)
		return 0;

	while (*list && strncmp(*list, str, len))
		list++;

	if (*list == NULL || (list_len = strlen(*list)) > max)
		return 0;

	memcpy(str, *list, list_len);

	return list_len + 1;
}

static uint16_t
state_complete_user(char *str, uint16_t len, uint16_t max, int first)
{
	struct user *u;
	struct channel *c = current_channel();

	if (c->server == NULL)
		return 0;

	if ((u = user_list_get(&(c->users), c->server->casemapping, str, len)) == NULL)
		return 0;

	if ((u->nick_len + (first != 0)) > max)
		return 0;

	memcpy(str, u->nick, u->nick_len);

	if (first)
		str[u->nick_len] = ':';

	return u->nick_len + (first != 0);
}

static uint16_t
state_complete(char *str, uint16_t len, uint16_t max, int first)
{
	if (first && str[0] == '/')
		return state_complete_list(str + 1, len - 1, max - 1, irc_list);

	if (first && str[0] == ':')
		return state_complete_list(str + 1, len - 1, max - 1, cmd_list);

	return state_complete_user(str, len, max, first);
}

static void
command(struct channel *c, char *args)
{
	const char *arg;

	if (!(arg = irc_strsep(&args)))
		return;

	#define X(CMD) \
	if (!strcasecmp(arg, #CMD)) { \
		command_##CMD(c, args);   \
		return;                   \
	}
	COMMAND_HANDLERS
	#undef X

	action(action_error, "Unknown command '%s'", arg);
}

static void
command_clear(struct channel *c, char *args)
{
	UNUSED(c);

	char *arg;

	if ((arg = irc_strsep(&args))) {
		action(action_error, "clear: Unknown arg '%s'", arg);
		return;
	}

	state_channel_clear(0);
}

static void
command_close(struct channel *c, char *args)
{
	UNUSED(c);

	char *arg;

	if ((arg = irc_strsep(&args))) {
		action(action_error, "close: Unknown arg '%s'", arg);
		return;
	}

	state_channel_close(0);
}

static void
command_connect(struct channel *c, char *args)
{
	/* :connect [hostname [options]] */

	char *arg;

	if (!(arg = irc_strsep(&args))) {

		int err;

		if (!c->server) {
			action(action_error, "connect: This is not a server");
		} else if ((err = io_cx(c->server->connection))) {
			action(action_error, "connect: %s", io_err(err));
		}

	} else {

		int ret;
		struct server *s;

		const char *host        = arg;
		const char *port        = NULL;
		const char *pass        = NULL;
		const char *username    = default_username;
		const char *realname    = default_realname;
		const char *mode        = NULL;
		const char *nicks       = default_nicks;
		const char *chans       = NULL;
		const char *tls_ca_file = NULL;
		const char *tls_ca_path = NULL;
		const char *tls_cert    = NULL;
		const char *sasl        = NULL;
		const char *sasl_user   = NULL;
		const char *sasl_pass   = NULL;
		int ipv                 = IO_IPV_UNSPEC;
		int tls                 = IO_TLS_ENABLED;
		int tls_vrfy            = IO_TLS_VRFY_REQUIRED;

		while ((arg = irc_strsep(&args))) {

			if (*arg != '-') {
				action(action_error, ":connect [hostname [options]]");
				return;
			} else if (!strcmp(arg, "-p") || !strcmp(arg, "--port")) {
				if (!(port = irc_strsep(&args))) {
					action(action_error, "connect: '-p/--port' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "-w") || !strcmp(arg, "--pass")) {
				if (!(pass = irc_strsep(&args))) {
					action(action_error, "connect: '-w/--pass' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "-u") || !strcmp(arg, "--username")) {
				if (!(username = irc_strsep(&args))) {
					action(action_error, "connect: '-u/--username' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "-r") || !strcmp(arg, "--realname")) {
				if (!(realname = irc_strsep(&args))) {
					action(action_error, "connect: '-r/--realname' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "-m") || !strcmp(arg, "--mode")) {
				if (!(mode = irc_strsep(&args))) {
					action(action_error, "connect: '-m/--mode' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "-n") || !strcmp(arg, "--nicks")) {
				if (!(nicks = irc_strsep(&args))) {
					action(action_error, "connect: '-n/--nicks' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "-c") || !strcmp(arg, "--chans")) {
				if (!(chans = irc_strsep(&args))) {
					action(action_error, "connect: '-c/--chans' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "--tls-cert")) {
				if (!(tls_cert = irc_strsep(&args))) {
					action(action_error, "connect: '--tls-cert' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "--tls-ca-file")) {
				if (!(tls_ca_file = irc_strsep(&args))) {
					action(action_error, "connect: '--tls-ca-file' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "--tls-ca-path")) {
				if (!(tls_ca_path = irc_strsep(&args))) {
					action(action_error, "connect: '--tls-ca-path' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "--tls-verify")) {
				if (!(arg = irc_strsep(&args))) {
					action(action_error, "connect: '--tls-verify' requires an argument");
					return;
				} else if (!strcmp(arg, "0") || !strcasecmp(arg, "disabled")) {
					tls_vrfy = IO_TLS_VRFY_DISABLED;
				} else if (!strcmp(arg, "1") || !strcasecmp(arg, "optional")) {
					tls_vrfy = IO_TLS_VRFY_OPTIONAL;
				} else if (!strcmp(arg, "2") || !strcasecmp(arg, "required")) {
					tls_vrfy = IO_TLS_VRFY_REQUIRED;
				} else {
					action(action_error, "connect: invalid option for '--tls-verify' '%s'", arg);
					return;
				}
			} else if (!strcmp(arg, "--tls-disable")) {
				tls = IO_TLS_DISABLED;
			} else if (!strcmp(arg, "--sasl")) {
				if (!(sasl = irc_strsep(&args))) {
					action(action_error, "connect: '--sasl' requires an argument");
					return;
				} else if (!strcasecmp(sasl, "EXTERNAL")) {
					;
				} else if (!strcasecmp(sasl, "PLAIN")) {
					;
				} else {
					action(action_error, "connect: invalid option for '--sasl' '%s'", sasl);
					return;
				}
			} else if (!strcmp(arg, "--sasl-user")) {
				if (!(sasl_user = irc_strsep(&args))) {
					action(action_error, "connect: '--sasl-user' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "--sasl-pass")) {
				if (!(sasl_pass = irc_strsep(&args))) {
					action(action_error, "connect: '--sasl-pass' requires an argument");
					return;
				}
			} else if (!strcmp(arg, "--ipv4")) {
				ipv = IO_IPV_4;
			} else if (!strcmp(arg, "--ipv6")) {
				ipv = IO_IPV_6;
			} else {
				action(action_error, "connect: unknown option '%s'", arg);
				return;
			}
		}

		if (port == NULL)
			port = (tls == IO_TLS_ENABLED) ? "6697" : "6667";

		s = server(host, port, pass, username, realname, mode);

		if (nicks && server_set_nicks(s, nicks)) {
			action(action_error, "connect: invalid -n/--nicks: '%s'", nicks);
			server_free(s);
			return;
		}

		if (chans && server_set_chans(s, chans)) {
			action(action_error, "connect: invalid -c/--chans: '%s'", chans);
			server_free(s);
			return;
		}

		if (server_list_add(state_server_list(), s)) {
			action(action_error, "connect: duplicate server: %s:%s", host, port);
			server_free(s);
			return;
		}

		if (sasl)
			server_set_sasl(s, sasl, sasl_user, sasl_pass);

		s->connection = connection(
			s,
			host,
			port,
			tls_ca_file,
			tls_ca_path,
			tls_cert,
			(ipv | tls | tls_vrfy));

		if ((ret = io_cx(s->connection)))
			server_error(s, "failed to connect: %s", io_err(ret));

		channel_set_current(s->channel);
	}
}

static void
command_disconnect(struct channel *c, char *args)
{
	char *arg;
	int err;

	if (!c->server) {
		action(action_error, "disconnect: This is not a server");
		return;
	}

	if ((arg = irc_strsep(&args))) {
		action(action_error, "disconnect: Unknown arg '%s'", arg);
		return;
	}

	if ((err = io_dx(c->server->connection)))
		action(action_error, "disconnect: %s", io_err(err));
}

static void
command_quit(struct channel *c, char *args)
{
	UNUSED(c);

	char *arg;

	if ((arg = irc_strsep(&args))) {
		action(action_error, "quit: Unknown arg '%s'", arg);
		return;
	}

	io_stop();
}

static int
state_input_ctrlch(const char *c, size_t len)
{
	/* Input a control character or escape sequence */

	/* ESC begins a key sequence */
	if (*c == 0x1b) {

		c++;

		if (len == 1)
			return 0;

		/* arrow up */
		else if (!strncmp(c, "[A", len - 1))
			return input_hist_back(&(current_channel()->input));

		/* arrow down */
		else if (!strncmp(c, "[B", len - 1))
			return input_hist_forw(&(current_channel()->input));

		/* arrow right */
		else if (!strncmp(c, "[C", len - 1))
			return input_cursor_forw(&(current_channel()->input));

		/* arrow left */
		else if (!strncmp(c, "[D", len - 1))
			return input_cursor_back(&(current_channel()->input));

		/* home */
		else if (!strncmp(c, "[H", len - 1))
			buffer_scrollback_bot();

		/* end */
		else if (!strncmp(c, "[F", len - 1))
			buffer_scrollback_top();

		/* delete */
		else if (!strncmp(c, "[3~", len - 1))
			return input_delete_forw(&(current_channel()->input));

		/* page up */
		else if (!strncmp(c, "[5~", len - 1))
			buffer_scrollback_back();

		/* page down */
		else if (!strncmp(c, "[6~", len - 1))
			buffer_scrollback_forw();

	} else switch (*c) {

		/* Backspace */
		case 0x7F:
			return input_delete_back(&(current_channel()->input));

		/* Horizontal tab */
		case 0x09:
			return input_complete(&(current_channel()->input), state_complete);

		/* Line feed */
		case 0x0A:
			return state_input_linef(current_channel());

		case CTRL('c'):
			/* Cancel current input */
			return input_reset(&(current_channel()->input));

		case CTRL('l'):
			/* Clear current channel */
			state_channel_clear(1);
			break;

		case CTRL('p'):
			/* Go to previous channel */
			channel_move_prev();
			break;

		case CTRL('n'):
			/* Go to next channel */
			channel_move_next();
			break;

		case CTRL('x'):
			state_channel_close(1);
			break;

		case CTRL('u'):
			/* Scoll buffer up */
			buffer_scrollback_back();
			break;

		case CTRL('d'):
			/* Scoll buffer down */
			buffer_scrollback_forw();
			break;
	}

	return 0;
}

static int
state_input_linef(struct channel *c)
{
	/* Handle line feed */

	char buf[INPUT_LEN_MAX + 1];
	size_t len;

	if ((len = input_write(&(c->input), buf, sizeof(buf), 0)) == 0)
		return 0;

	input_hist_push(&(c->input));

	switch (buf[0]) {
		case ':':
			if (len > 1 && buf[1] == ':')
				irc_send_message(current_channel()->server, current_channel(), buf + 1);
			else
				command(current_channel(), buf + 1);
			break;
		case '/':
			if (len > 1 && buf[1] == '/')
				irc_send_message(current_channel()->server, current_channel(), buf + 1);
			else
				irc_send_command(current_channel()->server, current_channel(), buf + 1);
			break;
		default:
			irc_send_message(current_channel()->server, current_channel(), buf);
	}

	return 1;
}

void
io_cb_read_inp(char *buf, size_t len)
{
	int redraw_input = 0;

	if (len == 0)
		fatal("zero length message");
	else if (action_handler)
		redraw_input = state_input_action(buf, len);
	else if (iscntrl(*buf))
		redraw_input = state_input_ctrlch(buf, len);
	else
		redraw_input = input_insert(&current_channel()->input, buf, len);

	if (redraw_input)
		draw(DRAW_INPUT);

	draw(DRAW_FLUSH);
}

void
io_cb_read_soc(char *buf, size_t len, const void *cb_obj)
{
	struct server *s = (struct server *)cb_obj;
	struct channel *c = s->channel;
	size_t ci = s->read.i;
	size_t n = len;

	for (size_t i = 0; i < n; i++) {

		char cc = buf[i];

		if (ci && cc == '\n' && ((i && buf[i - 1] == '\r') || (!i && s->read.cl == '\r'))) {

			s->read.buf[ci] = 0;

			debug_recv(ci, s->read.buf);

			struct irc_message m;

			if (irc_message_parse(&m, s->read.buf) != 0)
				newlinef(c, 0, FROM_ERROR, "failed to parse message");
			else
				irc_recv(s, &m);

			ci = 0;
		} else if (ci < IRC_MESSAGE_LEN && cc && cc != '\n' && cc != '\r') {
			s->read.buf[ci++] = cc;
		}
	}

	s->read.cl = buf[n - 1];
	s->read.i = ci;

	draw(DRAW_FLUSH);
}

void
io_cb_cxed(const void *cb_obj)
{
	struct server *s = (struct server *)cb_obj;

	int ret;

	server_reset(s);
	server_nicks_next(s);

	s->connected = 1;

	if ((ret = io_sendf(s->connection, "CAP LS " IRCV3_CAP_VERSION)))
		server_error(s, "sendf fail: %s", io_err(ret));

	if (s->pass && (ret = io_sendf(s->connection, "PASS %s", s->pass)))
		server_error(s, "sendf fail: %s", io_err(ret));

	if ((ret = io_sendf(s->connection, "NICK %s", s->nick)))
		server_error(s, "sendf fail: %s", io_err(ret));

	if ((ret = io_sendf(s->connection, "USER %s 0 * :%s", s->username, s->realname)))
		server_error(s, "sendf fail: %s", io_err(ret));

	draw(DRAW_STATUS);
	draw(DRAW_FLUSH);
}

void
io_cb_dxed(const void *cb_obj)
{
	struct server *s = (struct server *)cb_obj;
	struct channel *c = s->channel;

	server_reset(s);

	do {
		newlinef(c, 0, FROM_ERROR, " -- disconnected --");
		channel_reset(c);
		c = c->next;
	} while (c != s->channel);

	draw(DRAW_STATUS);
	draw(DRAW_FLUSH);
}

void
io_cb_ping(const void *cb_obj, unsigned ping)
{
	int ret;
	struct server *s = (struct server *)cb_obj;

	s->ping = ping;

	if (ping != IO_PING_MIN)
		draw(DRAW_STATUS);
	else if ((ret = io_sendf(s->connection, "PING :%s", s->host)))
		server_error(s, "sendf fail: %s", io_err(ret));

	draw(DRAW_FLUSH);
}

void
io_cb_sigwinch(unsigned cols, unsigned rows)
{
	state_tty_cols = cols;
	state_tty_rows = rows;

	draw(DRAW_ALL);
	draw(DRAW_FLUSH);
}

void
io_cb_info(const void *cb_obj, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	newlinev(((struct server *)cb_obj)->channel, 0, FROM_INFO, fmt, ap);

	va_end(ap);

	draw(DRAW_FLUSH);
}

void
io_cb_error(const void *cb_obj, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	newlinev(((struct server *)cb_obj)->channel, 0, FROM_ERROR, fmt, ap);

	va_end(ap);

	draw(DRAW_FLUSH);
}
