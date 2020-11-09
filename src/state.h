#ifndef RIRC_STATE_H
#define RIRC_STATE_H

#include "src/components/buffer.h"
#include "src/components/channel.h"
#include "src/components/server.h"

#define FROM_INFO "--"
#define FROM_ERROR "-!!-"
#define FROM_UNKNOWN "-\?\?-"
#define FROM_JOIN ">>"
#define FROM_NICK "--"
#define FROM_PART "<<"
#define FROM_QUIT "<<"

#define server_info(S, ...) \
	do { newlinef((S)->channel, BUFFER_LINE_SERVER_INFO, FROM_INFO, __VA_ARGS__); } while (0)

#define server_error(S, ...) \
	do { newlinef((S)->channel, BUFFER_LINE_SERVER_ERROR, FROM_ERROR, __VA_ARGS__); } while (0)

#define server_unknown(S, ...) \
	do { newlinef((S)->channel, BUFFER_LINE_SERVER_ERROR, FROM_UNKNOWN, __VA_ARGS__); } while (0)

int state_server_set_chans(struct server*, const char*);

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

void newlinef(struct channel*, enum buffer_line_t, const char*, const char*, ...);
void newline(struct channel*, enum buffer_line_t, const char*, const char*);

extern char *action_message;

#endif
