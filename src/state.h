#ifndef STATE_H
#define STATE_H

#include "src/components/buffer.h"
#include "src/components/channel.h"
#include "src/components/input.h"
#include "src/components/server.h"
#include "src/draw.h"

/* state.h
 *
 * Interface for retrieving and altering global state of the program */

int state_server_set_chans(struct server*, const char*);


/* state.c */
struct channel* current_channel(void);

struct server_list* state_server_list(void);

void state_init(void);
void state_term(void);

// TODO: most of this stuff can be static
//TODO: move to channel.c, function of server's channel list
/* Useful state retrieval abstractions */
struct channel* channel_get_first(void);
struct channel* channel_get_last(void);
struct channel* channel_get_next(struct channel*);
struct channel* channel_get_prev(struct channel*);

/* FIXME: */
void buffer_scrollback_back(struct channel*);
void buffer_scrollback_forw(struct channel*);
void channel_clear(struct channel*);

void channel_close(struct channel*);
void channel_move_prev(void);
void channel_move_next(void);
void channel_set_current(struct channel*);

void free_channel(struct channel*);

#define FROM_ERROR "-!!-"
#define FROM_INFO "--"
#define FROM_UNKNOWN "-\?\?-"
#define FROM_JOIN ">>"
#define FROM_NICK "--"
#define FROM_PART "<<"
#define FROM_QUIT "<<"

// info / err
#define server_msg(S, ...) \
	do { newlinef((S)->channel, BUFFER_LINE_SERVER_MSG, FROM_INFO, __VA_ARGS__); } while (0)

#define server_err(S, ...) \
	do { newlinef((S)->channel, BUFFER_LINE_SERVER_ERR, FROM_ERROR, __VA_ARGS__); } while (0)

#define server_unknown(S, M, ...) \
	do { newlinef((S)->channel, BUFFER_LINE_SERVER_ERR, FROM_UNKNOWN, (M), __VA_ARGS__); } while (0)

// TODO: replace above macros
#define server_message(S, F, M, ...) \
	do { newlinef((S)->channel, BUFFER_LINE_SERVER_ERR, (F), (M), __VA_ARGS__); } while (0)

void newlinef(struct channel*, enum buffer_line_t, const char*, const char*, ...);
void newline(struct channel*, enum buffer_line_t, const char*, const char*);

/* TODO: refactor, should be static in state */
/* Function prototypes for setting draw bits */
#define X(bit) void draw_##bit(void);
DRAW_BITS
#undef X
void draw_all(void);

void redraw(void);

extern char *action_message;

#endif
