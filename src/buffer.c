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

void
buffer_newline(struct buffer *b, buffer_line_t type, const char *from, const char *text)
{
	struct buffer_line *l = buffer_push(b);

	/* FIXME: move fatal to utils.h, move some utils stuff here
	if (from == NULL || text == NULL)
		fatal("buffer_newline");
	*/

	size_t remainder = 0,
		   text_len = strlen(text),
		   from_len = strlen(from);

	if (text_len > TEXT_LENGTH_MAX) {
		remainder = text_len - TEXT_LENGTH_MAX;
		text_len = TEXT_LENGTH_MAX;
	}

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

unsigned int
buffer_line_rows(struct buffer_line *l, unsigned int w)
{
	/* Count the number of times a buffer line will wrap within w columns */

	int count = 0;

	char *ptr1 = l->text;
	char *ptr2 = l->text + l->len;

	do {
		word_wrap(w, &ptr1, ptr2);
		count++;
	} while (*ptr1);

	return count;
}


/* TODO
 * line rows, scrollback, activity
 *
 * reimplement old functionality:
 * buffer_line:
 *	unsigned int rows;
 *	unsigned int w
 * buffer:
 *	struct buffer_line *scrollback;
 *	unsigned int w
 * set buffer activity
 * set draw bits
 * move newline and newlinef here as functions of a buffer?
 *
 * buffer_reset <- clear/reset all fields (pretty much just memset 0)
 *
 * reimplement draw functions to use these abstractions
 * */
