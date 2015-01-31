#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "common.h"

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

static void resize(void);
static void draw_buffer(channel*);
static void draw_chans(channel*);
static void draw_input(channel*);
static void draw_status(channel*);

static char* word_wrap(int, char**, char*);
static int count_line_rows(int, line*);
static int nick_col(char*);

struct winsize w;

static int nick_colours[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
static int actv_cols[ACTIVITY_T_SIZE] = {239, 247, 3};

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
}

static void
resize(void)
{
	/* Get terminal dimensions */
	ioctl(0, TIOCGWINSZ, &w);

	/* Clear, move to top separator and set color */
	printf(CLEAR_FULL MOVE(2, 1) FG(239));

	/* Draw upper separator */
	for (int i = 0; i < w.ws_col; i++)
		printf("―");

	/* Draw bottom bar, set color back to default */
	printf(MOVE(%d, 1) " >>> " FG(250), w.ws_row);

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
	 * - The buffer_head line should always be drawn in full when possible
	 * - Lines wrap on whitespace when possible
	 * - The top-most lines draws partially when required
	 * - Buffers requiring fewer rows than available draw from the top down
	 *
	 * Rows are numbered from the top down, 1 to w.ws_row, so for w.ws_row = N,
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
	 * 1. Starting from line L = buffer_head, traverse backwards through the
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
	int buffer_start = 3, buffer_end = w.ws_row - 2;
	int print_row = buffer_start;
	int max_row = buffer_end - buffer_start + 1;
	int count_row = 0;

	/* Insufficient rows for drawing */
	if (buffer_end < buffer_start)
		return;

	/* (#terminal columns) - strlen((widest nick in c)) - strlen(" HH:MM   ~ ") */
	int text_cols = w.ws_col - c->nick_pad - 11;

	/* Insufficient columns for drawing */
	if (text_cols < 1)
		goto clear_remainder;

	line *tmp, *l = c->buffer_head;

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

		while (count_row-- > max_row)
			word_wrap(text_cols, &ptr1, ptr2);

		do {
			printf(MOVE(%d, %d) CLEAR_LINE, print_row++, c->nick_pad + 10);
			printf(FG(239) "~" FG(250) " ");

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
		int from_fg;
		char *from_bg = "";

		if (l->type == LINE_JOIN || l->type == LINE_PART || l->type == LINE_QUIT)
			from_fg = 239;
		else if (l->type == LINE_PINGED)
			from_fg = 255, from_bg = BG(1);
		else
			from_fg = nick_col(l->from);

		printf(MOVE(%d, 1) CLEAR_LINE, print_row++);
		printf(FG(239) " %02d:%02d  %*s" FG(%d) "%s%s" BG_R FG(239) " ~ " FG(250),
				l->time_h, l->time_m,
				(int)(c->nick_pad - strlen(l->from)), "",
				from_fg, from_bg, l->from);

		char *ptr1 = l->text;
		char *ptr2 = l->text + l->len;

		char *print = ptr1;
		char *wrap = word_wrap(text_cols, &ptr1, ptr2);

		while (print < wrap)
			putchar(*print++);

		if (print_row > buffer_end)
			break;

		/* Draw any line continuations */
		while (*ptr1) {
			printf(MOVE(%d, %d) CLEAR_LINE, print_row++, c->nick_pad + 10);
			printf(FG(239) "~" FG(250) " ");

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
		if (width + len + 4 < w.ws_col) {

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
	if (confirm_message) {
		printf(MOVE(%d, 6) CLEAR_RIGHT FG(250) "%s", w.ws_row, confirm_message);
		return;
	}

	int winsz = w.ws_col / 3;

	input *in = c->input;

	/* Reframe the input bar window */
	if (in->head > (in->window + w.ws_col - 6))
		in->window += winsz;
	else if (in->head == in->window - 1)
		in->window = (in->window - winsz > in->line->text)
			? in->window - winsz : in->line->text;

	printf(MOVE(%d, 6) CLEAR_RIGHT FG(250), w.ws_row);

	char *p = in->window;
	while (p < in->head)
		putchar(*p++);

	p = in->tail;

	char *end = in->tail + w.ws_col - 5 - (in->head - in->window);

	while (p < end && p < in->line->text + MAX_INPUT)
		putchar(*p++);

	int col = (in->head - in->window);

	printf(MOVE(%d, %d), w.ws_row, col + 6);
}

/* TODO:
 *
 * Could use some cleaning up*/
/* Statusbar:
 *
 * server / private chat:
 * --[usermodes]---
 *
 * channel:
 * --[usermodes]--[chantype chanmodes chancount]---
 * */
static void
draw_status(channel *c)
{
	printf(CURSOR_SAVE);
	printf(MOVE(%d, 1) CLEAR_LINE FG(239), w.ws_row - 1);

	int i = 0, j, mode;
	char umode_str[] = UMODE_STR;
	char cmode_str[] = CMODE_STR;

	/* usermodes */
	if (c->server && (mode = c->server->usermode)) {
		i += printf("―[+") - 2;
		for (j = 0; j < UMODE_MAX; j++) {
			if (mode & (1 << j)) {
				putchar(umode_str[j]);
				i++;
			}
		}
		i += printf("]");
	}

	/* private chat */
	if (c->type == 'p') {
		i += printf("―[priv]") - 2;
	/* chantype, chanmodes, chancount */
	} else if (c->type) {
		i += printf("―[%c", c->type) - 2;

		if ((mode = c->chanmode)) {
			i += printf(" +");
			for (j = 0; j < CMODE_MAX; j++) {
				if (mode & (1 << j)) {
					putchar(cmode_str[j]);
					i++;
				}
			}
		}
		i += printf(" %d]", c->nick_count);
	}

	for (; i < w.ws_col; i++)
		printf("―");

	printf(CURSOR_RESTORE);
}

static char*
word_wrap(int text_cols, char **ptr1, char *ptr2)
{
	/* Greedy word wrap algorithm.
	 *
	 * Given a string bounded by [start, end), return a pointer to the
	 * character one past the maximum printable character for this string segment
	 * within text_cols wrapped on whitespace, and set ptr1 to the first character
	 * that is printable on the next line.
	 *
	 * This algorithm never discards whitespace at the beginning of lines, but
	 * does discard whitespace between line continuations and at end of lines.
	 *
	 * text_cols: the number of printable columns
	 * ptr1:      the first character in string
	 * ptr2:      the string's null terminator
	 */

	char *tmp, *ret = (*ptr1) + text_cols;

	assert(text_cols > 0);

	/* Entire line fits within text_cols */
	if (ret >= ptr2)
		return (*ptr1 = ptr2);

	/* At least one char exists that can print on current line */

	if (*ret == ' ') {

		/* Wrap on this space, find printable character for next line */
		for (tmp = ret; *tmp == ' '; tmp++)
			;

		*ptr1 = tmp;

	} else {

		/* Find a space to wrap on, or wrap on */
		for (tmp = (*ptr1) + 1; *ret != ' ' && ret > tmp; ret--)
			;

		/* No space found, wrap on entire segment */
		if (ret == tmp)
			return (*ptr1 = (*ptr1) + text_cols);

		*ptr1 = ret + 1;
	}

	return ret;
}

static int
count_line_rows(int text_cols, line *l)
{
	/* Count the number of times a line will wrap within text_cols columns */

	int count = 0;

	char *ptr1 = l->text;
	char *ptr2 = l->text + l->len;

	do {
		word_wrap(text_cols, &ptr1, ptr2);

		count++;
	} while (*ptr1);

	return count;
}

static int
nick_col(char *nick)
{
	int colour = 0;

	while (*nick)
		colour += *nick++;

	return nick_colours[colour % sizeof(nick_colours) / sizeof(nick_colours[0])];
}
