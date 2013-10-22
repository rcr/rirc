#define BUFFSIZE 512
#define MAXINPUT 200
#define SCROLLBACK 300

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

typedef struct line
{
	int len;
	int time;
	char from[20];
	char *text;
} line;

typedef struct channel
{
	int active;
	int cur_line;
	char name[20];
	line chat[SCROLLBACK];
} channel;
