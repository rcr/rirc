#ifndef DRAW_H
#define DRAW_H

#include "src/components/buffer.h"

/* Draw component, e.g. draw_buffer(); */
#define DRAW_BITS \
	X(buffer) \
	X(input)  \
	X(nav)    \
	X(status)

union draw
{
	struct {
		#define X(bit) unsigned int bit : 1;
		DRAW_BITS
		#undef X
	} bits;
	unsigned int all_bits;
};

void draw(union draw);
void bell(void);
void split_buffer_cols(struct buffer_line*, unsigned int*, unsigned int*, unsigned int, unsigned int);

#endif
