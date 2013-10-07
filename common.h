#define BUFFSIZE 512
#define MAXINPUT 200
#define SCROLLBACK 10

/* rirc.c */
void fatal(char*);

/* net.c */
void send_msg(char*, int);
void recv_msg(char*, int);

/* ui.c */
void resize(void);
void draw_chans(void);

/* input.c */
void input(char*, int);
void print_line(char*, int, int);

int run;

struct channel
{
	int active;
	int cur_line;
	char name[50];
	char *chat[SCROLLBACK];
};
