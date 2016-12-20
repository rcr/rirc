#ifndef BUFFER_H
#define BUFFER_H

#include <time.h>

#define TEXT_LENGTH_MAX 510
#define FROM_LENGTH_MAX 100

#define BUFFER_LINES_MAX (1 << 10)

typedef enum {
	BUFFER_OTHER,   /* Default/all other buffers */
	BUFFER_CHANNEL, /* Channel message buffer */
	BUFFER_SERVER,  /* Server message buffer */
	BUFFER_PRIVATE, /* Private message buffer */
	BUFFER_T_SIZE
} buffer_t;

typedef enum {
	BUFFER_LINE_OTHER,  /* Default/all other lines */
	BUFFER_LINE_CHAT,   /* Line of text from another IRC user */
	BUFFER_LINE_PINGED, /* Line of text from another IRC user containing current nick */
	BUFFER_LINE_T_SIZE
} buffer_line_t;

struct buffer_line
{
	buffer_line_t type;
	char from[FROM_LENGTH_MAX + 1];
	char text[TEXT_LENGTH_MAX + 1];
	size_t len;
	time_t time;
	unsigned int rows;
	unsigned int w;
};

struct buffer
{
	buffer_t type; /* TODO: set when new_channel */

	unsigned int head;
	unsigned int tail;

	size_t pad;

	struct buffer_line buffer_lines[BUFFER_LINES_MAX];
};

struct buffer_line* buffer_f(struct buffer*);
struct buffer_line* buffer_l(struct buffer*);

void buffer_newline(struct buffer*, buffer_line_t, const char*, const char*);

#endif
