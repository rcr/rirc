#define BUFFSIZE 512
#define MAXINPUT 200
#define SCROLLBACK 300
#define SENDBUFF MAXINPUT + 3

typedef enum {DEFAULT, NOCHECK, JOINPART, NICK, ACTION, NUMRPL} line_t;

/* rirc.c */
void fatal(char*);

/* net.c */
void con_lost(int);
void channel_sw(int);
void init_chans(void);
void send_msg(char*, int);
void recv_msg(char*, int, int);

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

typedef struct server
{
	int soc; /* if soc == 0, disconnected */
	int reg; /* registered with the server */
	char name[50];
} server;

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
	struct server *server;
	struct channel *prev;
	struct channel *next;
} channel;
