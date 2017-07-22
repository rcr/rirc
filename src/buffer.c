/* TODO: reduce overall size of buffer, implement growable text area with
 * ring of buffer_lines pointing to strings, instantiated with initial size
 *
 * i.e.            line(n).text            line(n+1).text
 *                           v                         v
 * text buffer: [...\0user1\0some line of text\0user2\0some other line\0...]
 *                    ^                         ^
 *          line(n).user             line(n+1).user
 *
 * will allow removal of text/from truncation */

#include <string.h>

#include "buffer.h"
#include "utils.h"

#if (BUFFER_LINES_MAX & (BUFFER_LINES_MAX - 1)) != 0
	/* Required for proper masking when indexing */
	#error BUFFER_LINES_MAX must be a power of 2
#endif

#define MASK(X) ((X) & (BUFFER_LINES_MAX - 1))

static unsigned int buffer_full(struct buffer*);
static unsigned int buffer_size(struct buffer*);

static struct buffer_line* buffer_push(struct buffer*);

static unsigned int
buffer_full(struct buffer *b)
{
	return buffer_size(b) == BUFFER_LINES_MAX;
}

static unsigned int
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

	return &b->buffer_lines[MASK(b->head++)];
}

struct buffer_line*
buffer_head(struct buffer *b)
{
	/* Return the first printable line in a buffer */

	return buffer_size(b) == 0 ? NULL : &b->buffer_lines[MASK(b->head - 1)];
}

struct buffer_line*
buffer_tail(struct buffer *b)
{
	/* Return the last printable line in a buffer */

	return buffer_size(b) == 0 ? NULL : &b->buffer_lines[MASK(b->tail)];
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
		fatal("invalid index");

	return &b->buffer_lines[MASK(i)];
}

unsigned int
buffer_line_rows(struct buffer_line *line, unsigned int w)
{
	/* Return the number of times a buffer line will wrap within w columns */

	char *p;

	if (w == 0)
		fatal("width is zero");

	/* Empty lines occupy are considered to occupy a row */
	if (!*line->text)
		return line->_rows = 1;

	if (line->_w != w) {
		line->_w = w;

		for (p = line->text, line->_rows = 0; *p; line->_rows++)
			word_wrap(w, &p, line->text + line->text_len);
	}

	return line->_rows;
}

void
buffer_newline(
		struct buffer *b,
		enum buffer_line_t type,
		const char *from,
		const char *text,
		size_t from_len,
		size_t text_len)
{
	struct buffer_line *line;

	if (from == NULL)
		fatal("from is NULL");

	if (text == NULL)
		fatal("text is NULL");

	size_t remainder_len = 0;

	if (!from_len)
		from_len = strlen(from);

	if (!text_len)
		text_len = strlen(text);

	line = memset(buffer_push(b), 0, sizeof(*line));

	/* Split overlength lines into continuations */
	if (text_len > TEXT_LENGTH_MAX) {
		remainder_len = text_len - TEXT_LENGTH_MAX;
		text_len = TEXT_LENGTH_MAX;
	}

	/* Silently truncate */
	if (from_len > FROM_LENGTH_MAX)
		from_len = FROM_LENGTH_MAX;

	memcpy(line->from, from, from_len);
	memcpy(line->text, text, text_len);

	*(line->from + from_len) = '\0';
	*(line->text + text_len) = '\0';

	line->from_len = from_len;
	line->text_len = text_len;

	line->time = time(NULL);
	line->type = type;

	if (from_len > b->pad)
		b->pad = from_len;

	if (remainder_len)
		buffer_newline(b, type, from, text + TEXT_LENGTH_MAX, from_len, remainder_len);
}

float
buffer_scrollback_status(struct buffer *b)
{
	/* Return the buffer scrollback status as a number between [0, 100] */

	if (buffer_line(b, b->scrollback) == buffer_head(b))
		return 0;

	return (float)(b->head - b->scrollback) / (float)(buffer_size(b));
}

struct buffer
buffer(enum buffer_t type)
{
	/* Initialize a buffer */

	return (struct buffer) { .type = type };
}
