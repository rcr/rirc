#include "src/state.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <stdio.h>

#include "config.h"
#include "src/components/channel.h"
#include "src/draw.h"
#include "src/handlers/irc_recv.h"
#include "src/handlers/irc_send.h"
#include "src/io.h"
#include "src/rirc.h"
#include "src/utils/utils.h"

/* See: https://vt100.net/docs/vt100-ug/chapter3.html */
#define CTRL(k) ((k) & 0x1f)

static void _newline(struct channel*, enum buffer_line_t, const char*, const char*, va_list);

static int state_input_linef(struct channel*);
static int state_input_ctrlch(const char*, size_t);
static int state_input_action(const char*, size_t);

static uint16_t state_complete(char*, uint16_t, uint16_t, int);
static uint16_t state_complete_list(char*, uint16_t, uint16_t, const char**);
static uint16_t state_complete_user(char*, uint16_t, uint16_t, int);

static void state_channel_clear(int);
static void state_channel_close(int);

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
	state.default_channel = state.current_channel = channel("rirc", CHANNEL_T_RIRC);

	newline(state.default_channel, 0, "--", "      _");
	newline(state.default_channel, 0, "--", " _ __(_)_ __ ___");
	newline(state.default_channel, 0, "--", "| '__| | '__/ __|");
	newline(state.default_channel, 0, "--", "| |  | | | | (__");
	newline(state.default_channel, 0, "--", "|_|  |_|_|  \\___|");
	newline(state.default_channel, 0, "--", "");
	newline(state.default_channel, 0, "--", " - version " VERSION);
	newline(state.default_channel, 0, "--", " - compiled " __DATE__ ", " __TIME__);
#ifndef NDEBUG
	newline(state.default_channel, 0, "--", " - compiled with DEBUG flags");
#endif
}

void
state_term(void)
{
	/* Exit handler; must return normally */

	struct server *s1, *s2;

	channel_free(state.default_channel);

	if ((s1 = state_server_list()->head) == NULL)
		return;

	do {
		s2 = s1;
		s1 = s2->next;
		connection_free(s2->connection);
		server_free(s2);
	} while (s1 != state_server_list()->head);
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
newline(struct channel *c, enum buffer_line_t type, const char *from, const char *mesg)
{
	/* Default wrapper for _newline */

	newlinef(c, type, from, "%s", mesg);
}

void
newlinef(struct channel *c, enum buffer_line_t type, const char *from, const char *fmt, ...)
{
	/* Formating wrapper for _newline */

	va_list ap;

	va_start(ap, fmt);
	_newline(c, type, from, fmt, ap);
	va_end(ap);
}

static void
_newline(struct channel *c, enum buffer_line_t type, const char *from, const char *fmt, va_list ap)
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
		from_str = "-!!-";
		from_len = strlen(from_str);
	} else {
		text_str = buf;
		text_len = len;
		from_str = from;

		const struct user *u = NULL;

		if (type == BUFFER_LINE_CHAT) {
			u = user_list_get(&(c->users), c->server->casemapping, from, 0);
		}

		if (u) {
			prefix = u->prfxmodes.prefix;
			from_len = u->nick_len;
		} else {
			from_len = strlen(from);
		}
	}

	// TODO: preformat the time string here

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
	} else {
		c->activity = MAX(c->activity, ACTIVITY_ACTIVE);
		draw(DRAW_NAV);
	}
}

int
state_server_set_chans(struct server *s, const char *chans)
{
	char *p1, *p2, *base;
	size_t n = 0;

	p2 = base = strdup(chans);

	do {
		n++;

		p1 = p2;
		p2 = strchr(p2, ',');

		if (p2)
			*p2++ = 0;

		if (!irc_ischan(p1)) {
			free(base);
			return -1;
		}
	} while (p2);

	for (const char *chan = base; n; n--) {
		struct channel *c;
		c = channel(chan, CHANNEL_T_CHANNEL);
		c->server = s;
		channel_list_add(&s->clist, c);
		chan = strchr(chan, 0) + 1;
	}

	free(base);

	return 0;
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

	struct channel *c = current_channel();
	struct server *s = c->server;

	if (c->type == CHANNEL_T_RIRC) {
		action(action_error, "Type :quit to exit rirc");
		return;
	}

	if (action_confirm) {

		if (c->type == CHANNEL_T_CHANNEL || c->type == CHANNEL_T_PRIVATE)
			action(action_close, "Close '%s'?   [y/n]", c->name);

		if (c->type == CHANNEL_T_SERVER)
			action(action_close, "Close server '%s'? [%d channels]   [y/n])",
				c->name, (s->clist.count - 1));

		return;
	}

	if (c->type == CHANNEL_T_CHANNEL || c->type == CHANNEL_T_PRIVATE) {

		if (s->connected && c->type == CHANNEL_T_CHANNEL && !c->parted)
			io_sendf(s->connection, "PART %s :%s", c->name, DEFAULT_PART_MESG);

		channel_set_current(c->next);
		channel_list_del(&(s->clist), c);
		channel_free(c);
		return;
	}

	if (c->type == CHANNEL_T_SERVER) {

		if (s->connected) {
			io_sendf(s->connection, "QUIT :%s", DEFAULT_QUIT_MESG);
			io_dx(s->connection);
		}

		channel_set_current((s->next != s ? s->next->channel : state.default_channel));
		connection_free(s->connection);
		server_list_del(state_server_list(), s);
		server_free(s);
		return;
	}
}

//TODO:
//improvement: don't set the scrollback if the buffer tail is in view
void
buffer_scrollback_back(struct channel *c)
{
	/* Scroll a buffer back one page */

	struct buffer *b = &c->buffer;

	unsigned int buffer_i = b->scrollback,
	             count = 0,
	             text_w = 0,
	             cols = state_tty_cols,
	             rows = state_tty_rows - 4;

	struct buffer_line *line = buffer_line(b, buffer_i);

	/* Skip redraw */
	if (line == buffer_tail(b))
		return;

	/* Find top line */
	for (;;) {

		buffer_line_split(line, NULL, &text_w, cols, b->pad);

		count += buffer_line_rows(line, text_w);

		if (count >= rows)
			break;

		if (line == buffer_tail(b))
			return;

		line = buffer_line(b, --buffer_i);
	}

	b->scrollback = buffer_i;

	/* Top line in view draws in full; scroll back one additional line */
	if (count == rows && line != buffer_tail(b))
		b->scrollback--;

	draw(DRAW_BUFFER);
	draw(DRAW_STATUS);
}

void
buffer_scrollback_forw(struct channel *c)
{
	/* Scroll a buffer forward one page */

	unsigned int count = 0,
	             text_w = 0,
	             cols = state_tty_cols,
	             rows = state_tty_rows - 4;

	struct buffer *b = &c->buffer;

	struct buffer_line *line = buffer_line(b, b->scrollback);

	/* Skip redraw */
	if (line == buffer_head(b))
		return;

	/* Find top line */
	for (;;) {

		buffer_line_split(line, NULL, &text_w, cols, b->pad);

		count += buffer_line_rows(line, text_w);

		if (line == buffer_head(b))
			break;

		if (count >= rows)
			break;

		line = buffer_line(b, ++b->scrollback);
	}

	/* Bottom line in view draws in full; scroll forward one additional line */
	if (count == rows && line != buffer_head(b))
		b->scrollback++;

	draw(DRAW_BUFFER);
	draw(DRAW_STATUS);
}

/* FIXME:
 *  - These abstractions should take into account the new component hierarchy
 *    and have the backwards pointer from channel to server removed, in favour
 *    of passing a current_server() to handlers
 *  - The server's channel should not be part of the server's channel_list
 */
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
	if (c == state.default_channel)
		return c;
	else {
		/* Return the next channel, accounting for server wrap around */
		return !(c->next == c->server->channel) ? c->next : c->server->next->channel;
	}
}

struct channel*
channel_get_prev(struct channel *c)
{
	if (c == state.default_channel)
		return c;
	else
		/* Return the previous channel, accounting for server wrap around */
		return !(c == c->server->channel) ? c->prev : c->server->prev->channel->prev;
}

void
channel_set_current(struct channel *c)
{
	/* Set the state to an arbitrary channel */
	state.current_channel = c;
	draw(DRAW_ALL);
}

void
channel_move_prev(void)
{
	/* Set the current channel to the previous channel */

	struct channel *c = channel_get_prev(state.current_channel);

	if (c != state.current_channel) {
		state.current_channel = c;
		draw(DRAW_ALL);
	}
}

void
channel_move_next(void)
{
	/* Set the current channel to the next channel */

	struct channel *c = channel_get_next(state.current_channel);

	if (c != state.current_channel) {
		state.current_channel = c;
		draw(DRAW_ALL);
	}
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
command(struct channel *c, char *buf)
{
	const char *cmnd;
	int err;

	if (!(cmnd = strsep(&buf)))
		return;

	if (!strcasecmp(cmnd, "clear")) {
		state_channel_clear(0);
		return;
	}

	if (!strcasecmp(cmnd, "close")) {
		state_channel_close(0);
		return;
	}

	if (!strcasecmp(cmnd, "connect")) {
		if ((err = io_cx(c->server->connection)))
			action(action_error, "Error: %s", io_err(err));
		return;
	}

	if (!strcasecmp(cmnd, "disconnect")) {
		if ((err = io_dx(c->server->connection)))
			action(action_error, "Error: %s", io_err(err));
		return;
	}

	if (!strcasecmp(cmnd, "quit")) {
		io_stop();
		return;
	}

	action(action_error, "Unknown command '%s'", cmnd);
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

		/* delete */
		else if (!strncmp(c, "[3~", len - 1))
			return input_delete_forw(&(current_channel()->input));

		/* page up */
		else if (!strncmp(c, "[5~", len - 1))
			buffer_scrollback_back(current_channel());

		/* page down */
		else if (!strncmp(c, "[6~", len - 1))
			buffer_scrollback_forw(current_channel());

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
			buffer_scrollback_back(current_channel());
			break;

		case CTRL('d'):
			/* Scoll buffer down */
			buffer_scrollback_forw(current_channel());
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
				irc_send_privmsg(current_channel()->server, current_channel(), buf + 1);
			else
				command(current_channel(), buf + 1);
			break;
		case '/':
			if (len > 1 && buf[1] == '/')
				irc_send_privmsg(current_channel()->server, current_channel(), buf + 1);
			else
				irc_send_command(current_channel()->server, current_channel(), buf + 1);
			break;
		default:
			irc_send_privmsg(current_channel()->server, current_channel(), buf);
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
				newlinef(c, 0, "-!!-", "failed to parse message");
			else
				irc_recv(s, &m);

			ci = 0;
		} else if (ci < IRC_MESSAGE_LEN && (isprint(cc) || cc == 0x01)) {
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
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

	if (s->pass && (ret = io_sendf(s->connection, "PASS %s", s->pass)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

	if ((ret = io_sendf(s->connection, "NICK %s", s->nick)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

	if ((ret = io_sendf(s->connection, "USER %s 8 * :%s", s->username, s->realname)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

	draw(DRAW_STATUS);
	draw(DRAW_FLUSH);
}

void
io_cb_dxed(const void *cb_obj)
{
	struct server *s = (struct server *)cb_obj;
	struct channel *c = s->channel;

	s->connected = 0;

	do {
		newline(c, 0, "-!!-", " -- disconnected --");
		channel_reset(c);
		c = c->next;
	} while (c != s->channel);

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
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

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

	_newline(((struct server *)cb_obj)->channel, 0, "--", fmt, ap);

	va_end(ap);

	draw(DRAW_FLUSH);
}

void
io_cb_error(const void *cb_obj, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	_newline(((struct server *)cb_obj)->channel, 0, "-!!-", fmt, ap);

	va_end(ap);

	draw(DRAW_FLUSH);
}
