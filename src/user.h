#ifndef NICKLIST_H
#define NICKLIST_H

#include "tree.h"

enum user_err
{
	USER_ERR_DUPLICATE = -2,
	USER_ERR_NOT_FOUND = -1,
	USER_ERR_NONE
};

struct user
{
	AVL_NODE(user) node;
	/* TODO: struct mode */
	char *nick;
	char prefix;
	char _[]; /* TODO: can this just be char nick[]?  */
};

struct user_list
{
	AVL_HEAD(user);
	unsigned int count;
};

enum user_err user_list_add(struct user_list*, char*);
enum user_err user_list_del(struct user_list*, char*);
enum user_err user_list_rpl(struct user_list*, char*, char*);

struct user* user_list_get(struct user_list*, char*, size_t);

void user_list_free(struct user_list*);

#endif
