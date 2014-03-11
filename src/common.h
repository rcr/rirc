#define BUFFSIZE 512
#define MAXINPUT 200
#define NICKSIZE 50 /* TODO */
#define MAXSERVERS 10
#define SCROLLBACK 300
#define SENDBUFF MAXINPUT + 3

typedef enum {SERVER, CHANNEL} channel_t;
typedef enum {NONE, ACTIVE, PINGED, ACTV_SIZE} activity_t;
typedef enum {DEFAULT, JOINPART, NICK, ACTION, NUMRPL} line_t;

/* User modes */
#define UMODE_a (1 << 0) /* away */
#define UMODE_i (1 << 1) /* invisible */
#define UMODE_w (1 << 2) /* receiving wallops */
#define UMODE_r (1 << 3) /* restricted user connection */
#define UMODE_o (1 << 4) /* operator */
#define UMODE_O (1 << 5) /* local operator */
#define UMODE_s (1 << 6) /* receiving server notices */

typedef struct node {
	int height;
	struct node *l;
	struct node *r;
	char nick[NICKSIZE];
} node;

typedef struct line
{
	int len;
	int time_h;
	int time_m;
	char *text;
	char from[NICKSIZE];
	line_t type;
} line;

typedef struct channel
{
	activity_t active;
	channel_t type;
	char name[50];
	int nick_pad;
	int nick_count;
	struct channel *next;
	struct channel *prev;
	struct line *cur_line;
	struct line chat[SCROLLBACK];
	struct node *nicklist;
	struct server *server;
} channel;

typedef struct server
{
	char *iptr;
	char *nptr;
	char input[BUFFSIZE];
	char name[50];
	char nick_me[NICKSIZE];
	int port;
	int reg;
	int soc;
	int usermode;
	struct channel *channel;
} server;

/* rirc.c */
int run;
void fatal(char*);

/* net.c */
void con_lost(int);
void channel_sw(int);
void channel_close(void);
void send_mesg(char*);
void recv_mesg(char*, int, int);

/* ui.c */
int window;
void resize(void);
void draw_bar(void);
void draw_full(void);
void draw_chat(void);
void draw_chans(void);
void draw_input(void);

/* input.c */
int inp1, inp2;
char input_bar[SENDBUFF];
void input(char*, int);

/* utils.c */
int check_pinged(char*, char*);
int cmdcmp(char*, char*);
int cmdcmpc(char*, char*);
int getarg(char**, char**);
int getargc(char**, char**, char);
int nicklist_insert(node**, char*);
int nicklist_delete(node**, char*);
void free_nicklist(node*);
