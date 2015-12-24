/**
 * state.c
 *
 * All manipulation of global program state
 *
 **/

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <stdio.h>

#include "common.h"
#include "state.h"

static int action_close_server(char);

/* TODO: move to state struct */
/* Defined in draw.c */
extern int term_rows, term_cols;

/* Global state of rirc */


/* TODO: move all current global state from elsewhere to here */
/* TODO: subdivide by drawn component */
#if 0
/* TODO: in progress */
static void nav_set_frame_L(void);
static void nav_set_frame_R(void);
static void nav_set_frame_C(void);

static struct
{
	channel *current_channel;
	channel *default_channel; /* The serverless 'rirc' buffer */

	struct {
		channel *frame_L;
		channel *frame_R;
	} nav;
} state;
#endif

void
init_state(void)
{
	/* TODO: move these to state struct */
	rirc = ccur = new_channel("rirc", NULL, NULL, BUFFER_OTHER);

	/* Splashscreen */
	newline(rirc, 0, "--", "      _");
	newline(rirc, 0, "--", " _ __(_)_ __ ___");
	newline(rirc, 0, "--", "| '__| | '__/ __|");
	newline(rirc, 0, "--", "| |  | | | | (__");
	newline(rirc, 0, "--", "|_|  |_|_|  \\___|");
	newline(rirc, 0, "--", "");
	newline(rirc, 0, "--", " - version " VERSION);
	newline(rirc, 0, "--", " - compiled " __DATE__ ", " __TIME__);
#ifdef DEBUG
	newline(rirc, 0, "--", " - compiled with DEBUG flags");
#endif

	/* Init a full redraw */
	draw(D_RESIZE);
}

void
free_state(void)
{
	free_channel(rirc);
}

void
newline(channel *c, line_t type, const char *from, const char *mesg)
{
	/* Default wrapper for _newline because length of message won't be known */

	_newline(c, type, from, mesg, strlen(mesg));
}

void
newlinef(channel *c, line_t type, const char *from, const char *fmt, ...)
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

void
_newline(channel *c, line_t type, const char *from, const char *mesg, size_t len)
{
	/* Static function for handling inserting new lines into buffers */

	buffer_line *new_line;

	/* c->buffer_head points to the first printable line, so get the next line in the
	 * circular buffer */
	if ((new_line = c->buffer_head + 1) == &c->buffer[SCROLLBACK_BUFFER])
		new_line = c->buffer;

	/* Increment the channel's scrollback pointer if it pointed to the first or last line, ie:
	 *  - if it points to c->buffer_head, it pointed to the previous first line in the buffer
	 *  - if it points to new_line, it pointed to the previous last line in the buffer
	 *  */
	if (c->draw.scrollback == c->buffer_head || c->draw.scrollback == new_line)
		if (++c->draw.scrollback == &c->buffer[SCROLLBACK_BUFFER])
			c->draw.scrollback = c->buffer;

	c->buffer_head = new_line;

	/* new_channel() memsets c->buffer to 0, so this will either free(NULL) or an old line */
	free(new_line->text);

	if (c == NULL)
		fatal("channel is null");

	/* Set the line meta data */
	new_line->len = len;
	new_line->type = type;
	new_line->time = time(NULL);

	/* Rows are recalculated by the draw routine when == 0 */
	new_line->rows = 0;

	/* If from is NULL, assume server message */
	strncpy(new_line->from, (from) ? from : c->name, NICKSIZE);

	size_t len_from;
	if ((len_from = strlen(new_line->from)) > c->draw.nick_pad)
		c->draw.nick_pad = len_from;

	if (mesg == NULL)
		fatal("mesg is null");

	if ((new_line->text = malloc(new_line->len + 1)) == NULL)
		fatal("newline");

	strcpy(new_line->text, mesg);

	if (c == ccur)
		draw(D_BUFFER);
	else if (!type && c->active < ACTIVITY_ACTIVE) {
		c->active = ACTIVITY_ACTIVE;
		draw(D_CHANS);
	}
}

channel*
new_channel(char *name, server *server, channel *chanlist, buffer_t type)
{
	channel *c;

	if ((c = calloc(1, sizeof(*c))) == NULL)
		fatal("calloc");

	c->server = server;
	c->buffer_type = type;
	c->buffer_head = c->buffer;
	c->active = ACTIVITY_DEFAULT;
	c->input = new_input();
	c->draw.scrollback = c->buffer_head;

	/* TODO: if channel name length exceeds CHANSIZE we'll never appropriately
	 * associate incomming messages with this channel anyways so it shouldn't be allowed */
	strncpy(c->name, name, CHANSIZE);

	/* Append the new channel to the list */
	DLL_ADD(chanlist, c);

	draw(D_FULL);

	return c;
}

void
free_channel(channel *c)
{
	buffer_line *l;
	for (l = c->buffer; l < c->buffer + SCROLLBACK_BUFFER; l++)
		free(l->text);

	free_avl(c->nicklist);
	free_input(c->input);
	free(c);
}

channel*
channel_get(char *chan, server *s)
{
	channel *c = s->channel;

	do {
		if (!strcasecmp(c->name, chan))
			return c;

	} while ((c = c->next) != s->channel);

	return NULL;
}

void
clear_channel(channel *c)
{
	free(c->buffer_head->text);

	c->buffer_head->text = NULL;

	c->draw.nick_pad = 0;

	draw(D_BUFFER);
}

/* Confirm closing a server */
static int
action_close_server(char c)
{
	if (c == 'n' || c == 'N')
		return 1;

	if (c == 'y' || c == 'Y') {

		channel *c = ccur;

		/* If closing the last server */
		if ((ccur = c->server->next->channel) == c->server->channel)
			ccur = rirc;

		server_disconnect(c->server, 0, 1, DEFAULT_QUIT_MESG);

		draw(D_FULL);

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
part_channel(channel *c)
{
	/* Set the state of a parted channel */

	free_avl(c->nicklist);

	c->chanmode = 0;
	c->nick_count = 0;
	c->nicklist = NULL;
	c->parted = 1;
}

channel*
channel_close(channel *c)
{
	/* Close a buffer and return the next
	 *
	 * If closing a server buffer, confirm with the user */

	channel *ret = c;

	/* c in this case is the main buffer */
	if (c->server == NULL)
		return c;

	if (c->buffer_type == BUFFER_SERVER) {
		/* Closing a server, confirm the number of channels being closed */

		int num_chans = 0;

		while ((c = c->next)->buffer_type != BUFFER_SERVER)
			num_chans++;

		if (num_chans)
			action(action_close_server, "Close server '%s'? Channels: %d   [y/n]",
					c->server->host, num_chans);
		else
			action(action_close_server, "Close server '%s'?   [y/n]", c->server->host);
	} else {
		/* Closing a channel */

		if (c->buffer_type == BUFFER_CHANNEL && !c->parted)
			sendf(NULL, c->server, "PART %s", c->name);

		/* If channel c is last in the list, return the previous channel */
		ret = !(c->next == c->server->channel) ?
			c->next : c->prev;

		DLL_DEL(c->server->channel, c);
		free_channel(c);

		draw(D_FULL);
	}

	return ret;
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

	draw(D_FULL);

	return ret;
}

void
buffer_scrollback_back(channel *c)
{
	/* Scroll a buffer back one page */

	buffer_line *l = c->draw.scrollback;

	/* Terminal rows - nav - separator*2 - input */
	int rows = term_rows - 4;

	do {
		/* Circular buffer prev */
		l = (l == c->buffer) ? &c->buffer[SCROLLBACK_BUFFER - 1] : l - 1;

		/* If last scrollback line is found before a full page is counted, do nothing */
		if (l->text == NULL || l == c->buffer_head)
			return;

		rows -= l->rows;

	} while (rows > 0);

	c->draw.scrollback = l;

	draw(D_BUFFER);
}

void
buffer_scrollback_forw(channel *c)
{
	/* Scroll a buffer forward one page */

	buffer_line *l = c->draw.scrollback;

	/* Terminal rows - nav - separator*2 - input */
	int rows = term_rows - 4;

	/* Unfortunately Scrolling forward might encountej
	 * lines that haven't been drawn since resizing */
	int text_cols = term_cols - c->draw.nick_pad - 11;

	do {
		if (l == c->buffer_head)
			break;

		if (l->rows == 0)
			l->rows = count_line_rows(text_cols, l);

		rows -= l->rows;

		/* Circular buffer next */
		l = (l == &c->buffer[SCROLLBACK_BUFFER - 1]) ? c->buffer : l + 1;

	} while (rows > 0);

	c->draw.scrollback = l;

	draw(D_BUFFER);
}

void
auto_nick(char **autonick, char *nick)
{
	char *p = *autonick;
	while (*p == ' ' || *p == ',')
		p++;

	if (*p == '\0') {

		/* Autonicks exhausted, generate a random nick */
		char *base = "rirc_";
		char *cset = "0123456789ABCDEF";

		strcpy(nick, base);
		nick += strlen(base);

		int i, len = strlen(cset);
		for (i = 0; i < 4; i++)
			*nick++ = cset[rand() % len];
	} else {
		int c = 0;
		while (*p != ' ' && *p != ',' && *p != '\0' && c++ < NICKSIZE)
			*nick++ = *p++;
		*autonick = p;
	}

	*nick = '\0';
}


