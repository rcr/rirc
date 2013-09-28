extern struct winsize w;

#define MAXINPUT 200

/* testing */
#define CHANTEST 10
int current_chan = 3;
struct chan
{
	int active;
	char name[50];
};
struct chan chan_list[CHANTEST];

void
draw_chans()
{
	printf("\033[H");
	int i, len, width = 0;
	for (i = 0; i < CHANTEST; i++) {
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
init_draw()
{
	int i;
	printf("\033[H\033[J"); /* Clear */
	printf("\033[%d;1H\033[2K\033[30m", 2);
	for (i = 0; i < w.ws_col; i++)
		printf("―");
	printf("\033[%d;1H\033[2K", w.ws_row-1);
	for (i = 0; i < w.ws_col; i++)
		printf("―");
	printf("\033[%d;1H\033[2K >>> \033[0m", w.ws_row);

	/* FIXME: simulate some channels to test drawing stuff */
	struct chan chan1 = { .name = "#chantesting1", .active=0 }; chan_list[1] = chan1;
	struct chan chan2 = { .name = "#chantesting2", .active=1 }; chan_list[2] = chan2;
	struct chan chan3 = { .name = "#chantesting3", .active=1 }; chan_list[3] = chan3;
	struct chan chan4 = { .name = "#chantesting4", .active=0 }; chan_list[4] = chan4;
	struct chan chan5 = { .name = "#chantesting5", .active=0 }; chan_list[5] = chan5;
	struct chan chan6 = { .name = "#chantesting6", .active=0 }; chan_list[6] = chan6;
	struct chan chan7 = { .name = "#chantesting7", .active=1 }; chan_list[7] = chan7;
	struct chan chan8 = { .name = "#chantesting8", .active=1 }; chan_list[8] = chan8;
	struct chan chan9 = { .name = "#chantesting9", .active=0 }; chan_list[9] = chan9;
	draw_chans();
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
