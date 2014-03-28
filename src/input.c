#include <ctype.h>
#include <stdlib.h>

#include "common.h"

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
	if (back && in->input_line->prev)
		in->input_line = in->input_line->prev;
	else if (!back && in->input_line->next)
		in->input_line = in->input_line->next;

	in->head = in->input_line->end;
	in->tail = in->input_line->text + MAXINPUT;
	in->window = in->input_line->text - 15;
	if (in->window < in->input_line->text)
		in->window = in->input_line->text;
}

input_l*
new_inputl(input_l *prev)
{
	input_l *l;
	if ((l = malloc(sizeof(input_l))) == NULL)
		fatal("new_input");

	if (prev)
		prev->next = l;

	l->end = l->text;
	l->next = NULL;
	l->prev = prev;

	return l;
}

input*
new_input(void)
{
	input *i;
	if ((i = malloc(sizeof(input))) == NULL)
		fatal("new_input");

	i->count = 0;
	i->input_line = new_inputl(NULL);
	i->list_head = i->input_line;
	i->list_tail = i->input_line;
	i->head = i->input_line->text;
	i->tail = i->input_line->text + MAXINPUT;
	i->window = i->input_line->text;

	return i;
}

void
free_input(input *i)
{
	input_l *t, *l = i->list_head;
	do {
		t = l;
		l = l->prev;
		free(t);
	} while (l);
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
	if (left && in->head > in->input_line->text)
		in->head--;
	else if (!left && in->tail < in->input_line->text + MAXINPUT)
		in->tail++;
}

void
cur_lr(int left)
{
	if (left && in->head > in->input_line->text)
		*--in->tail = *--in->head;
	else if (!left && in->tail < in->input_line->text + MAXINPUT)
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
	char *head = in->head, *tail = in->tail, *end = in->input_line->text + MAXINPUT;

	if (head == in->input_line->text && tail == end)
		return;

	while (tail < end)
		*head++ = *tail++;
	*head = '\0';

	send_mesg(in->input_line->text);

	in->input_line->end = head;

	if (in->input_line == in->list_head) {
		if (in->count < SCROLLBACK_INPUT)
			in->count++;
		else {
			input_l *t = in->list_tail;
			in->list_tail = in->list_tail->next;
			in->list_tail->prev = NULL;
			free(t);
		}
		in->input_line = in->list_head = new_inputl(in->list_head);
	} else {
		in->input_line = in->list_head;
	}
	in->head = in->input_line->text;
	in->tail = in->input_line->text + MAXINPUT;
	in->window = in->input_line->text;
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
