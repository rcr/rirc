/* draw.c
 *
 * Draw the elements in state.c to the terminal.
 *
 * Assumes vt-100 compatible escape codes, as such YMMV */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "common.h"
#include "state.h"
/* FIXME: this has to be included after common.h for activity cols */
#include "config.h"

/* Set foreground/background colour */
#define FG(X) "\x1b[38;5;"#X"m"
#define BG(X) "\x1b[48;5;"#X"m"

/* Reset foreground/background colour */
#define FG_R "\x1b[39m"
#define BG_R "\x1b[49m"

#define MOVE(X, Y) "\x1b["#X";"#Y"H"

#define CLEAR_FULL  "\x1b[2J"
#define CLEAR_LINE  "\x1b[2K"
#define CLEAR_RIGHT "\x1b[K"

/* Save and restore the cursor's location */
#define CURSOR_SAVE    "\x1b[s"
#define CURSOR_RESTORE "\x1b[u"

#define SEPARATOR_FG_COL FG_R
#define SEPARATOR_BG_COL BG_R

static void resize(void);
static void draw_buffer(channel*);
static void draw_nav(struct state const*);
static void draw_input(channel*);
static void draw_status(channel*);

static int nick_col(char*);

unsigned int term_rows, term_cols;

void
redraw(channel *c)
{
	if (!draw) return;

	if (draw & D_RESIZE) resize();

	struct state const* st = get_state();

	//TODO: pass st to other draw functions
	if (draw & D_BUFFER) draw_buffer(c);
	if (draw & D_CHANS)  draw_nav(st);
	if (draw & D_INPUT)  draw_input(c);
	if (draw & D_STATUS) draw_status(c);

	draw = 0;

	fflush(stdout);
}

static int
nick_col(char *nick)
{
	int colour = 0;

	while (*nick)
		colour += *nick++;

	return nick_colours[colour % sizeof(nick_colours) / sizeof(nick_colours[0])];
}

/* TODO: this sets some global state...
 *
 * instead, resize() should be a function in state.c, this should be renamed
 * draw_full or similar, and should be passed in the state object like the others,
 * term_cols, term_rows should be moved out of common.h and into the state struct */
static void
resize(void)
{
	unsigned int i;
	struct winsize w;

	/* Get terminal dimensions */
	ioctl(0, TIOCGWINSZ, &w);

	term_rows = (w.ws_row > 0) ? w.ws_row : 0;
	term_cols = (w.ws_col > 0) ? w.ws_col : 0;

	/* Clear, move to top separator */
	printf(CLEAR_FULL MOVE(2, 1));

	/* Draw upper separator */
	printf(SEPARATOR_FG_COL SEPARATOR_BG_COL);
	for (i = 0; i < term_cols; i++)
		printf(HORIZONTAL_SEPARATOR);

	/* Draw bottom bar, set colour back to default */
	printf(FG_R BG_R);
	printf(MOVE(%d, 1) " >>> ", term_rows);

	/* Mark all buffers as resized for next draw */
	rirc->resized = 1;

	channel *c = ccur;

	do {
		c->resized = 1;
	} while ((c = channel_get_next(c)) != ccur);

	/* Draw everything else */
	draw(D_FULL);
}

/* TODO:
 *
 * Colourize line types
 *
 * Functional first draft, could use some cleaning up */
static void
draw_buffer(channel *c)
{
	/* Dynamically draw the current channel's buffer such that:
	 *
	 * - The scrollback line should always be drawn in full when possible
	 * - Lines wrap on whitespace when possible
	 * - The top-most lines draws partially when required
	 * - Buffers requiring fewer rows than available draw from the top down
	 *
	 * Rows are numbered from the top down, 1 to term_rows, so for term_rows = N,
	 * the drawable area for the buffer is bounded [r3, rN-2]:
	 *      __________________________
	 * r1   |         (nav)          |
	 * r2   |------------------------|
	 * r3   |    ::buffer start::    |
	 *      |                        |
	 * ...  |                        |
	 *      |                        |
	 * rN-2 |     ::buffer end::     |
	 * rN-1 |------------------------|
	 * rN   |________(input)_________|
	 *
	 *
	 * So the general steps for drawing are:
	 *
	 * 1. Starting from line L = scrollback, traverse backwards through the
	 *    buffer summing the rows required to draw lines, until the sum
	 *    exceeds the number of rows available
	 *
	 * 2. L now points to the top-most line to be drawn. L might not be able
	 *    to draw in full, so discard the excessive word-wrapped segments and
	 *    draw the remainder
	 *
	 * 3. Traverse forward through the buffer, drawing lines until buffer_head
	 *    is encountered
	 *
	 * 4. Clear any remaining rows that might exist in the case where the lines
	 *    in the channel's buffer are insufficient to fill all rows
	 */

	printf(CURSOR_SAVE);

	/* Establish current, min and max row for drawing */
	int buffer_start = 3, buffer_end = term_rows - 2;
	int print_row = buffer_start;
	int max_row = buffer_end - buffer_start + 1;
	int count_row = 0;

	/* Insufficient rows for drawing */
	if (buffer_end < buffer_start) {
		printf(CURSOR_RESTORE);
		return;
	}

	printf(FG_R BG_R);

	/* (#terminal columns) - strlen((widest nick in c)) - strlen(" HH:MM   ~ ") */
	int text_cols = term_cols - c->draw.nick_pad - 11;

	/* Insufficient columns for drawing */
	if (text_cols < 1)
		goto clear_remainder;

	buffer_line *tmp, *l = c->draw.scrollback;


	/* Empty buffer */
	if (l->text == NULL)
		goto clear_remainder;

	/* If the window has been resized, force all cached line rows to be recalculated */
	if (c->resized) {
		for (tmp = c->buffer; tmp < &c->buffer[SCROLLBACK_BUFFER]; tmp++)
			tmp->rows = 0;

		c->resized = 0;
	}

	/* 1. Find top-most drawable line */
	for (;;) {

		/* Store the number of rows until a resize */
		if (l->rows == 0)
			l->rows = count_line_rows(text_cols, l);

		count_row += l->rows;

		if (count_row >= max_row)
			break;

		tmp = (l == c->buffer) ? &c->buffer[SCROLLBACK_BUFFER - 1] : l - 1;

		if (tmp->text == NULL || tmp == c->buffer_head)
			break;

		l = tmp;
	}

	/* 2. Handle top-most line if it can't draw in full */
	if (count_row > max_row) {
		char *ptr1 = l->text;
		char *ptr2 = l->text + l->len;
		
		int text_fg = (l->text[0] == QUOTE_CHAR ?
				MSG_GREEN_FG : MSG_DEFAULT_FG);

		while (count_row-- > max_row)
			word_wrap(text_cols, &ptr1, ptr2);

		do {
			printf(MOVE(%d, %d) CLEAR_LINE, print_row++, (int)c->draw.nick_pad + 10);
			printf(FG(%d) VERTICAL_SEPARATOR FG(%d) " ", NEUTRAL_FG, text_fg);

			char *print = ptr1;
			char *wrap = word_wrap(text_cols, &ptr1, ptr2);

			while (print < wrap)
				putchar(*print++);
		} while (*ptr1);


		if (l == c->buffer_head)
			goto clear_remainder;

		l = (l == &c->buffer[SCROLLBACK_BUFFER - 1]) ? c->buffer : l + 1;

		if (l->text == NULL)
			goto clear_remainder;
	}

	/* 3. Draw all lines */
	while (print_row <= buffer_end) {

		/* Draw the main line segment */
		printf(MOVE(%d, 1) CLEAR_LINE, print_row++);

		/* Main line segment format example:
		 *
		 * | 01:23  long_nick ~ hello world |
		 * | 12:34        rcr ~ testing     |
		 *
		 * */
		int from_fg = -1;
		int from_bg = -1;

		if (l->type == LINE_DEFAULT)
			;

		else if (l->type == LINE_CHAT)
			from_fg = nick_col(l->from);

		else if (l->type == LINE_PINGED)
			from_fg = 255, from_bg = 1;

		struct tm *tmp = localtime(&l->time);

		/* Timestamp and padding */
		printf(FG(%d) " %02d:%02d  %*s",
				NEUTRAL_FG, tmp->tm_hour, tmp->tm_min,
				(int)(c->draw.nick_pad - strlen(l->from)), "");

		/* Set foreground and background for the line sender */
		if (from_fg >= 0)
			printf(FG(%d), from_fg);

		if (from_bg >= 0)
			printf(BG(%d), from_bg);

		char *ptr1 = l->text;
		char *ptr2 = l->text + l->len;

		char *print = ptr1;
		char *wrap = word_wrap(text_cols, &ptr1, ptr2);

		int text_fg = (l->text[0] == QUOTE_CHAR ?
				MSG_GREEN_FG : MSG_DEFAULT_FG);
		
		/* Line sender and separator */
		printf("%s" FG(%d) BG_R VERTICAL_SEPARATOR FG(%d),
				l->from, NEUTRAL_FG, text_fg);

		while (print < wrap)
			putchar(*print++);

		if (print_row > buffer_end)
			break;

		/* Draw any line continuations */
		while (*ptr1) {
			printf(MOVE(%d, %d) CLEAR_LINE, print_row++, (int)c->draw.nick_pad + 10);
			printf(FG(%d) VERTICAL_SEPARATOR FG(%d) " ", NEUTRAL_FG, text_fg);

			char *print = ptr1;
			char *wrap = word_wrap(text_cols, &ptr1, ptr2);

			while (print < wrap)
				putchar(*print++);

			if (print_row > buffer_end)
				break;
		}

		if (l == c->buffer_head)
			break;

		l = (l == &c->buffer[SCROLLBACK_BUFFER - 1]) ? c->buffer : l + 1;

		if (l->text == NULL)
			break;
	}

clear_remainder:

	/* 4. Clear any remaining rows */
	while (print_row <= buffer_end)
		printf(MOVE(%d, 1) CLEAR_LINE, print_row++);

	printf(CURSOR_RESTORE);
}


/* TODO
 *
 * | [server-name[:port]] *#chan |
 *
 * - Disconnected/parted channels are printed (#chan)
 * - Servers with non-standard ports are printed: server-name:port
 * - Channels that won't fit are printed at a minimum: #...
 *     - eg: | ...chan #chan2 chan3 |   Right printing
 *           | #chan1 #chan2 #ch... |   Left printing
 * */
static void
draw_nav(struct state const* st)
{
	/* Dynamically draw the nav such that:
	 *
	 *  - The current channel is kept framed while navigating
	 *  - Channels are coloured based on their current activity
	 *  - The nav is kept framed between the first and last channels
	 */

	printf(CURSOR_SAVE);
	printf(MOVE(1, 1) CLEAR_LINE);

	static channel *frame_prev, *frame_next;

	channel *tmp, *c = st->current_channel;

	channel *c_first = channel_get_first();
	channel *c_last = channel_get_last();

	/* By default assume drawing starts towards the next channel */
	unsigned int nextward = 1;

	size_t len, total_len = 0;

	/* Bump the channel frames, if applicable */
	if ((total_len = (strlen(c->name) + 2)) >= term_cols) {
		printf(CURSOR_RESTORE);
		return;
	} else if (c == frame_prev && frame_prev != c_first) {
		frame_prev = channel_get_prev(frame_prev);
	} else if (c == frame_next && frame_next != c_last) {
		frame_next = channel_get_next(frame_next);
	}

	/* Calculate the new frames */
	channel *tmp_prev = c, *tmp_next = c;

	for (;;) {

		if (tmp_prev == c_first || tmp_prev == frame_prev) {

			/* Pad out nextward */

			tmp = channel_get_next(tmp_next);
			len = strlen(tmp->name);

			while ((total_len += (len + 2)) < term_cols && tmp != c_first) {

				tmp_next = tmp;

				tmp = channel_get_next(tmp);
				len = strlen(tmp->name);
			}

			break;
		}

		if (tmp_next == c_last || tmp_next == frame_next) {

			/* Pad out prevward */

			tmp = channel_get_prev(tmp_prev);
			len = strlen(tmp->name);

			while ((total_len += (len + 2)) < term_cols && tmp != c_last) {

				tmp_prev = tmp;

				tmp = channel_get_prev(tmp);
				len = strlen(tmp->name);
			}

			break;
		}


		tmp = nextward ? channel_get_next(tmp_next) : channel_get_prev(tmp_prev);
		len = strlen(tmp->name);

		/* Next channel doesn't fit */
		if ((total_len += (len + 2)) >= term_cols)
			break;

		if (nextward)
			tmp_next = tmp;
		else
			tmp_prev = tmp;

		nextward = !nextward;
	}

	frame_prev = tmp_prev;
	frame_next = tmp_next;

	/* Draw coloured channel names, from frame to frame */
	for (c = frame_prev; ; c = channel_get_next(c)) {

		/* Set print colour and print name */
		if (printf(FG(%d), (c == st->current_channel) ? 255 : actv_cols[c->active]) < 0)
			break;

		if (printf(" %s ", c->name) < 0)
			break;

		if (c == frame_next)
			break;
	}

	printf(CURSOR_RESTORE);
}

/* TODO:
 *
 * Could use some cleaning up*/
static void
draw_input(channel *c)
{
	/* Action messages override the input bar */
	if (action_message) {
		printf(MOVE(%d, 6) CLEAR_RIGHT FG(%d) "%s",
				term_rows, MSG_DEFAULT_FG, action_message);
		return;
	}

	unsigned int winsz = term_cols / 3;

	input *in = c->input;

	/* Reframe the input bar window */
	if (in->head > (in->window + term_cols - 6))
		in->window += winsz;
	else if (in->head == in->window - 1)
		in->window = (in->window - winsz > in->line->text)
			? in->window - winsz : in->line->text;

	printf(MOVE(%d, 6) CLEAR_RIGHT FG(%d), term_rows, MSG_DEFAULT_FG);

	char *p = in->window;
	while (p < in->head)
		putchar(*p++);

	p = in->tail;

	char *end = in->tail + term_cols - 5 - (in->head - in->window);

	while (p < end && p < in->line->text + MAX_INPUT)
		putchar(*p++);

	int col = (in->head - in->window);

	printf(MOVE(%d, %d), term_rows, col + 6);
}

static void
draw_status(channel *c)
{
	/* TODO: scrollback status */

	/* server / private chat:
	 * |-[usermodes]-(latency)---...|
	 *
	 * channel:
	 * |-[usermodes]-[chancount chantype chanmodes]/[priv]-(latency)---...|
	 * */

	printf(CURSOR_SAVE);
	printf(MOVE(%d, 1) CLEAR_LINE, term_rows - 1);

	/* Insufficient columns for meaningful status */
	if (term_cols < 3) {
		printf(CURSOR_RESTORE);
		return;
	}

	printf(SEPARATOR_FG_COL SEPARATOR_BG_COL);

	/* Print status to temporary buffer */
	char status_buff[term_cols + 1];

	int ret;
	unsigned int col = 0;

	memset(status_buff, 0, term_cols + 1);

	/* -[usermodes] */
	if (c->server && *c->server->usermodes) {
		ret = snprintf(status_buff + col, term_cols - col + 1, "%s", HORIZONTAL_SEPARATOR "[+");
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;

		ret = snprintf(status_buff + col, term_cols - col + 1, "%s", c->server->usermodes);
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;

		ret = snprintf(status_buff + col, term_cols - col + 1, "%s", "]");
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;
	}

	/* If private chat buffer:
	 * -[priv] */
	if (c->buffer_type == BUFFER_PRIVATE) {
		ret = snprintf(status_buff + col, term_cols - col + 1, "%s", HORIZONTAL_SEPARATOR "[priv]");
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;
	}

	/* If IRC channel buffer:
	 * -[chancount chantype chanmodes] */
	if (c->buffer_type == BUFFER_CHANNEL) {

		ret = snprintf(status_buff + col, term_cols - col + 1, HORIZONTAL_SEPARATOR "[%d", c->nick_count);
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;

		if (c->type_flag) {
			ret = snprintf(status_buff + col, term_cols - col + 1, " %c", c->type_flag);
			if (ret < 0 || (col += ret) >= term_cols)
				goto print_status;
		}

		if (*c->chanmodes) {
			ret = snprintf(status_buff + col, term_cols - col + 1, " +%s", c->chanmodes);
			if (ret < 0 || (col += ret) >= term_cols)
				goto print_status;
		}

		ret = snprintf(status_buff + col, term_cols - col + 1, "%s", "]");
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;
	}

	/* -(latency) */
	if (c->server && c->server->latency_delta) {
		ret = snprintf(status_buff + col, term_cols - col + 1,
				HORIZONTAL_SEPARATOR "(%llds)", (long long) c->server->latency_delta);
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;
	}

print_status:

	printf(status_buff);

	/* Trailing separator */
	while (col++ < term_cols)
		printf(HORIZONTAL_SEPARATOR);

	printf(CURSOR_RESTORE);
}
