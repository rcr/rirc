#ifndef COMMON_H
#define COMMON_H

/* FIXME: refactoring */
#include "buffer.h"
#include "draw.h"
#include "utils.h"

#define VERSION "0.1"

#define MAX_SERVERS 32

//FIXME:
#define SCROLLBACK_INPUT 15
#define MAX_INPUT 256
#define NICKSIZE 255

#define BUFFSIZE 512
#define RECONNECT_DELTA 15
#define MODE_SIZE (26 * 2) + 1 /* Supports modes [az-AZ] */

/* When tab completing a nick at the beginning of the line, append the following char */
#define TAB_COMPLETE_DELIMITER ':'

/* Compile time checks */
#if BUFFSIZE < MAX_INPUT
/* Required so input lines can be safely strcpy'ed into a send buffer */
#error BUFFSIZE must be greater than MAX_INPUT
#endif

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

/* Buffer bar activity types */
typedef enum
{
	ACTIVITY_DEFAULT,
	ACTIVITY_ACTIVE,
	ACTIVITY_PINGED,
	ACTIVITY_T_SIZE
} activity_t;

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
	unsigned int count;
	struct input_line *line;
	struct input_line *list_head;
} input;

/* Channel */
typedef struct channel
{
	activity_t active;
	char *name;
	char type_flag;
	char chanmodes[MODE_SIZE];
	int nick_count;
	int parted;
	struct buffer buffer;
	struct channel *next;
	struct channel *prev;
	struct avl_node *nicklist;
	struct server *server;
	struct input *input;
} channel;

/* Server */
typedef struct server
{
	char *host;
	char input[BUFFSIZE];
	char *iptr;
	char nick[NICKSIZE + 1];
	char *nicks;
	char *nptr;
	char *port;
	char *join;
	char usermodes[MODE_SIZE];
	int soc;
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

/* rirc.c */
extern struct config
{
	int join_part_quit_threshold;
	char *username;
	char *realname;
	char *default_nick;
} config;

/* net.c */
int sendf(char*, server*, const char*, ...);
server* get_server_head(void);
void check_servers(void);
void server_connect(char*, char*, char*, char*);
void server_disconnect(server*, int, int, char*);

/* input.c */
input* new_input(void);
void action(int(*)(char), const char*, ...);
void free_input(input*);
void poll_input(void);
extern char *action_message;

/* mesg.c */
void init_mesg(void);
void free_mesg(void);
void recv_mesg(char*, int, server*);
void send_mesg(char*, channel*);
void send_paste(char*);
extern avl_node* commands;

#endif
