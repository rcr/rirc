#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "common.h"

#define C(x) "\x1b[38;5;"#x"m"

int nick_col(char*);
int print_line(int, line*);
int print_more(char*, char*, int);
char* word_wrap(char*, char*);

struct winsize w;

int tw = 0;  /* text width */

int nick_cols[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
int actv_cols[ACTV_SIZE] = {239, 247, 3};

void
resize(void)
{
	ioctl(0, TIOCGWINSZ, &w);

	printf("\x1b[H\x1b[J");/* Clear */

	int i;
	printf("\x1b[2;1H\x1b[2K"C(239));
	for (i = 0; i < w.ws_col; i++) /* Upper separator */
		printf("―");

	draw_status(); /* Status bar and lower separator */

	printf("\x1b[%d;1H\x1b[2K >>> "C(250), w.ws_row); /* bottom bar */
	draw_full();
}

/* Statusbar:
 *
 * server / private chat:
 * --[usermodes]---
 *
 * channel:
 * --[usermodes]--[chantype chanmodes chancount]---
 * */
void
draw_status(void)
{
	printf("\x1b[s"); /* save cursor location */
	printf("\x1b[%d;1H\x1b[2K"C(239), w.ws_row-1);

	int i = 0, j, mode;
	char umode_str[] = UMODE_STR;
	char cmode_str[] = CMODE_STR;

	/* usermodes */
	if (ccur->server && (mode = ccur->server->usermode)) {
		i += printf("―[+") - 2;
		for (j = 0; j < UMODE_MAX; j++) {
			if (mode & (1 << j)) {
				putchar(umode_str[j]);
				i++;
			}
		}
		i += printf("]");
	}

	/* private chat */
	if (ccur->type == 'p') {
		i += printf("―[priv]") - 2;
	/* chantype, chanmodes, chancount */
	} else if (ccur->type) {
		i += printf("―[%c", ccur->type) - 2;

		if ((mode = ccur->chanmode)) {
			i += printf(" +");
			for (j = 0; j < CMODE_MAX; j++) {
				if (mode & (1 << j)) {
					putchar(cmode_str[j]);
					i++;
				}
			}
		}
		i += printf(" %d]", ccur->nick_count);
	}

	for (; i < w.ws_col; i++)
		printf("―");

	printf("\x1b[u"); /* restore cursor location */
}

void
draw_full(void)
{
	draw_chans();
	draw_chat();
	draw_status();
	draw_input();
}

void
draw_chans(void)
{
	printf("\x1b[s"); /* save cursor location */
	printf("\x1b[H\x1b[K");
	int len, width = 0;
	channel *c = cfirst;
	do {
		len = strlen(c->name);
		if (width + len + 4 < w.ws_col) {
			printf("\x1b[38;5;%dm  %s  ",
					(c == ccur) ? 255 : actv_cols[c->active], c->name);
			width += len + 4;
			c = c->next;
		}
		else break;
	} while (c != cfirst);
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
	}

	return NULL;
}

void
draw_chat(void)
{
	printf("\x1b[s"); /* save cursor location */

	tw = w.ws_col - ccur->nick_pad - 12;

	int r = print_line(w.ws_row - 2, ccur->cur_line - 1);
	while (r < w.ws_row - 1)
		printf("\x1b[%d;1H\x1b[2K", r++);
	printf("\x1b[u"); /* restore cursor location */
}

int
print_line(int row, line *l)
{
	if (l < ccur->chat)
		l += SCROLLBACK_BUFFER;

	if (!l->len || l == ccur->cur_line)
		return 3;

	int count = 1;
	char *ptr1, *ptr2, *wrap;

	ptr1 = l->text;
	ptr2 = l->text + l->len;

	while ((ptr1 = word_wrap(ptr1, ptr2)) != NULL && ptr1 != ptr2)
		count++;

	if (row - count > 2)
		row = print_line(row - count, l - 1) + count - 1;

	ptr1 = l->text;
	if ((wrap = word_wrap(ptr1, ptr2)) != NULL)
		row = print_more(wrap, ptr2, row);
	else
		wrap = ptr2;

	int fromcol;
	if (l->type == JOINPART)
		fromcol = 239;
	else
		fromcol = nick_col(l->from);

	if (row > 2) {
		printf("\x1b[%d;1H\x1b[2K", row);
		printf(C(239)" %02d:%02d  "C(%d)"%*s%s "C(239)"~"C(250)" ",
				l->time_h, l->time_m, fromcol,
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
draw_input(void)
{
	if (confirm) {
		printf(C(239)"\x1b[%d;6H\x1b[K"C(250), w.ws_row);
		printf("Confirm sending %d lines? (y/n)", confirm);
		return;
	}

	int winsz = w.ws_col / 3;

	input *in = ccur->input;

	/* Reframe the input bar window */
	if (in->head > (in->window + w.ws_col - 6))
		in->window += winsz;
	else if (in->head == in->window - 1)
		in->window = (in->window - winsz > in->line->text)
			? in->window - winsz : in->line->text;

	printf(C(239)"\x1b[%d;6H\x1b[K"C(250), w.ws_row);

	char *p = in->window;
	while (p < in->head)
		putchar(*p++);

	p = in->tail;

	char *end = in->tail + w.ws_col - 5 - (in->head - in->window);

	while (p < end && p < in->line->text + MAXINPUT)
		putchar(*p++);

	int col = (in->head - in->window);

	printf("\x1b[%d;%dH", w.ws_row, col + 6);
}
