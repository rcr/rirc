#ifndef STATE_H
#define STATE_H

/* Interface for retrieving and altering global state of the program */

channel* channel_close(channel*);
channel* channel_get(char*, server*);
channel* channel_switch(channel*, int);
channel* new_channel(char*, server*, channel*, buffer_t);
void auto_nick(char**, char*);
void buffer_scrollback_back(channel*);
void buffer_scrollback_forw(channel*);
void clear_channel(channel*);
void free_channel(channel*);
void newline(channel*, line_t, const char*, const char*);
void _newline(channel*, line_t, const char*, const char*, size_t);
void newlinef(channel*, line_t, const char*, const char*, ...);
void nicklist_print(channel*);
void part_channel(channel*);

void init_state(void);
void free_state(void);

#endif
