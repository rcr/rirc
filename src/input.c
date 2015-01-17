/* input.c
 *
 * Input handling from stdin
 *
 * All input is handled synchronously and refers to the current
 * channel being drawn (ccur)
 *
 * Escape sequences are assumed to be ANSI. As such, you mileage may vary
 * */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include "common.h"

/* Max number of characters accepted in user pasted input */
#define MAX_PASTE 2048

/* Max length of user action confirmation messages */
#define MAX_CONFIRM 256

/* Defined in draw.c */
extern struct winsize w;

/* Static buffer that accepts input from stdin */
static char input_buff[MAX_PASTE];

/* User input handlers */
static void input_char(char);
static void input_cchar(char);
static void input_cseq(char*, ssize_t);
static void input_paste(char*, ssize_t);
static void input_confirm(char*, ssize_t);

/* Actions confirmation handling */
static int (*confirm_handler)(char);
static char confirm_buff[MAX_CONFIRM];

/* Confirmation handler when multi-line pastes are encountered */
static int send_paste(char);

/* Send the current input to be parsed and handled */
static void send_input(void);

/* Input line manipulation functions */
static inline void cursor_left(input*);
static inline void cursor_right(input*);
static inline void delete_left(input*);
static inline void delete_right(input*);
static inline void scroll_up(input*);
static inline void scroll_down(input*);

/* Input line util functions */
static inline void reset_line(input*);
static inline void reframe_line(input*);

static input_line* new_input_line(input_line*);

input*
new_input(void)
{
	input *i;
	if ((i = malloc(sizeof(input))) == NULL)
		fatal("new_input");

	i->count = 0;
	i->line = new_input_line(NULL);
	i->list_head = i->line;
	i->head = i->line->text;
	i->tail = i->line->text + MAX_INPUT;
	i->window = i->line->text;

	return i;
}

void
free_input(input *i)
{
	input_line *t, *l = i->list_head;

	do {
		t = l;
		l = l->next;
		free(t);
	} while (l != i->list_head);

	free(i);
}

static input_line*
new_input_line(input_line *prev)
{
	input_line *l;
	if ((l = malloc(sizeof(input_line))) == NULL)
		fatal("new_input_line");

	l->end = l->text;
	l->prev = prev ? prev : l;
	l->next = prev ? prev->next : l;

	if (prev)
		prev->next = prev->next->prev = l;

	return l;
}

void
poll_input(void)
{
	/* Poll stdin for user input. 4 cases:
	 *
	 * 1. A single printable character
	 * 2. A single byte control character
	 * 3. A multibyte control sequence
	 * 4. Pasted input (up to MAX_PASTE characters)
	 *
	 * Pastes should be sanitized of unprintable characters and split into
	 * lines by \n characters or by MAX_INPUT. The user is warned about
	 * pastes exceeding a single line before sending. */

	int ret;
	fd_set stdin_fd;

	FD_ZERO(&stdin_fd);
	FD_SET(STDIN_FILENO, &stdin_fd);

	/* FIXME: handle EINTR */
	if ((ret = select(2, &stdin_fd, NULL, NULL, &(struct timeval){ .tv_usec = 200000 })) < 0)
		fatal("poll_input - select");

	if (ret) {

		ssize_t count;

		if ((count = read(STDIN_FILENO, input_buff, MAX_PASTE)) < 0)
			fatal("poll_input - read");

		if (count == 0)
			fatal("poll_input - stdin closed");

		/* Waiting for user confirmation, ignore everything else */
		 if (confirm_message)
			input_confirm(input_buff, count);

		/* Case 1 */
		else if (count == 1 && isprint(*input_buff))
			input_char(*input_buff);

		/* Case 2 */
		else if (count == 1 && iscntrl(*input_buff))
			input_cchar(*input_buff);

		/* Case 3 */
		else if (*input_buff == 0x1b)
			input_cseq(input_buff, count);

		/* Case 4 */
		else if (count > 1)
			input_paste(input_buff, count);
	}
}

/*
 * User input handlers
 * */

static void
input_char(char c)
{
	/* Input a single character */

	if (ccur->input->head < ccur->input->tail)
		*ccur->input->head++ = c;

	draw(D_INPUT);
}

static void
input_cchar(char c)
{
	/* Input a single byte control character */

	switch (c) {

		/* BS -- '\b' (backspace) */
		case 0x7F:
			delete_left(ccur->input);
			break;

		/* LF -- '\n' (new line) */
		case 0x0A:
			send_input();
			break;

		/* CAN -- (cancel) */
		case 0x18:
			ccur = channel_close(ccur);
			break;
	}
}

static void
input_cseq(char *input, ssize_t len)
{
	/* Input a multibyte control sequence */

	/* Skip comparing the escape character */
	input++;
	if (--len == 0)
		return;

	/* arrow up */
	else if (!strncmp(input, "[A", len))
		scroll_up(ccur->input);

	/* arrow down */
	else if (!strncmp(input, "[B", len))
		scroll_down(ccur->input);

	/* arrow right */
	else if (!strncmp(input, "[C", len))
		cursor_right(ccur->input);

	/* arrow left */
	else if (!strncmp(input, "[D", len))
		cursor_left(ccur->input);

	/* delete */
	else if (!strncmp(input, "[3~", len))
		delete_right(ccur->input);

	/* page up */
	else if (!strncmp(input, "[5~", len))
		ccur = channel_switch(ccur, 0);

	/* page down */
	else if (!strncmp(input, "[6~", len))
		ccur = channel_switch(ccur, 1);

	/* mousewheel up */
	else if (!strncmp(input, "[M`", len))
		ccur = channel_switch(ccur, 0);

	/* mousewheel down */
	else if (!strncmp(input, "[Ma", len))
		ccur = channel_switch(ccur, 1);
}

static void
input_paste(char *input, ssize_t len)
{
	/* Input pasted text */

	/* TODO: count the number of lines that would be sent,
	 *
	 * - sanitize the input of unprintable chars, warn the user about num dicarded
	 * - warn the user about input larger than MAX_PASTE which is discarded
	 * - if multiline paste, confirm the number of lines with the user
	 * - consider that input might be pasted into the middle of a line */

	confirm(send_paste, "Testing paste confirm. y/n? ");
}

static void
input_confirm(char *input, ssize_t len)
{
	/* Waiting for user confirmation */

	if (len == 1 && confirm_handler(*input)) {

		confirm_message = NULL;
		confirm_handler = NULL;

		draw(D_INPUT);
	}
}

void
confirm(int(*c_handler)(char), const char *fmt, ...)
{
	/* Set a confirmation for the user.
	 *
	 * The confirmation handler is then passed any future input, and is
	 * expected to return a non-zero value when the confirmation is resolved*/

	va_list ap;

	va_start(ap, fmt);
	vsnprintf(confirm_buff, MAX_CONFIRM, fmt, ap);
	va_end(ap);

	confirm_handler = c_handler;
	confirm_message = confirm_buff;

	draw(D_INPUT);
}

/*
 * Input line manipulation functions
 * */

static inline void
cursor_left(input *in)
{
	if (in->head > in->line->text)
		*(--in->tail) = *(--in->head);

	draw(D_INPUT);
}

static inline void
cursor_right(input *in)
{
	if (in->tail < in->line->text + MAX_INPUT)
		*(in->head++) = *(in->tail++);

	draw(D_INPUT);
}

static inline void
delete_left(input *in)
{
	if (in->head > in->line->text)
		in->head--;

	draw(D_INPUT);
}

static inline void
delete_right(input *in)
{
	if (in->tail < in->line->text + MAX_INPUT)
		in->tail++;

	draw(D_INPUT);
}

static inline void
scroll_up(input *in)
{
	reset_line(in);

	if (in->line->prev != in->list_head)
		in->line = in->line->prev;

	reframe_line(in);

	draw(D_INPUT);
}

static inline void
scroll_down(input *in)
{
	reset_line(in);

	if (in->line != in->list_head)
		in->line = in->line->next;

	reframe_line(in);

	draw(D_INPUT);
}

/*
 * Input line util functions
 * */

static inline void
reset_line(input *in)
{
	/* Reset the line's pointers such that new characters are inserted at the end */

	char *head = in->head,
		 *tail = in->tail,
		 *end  = in->line->text + MAX_INPUT;

	while (tail < end)
		*head++ = *tail++;

	in->line->end = head;
}

static inline void
reframe_line(input *in)
{
	/* Reframe the line content in the input bar window */

	in->head = in->line->end;
	in->tail = in->line->text + MAX_INPUT;
	in->window = in->head - (2 * w.ws_col / 3);

	if (in->window < in->line->text)
		in->window = in->line->text;
}

/*
 * Input sending functions
 * */

static int
send_paste(char c)
{
	/* TODO: Testing confirmation functions.
	 * This should send the paste if confirmed */

	if (c == 'y' || c == 'Y') {
		newlinef(ccur, 0, "Testing:", "got paste: y/Y");
		return 1;
	}

	if (c == 'n' || c == 'N') {
		newlinef(ccur, 0, "Testing:", "got paste: n/N");
		return 1;
	}

	return 0;
}

static void
send_input(void)
{
	/* TODO: cleanup */

	char sendbuff[BUFFSIZE];
	input *in = ccur->input;
	draw(D_INPUT);

	char *head = in->head, *tail = in->tail, *end = in->line->text + MAX_INPUT;

	/* Empty input */
	if (head == in->line->text && tail == end)
		return;

	while (tail < end)
		*head++ = *tail++;
	*head = '\0';
	in->line->end = head;

	/* strcpy is safe here since MAX_INPUT < BUFFSIZE */
	strcpy(sendbuff, in->line->text);

	if (in->line == in->list_head) {
		if (in->count < SCROLLBACK_INPUT)
			in->count++;
		else {
			input_line *t = in->list_head->next;
			in->list_head->next = t->next;
			t->next->prev = in->list_head;
			free(t);
		}
		in->line = in->list_head = new_input_line(in->list_head);
	} else {
		/* Resending an input scrollback */

		/* Move from list to head */
		in->line->next->prev = in->line->prev;
		in->line->prev->next = in->line->next;

		in->list_head->prev->next = in->line;
		in->line->prev = in->list_head->prev;
		in->list_head->prev = in->line;
		in->line->next = in->list_head;

		in->line = in->list_head;
	}
	in->head = in->line->text;
	in->tail = in->line->text + MAX_INPUT;
	in->window = in->line->text;

	send_mesg(sendbuff);
}
