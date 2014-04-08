#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <termios.h>

#include "common.h"

void init(void);
void cleanup(int);
void main_loop(void);

extern int numfds;

struct termios oterm, nterm;
struct pollfd fds[MAXSERVERS + 1] = {{0}};

int
main(int argc, char **argv)
{
	init();
	main_loop();
	cleanup(1);
	return 0;
}

void
fatal(char *e)
{
	perror(e);
	cleanup(0);
	exit(1);
}

void
signal_sigwinch(int unused)
{
	resize();
	if (signal(SIGWINCH, signal_sigwinch) == SIG_ERR)
		fatal("signal handler: SIGWINCH");
}

void
init(void)
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

	srand(time(NULL));

	confirm = 0;

	rirc = cfirst = ccur = new_channel("rirc");

	/* Set sigwinch, init draw */
	signal_sigwinch(0);
}

void
cleanup(int clear)
{
	tcsetattr(0, TCSADRAIN, &oterm);

	ccur = cfirst;
	do {
		channel_close();
	} while (cfirst != rirc);

	free_channel(rirc);

	if (clear)
		printf("\x1b[H\x1b[J");
}

void
main_loop(void)
{
	char buf[BUFFSIZE];
	int i, ret, count = 0, time = 200;

	fds[0].fd = 0; /* stdin */
	for (i = 0; i < MAXSERVERS + 1; i++)
		fds[i].events = POLLIN;

	run = 1;

	while (run) {

		ret = poll(fds, numfds, time);

		if (ret == 0) { /* timed out check input buffer */
			if (count > 0) {
				inputc(buf, count);
				count = 0;
			}
			time = 200;
		} else if (fds[0].revents & POLLIN) {
			count = read(0, buf, BUFFSIZE);
			time = 0;
		/* Loop through all open sockets */
		} else {
			for (i = 1; i < numfds; i++) {
				if (fds[i].revents & POLLIN) {
					if ((count = read(fds[i].fd, buf, BUFFSIZE)) == 0)
						con_lost(fds[i].fd);
					else
						recv_mesg(buf, count, fds[i].fd);
					time = count = 0;
				}
			}
		}
	}
}
