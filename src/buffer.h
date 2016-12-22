#ifndef BUFFER_H
#define BUFFER_H

#include <time.h>

#include "utils.h"

#define TEXT_LENGTH_MAX 510
#define FROM_LENGTH_MAX 100

#define BUFFER_LINES_MAX (1 << 10)

enum buffer_line_t
{
	BUFFER_LINE_OTHER,  /* Default/all other lines */
	BUFFER_LINE_CHAT,   /* Line of text from another IRC user */
	BUFFER_LINE_PINGED, /* Line of text from another IRC user containing current nick */
	BUFFER_LINE_T_SIZE
};

enum buffer_t
{
	BUFFER_OTHER,   /* Default/all other buffers */
	BUFFER_CHANNEL, /* Channel message buffer */
	BUFFER_SERVER,  /* Server message buffer */
	BUFFER_PRIVATE, /* Private message buffer */
	BUFFER_T_SIZE
};

struct buffer_line
{
	enum buffer_line_t type;
	char from[FROM_LENGTH_MAX + 1];
	char text[TEXT_LENGTH_MAX + 1];
	size_t from_len;
	size_t text_len;
	time_t time;
	unsigned int rows; /* Cached number of rows occupied when wrapping on w columns */
	unsigned int w;    /* Cached width for rows */
};

struct buffer
{
	enum buffer_t type; /* TODO: set when new_channel */
	unsigned int head;
	unsigned int tail;
	unsigned int scrollback; /* Index of the current line between [tail, head) for scrollback */
	size_t pad;
	struct buffer_line buffer_lines[BUFFER_LINES_MAX];
};

unsigned int buffer_line_rows(struct buffer_line*, unsigned int);

struct buffer_line* buffer_head(struct buffer*);
struct buffer_line* buffer_tail(struct buffer*);
struct buffer_line* buffer_sb(struct buffer*);

void buffer_newline(struct buffer*, enum buffer_line_t, const char*, const char*);

#endif
