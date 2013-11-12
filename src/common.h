#define BUFFSIZE 512
#define MAXINPUT 200
#define SCROLLBACK 300

char errbuff[BUFFSIZE];

/* rirc.c */
void fatal(char*);

/* net.c */
void channel_sw(int);
void init_chans(void);
void send_msg(char*, int);
void recv_msg(char*, int);

/* ui.c */
void resize(void);
void draw_full(void);
void draw_chat(void);
void draw_chans(void);
void draw_input(char*, int, int);

/* input.c */
void input(char*, int);

int run;

typedef struct line
{
	int len;
	int time_h;
	int time_m;
	char from[50];
	char *text;
} line;

typedef struct channel
{
	int active;
	int cur_line;
	int nick_pad;
	int connected;
	char name[50];
	line chat[SCROLLBACK];
	struct channel *prev;
	struct channel *next;
} channel;
