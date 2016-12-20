#include <string.h>

#include "buffer.h"

#if (BUFFER_LINES_MAX & (BUFFER_LINES_MAX - 1)) != 0
	/* Required for proper masking when indexing */
	#error BUFFER_LINES_MAX must be a power of 2
#endif

#define MASK(X) ((X) & (BUFFER_LINES_MAX - 1))

static unsigned int buffer_size(struct buffer*);
static unsigned int buffer_full(struct buffer*);

static struct buffer_line* buffer_push(struct buffer*);

static unsigned int
buffer_size(struct buffer *b)
{
	return b->head - b->tail;
}

static unsigned int
buffer_full(struct buffer *b)
{
	return buffer_size(b) == BUFFER_LINES_MAX;
}

static struct buffer_line*
buffer_push(struct buffer *b)
{
	if (buffer_full(b))
		b->tail++;

	return &b->buffer_lines[MASK(b->head++)];
}

struct buffer_line*
buffer_f(struct buffer *b)
{
	/* Return the first printable line in a buffer */

	return &b->buffer_lines[MASK(b->head - 1)];
}

struct buffer_line*
buffer_l(struct buffer *b)
{
	/* Return the last printable line in a buffer */

	return &b->buffer_lines[MASK(b->tail)];
}

unsigned int
buffer_line_rows(struct buffer_line *l, unsigned int w)
{
	/* Return the number of times a buffer line will wrap within w columns */

	if (w == 0)
		fatal("width is zero");

	if (l->w != w) {
		unsigned int count = 0;

		char *ptr1 = l->text;
		char *ptr2 = l->text + l->text_len;

		do {
			word_wrap(w, &ptr1, ptr2);
			count++;
		} while (*ptr1);

		l->w = w;
		l->rows = count;
	}

	return l->rows;
}

void
buffer_newline(struct buffer *b, enum buffer_line_t type, const char *from, const char *text)
{
	struct buffer_line *l = buffer_push(b);

	if (from == NULL)
		fatal("from is NULL");

	if (text == NULL)
		fatal("text is NULL");

	size_t remainder = 0,
		   text_len = strlen(text),
		   from_len = strlen(from);

	/* Split overlength lines into continuations */
	if (text_len > TEXT_LENGTH_MAX) {
		remainder = text_len - TEXT_LENGTH_MAX;
		text_len = TEXT_LENGTH_MAX;
	}

	/* Silently truncate */
	if (from_len > FROM_LENGTH_MAX)
		from_len = FROM_LENGTH_MAX;

	memcpy(l->from, from, from_len);
	memcpy(l->text, text, text_len);

	*(l->from + text_len) = '\0';
	*(l->text + text_len) = '\0';

	l->from_len = from_len;
	l->text_len = text_len;

	l->time = time(NULL);
	l->type = type;
	l->rows = 0;
	l->w = 0;

	if (from_len > b->pad)
		b->pad = from_len;

	if (remainder)
		buffer_newline(b, type, from, text + TEXT_LENGTH_MAX);
}

/* TODO
 * scrollback
 *   ensure always >= buffer_l, < buffer_f
 *     conditionally increment on buffer_push
 *   buffer_page_f(buffer, unsigned int rows)
 *     don't redraw if advance wouldn't change lines on screen
 *     special case when scrolling forward from back of buffer
 *   buffer_page_b(buffer, unsigned int rows)
 * activity, draw bits
 *   set when newline is called on a channel
 *
 * buffer_reset <- clear/reset all fields (pretty much just memset 0)
 *
 * reimplement draw functions to use these abstractions
 * */
