#include <ctype.h>

#include "common.h"

int inp1 = 0;
int inp2 = MAXINPUT-1;

void cur_lr(int);
void del_char(int);
void ins_char(char);
void ready_send(void);
int esccmp(char*, char*);

void
ins_char(char c)
{
	if (inp1 < inp2)
		input_bar[inp1++] = c;
}

void
del_char(int left)
{
	if (left && inp1 > 0)
		inp1--;
	else if (!left && inp2 < MAXINPUT-1)
		inp2++;
}

void
cur_lr(int left)
{
	if (left && inp1 > 0)
		input_bar[--inp2] = input_bar[--inp1];
	else if (!left && inp2 < MAXINPUT-1)
		input_bar[inp1++] = input_bar[inp2++];
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
	if (inp1 == 0)
		return;
	while (inp2 < MAXINPUT-1)
		input_bar[inp1++] = input_bar[inp2++];
	input_bar[inp1] = '\0';
	send_mesg(input_bar);
	inp1 = window = 0;
	inp2 = MAXINPUT-1;
}

void
input(char *inp, int count)
{
	if (count == 1) {
		char c = *inp;
		if (isprint(c))
			ins_char(c);
		else if (c == 0x7F) /* backspace */
			del_char(1);
		else if (c == 0x0A) /* LF */
			ready_send();
		else if (c == 0x18) /* ctrl-X */
			channel_close();
	} else if (count > 0 && *inp == 0x1B) { /* escape sequence */
		inp++;
		if (esccmp("[A", inp)) /* arrow up */
			channel_sw(0); /* FIXME: testing channel switching */
		if (esccmp("[B", inp)) /* arrow down */
			channel_sw(1);
		if (esccmp("[C", inp)) /* arrow right */
			cur_lr(0);
		if (esccmp("[D", inp)) /* arrow left */
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
