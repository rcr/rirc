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

void
draw_chat()
{
	printf("\x1b[s"); /* save cursor location */
	line *l = ccur->chat;
	int i, h, tw = w.ws_col - ccur->nick_pad - 15;
	for (i = 3, h = w.ws_row - 1; i < h; i++)
	{
		printf("\x1b[%d;1H\x1b[2K", i);
		if (l->len > 0) {
			int n = l->len;
			printf(C(239)" %02d:%02d  "C(%d)"%*s%s "C(239)"~"C(250)" ",
					l->time_h, l->time_m, nick_col(l->from),
					(int)(ccur->nick_pad - strlen(l->from)), "", l->from);
			char *end = l->text + l->len - 2;
			if (n > tw) {
				char *ptr1 = l->text;
				for (;;) {
					char *ptr2 = ptr1 + tw;
					if (ptr2 > end)
						ptr2 = end;
					else
						while (*ptr2 != ' ' && ptr2 > ptr1)
							ptr2--;
					if (ptr2 == ptr1)
						ptr2 += tw;
					while (ptr1 <= ptr2)
						putchar(*ptr1++);
					if (ptr2 < end)
						printf("\x1b[%d;%dH\x1b[2K"C(239)"~"C(250)" ", ++i, ccur->nick_pad + 10);
					else
						break;
					ptr1 = ptr2 + 1;
					while (*ptr1 == ' ')
						ptr1++;
				}
			} else {
				printf("%s", l->text);
			}
		}
		l++;
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
