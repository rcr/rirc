#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "common.h"

#define C(x) "\x1b[38;5;"#x"m"
#define MAXINPUT 200

int nick_col(char*);
int print_line(int, int);
int print_more(char*, char*, int);
char* word_wrap(char*, char*);

extern channel rirc;
extern channel *ccur;
channel *rircp = &rirc;

struct winsize w;

int tw = 0;  /* text width */
int nlw = 3; /* nicklist width */

int nick_cols[] = {20, 51, 216, 83, 103, 115, 163, 193};

void
resize(void)
{
	ioctl(0, TIOCGWINSZ, &w);

	printf("\x1b[H\x1b[J");/* Clear */

	int i;
	printf("\x1b[2;1H\x1b[2K"C(239));
	for (i = 0; i < w.ws_col; i++) /* Upper separator */
		printf("―");

	printf("\x1b[%d;1H\x1b[2K", w.ws_row-1);
	for (i = 0; i < w.ws_col; i++) /* Lower separator */
		printf("―");
	printf("\x1b[%d;1H\x1b[2K >>> "C(250), w.ws_row); /* bottom bar */
	draw_full();
}

void
draw_full(void)
{
	draw_chans();
	draw_chat();
	/* TODO: redraw input bar */
}

void
draw_chans(void)
{
	printf("\x1b[s"); /* save cursor location */
	printf("\x1b[H\x1b[K");
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
			printf("\x1b[38;5;%dm  %s  ", color, c->name);
			width += len + 4;
			c = c->next;
		}
		else break;
	} while (c != rircp);
	printf("\x1b[u"); /* restore cursor location */
}

int
nick_col(char *nick)
{
	int col = 0;
	while (*nick != '\0')
		col += *nick++;
	return nick_cols[col % sizeof(nick_cols)/sizeof(nick_cols[0])];
}

char*
word_wrap(char *start, char *end)
{
	char *wrap;
	if ((wrap = start + tw) < end) {

		while (*wrap != ' ' && wrap > start)
			wrap--;

		if (wrap == start)
			wrap += tw;

		if (wrap == end)
			return NULL;
		else if (*wrap == ' ')
			return wrap + 1;
		else
			return wrap;
	} else {
		return NULL;
	}
}

void
draw_chat(void)
{
	printf("\x1b[s"); /* save cursor location */
	int r = print_line(w.ws_row - 2, ccur->cur_line);
	while (r < w.ws_row - 1)
		printf("\x1b[%d;1H\x1b[2K", r++);
	printf("\x1b[u"); /* restore cursor location */
}

int
print_line(int row, int i)
{
	line *l = ccur->chat + ((i - 1 + SCROLLBACK) % SCROLLBACK);

	if (!l->len)
		return 3;

	tw = w.ws_col - ccur->nick_pad - nlw - 13;

	int count = 1;
	char *ptr1, *ptr2, *wrap;

	ptr1 = l->text;
	ptr2 = l->text + l->len;

	while ((ptr1 = word_wrap(ptr1, ptr2)) != NULL && ptr1 != ptr2)
		count++;

	if (row - count > 2)
		row = print_line(row - count, i - 1) + count - 1;

	ptr1 = l->text;
	if ((wrap = word_wrap(ptr1, ptr2)) != NULL)
		row = print_more(wrap, ptr2, row);
	else
		wrap = ptr2;

	if (row > 2) {
		printf("\x1b[%d;1H\x1b[2K", row);
		printf(C(239)" %02d:%02d  "C(%d)"%*s%s "C(239)"~"C(250)" ",
				l->time_h, l->time_m, nick_col(l->from),
				(int)(ccur->nick_pad - strlen(l->from)), "", l->from);
		while (ptr1 < wrap)
			putchar(*ptr1++);
	}
	return row + count;
}

int
print_more(char *start, char *end, int row)
{
	char *wrap;
	if ((wrap = word_wrap(start, end)) != NULL && wrap != end)
		row = print_more(wrap, end, row);
	else
		wrap = end;

	if (row > 2) {
		printf("\x1b[%d;%dH\x1b[2K", row, ccur->nick_pad + 10);
		printf(C(239)"~"C(250)" ");
		while (start < wrap)
			putchar(*start++);
	}
	return row - 1;
}

void
draw_input(char *text, int ptr1, int ptr2)
{
	int p;
	printf(C(239)"\x1b[%d;6H\x1b[K"C(250), w.ws_row);
	for (p = 0; p < ptr1; p++)
		putchar(text[p]);
	for (p = ptr2; p < MAXINPUT-1; p++)
		putchar(text[p]);
	printf("\x1b[%d;%dH", w.ws_row, ptr1+6);
}
