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

static unsigned int buffer_size(struct buffer *b) { return b->head - b->tail; }
static unsigned int buffer_full(struct buffer *b) { return buffer_size(b) == BUFFER_LINES_MAX; }

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
	return &b->buffer_lines[MASK(b->head - 1)];
}

struct buffer_line*
buffer_l(struct buffer *b)
{
	return &b->buffer_lines[MASK(b->tail)];
}

void
newline(struct buffer *b, char *text)
{
	/* TODO: lines > max length, recursively call newline on remainder */

	struct buffer_line *l = buffer_push(b);

	strncpy(l->text, text, LINE_LENGTH_MAX);

	*(l->text + LINE_LENGTH_MAX) = 0;
}
