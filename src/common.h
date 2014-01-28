#define BUFFSIZE 512
#define MAXINPUT 200
#define MAXSERVERS 5
#define SCROLLBACK 300
#define SENDBUFF MAXINPUT + 3

typedef enum {SERVER, CHANNEL} channel_t;
typedef enum {NONE, ACTIVE, PINGED, ACTV_SIZE} activity_t;
typedef enum {DEFAULT, JOINPART, NICK, ACTION, NUMRPL} line_t;

/* rirc.c */
void fatal(char*);

/* net.c */
void con_lost(int);
void channel_sw(int);
void channel_close(void);
void send_mesg(char*, int);
void recv_mesg(char*, int, int);

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

/* utils.c */
struct node* node_delete(struct node*, char*);
struct node* node_insert(struct node*, char*);

int run;

typedef struct node {
	int height;
	char *nick;
	struct node *l;
	struct node *r;
} node;

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
	int nick_pad;
	int nick_count;
	node *nick_root;
	char name[50];
	channel_t type;
	activity_t active;
	line *cur_line;
	line chat[SCROLLBACK];
	struct server *server;
	struct channel *prev;
	struct channel *next;
} channel;

typedef struct server
{
	int soc; /* if soc == 0, disconnected */
	int reg; /* registered with the server */
	int port;
	char name[50];
	char *nptr;
	char nick_me[50];
	char input[BUFFSIZE];
	char *iptr;
	channel *channel;
} server;
