#include "src/draw.h"

#include "config.h"
#include "src/components/channel.h"
#include "src/components/input.h"
#include "src/io.h"
#include "src/state.h"
#include "src/utils/utils.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Control sequence initiator */
#define CSI "\x1b["

#define ATTR_BG(X)         CSI "48;5;"#X"m"
#define ATTR_FG(X)         CSI "38;5;"#X"m"
#define ATTR_RESET         CSI "0m"
#define ATTR_RESET_BG      CSI "49m"
#define ATTR_RESET_FG      CSI "39m"
#define CLEAR_FULL         CSI "2J"
#define CLEAR_LINE         CSI "2K"
#define CURSOR_POS(X, Y)   CSI #X";"#Y"H"
#define CURSOR_POS_RESTORE CSI "u"
#define CURSOR_POS_SAVE    CSI "s"

/* Minimum rows or columns to safely draw */
#define COLS_MIN 5
#define ROWS_MIN 5

#ifndef BUFFER_PADDING
#define BUFFER_PADDING 1
#endif

#define UTF8_CONT(C) (((unsigned char)(C) & 0xC0) == 0x80)

#define ATTR_CODE_BOLD      0x02
#define ATTR_CODE_COLOUR    0x03
#define ATTR_CODE_ITALIC    0x1D
#define ATTR_CODE_MONOSPACE 0x11
#define ATTR_CODE_RESET     0x0F
#define ATTR_CODE_REVERSE   0x16
#define ATTR_CODE_STRIKE    0x1E
#define ATTR_CODE_UNDERLINE 0x1F

/* https://modern.ircdocs.horse/formatting.html#colors
 * https://modern.ircdocs.horse/formatting.html#colors-16-98 */
static int irc_to_ansi_colour[] = {
	15,  0,   4,   2,   9,   1,   5,   3,   11,  10,
	6,   14,  12,  13,  8,   7,   52,  94,  100, 58,
	22,  29,  23,  24,  17,  54,  53,  89,  88,  130,
	142, 64,  28,  35,  30,  25,  18,  91,  90,  125,
	124, 166, 184, 106, 34,  49,  37,  33,  19,  129,
	127, 161, 196, 208, 226, 154, 46,  86,  51,  75,
	21,  171, 201, 198, 203, 215, 227, 191, 83,  122,
	87,  111, 63,  177, 207, 205, 217, 223, 229, 193,
	157, 158, 159, 153, 147, 183, 219, 212, 16,  233,
	235, 237, 239, 241, 244, 247, 250, 254, 231, -1,
};

/* Terminal coordinate row/column boundaries (inclusive)
 * for objects being drawn. The origin for terminal
 * coordinates is in the top left, indexed from 1
 *
 *   \ c1     cN
 *    +---------+
 *  r1|         |
 *    |         |
 *    |         |
 *  rN|         |
 *    +---------+
 */

struct coords
{
	unsigned c1;
	unsigned cN;
	unsigned r1;
	unsigned rN;
};

static struct
{
	union {
		struct {
			unsigned separators  : 1;
			unsigned buffer      : 1;
			unsigned input       : 1;
			unsigned nav         : 1;
			unsigned status      : 1;
		};
		unsigned all;
	} bits;
	unsigned bell : 1;
	unsigned scroll_buffer_back : 1;
	unsigned scroll_buffer_forw : 1;
} draw_state;

static struct coords coords(unsigned, unsigned, unsigned, unsigned);
static unsigned nick_col(char*);
static unsigned drawf(unsigned*, const char*, ...);

static const char* draw_buffer_scrollback_status(struct buffer*, char*, size_t);
static size_t draw_buffer_wrap(const char*, size_t, size_t);
static unsigned draw_buffer_line_rows(struct buffer_line*, unsigned);
static void draw_bits(void);
static void draw_buffer(struct buffer*, struct coords);
static void draw_buffer_line(struct buffer_line*, struct coords, unsigned, unsigned, unsigned, unsigned);
static void draw_buffer_line_split(struct buffer_line*, unsigned*, unsigned*, unsigned, unsigned);
static void draw_buffer_scroll_back(void);
static void draw_buffer_scroll_forw(void);
static void draw_input(struct input*, struct coords);
static void draw_nav(struct channel*);
static void draw_separators(void);
static void draw_status(struct channel*);

static size_t draw_attr_len(const char *str);
static void draw_attr_bg(int);
static void draw_attr_fg(int);
static void draw_attr_reset(void);
static void draw_char(int);
static void draw_clear_full(void);
static void draw_clear_line(void);
static void draw_cursor_pos(int, int);
static void draw_cursor_pos_restore(void);
static void draw_cursor_pos_save(void);
static unsigned draw_irc_colour(const char *code, int *fg, int *bg);

static int actv_colours[ACTIVITY_T_SIZE] = ACTIVITY_COLOURS
static int bg_last = -1;
static int fg_last = -1;
static int nick_colours[] = NICK_COLOURS
static int drawing;

void
draw_init(void)
{
	drawing = 1;
}

void
draw_term(void)
{
	drawing = 0;

	draw(DRAW_CLEAR);
}

void
draw(enum draw_bit bit)
{
	switch (bit) {
		case DRAW_FLUSH:
			draw_bits();
			draw_state.bits.all = 0;
			draw_state.scroll_buffer_back = 0;
			draw_state.scroll_buffer_forw = 0;
			draw_state.bell = 0;
			break;
		case DRAW_BELL:
			draw_state.bell = 1;
			break;
		case DRAW_BUFFER:
			draw_state.bits.buffer = 1;
			break;
		case DRAW_BUFFER_BACK:
			draw_state.scroll_buffer_back = 1;
			draw_state.scroll_buffer_forw = 0;
			break;
		case DRAW_BUFFER_FORW:
			draw_state.scroll_buffer_forw = 1;
			draw_state.scroll_buffer_back = 0;
			break;
		case DRAW_INPUT:
			draw_state.bits.input = 1;
			break;
		case DRAW_NAV:
			draw_state.bits.nav = 1;
			break;
		case DRAW_STATUS:
			draw_state.bits.status = 1;
			break;
		case DRAW_ALL:
			draw_state.bits.all = -1;
			break;
		case DRAW_CLEAR:
			draw_attr_reset();
			draw_clear_full();
			break;
		default:
			fatal("unknown draw bit");
	}
}

static void
draw_bits(void)
{
	if (!drawing)
		return;

	if (draw_state.bell && BELL_ON_PINGED)
		putchar('\a');

	if (!draw_state.bits.all)
		return;

	struct channel *c = current_channel();

	unsigned cols = state_cols();
	unsigned rows = state_rows();

	draw_cursor_pos_save();

	if (cols < COLS_MIN || rows < ROWS_MIN) {
		draw_clear_full();
		draw_cursor_pos(1, 1);
		goto flush;
	}

	/* handle state altering draw functions before drawing components */

	if (draw_state.scroll_buffer_back)
		draw_buffer_scroll_back();

	if (draw_state.scroll_buffer_forw)
		draw_buffer_scroll_forw();

	/* draw components */

	if (draw_state.bits.separators) {
		draw_attr_reset();
		draw_separators();
	}

	if (draw_state.bits.buffer) {
		draw_attr_reset();
		draw_buffer(&c->buffer, coords(1, cols, 3, rows - 2));
	}

	if (draw_state.bits.input) {
		draw_attr_reset();
		draw_input(&c->input, coords(1, cols, rows, rows));
	}

	if (draw_state.bits.nav) {
		draw_attr_reset();
		draw_nav(c);
	}

	if (draw_state.bits.status) {
		draw_attr_reset();
		draw_status(c);
	}

flush:

	draw_attr_reset();
	draw_cursor_pos_restore();

	fflush(stdout);
}

static const char*
draw_buffer_scrollback_status(struct buffer *b, char *buf, size_t n)
{
	/* Format scrollback status to buffer as unsigned value [0, 100]
	 *
	 * Calculated as perentage of lines 'below' the currently drawn buffer
	 * lines over the total of currently undrawn buffer lines, i.e.:
	 *
	 *  tail  +----------+
	 *        |    n2    |
	 *        +----------+ buffer_i_top
	 *        |          |
	 *  drawn |          |
	 *        |          |
	 *        +----------+ buffer_i_bot
	 *        |    n1    |
	 *  head  +----------+
	 */

	if (buffer_line(b, b->scrollback) == buffer_head(b))
		return NULL;

	float n1 = (b->head - b->buffer_i_bot) - 1;
	float n2 = (b->buffer_i_top - b->tail);

	if (!n1 && !n2)
		return NULL;

	(void) snprintf(buf, n, "%u", (unsigned)(100 * (n1 / (n1 + n2))));

	return buf;
}

static size_t
draw_buffer_wrap(const char *str, size_t len, size_t cols)
{
	size_t i = 0;
	size_t ret;
	size_t w = 0;

	if (!cols)
		return 0;

	if (len <= cols)
		return len;

	while ((ret = draw_attr_len((str + i))))
		i += ret;

	while (cols && str[i]) {

		if (str[i] == ' ') {
			while (cols && str[i]) {
				if ((ret = draw_attr_len((str + i))))
					i += ret;
				else if (str[i] == ' ')
					i++, cols--;
				else if (str[i] != ' ')
					break;
			}
		} else {
			while (cols && str[i]) {
				if ((ret = draw_attr_len((str + i))))
					i += ret;
				else if (str[i] != ' ')
					i++, cols--;
				else if (str[i] == ' ')
					break;
			}
		}

		if (cols && str[i] && str[i] != ' ')
			w = i;
	}

	return ((str[i] && w) ? w : i);
}

static unsigned
draw_buffer_line_rows(struct buffer_line *line, unsigned cols)
{
	/* Return the number of times a buffer line will wrap within `cols` columns */

	if (cols == 0)
		fatal("cols is zero");

	/* Empty lines are considered to occupy a row */
	if (!*line->text)
		return line->cached.rows = 1;

	if (line->cached.cols != cols) {

		line->cached.cols = cols;
		line->cached.rows = 0;

		const char *p = line->text;
		size_t len = line->text_len;
		size_t ret;

		while (len) {
			ret = draw_buffer_wrap(p, len, cols);
			len -= ret;
			p += ret;
			line->cached.rows++;
		}
	}

	return line->cached.rows;
}

static void
draw_buffer(struct buffer *b, struct coords coords)
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
	 * 3. Traverse forward through the buffer, drawing lines until buffer.head
	 *    is encountered
	 */

	unsigned buffer_i = b->scrollback;
	unsigned cols_head;
	unsigned cols_text;
	unsigned cols_total = coords.cN - coords.c1 + 1;
	unsigned row;
	unsigned row_count = 0;
	unsigned row_total = coords.rN - coords.r1 + 1;

	/* Clear the buffer area */
	for (row = coords.r1; row <= coords.rN; row++) {
		draw_cursor_pos(row, 1);
		draw_clear_line();
	}

	struct buffer_line *line = buffer_line(b, buffer_i);

	if (line == NULL)
		return;

	struct buffer_line *tail = buffer_tail(b);
	struct buffer_line *head = buffer_head(b);

	/* Find top line */
	for (;;) {

		draw_buffer_line_split(line, NULL, &cols_text, cols_total, b->pad);

		row_count += draw_buffer_line_rows(line, cols_text);

		if (line == tail)
			break;

		if (row_count >= row_total)
			break;

		line = buffer_line(b, --buffer_i);
	}

	b->buffer_i_top = buffer_i;

	/* Handle impartial top line print */
	if (row_count > row_total) {

		draw_buffer_line_split(line, &cols_head, &cols_text, cols_total, b->pad);

		draw_buffer_line(
			line,
			coords,
			cols_head,
			cols_text,
			row_count - row_total,
			BUFFER_PADDING ? (b->pad - line->from_len) : 0
		);

		coords.r1 += draw_buffer_line_rows(line, cols_text) - (row_count - row_total);

		if (line == head) {
			b->buffer_i_bot = buffer_i;
			return;
		}

		line = buffer_line(b, ++buffer_i);
	}

	/* Draw all remaining lines */
	while (coords.r1 <= coords.rN) {

		draw_buffer_line_split(line, &cols_head, &cols_text, cols_total, b->pad);

		draw_buffer_line(
			line,
			coords,
			cols_head,
			cols_text,
			0,
			BUFFER_PADDING ? (b->pad - line->from_len) : 0
		);

		coords.r1 += draw_buffer_line_rows(line, cols_text);

		if (line == head) {
			b->buffer_i_bot = buffer_i;
			return;
		}

		line = buffer_line(b, ++buffer_i);
	}

	b->buffer_i_bot = (buffer_i - 1);
}

static void
draw_buffer_line(
		struct buffer_line *line,
		struct coords coords,
		unsigned cols_head,
		unsigned cols_text,
		unsigned skip,
		unsigned pad)
{
	char *p1 = line->text;
	size_t ret;
	unsigned head_col = coords.c1;
	unsigned text_bg = BUFFER_TEXT_BG;
	unsigned text_col = coords.c1 + cols_head;
	unsigned text_fg = BUFFER_TEXT_FG;

	if (!line->cached.initialized) {
		/* Initialize static cached properties of drawn lines */
		line->cached.colour = nick_col(line->from);
		line->cached.initialized = 1;
	}

	if (!skip) {

		char buf_h[3] = {0};
		char buf_m[3] = {0};
		int from_bg;
		int from_fg;
		unsigned head_cols = cols_head;

		struct tm *tm = localtime(&line->time);

		(void) snprintf(buf_h, sizeof(buf_h), "%02d", tm->tm_hour);
		(void) snprintf(buf_m, sizeof(buf_h), "%02d", tm->tm_min);

		draw_cursor_pos(coords.r1, head_col);

		if (!drawf(&head_cols, " %b%f%s:%s%a ",
				BUFFER_LINE_HEADER_BG,
				BUFFER_LINE_HEADER_FG,
				buf_h,
				buf_m))
			goto print_text;

		while (pad--) {
			if (!drawf(&head_cols, "%s", " "))
				goto print_text;
		}

		switch (line->type) {
			case BUFFER_LINE_CHAT:
				from_bg = BUFFER_LINE_HEADER_BG;
				from_fg = line->cached.colour;
				break;
			case BUFFER_LINE_PINGED:
				from_bg = BUFFER_LINE_HEADER_BG_PINGED;
				from_fg = BUFFER_LINE_HEADER_FG_PINGED;
				break;
			default:
				from_bg = BUFFER_LINE_HEADER_BG;
				from_fg = BUFFER_LINE_HEADER_FG;
				break;
		}

		if (!drawf(&head_cols, "%b%f%s%a %b%f~%a ",
				from_bg,
				from_fg,
				line->from,
				BUFFER_LINE_HEADER_BG,
				BUFFER_LINE_HEADER_FG))
			goto print_text;
	}

print_text:

	while (skip--)
		p1 += draw_buffer_wrap(p1, (line->text_len - (p1 - line->text)), cols_text);

	if (strlen(QUOTE_LEADER) && line->type == BUFFER_LINE_CHAT) {
		if (!strncmp(line->text, QUOTE_LEADER, strlen(QUOTE_LEADER))) {
			text_bg = QUOTE_TEXT_BG;
			text_fg = QUOTE_TEXT_FG;
		}
	}

	do {
		unsigned text_cols = cols_text;

		draw_cursor_pos(coords.r1, text_col);

		if (*p1) {

			ret = draw_buffer_wrap(p1, strlen(p1), text_cols);

			draw_attr_bg(text_bg);
			draw_attr_fg(text_fg);

			while (ret--) {

				size_t attr_len;

				while ((attr_len = draw_attr_len(p1)))
					p1 += attr_len;

				draw_char(*p1++);
			}

			draw_attr_reset();
		}

		coords.r1++;

	} while (*p1 && coords.r1 <= coords.rN);
}

static void
draw_buffer_line_split(
	struct buffer_line *line,
	unsigned *cols_head,
	unsigned *cols_text,
	unsigned cols,
	unsigned pad)
{
	unsigned _cols_head = sizeof(" HH:MM  ~ ");

	if (BUFFER_PADDING)
		_cols_head += pad;
	else
		_cols_head += line->from_len;

	/* If header won't fit, split in half */
	if (_cols_head >= cols)
		_cols_head = cols / 2;

	_cols_head -= 1;

	if (cols_head)
		*cols_head = _cols_head;

	if (cols_text)
		*cols_text = cols - _cols_head;
}

static void
draw_buffer_scroll_back(void)
{
	/* Scroll the current buffer back one page */

	struct buffer *b = &(current_channel()->buffer);

	unsigned buffer_i = b->scrollback;
	unsigned count = 0;
	unsigned cols_text = 0;
	unsigned cols = state_cols();
	unsigned rows = state_rows() - 4;

	struct buffer_line *line = buffer_line(b, buffer_i);

	/* Skip redraw */
	if (line == buffer_tail(b))
		return;

	/* Find top line */
	for (;;) {

		draw_buffer_line_split(line, NULL, &cols_text, cols, b->pad);

		count += draw_buffer_line_rows(line, cols_text);

		if (count >= rows)
			break;

		if (line == buffer_tail(b))
			return;

		line = buffer_line(b, --buffer_i);
	}

	b->scrollback = buffer_i;

	if (count == rows && line != buffer_tail(b))
		b->scrollback--;
}

static void
draw_buffer_scroll_forw(void)
{
	/* Scroll the current buffer forward one page */

	struct buffer *b = &(current_channel()->buffer);

	unsigned count = 0;
	unsigned cols_text = 0;
	unsigned cols = state_cols();
	unsigned rows = state_rows() - 4;

	if (b->buffer_i_top == b->tail && b->buffer_i_bot != (b->head - 1))
		b->scrollback = b->buffer_i_bot;

	struct buffer_line *line = buffer_line(b, b->scrollback);

	/* Skip redraw */
	if (line == buffer_head(b))
		return;

	/* Find top line */
	for (;;) {

		draw_buffer_line_split(line, NULL, &cols_text, cols, b->pad);

		count += draw_buffer_line_rows(line, cols_text);

		if (line == buffer_head(b))
			break;

		if (count >= rows)
			break;

		line = buffer_line(b, ++b->scrollback);
	}

	/* Bottom line in view draws in full; scroll forward one additional line */
	if (count == rows && line != buffer_head(b))
		b->scrollback++;
}

static void
draw_separators(void)
{
	unsigned cols = state_cols();

	draw_cursor_pos(2, 1);

	draw_attr_bg(SEP_BG);
	draw_attr_fg(SEP_FG);

	while (drawf(&cols, "%s", SEP_HORZ))
		;
}

static void
draw_input(struct input *inp, struct coords coords)
{
	/* Draw the input line, or the current action message */

	const char *action;
	unsigned cols = coords.cN - coords.c1 + 1;
	unsigned cursor_row = coords.r1;
	unsigned cursor_col = coords.cN;

	draw_cursor_pos(coords.r1, coords.c1);

	if ((action = action_message())) {
		if (!drawf(&cols, "%b%f%s%b%f-- %s --",
				INPUT_PREFIX_BG,
				INPUT_PREFIX_FG,
				INPUT_PREFIX,
				ACTION_BG,
				ACTION_FG,
				action))
			goto cursor;

		cursor_col = coords.cN - coords.c1 - cols + 3;
	} else {
		char input[INPUT_LEN_MAX];
		unsigned cursor_pre;
		unsigned cursor_inp;

		if (!drawf(&cols, "%b%f%s",
				INPUT_PREFIX_BG,
				INPUT_PREFIX_FG,
				INPUT_PREFIX))
			goto cursor;

		cursor_pre = coords.cN - coords.c1 - cols + 1;
		cursor_inp = input_frame(inp, input, cols);

		if (!drawf(&cols, "%b%f%s",
				INPUT_BG,
				INPUT_FG,
				input))
			goto cursor;

		cursor_col = cursor_pre + cursor_inp + 1;
	}

	draw_attr_reset();

	while (cols--)
		draw_char(' ');

cursor:

	cursor_row = MIN(cursor_row, coords.rN);
	cursor_col = MIN(cursor_col, coords.cN);

	draw_cursor_pos(cursor_row, cursor_col);
	draw_cursor_pos_save();
}

static void
draw_nav(struct channel *c)
{
	/* Dynamically draw the nav such that:
	 *
	 *  - The current channel is kept framed while navigating
	 *  - Channels are coloured based on their current activity
	 *  - The nav is kept framed between the first and last channels
	 */

	draw_cursor_pos(1, 1);
	draw_clear_line();

	static struct channel *frame_prev;
	static struct channel *frame_next;

	struct channel *c_first = channel_get_first();
	struct channel *c_last = channel_get_last();
	struct channel *tmp;
	struct channel *tmp_next = c;
	struct channel *tmp_prev = c;

	unsigned cols = state_cols();

	/* By default assume drawing starts towards the next channel */
	int nextward = 1;

	size_t len;
	size_t len_total = 0;

	c->activity = ACTIVITY_DEFAULT;

	/* Bump the channel frames, if applicable */
	if ((len_total = (c->name_len + 2)) >= state_cols())
		return;
	else if (c == frame_prev && frame_prev != c_first)
		frame_prev = channel_get_prev(frame_prev);
	else if (c == frame_next && frame_next != c_last)
		frame_next = channel_get_next(frame_next);

	/* Calculate the new frames */
	for (;;) {

		if (tmp_prev == c_first || tmp_prev == frame_prev) {

			/* Pad from the next-most frame towards the last channel */
			tmp = channel_get_next(tmp_next);
			len = tmp->name_len;

			while ((len_total += (len + 2)) < state_cols() && tmp != c_first) {
				tmp_next = tmp;
				tmp = channel_get_next(tmp);
				len = tmp->name_len;
			}

			/* Pad from the prev-most frame towards the first channel */
			tmp = channel_get_prev(tmp_prev);
			len = tmp->name_len;

			while ((len_total += (len + 2)) < state_cols() && tmp != c_last) {
				tmp_prev = tmp;
				tmp = channel_get_prev(tmp);
				len = tmp->name_len;
			}

			break;
		}

		if (tmp_next == c_last || tmp_next == frame_next) {

			/* Pad from the prev-most frame towards the first channel */
			tmp = channel_get_prev(tmp_prev);
			len = tmp->name_len;

			while ((len_total += (len + 2)) < state_cols() && tmp != c_last) {
				tmp_prev = tmp;
				tmp = channel_get_prev(tmp);
				len = tmp->name_len;
			}

			/* Pad from the next-most frame towards the last channel */
			tmp = channel_get_next(tmp_next);
			len = tmp->name_len;

			while ((len_total += (len + 2)) < state_cols() && tmp != c_first) {
				tmp_next = tmp;
				tmp = channel_get_next(tmp);
				len = tmp->name_len;
			}

			break;
		}

		tmp = nextward ? channel_get_next(tmp_next) : channel_get_prev(tmp_prev);
		len = tmp->name_len;

		/* Next channel doesn't fit */
		if ((len_total += (len + 2)) >= state_cols())
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
	for (tmp = frame_prev; ; tmp = channel_get_next(tmp)) {

		int fg = (tmp == c) ? NAV_CURRENT_CHAN : actv_colours[tmp->activity];

		if (!drawf(&cols, "%f %s ", fg, tmp->name))
			break;

		if (tmp == frame_next)
			break;
	}
}

static void
draw_status(struct channel *c)
{
	/* server buffer:
	 *  -[nick +usermodes]-(ping)-(scrollback)
	 *
	 * privmsg buffer:
	 *  -[nick +usermodes]-[privmsg]-(ping)-(scrollback)
	 *
	 * channel buffer:
	 *  -[nick +usermodes]-[+chanmodes chancount]-(ping)-(scrollback)
	 */

	#define STATUS_SEP_HORZ \
		"%b%f" SEP_HORZ "%b%f", SEP_BG, SEP_FG, STATUS_BG, STATUS_FG

	unsigned cols = state_cols();
	unsigned rows = state_rows();
	char scrollback[4];

	if (!cols || !(rows > 1))
		return;

	draw_cursor_pos(rows - 1, 1);

	/* -[nick +usermodes] */
	if (c->server && c->server->registered) {
		if (!drawf(&cols, STATUS_SEP_HORZ))
			return;
		if (!drawf(&cols, "[%s%s%s]",
				c->server->nick,
				(*(c->server->mode_str.str) ? " +" : ""),
				(*(c->server->mode_str.str) ? c->server->mode_str.str : "")))
			return;
	}

	/* -[privmsg] */
	if (c->type == CHANNEL_T_PRIVMSG) {
		if (!drawf(&cols, STATUS_SEP_HORZ))
			return;
		if (!drawf(&cols, "[privmsg]"))
			return;
	}

	/* -[+chanmodes chancount] */
	if (c->type == CHANNEL_T_CHANNEL && c->joined) {
		if (!drawf(&cols, STATUS_SEP_HORZ))
			return;
		if (!drawf(&cols, "[%s%s%s%u]",
				(*(c->chanmodes_str.str) ? "+" : ""),
				(*(c->chanmodes_str.str) ? c->chanmodes_str.str : ""),
				(*(c->chanmodes_str.str) ? " " : ""),
				 c->users.count))
			return;
	}

	/* -(ping) */
	if (c->server && c->server->ping) {
		if (!drawf(&cols, STATUS_SEP_HORZ))
			return;
		if (!drawf(&cols, "(%us)", c->server->ping))
			return;
	}

	/* -(scrollback) */
	if ((draw_buffer_scrollback_status(&c->buffer, scrollback, sizeof(scrollback)))) {
		if (!drawf(&cols, STATUS_SEP_HORZ))
			return;
		if (!drawf(&cols, "(%s%s)", scrollback, "%"))
			return;
	}

	draw_attr_bg(SEP_BG);
	draw_attr_fg(SEP_FG);

	while (drawf(&cols, "%s", SEP_HORZ))
		;
}

static struct coords
coords(unsigned c1, unsigned cN, unsigned r1, unsigned rN)
{
	unsigned cols = state_cols();
	unsigned rows = state_rows();

	if (!c1 || c1 > cN || cN > cols)
		fatal("Invalid coordinates: cols: %u %u %u", cols, c1, cN);

	if (!r1 || r1 > rN || rN > rows)
		fatal("Invalid coordinates: rows: %u %u %u", rows, r1, rN);

	return (struct coords) { .c1 = c1, .cN = cN, .r1 = r1, .rN = rN };
}

static unsigned
nick_col(char *nick)
{
	unsigned colour = 0;

	while (*nick)
		colour += *nick++;

	return nick_colours[colour % sizeof(nick_colours) / sizeof(nick_colours[0])];
}

static unsigned
drawf(unsigned *cols_p, const char *fmt, ...)
{
	/* Draw formatted text up to a given number of
	 * columns. Returns number of unused columns.
	 *
	 *  %a -- attribute reset
	 *  %b -- set background colour attribute
	 *  %f -- set foreground colour attribute
	 *  %d -- output signed integer
	 *  %u -- output unsigned integer
	 *  %s -- output string
	 */

	char buf[64];
	char c;
	va_list arg;
	unsigned cols;

	if (!(cols = *cols_p))
		return 0;

	va_start(arg, fmt);

	while (cols && (c = *fmt++)) {
		if (c == '%') {
			switch ((c = *fmt++)) {
				case 'a':
					draw_attr_reset();
					break;
				case 'b':
					draw_attr_bg(va_arg(arg, int));
					break;
				case 'f':
					draw_attr_fg(va_arg(arg, int));
					break;
				case 'd':
					(void) snprintf(buf, sizeof(buf), "%d", va_arg(arg, int));
					cols -= (unsigned) printf("%.*s", cols, buf);
					break;
				case 'u':
					(void) snprintf(buf, sizeof(buf), "%u", va_arg(arg, unsigned));
					cols -= (unsigned) printf("%.*s", cols, buf);
					break;
				case 's':
					for (const char *str = va_arg(arg, const char*); *str && cols; cols--) {
						do {
							draw_char(*str++);
						} while (UTF8_CONT(*str));
					}
					break;
				default:
					fatal("unknown drawf format character '%c'", c);
			}
		} else {
			cols--;
			draw_char(c);
			while (UTF8_CONT(*fmt))
				draw_char(*fmt++);
		}
	}

	va_end(arg);

	return (*cols_p = cols);
}

static size_t
draw_attr_len(const char *str)
{
	switch (*str) {
		case ATTR_CODE_COLOUR:
			return draw_irc_colour(str, NULL, NULL);
		case ATTR_CODE_BOLD:
		case ATTR_CODE_ITALIC:
		case ATTR_CODE_MONOSPACE:
		case ATTR_CODE_RESET:
		case ATTR_CODE_REVERSE:
		case ATTR_CODE_STRIKE:
		case ATTR_CODE_UNDERLINE:
			return 1;
		default:
			return 0;
	}
}

static void
draw_attr_bg(int bg)
{
	if (bg == -1)
		printf(ATTR_RESET_BG);

	if (bg >= 0 && bg <= 255)
		printf(ATTR_BG(%d), bg);

	bg_last = bg;
}

static void
draw_attr_fg(int fg)
{
	if (fg == -1)
		printf(ATTR_RESET_FG);

	if (fg >= 0 && fg <= 255)
		printf(ATTR_FG(%d), fg);

	fg_last = fg;
}

static void
draw_attr_reset(void)
{
	printf(ATTR_RESET);
}

static void
draw_clear_full(void)
{
	printf(CLEAR_FULL);
}

static void
draw_clear_line(void)
{
	printf(CLEAR_LINE);
}

static void
draw_char(int c)
{
	if (iscntrl(c)) {
		int ctrl_bg_last = bg_last;
		int ctrl_fg_last = fg_last;
		draw_attr_bg(CTRL_BG);
		draw_attr_fg(CTRL_FG);
		putchar((c | 0x40));
		draw_attr_bg(ctrl_bg_last);
		draw_attr_fg(ctrl_fg_last);
	} else {
		putchar(c);
	}
}

static void
draw_cursor_pos(int row, int col)
{
	printf(CURSOR_POS(%d, %d), row, col);
}

static void
draw_cursor_pos_restore(void)
{
	printf(CURSOR_POS_RESTORE);
}

static void
draw_cursor_pos_save(void)
{
	printf(CURSOR_POS_SAVE);
}

static unsigned
draw_irc_colour(const char *code, int *fg, int *bg)
{
	char c;
	int comma = 0;
	int digits_bg = 0;
	int digits_fg = 0;
	int parsed_fg = 0;
	int parsed_bg = 0;

	if (*code++ != 0x03)
		return 0;

	while ((c = *code++)) {

		if (isdigit(c)) {
			if (comma) {
				if (digits_bg >= 2)
					break;
				digits_bg++;
				parsed_bg *= 10;
				parsed_bg += (c - '0');
			} else {
				if (digits_fg >= 2)
					break;
				digits_fg++;
				parsed_fg *= 10;
				parsed_fg += (c - '0');
			}
		} else if (c == ',') {
			if (comma++)
				break;
		} else {
			break;
		}
	}

	if (fg && !digits_fg && !digits_bg)
		*fg = -1;

	if (bg && !digits_fg && !digits_bg)
		*bg = -1;

	if (fg && digits_fg)
		*fg = irc_to_ansi_colour[parsed_fg];

	if (bg && digits_bg)
		*bg = irc_to_ansi_colour[parsed_bg];

	return (1 + digits_fg + digits_bg + (comma && digits_bg));
}
