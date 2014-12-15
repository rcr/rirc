#define VERSION "0.0.1"

#define SCROLLBACK_BUFFER 200
#define SCROLLBACK_INPUT 15
#define BUFFSIZE 512
#define MAXINPUT 200
#define NICKSIZE 50 /* TODO */

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

#define fatal(mesg) \
	do {perror(mesg); exit(EXIT_FAILURE);} while (0);

/* Channel bar activity types */
typedef enum {
	ACTIVITY_DEFAULT,
	ACTIVITY_ACTIVE,
	ACTIVITY_PINGED,
	ACTIVITY_T_SIZE
} activity_t;

/* Buffer line types */
typedef enum {
	LINE_DEFAULT,
	LINE_NICK,
	LINE_JOIN,
	LINE_PART,
	LINE_QUIT,
	LINE_NUMRPL,
	LINE_PINGED,
	LINE_ACTION,
	LINE_DEBUG,
	LINE_T_SIZE
} line_t;

/* Global configuration */
struct config
{
	int join_part_quit_threshold;
	char *username;
	char *realname;
	char *nicks;
	char *auto_connect;
	char *auto_port;
	char *auto_join;
} config;

/* Nicklist AVL tree node */
typedef struct node
{
	int height;
	struct node *l;
	struct node *r;
	char nick[NICKSIZE];
} node;

/* Chat buffer line */
typedef struct line
{
	int len;
	int time_h;
	int time_m;
	char *text;
	char from[NICKSIZE];
	line_t type;
} line;

/* Channel input line */
typedef struct input_l
{
	char *end;
	char text[MAXINPUT+1];
	struct input_l *prev;
	struct input_l *next;
} input_l;

/* Channel input */
typedef struct input
{
	int count;
	char *head;
	char *tail;
	char *window;
	struct input_l *list_head;
	struct input_l *line;
} input;

/* Channel buffer */
typedef struct channel
{
	activity_t active;
	char name[50];
	char type;
	int chanmode;
	int nick_pad;
	int nick_count;
	int parted;
	struct channel *next;
	struct channel *prev;
	struct line *cur_line;
	struct line chat[SCROLLBACK_BUFFER];
	struct node *nicklist;
	struct server *server;
	struct input *input;
} channel;

/* Server */
typedef struct server
{
	char *iptr;
	char *nptr;
	char input[BUFFSIZE];
	char *host;
	char *port;
	char nick_me[NICKSIZE];
	int soc;
	int usermode;
	struct channel *channel;
	struct server *next;
	struct server *prev;
	void *connecting;
} server;

/* Parsed IRC message */
typedef struct parsed_mesg
{
	char *from;
	char *hostinfo;
	char *command;
	char *params;
	char *trailing;
} parsed_mesg;

/* rirc.c */
channel *rirc;
channel *ccur;

/* net.c */
void check_servers(void);
void server_connect(char*, char*);
channel* new_channel(char*, server*, channel*);
channel* channel_close(channel*);
channel* channel_switch(channel*, int);
void send_mesg(char*);
void newline(channel*, line_t, char*, char*, int);
void newlinef(channel*, line_t, char*, char*, ...);

/* draw.c */
void redraw(void);
unsigned int draw;
#define draw(X) draw |= X
#define D_RESIZE (1 << 0)
#define D_CHAT   (1 << 1)
#define D_CHANS  (1 << 2)
#define D_INPUT  (1 << 3)
#define D_STATUS (1 << 4)
#define D_FULL ~((draw & 0) | D_RESIZE);

/* input.c */
int confirm;
input* new_input(void);
void free_input(input*);
void inputc(char*, int);

/* utils.c */
char* strdup(const char*);
char* strdupf(const char*, ...);
char* errf(const char*, ...);
char* getarg(char**, int);
int check_pinged(char*, char*);
int nicklist_delete(node**, char*);
int nicklist_insert(node**, char*);
parsed_mesg* parse(char *mesg);
void free_nicklist(node*);
