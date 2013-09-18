#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "net.c"

/* init_ui(), cleaup() */
/* move ui stuff to its own .c later */
#include <poll.h>
#include <ctype.h>  /* isprint */
#include <termios.h>
#include <string.h> /* memcpy */
#include <sys/ioctl.h>
struct winsize w;
struct termios oterm, nterm;

#define BUFFSIZE 512
#define CLR "\033[H\033[J"

void resize(int);
void init_ui(void);
void cleanup(int);
void gui_loop(void);
void fatal(char *e) { perror(e); cleanup(0); exit(1); }

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
	setbuf(stdout, NULL);

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
	char buf[BUFFSIZE];
	int soc, ret, count = 0, time = 200;
	/* FIXME: [stdin + num_servers] */
	struct pollfd fds[2];

	soc = con_server("localhost");

#if 0
	/* testing server connection */
	char buf1[] = ":guest!~guest@localhost.localdomain NICK Guest\r\n";
	send(soc, buf1, strlen(buf1), 0);
	char buf2[] = ":guest!~guest@localhost.localdomain USER guest irc.testnet.net bla :TestName\r\n";
	send(soc, buf2, strlen(buf2), 0);
	char buf3[] = ":guest!~guest@localhost.localdomain JOIN #test\r\n";
	send(soc, buf3, strlen(buf3), 0);
#endif

	for (;;) {

		fds[0].fd = 0; /* stdin */
		fds[0].events = POLLIN;

		fds[1].fd = soc;
		fds[1].events = POLLIN;

		ret = poll(fds, 2, time);

		if (ret == 0) { /* timed out check input buffer */
			if (count == 1) {
				char c = buf[0];
				if (c == 'q')
					break;
				if (c == 'd') {
					puts("disconnecting");
					dis_server();
				}
			} else if (count > 0) { /* escape sequence or paste */
				putchar('~');
			}
			count = 0;
			time = 200;
		} else if (fds[0].revents & POLLIN) {
			count = read(0, buf, BUFFSIZE);
			time = 0;
		} else if (fds[1].revents & POLLIN) {
			count = read(soc, buf, BUFFSIZE);
			printf("%.*s", count, buf);
			time = count = 0;
		}
	}
}
