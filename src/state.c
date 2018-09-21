/**
 * state.c
 *
 * All manipulation of global program state
 *
 * TODO: moved keys, actions to this file. needs cleanup
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

static void term_state(void);

static void _newline(struct channel*, enum buffer_line_t, const char*, const char*, va_list);

struct channel* current_channel(void) { return state.current_channel; }
struct channel* default_channel(void) { return state.default_channel; }

static void state_io_cxed(struct server*);
static void state_io_dxed(struct server*, const char *);
static void state_io_ping(struct server*, unsigned int);
static void state_io_signal(enum io_sig_t);

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
	/* atexit doesn't set errno */
	if (atexit(term_state) != 0)
		fatal("atexit", 0);

	state.default_channel = state.current_channel = new_channel("rirc", NULL, CHANNEL_T_OTHER);

	/* Splashscreen */
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

	/* Initiate a full redraw */
	draw_all();
	redraw();
}

static void
term_state(void)
{
	/* Exit handler; must return normally */

	/* Reset terminal colours */
	printf("\x1b[38;0;m");
	printf("\x1b[48;0;m");

#ifndef DEBUG
	/* Clear screen */
	if (!fatal_exit) {
		printf("\x1b[H\x1b[J");
		channel_free(state.default_channel);
	}
#endif
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
	/* Static function for handling inserting new lines into buffers */

	char buf[BUFFSIZE];

	int len;

	struct string _from,
	              _text;

	struct user *u = NULL;

	if ((len = vsnprintf(buf, BUFFSIZE, fmt, ap)) < 0) {
		_text.str = "newlinef error: vsprintf failure";
		_text.len = strlen(_text.str);
		_from.str = "-!!-";
		_from.len = strlen(_from.str);
	} else {
		_text.str = buf;
		_text.len = len;
		_from.str = from;

		// FIXME: don't need to get user for many non-user message types
		if ((u = user_list_get(&(c->users), from, 0)) != NULL)
			_from.len = u->nick.len;
		else
			_from.len = strlen(from);
	}

	buffer_newline(&(c->buffer), type, _from, _text, ((u == NULL) ? 0 : u->prfxmodes.prefix));

	c->activity = MAX(c->activity, ACTIVITY_ACTIVE);

	if (c == current_channel())
		draw_buffer();
	else
		draw_nav();
}

struct channel*
new_channel(const char *name, struct server *s, enum channel_t type)
{
	struct channel *c = channel(name, type);

	/* TODO: deprecated, move to channel.c */

	if (s) {
		c->server = s;
		channel_list_add(&s->clist, c);
	}

	draw_all();

	return c;
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
		new_channel(chan, s, CHANNEL_T_CHANNEL);
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
static int input_action(const char*, size_t);
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
static char search_buff[MAX_SEARCH];
static char *search_ptr = search_buff;

static struct channel* search_channels(struct channel*, char*);
static struct channel*
search_channels(struct channel *start, char *search)
{
	if (start == NULL || *search == '\0')
		return NULL;

	/* Start the search one past the input */
	struct channel *c = channel_get_next(start);

	while (c != start) {

		if (strstr(c->name.str, search))
			return c;

		c = channel_get_next(c);
	}

	return NULL;
}
static int
input_action(const char *input, size_t len)
{
	/* Waiting for user confirmation */

	if (len == 1 && (*input == 0x03 || action_handler(*input))) {
		/* ^C canceled the action, or the action was resolved */

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

		if ((ret = io_sendf(s->connection, "QUIT %s", DEFAULT_QUIT_MESG)))
			newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

		io_dx(s->connection);
		server_list_del(state_server_list(), s);
		io_free(s->connection);
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

	va_list ap;

	va_start(ap, fmt);
	vsnprintf(action_buff, MAX_ACTION_MESG, fmt, ap);
	va_end(ap);

	action_handler = a_handler;
	action_message = action_buff;

	draw_input();
}
/* Action line should be:
 *
 *
 * Find: [current result]/[(server if not current server[socket if not 6667])] : <search input> */
static int
action_find_channel(char c)
{
	/* Incremental channel search */

	/* \n confirms selecting the current match */
	if (c == '\n' && search_cptr) {
		*(search_ptr = search_buff) = '\0';
		channel_set_current(search_cptr);
		search_cptr = NULL;
		draw_all();
		return 1;
	}

	/* \n, Esc, ^C cancels a search if no results are found */
	if (c == '\n' || c == 0x1b || c == 0x03) {
		*(search_ptr = search_buff) = '\0';
		return 1;
	}

	/* ^F repeats the search forward from the current result,
	 * or resets search criteria if no match */
	if (c == 0x06) {
		if (search_cptr == NULL) {
			*(search_ptr = search_buff) = '\0';
			action(action_find_channel, "Find: ");
			return 0;
		}

		search_cptr = search_channels(search_cptr, search_buff);
	} else if (c == 0x7f && search_ptr > search_buff) {
		/* Backspace */

		*(--search_ptr) = '\0';

		search_cptr = search_channels(current_channel(), search_buff);
	} else if (isprint(c) && search_ptr < search_buff + MAX_SEARCH && (search_cptr != NULL || *search_buff == '\0')) {
		/* All other input */

		*(search_ptr++) = c;
		*search_ptr = '\0';

		search_cptr = search_channels(current_channel(), search_buff);
	}

	/* Reprint the action message */
	if (search_cptr == NULL) {
		if (*search_buff)
			action(action_find_channel, "Find: NO MATCH -- %s", search_buff);
		else
			action(action_find_channel, "Find: ");
	} else {
		/* Found a channel */
		if (search_cptr->server == current_channel()->server) {
			action(action_find_channel, "Find: %s -- %s",
					search_cptr->name.str, search_buff);
		} else {
			if (!strcmp(search_cptr->server->port, "6667"))
				action(action_find_channel, "Find: %s/%s -- %s",
						search_cptr->server->host, search_cptr->name.str, search_buff);
			else
				action(action_find_channel, "Find: %s:%s/%s -- %s",
						search_cptr->server->host, search_cptr->server->port,
						search_cptr->name.str, search_buff);
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
			if (0 != (ret = io_sendf(c->server->connection, "PART %s", c->name.str))) {
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
	             text_w,
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
	             text_w,
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

/* Usefull server/channel structure abstractions for drawing */

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






static void
state_io_cxed(struct server *s)
{
	int ret;

	server_nicks_reset(s);
	server_nicks_next(s);

	s->ping = 0;
	draw_status();

	if (s->pass && (ret = io_sendf(s->connection, "PASS %s", s->pass)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

	if ((ret = io_sendf(s->connection, "NICK %s", s->nick)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

	if ((ret = io_sendf(s->connection, "USER %s 8 * :%s", s->username, s->realname)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));
}

static void
state_io_dxed(struct server *s, const char *reason)
{
	for (struct channel *c = s->channel->next; c != s->channel; c = c->next) {
		newlinef(c, 0, "-!!-", "(disconnected %s)", reason);
		channel_reset(c);
	}
}

static void
state_io_ping(struct server *s, unsigned int ping)
{
	int ret;

	s->ping = ping;

	if (ping != IO_PING_MIN)
		draw_status();
	else if ((ret = io_sendf(s->connection, "PING")))
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
			state_io_dxed(s, va_arg(ap, const char*));
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
send_cmnd(struct channel *c, char *buf)
{
	const char *cmnd;

	if (!(cmnd = getarg(&buf, " "))) {
		newline(c, 0, "-!!-", "Messages beginning with ':' require a command");
		return;
	}

	if (!strcasecmp(cmnd, "quit")) {
		/* TODO: close servers, free */
		exit(EXIT_SUCCESS);
	}

	if (!strcasecmp(cmnd, "connect")) {

		const char *host = getarg(&buf, " "),
		           *port = getarg(&buf, " "),
		           *pass = getarg(&buf, " "),
		           *user = getarg(&buf, " "),
		           *real = getarg(&buf, " "),
		           *help = ":connect [host [port] [pass] [user] [real]]";

		struct server *s;

		if (host == NULL) {
			int err;
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
				if ((s = server(host, port, pass, user, real)) == NULL)
					fatal("failed to create server", 0);

				server_list_add(state_server_list(), s);
				channel_set_current(s->channel);
				io_cx(s->connection);
				draw_all();
			}
		}
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
}

static int input_cchar(const char*, size_t);
static int
input_cchar(const char *c, size_t count)
{
	/* Input a control character or escape sequence */

	/* ESC begins a key sequence */
	if (*c == 0x1b) {

		c++;

		if (*c == 0)
			return 0;

		/* arrow up */
		else if (!strncmp(c, "[A", count - 1))
			return input_scroll_back(current_channel()->input, io_tty_cols());

		/* arrow down */
		else if (!strncmp(c, "[B", count - 1))
			return input_scroll_forw(current_channel()->input, io_tty_cols());

		/* arrow right */
		else if (!strncmp(c, "[C", count - 1))
			return cursor_right(current_channel()->input);

		/* arrow left */
		else if (!strncmp(c, "[D", count - 1))
			return cursor_left(current_channel()->input);

		/* delete */
		else if (!strncmp(c, "[3~", count - 1))
			return delete_right(current_channel()->input);

		/* page up */
		else if (!strncmp(c, "[5~", count - 1))
			buffer_scrollback_back(current_channel());

		/* page down */
		else if (!strncmp(c, "[6~", count - 1))
			buffer_scrollback_forw(current_channel());

	} else switch (*c) {

		/* Backspace */
		case 0x7F:
			return delete_left(current_channel()->input);

		/* Horizontal tab */
		case 0x09:
			return tab_complete(current_channel()->input, &(current_channel()->users));

		/* ^C */
		case 0x03:
			/* Cancel current input */
			current_channel()->input->head = current_channel()->input->line->text;
			current_channel()->input->tail = current_channel()->input->line->text + RIRC_MAX_INPUT;
			current_channel()->input->window = current_channel()->input->line->text;
			return 1;

		/* ^F */
		case 0x06:
			/* Find channel */
			if (current_channel()->server)
				 action(action_find_channel, "Find: ");
			break;

		/* ^L */
		case 0x0C:
			/* Clear current channel */
			/* TODO: as action with confirmation */
			channel_clear(current_channel());
			break;

		/* ^P */
		case 0x10:
			/* Go to previous channel */
			channel_move_prev();
			break;

		/* ^N */
		case 0x0E:
			/* Go to next channel */
			channel_move_next();
			break;

		/* ^X */
		case 0x18:
			/* Close current channel */
			channel_close(current_channel());
			break;

		/* ^U */
		case 0x15:
			/* Scoll buffer up */
			buffer_scrollback_back(current_channel());
			break;

		/* ^D */
		case 0x04:
			/* Scoll buffer down */
			buffer_scrollback_forw(current_channel());
			break;
	}

	return 0;
}

void
io_cb_read_inp(char *buff, size_t count)
{
	/* TODO: cleanup, switch on input type/contents */

	int redraw_input = 0;

	if (action_message) {
		redraw_input = input_action(buff, count);
	} else if (*buff == 0x0A) {
		/* Line feed */
		char sendbuf[BUFFSIZE];

		if (input_empty(state.current_channel->input))
			return;

		_send_input(state.current_channel->input, sendbuf);

		switch (sendbuf[0]) {
			case ':':
				send_cmnd(state.current_channel, sendbuf + 1);
				break;
			default:
				send_mesg(current_channel()->server, current_channel(), sendbuf);
		}
		redraw_input = 1;

	} else if (iscntrl(*buff)) {
		redraw_input = input_cchar(buff, count);
	} else {
		redraw_input = input(current_channel()->input, buff, count);
	}

	if (redraw_input)
		draw_input();

	redraw();
}

void
io_cb_read_soc(char *buff, size_t count, const void *cb_obj)
{
	/* TODO: */
	(void)(count);

	struct channel *c = ((struct server *)cb_obj)->channel;

	struct parsed_mesg p;

	if (!(parse_mesg(&p, buff)))
		newlinef(c, 0, "-!!-", "failed to parse message");
	else
		recv_mesg((struct server *)cb_obj, &p);

	redraw();
}
