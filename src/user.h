#ifndef NICKLIST_H
#define NICKLIST_H

#include "tree.h"

struct user
{
	AVL_NODE(user) node;
	char *nick;
	char prefix;
	char _[];
};

struct user_list
{
	AVL_HEAD(user);
	unsigned int count;
};

int user_list_add(struct user_list*, char*);
int user_list_del(struct user_list*, char*);
int user_list_rpl(struct user_list*, char*, char*);

struct user* user_list_get(struct user_list*, char*, size_t);

void user_list_free(struct user_list*);

#endif
