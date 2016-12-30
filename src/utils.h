#ifndef UTILS_H
#define UTILS_H

#include <errno.h>

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
parsed_mesg* parse(parsed_mesg*, char*);
void error(int status, const char*, ...);
void free_avl(avl_node*);

/* Irrecoverable error
 *   this define is precluded in test.h to aggregate fatal errors in testcases */
#ifndef fatal
#define fatal(mesg) \
	do { error(errno, "ERROR in %s: %s", __func__, mesg); } while (0)
#endif

#endif
