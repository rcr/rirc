/* All manipulation of stateful ui elements
 * */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "common.h"

static int action_close_server(char);

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

	line *new_line;

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
new_channel(char *name, server *server, channel *chanlist)
{
	channel *c;

	if ((c = calloc(1, sizeof(*c))) == NULL)
		fatal("calloc");

	c->server = server;
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
	line *l;
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
		if (!strcmp(c->name, chan))
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

/* Close a channel buffer/server and return the next channel */
channel*
channel_close(channel *c)
{
	/* Close a buffer,
	 *
	 * if closing a server buffer, confirm with the user */

	channel *ret = c;

	/* c in this case is the main buffer */
	if (c->server == NULL)
		return c;

	if (!c->type) {
		/* Closing a server, confirm the number of channels being closed */

		int num_chans = 0;

		while ((c = c->next)->type)
			num_chans++;

		if (num_chans)
			action(action_close_server, "Close server '%s'? Channels: %d   [y/n]",
					c->server->host, num_chans);
		else
			action(action_close_server, "Close server '%s'?   [y/n]", c->server->host);
	} else {
		/* Closing a channel */

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

/* TODO: draw scrollback status if != buffer_head */
void
buffer_scrollback_page(channel *c, int up)
{
	/* TODO Scroll the buffer up or down a full page */
	buffer_scrollback_line(c, up);
}

void
buffer_scrollback_line(channel *c, int up)
{
	/* Scroll the buffer up or down a single line */

	line *tmp, *l = c->draw.scrollback;

	if (up) {
		/* Don't scroll up over the buffer head */
		tmp = (l == c->buffer) ? &c->buffer[SCROLLBACK_BUFFER - 1] : l - 1;

		if (tmp->text != NULL && tmp != c->buffer_head)
			c->draw.scrollback = tmp;
	} else {
		/* Don't scroll down past the buffer head */
		if (l != c->buffer_head)
			c->draw.scrollback = (l == &c->buffer[SCROLLBACK_BUFFER - 1]) ? c->buffer : l + 1;
	}

	draw(D_BUFFER);
}
