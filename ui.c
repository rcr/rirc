extern struct winsize w;

#define MAXINPUT 200

void
init_draw()
{
	int i;
	printf("\033[H\033[J"); /* Clear */
	printf("\033[%d;1H\033[2K", 2);
	for (i = 0; i < w.ws_col; i++)
		printf("―");
	printf("\033[%d;1H\033[2K", w.ws_row-1);
	for (i = 0; i < w.ws_col; i++)
		printf("―");
	printf("\033[%d;1H\033[2K >>> ", w.ws_row);
}

void
print_line(char *text, int ptr1, int ptr2)
{
	int p;
	printf("\033[%d;1H\033[2K >>> ", w.ws_row);
	for (p = 0; p < ptr1; p++)
		putchar(text[p]);
	for (p = ptr2; p < MAXINPUT-1; p++)
		putchar(text[p]);
	printf("\033[%d;%dH", w.ws_row, ptr1+6);
}
