#define VERSION "-1"

#define SCROLLBACK_BUFFER 200
#define SCROLLBACK_INPUT 15
#define BUFFSIZE 512
#define NICKSIZE 256
#define CHANSIZE 256
#define MAX_INPUT 256
#define RECONNECT_DELTA 15

/* Compile time checks */
#if BUFFSIZE < MAX_INPUT
/* Required so input lines can be safely strcpy'ed into a send buffer */
#error BUFFSIZE must be greater than MAX_INPUT
#endif

#include <time.h>

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
#define UMODE_STR "aiwrRoOs"
#define UMODE_a (1 << 0) /* away */
#define UMODE_i (1 << 1) /* invisible */
#define UMODE_w (1 << 2) /* receiving wallops */
#define UMODE_r (1 << 3) /* restricted user connection */
#define UMODE_R (1 << 4) /* registered nicknames only*/
#define UMODE_o (1 << 5) /* operator */
#define UMODE_O (1 << 6) /* local operator */
#define UMODE_s (1 << 7) /* receiving server notices */
#define UMODE_MAX 8

/* Irrecoverable error */
#define fatal(mesg) \
	do {perror(mesg); exit(EXIT_FAILURE);} while (0)

/* Error message length */
#define MAX_ERROR 512

/* Message sent for PART and QUIT by default */
#define DEFAULT_QUIT_MESG "rirc v" VERSION

/* Translate defined values to strings at compile time */
#define TO_STR(X) #X
#define STR(X) TO_STR(X)

/* Suppress 'unused parameter' warnings */
#define UNUSED(X) ((void)(X))

/* Doubly linked list macros */
#define DLL_NEW(L, N) ((L) = (N)->next = (N)->prev = (N))

#define DLL_ADD(L, N) \
	do { \
		if ((L) == NULL) \
			DLL_NEW(L, N); \
		else { \
			((L)->next)->prev = (N); \
			(N)->next = ((L)->next); \
			(N)->prev = (L); \
			(L)->next = (N); \
		} \
	} while (0)

#define DLL_DEL(L, N) \
	do { \
		if (((N)->next) == (N)) \
			(L) = NULL; \
		else { \
			if ((L) == N) \
				(L) = ((N)->next); \
			((N)->next)->prev = ((N)->prev); \
			((N)->prev)->next = ((N)->next); \
		} \
	} while (0)

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
	LINE_PINGED,
	LINE_CHAT,
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
typedef struct avl_node
{
	int height;
	struct avl_node *l;
	struct avl_node *r;
	char *str;
} avl_node;

/* Chat buffer line */
typedef struct line
{
	int rows;
	size_t len;
	time_t time;
	char *text;
	char from[NICKSIZE];
	line_t type;
} line;

/* Channel input line */
typedef struct input_line
{
	char *end;
	char text[MAX_INPUT];
	struct input_line *next;
	struct input_line *prev;
} input_line;

/* Channel input */
typedef struct input
{
	char *head;
	char *tail;
	char *window;
	int count;
	struct input_line *line;
	struct input_line *list_head;
} input;

/* Channel buffer */
typedef struct channel
{
	activity_t active;
	char name[CHANSIZE];
	char type;
	int chanmode;
	int nick_count;
	int parted;
	int resized;
	struct channel *next;
	struct channel *prev;
	struct line *buffer_head;
	struct line buffer[SCROLLBACK_BUFFER];
	struct avl_node *nicklist;
	struct server *server;
	struct input *input;
	struct {
		size_t nick_pad;
		struct line *scrollback;
	} draw;
} channel;

/* Server */
typedef struct server
{
	time_t latency_time;
	time_t latency_delta;
	time_t reconnect_time;
	time_t reconnect_delta;
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
int sendf(char*, server*, const char*, ...);
void check_servers(void);
void server_connect(char*, char*);
void server_disconnect(server*, int, int, char*);

/* draw.c */
unsigned int draw;
void redraw(channel*);
#define draw(X) draw |= X
#define D_RESIZE (1 << 0)
#define D_BUFFER (1 << 1)
#define D_CHANS  (1 << 2)
#define D_INPUT  (1 << 3)
#define D_STATUS (1 << 4)
#define D_FULL ~((draw & 0) | D_RESIZE);

/* input.c */
char *action_message;
input* new_input(void);
void action(int(*)(char), const char*, ...);
void free_input(input*);
void poll_input(void);

/* utils.c */
char* strdup(const char*);
int avl_add(avl_node**, const char*);
int avl_del(avl_node**, const char*);
int check_pinged(char*, char*);
int parse(parsed_mesg*, char*);
void auto_nick(char**, char*);
void free_avl(avl_node*);

/* mesg.c */
void recv_mesg(char*, int, server*);
void send_mesg(char*);
void send_paste(char*);

/* state.c */
channel* channel_close(channel*);
channel* channel_get(char*, server*);
channel* channel_switch(channel*, int);
channel* new_channel(char*, server*, channel*);
void buffer_scrollback_line(channel*, int);
void buffer_scrollback_page(channel*, int);
void clear_channel(channel*);
void free_channel(channel*);
void newline(channel*, line_t, const char*, const char*);
void newlinef(channel*, line_t, const char*, const char*, ...);
void _newline(channel*, line_t, const char*, const char*, size_t);
