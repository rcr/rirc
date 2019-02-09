#ifndef BUFFER_H
#define BUFFER_H

#include <time.h>

#include "src/utils/utils.h"
#include "config.h"

#define TEXT_LENGTH_MAX 510 /* FIXME: remove max lengths in favour of growable buffer */
#define FROM_LENGTH_MAX 100

#ifndef BUFFER_LINES_MAX
#define BUFFER_LINES_MAX (1 << 10)
#endif

/* Buffer line types, in order of precedence */
enum buffer_line_t
{
	BUFFER_LINE_OTHER,      /* Default/all other lines */
	BUFFER_LINE_SERVER_MSG, /* Server info message */
	BUFFER_LINE_SERVER_ERR, /* Server error message */
	BUFFER_LINE_JOIN,       /* Irc join message */
	BUFFER_LINE_PART,       /* Irc part message */
	BUFFER_LINE_QUIT,       /* Irc quit message */
	BUFFER_LINE_CHAT,       /* Line of text from another IRC user */
	BUFFER_LINE_PINGED,     /* Line of text from another IRC user containing current nick */
	BUFFER_LINE_T_SIZE
};

struct buffer_line
{
	enum buffer_line_t type;
	char prefix; /* TODO as part of `from` */
	char from[FROM_LENGTH_MAX + 1]; /* TODO: from/text as struct string */
	char text[TEXT_LENGTH_MAX + 1];
	size_t from_len;
	size_t text_len;
	time_t time;
	struct {
		unsigned int colour; /* Cached colour of `from` text */
		unsigned int rows;   /* Cached number of rows occupied when wrapping on w columns */
		unsigned int w;      /* Cached width for rows */
		unsigned int initialized : 1;
	} cached;
};

struct buffer
{
	unsigned int head;
	unsigned int tail;
	unsigned int scrollback; /* Index of the current line between [tail, head) for scrollback */
	size_t pad;              /* Pad 'from' when printing to be at least this wide */
	struct buffer_line buffer_lines[BUFFER_LINES_MAX];
};

float buffer_scrollback_status(struct buffer*);

int buffer_page_back(struct buffer*, unsigned int, unsigned int);
int buffer_page_forw(struct buffer*, unsigned int, unsigned int);

unsigned int buffer_line_rows(struct buffer_line*, unsigned int);

void buffer(struct buffer*);

struct buffer_line* buffer_head(struct buffer*);
struct buffer_line* buffer_tail(struct buffer*);
struct buffer_line* buffer_line(struct buffer*, unsigned int);

void buffer_newline(
	struct buffer*,
	enum buffer_line_t,
	const char*,
	const char*,
	size_t,
	size_t,
	char);

#endif
