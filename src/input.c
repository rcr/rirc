#include <ctype.h>

#include "common.h"

#define SENDBUFF MAXINPUT+3 /* Allow room for \r\n\0 */

char text[SENDBUFF];
int ptr1 = 0;
int ptr2 = MAXINPUT-1;

void cur_lr(int);
void del_char(int);
void ins_char(char);
void ready_send(void);
int esccmp(char*, char*);

void
ins_char(char c)
{
	if (ptr1 < ptr2)
		text[ptr1++] = c;
}

void
del_char(int left)
{
	if (left && ptr1 > 0)
		ptr1--;
	else if (!left && ptr2 < MAXINPUT-1)
		ptr2++;
}

void
cur_lr(int left)
{
	if (left && ptr1 > 0)
		text[--ptr2] = text[--ptr1];
	else if (!left && ptr2 < MAXINPUT-1)
		text[++ptr1] = text[++ptr2];
}

int
esccmp(char *esc, char *inp)
{
	while (*esc++ == *inp++)
		if (*esc == '\0') return 1;
	return 0;
}

void
ready_send()
{
	if (ptr1 == 0)
		return;
	while (ptr2 < MAXINPUT-1)
		text[ptr1++] = text[ptr2++];
	text[ptr1] = '\0';
	send_msg(text, ptr1-1);
	ptr1 = 0;
	ptr2 = MAXINPUT-1;
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
	} else if (count > 0 && *inp++ == 0x1B) { /* escape sequence */
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
		//printf("\n\n\n\nGot: %s\n", inp);
	} /* else {
		paste
	} */
	draw_input(text, ptr1, ptr2);
}
