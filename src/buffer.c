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
newline(struct buffer *b, char *text)
{
	struct buffer_line *l = buffer_push(b);

	size_t remainder = 0, len = strlen(text);

	if (len > LINE_LENGTH_MAX) {
		remainder = len - LINE_LENGTH_MAX;
		len = LINE_LENGTH_MAX;
	}

	memcpy(l->text, text, len);

	*(l->text + len) = '\0';

	l->len = len;

	if (remainder) {
		newline(b, text + LINE_LENGTH_MAX);
	}
}
