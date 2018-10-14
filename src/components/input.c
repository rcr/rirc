/* TODO:
 * complete rewrite and unit test
 */

#include <string.h>

#include "src/components/input.h"
#include "src/state.h"
#include "src/utils/utils.h"

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

/* User input handlers */
static int input_char(struct input*, char);

/* Case insensitive tab complete for commands and nicks */
static int tab_complete_command(struct input*, char*, size_t);
static int tab_complete_nick(struct input*, struct user_list*, char*, size_t);

/* Input line util functions */
static inline void reset_line(struct input*);
static inline void reframe_line(struct input*, unsigned int);

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

/* TODO: ideally inputs shouldnt require being freed */
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
	i->tail = l->text + RIRC_MAX_INPUT;

	i->window = l->text;
}

int
input(struct input *inp, const char *buff, size_t count)
{
	/* Handle input, checking for control character or escape
	 * sequence. Otherwise copy all characters to the input struct
	 * of the current context */

	size_t start = count;

	while (count && input_char(inp, *buff)) {
		buff++;
		count--;
	}

	/* returns zero if no characters added */
	return (start != count);
}

/*
 * User input handlers
 * */

static int
input_char(struct input *inp, char c)
{
	/* Input a single character */

	if (inp->head >= inp->tail)
		/* Gap buffer is full */
		return 0;

	*inp->head++ = c;
	return 1;
}

/*
 * Input line manipulation functions
 * */

int
cursor_left(struct input *in)
{
	/* Move the cursor left */

	if (in->head > in->line->text) {
		*(--in->tail) = *(--in->head);
		return 1;
	}

	return 0;
}

int
cursor_right(struct input *in)
{
	/* Move the cursor right */

	if (in->tail < in->line->text + RIRC_MAX_INPUT) {
		*(in->head++) = *(in->tail++);
		return 1;
	}

	return 0;
}

int
delete_left(struct input *in)
{
	/* Delete the character left of the cursor */

	if (in->head > in->line->text) {
		in->head--;
		return 1;
	}

	return 0;
}

int
delete_right(struct input *in)
{
	/* Delete the character right of the cursor */

	if (in->tail < in->line->text + RIRC_MAX_INPUT) {
		in->tail++;
		return 1;
	}

	return 0;
}

int
input_scroll_back(struct input *in, unsigned int cols)
{
	/* Scroll backwards through the input history */

	/* Scrolling backwards on the last line */
	if (in->line->prev == in->list_head)
		return 0;

	reset_line(in);
	in->line = in->line->prev;
	reframe_line(in, cols);

	return 1;
}

int
input_scroll_forw(struct input *in, unsigned int cols)
{
	/* Scroll forwards through the input history */

	/* Scrolling forward on the first line */
	if (in->line == in->list_head)
		return 0;

	reset_line(in);
	in->line = in->line->next;
	reframe_line(in, cols);

	return 1;
}

/*
 * Input line util functions
 * */

static inline void
reset_line(struct input *in)
{
	/* Reset a line's gap buffer pointers such that new chars are inserted at the gap head */

	char *h_tmp = in->head, *t_tmp = in->tail;

	while (t_tmp < (in->line->text + RIRC_MAX_INPUT))
		*h_tmp++ = *t_tmp++;

	*h_tmp = '\0';

	in->line->end = h_tmp;
}

static inline void
reframe_line(struct input *in, unsigned int cols)
{
	/* Reframe a line's draw window */

	in->head = in->line->end;
	in->tail = in->line->text + RIRC_MAX_INPUT;
	in->window = in->head - (2 * cols / 3);

	if (in->window < in->line->text)
		in->window = in->line->text;
}

int
tab_complete(struct input *inp, struct user_list *ul)
{
	/* Case insensitive tab complete for commands and nicks */

	char *str = inp->head;
	size_t len = 0;

	/* Don't tab complete at beginning of line or if previous character is space */
	if (inp->head == inp->line->text || *(inp->head - 1) == ' ')
		return 0;

	/* Don't tab complete if cursor is scrolled left and next character isn't space */
	if (inp->tail < (inp->line->text + RIRC_MAX_INPUT) && *inp->tail != ' ')
		return 0;

	/* Scan backwards for the point to tab complete from */
	while (str > inp->line->text && *(str - 1) != ' ')
		len++, str--;

	if (str == inp->line->text && *str == '/')
		return tab_complete_command(inp, ++str, --len);
	else
		return tab_complete_nick(inp, ul, str, len);
}

static int
tab_complete_command(struct input *inp, char *str, size_t len)
{
	/* Command tab completion */

	char *p, **command = irc_commands;

	while (*command && strncmp(*command, str, len))
		command++;

	if (*command) {

		p = *command;

		/* Case insensitive matching, delete prefix */
		while (len--)
			delete_left(inp);

		while (*p && input_char(inp, *p++))
			;

		input_char(inp, ' ');

		return 1;
	} else {
		return 0;
	}
}

static int
tab_complete_nick(struct input *inp, struct user_list *ul, char *str, size_t len)
{
	/* Nick tab completion */

	const char *p;

	struct user *u;

	if ((u = user_list_get(ul, str, len))) {

		p = u->nick.str;

		/* Case insensitive matching, delete prefix */
		while (len--)
			delete_left(inp);

		while (*p && input_char(inp, *p++))
			;

		/* Tab completing first word in input, append delimiter and space */
		if (str == inp->line->text) {
			input_char(inp, TAB_COMPLETE_DELIMITER);
			input_char(inp, ' ');
		}

		return 1;
	} else {
		return 0;
	}
}

/*
 * Input sending functions
 * */

void
_send_input(struct input *in, char *buf)
{
	/* FIXME: refactoring WIP */

	/* Before sending, copy the tail of the gap buffer back to the head */
	reset_line(in);

	/* Pass a copy of the message to the send handler, since it may modify the contents */
	strcpy(buf, in->line->text);

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
	}
}

int
input_empty(struct input *in)
{
	/* FIXME: if cursor is 0 this is wrong */
	return (in->head == in->line->text);
}
