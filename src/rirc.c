#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <termios.h>

#include "common.h"

void startup(void);
void cleanup(void);
void main_loop(void);

extern int numfds;

struct termios oterm, nterm;
struct pollfd fds[MAXSERVERS + 1] = {{0}};

int
main(int argc, char **argv)
{
	startup();
	main_loop();
	printf("\x1b[H\x1b[J"); /* Clear */
	return EXIT_SUCCESS;
}

void
signal_sigwinch(int unused)
{
	resize();
	if (signal(SIGWINCH, signal_sigwinch) == SIG_ERR)
		fatal("signal handler: SIGWINCH");
}

void
startup(void)
{
	setbuf(stdout, NULL);

	/* Set terminal to raw mode */
	tcgetattr(0, &oterm);
	memcpy(&nterm, &oterm, sizeof(struct termios));
	nterm.c_lflag &= ~(ECHO | ICANON | ISIG);
	nterm.c_cc[VMIN] = 1;
	nterm.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSADRAIN, &nterm) < 0)
		fatal("tcsetattr");

	/* Set mousewheel event handling */
	printf("\x1b[?1000h");

	srand(time(NULL));

	confirm = 0;

	rirc = cfirst = ccur = new_channel("rirc");

	/* Set sigwinch, init draw */
	signal_sigwinch(0);

	/* Register cleanup for exit */
	atexit(cleanup);
}

void
cleanup(void)
{
	/* Reset terminal modes */
	tcsetattr(0, TCSADRAIN, &oterm);

	/* Reset mousewheel event handling */
	printf("\x1b[?1000l");

	ccur = cfirst;
	do {
		channel_close();
	} while (cfirst != rirc);

	free_channel(rirc);
}

void
main_loop(void)
{
	char buff[BUFFSIZE];
	int i, ret, count = 0, time = 200;

	fds[0].fd = 0; /* stdin */
	for (i = 0; i < MAXSERVERS + 1; i++)
		fds[i].events = POLLIN;


	while (1) {

		ret = poll(fds, numfds, time);

		/* Poll timeout */
		if (ret == 0) {
			if (count) {
				buff[count] = '\0'; /* FIXME, temporary fix */
				inputc(buff, count);
				count = 0;
			}
			time = 200;
		/* Check input on stdin */
		} else if (fds[0].revents & POLLIN) {
			count = read(0, buff, BUFFSIZE);
			time = 0;
		/* Check all open server sockets */
		} else {
			for (i = 1; i < numfds; i++) {
				if (fds[i].revents & POLLIN) {
					if ((count = read(fds[i].fd, buff, BUFFSIZE)) == 0)
						con_lost(fds[i].fd);
					else
						recv_mesg(buff, count, fds[i].fd);
					time = count = 0;
				}
			}
		}
	}
}
