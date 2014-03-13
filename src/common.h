#define BUFFSIZE 512
#define MAXINPUT 200
#define NICKSIZE 50 /* TODO */
#define MAXSERVERS 10
#define SCROLLBACK 300
#define SENDBUFF MAXINPUT + 3

typedef enum {NONE, ACTIVE, PINGED, ACTV_SIZE} activity_t;
typedef enum {DEFAULT, JOINPART, NICK, ACTION, NUMRPL} line_t;

/* Chan modes */
#define CMODE_STR "OovaimnqpsrtklbeI"
#define CMODE_O (1 << 0) /* give "channel creator" status */
#define CMODE_o (1 << 1) /* give/take channel operator privilege */
#define CMODE_v (1 << 2) /* give/take voice privilege */
#define CMODE_a (1 << 3) /* anonymous channel */
#define CMODE_i (1 << 4) /* invite-only channel */
#define CMODE_m (1 << 5) /* moderated channel */
#define CMODE_n (1 << 6) /* no messages to channel from the outside */
#define CMODE_q (1 << 7) /* quiet channel */
#define CMODE_p (1 << 8) /* private channel */
#define CMODE_s (1 << 9) /* secret channel */
#define CMODE_r (1 << 11) /* server reop channel */
#define CMODE_t (1 << 12) /* topic settable by channel operator only */
#define CMODE_k (1 << 13) /* set/remove channel password */
#define CMODE_l (1 << 14) /* set/remove user limit */
#define CMODE_b (1 << 15) /* set/remove ban mask */
#define CMODE_e (1 << 16) /* set/remove exception mask to override a ban */
#define CMODE_I (1 << 17) /* set/remove mask override invite-only flag */
#define CMODE_MAX 18

/* User modes */
#define UMODE_STR "aiwroOs"
#define UMODE_a (1 << 0) /* away */
#define UMODE_i (1 << 1) /* invisible */
#define UMODE_w (1 << 2) /* receiving wallops */
#define UMODE_r (1 << 3) /* restricted user connection */
#define UMODE_o (1 << 4) /* operator */
#define UMODE_O (1 << 5) /* local operator */
#define UMODE_s (1 << 6) /* receiving server notices */
#define UMODE_MAX 7

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
	char name[50];
	char type;
	int chanmode;
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
void draw_full(void);
void draw_chat(void);
void draw_chans(void);
void draw_input(void);
void draw_status(void);

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
