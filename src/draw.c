/* Draw the elements in state.c to the terminal
 * using terminal using vt-100 compatible escape codes
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "common.h"
#include "state.h"

/* Set foreground/background color */
#define FG(X) "\x1b[38;5;"#X"m"
#define BG(X) "\x1b[48;5;"#X"m"

/* Set bold foreground bold color */
#define FG_B(X) "\x1b[38;5;"#X";1m"

/* Reset foreground/background color */
#define FG_R "\x1b[39m"
#define BG_R "\x1b[49m"

#define MOVE(X, Y) "\x1b["#X";"#Y"H"

#define CLEAR_FULL  "\x1b[2J"
#define CLEAR_LINE  "\x1b[2K"
#define CLEAR_RIGHT "\x1b[K"

/* Save and restore the cursor's location */
#define CURSOR_SAVE    "\x1b[s"
#define CURSOR_RESTORE "\x1b[u"

/* TODO: move these things to config file, fix colour issues */
#define NEUTRAL_FG 239
#define MSG_DEFAULT_FG 250
#define MSG_GREEN_FG 113
#define QUOTE_CHAR '>'
static int nick_colours[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
static int actv_cols[ACTIVITY_T_SIZE] = {239, 247, 3};
//#define SEPARATOR "―"
#define SEPARATOR "-"

static void resize(void);
static void draw_buffer(channel*);
static void draw_chans(channel*);
static void draw_input(channel*);
static void draw_status(channel*);

static int nick_col(char*);

int term_rows, term_cols;

void
redraw(channel *c)
{
	if (!draw) return;

	if (draw & D_RESIZE) resize();

	if (draw & D_BUFFER) draw_buffer(c);
	if (draw & D_CHANS)  draw_chans(c);
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

static void
resize(void)
{
	struct winsize w;

	/* Get terminal dimensions */
	ioctl(0, TIOCGWINSZ, &w);

	term_rows = w.ws_row;
	term_cols = w.ws_col;

	/* Clear, move to top separator and set color */
	printf(CLEAR_FULL MOVE(2, 1) FG(%d), NEUTRAL_FG);

	/* Draw upper separator */
	for (int i = 0; i < term_cols; i++)
		printf(SEPARATOR);

	/* Draw bottom bar, set color back to default */
	printf(MOVE(%d, 1) " >>> " FG(%d), term_rows, MSG_DEFAULT_FG);

	/* Mark all buffers as resized for next draw */
	rirc->resized = 1;

	channel *c = ccur;
	do {
		c->resized = 1;
	} while ((c = channel_switch(c, 1)) != ccur);

	/* Draw everything else */
	draw(D_FULL);
}

/* TODO:
 *
 * Colorize line types
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
	 * r1   |     (channel bar)      |
	 * r2   |------------------------|
	 * r3   |    ::buffer start::    |
	 *      |                        |
	 * ...  |                        |
	 *      |                        |
	 * rN-2 |     ::buffer end::     |
	 * rN-1 |------------------------|
	 * rN   |______(input bar) ______|
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
	if (buffer_end < buffer_start)
		return;

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
			printf(FG(%d) "~" FG(%d) " ", NEUTRAL_FG, text_fg);

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
		printf("%s" FG(%d) BG_R " ~ " FG(%d),
				l->from, NEUTRAL_FG, text_fg);

		while (print < wrap)
			putchar(*print++);

		if (print_row > buffer_end)
			break;

		/* Draw any line continuations */
		while (*ptr1) {
			printf(MOVE(%d, %d) CLEAR_LINE, print_row++, (int)c->draw.nick_pad + 10);
			printf(FG(%d) "~" FG(%d) " ", NEUTRAL_FG, text_fg);

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

/* TODO:
 *
 * For non-default port, include it with the server name,
 * eg: localhost:9999
 *
 * Keep the current channel 'framed' similar to the input bar
 *
 * either (#channel) for disconnected/parted channels
 * or a different color/background color
 * */
static void
draw_chans(channel *ccur)
{
	printf(CURSOR_SAVE);
	printf(MOVE(1, 1) CLEAR_LINE);

	int len, width = 0;

	/* FIXME: temporary fix */
	channel *c = (ccur == rirc) ? ccur : ccur->server->channel;

	do {
		len = strlen(c->name);
		if (width + len + 4 < term_cols) {

			printf(FG(%d) "  %s  ", (c == ccur) ? 255 : actv_cols[c->active], c->name);

			width += len + 4;
			c = c->next;
		}
		else break;
	/* FIXME: temporary fix */
	} while (c != rirc && c != ccur->server->channel);

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

	int winsz = term_cols / 3;

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
	 * --[usermodes]--(latency)-
	 *
	 * channel:
	 * --[usermodes]--[chancount chantype chanmodes]/[priv]--(latency)-
	 * */

	if (term_cols < 3)
		return;

	printf(CURSOR_SAVE);
	printf(MOVE(%d, 1) CLEAR_LINE FG(%d), term_rows - 1, NEUTRAL_FG);

	/* Print status to temporary buffer */
	char status_buff[term_cols + 1];

	int ret, col = 0;

	memset(status_buff, 0, term_cols + 1);

	/* -[usermodes]-*/
	if (c->server && *c->server->usermodes) {
		ret = snprintf(status_buff + col, term_cols - col + 1, "%s", SEPARATOR "[+");
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;

		ret = snprintf(status_buff + col, term_cols - col + 1, "%s", c->server->usermodes);
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;

		ret = snprintf(status_buff + col, term_cols - col + 1, "%s", "]" SEPARATOR);
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;
	}

	/* If private chat buffer:
	 * -[priv]- */
	if (c->buffer_type == BUFFER_PRIVATE) {
		ret = snprintf(status_buff + col, term_cols - col + 1, "%s", SEPARATOR "[priv]" SEPARATOR);
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;
	}

	/* If IRC channel buffer:
	 * -[chancount chantype chanmodes]- */
	if (c->buffer_type == BUFFER_CHANNEL) {

		ret = snprintf(status_buff + col, term_cols - col + 1, SEPARATOR "[%d", c->nick_count);
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

		ret = snprintf(status_buff + col, term_cols - col + 1, "%s", "]" SEPARATOR);
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;
	}

	/* -(latency)- */
	if (c->server && c->server->latency_delta) {
		ret = snprintf(status_buff + col, term_cols - col + 1,
				SEPARATOR "(%llds)" SEPARATOR , (long long) c->server->latency_delta);
		if (ret < 0 || (col += ret) >= term_cols)
			goto print_status;
	}

print_status:

	printf(status_buff);

	/* Trailing separator */
	while (col++ < term_cols)
		printf(SEPARATOR);

	printf(CURSOR_RESTORE);
}
