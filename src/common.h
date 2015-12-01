#ifndef COMMON_H
#define COMMON_H

#define VERSION "0.1"

#define SCROLLBACK_BUFFER 200
#define SCROLLBACK_INPUT 15
#define BUFFSIZE 512
#define NICKSIZE 256
#define CHANSIZE 256
#define MAX_INPUT 256
#define RECONNECT_DELTA 15

/* When tab completing a nick at the beginning of the line, append the following char */
#define TAB_COMPLETE_DELIMITER ':'

/* Compile time checks */
#if BUFFSIZE < MAX_INPUT
/* Required so input lines can be safely strcpy'ed into a send buffer */
#error BUFFSIZE must be greater than MAX_INPUT
#endif

//FIXME: just use an ascii sized buffer and let servers use whatever flag they want
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

#include <time.h>
#include <errno.h>

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

/* Buffer types */
typedef enum {
	BUFFER_OTHER,   /* Default/all other buffers */
	BUFFER_CHANNEL, /* IRC channel buffer */
	BUFFER_SERVER,  /* Server message buffer */
	BUFFER_PRIVATE, /* Private chat buffer */
	BUFFER_T_SIZE
} buffer_t;

/* Buffer bar activity types */
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
	char *key;
	void *val;
} avl_node;

/* Chat buffer line */
typedef struct buffer_line
{
	int rows;
	size_t len;
	time_t time;
	char *text;
	char from[NICKSIZE + 1];
	line_t type;
} buffer_line;

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
	buffer_t buffer_type;
	char name[CHANSIZE];
	char type_flag;
	int chanmode;
	int nick_count;
	int parted;
	int resized;
	struct channel *next;
	struct channel *prev;
	struct buffer_line *buffer_head;
	struct buffer_line buffer[SCROLLBACK_BUFFER];
	struct avl_node *nicklist;
	struct server *server;
	struct input *input;
	struct {
		size_t nick_pad;
		struct buffer_line *scrollback;
	} draw;
} channel;

/* Server */
typedef struct server
{
	char *host;
	char input[BUFFSIZE];
	char *iptr;
	char nick[NICKSIZE + 1];
	char *nptr;
	char *port;
	int soc;
	int usermode;
	int pinging;
	struct avl_node *ignore;
	struct channel *channel;
	struct server *next;
	struct server *prev;
	time_t latency_delta;
	time_t latency_time;
	time_t reconnect_delta;
	time_t reconnect_time;
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
//TODO: move to state
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
char* getarg(char**, const char);
char* strdup(const char*);
char* word_wrap(int, char**, char*);
const avl_node* avl_get(avl_node*, const char*, size_t);
int avl_add(avl_node**, const char*, void*);
int avl_del(avl_node**, const char*);
int check_pinged(const char*, const char*);
int count_line_rows(int, buffer_line*);
parsed_mesg* parse(parsed_mesg*, char*);
void error(int status, const char*, ...);
void free_avl(avl_node*);

/* Irrecoverable error */
#define fatal(mesg) do { \
	error(errno, "ERROR in %s: %s", __func__, mesg); } while (0)

/* mesg.c */
avl_node* commands;
void init_mesg(void);
void free_mesg(void);
void recv_mesg(char*, int, server*);
void send_mesg(char*);
void send_paste(char*);

#endif
