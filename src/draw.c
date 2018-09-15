/* draw.c
 *
 * Draw the elements in state.c to the terminal.
 *
 * Assumes vt-100 compatible escape codes, as such YMMV */

/* TODO:
 *
 * setvbuf can be used for testing draw routines, pointing to static buffer
 *
 * vector io, from draw(), pass pointers to terminal row buffers, draw in a single write
 * */

#include <alloca.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "src/components/input.h"
#include "src/io.h"
#include "src/state.h"
#include "src/utils/utils.h"

#define ESC "\x1b"

#define RESET_ATTRIBUTES ESC"[0m"

#define FG(X) ESC"[38;5;"#X"m"
#define BG(X) ESC"[48;5;"#X"m"

#define MOVE(X, Y) ESC"["#X";"#Y"H"

#define CLEAR_FULL  ESC"[2J"
#define CLEAR_RIGHT ESC"[0K"
#define CLEAR_LEFT  ESC"[1K"
#define CLEAR_LINE  ESC"[2K"

/* Save and restore the cursor's location */
#define CURSOR_SAVE    ESC"[s"
#define CURSOR_RESTORE ESC"[u"

/* Minimum rows or columns to safely draw */
#define COLS_MIN 5
#define ROWS_MIN 5

/* Size of a full colour string for purposes of pre-formating text to print */
#define COLOUR_SIZE sizeof(RESET_ATTRIBUTES FG(255) BG(255))

#ifndef BUFFER_PADDING
	#define BUFFER_PADDING 1
#elif BUFFER_PADDING != 0 && BUFFER_PADDING != 1
	#error "BUFFER_PADDING options are 0 (no pad), 1 (padded)"
#endif

static int actv_colours[ACTIVITY_T_SIZE] = ACTIVITY_COLOURS
static int nick_colours[] = NICK_COLOURS

/* Terminal coordinate row/column boundaries (inclusive) for objects being drawn
 *
 *   \ c0     cN
 *    +---------+
 *  r0|         |
 *    |         |
 *    |         |
 *  rN|         |
 *    +---------+
 *
 * The origin for terminal coordinates is in the top left, indexed from 1
 *
 * */
struct coords
{
	unsigned int c1;
	unsigned int cN;
	unsigned int r1;
	unsigned int rN;
};

static int _draw_fmt(char**, size_t*, size_t*, int, const char*, ...);

static void _draw_buffer_line(struct buffer_line*, struct coords, unsigned int, unsigned int, unsigned int, unsigned int);
static void _draw_buffer(struct buffer*, struct coords);
static void _draw_input(struct input*, struct coords);
static void _draw_nav(struct channel*);
static void _draw_status(struct channel*);

static inline unsigned int nick_col(char*);
static inline void check_coords(struct coords);

static char* _colour(int, int);

void
draw(union draw draw)
{
	if (!draw.all_bits)
		return;

	struct channel *c = current_channel();

	if (io_tty_cols() < COLS_MIN || io_tty_rows() < ROWS_MIN) {
		printf(CLEAR_FULL MOVE(1, 1) "rirc");
		goto no_draw;
	}

	printf(CURSOR_SAVE);

	if (draw.bits.buffer) _draw_buffer(&c->buffer,
		(struct coords) {
			.c1 = 1,
			.cN = io_tty_cols(),
			.r1 = 3,
			.rN = io_tty_rows() - 2
		});

	if (draw.bits.nav)    _draw_nav(c);

	if (draw.bits.input)  _draw_input(c->input,
		(struct coords) {
			.c1 = 1,
			.cN = io_tty_cols(),
			.r1 = io_tty_rows(),
			.rN = io_tty_rows()
		});

	if (draw.bits.status) _draw_status(c);

	printf(RESET_ATTRIBUTES);
	printf(CURSOR_RESTORE);

no_draw:

	fflush(stdout);
}

void
bell(void)
{
	if (BELL_ON_PINGED)
		putchar('\a');
}

/* FIXME: works except when it doesn't.
 *
 * Fails when line headers are very long compared to text. tests/draw.c needed */
static void
_draw_buffer_line(
		struct buffer_line *line,
		struct coords coords,
		unsigned int head_w,
		unsigned int text_w,
		unsigned int skip,
		unsigned int pad)
{
	check_coords(coords);

	char *print_p1,
	     *print_p2,
	     *p1 = line->text,
	     *p2 = line->text + line->text_len;

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

		size_t buff_n = sizeof(header) - 1, /*  */
		       text_n = head_w - 1;

		struct tm *line_tm = localtime(&line->time);

		if (!_draw_fmt(&header_ptr, &buff_n, &text_n, 0,
				_colour(BUFFER_LINE_HEADER_FG_NEUTRAL, -1)))
			goto print_header;

		if (!_draw_fmt(&header_ptr, &buff_n, &text_n, 1,
				" %02d:%02d ", line_tm->tm_hour, line_tm->tm_min))
			goto print_header;

		if (!_draw_fmt(&header_ptr, &buff_n, &text_n, 1,
				"%*s", pad, ""))
			goto print_header;

		if (!_draw_fmt(&header_ptr, &buff_n, &text_n, 0, RESET_ATTRIBUTES))
			goto print_header;

		switch (line->type) {
			case BUFFER_LINE_OTHER:
				if (!_draw_fmt(&header_ptr, &buff_n, &text_n, 0,
						_colour(BUFFER_LINE_HEADER_FG_NEUTRAL, -1)))
					goto print_header;
				break;

			case BUFFER_LINE_CHAT:
				if (!_draw_fmt(&header_ptr, &buff_n, &text_n, 0,
						_colour(line->cached.colour, -1)))
					goto print_header;
				break;

			case BUFFER_LINE_PINGED:
				if (!_draw_fmt(&header_ptr, &buff_n, &text_n, 0,
						_colour(BUFFER_LINE_HEADER_FG_PINGED, BUFFER_LINE_HEADER_BG_PINGED)))
					goto print_header;
				break;

			case BUFFER_LINE_T_SIZE:
				break;
		}

		if (!_draw_fmt(&header_ptr, &buff_n, &text_n, 1,
				"%s", line->from))
			goto print_header;

print_header:
		/* Print the line header */
		printf(MOVE(%d, 1) "%s " RESET_ATTRIBUTES, coords.r1, header);
	}

	while (skip--)
		word_wrap(text_w, &p1, p2);

	do {
		char *sep = " "VERTICAL_SEPARATOR" ";

		if ((coords.cN - coords.c1) >= sizeof(*sep) + text_w) {
			printf(MOVE(%d, %d), coords.r1, (int)(coords.cN - (sizeof(*sep) + text_w + 1)));
			fputs(_colour(BUFFER_LINE_HEADER_FG_NEUTRAL, -1), stdout);
			fputs(sep, stdout);
		}

		if (*p1) {
			printf(MOVE(%d, %d), coords.r1, head_w);

			print_p1 = p1;
			print_p2 = word_wrap(text_w, &p1, p2);

			fputs(_colour(line->text[0] == QUOTE_CHAR
					? BUFFER_LINE_TEXT_FG_GREEN
					: BUFFER_LINE_TEXT_FG_NEUTRAL,
					-1),
				stdout);

			printf("%.*s", (int)(print_p2 - print_p1), print_p1);
		}

		coords.r1++;

	} while (*p1 && coords.r1 <= coords.rN);
}

static void
_draw_buffer(struct buffer *b, struct coords coords)
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

	unsigned int row,
	             row_count = 0,
	             row_total = coords.rN - coords.r1 + 1;

	unsigned int col_total = coords.cN - coords.c1 + 1;

	unsigned int buffer_i = b->scrollback,
	             head_w,
	             text_w;

	/* Clear the buffer area */
	for (row = coords.r1; row <= coords.rN; row++)
		printf(MOVE(%d, 1) CLEAR_LINE, row);

	struct buffer_line *line = buffer_line(b, buffer_i);

	if (line == NULL)
		return;

	struct buffer_line *tail = buffer_tail(b);
	struct buffer_line *head = buffer_head(b);

	/* Find top line */
	for (;;) {

		split_buffer_cols(line, NULL, &text_w, col_total, b->pad);

		row_count += buffer_line_rows(line, text_w);

		if (line == tail)
			break;

		if (row_count >= row_total)
			break;

		line = buffer_line(b, --buffer_i);
	}

	/* Handle impartial top line print */
	if (row_count > row_total) {

		split_buffer_cols(line, &head_w, &text_w, col_total, b->pad);

		_draw_buffer_line(
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

		split_buffer_cols(line, &head_w, &text_w, col_total, b->pad);

		_draw_buffer_line(
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
_draw_nav(struct channel *c)
{
	/* Dynamically draw the nav such that:
	 *
	 *  - The current channel is kept framed while navigating
	 *  - Channels are coloured based on their current activity
	 *  - The nav is kept framed between the first and last channels
	 */

	printf(MOVE(1, 1) CLEAR_LINE);

	static struct channel *frame_prev,
	                      *frame_next;

	struct channel *c_first = channel_get_first(),
	               *c_last = channel_get_last(),
	               *tmp;

	c->activity = ACTIVITY_DEFAULT;

	/* By default assume drawing starts towards the next channel */
	int colour, nextward = 1;

	size_t len, total_len = 0;

	/* Bump the channel frames, if applicable */
	if ((total_len = (c->name.len + 2)) >= io_tty_cols())
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
			len = tmp->name.len;

			while ((total_len += (len + 2)) < io_tty_cols() && tmp != c_first) {

				tmp_next = tmp;

				tmp = channel_get_next(tmp);
				len = tmp->name.len;
			}

			break;
		}

		if (tmp_next == c_last || tmp_next == frame_next) {

			/* Pad out prevward */

			tmp = channel_get_prev(tmp_prev);
			len = tmp->name.len;

			while ((total_len += (len + 2)) < io_tty_cols() && tmp != c_last) {

				tmp_prev = tmp;

				tmp = channel_get_prev(tmp);
				len = tmp->name.len;
			}

			break;
		}

		tmp = nextward ? channel_get_next(tmp_next) : channel_get_prev(tmp_prev);
		len = tmp->name.len;

		/* Next channel doesn't fit */
		if ((total_len += (len + 2)) >= io_tty_cols())
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

		colour = (tmp == c) ? NAV_CURRENT_CHAN : actv_colours[tmp->activity];

		if (fputs(_colour(colour, -1), stdout) < 0)
			break;

		if (printf(" %s ", tmp->name.str) < 0)
			break;

		if (tmp == frame_next)
			break;
	}
}

static void
_draw_input(struct input *in, struct coords coords)
{
	/* Draw the input line, or the current action message */

	check_coords(coords);

	unsigned int cols_t = coords.cN - coords.c1 + 1,
	             cursor = coords.c1;

	printf(RESET_ATTRIBUTES);
	printf(MOVE(%d, 1) CLEAR_LINE, coords.rN);
	printf(CURSOR_SAVE);

	/* Insufficient columns for meaningful input drawing */
	if (cols_t < 3)
		return;

	char input[cols_t + COLOUR_SIZE * 2 + 1];
	char *input_ptr = input;

	size_t buff_n = sizeof(input) - 1,
	       text_n = cols_t;

	if (sizeof(INPUT_PREFIX)) {

		if (!_draw_fmt(&input_ptr, &buff_n, &text_n, 0,
				"%s", _colour(INPUT_PREFIX_FG, INPUT_PREFIX_BG)))
			goto print_input;

		cursor = coords.c1 + sizeof(INPUT_PREFIX) - 1;

		if (!_draw_fmt(&input_ptr, &buff_n, &text_n, 1,
				INPUT_PREFIX))
			goto print_input;
	}

	if (!_draw_fmt(&input_ptr, &buff_n, &text_n, 0,
			"%s", _colour(INPUT_FG, INPUT_BG)))
		goto print_input;

	if (action_message) {

		cursor = coords.cN;

		if (!_draw_fmt(&input_ptr, &buff_n, &text_n, 1,
				"%s", action_message))
			goto print_input;

		cursor = cols_t - text_n + 1;

	} else {

		/*  Keep the input head in view, reframing if the cursor would be
		 *  drawn outside [A, B] as a function of input window and head
		 *
		 * |  <prefix>     <text area>  |
		 * |............|---------------|
		 * |             A             B|
		 * |                            | : cols_t
		 *              |               | : text_n
		 *
		 * The cursor should track the input head, where the next
		 * character would be entered
		 *
		 * In the <= A case: deletions occurred; the head is less than
		 * or equal to the window
		 *
		 * In the >= B case: insertions occurred; the distance from window
		 * to head is greater than the distance from [A, B]
		 *
		 * Set the window 2/3 of the text area width backwards from the head */

		size_t frame = text_n * 2 / 3;

		char *w1, *w2, *ptr;

		if (in->window >= in->head || (in->window + text_n) <= in->head) {

			w1 = in->line->text,
			w2 = in->line->text + frame;

			in->window = (w2 >= in->head) ? w1 : in->head - frame;
		}

		cursor = (cols_t - text_n) + (in->head - in->window + 1);

		for (ptr = in->window; text_n; text_n--) {

			/* Copy characters, x, from the gap buffer, i.e.:
			 *
			 *  window   head    tail
			 *       v      v       v
			 * |.....xxxxxxx|------|xxxxxxx| */

			if (ptr == in->head)
				ptr = in->tail;

			if (ptr == in->line->text + RIRC_MAX_INPUT)
				break;

			*input_ptr++ = *ptr++;
		}

		*input_ptr = '\0';
	}

print_input:

	fputs(input, stdout);
	printf(MOVE(%d, %d), coords.rN, (cursor >= coords.c1 && cursor <= coords.cN) ? cursor : coords.cN);
	printf(CURSOR_SAVE);
}

static void
_draw_status(struct channel *c)
{
	/* TODO: channel modes, channel type_flag, servermodes */

	/* server / private chat:
	 * |-[usermodes]-(latency)---...|
	 *
	 * channel:
	 * |-[usermodes]-[chancount chantype chanmodes]/[priv]-(latency)---...|
	 * */

	float sb;
	int ret;
	unsigned int col = 0;
	unsigned int cols = io_tty_cols();
	unsigned int rows = io_tty_rows();

	/* Insufficient columns for meaningful status */
	if (cols < 3)
		return;

	printf(RESET_ATTRIBUTES);

	printf(MOVE(2, 1));
	printf("%.*s", cols, (char *)(memset(alloca(cols), *HORIZONTAL_SEPARATOR, cols)));

	printf(MOVE(%d, 1) CLEAR_LINE, rows - 1);

	/* Print status to temporary buffer */
	char status_buff[cols + 1];

	memset(status_buff, 0, cols + 1);

	/* -[usermodes] */
	if (c->server && *(c->server->mode_str.str)) {
		ret = snprintf(status_buff + col, cols - col + 1, "%s", HORIZONTAL_SEPARATOR "[+");
		if (ret < 0 || (col += ret) >= cols)
			goto print_status;

		ret = snprintf(status_buff + col, cols - col + 1, "%s", c->server->mode_str.str);
		if (ret < 0 || (col += ret) >= cols)
			goto print_status;

		ret = snprintf(status_buff + col, cols - col + 1, "%s", "]");
		if (ret < 0 || (col += ret) >= cols)
			goto print_status;
	}

	/* If private chat buffer:
	 * -[priv] */
	if (c->buffer.type == BUFFER_PRIVATE) {
		ret = snprintf(status_buff + col, cols - col + 1, "%s", HORIZONTAL_SEPARATOR "[priv]");
		if (ret < 0 || (col += ret) >= cols)
			goto print_status;
	}

	/* If IRC channel buffer:
	 * -[chancount chantype chanmodes] */
	if (c->buffer.type == BUFFER_CHANNEL) {

		ret = snprintf(status_buff + col, cols - col + 1,
				HORIZONTAL_SEPARATOR "[%d", c->users.count);
		if (ret < 0 || (col += ret) >= cols)
			goto print_status;

		if (c->chanmodes.prefix) {
			ret = snprintf(status_buff + col, cols - col + 1, " %c", c->chanmodes.prefix);
			if (ret < 0 || (col += ret) >= cols)
				goto print_status;
		}

		if (*(c->chanmodes_str.str)) {
			ret = snprintf(status_buff + col, cols - col + 1, " +%s", c->chanmodes_str.str);
			if (ret < 0 || (col += ret) >= cols)
				goto print_status;
		}

		ret = snprintf(status_buff + col, cols - col + 1, "%s", "]");
		if (ret < 0 || (col += ret) >= cols)
			goto print_status;
	}

	/* -(latency) */
	if (c->server && c->server->latency_delta) {
		ret = snprintf(status_buff + col, cols - col + 1,
				HORIZONTAL_SEPARATOR "(%llds)", (long long) c->server->latency_delta);
		if (ret < 0 || (col += ret) >= cols)
			goto print_status;
	}

	/* -(scrollback%) */
	if ((sb = buffer_scrollback_status(&c->buffer))) {
		ret = snprintf(status_buff + col, cols - col + 1,
				HORIZONTAL_SEPARATOR "(%02d%%)", (int)(sb * 100));
		if (ret < 0 || (col += ret) >= cols)
			goto print_status;
	}

print_status:

	fputs(status_buff, stdout);

	/* Trailing separator */
	while (col++ < cols)
		printf(HORIZONTAL_SEPARATOR);
}

static inline void
check_coords(struct coords coords)
{
	/* Check coordinate validity before drawing, ensure at least one row, column */

	if (coords.r1 > coords.rN)
		fatal("row coordinates invalid", 0);

	if (coords.c1 > coords.cN)
		fatal("column coordinates invalid", 0);
}

static inline unsigned int
nick_col(char *nick)
{
	unsigned int colour = 0;

	while (*nick)
		colour += *nick++;

	return nick_colours[colour % sizeof(nick_colours) / sizeof(nick_colours[0])];
}

static char*
_colour(int fg, int bg)
{
	/* Set terminal foreground and background colours to a value [0, 255],
	 * or reset colour if given anything else
	 *
	 * Foreground(F): ESC"[38;5;Fm"
	 * Background(B): ESC"[48;5;Bm"
	 * */

	static char col_buff[COLOUR_SIZE + 1] = RESET_ATTRIBUTES;

	int ret = 0;

	char *col_buff_ptr = col_buff + sizeof(RESET_ATTRIBUTES) - 1;

	/* Assume any colour sequence begins by resetting all attributes */
	*(col_buff_ptr = col_buff + sizeof(RESET_ATTRIBUTES) - 1) = '\0';

	/* Set valid foreground colour */
	if (fg >= 0 && fg <= 255 && ((ret = sprintf(col_buff_ptr, ESC"[38;5;%dm", fg)) < 0))
		return strcpy(col_buff, RESET_ATTRIBUTES);

	col_buff_ptr += ret;

	/* Set valid background colour */
	if (bg >= 0 && bg <= 255 && ((ret = sprintf(col_buff_ptr, ESC"[48;5;%dm", bg)) < 0))
		return strcpy(col_buff, RESET_ATTRIBUTES);

	return col_buff;
}

static int
_draw_fmt(char **ptr, size_t *buff_n, size_t *text_n, int txt, const char *fmt, ...)
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

void
split_buffer_cols(
	struct buffer_line *line,
	unsigned int *head_w,
	unsigned int *text_w,
	unsigned int cols,
	unsigned int pad)
{
	unsigned int _head_w = sizeof(" HH:MM   "VERTICAL_SEPARATOR" ");

	if (BUFFER_PADDING)
		_head_w += pad;
	else
		_head_w += line->from_len;

	/* If header won't fit, split in half */
	if (_head_w >= cols)
		_head_w = cols / 2;

	//TODO: why?
	_head_w -= 1;

	if (head_w)
		*head_w = _head_w;
	if (text_w)
		*text_w = cols - _head_w + 1;
}
