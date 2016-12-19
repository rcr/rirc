#ifndef BUFFER_H
#define BUFFER_H

#define LINE_LENGTH_MAX 510

#define BUFFER_LINES_MAX (1 << 10)

struct buffer_line
{
	char text[LINE_LENGTH_MAX + 1];
};

struct buffer
{
	unsigned int head;
	unsigned int tail;

	struct buffer_line buffer_lines[BUFFER_LINES_MAX];
};

struct buffer_line* buffer_f(struct buffer*);
struct buffer_line* buffer_l(struct buffer*);

void newline(struct buffer*, char*);

#endif
