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
	int tw, n;
	printf("\033[s"); /* save cursor location */
	printf("\033[3;1H\033[0m");

	channel *c = &chan_list[current_chan];
	tw = w.ws_col - c->nick_pad - 11;

	line *l = c->chat;
	while (l->len > 0) {
		char *ptr = l->text;
		n = l->len - 1;
		printf(" %02d:%02d  %s ", l->time_h, l->time_m, l->from);
		printf("~ %.*s\n", tw, ptr);
		while (n > tw) {
			ptr += tw;
			char *wh = (ptr + tw - 1);
			printf("%*s~ %.*s\n", c->nick_pad + 9, " ", tw, ptr);
			n -= tw;
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
