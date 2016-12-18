#include "buffer.h"

#if (BUFFER_LINES_MAX & (BUFFER_LINES_MAX - 1)) != 0
	/* Required for proper masking when indexing */
	#error BUFFER_LINES_MAX must be a power of 2
#endif

#define MASK(X) ((X) & (BUFFER_LINES_MAX - 1))

static unsigned int buffer_size(struct buffer*);
static unsigned int buffer_full(struct buffer*);

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

int
buffer_f(struct buffer *b)
{
	return b->vals[MASK(b->head - 1)];
}

int
buffer_l(struct buffer *b)
{
	return b->vals[MASK(b->tail)];
}

void
buffer_push(struct buffer *b, int v)
{
	if (buffer_full(b))
		b->tail++;

	b->vals[MASK(b->head++)] = v;
}
