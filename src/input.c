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

//TODO: complete rewrite,
// line->end is not properly set in a lot of cases,
// should be rewritten with a better thought out design

#include <ctype.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "input.h"
#include "state.h"
#include "utils.h"

/* Max length of user action message */
#define MAX_ACTION_MESG 256

/* List of common IRC commands for tab completion */
static char* irc_commands[] = {
	"admin",    "away",     "clear",   "close",
	"connect",  "ctcp",     "die",     "disconnect",
	"encap",    "help",     "ignore",  "info",
	"invite",   "ison",     "join",    "kick",
	"kill",     "knock",    "links",   "list",
	"lusers",   "me",       "mode",    "motd",
	"msg",      "names",    "namesx",  "nick",
	"notice",   "oper",     "part",    "pass",
	"privmsg",  "quit",     "raw",     "rehash",
	"restart",  "rules",    "server",  "service",
	"servlist", "setname",  "silence", "squery",
	"squit",    "stats",    "summon",  "time",
	"topic",    "trace",    "uhnames", "unignore",
	"user",     "userhost", "userip",  "users",
	"version",  "wallops",  "watch",   "who",
	"whois",    "whowas",
	NULL
};

char *action_message;

/* User input handlers */
static int input_char(char);
static void input_cchar(char*);
static void input_action(char*, ssize_t);

/* Action handling */
static int (*action_handler)(char);
static char action_buff[MAX_ACTION_MESG];

/* Incremental channel search */
static int action_find_channel(char);

/* Case insensitive tab complete for commands and nicks */
static void tab_complete_command(struct input*, char*, size_t);
static void tab_complete_nick(struct input*, char*, size_t);
static void tab_complete(struct input*);

/* Send the current input to be parsed and handled */
static void send_input(void);

/* Input line manipulation functions */
static inline void cursor_left(struct input*);
static inline void cursor_right(struct input*);
static inline void delete_left(struct input*);
static inline void delete_right(struct input*);
static inline void input_scroll_backwards(struct input*);
static inline void input_scroll_forwards(struct input*);

/* Input line util functions */
static inline void reset_line(struct input*);
static inline void reframe_line(struct input*);

static void new_list_head(struct input*);

//TODO: struct input input(struct input*)
struct input*
new_input(void)
{
	struct input *i;

	if ((i = calloc(1, sizeof(*i))) == NULL)
		fatal("calloc", errno);

	new_list_head(i);

	return i;
}

void
free_input(struct input *i)
{
	/* Free an input and all of it's lines */

	struct input_line *t, *l = i->list_head;

	do {
		t = l;
		l = l->next;
		free(t);
	} while (l != i->list_head);

	free(i);
}

static void
new_list_head(struct input *i)
{
	/* Append a new line as the list_head */

	struct input_line *l;

	if ((l = calloc(1, sizeof(*l))) == NULL)
		fatal("calloc", errno);

	DLL_ADD(i->list_head, l);

	i->line = i->list_head = l;

	/* Gap buffer pointers */
	i->head = l->text;
	i->tail = l->text + MAX_INPUT;

	i->window = l->text;
}

void
poll_input(void)
{
	/* Poll stdin for user input, checking for control character
	 * or escape sequence. Otherwise copy all characters to the
	 * input struct of the current context
	 */

	int ret, timeout_ms = 200;

	char *inp_ptr, inp_buff[MAX_INPUT + 1];

	struct pollfd stdin_fd[] = {{ .fd = STDIN_FILENO, .events = POLLIN }};

	if ((ret = poll(stdin_fd, 1, timeout_ms)) < 0 && errno != EINTR)
		fatal("poll", errno);

	if (ret > 0) {

		ssize_t count;

		while ((count = read(STDIN_FILENO, inp_buff, MAX_INPUT)) < 0)
			if (errno != EINTR)
				fatal("read", errno);

		if (count == 0)
			fatal("stdin closed", 0);

		*(inp_buff + count) = '\0';

		if (action_message)
			input_action(inp_buff, count);

		else if (iscntrl(*inp_buff))
			input_cchar(inp_buff);

		else for (inp_ptr = inp_buff; *inp_ptr && input_char(*inp_ptr); inp_ptr++)
			;
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

	draw_input();

	return 1;
}

static void
input_cchar(char *c)
{
	/* Input a control character or escape sequence */

	/* ESC begins a key sequence */
	if (*c == 0x1b) {

		c++;

		if (*c == 0)
			return;

		/* arrow up */
		else if (!strcmp(c, "[A"))
			input_scroll_backwards(ccur->input);

		/* arrow down */
		else if (!strcmp(c, "[B"))
			input_scroll_forwards(ccur->input);

		/* arrow right */
		else if (!strcmp(c, "[C"))
			cursor_right(ccur->input);

		/* arrow left */
		else if (!strcmp(c, "[D"))
			cursor_left(ccur->input);

		/* delete */
		else if (!strcmp(c, "[3~"))
			delete_right(ccur->input);

		/* page up */
		else if (!strcmp(c, "[5~"))
			buffer_scrollback_back(ccur);

		/* page down */
		else if (!strcmp(c, "[6~"))
			buffer_scrollback_forw(ccur);

	} else switch (*c) {

		/* Backspace */
		case 0x7F:
			delete_left(ccur->input);
			break;

		/* Horizontal tab */
		case 0x09:
			tab_complete(ccur->input);
			break;

		/* Line feed */
		case 0x0A:
			send_input();
			break;

		/* ^C */
		case 0x03:
			/* Cancel current input */
			ccur->input->head = ccur->input->line->text;
			ccur->input->tail = ccur->input->line->text + MAX_INPUT;
			ccur->input->window = ccur->input->line->text;
			draw_input();
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
			channel_clear(ccur);
			break;

		/* ^P */
		case 0x10:
			/* Go to previous channel */
			channel_move_prev();
			break;

		/* ^N */
		case 0x0E:
			/* Go to next channel */
			channel_move_next();
			break;

		/* ^X */
		case 0x18:
			/* Close current channel */
			channel_close(ccur);
			break;

		/* ^U */
		case 0x15:
			/* Scoll buffer up */
			buffer_scrollback_back(ccur);
			break;

		/* ^D */
		case 0x04:
			/* Scoll buffer down */
			buffer_scrollback_forw(ccur);
			break;
	}
}

static void
input_action(char *input, ssize_t len)
{
	/* Waiting for user confirmation */

	if (len == 1 && (*input == 0x03 || action_handler(*input))) {
		/* ^C canceled the action, or the action was resolved */

		action_message = NULL;
		action_handler = NULL;

		draw_input();
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

	draw_input();
}

/*
 * Input line manipulation functions
 * */

static inline void
cursor_left(struct input *in)
{
	/* Move the cursor left */

	if (in->head > in->line->text)
		*(--in->tail) = *(--in->head);

	draw_input();
}

static inline void
cursor_right(struct input *in)
{
	/* Move the cursor right */

	if (in->tail < in->line->text + MAX_INPUT)
		*(in->head++) = *(in->tail++);

	draw_input();
}

static inline void
delete_left(struct input *in)
{
	/* Delete the character left of the cursor */

	if (in->head > in->line->text)
		in->head--;

	draw_input();
}

static inline void
delete_right(struct input *in)
{
	/* Delete the character right of the cursor */

	if (in->tail < in->line->text + MAX_INPUT)
		in->tail++;

	draw_input();
}

static inline void
input_scroll_backwards(struct input *in)
{
	/* Scroll backwards through the input history */

	/* Scrolling backwards on the last line */
	if (in->line->prev == in->list_head)
		return;

	reset_line(in);

	in->line = in->line->prev;

	reframe_line(in);

	draw_input();
}

static inline void
input_scroll_forwards(struct input *in)
{
	/* Scroll forwards through the input history */

	/* Scrolling forward on the first line */
	if (in->line == in->list_head)
		return;

	reset_line(in);

	in->line = in->line->next;

	reframe_line(in);

	draw_input();
}

/*
 * Input line util functions
 * */

static inline void
reset_line(struct input *in)
{
	/* Reset a line's gap buffer pointers such that new chars are inserted at the gap head */

	char *h_tmp = in->head, *t_tmp = in->tail;

	while (t_tmp < (in->line->text + MAX_INPUT))
		*h_tmp++ = *t_tmp++;

	*h_tmp = '\0';

	in->line->end = h_tmp;
}

static inline void
reframe_line(struct input *in)
{
	/* Reframe a line's draw window */

	in->head = in->line->end;
	in->tail = in->line->text + MAX_INPUT;
	in->window = in->head - (2 * _term_cols() / 3);

	if (in->window < in->line->text)
		in->window = in->line->text;
}

/* TODO: This is a first draft for simple channel searching functionality.
 *
 * It can be cleaned up, and input.c is probably not the most ideal place for this */
#define MAX_SEARCH 128
struct channel *search_cptr; /* Used for iterative searching, before setting ccur */
static char search_buff[MAX_SEARCH];
static char *search_ptr = search_buff;

static struct channel* search_channels(struct channel*, char*);
static struct channel*
search_channels(struct channel *start, char *search)
{
	if (start == NULL || *search == '\0')
		return NULL;

	/* Start the search one past the input */
	struct channel *c = channel_get_next(start);

	while (c != start) {

		if (strstr(c->name, search))
			return c;

		c = channel_get_next(c);
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
		channel_set_current(search_cptr);
		search_cptr = NULL;
		draw_all();
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

void
tab_complete(struct input *inp)
{
	/* Case insensitive tab complete for commands and nicks */

	char *str = inp->head;
	size_t len = 0;

	/* Don't tab complete at beginning of line or if previous character is space */
	if (inp->head == inp->line->text || *(inp->head - 1) == ' ')
		return;

	/* Don't tab complete if cursor is scrolled left and next character isn't space */
	if (inp->tail < (inp->line->text + MAX_INPUT) && *inp->tail != ' ')
		return;

	/* Scan backwards for the point to tab complete from */
	while (str > inp->line->text && *(str - 1) != ' ')
		len++, str--;

	if (*str == '/')
		tab_complete_command(inp, ++str, --len);
	else
		tab_complete_nick(inp, ++str, --len);
}

static void
tab_complete_command(struct input *inp, char *str, size_t len)
{
	/* Command tab completion */

	char *p, **command = irc_commands;

	while (*command && strncmp(*command, str, len))
		command++;

	if (command) {

		p = *command;

		/* Case insensitive matching, delete prefix */
		while (len--)
			delete_left(inp);

		while (*p && input_char(*p++))
			;

		input_char(' ');
	}
}

static void
tab_complete_nick(struct input *inp, char *str, size_t len)
{
	/* Nick tab completion */

	char *p;

	struct user *u;

	if ((u = user_list_get(&(ccur->users), str, len))) {

		p = u->nick;

		/* Case insensitive matching, delete prefix */
		while (len--)
			delete_left(inp);

		while (*p && input_char(*p++))
			;

		/* Tab completing first word in input, append delimiter and space */
		if (str == inp->line->text) {
			input_char(TAB_COMPLETE_DELIMITER);
			input_char(' ');
		}
	}
}

/*
 * Input sending functions
 * */

/* TODO: pass channel into this function too */
static void
send_input(void)
{
	char sendbuff[BUFFSIZE];

	struct input *in = ccur->input;

	/* Before sending, copy the tail of the gap buffer back to the head */
	reset_line(in);

	/* After resetting, check for empty line */
	if (in->head == in->line->text)
		return;

	/* Pass a copy of the message to the send handler, since it may modify the contents */
	strcpy(sendbuff, in->line->text);

	/* Now check if the sent line was 'new' or was resent input scrollback
	 *
	 * If a new line was sent:
	 *   append a blank input as the new list head
	 *
	 * If scrollback was sent:
	 *   move it to the front of the input scrollback
	 */
	if (in->line == in->list_head) {

		if (in->count == SCROLLBACK_INPUT) {
			/* Reached maximum input scrollback lines, delete the tail */
			struct input_line *t = in->list_head->next;

			DLL_DEL(in->list_head, t);
			free(t);
		} else {
			in->count++;
		}

		new_list_head(in);
	} else {
		/* TODO: the DLL macros don't seem to handle this task well...
		 * Maybe they should be replaced with generic DLL functions in utils */

		in->line->next->prev = in->line->prev;
		in->line->prev->next = in->line->next;

		in->list_head->prev->next = in->line;
		in->line->prev = in->list_head->prev;
		in->list_head->prev = in->line;
		in->line->next = in->list_head;

		in->line = in->list_head;

		reframe_line(in);
	}

	draw_input();

	/* Send the message last; the channel might be closed as a result of the command */
	send_mesg(sendbuff, ccur);
}
