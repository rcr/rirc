#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "common.h"

#define MAXINPUT 200

void draw_full(void);
void draw_chat(void);

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
	printf("\033[%d;1H\033[2K", w.ws_row-1);
	for (i = 0; i < w.ws_col; i++) /* Lower separator */
		printf("―");
	printf("\033[%d;1H\033[2K >>> \033[0m", w.ws_row); /* bottom bar */
}

void
draw_chat()
{
	;
}

void
draw_chans()
{
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
}

void
print_line(char *text, int ptr1, int ptr2)
{
	int p;
	printf("\033[30m\033[%d;1H\033[2K >>> \033[0m", w.ws_row);
	for (p = 0; p < ptr1; p++)
		putchar(text[p]);
	for (p = ptr2; p < MAXINPUT-1; p++)
		putchar(text[p]);
	printf("\033[%d;%dH", w.ws_row, ptr1+6);
}