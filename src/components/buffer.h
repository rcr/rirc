#ifndef RIRC_COMPONENTS_BUFFER_H
#define RIRC_COMPONENTS_BUFFER_H

#include <time.h>

#define TEXT_LENGTH_MAX 510 /* FIXME: remove max lengths in favour of growable buffer */
#define FROM_LENGTH_MAX 100

#ifndef BUFFER_LINES_MAX
#define BUFFER_LINES_MAX (1 << 10)
#endif

/* Buffer line types, in order of precedence */
enum buffer_line_type
{
	BUFFER_LINE_OTHER,        /* Default/all other lines */
	BUFFER_LINE_SERVER_INFO,  /* Server info message */
	BUFFER_LINE_SERVER_ERROR, /* Server error message */
	BUFFER_LINE_JOIN,         /* Irc JOIN message */
	BUFFER_LINE_NICK,         /* Irc NICK message */
	BUFFER_LINE_PART,         /* Irc PART message */
	BUFFER_LINE_QUIT,         /* Irc QUIT message */
	BUFFER_LINE_CHAT,         /* Line of text from another IRC user */
	BUFFER_LINE_PINGED,       /* Line of text from another IRC user containing current nick */
	BUFFER_LINE_T_SIZE
};

struct buffer_line
{
	enum buffer_line_type type;
	char prefix; /* TODO as part of `from` */
	char from[FROM_LENGTH_MAX + 1]; /* TODO: from/text as struct string */
	char text[TEXT_LENGTH_MAX + 1];
	size_t from_len;
	size_t text_len;
	time_t time;
	struct {
		unsigned colour; /* Cached colour of `from` text */
		unsigned rows;   /* Cached number of rows occupied when wrapping on w columns */
		unsigned w;      /* Cached width for rows */
		unsigned initialized : 1;
	} cached;
};

struct buffer
{
	unsigned head;
	unsigned tail;
	unsigned scrollback; /* Index of the current line between [tail, head) for scrollback */
	size_t pad;              /* Pad 'from' when printing to be at least this wide */
	struct buffer_line buffer_lines[BUFFER_LINES_MAX];
};

unsigned buffer_scrollback_status(struct buffer*);

int buffer_page_back(struct buffer*, unsigned, unsigned);
int buffer_page_forw(struct buffer*, unsigned, unsigned);

unsigned buffer_line_rows(struct buffer_line*, unsigned);

void buffer(struct buffer*);

struct buffer_line* buffer_head(struct buffer*);
struct buffer_line* buffer_tail(struct buffer*);
struct buffer_line* buffer_line(struct buffer*, unsigned);

void buffer_newline(
	struct buffer*,
	enum buffer_line_type,
	const char*,
	const char*,
	size_t,
	size_t,
	char);

void
buffer_line_split(
	struct buffer_line *line,
	unsigned *head_w,
	unsigned *text_w,
	unsigned cols,
	unsigned pad);

#endif
