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

#include "src/draw.h"
#include "src/state.h"
#include "src/utils.h"

/* State of rirc */
static struct
{
	struct channel *current_channel; /* the current channel being drawn */
	struct channel *default_channel; /* the default rirc channel at startup */

	//TODO: not used???
	struct server *server_list;

	unsigned int term_cols;
	unsigned int term_rows;

	union draw draw;
} state;

static int action_close_server(char);

static void _newline(struct channel*, enum buffer_line_t, const char*, const char*, size_t);

struct channel* current_channel(void) { return state.current_channel; }
struct channel* default_channel(void) { return state.default_channel; }

unsigned int _term_cols(void) { return state.term_cols; }
unsigned int _term_rows(void) { return state.term_rows; }

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
	/* atexit doesn't set errno */
	if (atexit(free_state) != 0)
		fatal("atexit", 0);

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
	/* Exit handler; must return normally */

	free_channel(state.default_channel);
}

void
newline(struct channel *c, enum buffer_line_t type, const char *from, const char *mesg)
{
	/* Default wrapper for _newline because length of message won't be known */

	char errmesg[] = "newline error: mesg is null";

	if (mesg == NULL)
		_newline(c, type, from, errmesg, strlen(errmesg));
	else
		_newline(c, type, from, mesg, strlen(mesg));
}

void
newlinef(struct channel *c, enum buffer_line_t type, const char *from, const char *fmt, ...)
{
	/* Formating wrapper for _newline */

	char buff[BUFFSIZE]; char errmesg[] = "newlinef error: vsprintf failure";
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(buff, BUFFSIZE - 1, fmt, ap);
	va_end(ap);

	if (len < 0)
		_newline(c, 0, "-!!-", errmesg, sizeof(errmesg));
	else
		_newline(c, type, from, buff, len);
}

static void
_newline(struct channel *c, enum buffer_line_t type, const char *from, const char *mesg, size_t mesg_len)
{
	/* Static function for handling inserting new lines into buffers */

	if (c == NULL)
		fatal("channel is null", 0);

	struct user *u;

	struct string _from = { .str = from };
	struct string _text = { .str = mesg, .len = mesg_len };

	if ((u = user_list_get(&(c->users), from, 0)) != NULL)
		_from.len = u->nick.len;
	else
		_from.len = strlen(from);

	buffer_newline(&(c->buffer), type, _from, _text, ((u == NULL) ? 0 : u->prfxmodes.prefix));

	c->activity = MAX(c->activity, ACTIVITY_ACTIVE);

	if (c == ccur)
		draw_buffer();
	else
		draw_nav();
}

struct channel*
new_channel(char *name, struct server *s, struct channel *chanlist, enum buffer_t type)
{
	struct channel *c = channel(name);

	/* TODO: deprecated, move to channel.c */

	c->buffer = buffer(type);
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

void
channel_clear(struct channel *c)
{
	c->buffer = buffer(c->buffer.type);

	draw_buffer();
}

/* Confirm closing a server */
static int
action_close_server(char c)
{
	if (c == 'n' || c == 'N')
		return 1;

	if (c == 'y' || c == 'Y') {

		//FIXME: logic here sucks
		struct channel *c = ccur;

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
reset_channel(struct channel *c)
{
	mode_reset(&(c->chanmodes), &(c->chanmodes_str));

	user_list_free(&(c->users));
}

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
			sendf(NULL, c->server, "PART %s", c->name.str);

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

/* Get a channel's next/previous, taking into account server wraparound */
struct channel*
channel_switch(struct channel *c, int next)
{
	struct channel *ret;

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

	draw_all();

	return ret;
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

	/* Bottom line in view draws in full; scroll forward one additional line */
	if (count == rows && line != buffer_head(b))
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
			/* coverity[dont_call] Acceptable use of insecure rand() function */
			*nick++ = cset[rand() % len];
	}

	*nick = '\0';
}

/* Usefull server/channel structure abstractions for drawing */

struct channel*
channel_get_first(void)
{
	struct server *s = get_server_head();

	/* First channel of the first server */
	return !s ? state.default_channel : s->channel;
}

struct channel*
channel_get_last(void)
{
	struct server *s = get_server_head();

	/* Last channel of the last server */
	return !s ? state.default_channel : s->prev->channel->prev;
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

void
net_cb_read_inp(const char *buff, size_t count)
{
	//newlinef(ccur, 0, "TESTING", "got: %s / %zu", buff, count);
	//draw_buffer();
	/* TODO */
	input(NULL, buff, count);
}

void
net_cb_read_soc(const char *buff, size_t count, struct server *s)
{
	/* TODO */
	(void)(buff);
	(void)(count);
	(void)(s);
}

void
net_cb_cxed(struct server *s, const char *mesg, ...)
{
	/* TODO */
	(void)(s);
	(void)(mesg);
}

void
net_cb_dxed(struct server *s, const char *mesg, ...)
{
	/* TODO */
	(void)(s);
	(void)(mesg);
}

void
net_cb_rxng(struct server *s, const char *mesg, ...)
{
	/* TODO */
	(void)(s);
	(void)(mesg);
}

void
net_cb_ping(struct server *s)
{
	/* TODO */
	(void)(s);
}
