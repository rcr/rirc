/**
 * state.c
 *
 * All manipulation of global program state
 *
 **/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <stdio.h>

#include "src/draw.h"
#include "src/io.h"
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
static void state_io_dxed(struct server*);
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

	state.default_channel = state.current_channel = new_channel("rirc", NULL, NULL, CHANNEL_T_OTHER);

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

	free_channel(state.default_channel);

	/* Reset terminal colours */
	printf("\x1b[38;0;m");
	printf("\x1b[48;0;m");

#ifndef DEBUG
	/* Clear screen */
	if (!fatal_exit)
		printf("\x1b[H\x1b[J");
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

	if (c == ccur)
		draw_buffer();
	else
		draw_nav();
}

struct channel*
new_channel(const char *name, struct server *s, struct channel *chanlist, enum channel_t type)
{
	struct channel *c = channel(name, type);

	/* TODO: deprecated, move to channel.c */

	c->chanmodes_str.type = MODE_STR_CHANMODE;
	c->input = new_input();
	c->server = s;

	/* Append the new channel to the list */
	DLL_ADD(chanlist, c);

	if (s) {
		channel_list_add(&s->clist, c);
	}

	draw_all();

	return c;
}

void
free_channel(struct channel *c)
{
	/* TODO: deprecated, move to channel.c */

	user_list_free(&(c->users));
	free_input(c->input);

	channel_free(c);
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
		new_channel(chan, s, s->channel, CHANNEL_T_CHANNEL);
		chan = strchr(chan, 0) + 1;
	}

	free(base);

	return 0;
}

/* TODO: buffer_clear */
void
channel_clear(struct channel *c)
{
	c->buffer = buffer();
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
struct channel *search_cptr; /* Used for iterative searching, before setting ccur */
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

		draw_input();
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
		struct channel *c = ccur;
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

		search_cptr = search_channels(ccur, search_buff);
	} else if (isprint(c) && search_ptr < search_buff + MAX_SEARCH && (search_cptr != NULL || *search_buff == '\0')) {
		/* All other input */

		*(search_ptr++) = c;
		*search_ptr = '\0';

		search_cptr = search_channels(ccur, search_buff);
	}

	/* Reprint the action message */
	if (search_cptr == NULL) {
		if (*search_buff)
			action(action_find_channel, "Find: NO MATCH -- %s", search_buff);
		else
			action(action_find_channel, "Find: ");
	} else {
		/* Found a channel */
		if (search_cptr->server == ccur->server) {
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

/* TODO: move to channel.c */
void
reset_channel(struct channel *c)
{
	mode_reset(&(c->chanmodes), &(c->chanmodes_str));

	user_list_free(&(c->users));
}

/* TODO: move to channel.c */
void
part_channel(struct channel *c)
{
	/* Set the state of a parted channel */
	reset_channel(c);

	c->parted = 1;
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
				newlinef(c->server->channel, 0, "sendf fail", "%s", io_err(ret));
			}
		}

		/* If closing the current channel, update state to a new channel */
		if (c == ccur) {
			state.current_channel = !(c->next == c->server->channel) ? c->next : c->prev;
			draw_all();
		} else {
			draw_nav();
		}

		DLL_DEL(c->server->channel, c);

		channel_list_del(&c->server->clist, c);

		free_channel(c);
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

	/* FIXME: */
#if 0
	struct server *s = state.servers.head;

	/* First channel of the first server */
	return !s ? state.default_channel : s->channel;
#endif
}

struct channel*
channel_get_last(void)
{
	struct server *s = state.servers.tail;

	return s ? s->channel : NULL;

	/* FIXME: */
#if 0
	struct server *s = state.servers.tail;

	/* Last channel of the last server */
	return !s ? state.default_channel : s->channel->prev;
#endif
}

struct channel*
channel_get_next(struct channel *c)
{
	if (c == state.default_channel)
		return c;
	else
		/* Return the next channel, accounting for server wrap around */
		return !(c->next == c->server->channel) ?  c->next : c->server->next->channel;
}

struct channel*
channel_get_prev(struct channel *c)
{
	if (c == state.default_channel)
		return c;
	else
		/* Return the previous channel, accounting for server wrap around */
		return !(c == c->server->channel) ?  c->prev : c->server->prev->channel->prev;
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

	if (s->pass && (ret = io_sendf(s->connection, "PASS %s", s->pass)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

	if ((ret = io_sendf(s->connection, "NICK %s", s->nick)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));

	if ((ret = io_sendf(s->connection, "USER %s 8 * :%s", s->username, s->realname)))
		newlinef(s->channel, 0, "-!!-", "sendf fail: %s", io_err(ret));
}

static void
state_io_dxed(struct server *s)
{
	(void)s;
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
			state_io_dxed(s);
			_newline(s->channel, 0, "-!!-", va_arg(ap, const char *), ap);
			break;
		case IO_CB_ERR:
			_newline(s->channel, 0, "-!!-", va_arg(ap, const char *), ap);
			break;
		case IO_CB_INFO:
			_newline(s->channel, 0, "--", va_arg(ap, const char *), ap);
			break;
		case IO_CB_PING_0:
		case IO_CB_PING_1:
		case IO_CB_PING_N:
			/* TODO  0: clear ping, draw
			 *       1: send ping message
			 *       N: set ping, draw */
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

	/* TODO:
	 *
	 * :connect
	 * :close
	 * :set (user, real, nicks, pass, channel key)
	 */
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
			input_scroll_backwards(ccur->input);

		/* arrow down */
		else if (!strncmp(c, "[B", count - 1))
			input_scroll_forwards(ccur->input);

		/* arrow right */
		else if (!strncmp(c, "[C", count - 1))
			cursor_right(ccur->input);

		/* arrow left */
		else if (!strncmp(c, "[D", count - 1))
			cursor_left(ccur->input);

		/* delete */
		else if (!strncmp(c, "[3~", count - 1))
			delete_right(ccur->input);

		/* page up */
		else if (!strncmp(c, "[5~", count - 1))
			buffer_scrollback_back(ccur);

		/* page down */
		else if (!strncmp(c, "[6~", count - 1))
			buffer_scrollback_forw(ccur);

	} else switch (*c) {

		/* Backspace */
		case 0x7F:
			delete_left(ccur->input);
			break;

		/* Horizontal tab */
		case 0x09:
			tab_complete(ccur->input);
			break;

		/* ^C */
		case 0x03:
			/* Cancel current input */
			ccur->input->head = ccur->input->line->text;
			ccur->input->tail = ccur->input->line->text + RIRC_MAX_INPUT;
			ccur->input->window = ccur->input->line->text;
			draw_input();
			break;

		/* ^F */
		case 0x06:
			/* Find channel */
			if (ccur->server)
				 action(action_find_channel, "Find: ");
			break;

		/* ^L */
		case 0x0C:
			/* Clear current channel */
			channel_clear(ccur);
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
			channel_close(ccur);
			break;

		/* ^U */
		case 0x15:
			/* Scoll buffer up */
			buffer_scrollback_back(ccur);
			break;

		/* ^D */
		case 0x04:
			/* Scoll buffer down */
			buffer_scrollback_forw(ccur);
			break;
	}

	return 0;
}

void
io_cb_read_inp(char *buff, size_t count)
{
	/* TODO: switch on input type, removed from input.c */

	if (action_message) {
		input_action(buff, count);
	} else if (iscntrl(*buff)) {
		input_cchar(buff, count);
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
				send_mesg(ccur->server, ccur, sendbuf);
		}
	} else {
		input(ccur->input, buff, count);
	}

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
