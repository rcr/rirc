#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "common.h"

/* Set foreground/background color */
#define FG(x) "\x1b[38;5;"#x"m"
#define BG(x) "\x1b[48;5;"#x"m"

/* Set bold foreground bold color */
#define FG_B(x) "\x1b[38;5;"#x";1m"

/* Reset foreground/background color */
#define FG_R "\x1b[39m"
#define BG_R "\x1b[49m"

#define CLEAR_FULL "\x1b[2J"
#define CLEAR_LINE "\x1b[2K"
#define MOVE(X, Y) "\x1b["#X";"#Y"H"

void draw_chat(void);
void draw_chans(void);
void draw_input(void);
void draw_status(void);

int nick_col(char*);
int print_line(int, line*);
int print_more(char*, char*, int);
char* word_wrap(char*, char*);

struct winsize w;

/* text width */
static int i, tw = 0;

static int nick_cols[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
static int actv_cols[ACTIVITY_T_SIZE] = {239, 247, 3};

void
resize(void)
{
	/* Get terminal dimensions */
	ioctl(0, TIOCGWINSZ, &w);

	/* Clear, set separator color, move to top separator */
	printf(CLEAR_FULL  FG(239)  MOVE(2, 1));

	/* Draw upper separator */
	for (i = 0; i < w.ws_col; i++)
		printf("―");

	/* Draw bottom bar, set color back to default */
	printf(MOVE(%d, 1)" >>> " FG(250), w.ws_row);

	/* Draw everything else */
	draw(D_FULL);
}

void
redraw(void)
{
	if (!draw)
		return;

	if (draw & D_CHAT)   draw_chat();
	if (draw & D_CHANS)  draw_chans();
	if (draw & D_INPUT)  draw_input();
	if (draw & D_STATUS) draw_status();
	draw = 0;
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
	printf("\x1b[%d;1H\x1b[2K"FG(239), w.ws_row-1);

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

	/* Restore cursor location */
	printf("\x1b[u");
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

	int from_fg;
	char *from_bg = "";

	if (l->type == LINE_JOIN || l->type == LINE_PART || l->type == LINE_QUIT)
		from_fg = 239;
	else if (l->type == LINE_PINGED)
		from_fg = 255, from_bg = BG(1);
	else
		from_fg = nick_col(l->from);

	if (row > 2) {
		printf("\x1b[%d;1H\x1b[2K", row);
		printf(FG(239)" %02d:%02d  %*s"FG(%d)"%s%s"BG_R FG(239)" ~ "FG(250),
				l->time_h, l->time_m,
				(int)(ccur->nick_pad - strlen(l->from)), "",
				from_fg, from_bg, l->from);
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
		printf(FG(239)"~"FG(250)" ");
		while (start < wrap)
			putchar(*start++);
	}

	return row - 1;
}

void
draw_input(void)
{
	if (confirm) {
		printf(FG(239)"\x1b[%d;6H\x1b[K"FG(250), w.ws_row);
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

	printf(FG(239)"\x1b[%d;6H\x1b[K"FG(250), w.ws_row);

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
