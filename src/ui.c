#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "common.h"

#define C(x) "\x1b[38;5;"#x"m"
#define MAXINPUT 200

void draw_full(void);

struct winsize w;

extern channel rirc;
extern channel *ccur;
channel *rircp = &rirc;

void
resize()
{
	ioctl(0, TIOCGWINSZ, &w);

	printf("\033[H\033[J");/* Clear */

	int i;
	printf("\033[2;1H\033[2K"C(239));
	for (i = 0; i < w.ws_col; i++) /* Upper separator */
		printf("―");

	printf("\033[%d;1H\033[2K", w.ws_row-1);
	for (i = 0; i < w.ws_col; i++) /* Lower separator */
		printf("―");
	printf("\033[%d;1H\033[2K >>> "C(250), w.ws_row); /* bottom bar */
	draw_full();
}

void
draw_full()
{
	draw_chans();
	draw_chat();
	/* TODO: redraw input bar */
}

int
nick_col(char *nick)
{
	int col = 0;
	while (*nick++ != '\0')
		col += *nick % 8;
	return (col % 8);
}

int
print_line(int rows, int i)
{
	;
}

void
draw_chat()
{
	printf("\x1b[s"); /* save cursor location */
	int rows = 4; /* save 4 lines, for separators, channel bar and input bar */
	int i = ccur->cur_line;
	int r = print_line(rows, i); /* returns row of last printed line */
	while (r < w.ws_row - 1) {
		printf("\x1b[%d;1H\x1b[2K", r++);
	}
	printf("\x1b[u"); /* restore cursor location */
}

void
draw_chans()
{
	printf("\033[s"); /* save cursor location */
	printf("\033[H\033[K");
	int len, width = 0;
	channel *c = rircp;
	do {
		len = strlen(c->name);
		if (width + len + 4 < w.ws_col) {
			int color;
			if (c == ccur)
				color = 255;
			else
				color = c->active ? 245 : 239;
			printf("\033[38;5;%dm  %s  ", color, c->name);
			width += len + 4;
			c = c->next;
		}
		else break;
	} while (c != rircp);
	printf("\033[u"); /* restore cursor location */
}

void
draw_input(char *text, int ptr1, int ptr2)
{
	int p;
	printf(C(239)"\033[%d;6H\033[K"C(250), w.ws_row);
	for (p = 0; p < ptr1; p++)
		putchar(text[p]);
	for (p = ptr2; p < MAXINPUT-1; p++)
		putchar(text[p]);
	printf("\033[%d;%dH", w.ws_row, ptr1+6);
}
