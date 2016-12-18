#ifndef BUFFER_H
#define BUFFER_H

#define BUFFER_LINES_MAX (1 << 10)

struct buffer
{
	unsigned int head;
	unsigned int tail;

	int vals[BUFFER_LINES_MAX];
};

int buffer_f(struct buffer*);
int buffer_l(struct buffer*);

void buffer_push(struct buffer*, int);

#endif
