#include <ctype.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "common.h"

extern struct winsize w;
input *in;

void cur_lr(int);
void del_char(int);
void ins_char(char);
void ready_send(void);
void scroll_input(int);
int esccmp(char*, char*);

void
scroll_input(int back)
{
	/* Reset the line */
	char *head = in->head, *tail = in->tail, *end = in->line->text + MAXINPUT;
	while (tail < end)
		*head++ = *tail++;
	*head = '\0';
	in->line->end = head;

	if (back && in->line->prev != in->list_head)
		in->line = in->line->prev;
	else if (!back && in->line != in->list_head)
		in->line = in->line->next;

	in->head = in->line->end;
	in->tail = in->line->text + MAXINPUT;
	in->window = in->head - (2 * w.ws_col / 3);
	if (in->window < in->line->text)
		in->window = in->line->text;
}

input_l*
new_inputl(input_l *prev)
{
	input_l *l;
	if ((l = malloc(sizeof(input_l))) == NULL)
		fatal("new_input");

	l->end = l->text;
	l->prev = prev ? prev : l;
	l->next = prev ? prev->next : l;

	if (prev)
		prev->next = prev->next->prev = l;

	return l;
}

input*
new_input(void)
{
	input *i;
	if ((i = malloc(sizeof(input))) == NULL)
		fatal("new_input");

	i->count = 0;
	i->line = new_inputl(NULL);
	i->list_head = i->line;
	i->head = i->line->text;
	i->tail = i->line->text + MAXINPUT;
	i->window = i->line->text;

	return i;
}

void
free_input(input *i)
{
	input_l *t, *l = i->list_head;
	do {
		t = l;
		l = l->next;
		free(t);
	} while (l != i->list_head);
	free(i);
}

void
ins_char(char c)
{
	if (in->head < in->tail)
		*in->head++ = c;
}

void
del_char(int left)
{
	if (left && in->head > in->line->text)
		in->head--;
	else if (!left && in->tail < in->line->text + MAXINPUT)
		in->tail++;
}

void
cur_lr(int left)
{
	if (left && in->head > in->line->text)
		*--in->tail = *--in->head;
	else if (!left && in->tail < in->line->text + MAXINPUT)
		*in->head++ = *in->tail++;
}

int
esccmp(char *esc, char *inp)
{
	while (*esc++ == *inp++)
		if (*esc == '\0') return 1;
	return 0;
}

void
ready_send(void)
{
	char *head = in->head, *tail = in->tail, *end = in->line->text + MAXINPUT;

	if (head == in->line->text && tail == end)
		return;

	while (tail < end)
		*head++ = *tail++;
	*head = '\0';
	in->line->end = head;

	send_mesg(in->line->text);


	if (in->line == in->list_head) {
		if (in->count < SCROLLBACK_INPUT)
			in->count++;
		else {
			input_l *t = in->list_head->next;
			in->list_head->next = t->next;
			t->next->prev = in->list_head;
			free(t);
		}
		in->line = in->list_head = new_inputl(in->list_head);
	} else {
		/* Remove from list */
		in->line->next->prev = in->line->prev;
		in->line->prev->next = in->line->next;

		/* Insert second from head */
		in->list_head->prev->next = in->line;
		in->line->prev = in->list_head->prev;
		in->list_head->prev = in->line;
		in->line->next = in->list_head;

		/* Reset */
		in->line = in->list_head;
	}
	in->head = in->line->text;
	in->tail = in->line->text + MAXINPUT;
	in->window = in->line->text;
}

void
inputc(char *inp, int count)
{
	in = ccur->input;

	if (count == 1) {
		char c = *inp;
		if (isprint(c))
			ins_char(c);
		else if (c == 0x7F) /* backspace */
			del_char(1);
		else if (c == 0x0A) /* LF */
			ready_send();
		else if (c == 0x18) /* ctrl-x */
			channel_close();
	} else if (count > 0 && *inp == 0x1B) { /* escape sequence */
		inp++;
		if (esccmp("[A", inp))  /* arrow up */
			scroll_input(1);
		if (esccmp("[B", inp))  /* arrow down */
			scroll_input(0);
		if (esccmp("[C", inp))  /* arrow right */
			cur_lr(0);
		if (esccmp("[D", inp))  /* arrow left */
			cur_lr(1);
		if (esccmp("[3~", inp)) /* delete */
			del_char(0);
		if (esccmp("[5~", inp)) /* page up */
			channel_sw(0);
		if (esccmp("[6~", inp)) /* page down */
			channel_sw(1);
	} else {
		;
		/* Got paste, confirm length with user, call warn_paste(int)
		 * then:
		 * while count-- , inp, send if max length */
	}
	draw_input();
}
