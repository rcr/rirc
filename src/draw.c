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

#define ATTR_FG(X)    CSI "38;5;"#X"m"
#define ATTR_BG(X)    CSI "48;5;"#X"m"
#define ATTR_RESET    CSI "0m"
#define ATTR_RESET_FG CSI "39m"
#define ATTR_RESET_BG CSI "49m"

#define CLEAR_FULL    CSI "2J"
#define CLEAR_LINE    CSI "2K"

#define C_MOVE(X, Y)  CSI ""#X";"#Y"H"
#define C_SAVE        CSI "s"
#define C_RESTORE     CSI "u"

/* Minimum rows or columns to safely draw */
#define COLS_MIN 5
#define ROWS_MIN 5

/* Size of a full colour string for purposes of pre-formating text to print */
#define COLOUR_SIZE sizeof(ATTR_RESET ATTR_FG(255) ATTR_BG(255))

#ifndef BUFFER_PADDING
#define BUFFER_PADDING 1
#endif

#define UTF8_CONT(C) (((unsigned char)(C) & 0xC0) == 0x80)

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
	unsigned c1, cN;
	unsigned r1, rN;
};

struct draw_state
{
	union {
		struct {
			unsigned separators : 1;
			unsigned buffer     : 1;
			unsigned input      : 1;
			unsigned nav        : 1;
			unsigned status     : 1;
		};
		unsigned all;
	} bits;
	unsigned bell : 1;
};

static void draw_bits(void);
static void draw_buffer(struct buffer*, struct coords);
static void draw_buffer_line(struct buffer_line*, struct coords, unsigned, unsigned, unsigned, unsigned);
static void draw_separators(void);
static void draw_input(struct input*, struct coords);
static void draw_nav(struct channel*);
static void draw_status(struct channel*);

static char* draw_colour(int, int);
static int draw_fmt(char**, size_t*, size_t*, int, const char*, ...);
static unsigned nick_col(char*);
static void check_coords(struct coords);

static int actv_colours[ACTIVITY_T_SIZE] = ACTIVITY_COLOURS
static int nick_colours[] = NICK_COLOURS
static struct draw_state draw_state;

static unsigned drawf(unsigned, const char*, ...);
static void draw_bg(int);
static void draw_fg(int);
static void draw_char(int);

static int bg_last = -1;
static int fg_last = -1;

void
draw(enum draw_bit bit)
{
	switch (bit) {
		case DRAW_FLUSH:
			draw_bits();
			draw_state.bits.all = 0;
			draw_state.bell = 0;
			break;
		case DRAW_BELL:
			draw_state.bell = 1;
			break;
		case DRAW_BUFFER:
			draw_state.bits.buffer = 1;
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
			printf(ATTR_RESET);
			printf(CLEAR_FULL);
			break;
		default:
			fatal("unknown draw bit");
	}
}

static void
draw_bits(void)
{
	if (draw_state.bell && BELL_ON_PINGED)
		putchar('\a');

	if (!draw_state.bits.all)
		return;

	struct coords coords;
	struct channel *c = current_channel();

	if (state_cols() < COLS_MIN || state_rows() < ROWS_MIN) {
		printf(CLEAR_FULL C_MOVE(1, 1) "rirc");
		fflush(stdout);
		return;
	}

	printf(C_SAVE);

	if (draw_state.bits.separators) {
		printf(ATTR_RESET);
		draw_separators();
	}

	if (draw_state.bits.buffer) {
		printf(ATTR_RESET);
		coords.c1 = 1;
		coords.cN = state_cols();
		coords.r1 = 3;
		coords.rN = state_rows() - 2;
		draw_buffer(&c->buffer, coords);
	}

	if (draw_state.bits.input) {
		printf(ATTR_RESET);
		coords.c1 = 1;
		coords.cN = state_cols();
		coords.r1 = state_rows();
		coords.rN = state_rows();
		draw_input(&c->input, coords);
	}

	if (draw_state.bits.nav) {
		printf(ATTR_RESET);
		draw_nav(c);
	}

	if (draw_state.bits.status) {
		printf(ATTR_RESET);
		draw_status(c);
	}

	printf(ATTR_RESET);
	printf(C_RESTORE);

	fflush(stdout);
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

	check_coords(coords);

	unsigned row,
	         row_count = 0,
	         row_total = coords.rN - coords.r1 + 1;

	unsigned col_total = coords.cN - coords.c1 + 1;

	unsigned buffer_i = b->scrollback,
	         head_w,
	         text_w;

	/* Clear the buffer area */
	for (row = coords.r1; row <= coords.rN; row++)
		printf(C_MOVE(%d, 1) CLEAR_LINE, row);

	struct buffer_line *line = buffer_line(b, buffer_i);

	if (line == NULL)
		return;

	struct buffer_line *tail = buffer_tail(b);
	struct buffer_line *head = buffer_head(b);

	/* Find top line */
	for (;;) {

		buffer_line_split(line, NULL, &text_w, col_total, b->pad);

		row_count += buffer_line_rows(line, text_w);

		if (line == tail)
			break;

		if (row_count >= row_total)
			break;

		line = buffer_line(b, --buffer_i);
	}

	/* Handle impartial top line print */
	if (row_count > row_total) {

		buffer_line_split(line, &head_w, &text_w, col_total, b->pad);

		draw_buffer_line(
			line,
			coords,
			head_w,
			text_w,
			row_count - row_total,
			BUFFER_PADDING ? (b->pad - line->from_len) : 0
		);

		coords.r1 += buffer_line_rows(line, text_w) - (row_count - row_total);

		if (line == head)
			return;

		line = buffer_line(b, ++buffer_i);
	}

	/* Draw all remaining lines */
	while (coords.r1 <= coords.rN) {

		buffer_line_split(line, &head_w, &text_w, col_total, b->pad);

		draw_buffer_line(
			line,
			coords,
			head_w,
			text_w,
			0,
			BUFFER_PADDING ? (b->pad - line->from_len) : 0
		);

		coords.r1 += buffer_line_rows(line, text_w);

		if (line == head)
			return;

		line = buffer_line(b, ++buffer_i);
	}
}

static void
draw_buffer_line(
		struct buffer_line *line,
		struct coords coords,
		unsigned head_w,
		unsigned text_w,
		unsigned skip,
		unsigned pad)
{
	check_coords(coords);

	char *p1 = line->text;
	char *p2 = line->text + line->text_len;

	if (!line->cached.initialized) {
		/* Initialize static cached properties of drawn lines */
		line->cached.colour = nick_col(line->from);
		line->cached.initialized = 1;
	}

	if (skip == 0) {

		/* Print the line header
		 *
		 * Since formatting codes don't occupy columns, enough space
		 * should be allocated for all such sequences
		 * */
		char header[head_w + COLOUR_SIZE * 4 + 1];
		char *header_ptr = header;

		size_t buff_n = sizeof(header) - 1,
		       text_n = head_w - 1;

		struct tm *line_tm = localtime(&line->time);

		if (!draw_fmt(&header_ptr, &buff_n, &text_n, 0,
				draw_colour(BUFFER_LINE_HEADER_FG_NEUTRAL, -1)))
			goto print_header;

		if (!draw_fmt(&header_ptr, &buff_n, &text_n, 1,
				" %02d:%02d ", line_tm->tm_hour, line_tm->tm_min))
			goto print_header;

		if (!draw_fmt(&header_ptr, &buff_n, &text_n, 1,
				"%*s", pad, ""))
			goto print_header;

		if (!draw_fmt(&header_ptr, &buff_n, &text_n, 0, ATTR_RESET))
			goto print_header;

		switch (line->type) {
			case BUFFER_LINE_OTHER:
			case BUFFER_LINE_SERVER_INFO:
			case BUFFER_LINE_SERVER_ERROR:
			case BUFFER_LINE_JOIN:
			case BUFFER_LINE_NICK:
			case BUFFER_LINE_PART:
			case BUFFER_LINE_QUIT:
				if (!draw_fmt(&header_ptr, &buff_n, &text_n, 0,
						draw_colour(BUFFER_LINE_HEADER_FG_NEUTRAL, -1)))
					goto print_header;
				break;

			case BUFFER_LINE_CHAT:
				if (!draw_fmt(&header_ptr, &buff_n, &text_n, 0,
						draw_colour(line->cached.colour, -1)))
					goto print_header;
				break;

			case BUFFER_LINE_PINGED:
				if (!draw_fmt(&header_ptr, &buff_n, &text_n, 0,
						draw_colour(BUFFER_LINE_HEADER_FG_PINGED, BUFFER_LINE_HEADER_BG_PINGED)))
					goto print_header;
				break;

			case BUFFER_LINE_T_SIZE:
				fatal("Invalid line type");
		}

		if (!draw_fmt(&header_ptr, &buff_n, &text_n, 1,
				"%s", line->from))
			goto print_header;

print_header:
		/* Print the line header */
		printf(C_MOVE(%d, 1) "%s " ATTR_RESET, coords.r1, header);
	}

	while (skip--)
		irc_strwrap(text_w, &p1, p2);

	do {
		const char *sep = " " SEP_VERT " ";

		if ((coords.cN - coords.c1) >= sizeof(*sep) + text_w) {
			printf(C_MOVE(%d, %d), coords.r1, (int)(coords.cN - (sizeof(*sep) + text_w + 1)));
			fputs(draw_colour(BUFFER_LINE_HEADER_FG_NEUTRAL, -1), stdout);
			fputs(sep, stdout);
		}

		if (*p1) {
			const char *text_p1 = p1;
			const char *text_p2 = irc_strwrap(text_w, &p1, p2);
			unsigned text_fg = BUFFER_TEXT_FG;
			unsigned text_bg = BUFFER_TEXT_BG;
			unsigned text_len = text_p2 - text_p1;

			if (line->type == BUFFER_LINE_CHAT && strlen(QUOTE_LEADER)) {
				if (!strncmp(line->text, QUOTE_LEADER, strlen(QUOTE_LEADER))) {
					text_fg = QUOTE_TEXT_FG;
					text_bg = QUOTE_TEXT_BG;
				}
			}

			printf(C_MOVE(%d, %d), coords.r1, head_w);
			printf("%s%.*s", draw_colour(text_fg, text_bg), text_len, text_p1);
		}

		coords.r1++;

	} while (*p1 && coords.r1 <= coords.rN);
}

static void
draw_separators(void)
{
	printf(C_MOVE(2, 1));

	draw_bg(SEP_BG);
	draw_fg(SEP_FG);

	for (unsigned i = 0; i < state_cols(); i++)
		printf(SEP_HORZ);
}

static void
draw_input(struct input *inp, struct coords coords)
{
	/* Draw the input line, or the current action message */

	check_coords(coords);

	const char *action;
	unsigned cols = coords.cN - coords.c1 + 1;
	unsigned cursor_row = coords.r1;
	unsigned cursor_col = coords.cN;

	printf(C_MOVE(%d, %d), coords.r1, coords.c1);

	if ((action = action_message())) {
		if (!(cols = drawf(cols, "%b%f%s%b%f-- %s --",
				INPUT_PREFIX_BG,
				INPUT_PREFIX_FG,
				INPUT_PREFIX,
				ACTION_BG,
				ACTION_FG,
				action)))
			goto cursor;

		cursor_col = coords.cN - coords.c1 - cols + 3;
	} else {
		char input[INPUT_LEN_MAX];
		unsigned cursor_pre;
		unsigned cursor_inp;

		if (!(cols = drawf(cols, "%b%f%s",
				INPUT_PREFIX_BG,
				INPUT_PREFIX_FG,
				INPUT_PREFIX)))
			goto cursor;

		cursor_pre = coords.cN - coords.c1 - cols + 1;
		cursor_inp = input_frame(inp, input, cols);

		if (!(cols = drawf(cols, "%b%f%s",
				INPUT_BG,
				INPUT_FG,
				input)))
			goto cursor;

		cursor_col = cursor_pre + cursor_inp + 1;
	}

	printf(ATTR_RESET);

	while (cols--)
		printf(" ");

cursor:

	cursor_row = MIN(cursor_row, coords.rN);
	cursor_col = MIN(cursor_col, coords.cN);

	printf(C_MOVE(%d, %d), cursor_row, cursor_col);
	printf(C_SAVE);
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

	printf(C_MOVE(1, 1) CLEAR_LINE);

	static struct channel *frame_prev,
	                      *frame_next;

	struct channel *c_first = channel_get_first(),
	               *c_last = channel_get_last(),
	               *tmp;

	unsigned cols = state_cols();

	c->activity = ACTIVITY_DEFAULT;

	/* By default assume drawing starts towards the next channel */
	int nextward = 1;

	size_t len, total_len = 0;

	/* Bump the channel frames, if applicable */
	if ((total_len = (c->name_len + 2)) >= state_cols())
		return;
	else if (c == frame_prev && frame_prev != c_first)
		frame_prev = channel_get_prev(frame_prev);
	else if (c == frame_next && frame_next != c_last)
		frame_next = channel_get_next(frame_next);

	/* Calculate the new frames */
	struct channel *tmp_prev = c, *tmp_next = c;

	for (;;) {

		if (tmp_prev == c_first || tmp_prev == frame_prev) {

			/* Pad out nextward */

			tmp = channel_get_next(tmp_next);
			len = tmp->name_len;

			while ((total_len += (len + 2)) < state_cols() && tmp != c_first) {

				tmp_next = tmp;

				tmp = channel_get_next(tmp);
				len = tmp->name_len;
			}

			break;
		}

		if (tmp_next == c_last || tmp_next == frame_next) {

			/* Pad out prevward */

			tmp = channel_get_prev(tmp_prev);
			len = tmp->name_len;

			while ((total_len += (len + 2)) < state_cols() && tmp != c_last) {

				tmp_prev = tmp;

				tmp = channel_get_prev(tmp);
				len = tmp->name_len;
			}

			break;
		}

		tmp = nextward ? channel_get_next(tmp_next) : channel_get_prev(tmp_prev);
		len = tmp->name_len;

		/* Next channel doesn't fit */
		if ((total_len += (len + 2)) >= state_cols())
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

		if (!(cols = drawf(cols, "%f %s ", fg, tmp->name)))
			break;

		if (tmp == frame_next)
			break;
	}
}

static void
draw_status(struct channel *c)
{
	/* server buffer:
	 *  -[usermodes]-(ping)-(scrollback)
	 *
	 * private buffer:
	 *  -[usermodes]-[priv]-(ping)-(scrollback)
	 *
	 * channel buffer:
	 *  -[usermodes]-[chantype chanmodes chancount]-(ping)-(scrollback)
	 */

	#define STATUS_SEP_HORZ \
		"%b%f" SEP_HORZ "%b%f", \
			SEP_BG, \
			SEP_FG, \
			STATUS_BG, \
			STATUS_FG

	unsigned cols = state_cols();
	unsigned rows = state_rows();
	unsigned scrollback;

	if (!cols || !(rows > 1))
		return;

	printf(C_MOVE(%d, 1), rows - 1);

	/* -[usermodes] */
	if (c->server && *(c->server->mode_str.str)) {
		if (!(cols = drawf(cols, STATUS_SEP_HORZ)))
			return;
		if (!(cols = drawf(cols, "[+%s]", c->server->mode_str.str)))
			return;
	}

	/* -[priv] */
	if (c->type == CHANNEL_T_PRIVATE) {
		if (!(cols = drawf(cols, STATUS_SEP_HORZ)))
			return;
		if (!(cols = drawf(cols, "[priv]")))
			return;
	}

	/* -[chantype chanmodes chancount] */
	if (c->type == CHANNEL_T_CHANNEL) {
		if (!(cols = drawf(cols, STATUS_SEP_HORZ)))
			return;
		if (!(cols = drawf(cols, "[%c %s %u]",
				c->chanmodes.prefix,
				c->chanmodes_str.str,
				c->users.count)))
			return;
	}

	/* -(ping) */
	if (c->server && c->server->ping) {
		if (!(cols = drawf(cols, STATUS_SEP_HORZ)))
			return;
		if (!(cols = drawf(cols, "(%us)", c->server->ping)))
			return;
	}

	/* -(scrollback) */
	if ((scrollback = buffer_scrollback_status(&c->buffer))) {
		if (!(cols = drawf(cols, STATUS_SEP_HORZ)))
			return;
		if (!(cols = drawf(cols, "(%u%%)", scrollback)))
			return;
	}

	draw_bg(SEP_BG);
	draw_fg(SEP_FG);

	while (cols--)
		printf(SEP_HORZ);
}

static void
check_coords(struct coords coords)
{
	/* Check coordinate validity before drawing, ensure at least one row, column */

	if (coords.r1 > coords.rN)
		fatal("row coordinates invalid (%u > %u)", coords.r1, coords.rN);

	if (coords.c1 > coords.cN)
		fatal("col coordinates invalid (%u > %u)", coords.c1, coords.cN);
}

static unsigned
nick_col(char *nick)
{
	unsigned colour = 0;

	while (*nick)
		colour += *nick++;

	return nick_colours[colour % sizeof(nick_colours) / sizeof(nick_colours[0])];
}

static char*
draw_colour(int fg, int bg)
{
	/* Set terminal foreground and background colours to a value [0, 255],
	 * or reset colour if given anything else */

	static char buf[COLOUR_SIZE + 1] = ATTR_RESET;
	size_t len = sizeof(ATTR_RESET) - 1;
	int ret = 0;

	if (fg >= 0 && fg <= 255) {
		if ((ret = snprintf(buf + len, sizeof(buf) - len, ATTR_FG(%d), fg)) < 0)
			buf[len] = 0;
		else
			len += ret;
	}

	if (bg >= 0 && bg <= 255) {
		if ((snprintf(buf + len, sizeof(buf) - len, ATTR_BG(%d), bg)) < 0)
			buf[len] = 0;
	}

	return buf;
}

static int
draw_fmt(char **ptr, size_t *buff_n, size_t *text_n, int txt, const char *fmt, ...)
{
	/* Write formatted text to a buffer for purposes of preparing an object to be drawn
	 * to the terminal.
	 *
	 * Calls to this function should distinguish between formatting and printed text
	 * with the txt flag.
	 *
	 *  - ptr    : pointer to location in buffer being printed to
	 *  - buff_n : remaining bytes available in buff
	 *  - text_n : remaining columns available for text
	 *  - txt    : flag set true if bytes being written are printable text
	 *
	 *  returns 0 on error, or if no more prints to this buffer can occur
	 */

	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vsnprintf(*ptr, *buff_n, fmt, ap);
	va_end(ap);

	if (ret < 0)
		return (**ptr = 0);

	size_t _ret = (size_t) ret;

	if (!txt && _ret >= *buff_n)
		return (**ptr = 0);

	if (txt) {
		if (*text_n > _ret)
			*text_n -= _ret;
		else {
			*ptr += *text_n;
			**ptr = 0;
			return (*text_n = 0);
		}
	}

	*ptr += _ret;

	return 1;
}

static unsigned
drawf(unsigned cols, const char *fmt, ...)
{
	/* Draw formatted text up to a given number of
	 * columns. Returns number of unused columns.
	 *
	 *  %a -- attribute reset
	 *  %b -- set background colour attribute
	 *  %f -- set foreground colour attribute
	 *  %c -- output char
	 *  %d -- output signed integer
	 *  %u -- output unsigned integer
	 *  %s -- output string
	 *  %% -- output literal '%'
	 */

	char buf[64];
	char c;
	va_list arg;

	va_start(arg, fmt);

	while (cols && (c = *fmt++)) {
		if (c == '%') {
			switch ((c = *fmt++)) {
				case 'a':
					printf(ATTR_RESET);
					break;
				case 'b':
					draw_bg(va_arg(arg, int));
					break;
				case 'f':
					draw_fg(va_arg(arg, int));
					break;
				case 'c':
					draw_char(va_arg(arg, int));
					cols--;
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
				case '%':
					putchar('%');
					cols--;
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

	return cols;
}

static void
draw_bg(int bg)
{
	if (bg == -1)
		printf(ATTR_RESET_BG);

	if (bg >= 0 && bg <= 255)
		printf(ATTR_BG(%d), bg);

	bg_last = bg;
}

static void
draw_fg(int fg)
{
	if (fg == -1)
		printf(ATTR_RESET_FG);

	if (fg >= 0 && fg <= 255)
		printf(ATTR_FG(%d), fg);

	fg_last = fg;
}

static void
draw_char(int c)
{
	if (iscntrl(c)) {
		int ctrl_bg_last = bg_last;
		int ctrl_fg_last = fg_last;
		draw_bg(CTRL_BG);
		draw_fg(CTRL_FG);
		putchar((c | 0x40));
		draw_bg(ctrl_bg_last);
		draw_fg(ctrl_fg_last);
	} else {
		putchar(c);
	}
}
