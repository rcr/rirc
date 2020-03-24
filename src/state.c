/**
 * state.c
 *
 * All manipulation of global program state
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <stdio.h>

#include "src/draw.h"
#include "src/io.h"
#include "src/rirc.h"
#include "src/state.h"
#include "src/utils/utils.h"
#include "src/handlers/irc_recv.h"
#include "src/handlers/irc_send.h"

/* See: https://vt100.net/docs/vt100-ug/chapter3.html */
#define CTRL(k) ((k) & 0x1f)

static void _newline(struct channel*, enum buffer_line_t, const char*, const char*, va_list);
static void state_io_cxed(struct server*);
static void state_io_dxed(struct server*, va_list);
static void state_io_ping(struct server*, unsigned int);
static void state_io_signal(enum io_sig_t);

static int state_input_linef(struct channel*);
static int state_input_ctrlch(const char*, size_t);
static int state_input_action(const char*, size_t);

static uint16_t state_complete(char*, uint16_t, uint16_t, int);
static uint16_t state_complete_list(char*, uint16_t, uint16_t, const char**);
static uint16_t state_complete_user(char*, uint16_t, uint16_t, int);

static void command(struct channel*, char*);

static struct
{
	struct channel *current_channel; /* the current channel being drawn */
	struct channel *default_channel; /* the default rirc channel at startup */
	struct server_list servers;
	union draw draw;
} state;

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
	"ctcp-action",
	"ctcp-clientinfo",
	"ctcp-finger",
	"ctcp-ping",
	"ctcp-source",
	"ctcp-time",
	"ctcp-userinfo",
	"ctcp-version",
	"admin",   "connect", "info",     "invite", "join",
	"kick",    "kill",    "links",    "list",   "lusers",
	"mode",    "motd",    "names",    "nick",   "notice",
	"oper",    "part",    "pass",     "ping",   "pong",
	"privmsg", "quit",    "servlist", "squery", "stats",
	"time",    "topic",   "trace",    "user",   "version",
	"who",     "whois",   "whowas",   NULL };

// TODO: from command handler list
/* List of rirc commands for tab completeion */
static const char *cmd_list[] = {
	"clear", "close", "connect", "disconnect", "quit", "set", NULL};

/* Set draw bits */
#define X(BIT) void draw_##BIT(void) { state.draw.bits.BIT = 1; }
DRAW_BITS
#undef X

void
draw_all(void)
{
	state.draw.all_bits = -1;
}

void
redraw(void)
{
	draw(state.draw);

	state.draw.all_bits = 0;
}

void
state_init(void)
{
	state.default_channel = state.current_channel = channel("rirc", CHANNEL_T_OTHER);

	newline(state.default_channel, 0, "--", "      _");
	newline(state.default_channel, 0, "--", " _ __(_)_ __ ___");
	newline(state.default_channel, 0, "--", "| '__| | '__/ __|");
	newline(state.default_channel, 0, "--", "| |  | | | | (__");
	newline(state.default_channel, 0, "--", "|_|  |_|_|  \\___|");
	newline(state.default_channel, 0, "--", "");
	newline(state.default_channel, 0, "--", " - version " VERSION);
	newline(state.default_channel, 0, "--", " - compiled " __DATE__ ", " __TIME__);
#ifdef DEBUG
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
		io_free(s2->connection);
		server_free(s2);
	} while (s1 != state_server_list()->head);
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
		draw_buffer();
	} else {
		c->activity = MAX(c->activity, ACTIVITY_ACTIVE);
		draw_nav();
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

void
channel_clear(struct channel *c)
{
	memset(&(c->buffer), 0, sizeof(c->buffer));
	draw_buffer();
}

/* WIP:
 *
 * removed action subsystem from input.c
 *
 * eventually should go in action.{h,c}
 *
 */
/* Max length of user action message */
#define MAX_ACTION_MESG 256
char *action_message;
static int action_close_server(char);
/* Action handling */
static int (*action_handler)(char);
static char action_buff[MAX_ACTION_MESG];
/* Incremental channel search */
static int action_find_channel(char);
/* TODO: This is a first draft for simple channel searching functionality.
 *
 * It can be cleaned up, and input.c is probably not the most ideal place for this */
#define MAX_SEARCH 128
struct channel *search_cptr; /* Used for iterative searching, before setting the current channel */
static char search_buf[MAX_SEARCH];
static size_t search_i;

static struct channel* search_channels(struct channel*, char*);
static struct channel*
search_channels(struct channel *start, char *search)
{
	if (start == NULL || *search == '\0')
		return NULL;

	/* Start the search one past the input */
	struct channel *c = channel_get_next(start);

	while (c != start) {

		if (strstr(c->name, search))
			return c;

		c = channel_get_next(c);
	}

	return NULL;
}
static int
state_input_action(const char *input, size_t len)
{
	/* Waiting for user confirmation */

	if (len == 1 && (*input == CTRL('c') || action_handler(*input))) {
		/* ^c canceled the action, or the action was resolved */

		action_message = NULL;
		action_handler = NULL;

		return 1;
	}

	return 0;
}
static int
action_close_server(char c)
{
	/* Confirm closing a server */

	if (c == 'n' || c == 'N')
		return 1;

	if (c == 'y' || c == 'Y') {

		int ret;
		struct channel *c = current_channel();
		struct server *s = c->server;

		/* If closing the last server */
		if ((state.current_channel = c->server->next->channel) == c->server->channel)
			state.current_channel = state.default_channel;

		if ((ret = io_sendf(s->connection, "QUIT :%s", DEFAULT_QUIT_MESG)))
			newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

		io_dx(s->connection);
		io_free(s->connection);
		server_list_del(state_server_list(), s);
		server_free(s);

		draw_all();

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
	len = vsnprintf(action_buff, MAX_ACTION_MESG, fmt, ap);
	va_end(ap);

	if (len < 0) {
		debug("vsnprintf failed");
	} else {
		action_handler = a_handler;
		action_message = action_buff;
		draw_input();
	}
}
/* Action line should be:
 *
 *
 * Find: [current result]/[(server if not current server[socket if not 6667])] : <search input> */
static int
action_find_channel(char c)
{
	/* Incremental channel search */

	/* \n, Esc, ^C cancels a search if no results are found */
	if (c == '\n' || c == 0x1b || c == CTRL('c')) {

		/* Confirm non-empty match */
		if (c == '\n' && search_cptr)
			channel_set_current(search_cptr);

		search_buf[0] = 0;
		search_i = 0;
		search_cptr = NULL;
		return 1;
	}

	/* ^F repeats the search forward from the current result,
	 * or resets search criteria if no match */
	if (c == CTRL('f')) {
		if (search_cptr == NULL) {
			search_buf[0] = 0;
			search_i = 0;
			action(action_find_channel, "Find: ");
			return 0;
		}

		search_cptr = search_channels(search_cptr, search_buf);
	} else if (c == 0x7f && search_i) {
		/* Backspace */
		search_buf[--search_i] = 0;
		search_cptr = search_channels(current_channel(), search_buf);

	} else if (isprint(c) && search_i < (sizeof(search_buf) - 1)) {
		/* All other input */
		search_buf[search_i++] = c;
		search_buf[search_i] = 0;
		search_cptr = search_channels(current_channel(), search_buf);
	}

	/* Reprint the action message */
	if (search_cptr == NULL) {
		if (*search_buf)
			action(action_find_channel, "Find: NO MATCH -- %s", search_buf);
		else
			action(action_find_channel, "Find: ");
	} else {
		/* Found a channel */
		if (search_cptr->server == current_channel()->server) {
			action(action_find_channel, "Find: %s -- %s",
					search_cptr->name, search_buf);
		} else {
			if (!strcmp(search_cptr->server->port, "6667"))
				action(action_find_channel, "Find: %s/%s -- %s",
						search_cptr->server->host, search_cptr->name, search_buf);
			else
				action(action_find_channel, "Find: %s:%s/%s -- %s",
						search_cptr->server->host, search_cptr->server->port,
						search_cptr->name, search_buf);
		}
	}

	return 0;
}

void
channel_close(struct channel *c)
{
	/* Close a channel. If the current channel is being
	 * closed, update state appropriately */

	if (c == state.default_channel) {
		newline(c, 0, "--", "Type :quit to exit rirc");
		return;
	}

	if (c->type == CHANNEL_T_SERVER) {
		/* Closing a server, confirm the number of channels being closed */

		int num_chans = 0;

		while ((c = c->next)->type != CHANNEL_T_SERVER)
			num_chans++;

		if (num_chans)
			action(action_close_server, "Close server '%s'? Channels: %d   [y/n]",
					c->server->host, num_chans);
		else
			action(action_close_server, "Close server '%s'?   [y/n]", c->server->host);
	} else {
		/* Closing a channel */
		if (c->type == CHANNEL_T_CHANNEL && !c->parted) {
			int ret;
			if (0 != (ret = io_sendf(c->server->connection, "PART %s", c->name))) {
				// FIXME: closing a parted channel when server is disconnected isnt an error
				newlinef(c->server->channel, 0, "sendf fail", "%s", io_err(ret));
			}
		}

		/* If closing the current channel, update state to a new channel */
		if (c == current_channel()) {
			channel_set_current(c->next);
		} else {
			draw_nav();
		}

		channel_list_del(&c->server->clist, c);
		channel_free(c);
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
	             cols = io_tty_cols(),
	             rows = io_tty_rows() - 4;

	struct buffer_line *line = buffer_line(b, buffer_i);

	/* Skip redraw */
	if (line == buffer_tail(b))
		return;

	/* Find top line */
	for (;;) {

		split_buffer_cols(line, NULL, &text_w, cols, b->pad);

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

	draw_buffer();
	draw_status();
}

void
buffer_scrollback_forw(struct channel *c)
{
	/* Scroll a buffer forward one page */

	unsigned int count = 0,
	             text_w = 0,
	             cols = io_tty_cols(),
	             rows = io_tty_rows() - 4;

	struct buffer *b = &c->buffer;

	struct buffer_line *line = buffer_line(b, b->scrollback);

	/* Skip redraw */
	if (line == buffer_head(b))
		return;

	/* Find top line */
	for (;;) {

		split_buffer_cols(line, NULL, &text_w, cols, b->pad);

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

	draw_buffer();
	draw_status();
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
	draw_all();
}

void
channel_move_prev(void)
{
	/* Set the current channel to the previous channel */

	struct channel *c = channel_get_prev(state.current_channel);

	if (c != state.current_channel) {
		state.current_channel = c;
		draw_all();
	}
}

void
channel_move_next(void)
{
	/* Set the current channel to the next channel */

	struct channel *c = channel_get_next(state.current_channel);

	if (c != state.current_channel) {
		state.current_channel = c;
		draw_all();
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
state_io_cxed(struct server *s)
{
	int ret;

	server_reset(s);
	server_nicks_next(s);

	if (s->pass && (ret = io_sendf(s->connection, "PASS %s", s->pass)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

	if ((ret = io_sendf(s->connection, "NICK %s", s->nick)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

	if ((ret = io_sendf(s->connection, "USER %s 8 * :%s", s->username, s->realname)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

	draw_status();
}

static void
state_io_dxed(struct server *s, va_list ap)
{
	struct channel *c = s->channel;
	va_list ap_copy;

	do {
		va_copy(ap_copy, ap);
		_newline(c, 0, "-!!-", va_arg(ap_copy, const char *), ap_copy);
		va_end(ap_copy);
		channel_reset(c);
		c = c->next;
	} while (c != s->channel);
}

static void
state_io_ping(struct server *s, unsigned int ping)
{
	int ret;

	s->ping = ping;

	if (ping != IO_PING_MIN)
		draw_status();
	else if ((ret = io_sendf(s->connection, "PING :%s", s->host)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));
}

static void
state_io_signal(enum io_sig_t sig)
{
	switch (sig) {
		case IO_SIGWINCH:
			draw_all();
			break;
		default:
			newlinef(state.default_channel, 0, "-!!-", "unhandled signal %d", sig);
	}
}

void
io_cb(enum io_cb_t type, const void *cb_obj, ...)
{
	struct server *s = (struct server *)cb_obj;
	va_list ap;

	va_start(ap, cb_obj);

	switch (type) {
		case IO_CB_CXED:
			state_io_cxed(s);
			_newline(s->channel, 0, "--", va_arg(ap, const char *), ap);
			break;
		case IO_CB_DXED:
			state_io_dxed(s, ap);
			break;
		case IO_CB_PING_0:
		case IO_CB_PING_1:
		case IO_CB_PING_N:
			state_io_ping(s, va_arg(ap, unsigned int));
			break;
		case IO_CB_ERR:
			_newline(s->channel, 0, "-!!-", va_arg(ap, const char *), ap);
			break;
		case IO_CB_INFO:
			_newline(s->channel, 0, "--", va_arg(ap, const char *), ap);
			break;
		case IO_CB_SIGNAL:
			state_io_signal(va_arg(ap, enum io_sig_t));
			break;
		default:
			fatal("unhandled io_cb_t: %d", type);
	}

	va_end(ap);

	redraw();
}

static void
command(struct channel *c, char *buf)
{
	const char *cmnd;
	char *saveptr;
	int err;

	if (!(cmnd = strtok_r(buf, " ", &saveptr))) {
		newline(c, 0, "-!!-", "Messages beginning with ':' require a command");
		return;
	}

	if (!strcasecmp(cmnd, "quit")) {
		io_term();
	}

	if (!strcasecmp(cmnd, "connect")) {

		const char *host = strtok_r(NULL, " ", &saveptr);
		const char *port = strtok_r(NULL, " ", &saveptr);
		const char *pass = strtok_r(NULL, " ", &saveptr);
		const char *user = strtok_r(NULL, " ", &saveptr);
		const char *real = strtok_r(NULL, " ", &saveptr);
		const char *help = ":connect [host [port] [pass] [user] [real]]";
		struct server *s;

		if (host == NULL) {
			if (c->server == NULL) {
				newlinef(c, 0, "-!!-", "%s", help);
			} else if ((err = io_cx(c->server->connection))) {
				newlinef(c, 0, "-!!-", "%s", io_err(err));
			}
		} else {
			port = (port ? port : "6667");
			user = (user ? user : default_username);
			real = (real ? real : default_realname);

			if ((s = server_list_get(&state.servers, host, port)) != NULL) {
				channel_set_current(s->channel);
				newlinef(s->channel, 0, "-!!-", "already connected to %s:%s", host, port);
			} else {
				s = server(host, port, pass, user, real);
				s->connection = connection(s, host, port);
				server_list_add(state_server_list(), s);
				channel_set_current(s->channel);
				io_cx(s->connection);
				draw_all();
			}
		}
		return;
	}

	if (!strcasecmp(cmnd, "disconnect")) {
		io_dx(c->server->connection);
		return;
	}

	if (!strcasecmp(cmnd, "clear")) {
		channel_clear(c);
		return;
	}

	if (!strcasecmp(cmnd, "close")) {
		channel_close(c);
		return;
	}

	if (!strcasecmp(cmnd, "set")) {
		/* TODO user, real, nicks, pass, key */
		return;
	}

	/* TODO:
	 * help
	 * ignore
	 * unignore
	 * version
	 * find
	 * buffers
	 * b#
	 * b<num>
	 */
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

		case CTRL('f'):
			/* Find channel */
			if (current_channel()->server)
				 action(action_find_channel, "Find: ");
			break;

		case CTRL('l'):
			/* Clear current channel */
			/* TODO: as action with confirmation */
			channel_clear(current_channel());
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
			/* Close current channel */
			channel_close(current_channel());
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

	input_hist_push(&(c->input));

	return 1;
}

void
io_cb_read_inp(char *buf, size_t len)
{
	int redraw_input = 0;

	if (len == 0)
		fatal("zero length message");
	else if (action_message)
		redraw_input = state_input_action(buf, len);
	else if (iscntrl(*buf))
		redraw_input = state_input_ctrlch(buf, len);
	else
		redraw_input = input_insert(&current_channel()->input, buf, len);

	if (redraw_input)
		draw_input();

	redraw();
}

void
io_cb_read_soc(char *buf, size_t len, const void *cb_obj)
{
	struct channel *c = ((struct server *)cb_obj)->channel;

	struct irc_message m;

	if (!(irc_message_parse(&m, buf, len)))
		newlinef(c, 0, "-!!-", "failed to parse message");
	else
		irc_recv((struct server *)cb_obj, &m);

	redraw();
}
