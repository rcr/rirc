/* TODO: reduce overall size of buffer, implement growable text area with
 * ring of buffer_lines pointing to strings, keeping nulls for easy casting
 * to c strings
 *
 * i.e.            line(n).text            line(n+1).text
 *                           v                         v
 * text buffer: [...\0user1\0some line of text\0user2\0some other line\0...]
 *                    ^                         ^
 *          line(n).user             line(n+1).user
 *
 *
 * growable area, e.g. realloced in factors of 1.5, indexed by buffer_segment, e.g.
 * struct segment {
 *     uint16_t index;
 *     uint16_t len;
 * }
 * indexed from base of data pointer
 *
 * TODO: cache a sensible number of line breaks (2?) against the line width
 * and dynamically calculate the rest, terminal size rarely changes
 *
 * TODO: buffer_newline should accept the formatting arguments rather than
 * vsnprintf to a temp buffer and pass, e.g.:
 * void buffer_newline(buffer*, buffer_line_t, struct user*, const char*, ...)
 *
 * TODO: print time string to buffer space
 * */

#include <string.h>

#include "src/components/buffer.h"

#define BUFFER_MASK(X) ((X) & (BUFFER_LINES_MAX - 1))

#if BUFFER_MASK(BUFFER_LINES_MAX)
/* Required for proper masking when indexing */
#error BUFFER_LINES_MAX must be a power of 2
#endif

static inline unsigned int buffer_full(struct buffer*);
static inline unsigned int buffer_size(struct buffer*);

static struct buffer_line* buffer_push(struct buffer*);

static inline unsigned int
buffer_full(struct buffer *b)
{
	return buffer_size(b) == BUFFER_LINES_MAX;
}

static inline unsigned int
buffer_size(struct buffer *b)
{
	return b->head - b->tail;
}

static struct buffer_line*
buffer_push(struct buffer *b)
{
	/* Return a new buffer_line pushed to a buffer, ensure that:
	 *  - scrollback stays between [tail, head)
	 *  - tail increments when the buffer is full */

	if (buffer_line(b, b->scrollback) == buffer_head(b))
		b->scrollback = b->head;

	if (buffer_full(b)) {

		/* scrollback locked to tail */
		if (b->scrollback == b->tail)
			b->scrollback++;

		b->tail++;
	}

	return &b->buffer_lines[BUFFER_MASK(b->head++)];
}

struct buffer_line*
buffer_head(struct buffer *b)
{
	/* Return the first printable line in a buffer */

	return buffer_size(b) == 0 ? NULL : &b->buffer_lines[BUFFER_MASK(b->head - 1)];
}

struct buffer_line*
buffer_tail(struct buffer *b)
{
	/* Return the last printable line in a buffer */

	return buffer_size(b) == 0 ? NULL : &b->buffer_lines[BUFFER_MASK(b->tail)];
}

struct buffer_line*
buffer_line(struct buffer *b, unsigned int i)
{
	/* Return the buffer line indexed by i */

	if (buffer_size(b) == 0)
		return NULL;

	/* Check that the index is between [tail, head) in a way that accounts for unsigned overflow
	 *
	 * Normally:
	 *   |-----T-----H-----|
	 *      a     b     c
	 *
	 *  a, c : invalid
	 *  b    : valid
	 *
	 * Inverted after overflow of head:
	 *   |-----H-----T-----|
	 *      a     b     c
	 *
	 *  a, c : valid
	 *  b    : invalid
	 *  */

	if (((b->head > b->tail) && (i < b->tail || i >= b->head)) ||
	    ((b->tail > b->head) && (i < b->tail && i >= b->head)))
		fatal("invalid index", 0);

	return &b->buffer_lines[BUFFER_MASK(i)];
}

unsigned int
buffer_line_rows(struct buffer_line *line, unsigned int w)
{
	/* Return the number of times a buffer line will wrap within w columns */

	char *p;

	if (w == 0)
		fatal("width is zero", 0);

	/* Empty lines are considered to occupy a row */
	if (!*line->text)
		return line->cached.rows = 1;

	if (line->cached.w != w) {
		line->cached.w = w;

		for (p = line->text, line->cached.rows = 0; *p; line->cached.rows++)
			word_wrap(w, &p, line->text + line->text_len);
	}

	return line->cached.rows;
}

void
buffer_newline(
		struct buffer *b,
		enum buffer_line_t type,
		struct string from,
		struct string text,
		char prefix)
{
	struct buffer_line *line;

	if (from.str == NULL)
		fatal("from string is NULL", 0);

	if (text.str == NULL)
		fatal("text string is NULL", 0);

	line = memset(buffer_push(b), 0, sizeof(*line));

	line->from_len = MIN(from.len + (!!prefix), FROM_LENGTH_MAX);
	line->text_len = MIN(text.len,              TEXT_LENGTH_MAX);

	if (prefix)
		*line->from = prefix;

	memcpy(line->from + (!!prefix), from.str, line->from_len);
	memcpy(line->text,              text.str, line->text_len);

	*(line->from + line->from_len) = '\0';
	*(line->text + line->text_len) = '\0';

	line->time = time(NULL);
	line->type = type;

	if (line->from_len > b->pad)
		b->pad = line->from_len;

	if (text.len > TEXT_LENGTH_MAX) {

		struct string _text = {
			.str = text.str + TEXT_LENGTH_MAX,
			.len = text.len - TEXT_LENGTH_MAX
		};

		buffer_newline(b, type, from, _text, prefix);
	}
}

float
buffer_scrollback_status(struct buffer *b)
{
	/* Return the buffer scrollback status as a number between [0, 100] */

	if (buffer_line(b, b->scrollback) == buffer_head(b))
		return 0;

	return (float)(b->head - b->scrollback) / (float)(buffer_size(b));
}

void
buffer(struct buffer *b)
{
	/* Initialize a buffer */

	memset(b, 0, sizeof(*b));
}
