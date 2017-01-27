#ifndef COMMON_H
#define COMMON_H

/* FIXME: refactoring */
#include "buffer.h"
#include "draw.h"
#include "utils.h"

#define VERSION "0.1"

#define MAX_SERVERS 32

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

/* Buffer bar activity types */
typedef enum
{
	ACTIVITY_DEFAULT,
	ACTIVITY_ACTIVE,
	ACTIVITY_PINGED,
	ACTIVITY_T_SIZE
} activity_t;

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

/* mesg.c */
void init_mesg(void);
void free_mesg(void);
void recv_mesg(char*, int, server*);
void send_mesg(char*, channel*);
void send_paste(char*);
extern avl_node* commands;

#endif
