#include <ctype.h>

/* maximum input size */
#define MAXINPUT 10

#define CURFORW printf("\033[C")
#define CURBACK printf("\033[D")

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
del_char()
{
	if (ptr1 > 0)
		ptr1--;
}

void
cur_lr(int left)
{
	if (left) {
		if (ptr1 > 0) {
			text[ptr2--] = text[--ptr1];
		}
	} else {
		if (ptr2 < MAXINPUT) {
			text[ptr1++] = text[++ptr2];
		}
	}
}

void
print_line()
{
	printf("\033[2K\033[1;1H  >>> "); /* | >>> | <   > */
	int p = 0;

#if 0
	/* test print1 */
	while (p < ptr1)
		putchar(text[p++]);
	while (p < ptr2) {
		putchar('.');
		p++;
	}
	while (p < MAXINPUT)
		putchar(text[p++]);

	/* test print2 */
	text[MAXINPUT-1] = '\0';
	printf("\n%s\n", text);

	printf("%d: %c   %d: %c\n\n", ptr1, text[ptr1], ptr2, text[ptr2]);

	/* actual print */
	printf("\033[2K"); /* 0: clear from cursor to end, 2: clear whole line*/
#endif



	for (p = 0; p < ptr1; p++)
		putchar(text[p]);
	for (p = ptr2; p < MAXINPUT-1; p++)
		putchar(text[p]);
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
			del_char();
	} else if (count > 0 && *inp++ == 0x1B) { /* escape sequence */
		if (esccmp("[A", inp))
			puts("UP");
		if (esccmp("[B", inp))
			puts("DOWN");
		if (esccmp("[C", inp))
			cur_lr(0);
		if (esccmp("[D", inp))
			cur_lr(1);
	}
	print_line();
}
