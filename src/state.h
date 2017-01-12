#ifndef STATE_H
#define STATE_H

/* state.h
 *
 * Interface for retrieving and altering global state of the program */

/* state.c */
/* FIXME: terrible, until i remove references to ccur/rirc */
#define rirc (default_channel())
#define ccur (current_channel())
channel* current_channel(void);
channel* default_channel(void);

//TODO: rename
unsigned int _term_cols(void);
unsigned int _term_rows(void);

void resize(void);

void init_state(void);
void free_state(void);

/* Useful state retrieval abstractions */
channel* channel_get(char*, server*);
channel* channel_get_first(void);
channel* channel_get_last(void);
channel* channel_get_next(channel*);
channel* channel_get_prev(channel*);

/* State altering interface */
channel* new_channel(char*, server*, channel*, enum buffer_t);
void auto_nick(char**, char*);

/* FIXME: */
void buffer_scrollback_back(channel*);
void buffer_scrollback_forw(channel*);
void channel_clear(channel*);

void channel_close(channel*);
void channel_move_prev(void);
void channel_move_next(void);
void channel_set_current(channel*);
void channel_set_mode(channel*, const char*);
void free_channel(channel*);
void newline(channel*, enum buffer_line_t, const char*, const char*);
void newlinef(channel*, enum buffer_line_t, const char*, const char*, ...);
void nicklist_print(channel*);
void part_channel(channel*);
void reset_channel(channel*);
void server_set_mode(server*, const char*);

#endif
