#include "src/components/buffer.h"

#include "src/utils/utils.h"

#include <string.h>

#define BUFFER_MASK(X) ((X) & (BUFFER_LINES_MAX - 1))

#if BUFFER_MASK(BUFFER_LINES_MAX)
#error BUFFER_LINES_MAX must be a power of 2
#endif

static struct buffer_line* buffer_push(struct buffer*);

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
buffer_line(struct buffer *b, unsigned i)
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
		fatal("invalid index: %d", i);

	return &b->buffer_lines[BUFFER_MASK(i)];
}

void
buffer_newline(
		struct buffer *b,
		enum buffer_line_type type,
		const char *from_str,
		const char *text_str,
		size_t from_len,
		size_t text_len,
		char prefix)
{
	struct buffer_line *line;

	if (from_str == NULL)
		fatal("from string is NULL");

	if (text_str == NULL)
		fatal("text string is NULL");

	line = memset(buffer_push(b), 0, sizeof(*line));

	line->from_len = MIN(from_len + (!!prefix), FROM_LENGTH_MAX);
	line->text_len = MIN(text_len,              TEXT_LENGTH_MAX);

	if (prefix)
		*line->from = prefix;

	memcpy(line->from + (!!prefix), from_str, line->from_len);
	memcpy(line->text,              text_str, line->text_len);

	*(line->from + line->from_len) = '\0';
	*(line->text + line->text_len) = '\0';

	line->time = time(NULL);
	line->type = type;

	if (line->from_len > b->pad)
		b->pad = line->from_len;
}

void
buffer(struct buffer *b)
{
	/* Initialize a buffer */

	memset(b, 0, sizeof(*b));
}

unsigned
buffer_size(struct buffer *b)
{
	/* Return number of lines in ring buffer  */

	return b->head - b->tail;
}

static struct buffer_line*
buffer_push(struct buffer *b)
{
	/* Return a new `struct buffer_line *` pushed to a buffer */

	/* lock scrollback to head */
	if (buffer_line(b, b->scrollback) == buffer_head(b))
		b->scrollback = b->head;

	/* lock scrollback to tail */
	if (buffer_size(b) == BUFFER_LINES_MAX) {

		if (b->scrollback == b->tail)
			b->scrollback++;

		b->tail++;
	}

	return &(b->buffer_lines[BUFFER_MASK(b->head++)]);
}
