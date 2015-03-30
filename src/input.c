/* input.c
 *
 * Input handling from stdin
 *
 * All input is handled synchronously and refers to the current
 * channel being drawn (ccur)
 *
 * A buffer input line consists of a doubly linked list of gap buffers
 *
 * Escape sequences are assumed to be ANSI. As such, you mileage may vary
 * */

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common.h"

/* Max number of characters accepted in user pasted input */
#define MAX_PASTE 2048

/* Max number of lines that can result from a paste */
#define MAX_PASTE_LINES 128

/* Max length of user action message */
#define MAX_ACTION_MESG 256

/* Defined in draw.c */
extern struct winsize w;

/* Static buffer that accepts input from stdin */
static char input_buff[MAX_PASTE];

/* Buffer to hold paste message while waiting for confirmation, includes room for \r\n */
static char paste_buff[MAX_INPUT + MAX_PASTE + (2 * MAX_PASTE_LINES)];
static size_t paste_len;

/* User input handlers */
static int input_char(char);
static void input_cchar(char);
static void input_cseq(char*, ssize_t);
static void input_paste(char*, ssize_t);
static void input_action(char*, ssize_t);

/* Action handling */
static int (*action_handler)(char);
static char action_buff[MAX_ACTION_MESG];

/* Confirmation handler when multi-line pastes are encountered */
static int action_send_paste(char);

/* Incremental channel search */
static int action_find_channel(char);

/* Send the current input to be parsed and handled */
static void send_input(void);

/* Input line manipulation functions */
static inline void cursor_left(input*);
static inline void cursor_right(input*);
static inline void delete_left(input*);
static inline void delete_right(input*);
static inline void input_scroll_up(input*);
static inline void input_scroll_down(input*);

/* Input line util functions */
static inline void reset_line(input*);
static inline void reframe_line(input*);

static input_line* new_input_line(input_line*);

input*
new_input(void)
{
	input *i;

	if ((i = calloc(1, sizeof(*i))) == NULL)
		fatal("calloc");

	/* DLL of input lines */
	i->list_head = i->line = new_input_line(NULL);

	/* Gap buffer pointers */
	i->head = i->line->text;
	i->tail = i->line->text + MAX_INPUT;

	i->window = i->line->text;

	return i;
}

void
free_input(input *i)
{
	/* Free an input and all of it's lines */

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
	/* Add a new line to an input's DLL */

	input_line *l;

	if ((l = calloc(1, sizeof(*l))) == NULL)
		fatal("calloc");

	DLL_ADD(prev, l);

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
	int timeout_ms = 200;

	struct pollfd stdin_fd[] = {{ .fd = STDIN_FILENO, .events = POLLIN }};

	if ((ret = poll(stdin_fd, 1, timeout_ms)) < 0 && errno != EINTR)
		fatal("poll");

	if (ret > 0) {

		ssize_t count;

		if ((count = read(STDIN_FILENO, input_buff, MAX_PASTE)) < 0 && errno != EINTR)
			fatal("read");

		if (count == 0)
			fatal("stdin closed");

		/* Waiting for user action, ignore everything else */
		 if (action_message)
			input_action(input_buff, count);

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

static int
input_char(char c)
{
	/* Input a single character */

	if (ccur->input->head >= ccur->input->tail)
		/* Gap buffer is full */
		return 0;

	*ccur->input->head++ = c;

	draw(D_INPUT);

	return 1;
}

static void
input_cchar(char c)
{
	/* Input a single byte control character */

	switch (c) {

		/* Backspace */
		case 0x7F:
			delete_left(ccur->input);
			break;

		/* Line feed */
		case 0x0A:
			send_input();
			break;

		/* ^F */
		case 0x06:
			/* Find channel */
			if (ccur->server)
				action(action_find_channel, "Find: ");
			break;

		/* ^L */
		case 0x0C:
			/* Clear current channel */
			clear_channel(ccur);
			break;

		/* ^N */
		case 0x10:
			/* Go to next channel */
			ccur = channel_switch(ccur, 0);
			break;

		/* ^P */
		case 0x0E:
			/* Go to previous channel */
			ccur = channel_switch(ccur, 1);
			break;

		/* ^X */
		case 0x18:
			/* Close current channel */
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
		input_scroll_up(ccur->input);

	/* arrow down */
	else if (!strncmp(input, "[B", len))
		input_scroll_down(ccur->input);

	/* arrow right */
	else if (!strncmp(input, "[C", len))
		cursor_right(ccur->input);

	/* arrow left */
	else if (!strncmp(input, "[D", len))
		cursor_left(ccur->input);

	/* delete */
	else if (!strncmp(input, "[3~", len))
		delete_right(ccur->input);

	/* FIXME: scrollback disabled while it's reworked */
#if 0
	/* page up */
	else if (!strncmp(input, "[5~", len))
		buffer_scrollback_page(ccur, 1);

	/* page down */
	else if (!strncmp(input, "[6~", len))
		buffer_scrollback_page(ccur, 0);

	/* mousewheel up */
	else if (!strncmp(input, "[M`", len))
		buffer_scrollback_line(ccur, 1);

	/* mousewheel down */
	else if (!strncmp(input, "[Ma", len))
		buffer_scrollback_line(ccur, 0);
#endif
}

static void
input_paste(char *paste, ssize_t len)
{
	/* Input pasted text and render a buffer of messages that will be sent if confirmed */

	/* If first character is /, assume a command is being entered:
	 *  - don't input more than a single input line will accept
	 *  - don't automatically send anything
	 */
	if (*ccur->input->line->text == '/') {

		while (len && input_char(*paste))
			len--, paste++;

		return;
	}

	/* Determine how many lines would be required for the paste, confirm with user
	 *
	 * Where each message will be:
	 *   `PRIVMESG <target> :<mesg>\r\n`
	 */

	/* Max number of characters per message that can be sent to this target */
	size_t max_len = BUFFSIZE - strlen("PRIVMESG  :\r\n") - strlen(ccur->name);

	/* Get the number of characters currently on the input line's gap buffer */
	size_t input_len = (ccur->input->head - ccur->input->line->text)
		+ (MAX_INPUT - (ccur->input->tail - ccur->input->line->text));

	/* If there are no \n characters in the paste and (head + paste + tail) fits in one
	 * input line, insert the paste and skip the rest of the processing */
	if ((input_len + len) <= max_len && !strchr(paste, '\n')) {

		while (len && input_char(*paste))
			len--, paste++;

		return;
	}

	/* Else render the paste buffer, count lines, confirm with user
	 *
	 * Since paste might be inserted into the middle of the line, copy the input
	 * head, then scan and copy the paste, then copy the tail
	 *
	 * The paste body should be scanned for \n, the head and tail can be assumed to contain none
	 */
	char *input_ptr, *paste_ptr = paste_buff;

	int line_count = 1;

	/* Copy the input head to the paste buffer */
	for (input_ptr = ccur->input->line->text; input_ptr < ccur->input->head; input_ptr++)
		*paste_ptr++ = *input_ptr;

	/* Initial length is the input head's count */
	size_t line_len = ccur->input->head - ccur->input->line->text;

	/* Scan the paste for \n characters and count the length, if \n is encountered
	 * or the max number of characters is encountered, insert message terminator */
	for (input_ptr = paste; input_ptr < &paste[len]; input_ptr++) {

		if (*input_ptr == '\n' || line_len == max_len) {

			/* Dedupe \n characters to avoid sending empty lines */
			if (*paste_ptr == '\n')
				continue;

			line_count++;

			*paste_ptr++ = '\r';
			*paste_ptr++ = '\n';

			line_len = 0;

			if (line_count == MAX_PASTE_LINES) {
				newlinef(ccur, 0, "-!!-",
						"MAX_PASTE_LINES (%d) encountered, discarding excess", MAX_PASTE_LINES);

				/* Goto send_paste to avoid adding the tail to the paste */
				goto send_paste;
			}
		} else {
			*paste_ptr++ = *input_ptr;
		}
	}

	/* Copy the input tail to the paste buffer
	 *
	 * Since line_count is at least < MAX_PASTE_LINES there will always  be enough
	 * for the tail, though it may be be split at least once more
	 */
	for (input_ptr = ccur->input->tail; input_ptr < ccur->input->head; input_ptr++) {

		if (line_len == max_len) {
			line_count++;

			*paste_ptr++ = '\r';
			*paste_ptr++ = '\n';

			line_len = 0;
		}

		*paste_ptr++ = *input_ptr;
		line_len++;
	}

send_paste:

	/* Store the paste length */
	paste_len = paste_ptr - paste_buff;

	/* Confirm sending the paste */
	action(action_send_paste, "Confirm sending %d lines? [y/n]", line_count);
}

static void
input_action(char *input, ssize_t len)
{
	/* Waiting for user confirmation */

	if (len == 1 && action_handler(*input)) {

		action_message = NULL;
		action_handler = NULL;

		draw(D_INPUT);
	}
}

void
action(int (*a_handler)(char), const char *fmt, ...)
{
	/* Begin a user action
	 *
	 * The action handler is then passed any future input, and is
	 * expected to return a non-zero value when the action is resolved
	 * */

	va_list ap;

	va_start(ap, fmt);
	vsnprintf(action_buff, MAX_ACTION_MESG, fmt, ap);
	va_end(ap);

	action_handler = a_handler;
	action_message = action_buff;

	draw(D_INPUT);
}

/*
 * Input line manipulation functions
 * */

static inline void
cursor_left(input *in)
{
	/* Move the cursor left */

	if (in->head > in->line->text)
		*(--in->tail) = *(--in->head);

	draw(D_INPUT);
}

static inline void
cursor_right(input *in)
{
	/* Move the cursor right */

	if (in->tail < in->line->text + MAX_INPUT)
		*(in->head++) = *(in->tail++);

	draw(D_INPUT);
}

static inline void
delete_left(input *in)
{
	/* Delete the character left of the cursor */

	if (in->head > in->line->text)
		in->head--;

	draw(D_INPUT);
}

static inline void
delete_right(input *in)
{
	/* Delete the character right of the cursor */

	if (in->tail < in->line->text + MAX_INPUT)
		in->tail++;

	draw(D_INPUT);
}

static inline void
input_scroll_up(input *in)
{
	/* Scroll up through the input history */

	reset_line(in);

	if (in->line->prev != in->list_head)
		in->line = in->line->prev;

	reframe_line(in);

	draw(D_INPUT);
}

static inline void
input_scroll_down(input *in)
{
	/* Scroll down through the input history */

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
	/* Reset a line's gap buffer pointers such that new chars are inserted at the gap head */

	char *h_tmp = in->head, *t_tmp = in->tail;

	while (t_tmp < (in->line->text + MAX_INPUT))
		*h_tmp++ = *t_tmp++;

	in->line->end = h_tmp;
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

/* TODO: This is a first draft for simple channel searching functionality.
 *
 * It can be cleaned up, and input.c is probably not the most ideal place for this */
#define MAX_SEARCH 128
channel *search_cptr; /* Used for iterative searching, before setting ccur */
static char search_buff[MAX_SEARCH];
static char *search_ptr = search_buff;

static channel* search_channels(channel*, char*);
static channel*
search_channels(channel *start, char *search)
{
	if (start == NULL || *search == '\0')
		return NULL;

	/* Start the search one past the input */
	channel *c = channel_switch(start, 1);

	while (c != start) {

		if (strstr(c->name, search))
			return c;

		c = channel_switch(c, 1);
	}

	return NULL;
}

/* Action line should be:
 *
 *
 * Find: [current result]/[(server if not current server[socket if not 6667])] : <search input> */
static int
action_find_channel(char c)
{
	/* Incremental channel search */

	/* \n confirms selecting the current match */
	if (c == '\n' && search_cptr) {
		*(search_ptr = search_buff) = '\0';
		ccur = search_cptr;
		search_cptr = NULL;
		draw(D_FULL);
		return 1;
	}

	/* \n, Esc, ^C cancels a search if no results are found */
	if (c == '\n' || c == 0x1b || c == 0x03) {
		*(search_ptr = search_buff) = '\0';
		return 1;
	}

	/* ^F repeats the search forward from the current result,
	 * or resets search criteria if no match */
	if (c == 0x06) {
		if (search_cptr == NULL) {
			*(search_ptr = search_buff) = '\0';
			action(action_find_channel, "Find: ");
			return 0;
		}

		search_cptr = search_channels(search_cptr, search_buff);
	} else if (c == 0x7f && search_ptr > search_buff) {
		/* Backspace */

		*(--search_ptr) = '\0';

		search_cptr = search_channels(ccur, search_buff);
	} else if (isprint(c) && search_ptr < search_buff + MAX_SEARCH && (search_cptr != NULL || *search_buff == '\0')) {
		/* All other input */

		*(search_ptr++) = c;
		*search_ptr = '\0';

		search_cptr = search_channels(ccur, search_buff);
	}

	/* Reprint the action message */
	if (search_cptr == NULL) {
		if (*search_buff)
			action(action_find_channel, "Find: NO MATCH -- %s", search_buff);
		else
			action(action_find_channel, "Find: ");
	} else {
		/* Found a channel */
		if (search_cptr->server == ccur->server) {
			action(action_find_channel, "Find: %s -- %s",
					search_cptr->name, search_buff);
		} else {
			if (!strcmp(search_cptr->server->port, "6667"))
				action(action_find_channel, "Find: %s/%s -- %s",
						search_cptr->server->host, search_cptr->name, search_buff);
			else
				action(action_find_channel, "Find: %s:%s/%s -- %s",
						search_cptr->server->host, search_cptr->server->port,
						search_cptr->name, search_buff);
		}
	}

	return 0;
}

/*
 * Input sending functions
 * */

static int
action_send_paste(char c)
{
	/* Confirmed send */
	if (toupper(c) == 'Y') {
		send_paste(paste_buff);
		return 1;
	}

	/* Confirmed don't send */
	if (toupper(c) == 'N')
		return 1;

	/* Do nothing */
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
