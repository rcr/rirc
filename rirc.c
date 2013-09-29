#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <termios.h>
struct termios oterm, nterm;

int soc = -1;

#include "ui.c"
#include "net.c"
#include "input.c"

void init_ui(void);
void cleanup(int);
void main_loop(void);
void fatal(char *e) { perror(e); cleanup(0); exit(1); }

int
main(int argc, char **argv)
{
	init_ui();
	main_loop();
	cleanup(1);
	return 0;
}

void
signal_sigwinch(int unused)
{
	resize();
	if (signal(SIGWINCH, signal_sigwinch) == SIG_ERR)
		fatal("signal handler: SIGWINCH");
}

void
init_ui(void)
{
	setbuf(stdout, NULL);

	/* set terminal to raw mode */
	tcgetattr(0, &oterm);
	memcpy(&nterm, &oterm, sizeof(struct termios));
	nterm.c_lflag &= ~(ECHO | ICANON | ISIG);
	nterm.c_cc[VMIN] = 1;
	nterm.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSADRAIN, &nterm) < 0)
		fatal("tcsetattr");

	/* Set sigwinch, init draw */
	signal_sigwinch(0);
}

void
cleanup(int clear)
{
	tcsetattr(0, TCSADRAIN, &oterm);
	if (clear) printf("\033[H\033[J");
}

void
main_loop(void)
{
	char buf[BUFFSIZE];
	int ret, count = 0, time = 200;
	struct pollfd fds[2];

	fds[0].fd = 0; /* stdin */
	fds[0].events = POLLIN;
	fds[1].events = POLLIN;

	for (;;) {

		fds[1].fd = soc;
		ret = poll(fds, 1 + connected, time);

		if (ret == 0) { /* timed out check input buffer */
			if (count > 0) {
				input(buf, count);

				/* FIXME: */
				if (buf[0] == 'q')
					break;
			}
			count = 0;
			time = 200;
		} else if (fds[0].revents & POLLIN) {
			count = read(0, buf, BUFFSIZE);
			time = 0;
		} else if (fds[1].revents & POLLIN) {
			count = read(soc, buf, BUFFSIZE);
			recv_msg(buf, count);
			time = count = 0;
		}
	}
}
