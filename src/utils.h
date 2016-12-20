#ifndef UTILS_H
#define UTILS_H

#include <errno.h>


/* FIXME: remove when new buffer_line implementation is finished */
#define NICKSIZE 256
#include <time.h>
/* Buffer line types */
typedef enum {
	LINE_DEFAULT,
	LINE_PINGED,
	LINE_CHAT,
	LINE_T_SIZE
} line_t;
/* Channel buffer line */
typedef struct _buffer_line
{
	int rows;
	size_t len;
	time_t time;
	char *text;
	char from[NICKSIZE + 1];
	line_t type;
} _buffer_line;



/* Nicklist AVL tree node */
typedef struct avl_node
{
	int height;
	struct avl_node *l;
	struct avl_node *r;
	char *key;
	void *val;
} avl_node;

/* Parsed IRC message */
typedef struct parsed_mesg
{
	char *from;
	char *hostinfo;
	char *command;
	char *params;
	char *trailing;
} parsed_mesg;


char* getarg(char**, const char*);
char* strdup(const char*);
char* word_wrap(int, char**, char*);
const avl_node* avl_get(avl_node*, const char*, size_t);
int avl_add(avl_node**, const char*, void*);
int avl_del(avl_node**, const char*);
int check_pinged(const char*, const char*);
int count_line_rows(int, _buffer_line*);
parsed_mesg* parse(parsed_mesg*, char*);
void error(int status, const char*, ...);
void free_avl(avl_node*);

/* Irrecoverable error */
#define fatal(mesg) \
	do { error(errno, "ERROR in %s: %s", __func__, mesg); } while (0)

#endif
