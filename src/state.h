#ifndef RIRC_STATE_H
#define RIRC_STATE_H

#include "src/components/buffer.h"
#include "src/components/channel.h"
#include "src/components/server.h"

#define FROM_INFO    "--"
#define FROM_ERROR   "-!!-"
#define FROM_UNKNOWN "-\?\?-"
#define FROM_JOIN    ">>"
#define FROM_PART    "<<"
#define FROM_QUIT    "<<"

#define server_info(S, ...) \
	do { newlinef((S)->channel, BUFFER_LINE_SERVER_INFO, FROM_INFO, __VA_ARGS__); } while (0)

#define server_error(S, ...) \
	do { newlinef((S)->channel, BUFFER_LINE_SERVER_ERROR, FROM_ERROR, __VA_ARGS__); } while (0)

void state_init(void);
void state_term(void);

/* Get tty dimensions */
unsigned state_cols(void);
unsigned state_rows(void);

const char *action_message(void);
struct channel* channel_get_first(void);
struct channel* channel_get_last(void);
struct channel* channel_get_next(struct channel*);
struct channel* channel_get_prev(struct channel*);
struct channel* current_channel(void);
struct server_list* state_server_list(void);
void channel_set_current(struct channel*);
void newlinef(struct channel*, enum buffer_line_type, const char*, const char*, ...);

#endif
