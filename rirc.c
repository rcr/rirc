#include <stdio.h>
#include <stdlib.h>

/* init_ui(), cleaup() */
/* move ui stuff to its own .c later */
#include <termios.h>
#include <string.h> /* memcpy */
struct termios oterm, nterm;

#define CLR "\033[H\033[J"

void fatal(char *e) { perror(e); exit(1); }
void init_ui(void);
void cleanup(void);

int
main(int argc, char **argv)
{
	init_ui();
	/* mainloop*/
	cleanup();
	return 0;
}

void
init_ui(void)
{
	printf(CLR);

	tcgetattr(0, &oterm);
	memcpy(&nterm, &oterm, sizeof(struct termios));
	nterm.c_lflag &= ~(ECHO | ICANON | ISIG);
	nterm.c_cc[VMIN] = 1;
	nterm.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSADRAIN, &nterm) < 0)
		fatal("tcsetattr");
}

void
cleanup(void)
{
	tcsetattr(0, TCSADRAIN, &oterm);
	printf(CLR);
}
