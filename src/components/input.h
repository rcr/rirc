#ifndef INPUT_H
#define INPUT_H

#define SCROLLBACK_INPUT 15
#define BUFFSIZE 512
#define RIRC_MAX_INPUT 256 /* FIXME: MAX_INPUT conflicts with limits.h */

/* When tab completing a nick at the beginning of the line, append the following char */
#define TAB_COMPLETE_DELIMITER ':'

/* Compile time checks */
#if BUFFSIZE < RIRC_MAX_INPUT
	/* Required so input lines can be safely strcpy'ed into a send buffer */
	#error BUFFSIZE must be greater than RIRC_MAX_INPUT
#endif

/* Channel input line */
struct input_line
{
	char *end;
	char text[RIRC_MAX_INPUT + 1];
	struct input_line *next;
	struct input_line *prev;
};

/* Channel input */
struct input
{
	char *head;
	char *tail;
	char *window;
	unsigned int count;
	struct input_line *line;
	struct input_line *list_head;
};

/* TODO: refactor */
struct input* new_input(void);
void action(int(*)(char), const char*, ...);
void free_input(struct input*);
extern char *action_message;

/* TODO: return state altering function */
int input(struct input*, const char*, size_t);

#endif
