#ifndef NICKLIST_H
#define NICKLIST_H

#include "tree.h"

struct user
{
	//AVL_NODE(user) node;
	char flag;
	const char *nick;
	const char *user;
	const char *host;
	char _[];
};

struct _user_list
{
	//AVL_HEAD(user);
	unsigned int count;
};

int _user_list_add(struct _user_list*, const char*, const char*, const char*);
int _user_list_del(struct _user_list*, const char*);

struct user* _user_list_get(struct _user_list*, const char*);

/* ^ WIP */

struct user_list
{
	struct avl_node *root;

	unsigned int count;
};

int user_list_add(struct user_list*, const char*);
int user_list_del(struct user_list*, const char*);

const char* user_list_get(struct user_list*, const char*, size_t);

void user_list_free(struct user_list*);

#endif
