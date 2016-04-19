#ifndef STATE_H
#define STATE_H

/* state.h
 *
 * Interface for retrieving and altering global state of the program */

/* Global state of rirc */
struct state
{
	channel *current_channel; /* the current channel being drawn */
	channel *default_channel; /* the default rirc channel at startup */

	server *server_list;
};

void init_state(void);
void free_state(void);

/* Ensure state cannot be altered by functions beyond this interface */
struct state const* get_state(void);

/* Useful state retrieval abstractions */
channel* channel_get(char*, server*);
channel* channel_get_first();
channel* channel_get_last();
channel* channel_get_next(channel*);
channel* channel_get_prev(channel*);

/* State altering interface */
channel* new_channel(char*, server*, channel*, buffer_t);
void auto_nick(char**, char*);
void buffer_scrollback_back(channel*);
void buffer_scrollback_forw(channel*);
void channel_clear(channel*);
void channel_close(channel*);
void channel_move_prev(void);
void channel_move_next(void);
void channel_set_current(channel*);
void channel_set_mode(channel*, const char*);
void free_channel(channel*);
void newline(channel*, line_t, const char*, const char*);
void newlinef(channel*, line_t, const char*, const char*, ...);
void nicklist_print(channel*);
void part_channel(channel*);
void reset_channel(channel*);
void server_set_mode(server*, const char*);

#endif
