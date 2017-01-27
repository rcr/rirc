#ifndef INPUT_H
#define INPUT_H

#include "utils.h"

//FIXME:
#define SCROLLBACK_INPUT 15
#define MAX_INPUT 256

/* Channel input line */
typedef struct input_line
{
	char *end;
	char text[MAX_INPUT];
	struct input_line *next;
	struct input_line *prev;
} input_line;

/* Channel input */
typedef struct input
{
	char *head;
	char *tail;
	char *window;
	unsigned int count;
	struct input_line *line;
	struct input_line *list_head;
} input;

/* TODO: refactor */
input* new_input(void);
void action(int(*)(char), const char*, ...);
void free_input(input*);
void poll_input(void);
extern char *action_message;

#endif
