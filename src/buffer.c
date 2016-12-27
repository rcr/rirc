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
	/* Return a new buffer_line pushed to a buffer, ensure that:
	 *  - scrollback stays between [tail, head)
	 *  - tail increments when the buffer is full */

	if (buffer_sb(b) == buffer_head(b))
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
buffer_sb(struct buffer *b)
{
	/* Return the buffer scrollback line or NULL if buffer is empty */

	if (buffer_size(b) == 0)
		return NULL;

	return &b->buffer_lines[MASK(b->scrollback)];
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

	size_t remainder_len = 0,
	       text_len = strlen(text),
	       from_len = strlen(from);

	/* Split overlength lines into continuations */
	if (text_len > TEXT_LENGTH_MAX) {
		remainder_len = text_len - TEXT_LENGTH_MAX;
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

	if (remainder_len)
		buffer_newline(b, type, from, text + TEXT_LENGTH_MAX);
}

unsigned int
buffer_sb_status(struct buffer *b)
{
	/* Return the buffer scrollback status as a number between [0, 100] */

	if (buffer_sb(b) == buffer_head(b))
		return 0;

	return (unsigned int)(100 * ((float)(b->head - b->scrollback) / (float)(BUFFER_LINES_MAX)));
}

int
buffer_page_back(struct buffer *b, unsigned int rows, unsigned int cols)
{
	unsigned int r;

	if (!rows)
		fatal("rows are 0");

	if (!cols)
		fatal("cols are 0");

	/* Should always go back at least one line */
	while (buffer_sb(b) != buffer_tail(b)) {

		b->scrollback--;

		r = buffer_line_rows(&b->buffer_lines[MASK(b->scrollback)], cols);

		if (r > rows)
			return 1;

		rows -= r;
	}

	return 0;
}

int
buffer_page_forw(struct buffer *b, unsigned int rows, unsigned int cols)
{
	unsigned int r;

	if (!rows)
		fatal("rows are 0");

	if (!cols)
		fatal("cols are 0");

	while (buffer_sb(b) != buffer_head(b)) {

		b->scrollback++;

		r = buffer_line_rows(&b->buffer_lines[MASK(b->scrollback)], cols);

		if (r > rows)
			return 1;

		rows -= r;
	}


	return 0;
}

struct buffer
buffer_init(enum buffer_t type)
{
	/* Initialize a buffer */

	return (struct buffer) { .type = type };
}


/* TODO
 * activity, draw bits
 *   set when newline is called on a channel
 *
 * reimplement draw functions to use these abstractions
 * */
