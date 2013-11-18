#define BUFFSIZE 512
#define MAXINPUT 200
#define SCROLLBACK 300
#define SENDBUFF MAXINPUT + 3

char errbuff[BUFFSIZE];

typedef enum {DEFAULT, JOINPART, ACTION} line_t;

/* rirc.c */
void fatal(char*);

/* net.c */
void channel_sw(int);
void init_chans(void);
void send_msg(char*, int);
void recv_msg(char*, int);

/* ui.c */
int window;
void resize(void);
void draw_full(void);
void draw_chat(void);
void draw_chans(void);
void draw_input(void);

/* input.c */
int inp1, inp2;
char input_bar[SENDBUFF];
void input(char*, int);

int run;

typedef struct line
{
	int len;
	int time_h;
	int time_m;
	char from[50];
	char *text;
	line_t type;
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
