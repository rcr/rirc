#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "common.h"

#define MAXINPUT 200

void draw_full(void);

struct winsize w;

extern int chan_count;
extern int current_chan;
extern struct channel chan_list[];

void
resize()
{
	ioctl(0, TIOCGWINSZ, &w);
	draw_full();
}

void
draw_full()
{
	int i;
	printf("\033[H\033[J");/* Clear */
	draw_chans();
	printf("\033[2;1H\033[2K\033[30m");
	for (i = 0; i < w.ws_col; i++) /* Upper separator */
		printf("―");
	draw_chat();
	printf("\033[%d;1H\033[2K", w.ws_row-1);
	for (i = 0; i < w.ws_col; i++) /* Lower separator */
		printf("―");
	printf("\033[%d;1H\033[2K >>> \033[0m", w.ws_row); /* bottom bar */
	/* TODO: redraw input bar */
}

void
draw_chat()
{
	printf("\033[s"); /* save cursor location */
	printf("\033[3;1H\033[0m");
	channel *c = &chan_list[current_chan];
	int tw = w.ws_col - c->nick_pad - 12;
	line *l = c->chat;
	while (l->len > 0) {
		int n = l->len;
		printf(" %02d:%02d  %s ~ ", l->time_h, l->time_m, l->from);
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
					printf("\n                  ~ ");
				else
					break;
				ptr1 = ptr2;
				while (*ptr1 == ' ')
					ptr1++;
			}
			printf("\n");
		} else {
			printf("%s\n", l->text);
		}
		l++;
	}
	printf("\033[u"); /* restore cursor location */
}

void
draw_chans()
{
	printf("\033[s"); /* save cursor location */
	printf("\033[H\033[K");
	int i, len, width = 0;
	for (i = 0; i < chan_count; i++) {
		len = strlen(chan_list[i].name);
		if (width + len + 4 < w.ws_col) {
			int color;
			if (i == current_chan)
				color = 255;
			else
				color = chan_list[i].active ? 245 : 239;
			printf("\033[38;5;%dm  %s  ", color, chan_list[i].name);
			width += len + 4;
		}
		else break;
	}
	printf("\033[u"); /* restore cursor location */
}

void
print_line(char *text, int ptr1, int ptr2)
{
	int p;
	printf("\033[30m\033[%d;6H\033[K\033[0m", w.ws_row);
	for (p = 0; p < ptr1; p++)
		putchar(text[p]);
	for (p = ptr2; p < MAXINPUT-1; p++)
		putchar(text[p]);
	printf("\033[%d;%dH", w.ws_row, ptr1+6);
}
