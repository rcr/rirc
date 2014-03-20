#include <ctype.h>

#include "common.h"

extern channel *ccur;
input *in;

void cur_lr(int);
void del_char(int);
void ins_char(char);
void ready_send(void);
int esccmp(char*, char*);

void
ins_char(char c)
{
	if (in->head < in->tail)
		*in->head++ = c;
}

void
del_char(int left)
{
	if (left && in->head > in->text)
		in->head--;
	else if (!left && in->tail < in->text + MAXINPUT)
		in->tail++;
}

void
cur_lr(int left)
{
	if (left && in->head > in->text)
		*--in->tail = *--in->head;
	else if (!left && in->tail < in->text + MAXINPUT)
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
	char *head = in->head, *tail = in->tail, *end = in->text + MAXINPUT;

	if (head == in->text && tail == end)
		return;

	while (tail < end)
		*head++ = *tail++;
	*head = '\0';

	send_mesg(in->text);

	in->head = in->text;
	in->tail = in->text + MAXINPUT;
	in->window = in->text;
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
			channel_sw(0);
		if (esccmp("[B", inp))  /* arrow down */
			channel_sw(1);
		if (esccmp("[C", inp))  /* arrow right */
			cur_lr(0);
		if (esccmp("[D", inp))  /* arrow left */
			cur_lr(1);
		if (esccmp("[3~", inp)) /* delete */
			del_char(0);
	} else {
		;
		/* Got paste, confirm length with user, call warn_paste(int)
		 * then:
		 * while count-- , inp, send if max length */
	}
	draw_input();
}
