#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "net.c"

/* init_ui(), cleaup() */
/* move ui stuff to its own .c later */
#include <termios.h>
#include <string.h> /* memcpy */
#include <sys/ioctl.h>
struct winsize w;
struct termios oterm, nterm;

#define CLR "\033[H\033[J"

void fatal(char *e) { perror(e); cleanup(0); exit(1); }
void resize(int);
void init_ui(void);
void cleanup(int);
void gui_loop(void);

int
main(int argc, char **argv)
{
	init_ui();
	/* testing */
	gui_loop();
	cleanup(1);
	return 0;
}

void
resize(int unused)
{
	ioctl(0, TIOCGWINSZ, &w);
	/* ~ redraw */
	if (signal(SIGWINCH, resize) == SIG_ERR)
		fatal("signal handler: SIGWINCH");
}

void
init_ui(void)
{
	printf(CLR);

	/* set terminal to raw mode */
	tcgetattr(0, &oterm);
	memcpy(&nterm, &oterm, sizeof(struct termios));
	nterm.c_lflag &= ~(ECHO | ICANON | ISIG);
	nterm.c_cc[VMIN] = 1;
	nterm.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSADRAIN, &nterm) < 0)
		fatal("tcsetattr");

	/* get terminal dimensions */
	resize(0);
}

void
cleanup(int clear)
{
	tcsetattr(0, TCSADRAIN, &oterm);
	if (clear)
		printf(CLR);
}

void
gui_loop(void)
{
	char c;
	int socket = -1;
	for (;;) {
		if ((c = getchar()) == 'q')
			break;

		/* testing */
		else if (c == 'c')
			socket = connect_irc("localhost");
		else if (c == 'd')
			disconnect_irc(socket);

		else
			putchar(c);
	}
}
