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
#include <sys/ioctl.h>

#include "draw.h"
#include "input.h"
#include "state.h"
#include "utils.h"

/* State of rirc */
static struct
{
	channel *current_channel; /* the current channel being drawn */
	channel *default_channel; /* the default rirc channel at startup */

	server *server_list;

	unsigned int term_cols;
	unsigned int term_rows;

	union draw draw;
} state;

static int action_close_server(char);

static void _newline(channel*, enum buffer_line_t, const char*, const char*, size_t);

channel* current_channel(void) { return state.current_channel; }
channel* default_channel(void) { return state.default_channel; }

unsigned int _term_cols(void) { return state.term_cols; }
unsigned int _term_rows(void) { return state.term_rows; }

/* Set draw bits */
#define X(BIT) void draw_##BIT(void) { state.draw.bits.BIT = 1; }
DRAW_BITS
#undef X

/* Set all draw bits */
void draw_all(void) { state.draw.all_bits = -1; }

void
redraw(void)
{
	draw(state.draw);

	state.draw.all_bits = 0;
}

void
resize(void)
{
	/* Resize the terminal dimensions */

	struct winsize w;

	ioctl(0, TIOCGWINSZ, &w);

	state.term_rows = (w.ws_row > 0) ? w.ws_row : 0;
	state.term_cols = (w.ws_col > 0) ? w.ws_col : 0;

	draw_all();
}

void
init_state(void)
{
	state.default_channel = state.current_channel = new_channel("rirc", NULL, NULL, BUFFER_OTHER);

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
	resize();
}

void
free_state(void)
{
	free_channel(state.default_channel);
}

void
newline(channel *c, enum buffer_line_t type, const char *from, const char *mesg)
{
	/* Default wrapper for _newline because length of message won't be known */

	_newline(c, type, from, mesg, strlen(mesg)); /* FIXME: sizeof for static strings? */
}

void
newlinef(channel *c, enum buffer_line_t type, const char *from, const char *fmt, ...)
{
	/* Formating wrapper for _newline */

	char buff[BUFFSIZE];
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(buff, BUFFSIZE, fmt, ap);
	va_end(ap);

	_newline(c, type, from, buff, len);
}

static void
_newline(channel *c, enum buffer_line_t type, const char *from, const char *mesg, size_t len)
{
	/* Static function for handling inserting new lines into buffers */

	/* FIXME: pass len still? */
	UNUSED(c);
	UNUSED(type);
	UNUSED(from);
	UNUSED(mesg);
	UNUSED(len);

	if (c == NULL)
		fatal("channel is null");

	buffer_newline(&c->buffer, type, from, mesg);

	if (c->active < ACTIVITY_ACTIVE)
		c->active = ACTIVITY_ACTIVE;

	if (c == ccur)
		draw_buffer();
	else
		draw_nav();
}

channel*
new_channel(char *name, server *server, channel *chanlist, enum buffer_t type)
{
	channel *c;

	if ((c = calloc(1, sizeof(*c))) == NULL)
		fatal("calloc");

	c->buffer = buffer(type);
	c->input = new_input();
	c->name = strdup(name);
	c->server = server;

	/* Append the new channel to the list */
	DLL_ADD(chanlist, c);

	draw_all();

	return c;
}

void
free_channel(channel *c)
{
	nicklist_free(&(c->nicklist));

	free_input(c->input);
	free(c->name);
	free(c);
}

channel*
channel_get(char *chan, server *s)
{
	if (!s)
		return NULL;

	channel *c = s->channel;

	do {
		if (!strcasecmp(c->name, chan))
			return c;

	} while ((c = c->next) != s->channel);

	return NULL;
}

/* FIXME: functions that operate on a buffer should just take the buffer as an argument */
void
channel_clear(channel *c)
{
	UNUSED(c);
	/* FIXME: c->buffer = buffer_init(c->buffer.type) */
	;
}

/* Confirm closing a server */
static int
action_close_server(char c)
{
	if (c == 'n' || c == 'N')
		return 1;

	if (c == 'y' || c == 'Y') {

		//FIXME: logic here sucks
		channel *c = ccur;

		/* If closing the last server */
		if ((state.current_channel = c->server->next->channel) == c->server->channel)
			state.current_channel = state.default_channel;

		server_disconnect(c->server, 0, 1, DEFAULT_QUIT_MESG);

		draw_all();

		return 1;
	}

	return 0;
}

void
nicklist_print(channel *c)
{
	newline(c, 0, "TODO", "Print ignore list to channel");
}

void
reset_channel(channel *c)
{
	memset(c->chanmodes, 0, MODE_SIZE);

	nicklist_free(&(c->nicklist));
}

void
part_channel(channel *c)
{
	/* Set the state of a parted channel */
	reset_channel(c);

	c->parted = 1;
}

void
channel_close(channel *c)
{
	/* Close a channel. If the current channel is being
	 * closed, update state appropriately */

	if (c == state.default_channel) {
		newline(c, 0, "--", "Type /quit to exit rirc");
		return;
	}

	if (c->buffer.type == BUFFER_SERVER) {
		/* Closing a server, confirm the number of channels being closed */

		int num_chans = 0;

		while ((c = c->next)->buffer.type != BUFFER_SERVER)
			num_chans++;

		if (num_chans)
			action(action_close_server, "Close server '%s'? Channels: %d   [y/n]",
					c->server->host, num_chans);
		else
			action(action_close_server, "Close server '%s'?   [y/n]", c->server->host);
	} else {
		/* Closing a channel */

		if (c->buffer.type == BUFFER_CHANNEL && !c->parted)
			sendf(NULL, c->server, "PART %s", c->name);

		/* If closing the current channel, update state to a new channel */
		if (c == ccur) {
			state.current_channel = !(c->next == c->server->channel) ? c->next : c->prev;
			draw_all();
		} else {
			draw_nav();
		}

		DLL_DEL(c->server->channel, c);
		free_channel(c);
	}
}

/* Get a channel's next/previous, taking into account server wraparound */
channel*
channel_switch(channel *c, int next)
{
	channel *ret;

	/* c in this case is the main buffer */
	if (c->server == NULL)
		return c;

	if (next)
		/* If wrapping around forwards, get next server's first channel */
		ret = !(c->next == c->server->channel) ?
			c->next : c->server->next->channel;
	else
		/* If wrapping around backwards, get previous server's last channel */
		ret = !(c == c->server->channel) ?
			c->prev : c->server->prev->channel->prev;

	ret->active = ACTIVITY_DEFAULT;

	draw_all();

	return ret;
}

void
buffer_scrollback_back(channel *c)
{
	/* Scroll a buffer back one page */

	struct buffer *b = &c->buffer;

	unsigned int buffer_i = b->scrollback,
	             count = 0,
				 text_w,
	             cols = _term_cols(),
	             rows = _term_rows() - 4;

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

	/* Top line is partial */
	if (count == rows)
		b->scrollback--;

	draw_buffer();
	draw_status();
}

void
buffer_scrollback_forw(channel *c)
{
	/* Scroll a buffer forward one page */

	unsigned int count = 0,
				 text_w,
	             cols = _term_cols(),
	             rows = _term_rows() - 4;

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

	if (count == rows)
		b->scrollback++;

	draw_buffer();
	draw_status();
}

void
auto_nick(char **autonick, char *nick)
{
	/* Copy the next choice in a server's nicks to it's nick pointer, e.g.:
	 *   "nick, nick_, nick__"
	 *
	 * If the server's options are exhausted (or NULL) set a randomized default */

	char *p = *autonick;
	int i;

	while (p && (*p == ' ' || *p == ','))
		p++;

	if (p && *p != '\0') {
		/* Copy the next choice into nick */

		for (i = 0; i < NICKSIZE && *p && *p != ' ' && *p != ','; i++)
			*nick++ = *p++;

		*autonick = p;
	} else {
		/* Autonicks exhausted, generate a random nick */

		char *base = "rirc_";
		char *cset = "0123456789ABCDEF";

		strcpy(nick, base);
		nick += strlen(base);

		size_t len = strlen(cset);

		for (i = 0; i < 4; i++)
			*nick++ = cset[rand() % len];
	}

	*nick = '\0';
}

static void
set_mode_str(char mode_str[MODE_SIZE], const char *modes)
{
	/* Given a string of modes, eg: +abc, add or remove flags
	 * from the mode_str set, maintaining alphabetic order */

	char *ptr, pm = 0;

	while (*modes) {

		if (*modes == '-' || *modes == '+')
			pm = *modes;

		/* Silently skip invalid flags */
		else if (!isalpha(*modes) || !pm)
			;

		/* Add flags */
		else if (pm == '+' && !strchr(mode_str, *modes)) {

			char *tmp;

			/* Find location to insert, alphabetically */
			for (ptr = mode_str; *ptr && *ptr < *modes; ptr++)
				;

			/* Shift flags */
			for (tmp = strchr(mode_str, '\0') + 1; tmp > ptr; tmp--)
				*tmp = *(tmp - 1);

			*ptr = *modes;
		}

		/* Remove flags, if found */
		else if (pm == '-' && (ptr = strchr(mode_str, *modes))) {
			do {
				*ptr = *(ptr + 1);
			} while (*ptr++);
		}

		modes++;
	}
}

void
server_set_mode(server *s, const char *modes)
{
	set_mode_str(s->usermodes, modes);

	if (ccur->server == s)
		draw_status();
}

void
channel_set_mode(channel *c, const char *modes)
{
	set_mode_str(c->chanmodes, modes);

	if (ccur == c)
		draw_status();
}

/* Usefull server/channel structure abstractions for drawing */

channel*
channel_get_first(void)
{
	server *s = get_server_head();

	/* First channel of the first server */
	return !s ? state.default_channel : s->channel;
}

channel*
channel_get_last(void)
{
	server *s = get_server_head();

	/* Last channel of the last server */
	return !s ? state.default_channel : s->prev->channel->prev;
}

channel*
channel_get_next(channel *c)
{
	if (c == state.default_channel)
		return c;
	else
		/* Return the next channel, accounting for server wrap around */
		return !(c->next == c->server->channel) ?  c->next : c->server->next->channel;
}

channel*
channel_get_prev(channel *c)
{
	if (c == state.default_channel)
		return c;
	else
		/* Return the previous channel, accounting for server wrap around */
		return !(c == c->server->channel) ?  c->prev : c->server->prev->channel->prev;
}

void
channel_set_current(channel *c)
{
	/* Set the state to an arbitrary channel */

	state.current_channel = c;

	draw_all();
}

void
channel_move_prev(void)
{
	/* Set the current channel to the previous channel */

	channel *c = channel_get_prev(state.current_channel);

	if (c != state.current_channel) {
		state.current_channel = c;
		draw_all();
	}
}

void
channel_move_next(void)
{
	/* Set the current channel to the next channel */

	channel *c = channel_get_next(state.current_channel);

	if (c != state.current_channel) {
		state.current_channel = c;
		draw_all();
	}
}
