#include <ctype.h>

#define MAXINPUT 30

char text[MAXINPUT];
int ptr1 = 0;
int ptr2 = MAXINPUT-1;

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

/* belongs in ui.c */
void
print_line()
{
	int p;
	printf("\033[2K\033[1;1H  >>> ");
	for (p = 0; p < ptr1; p++)
		putchar(text[p]);
	for (p = ptr2; p < MAXINPUT-1; p++)
		putchar(text[p]);
	printf("\033[1;%dH", ptr1 + 7);
}

int
esccmp(char *esc, char *inp)
{
	while (*esc++ == *inp++)
		if (*esc == '\0') return 1;
	return 0;
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
	} else if (count > 0 && *inp++ == 0x1B) { /* escape sequence */
		if (esccmp("[A", inp)) /* arrow up */
			;
		if (esccmp("[B", inp)) /* arrow down */
			;
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
	print_line();
}
