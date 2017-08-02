#ifndef NICKLIST_H
#define NICKLIST_H

#include "tree.h"

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
